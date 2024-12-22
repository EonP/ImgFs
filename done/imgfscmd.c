/**
 * @file imgfscmd.c
 * @brief imgFS command line interpreter for imgFS core commands.
 *
 * Image Filesystem Command Line Tool
 *
 * @author Mia Primorac
 */

#include "imgfs.h"
#include "imgfscmd_functions.h"
#include "util.h"   // for _unused

#include <stdlib.h>
#include <string.h>
#include <vips/vips.h>

// Function pointer for commands
typedef int (*command)(int argc, char* argv[]);

// Structure mapping command names to corresponding functions
struct command_mapping {
    const char* commandName;
    command commandFunc;
};

// Number of commands
#define NUM_COMMANDS 6

// Array of all necessary command mappings (mapping command names to their functions)
const struct command_mapping commands[NUM_COMMANDS] = {
    {"list", do_list_cmd},
    {"create", do_create_cmd},
    {"help", help},
    {"delete", do_delete_cmd},
    {"read", do_read_cmd},
    {"insert", do_insert_cmd}
};

/*******************************************************************************
 * MAIN
 */
int main(int argc, char* argv[])
{
    int ret = 0;

    if (argc < 2) {
        ret = ERR_NOT_ENOUGH_ARGUMENTS;
    } else {
        /* **********************************************************************
         * TODO WEEK 07: THIS PART SHALL BE EXTENDED.
         * **********************************************************************
         */

        // Initialize vips library
        if (VIPS_INIT(argv[0])) {
            return ERR_IMGLIB;
        }

        ret = ERR_INVALID_COMMAND;

        // Look for a matching command
        for (int i = 0; i < NUM_COMMANDS; ++i) {
            // Call the command if program name exists as a command function
            if (strcmp(argv[1], commands[i].commandName) == 0) {
                argc -= 2; argv += 2; // Skip command name and program name
                ret = commands[i].commandFunc(argc, argv); // Calls the requested command
                break;
            }
        }

    }

    // Check for errors
    if (ret) {
        fprintf(stderr, "ERROR: %s\n", ERR_MSG(ret));
        help(argc, argv);
    }

    // Shutting down the vips library
    vips_shutdown();
    return ret;
}
