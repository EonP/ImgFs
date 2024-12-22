/**
 * @file imgfscmd_functions.c
 * @brief imgFS command line interpreter for imgFS core commands.
 *
 * @author Mia Primorac
 */

#include "imgfs.h"
#include "imgfscmd_functions.h"
#include "util.h"   // for _unused

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

// default values
static const uint32_t default_max_files = 128;
static const uint16_t default_thumb_res =  64;
static const uint16_t default_small_res = 256;

// max values
static const uint16_t MAX_THUMB_RES = 128;
static const uint16_t MAX_SMALL_RES = 512;
static const uint32_t MAX_UINT32 = 4294967295;

/**********************************************************************
 * Create the name of the file to use to save the read image.
 ********************************************************************** */
static void create_name(const char* img_id, int resolution, char** new_name)
{
    // Check the valididy of the arguments
    if (img_id == NULL || new_name == NULL || !(0 <= resolution && resolution < NB_RES)) {
        return;
    }

    // Array storing the suffixes of each resolution
    const char* suffixes[NB_RES] = {"_thumb", "_small", "_orig"};

    // Length of the new name (including the null character)
    size_t len = MAX_IMG_ID + strlen(suffixes[resolution]) + strlen(".jpg") + 1;

    // Allocate the memory for the new name
    *new_name = calloc(len, 1);
    if (*new_name == NULL) return;

    // Format of the new name
    sprintf(*new_name, "%s%s%s", img_id, suffixes[resolution], ".jpg");
}

/**********************************************************************
 * Write the content of the provided image_buffer to a file
 ********************************************************************** */
static int write_disk_image(const char *filename, const char *image_buffer, uint32_t image_size)
{
    M_REQUIRE_NON_NULL(filename);
    M_REQUIRE_NON_NULL(image_buffer);

    // Open file in "write binary" mode
    FILE* file = fopen(filename, "wb");
    if (file == NULL) return ERR_IO;

    int ret = ERR_NONE;
    // Write constents of buffer to the file
    if (fwrite(image_buffer, image_size, 1, file) != 1) {
        ret = ERR_IO;
    }

    // Close the file
    fclose(file);
    return ret;
}

/**********************************************************************
 *  Reads an image from disk
 ********************************************************************** */
static int read_disk_image(const char *path, char **image_buffer, uint32_t *image_size)
{
    M_REQUIRE_NON_NULL(path);
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(image_size);

    // Open file in "read binary" mode
    FILE* file = fopen(path, "rb");
    if (file == NULL) return ERR_IO;

    // Move file position indicator to the end of the file (to extract file size)
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ERR_IO;
    }

    long size = ftell(file); // Save the size of file
    if (size == -1) {
        fclose(file);
        return ERR_IO;
    }

    // Move file position indicator to the start of the file
    if (fseek(file, 0, SEEK_SET) != 0) {
        return ERR_IO;
    }

    // Allocate memory for the buffer
    *image_buffer = calloc((size_t) size, 1);
    if (*image_buffer == NULL) {
        fclose(file); // Close the file
        return ERR_OUT_OF_MEMORY;
    }

    int ret = ERR_NONE;
    // Put the contents of the image file to the buffer
    if (fread(*image_buffer, (size_t) size, 1, file) != 1) {
        ret = ERR_IO;
    }

    // Set image_size to the size of the file
    *image_size = (uint32_t) size;

    // Close the file
    fclose(file);
    return ret;
}

/**********************************************************************
 * Displays some explanations.
 ********************************************************************** */
int help(int useless _unused, char** useless_too _unused)
{
    /* **********************************************************************
     * TODO WEEK 08: WRITE YOUR CODE HERE.
     * **********************************************************************
     */

    printf(
    "imgfscmd [COMMAND] [ARGUMENTS]\n"
    "  help: displays this help.\n"
    "  list <imgFS_filename>: list imgFS content.\n"
    "  create <imgFS_filename> [options]: create a new imgFS.\n"
    "      options are:\n"
    "          -max_files <MAX_FILES>: maximum number of files.\n"
    "                                  default value is %u\n"
    "                                  maximum value is %u\n"
    "          -thumb_res <X_RES> <Y_RES>: resolution for thumbnail images.\n"
    "                                  default value is %ux%u\n"
    "                                  maximum value is %ux%u\n"
    "          -small_res <X_RES> <Y_RES>: resolution for small images.\n"
    "                                  default value is %ux%u\n"
    "                                  maximum value is %ux%u\n"
    "  read   <imgFS_filename> <imgID> [original|orig|thumbnail|thumb|small]:\n"
    "      read an image from the imgFS and save it to a file.\n"
    "      default resolution is \"original\".\n"
    "  insert <imgFS_filename> <imgID> <filename>: insert a new image in the imgFS.\n"
    "  delete <imgFS_filename> <imgID>: delete image imgID from imgFS.\n"
    , default_max_files, MAX_UINT32, default_thumb_res, default_thumb_res, MAX_THUMB_RES,
    MAX_THUMB_RES, default_small_res, default_small_res, MAX_SMALL_RES, MAX_SMALL_RES
    );

    return ERR_NONE;
}

/**********************************************************************
 * Opens imgFS file and calls do_list().
 ********************************************************************** */
int do_list_cmd(int argc, char** argv)
{
    /* **********************************************************************
     * TODO WEEK 07: WRITE YOUR CODE HERE.
     * **********************************************************************
     */

    M_REQUIRE_NON_NULL(argv);
    if (argc != 1) return ERR_INVALID_COMMAND; // only one argument expected

    const char* filename = argv[0];
    M_REQUIRE_NON_NULL(filename);

    struct imgfs_file imgfs_file;

    // Open file in read-binary mode
    int ret = do_open(filename, "rb", &imgfs_file);

    if (ret) return ret;

    // List header and metadata of file
    ret = do_list(&imgfs_file, STDOUT, NULL);

    // Close file
    do_close(&imgfs_file);
    return ret;
}

/**********************************************************************
 * Prepares and calls do_create command.
********************************************************************** */
int do_create_cmd(int argc, char** argv)
{
    /* **********************************************************************
     * TODO WEEK 08: WRITE YOUR CODE HERE (and change the return if needed).
     * **********************************************************************
     */

    M_REQUIRE_NON_NULL(argv);
    if (argc < 1) return ERR_NOT_ENOUGH_ARGUMENTS; // A minimum of one argument is expected

    const char* filename = argv[0];
    M_REQUIRE_NON_NULL(filename);
    --argc; ++argv; // Skip file name

    // Initialize resolutions to default values
    uint16_t thumb_res_x = default_thumb_res;
    uint16_t thumb_res_y = default_thumb_res;
    uint16_t small_res_x = default_small_res;
    uint16_t small_res_y = default_small_res;
    uint32_t max_files = default_max_files;

    // Loop while there still exist arguments
    while(argc > 0) {
        // Check if current argument is "-max_files"
        if (strcmp("-max_files", argv[0]) == 0) {
            --argc; ++argv; // Skip argument
            if (argc < 1) return ERR_NOT_ENOUGH_ARGUMENTS; // One argument for -max_files

            max_files = atouint32(argv[0]);
            if (max_files == 0) return ERR_MAX_FILES;

            --argc; ++argv; // Go to next instruction

            // Check if current argument is "-thumb_res"
        } else if (strcmp("-thumb_res", argv[0]) == 0) {
            --argc; ++argv; // Skip argument
            if (argc < 2) return ERR_NOT_ENOUGH_ARGUMENTS; // Two argument for -thumb_res

            thumb_res_x = atouint16(argv[0]);
            thumb_res_y = atouint16(argv[1]);
            // Make sure resolutions are in bounds
            if (!(0 < thumb_res_x && thumb_res_x <= MAX_THUMB_RES && 0 < thumb_res_y && thumb_res_y <= MAX_THUMB_RES)) {
                return ERR_RESOLUTIONS;
            }

            argc -= 2; argv += 2; // Go to next instruction

            // Check if current argument is "-small_res"
        } else if (strcmp("-small_res", argv[0]) == 0) {
            --argc; ++argv; // Skip argument
            if (argc < 2) return ERR_NOT_ENOUGH_ARGUMENTS; // Two argument for -small_res

            small_res_x = atouint16(argv[0]);
            small_res_y = atouint16(argv[1]);
            // Make sure resolutions are in bounds
            if (!(0 < small_res_x && small_res_x <= MAX_SMALL_RES && 0 < small_res_y && small_res_y <= MAX_SMALL_RES)) {
                return ERR_RESOLUTIONS;
            }

            argc -= 2; argv += 2; // go to next instruction

            // Error occurs if arguments are incorectly written
        } else {
            return ERR_INVALID_ARGUMENT;
        }
    }

    // Create new imgfs_file and initialize with given values or default ones
    struct imgfs_file imgfs_file;
    zero_init_var(imgfs_file);
    imgfs_file.header.max_files = max_files;
    imgfs_file.header.resized_res[0] = thumb_res_x;
    imgfs_file.header.resized_res[1] = thumb_res_y;
    imgfs_file.header.resized_res[2] = small_res_x;
    imgfs_file.header.resized_res[3] = small_res_y;

    // Create the imgfs with name filename
    int ret = do_create(filename, &imgfs_file);

    // Close file
    do_close(&imgfs_file);
    return ret;
}

/**********************************************************************
 * Deletes an image from the imgFS.
 */
int do_delete_cmd(int argc, char** argv)
{
    /* **********************************************************************
     * TODO WEEK 08: WRITE YOUR CODE HERE (and change the return if needed).
     * **********************************************************************
     */

    M_REQUIRE_NON_NULL(argv);
    if (argc != 2) return ERR_NOT_ENOUGH_ARGUMENTS; // Only takes two arguments: filename and img_id

    const char* filename = argv[0];
    M_REQUIRE_NON_NULL(filename);
    const char* img_id = argv[1];
    M_REQUIRE_NON_NULL(img_id);

    // Check img_id length is in bounds
    if (!(0 < strlen(img_id) && strlen(img_id) <= MAX_IMG_ID)) return ERR_INVALID_IMGID;

    struct imgfs_file imgfs_file;
    zero_init_var(imgfs_file);

    // open filename in "rb+" mode (both reading and writing)
    int ret = do_open(filename, "rb+", &imgfs_file);

    if (ret) return ret;

    // delete image with img_id in imgfs_file
    ret = do_delete(img_id, &imgfs_file);

    // Close file
    do_close(&imgfs_file);
    return ret;
}

int do_read_cmd(int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc != 2 && argc != 3) return ERR_NOT_ENOUGH_ARGUMENTS;

    const char * const img_id = argv[1];

    const int resolution = (argc == 3) ? resolution_atoi(argv[2]) : ORIG_RES;
    if (resolution == -1) return ERR_RESOLUTIONS;

    struct imgfs_file myfile;
    zero_init_var(myfile);
    int error = do_open(argv[0], "rb+", &myfile);
    if (error != ERR_NONE) return error;

    char *image_buffer = NULL;
    uint32_t image_size = 0;
    error = do_read(img_id, resolution, &image_buffer, &image_size, &myfile);
    do_close(&myfile);
    if (error != ERR_NONE) {
        return error;
    }

    // Extracting to a separate image file.
    char* tmp_name = NULL;
    create_name(img_id, resolution, &tmp_name);
    if (tmp_name == NULL) return ERR_OUT_OF_MEMORY;
    error = write_disk_image(tmp_name, image_buffer, image_size);
    free(tmp_name);
    free(image_buffer);

    return error;
}

int do_insert_cmd(int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc != 3) return ERR_NOT_ENOUGH_ARGUMENTS;

    struct imgfs_file myfile;
    zero_init_var(myfile);
    int error = do_open(argv[0], "rb+", &myfile);
    if (error != ERR_NONE) return error;

    char *image_buffer = NULL;
    uint32_t image_size;

    // Reads image from the disk.
    error = read_disk_image (argv[2], &image_buffer, &image_size);
    if (error != ERR_NONE) {
        do_close(&myfile);
        return error;
    }

    error = do_insert(image_buffer, image_size, argv[1], &myfile);
    free(image_buffer);
    do_close(&myfile);
    return error;
}
