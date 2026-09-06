#ifndef BMP_H
#define BMP_H

#include "image.h"

/* Reads a BMP file from disk into a new Image (our own struct,
   not the raw library format). Returns NULL on failure. */
Image *load_bmp(const char *filename);

/* Writes an Image out to disk as a BMP file.
   Returns non-zero (true) on success, 0 on failure. */
int save_bmp(const char *filename, const Image *image);

#endif