#ifndef IMAGE_H
#define IMAGE_H

/* ---------------------------------------------------------
   Pixel

   One point in the image. 24-bit color = 3 channels,
   each 0-255, stored as unsigned char (1 byte each).
   --------------------------------------------------------- */
typedef struct
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
} Pixel;

/* ---------------------------------------------------------
   Image

   width, height  -> dimensions in pixels
   data           -> a FLAT (1D) dynamically allocated array
                      of width*height Pixels, stored row by
                      row (row-major order).

   To reach pixel (x, y):
       image->data[y * image->width + x]

   Why this formula works: row y starts at index (y * width)
   because the previous y rows each contain `width` pixels.
   Then +x moves across to the correct column in that row.
   --------------------------------------------------------- */
typedef struct
{
    int width;
    int height;
    Pixel *data;
} Image;

/* Allocates a new Image with the given size (pixel data is
   uninitialized garbage until you fill it in). Returns NULL
   if memory allocation fails. */
Image *create_image(int width, int height);

/* Releases an Image and its pixel data. Safe to call with NULL. */
void free_image(Image *image);

/* Creates a brand new Image that is a full (deep) copy of
   source -- used by the Undo system so the saved copy is
   completely independent of the original. */
Image *copy_image(const Image *source);

#endif