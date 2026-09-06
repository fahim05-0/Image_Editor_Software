#include <stdio.h>
#include <stdlib.h>
#include "image.h"

/* ---------------------------------------------------------
   create_image()

   Allocates memory for a new Image in TWO steps:
     1. The Image struct itself (holds width, height, and a
        pointer to the pixel array).
     2. The pixel array (width * height Pixels), pointed to
        by image->data.

   These are two separate malloc() calls because the struct
   and the array it points to are different memory blocks --
   the struct only stores the *address* of the array, not the
   array's contents directly.

   Every malloc() is checked for NULL, so a failed allocation
   is reported cleanly instead of crashing later when we try
   to write into memory that was never actually given to us.
   --------------------------------------------------------- */
Image *create_image(int width, int height)
{
    Image *image = malloc(sizeof(Image));

    if (image == NULL)
        return NULL;

    image->width = width;
    image->height = height;

    image->data = malloc(width * height * sizeof(Pixel));

    if (image->data == NULL)
    {
        /* Struct allocation succeeded but the pixel array
           failed -- free the struct too, so we don't leak it. */
        free(image);
        return NULL;
    }

    return image;
}

/* ---------------------------------------------------------
   free_image()

   Frees the pixel array FIRST, then the struct itself.
   Order matters: once `image` is freed, image->data can no
   longer be safely read (it would be a dangling pointer), so
   the inner allocation must be released before the outer one.
   --------------------------------------------------------- */
void free_image(Image *image)
{
    if (image != NULL)
    {
        free(image->data);
        free(image);
    }
}