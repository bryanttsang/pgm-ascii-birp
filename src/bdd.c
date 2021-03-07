#include <stdlib.h>
#include <stdio.h>

#include "bdd.h"
#include "debug.h"

/*
 * Macros that take a pointer to a BDD node and obtain pointers to its left
 * and right child nodes, taking into account the fact that a node N at level l
 * also implicitly represents nodes at levels l' > l whose left and right children
 * are equal (to N).
 *
 * You might find it useful to define macros to do other commonly occurring things;
 * such as converting between BDD node pointers and indices in the BDD node table.
 */
#define LEFT(np, l) ((l) > (np)->level ? (np) : bdd_nodes + (np)->left)
#define RIGHT(np, l) ((l) > (np)->level ? (np) : bdd_nodes + (np)->right)

int unused = BDD_NUM_LEAVES;
int serial = 0;

int hash(int level, int left, int right) {
    return ((((left * right) + level) & 0x7FFFFFFF) % BDD_HASH_SIZE);
}

int bdd_lookup(int level, int left, int right) {
    if (left == right) {
        return left;
    }
    int hashVal = hash(level, left, right);
    BDD_NODE *node = *(bdd_hash_map + hashVal);
    while (node != NULL) {
        if (node->level == level && node->left == left && node->right == right) {
            return *(bdd_hash_map + hashVal) - bdd_nodes;
        }
        hashVal = (hashVal+1) % BDD_HASH_SIZE;
        node = *(bdd_hash_map + hashVal);
    }
    BDD_NODE newNode = {level, left, right};
    *(bdd_nodes + unused) = newNode;
    *(bdd_hash_map + hashVal) = (bdd_nodes + unused);
    unused++;
    return *(bdd_hash_map + hashVal) - bdd_nodes;
}

int bdd_min_level(int w, int h) {
    int l = 0;
    while (1<<l < w*w || 1<<l < h*h) {
        l += 2;
    }
    return l;
}

int bfrhelp(int level, int w, int h, int ow, int oh, int rw, int rh, unsigned char *raster) {
    if (level == 0) {
        if (rw >= ow || rh >= oh) {
            return 0;
        }
        return *(raster + ow*rh + rw);
    }
    if (level%2 == 0) {
        int l = bfrhelp(level-1, w, h/2, ow, oh, rw, rh, raster);
        int r = bfrhelp(level-1, w, h/2, ow, oh, rw, rh + h/2, raster);
        return bdd_lookup(level, l, r);
    } else {
        int l = bfrhelp(level-1, w/2, h, ow, oh, rw, rh, raster);
        int r = bfrhelp(level-1, w/2, h, ow, oh, rw + w/2, rh, raster);
        return bdd_lookup(level, l, r);
    }
}

BDD_NODE *bdd_from_raster(int w, int h, unsigned char *raster) {
    if (w > 8192 || h > 8192) {
        return NULL;
    }
    int bml = bdd_min_level(w, h);
    BDD_NODE *root = (bdd_nodes + bfrhelp(bml, 1<<(bml/2), 1<<(bml/2), w, h, 0, 0, raster));
    return root;
}

void bdd_to_raster(BDD_NODE *node, int w, int h, unsigned char *raster) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            int a = bdd_apply(node, i, j);
            *(raster + i*w + j) = a;
        }
    }
}

int bshelp(BDD_NODE *node, FILE *out) {
    if (node->level == 0) {
        if (*(bdd_index_map + (node - bdd_nodes)) == 0) {
            fputc('@', out);
            fputc(node - bdd_nodes, out);
            serial++;
            *(bdd_index_map + (node - bdd_nodes)) = serial;
            return serial;
        }
        return *(bdd_index_map + (node - bdd_nodes));
    }
    if (*(bdd_index_map + bdd_lookup(node->level, node->left, node->right)) == 0) {
        int l = bshelp(bdd_nodes + node->left, out);
        int r = bshelp(bdd_nodes + node->right, out);
        serial++;
        *(bdd_index_map + bdd_lookup(node->level, node->left, node->right)) = serial;
        fputc('@' + node->level, out);
        fputc(l & 0xFF, out);
        fputc((l>>8) & 0xFF, out);
        fputc((l>>16) & 0xFF, out);
        fputc((l>>24) & 0xFF, out);
        fputc(r & 0xFF, out);
        fputc((r>>8) & 0xFF, out);
        fputc((r>>16) & 0xFF, out);
        fputc((r>>24) & 0xFF, out);
        return serial;
    }
    return *(bdd_index_map + bdd_lookup(node->level, node->left, node->right));
}

int bdd_serialize(BDD_NODE *node, FILE *out) {
    if (node == NULL) {
        return -1;
    }
    serial = 0;
    for (int i = 0; i < BDD_NODES_MAX; i++) {
        *(bdd_index_map + i) = 0;
    }
    bshelp(node, out);
    return 0;
}

BDD_NODE *bdd_deserialize(FILE *in) {
    if (in == NULL) {
        return NULL;
    }
    serial = 0;
    for (int i = 0; i < BDD_NODES_MAX; i++) {
        *(bdd_index_map + i) = 0;
    }
    int c;
    int v;
    do {
        c = fgetc(in);
        if (feof(in)) {
            break;
        }
        serial++;
        if (c == '@') {
            v = fgetc(in);
            if (feof(in) || v < 0 || v > 255) {
                return NULL;
            }
            *(bdd_index_map + serial-1) = v;
        }
        else if ('@' < c && c <= '`') {
            unsigned int vl = 0;
            unsigned int vr = 0;
            for (int i = 0; i < 4; i++) {
                v = fgetc(in);
                if (feof(in) || v < 0 || v > 255) {
                    return NULL;
                }
                vl += (v<<(i*8));
            }
            for (int i = 0; i < 4; i++) {
                v = fgetc(in);
                if (feof(in) || v < 0 || v > 255) {
                    return NULL;
                }
                vr += (v<<(i*8));
            }
            *(bdd_index_map + serial-1) = bdd_lookup(c-'@', *(bdd_index_map + vl-1), *(bdd_index_map + vr-1)); 
        }
        else {
            return NULL;
        }
    } while (1);
    return bdd_nodes + *(bdd_index_map + serial-1);
}

unsigned char bdd_apply(BDD_NODE *node, int r, int c) {
    if (r >= (1<<((node->level)/2)) || c >= (1<<((node->level)/2)) || r < 0 || c < 0) {
        return 0;
    }
    BDD_NODE *n = node;
    while (n->level > 0) {
        if (n->level % 2 == 0) {
            if (((r >> ((n->level - 2) / 2)) & 0x1) == 0) {
                n = bdd_nodes + n->left;
            }
            else {
                n = bdd_nodes + n->right;
            }
        }
        else {
            if (((c >> ((n->level - 1) / 2)) & 0x1) == 0) {
                n = bdd_nodes + n->left;
            }
            else {
                n = bdd_nodes + n->right;
            }
        }
    }
    return n - bdd_nodes;
}

int bmhelp(BDD_NODE *node, unsigned char (*func)(unsigned char)) {
    if (node->level == 0) {
        return func(node - bdd_nodes);
    }
    int l = bmhelp(bdd_nodes + node->left, func);
    int r = bmhelp(bdd_nodes + node->right, func);
    return bdd_lookup(node->level, l, r);
}

BDD_NODE *bdd_map(BDD_NODE *node, unsigned char (*func)(unsigned char)) {
    if (node == NULL) {
        return NULL;
    }
    BDD_NODE *root = (bdd_nodes + bmhelp(node, func));
    return root;
}

int brhelp(BDD_NODE *node, int level, int r, int c, int d) {
    if (d == 1) {
        return bdd_apply(node, r, c);
    }
    int tl = brhelp(node, level-2, r, c, d/2);
    int tr = brhelp(node, level-2, r, c + d/2, d/2);
    int bl = brhelp(node, level-2, r + d/2, c, d/2);
    int br = brhelp(node, level-2, r + d/2, c + d/2, d/2);
    int t = bdd_lookup(level-1, tr, br);
    int b = bdd_lookup(level-1, tl, bl);
    return bdd_lookup(level, t, b);
}

BDD_NODE *bdd_rotate(BDD_NODE *node, int level) {
    if (node == NULL || level%2 != 0 || level < 0) {
        return NULL;
    }
    BDD_NODE *root = (bdd_nodes + brhelp(node, level, 0, 0, 1<<(level/2)));
    return root;
}

int zoom_in(BDD_NODE *node, int level, int r, int c, int w, int h, int factor) {
    if (level == 0) {
        return bdd_apply(node, r, c);
    }
    if (level%2 == 0) {
        int left = zoom_in(node, level-1, r, c, w, h/2, factor);
        int right = zoom_in(node, level-1, r + h/2, c, w, h/2, factor);
        return bdd_lookup(level+factor, left, right);
    } else {
        int left = zoom_in(node, level-1, r, c, w/2, h, factor);
        int right = zoom_in(node, level-1, r, c + w/2, w/2, h, factor);
        return bdd_lookup(level+factor, left, right);
    }
}

int zoom_out(BDD_NODE *node, int level, int r, int c, int w, int h, int factor) {
    if (level == factor) {
        for (int i = r; i < r+w; i++) {
            for (int j = c; j < c+h; j++) {
                if (bdd_apply(node, i, j) != 0) {
                    return 255;
                }
            }
        }
        return 0;
    }
    if (level%2 == 0) {
        int left = zoom_out(node, level-1, r, c, w, h/2, factor);
        int right = zoom_out(node, level-1, r + h/2, c, w, h/2, factor);
        return bdd_lookup(level-factor, left, right);
    } else {
        int left = zoom_out(node, level-1, r, c, w/2, h, factor);
        int right = zoom_out(node, level-1, r, c + w/2, w/2, h, factor);
        return bdd_lookup(level-factor, left, right);
    }
}

BDD_NODE *bdd_zoom(BDD_NODE *node, int level, int factor) {
    if (node == NULL) {
        return NULL;
    }
    if (factor == 0) {
        return node;
    }
    int sign = (factor>>7) & 1;
    if (sign == 0) {
        if (level + 2*factor > 32) {
            return NULL;
        }
        BDD_NODE *root = (bdd_nodes + zoom_in(node, level, 0, 0, 1<<(level/2), 1<<(level/2), 2*factor));
        return root;
    } 
    else {
        sign = ((factor ^ 0xFF)+1) & 0xFF;
        if (sign > level/2) {
            sign = level/2;
        }
        BDD_NODE *root = (bdd_nodes + zoom_out(node, level, 0, 0, 1<<(level/2), 1<<(level/2), 2*sign));
        return root;
    }
}
