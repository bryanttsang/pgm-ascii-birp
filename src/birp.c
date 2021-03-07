/*
 * BIRP: Binary decision diagram Image RePresentation
 */

#include "image.h"
#include "bdd.h"
#include "const.h"
#include "debug.h"

int pgm_to_birp(FILE *in, FILE *out) {
    int width, height;
    if (img_read_pgm(in, &width, &height, raster_data, RASTER_SIZE_MAX) == -1) {
        return -1;
    }
    img_write_birp(bdd_from_raster(width, height, raster_data), width, height, out);
    return 0;
}

int birp_to_pgm(FILE *in, FILE *out) {
    int width, height;
    BDD_NODE *root = img_read_birp(in, &width, &height);
    if (root == NULL) {
        return -1;
    }
    bdd_to_raster(root, width, height, raster_data);
    if (img_write_pgm(raster_data, width, height, out) == -1) {
        return -1;
    }
    return 0;
}

unsigned char negative(unsigned char value) {
    return 255 - value;
}

unsigned char threshold(unsigned char value) {
    int t = (global_options>>16) & 0xFF;
    if (value < t) {
        return 0;
    }
    return 255;
}

int birp_to_birp(FILE *in, FILE *out) {
    int width, height;
    BDD_NODE *root = img_read_birp(in, &width, &height);
    if (root == NULL) {
        return -1;
    }
    int tform = (global_options>>8) & 0xF;
    if (tform == 0) {
        if (img_write_birp(root, width, height, out) == -1) {
            return -1;
        }
    }
    if (tform == 1) {
        if (img_write_birp(bdd_map(root, negative), width, height, out) == -1) {
            return -1;
        }
    }
    if (tform == 2) {
        if (img_write_birp(bdd_map(root, threshold), width, height, out) == -1) {
            return -1;
        }
    }
    if (tform == 3) {
        int factor = (global_options>>16) & 0xFF;
        if (factor == 0) {
            img_write_birp(root, width, height, out);
            return 0;
        }
        int sign = (factor>>7) & 1;
        int bml = bdd_min_level(width, height);
        if (sign == 0) {
            if (img_write_birp(bdd_zoom(root, bml, factor), 1<<(bml/2 + factor), 1<<(bml/2 + factor), out) == -1) {
                return -1;
            }
        }
        if (sign == 1) {
            sign = ((factor ^ 0xFF)+1) & 0xFF;
            if (sign > bml/2) {
                sign = bml/2;
            }
            if (img_write_birp(bdd_zoom(root, bml, factor), 1<<(bml/2 - sign), 1<<(bml/2 - sign), out) == -1) {
                return -1;
            }
        }
    }
    if (tform == 4) {
        int bml = bdd_min_level(width, height);
        if (img_write_birp(bdd_rotate(root, bml), 1<<(bml/2), 1<<(bml/2), out) == -1) {
            return -1;
        }
    }
    return 0;
}

int pgm_to_ascii(FILE *in, FILE *out) {
    int width, height;
    if (img_read_pgm(in, &width, &height, raster_data, RASTER_SIZE_MAX) == -1) {
        return -1;
    }
    for (int i = 0; i < height*width; i++) {
        if (0 <= *(raster_data + i) && *(raster_data + i) <= 63) {
            fputc(' ', out);
        }
        else if (64 <= *(raster_data + i) && *(raster_data + i) <= 127) {
            fputc('.', out);
        }
        else if (128 <= *(raster_data + i) && *(raster_data + i) <= 191) {
            fputc('*', out);
        }
        else if (192 <= *(raster_data + i) && *(raster_data + i) <= 255) {
            fputc('@', out);
        }
        else {
            return -1;
        }
        if (i % width == width-1) {
            fputc('\n', out);
        }
    }
    return 0;
}

int birp_to_ascii(FILE *in, FILE *out) {
    int width, height;
    BDD_NODE *root = img_read_birp(in, &width, &height);
    if (root == NULL) {
        return -1;
    }
    bdd_to_raster(root, width, height, raster_data);
    for (int i = 0; i < height*width; i++) {
        if (0 <= *(raster_data + i) && *(raster_data + i) <= 63) {
            fputc(' ', out);
        }
        else if (64 <= *(raster_data + i) && *(raster_data + i) <= 127) {
            fputc('.', out);
        }
        else if (128 <= *(raster_data + i) && *(raster_data + i) <= 191) {
            fputc('*', out);
        }
        else if (192 <= *(raster_data + i) && *(raster_data + i) <= 255) {
            fputc('@', out);
        }
        else {
            return -1;
        }
        if (i % width == width-1) {
            fputc('\n', out);
        }
    }
    return 0;
}

int streq(char *str1, char *str2) {
    char s1 = *str1;
    char s2 = *str2;
    for (int i = 0; (s1 != 0) && (s2 != 0); i++) {
        s1 = *(str1+i);
        s2 = *(str2+i);
        if (s1 != s2) {
            return 0;
        }
    }
    return s1 == s2;
}

int strtoint(char *str) {
    char s = *str;
    int n = 0;
    for (int i = 0; ; i++) {
        if (i > 3) {
            return -1;
        }
        s = *(str+i);
        if (!s) {
            break;
        }
        if (s < 48 || s > 57) {
            return -1;
        }
        n *= 10;
        n += (s - 48);
    }
    return n;
}

/**
 * @brief Validates command line arguments passed to the program.
 * @details This function will validate all the arguments passed to the
 * program, returning 0 if validation succeeds and -1 if validation fails.
 * Upon successful return, the various options that were specifed will be
 * encoded in the global variable 'global_options', where it will be
 * accessible elsewhere int the program.  For details of the required
 * encoding, see the assignment handout.
 *
 * @param argc The number of arguments passed to the program from the CLI.
 * @param argv The argument strings passed to the program from the CLI.
 * @return 0 if validation succeeds and -1 if validation fails.
 * @modifies global variable "global_options" to contain an encoded representation
 * of the selected program options.
 */
int validargs(int argc, char **argv) {
    global_options = 0;
    int i = 0;
    char *arg;
    arg = *argv++;
    global_options = 34;
    int ibirp = 1;
    int obirp = 1;
    int input = 1;
    int output = 1;
    int transform = 1;
    while ((i++) < argc-1) {
        arg = *argv++;
        if (streq(arg, "-h")) {
            if (i == 1) {
                global_options = 0x80000000;
                return 0;
            }
            return -1;
        }
        else if (streq(arg, "-i")) {
            if ((i > 3) || !input || !transform) {
                return -1;
            }
            arg = *argv++;
            if (!arg) {
                return -1;
            }
            i++;
            if (streq(arg, "pgm")) {
                global_options &= 16777200;
                global_options |= 1;
                ibirp = 0;
                input = 0;
            }
            else if (streq(arg, "birp")) {
                input = 0;
            }
            else {
                return -1;
            }
        }
        else if (streq(arg, "-o")) {
            if ((i > 3) || !output || !transform) {
                return -1;
            }
            arg = *argv++;
            if (!arg) {
                return -1;
            }
            i++;
            if (streq(arg, "pgm")) {
                global_options &= 16776975;
                global_options |= (1 << 4);
                obirp = 0;
                output = 0;
            }
            else if (streq(arg, "birp")) {
                output = 0;
            }
            else if (streq(arg, "ascii")) {
                global_options &= 16776975;
                global_options |= (3 << 4);
                obirp = 0;
                output = 0;
            }
            else {
                return -1;
            }
        }
        else if (streq(arg, "-n")) {
            if (ibirp && obirp && transform) {
                global_options |= (1 << 8);
                transform = 0;
            }
            else {
                return -1;
            }
        }
        else if (streq(arg, "-r")) {
            if (ibirp && obirp && transform) {
                global_options |= (4 << 8);
                transform = 0;
            }
            else {
                return -1;
            }
        }
        else if (streq(arg, "-t")) {
            if (ibirp && obirp && transform) {
                global_options |= (2 << 8);
                transform = 0;
                arg = *argv++;
                if (!arg) {
                    return -1;
                }
                i++;
                int range = strtoint(arg);
                if (range >= 0 && range <= 255) {
                    global_options |= (range << 16);
                }
                else {
                    return -1;
                }
            }
            else {
                return -1;
            }
        }
        else if (streq(arg, "-z")) {
            if (ibirp && obirp && transform) {
                global_options |= (3 << 8);
                transform = 0;
                arg = *argv++;
                if (!arg) {
                    return -1;
                }
                i++;
                int range = strtoint(arg);
                if (range == 0) {
                    range = 0;
                }
                else if (range > 0 && range <= 16) {
                    range ^= 255;
                    global_options |= ((range+1) << 16);
                }
                else {
                    return -1;
                }
            }
            else {
                return -1;
            }
        }
        else if (streq(arg, "-Z")) {
            if (ibirp && obirp && transform) {
                global_options |= (3 << 8);
                transform = 0;
                arg = *argv++;
                if (!arg) {
                    return -1;
                }
                i++;
                int range = strtoint(arg);
                if (range >= 0 && range <= 16) {
                    global_options |= (range << 16);
                }
                else {
                    return -1;
                }
            }
            else {
                return -1;
            }
        }
        else {
            return -1;
        }
    }
    return 0;
}
