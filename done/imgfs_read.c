#include <stdlib.h>
#include <string.h>
#include "image_content.h"

int do_read(const char* img_id, int resolution, char** image_buffer, uint32_t* image_size, struct imgfs_file* imgfs_file)
{
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(image_size);

    size_t i = 0;
    // Find index of metadata that has the same "img_id" as the one passed as argument

    while (i < imgfs_file->header.max_files
           && strcmp(img_id, imgfs_file->metadata[i].img_id) != 0) {
        ++i;
    }
    // No image with the same image image id found
    if (i >= imgfs_file->header.max_files) return ERR_IMAGE_NOT_FOUND;

    int ret = ERR_NONE; // Return value of lazily_resize
    if (resolution == SMALL_RES || resolution == THUMB_RES) {
        // If image doesn't exist in the requested resolution call lazily_resize
        if (imgfs_file->metadata[i].offset[resolution] == 0 || imgfs_file->metadata[i].size[resolution] == 0) {
            ret = lazily_resize(resolution, imgfs_file, i);
            // Return error if laziliy resize failed
            if (ret) return ret;
        }
    }

    // Get offset and size (modified by resize) to correctly read from file to buffer
    size_t size = imgfs_file->metadata[i].size[resolution];
    size_t off = imgfs_file->metadata[i].offset[resolution];

    *image_buffer = calloc(1, size); // Dynamically allocate memory region to read contents of image file
    if (*image_buffer == NULL) return ERR_OUT_OF_MEMORY;

    // Move file position indicator to the image we want to read
    if (fseek(imgfs_file->file, (long) off, SEEK_SET) != 0) return ERR_IO;

    // Read image file and place its contents in the buffer
    if (fread(*image_buffer, size, 1, imgfs_file->file) != 1) return ERR_IO;

    // As no errors have occured, correctly change the image size field
    *image_size = (uint32_t) size;

    return ERR_NONE;
}
