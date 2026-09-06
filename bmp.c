#include <stdlib.h>
#include "bmp.h"

/* stb_image / stb_image_write are third-party, single-header
   libraries (not written by us -- downloaded separately).
   They handle the complex, low-level BMP file format details
   (headers, byte alignment, etc.) so we don't have to.

   The IMPLEMENTATION defines below must appear EXACTLY ONCE
   in the whole project, in exactly one .c file (this one).
   They tell the header "also compile the actual function
   bodies here, not just the declarations." If any other .c
   file also defines these, you'll get duplicate-symbol linker
   errors. */
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

/* ---------------------------------------------------------
   load_bmp()

   1. stbi_load() reads the file and hands back a flat byte
      array in the form R,G,B,R,G,B,... (the `3` argument
      forces exactly 3 channels, even if the source file has
      a different channel count).
   2. We copy those bytes into OUR OWN Image/Pixel structs,
      because the rest of the program (processing.c, main.c)
      is built around our Image type, not stb's raw array.
   3. stbi_image_free() releases stb's buffer -- note this is
      stb's own free function, not the standard free(), since
      stb manages that allocation internally.
   --------------------------------------------------------- */
Image *load_bmp(const char *filename)
{
    int width;
    int height;
    int channels;

    unsigned char *pixels =
        stbi_load(filename, &width, &height, &channels, 3);

    if (pixels == NULL)
        return NULL;

    Image *image = create_image(width, height);

    if (image == NULL)
    {
        stbi_image_free(pixels);
        return NULL;
    }

    for (int i = 0; i < width * height; i++)
    {
        image->data[i].r = pixels[i * 3];
        image->data[i].g = pixels[i * 3 + 1];
        image->data[i].b = pixels[i * 3 + 2];
    }

    stbi_image_free(pixels);

    return image;
}

/* ---------------------------------------------------------
   save_bmp()

   Our Image->data is already a tightly packed array of
   {r,g,b} Pixel structs -- which happens to have the exact
   same memory layout stbi_write_bmp() expects (3 bytes per
   pixel, row by row). So we can pass image->data straight in,
   with no manual conversion needed.
   --------------------------------------------------------- */
int save_bmp(const char *filename, const Image *image)
{
    int result =
        stbi_write_bmp(
            filename,
            image->width,
            image->height,
            3,
            image->data
        );

    return result;
}