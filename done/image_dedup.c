#include <stdio.h>
#include <string.h>
#include "image_dedup.h"

int do_name_and_content_dedup(struct imgfs_file* imgfs_file, uint32_t index)
{
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);
    // Check that the index is in bounds and that the metadata at the index is valid
    if (!(0 <= index && index < imgfs_file->header.max_files)) {
        return ERR_IMAGE_NOT_FOUND;
    }

    const char* img_id = imgfs_file->metadata[index].img_id;
    const unsigned char* sha = imgfs_file->metadata[index].SHA;

    for (size_t i = 0; i < imgfs_file->header.max_files ; ++i) {
        // We must check if current metadata is valid and the i is not the index to check for duplication
        if(imgfs_file->metadata[i].is_valid && i != index) {
            // Duplicate image if the id's are equal
            if (strcmp(img_id, imgfs_file->metadata[i].img_id) == 0) {
                return ERR_DUPLICATE_ID;
                // Check if hash code of the images are equal => we deduplicate
            } else if (memcmp(sha, imgfs_file->metadata[i].SHA, SHA256_DIGEST_LENGTH) == 0) {
                // To deduplicate, we modify metadata at index position to reference attributes of copy found
                for (size_t j = 0; j < NB_RES; ++j) {
                    imgfs_file->metadata[index].offset[j] = imgfs_file->metadata[i].offset[j];
                    imgfs_file->metadata[index].size[j] = imgfs_file->metadata[i].size[j];
                }
                return ERR_NONE;
            }
        }
    }

    // Set offset to zero if image at position index has no duplicate content
    imgfs_file->metadata[index].offset[ORIG_RES] = 0;

    // Return ERR_NONE if image at position index has no duplicate name (img_id)
    return ERR_NONE;

}
