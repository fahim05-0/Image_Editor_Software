#ifndef PROCESSING_H
#define PROCESSING_H

#include "image.h"

/* ---------------------------------------------------------
   These are the actual image-manipulation algorithms,
   implemented by hand (as the assignment requires) rather
   than by calling a ready-made library function.

   Functions that keep the same width/height (grayscale,
   brightness, invert, flip, blur) modify the Image IN PLACE
   through the pointer -- no return value needed.

   Functions that change the dimensions (rotate, crop) can't
   modify in place, so they allocate and return a brand new
   Image*. The caller is responsible for free_image()-ing the
   old one and switching to the new pointer.
   --------------------------------------------------------- */

void apply_grayscale(Image *image);
void adjust_brightness(Image *image, int value);
void invert_image(Image *image);

void flip_horizontal(Image *image);
void flip_vertical(Image *image);

Image *rotate_90_clockwise(Image *image);

Image *crop_image(Image *image,
                   int x,
                   int y,
                   int width,
                   int height);

void apply_blur(Image *image);

/* Undo support: save_undo() snapshots the image BEFORE a
   change is applied; undo_image() restores that snapshot.
   Only one level of undo is kept (a second save_undo() call
   overwrites the previous snapshot). */
void save_undo(const Image *image);
void undo_image(Image *image);

Image *copy_image(const Image *source);

#endif