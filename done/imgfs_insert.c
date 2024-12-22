#include <string.h>
#include <stdlib.h>
#include "image_content.h"
#include "image_dedup.h"

int do_insert(const char* image_buffer, size_t image_size, const char* img_id, struct imgfs_file* imgfs_file)
{
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);
    M_REQUIRE_NON_NULL(img_id);

    // If we can't insert any more files return error
    if (imgfs_file->header.nb_files >= imgfs_file->header.max_files) return ERR_IMGFS_FULL;

    size_t i = 0;
    // Find index where the metadata is not valid
    while (i < imgfs_file->header.max_files && imgfs_file->metadata[i].is_valid) ++i;

    // Calculate the SHA code of the image_buffer and copy it to the metadata
    SHA256((const unsigned char *) image_buffer, image_size, imgfs_file->metadata[i].SHA);

    // Copy the given image id to the metadata
    strncpy(imgfs_file->metadata[i].img_id, img_id, MAX_IMG_ID + 1);

    // Update the size of the the image in original resolution in the metadata
    imgfs_file->metadata[i].size[ORIG_RES] = (uint32_t) image_size;
    imgfs_file->metadata[i].size[SMALL_RES] = 0;
    imgfs_file->metadata[i].size[THUMB_RES] = 0;

    // Width and height of the image
    uint32_t width = 0;
    uint32_t height = 0;
    // Get the width and height of image with the get_resolution method
    int ret = get_resolution(&height, &width, image_buffer, image_size);
    if(ret) return ret;

    // Update width and height in the metadata
    imgfs_file->metadata[i].orig_res[0] = width;
    imgfs_file->metadata[i].orig_res[1] = height;

    // Check for if the duplicate of this image exists
    ret = do_name_and_content_dedup(imgfs_file, (uint32_t) i);
    if(ret) return ret;

    // If there are no duplicates, we have to update the offset of the image
    if(imgfs_file->metadata[i].offset[ORIG_RES] == 0) {
        // Move file position indicator to the end of the file (where we add the new image)
        if (fseek(imgfs_file->file, 0, SEEK_END) != 0) return ERR_IO;

        long off = ftell(imgfs_file->file); // Offset of the end of the file
        if (off == -1) return ERR_IO;

        // Update offset field of the metadata
        imgfs_file->metadata[i].offset[THUMB_RES] = 0;
        imgfs_file->metadata[i].offset[SMALL_RES] = 0;
        imgfs_file->metadata[i].offset[ORIG_RES] = (uint64_t) off;

        // Write the contents of the buffer at the end of the file
        if (fwrite(image_buffer, image_size, 1, imgfs_file->file) != 1) return ERR_IO;
    }

    // Set the valid field of the metadata to 1
    imgfs_file->metadata[i].is_valid = NON_EMPTY;

    // Update the header imformation
    imgfs_file->header.nb_files += 1;
    imgfs_file->header.version += 1;

    // Put the file position indicator at the start of the file (header start)
    if (fseek(imgfs_file->file, 0, SEEK_SET) != 0) return ERR_IO;

    // Write the contents of the header from the file
    if (fwrite(&(imgfs_file->header), sizeof(struct imgfs_header), 1, imgfs_file->file) != 1) return ERR_IO;

    // Put the file position indicator at the correct position in the metadata
    long off = (long) (sizeof(struct imgfs_header) + i * sizeof(struct img_metadata));
    if (fseek(imgfs_file->file, off, SEEK_SET) != 0) return ERR_IO;

    // Write the contents of the metadata at index i from the file
    if (fwrite(&imgfs_file->metadata[i], sizeof(struct img_metadata), 1, imgfs_file->file) != 1) return ERR_IO;

    return ERR_NONE;
}
