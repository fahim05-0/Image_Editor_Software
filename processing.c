#include <stdlib.h>
#include "processing.h"

/* Holds the single saved "undo" snapshot. `static` means this
   pointer is only visible inside this file -- no other .c file
   can read or modify it directly, only through save_undo()/
   undo_image(). This keeps the undo mechanism encapsulated. */
static Image *undo_image_data = NULL;


/* ---------------------------------------------------------
   apply_grayscale()

   Converts every pixel to a shade of grey using a WEIGHTED
   average, not a simple (r+g+b)/3 average. The human eye is
   most sensitive to green, then red, then blue -- these
   particular coefficients (0.299, 0.587, 0.114) come from the
   NTSC standard and produce a grey value that "looks" like the
   correct brightness to a human viewer.
   --------------------------------------------------------- */
void apply_grayscale(Image *image)
{
    for (int y = 0; y < image->height; y++)
    {
        for (int x = 0; x < image->width; x++)
        {
            Pixel *p =
                &image->data[y * image->width + x];

            int gray =
                0.299 * p->r +
                0.587 * p->g +
                0.114 * p->b;

            p->r = gray;
            p->g = gray;
            p->b = gray;
        }
    }
}


/* ---------------------------------------------------------
   adjust_brightness()

   Adds `value` to every channel. The addition is done in a
   plain `int`, NOT directly in the unsigned char field --
   because unsigned char can only hold 0-255. If we added
   directly and the result went above 255 or below 0, it would
   silently WRAP AROUND (e.g. 250 + 20 would become 14, not
   255) due to integer overflow on an 8-bit type. Doing the
   math in `int` first, then clamping into [0,255], avoids that.
   --------------------------------------------------------- */
void adjust_brightness(Image *image, int value)
{
    for (int y = 0; y < image->height; y++)
    {
        for (int x = 0; x < image->width; x++)
        {
            Pixel *p =
                &image->data[y * image->width + x];

            int r = p->r + value;
            int g = p->g + value;
            int b = p->b + value;

            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;

            if (r < 0) r = 0;
            if (g < 0) g = 0;
            if (b < 0) b = 0;

            p->r = r;
            p->g = g;
            p->b = b;
        }
    }
}


/* Simple photographic negative: each channel becomes its
   "opposite" on the 0-255 scale. */
void invert_image(Image *image)
{
    for (int y = 0; y < image->height; y++)
    {
        for (int x = 0; x < image->width; x++)
        {
            Pixel *p =
                &image->data[y * image->width + x];

            p->r = 255 - p->r;
            p->g = 255 - p->g;
            p->b = 255 - p->b;
        }
    }
}


/* ---------------------------------------------------------
   flip_horizontal()

   Swaps each pixel with its mirror on the opposite side of
   the same row. The loop only goes up to width/2 -- if it
   went all the way to width, every pair would get swapped
   TWICE, ending up back in the original (unflipped) order.
   --------------------------------------------------------- */
void flip_horizontal(Image *image)
{
    for (int y = 0; y < image->height; y++)
    {
        for (int x = 0; x < image->width / 2; x++)
        {
            int opposite = image->width - 1 - x;

            Pixel temp =
                image->data[y * image->width + x];

            image->data[y * image->width + x] =
                image->data[y * image->width + opposite];

            image->data[y * image->width + opposite] =
                temp;
        }
    }
}


/* Same idea as flip_horizontal, but swapping rows (top <-> bottom)
   instead of columns (left <-> right). */
void flip_vertical(Image *image)
{
    for (int y = 0; y < image->height / 2; y++)
    {
        int opposite = image->height - 1 - y;

        for (int x = 0; x < image->width; x++)
        {
            Pixel temp =
                image->data[y * image->width + x];

            image->data[y * image->width + x] =
                image->data[opposite * image->width + x];

            image->data[opposite * image->width + x] =
                temp;
        }
    }
}


/* ---------------------------------------------------------
   rotate_90_clockwise()

   Rotating changes the dimensions (a wide image becomes tall
   and vice versa), so we CANNOT rotate in place -- we need a
   brand new Image with width and height swapped.

   For each source pixel at (x, y), we work out where it lands
   in the rotated image:
     new_x = image->height - 1 - y
     new_y = x
   (Picture physically turning the photo 90 degrees clockwise --
   the old left edge becomes the new top edge, etc.)
   --------------------------------------------------------- */
Image *rotate_90_clockwise(Image *image)
{
    Image *rotated =
        create_image(image->height, image->width);  /* dimensions swapped */

    if (rotated == NULL)
        return NULL;

    for (int y = 0; y < image->height; y++)
    {
        for (int x = 0; x < image->width; x++)
        {
            int new_x = image->height - 1 - y;
            int new_y = x;

            rotated->data[new_y * rotated->width + new_x] =
                image->data[y * image->width + x];
        }
    }

    return rotated;
}


/* ---------------------------------------------------------
   crop_image()

   Extracts a rectangular region starting at (x, y) with the
   given width/height. First validates that the requested
   rectangle actually fits inside the source image -- if any
   part of it would fall outside, the whole operation is
   rejected (returns NULL) rather than reading out-of-bounds
   memory.
   --------------------------------------------------------- */
Image *crop_image(Image *image, int x, int y,
                  int width, int height)
{
    if (x < 0 || y < 0 ||
        width <= 0 || height <= 0 ||
        x + width > image->width ||
        y + height > image->height)
    {
        return NULL;
    }

    Image *cropped = create_image(width, height);

    if (cropped == NULL)
        return NULL;

    for (int j = 0; j < height; j++)
    {
        for (int i = 0; i < width; i++)
        {
            cropped->data[j * width + i] =
                image->data[(y + j) * image->width + (x + i)];
        }
    }

    return cropped;
}


/* ---------------------------------------------------------
   apply_blur()

   3x3 box blur: each output pixel is the average of itself
   and its up-to-8 neighbours.

   CRITICAL DETAIL: we write results into a SEPARATE `blurred`
   image instead of overwriting `image` directly while looping.
   If we wrote directly into `image`, then by the time we got
   to pixel (x+1, y) its neighbour (x, y) would already hold
   the NEW blurred value instead of the ORIGINAL one -- corrupting
   every calculation after the first pixel. Using a separate
   output buffer means every pixel's blur is computed from the
   untouched original data, then the whole result is copied
   back into `image` at the end.

   `count` varies for edge/corner pixels (they have fewer than
   8 neighbours), so we divide by however many neighbours were
   actually found, not always by 9.
   --------------------------------------------------------- */
void apply_blur(Image *image)
{
    Image *blurred = create_image(image->width, image->height);

    if (blurred == NULL)
        return;

    for (int y = 0; y < image->height; y++)
    {
        for (int x = 0; x < image->width; x++)
        {
            int sum_r = 0;
            int sum_g = 0;
            int sum_b = 0;
            int count = 0;

            for (int dy = -1; dy <= 1; dy++)
            {
                for (int dx = -1; dx <= 1; dx++)
                {
                    int nx = x + dx;
                    int ny = y + dy;

                    /* Skip neighbours that fall outside the image
                       (this happens for pixels on the border). */
                    if (nx >= 0 && nx < image->width &&
                        ny >= 0 && ny < image->height)
                    {
                        Pixel *p =
                            &image->data[ny * image->width + nx];

                        sum_r += p->r;
                        sum_g += p->g;
                        sum_b += p->b;

                        count++;
                    }
                }
            }

            Pixel *p =
                &blurred->data[y * blurred->width + x];

            p->r = sum_r / count;
            p->g = sum_g / count;
            p->b = sum_b / count;
        }
    }

    /* Copy the blurred result back into the original image,
       then free the temporary buffer. */
    for (int i = 0; i < image->width * image->height; i++)
    {
        image->data[i] = blurred->data[i];
    }

    free_image(blurred);
}


/* ---------------------------------------------------------
   save_undo()

   Called right BEFORE a destructive operation. Frees whatever
   was previously saved (only one level of undo is kept -- a
   new save_undo() call always replaces the old snapshot), then
   stores a full DEEP COPY of the current image. It has to be a
   deep copy (a new, independent pixel array), not just saving
   the pointer -- otherwise, once the caller modifies the real
   image in place, the "saved" snapshot would show the modified
   data too, since it'd be pointing at the same memory.
   --------------------------------------------------------- */
void save_undo(const Image *image)
{
    if (undo_image_data != NULL)
    {
        free_image(undo_image_data);
        undo_image_data = NULL;
    }

    undo_image_data = copy_image(image);
}


/* ---------------------------------------------------------
   undo_image()

   Restores the most recent snapshot into `image`. If the
   snapshot has different dimensions than the current image
   (e.g. undoing a crop or rotate), the existing pixel array is
   freed and a new one of the correct size is allocated before
   copying the data back in.

   After restoring, the snapshot is freed and cleared -- so
   calling undo twice in a row without a new save_undo() in
   between does nothing the second time (only one level of undo).
   --------------------------------------------------------- */
void undo_image(Image *image)
{
    if (undo_image_data == NULL)
        return;

    if (image->width != undo_image_data->width ||
        image->height != undo_image_data->height)
    {
        free(image->data);

        image->width = undo_image_data->width;
        image->height = undo_image_data->height;

        image->data =
            malloc(image->width * image->height * sizeof(Pixel));
    }

    for (int i = 0;
         i < undo_image_data->width * undo_image_data->height;
         i++)
    {
        image->data[i] = undo_image_data->data[i];
    }

    free_image(undo_image_data);
    undo_image_data = NULL;
}


/* Allocates a new Image and copies every pixel from `source`
   into it -- a true deep copy, fully independent memory. */
Image *copy_image(const Image *source)
{
    if (source == NULL)
        return NULL;

    Image *copy = create_image(source->width, source->height);

    if (copy == NULL)
        return NULL;

    for (int i = 0;
         i < source->width * source->height;
         i++)
    {
        copy->data[i] = source->data[i];
    }

    return copy;
}