#include <stdio.h>
#include "imgfs.h"
#include "image_content.h"
#include <vips/vips.h>


int lazily_resize(int resolution, struct imgfs_file* imgfs_file, size_t index)
{
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);
    M_REQUIRE_NON_NULL(imgfs_file->file);
    // Check that index is in bounds and metadata at the index is valid
    if (!(0 <= index && index < imgfs_file->header.max_files) || !imgfs_file->metadata[index].is_valid) {
        return ERR_INVALID_IMGID;
    }
    // Check that resolution is either THUMB_RES, SMALL_RES or ORIG_RES
    if (!(resolution == THUMB_RES || resolution == SMALL_RES || resolution == ORIG_RES)) return ERR_RESOLUTIONS;

    // If resolution is ORIG_RES no need to do anything
    if (resolution == ORIG_RES) return ERR_NONE;

    // If the resolution requested already exists, do nothing and return no error
    if (imgfs_file->metadata[index].offset[resolution] != 0 && imgfs_file->metadata[index].size[resolution] != 0) {
        return ERR_NONE;
    }

    long og_off = (long) imgfs_file->metadata[index].offset[ORIG_RES]; // Offset in file of image in its original resolution
    // Move file position indicator to image in database in its original resolution
    if (fseek(imgfs_file->file, og_off, SEEK_SET) != 0) return ERR_IO;

    size_t og_size = imgfs_file->metadata[index].size[ORIG_RES]; // Size of image in its original resolution
    // Allocating memory for first buffer
    void* buffer1 = calloc(og_size, 1);
    if (buffer1 == NULL) return ERR_OUT_OF_MEMORY;

    // Read image from file in the buffer
    if (fread(buffer1, 1, og_size, imgfs_file->file) != og_size) {
        free(buffer1); buffer1 = NULL;
        return ERR_IO;
    }

    VipsImage* og_img = NULL; // Vips image corresponding to original image
    // Load the buffer in the vips image corresponding to the original resolution
    if (vips_jpegload_buffer(buffer1, og_size, &og_img, NULL) != 0) {
        free(buffer1); buffer1 = NULL;
        return ERR_IMGLIB;
    }

    VipsImage* thumb_img = NULL; // Vips image corresponding to resized image
    // Create thumbnail vips image.
    int width = imgfs_file->header.resized_res[2 * resolution];
    int height = imgfs_file->header.resized_res[2 * resolution + 1];
    if (vips_thumbnail_image(og_img, &thumb_img, width, "height", height, NULL) != 0) {
        free(buffer1); buffer1 = NULL;
        g_object_unref(VIPS_OBJECT(og_img)); og_img = NULL;
        return ERR_IMGLIB;
    }

    void* buffer2 = NULL; // Second buffer to save thumbnail (it is allocated by vips function)
    size_t len = 0; // Length of thumbnail image to be modified by the vips method
    // Save constent from thumb_img to the buffer
    if (vips_jpegsave_buffer(thumb_img, &buffer2, &len, NULL) != 0) {
        free(buffer1); buffer1 = NULL;
        g_object_unref(VIPS_OBJECT(og_img)); og_img = NULL;
        g_object_unref(VIPS_OBJECT(thumb_img)); thumb_img = NULL;
        return ERR_IMGLIB;
    }

    free(buffer1); buffer1 = NULL; // Free the first buffer as no longer needed
    g_object_unref(VIPS_OBJECT(og_img)); og_img = NULL;// No longer need original image obect
    g_object_unref(thumb_img); thumb_img = NULL; // No longer need thumbnail image obect

    // Move file position indicator to end of file
    if (fseek(imgfs_file->file, 0, SEEK_END) != 0) {
        g_free(buffer2); buffer2 = NULL;
        return ERR_IO;
    }

    long off = ftell(imgfs_file->file); // Offset of the end of file
    // Set end of file offset with ftell
    if (off == -1) {
        g_free(buffer2); buffer2 = NULL;
        return ERR_IO;
    }

    // Write contents of buffer to the end of the file
    if (fwrite(buffer2, len, 1, imgfs_file->file) != 1) {
        g_free(buffer2); buffer2 = NULL;
        return ERR_IO;
    }

    g_free(buffer2); buffer2 = NULL; // Free the thumbnail buffer after writing

    // We update metadata offset and size of image for the given resolution
    imgfs_file->metadata[index].offset[resolution] = (uint64_t) off;
    imgfs_file->metadata[index].size[resolution] = (uint32_t) len;

    size_t metadata_off = sizeof(struct imgfs_header) + index * sizeof(struct img_metadata); // offset of metadata at given index
    // Move file position indicator to the correct metadata
    if (fseek(imgfs_file->file, (long) metadata_off, SEEK_SET) != 0) return ERR_IO;
    // Write the modified metadata to the file
    if (fwrite(&imgfs_file->metadata[index], sizeof(struct img_metadata), 1, imgfs_file->file) != 1) return ERR_IO;

    return ERR_NONE;
}

int get_resolution(uint32_t *height, uint32_t *width, const char *image_buffer, size_t image_size)
{
    M_REQUIRE_NON_NULL(height);
    M_REQUIRE_NON_NULL(width);
    M_REQUIRE_NON_NULL(image_buffer);

    VipsImage* original = NULL;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    const int err = vips_jpegload_buffer((void*) image_buffer, image_size,
                                         &original, NULL);
#pragma GCC diagnostic pop
    if (err != ERR_NONE) return ERR_IMGLIB;

    *height = (uint32_t) vips_image_get_height(original);
    *width  = (uint32_t) vips_image_get_width (original);

    g_object_unref(VIPS_OBJECT(original));
    return ERR_NONE;
}
