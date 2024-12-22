#include "imgfs.h"
#include "util.h"
#include<json-c/json.h>
#include <string.h>

int do_list(const struct imgfs_file* imgfs_file, enum do_list_mode output_mode, char** json)
{
    M_REQUIRE_NON_NULL(imgfs_file);

    // Switch based on the output mode to determine how to list the contents of file
    switch (output_mode) {
    case STDOUT:
        // first print header info
        print_header(&imgfs_file->header);
        if(imgfs_file->header.nb_files == 0) {
            printf("<< empty imgFS >>"); // image filesystem is empty (no metadata to print)
        } else {
            size_t remaining = imgfs_file->header.nb_files; // number of existing file

            // Look for metadata and exit loop if all metadata has been printed
            for (size_t i = 0 ; i < imgfs_file->header.max_files && remaining > 0; ++i) {
                if(imgfs_file->metadata[i].is_valid) {
                    print_metadata(&imgfs_file->metadata[i]);
                    --remaining;
                }
            }

        }
        break;
    case JSON: {
        //Create json object using proper function
        json_object* obj = json_object_new_object();
        if (obj == NULL) return ERR_RUNTIME;

        //Create json array using proper function
        json_object* array = json_object_new_array();
        if (array == NULL) {
            json_object_put(obj);
            return ERR_RUNTIME;
        }

        // Add the array to the JSON object
        if (json_object_object_add(obj, "Images", array) != 0) {
            json_object_put(array);
            json_object_put(obj);
            return ERR_RUNTIME;
        }

        //Iterate to find a valid metadata in the imgfs_file

        size_t remaining = imgfs_file->header.nb_files; // number of existing files

        // Look for metadata and exit loop if all metadata has been printed
        for (size_t i = 0 ; i < imgfs_file->header.max_files && remaining > 0; ++i) {
            if(imgfs_file->metadata[i].is_valid) {
                // If you find a valide metadata create a json_string
                json_object* string = json_object_new_string(imgfs_file->metadata[i].img_id);
                if (string == NULL) {
                    json_object_put(obj);
                    return ERR_RUNTIME;
                }
                // Add the string to the array
                json_object_array_add(array, string);
                --remaining;
            }
        }

        // Now need to convert the json_ojec to json_string
        const char* str = json_object_to_json_string(obj);
        if(str == NULL) {
            json_object_put(obj);
            return ERR_RUNTIME;
        }

        *json = calloc(strlen(str) + 1, 1);
        if (*json == NULL) {
            json_object_put(obj);
            return ERR_OUT_OF_MEMORY;
        }

        strcpy(*json, str);

        //Free the the json_obj
        json_object_put(obj);
        break;
    }
    default:
        break;
    }

    return ERR_NONE;
}
