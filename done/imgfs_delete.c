#include <stdio.h>
#include "imgfs.h"
#include <string.h>

int do_delete(const char* img_id, struct imgfs_file* imgfs_file)
{
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(imgfs_file->file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);

    size_t i;
    size_t j;
    int found = 0;
    // Find index of metadata that has the same "img_id" as the one passed as argument
    for (j = 0; j < (imgfs_file->header).max_files && !found; ++j) {
        if (strncmp(img_id, imgfs_file->metadata[j].img_id, MAX_IMG_ID + 1) == 0 && imgfs_file->metadata[j].is_valid) {
            found = 1;
            i = j;
        }
    }

    // Check that index is in bounds and that metadata found is valid
    if (!found) return ERR_IMAGE_NOT_FOUND;

    imgfs_file->metadata[i].is_valid = EMPTY; // set valid to 0

    long off = (long)(sizeof(struct imgfs_header) + i * sizeof(struct img_metadata));
    // Move file position indicator to the location of the metadata corresponding to index i
    if (fseek(imgfs_file->file, off, SEEK_SET) != 0) return ERR_IO;

    // Rewrite metadata with the changed "is_valid" parameter in file
    size_t nb_metadata = fwrite(&imgfs_file->metadata[i], sizeof(struct img_metadata), 1, imgfs_file->file);
    if (nb_metadata != 1) return ERR_IO;

    // Metadata write is successfull so modify header
    imgfs_file->header.nb_files -= 1;
    imgfs_file->header.version += 1;

    // Move file position indicator at the begining of the file
    if (fseek(imgfs_file->file, 0, SEEK_SET) != 0) return ERR_IO;

    // Write modifies header to file
    size_t nb_header = fwrite(&(imgfs_file->header), sizeof(struct imgfs_header), 1, imgfs_file->file);
    if (nb_header != 1) return ERR_IO;

    return ERR_NONE;
}
