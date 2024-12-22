#include <stdio.h>
#include "imgfs.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

int do_create(const char* imgfs_filename, struct imgfs_file* imgfs_file)
{
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_filename);

    size_t items_written = 0; // items written (header and number of metatada)
    imgfs_file->header.version = 0;
    imgfs_file->header.nb_files = 0;

    // Open file in "write binary" mode
    imgfs_file->file = fopen(imgfs_filename, "wb");
    if(imgfs_file->file == NULL) return ERR_IO;

    // Set the database name with the provided constant CAT_TXT
    strcpy(imgfs_file->header.name, CAT_TXT);

    // Allocate memory for the metadata
    imgfs_file->metadata = calloc(imgfs_file->header.max_files, sizeof(struct img_metadata));
    if(imgfs_file->metadata == NULL) return ERR_OUT_OF_MEMORY;

    // Write the header to the imgfs_file
    if(fwrite(&(imgfs_file->header), sizeof(struct imgfs_header), 1, imgfs_file->file) != 1) return ERR_IO;

    ++items_written; // header was written successfully so increase items_written

    // Write the metadata to the imgfs_file
    size_t nb_metadata = fwrite(imgfs_file->metadata, sizeof(struct img_metadata), imgfs_file->header.max_files, imgfs_file->file);

    // If unsuccessfull write, still print because header was successfully written
    if(nb_metadata != imgfs_file->header.max_files) {
        printf("%zu item(s) written\n", items_written);
        return ERR_IO;
    }

    items_written += nb_metadata; // metadata was written successfully so increase items_written

    printf("%zu item(s) written\n", items_written);

    return ERR_NONE;

}
