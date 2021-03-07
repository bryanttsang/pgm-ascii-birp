#include <stdio.h>
#include <stdlib.h>

#include "const.h"
#include "debug.h"

#ifdef _STRING_H
#error "Do not #include <string.h>. You will get a ZERO."
#endif

#ifdef _STRINGS_H
#error "Do not #include <strings.h>. You will get a ZERO."
#endif

#ifdef _CTYPE_H
#error "Do not #include <ctype.h>. You will get a ZERO."
#endif

/*
 * Just a reminder: All non-main functions should
 * be in another file not named main.c
 */
int main(int argc, char **argv) {
    int exitcode = validargs(argc, argv);
    if (exitcode) {
        USAGE(*argv, EXIT_FAILURE);
        return EXIT_FAILURE;
    }
    if (global_options & HELP_OPTION) {
        USAGE(*argv, EXIT_SUCCESS);
        return EXIT_SUCCESS;
    }
    int conversion = global_options & 0xFF;
    if (conversion == 0x21) {
        if (pgm_to_birp(stdin, stdout) == -1) {
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
    if (conversion == 0x12) {
        if (birp_to_pgm(stdin, stdout) == -1) {
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
    if (conversion == 0x22) {
        if (birp_to_birp(stdin, stdout) == -1) {
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
    if (conversion == 0x31) {
        if (pgm_to_ascii(stdin, stdout) == -1) {
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
    if (conversion == 0x32) {
        if (birp_to_ascii(stdin, stdout) == -1) {
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}