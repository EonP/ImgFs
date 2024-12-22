/**
 * @file imgfs.h
 * @brief Main header file for imgFS core library.
 *
 * Defines the format of the data structures that will be stored on
 * the disk and provides interface functions.
 *
 * The imgFS data structure starts with exactly one header structure,
 * followed by exactly imgfs_header.max_files metadata structures.
 * The actual content is not defined by these structures because it
 * should be stored as raw bytes appended at the end of the imgFS
 * file and addressed by offsets in the metadata structure.
 *
 * @author Mia Primorac
 */

#pragma once

#include "error.h" /* Not needed in this very file,
                    * but we provide it here, as it is required by
                    * all the functions of this lib.
                    */
#include <openssl/sha.h>   // for SHA256_DIGEST_LENGTH
#include <stdint.h>        // for uint32_t, uint64_t
#include <stdio.h>         // for FILE

#define CAT_TXT "EPFL ImgFS 2024"

// Constraints
#define MAX_IMGFS_NAME  31  // max. size of a ImgFS name
#define MAX_IMG_ID     127  // max. size of an image id

// For is_valid in imgfs_metadata
#define EMPTY     0
#define NON_EMPTY 1

// imgFS library internal codes for different image resolutions
#define THUMB_RES 0
#define SMALL_RES 1
#define ORIG_RES  2
#define NB_RES    3

#define ORIG_RES_SIZE 2

#ifdef __cplusplus
extern "C" {
#endif

/* **********************************************************************
 * TODO WEEK 07: DEFINE YOUR STRUCTS HERE.
 * **********************************************************************
 */

// Structure representing the header of an image file system
struct imgfs_header {
    char name[MAX_IMGFS_NAME+1]; // name of the database
    uint32_t version; // version of the database
    uint32_t nb_files; // current number of images in the system
    uint32_t max_files; // maximum number of images that the system can contain
    uint16_t resized_res[2 * (NB_RES-1)]; // resolutions of the "thumbnail" and "small" images (widths and heights)
    uint32_t unused_32; // unused but intended for future evolutions or temporary information
    uint64_t unused_64; // unused but intended for future evolutions or temporary information
};

// Structure representing metadata for individual images
struct img_metadata {
    char img_id[MAX_IMG_ID+1]; // unique identifier (name) for the image
    unsigned char SHA[SHA256_DIGEST_LENGTH]; // image hash code
    uint32_t orig_res[ORIG_RES_SIZE]; // resolution of the original image
    uint32_t size[NB_RES]; // memory sizes (in bytes) of images at different resolutions
    uint64_t offset[NB_RES]; // positions in the "image database" file of images at different resolutions
    uint16_t is_valid; // indicates whether the image is in use
    uint16_t unused_16; // unused but intended for future evolutions
};

// Structure representing a file within the image file system
struct imgfs_file {
    FILE* file; // file containing everything (on disk)
    struct imgfs_header header; // general information ("header") of the image database
    struct img_metadata* metadata; // "metadata" of the images in the database
};

/**
 * @brief Prints imgFS header informations.
 *
 * @param header The header to be displayed.
 */
void print_header(const struct imgfs_header* header);

/**
 * @brief Prints image metadata informations.
 *
 * @param metadata The metadata of one image.
 */
void print_metadata(const struct img_metadata* metadata);

/**
 * @brief Open imgFS file, read the header and all the metadata.
 *
 * @param imgfs_filename Path to the imgFS file
 * @param open_mode Mode for fopen(), eg.: "rb", "rb+", etc.
 * @param imgfs_file Structure for header, metadata and file pointer.
 */
int do_open(const char* imgfs_filename,
            const char* open_mode,
            struct imgfs_file* imgfs_file);

/**
 * @brief Do some clean-up for imgFS file handling.
 *
 * @param imgfs_file Structure for header, metadata and file pointer to be freed/closed.
 */
void do_close(struct imgfs_file* imgfs_file);

/**
 * @brief List of possible output modes for do_list()
 *
 * @param imgfs_file Structure for header, metadata and file pointer to be freed/closed.
 */
enum do_list_mode {
    STDOUT,
    JSON,
    NB_DO_LIST_MODES
};

/**
 * @brief Displays (on stdout) imgFS metadata.
 *
 * @param imgfs_file In memory structure with header and metadata.
 * @param output_mode What style to use for displaying infos.
 * @param json A pointer to a string containing the list in JSON format if output_mode is JSON.
 *      It will be dynamically allocated by the function. Ignored for other output modes.
 * @return some error code.
 */
int do_list(const struct imgfs_file* imgfs_file,
            enum do_list_mode output_mode, char** json);

/**
 * @brief Creates the imgFS called imgfs_filename. Writes the header and the
 *        preallocated empty metadata array to imgFS file.
 *
 * @param imgfs_filename Path to the imgFS file
 * @param imgfs_file In memory structure with header and metadata.
 */
int do_create(const char* imgfs_filename, struct imgfs_file* imgfs_file);

/**
 * @brief Deletes an image from a imgFS imgFS.
 *
 * Effectively, it only invalidates the is_valid field and updates the
 * metadata.  The raw data content is not erased, it stays where it
 * was (and  new content is always appended to the end; no garbage
 * collection).
 *
 * @param img_id The ID of the image to be deleted.
 * @param imgfs_file The main in-memory data structure
 * @return Some error code. 0 if no error.
 */
int do_delete(const char* img_id, struct imgfs_file* imgfs_file);

/**
 * @brief Transforms resolution string to its int value.
 *
 * @param resolution The resolution string. Shall be "original",
 *        "orig", "thumbnail", "thumb" or "small".
 * @return The corresponding value or -1 if error.
 */
int resolution_atoi(const char* resolution);

/**
 * @brief Reads the content of an image from a imgFS.
 *
 * @param img_id The ID of the image to be read.
 * @param resolution The desired resolution for the image read.
 * @param image_buffer Location of the location of the image content
 * @param image_size Location of the image size variable
 * @param imgfs_file The main in-memory data structure
 * @return Some error code. 0 if no error.
 */
int do_read(const char* img_id, int resolution, char** image_buffer,
            uint32_t* image_size, struct imgfs_file* imgfs_file);

/**
 * @brief Insert image in the imgFS file
 *
 * @param buffer Pointer to the raw image content
 * @param size Image size
 * @param img_id Image ID
 * @return Some error code. 0 if no error.
 */
int do_insert(const char* image_buffer, size_t image_size,
              const char* img_id, struct imgfs_file* imgfs_file);

/**
 * @brief Removes the deleted images by moving the existing ones
 *
 * @param imgfs_path The path to the imgFS file
 * @param imgfs_tmp_bkp_path The path to the a (to be created) temporary imgFS backup file
 * @return Some error code. 0 if no error.
 */
int do_gbcollect(const char* imgfs_path, const char* imgfs_tmp_bkp_path);

#ifdef __cplusplus
}
#endif
