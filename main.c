#include <stdio.h>
#include <stdlib.h>

#include <iup.h>

#include "image.h"
#include "processing.h"
#include "bmp.h"

/* ---------------------------------------------------------
   GLOBAL STATE

   These are declared globally because both main() and the
   button callback functions (open_callback, grayscale_callback,
   etc.) need to read/modify the same image and GUI handles.
   --------------------------------------------------------- */

/* The image currently loaded in the editor (NULL if none loaded) */
Image *current_image = NULL;

/* The IUP label widget that displays the image on screen */
Ihandle *image_label = NULL;

/* The IUP image object created from current_image's pixel data.
   This is what actually gets drawn inside image_label. */
Ihandle *display_image = NULL;

/* Text box where the user types a brightness value */
Ihandle *brightness_input = NULL;

/* Text boxes for the crop region (X, Y, Width, Height) */
Ihandle *crop_x_input = NULL;
Ihandle *crop_y_input = NULL;
Ihandle *crop_w_input = NULL;
Ihandle *crop_h_input = NULL;


/* ---------------------------------------------------------
   refresh_image()

   Converts current_image (our own Image struct) into an
   IUP-compatible image and shows it inside image_label.

   IMPORTANT IUP QUIRK:
   Once a label has been "mapped" (i.e. the dialog has been
   shown with IupShowXY), you CANNOT switch it from
   text-mode/empty-mode into image-mode. It will silently do
   nothing. That's why, in main(), image_label is given a
   small placeholder IMAGE *before* the dialog is shown -- this
   puts it into "image mode" from the very start, so that later
   calls to change the IMAGE attribute (right here) actually work.
   --------------------------------------------------------- */
void refresh_image(void)
{
    if (current_image == NULL)
        return;

    /* Free the previous IUP image object before creating a new one,
       to avoid a memory leak every time refresh_image() runs. */
    if (display_image != NULL)
    {
        IupDestroy(display_image);
        display_image = NULL;
    }

    int total =
        current_image->width *
        current_image->height *
        3; /* 3 bytes per pixel: R, G, B */

    unsigned char *rgb =
        (unsigned char *)malloc(total);

    if (rgb == NULL)
    {
        IupMessage("Error", "Memory allocation failed.");
        return;
    }

    /* Convert our Pixel struct array (r,g,b,r,g,b...) into the
       flat byte array format IupImageRGB expects. */
    for (int i = 0;
         i < current_image->width * current_image->height;
         i++)
    {
        rgb[i * 3]     = current_image->data[i].r;
        rgb[i * 3 + 1] = current_image->data[i].g;
        rgb[i * 3 + 2] = current_image->data[i].b;
    }

    /* IupImageRGB copies the pixel data internally, so it's safe
       to free(rgb) right after this call. */
    display_image =
        IupImageRGB(
            current_image->width,
            current_image->height,
            rgb
        );

    free(rgb);

    if (display_image == NULL)
    {
        IupMessage(
            "Error",
            "Could not create IUP image."
        );
        return;
    }

    /* Register the image under a name so it can be referenced by
       string in IupSetAttribute below. */
    IupSetHandle(
        "CURRENT_IMAGE",
        display_image
    );

    /* Tell the label to display this image. Because image_label
       was already in "image mode" from the start (see main()),
       this update actually takes effect. */
    IupSetAttribute(
        image_label,
        "IMAGE",
        "CURRENT_IMAGE"
    );

    /* Redraw the dialog so the change becomes visible immediately. */
    IupRefresh(image_label);
}


/* ---------------------------------------------------------
   BUTTON CALLBACKS

   Each callback follows the same pattern:
     1. Make sure an image is loaded (otherwise show an error).
     2. Save the current state for Undo (where applicable).
     3. Call the actual processing function (from processing.c).
     4. Call refresh_image() to redraw the result on screen.

   None of these functions contain the actual image-processing
   algorithm -- that logic lives in processing.c, exactly as the
   assignment asks for (callbacks should just call the
   appropriate function, not contain the algorithm itself).
   --------------------------------------------------------- */

/* Open button: shows a file picker, loads the chosen BMP */
int open_callback(Ihandle *self)
{
    Ihandle *filedlg = IupFileDlg();

    IupSetAttribute(filedlg, "DIALOGTYPE", "OPEN");
    IupSetAttribute(filedlg, "FILTER", "*.bmp");
    IupSetAttribute(filedlg, "FILTERINFO", "BMP Image");

    IupPopup(filedlg, IUP_CENTER, IUP_CENTER);

    /* STATUS != -1 means the user picked a file (didn't cancel) */
    if (IupGetInt(filedlg, "STATUS") != -1)
    {
        char *filename = IupGetAttribute(filedlg, "VALUE");

        Image *new_image = load_bmp(filename);

        if (new_image == NULL)
        {
            IupMessage("Error", "Could not open BMP image.");
        }
        else
        {
            /* Free whatever was loaded before, to avoid a leak */
            if (current_image != NULL)
                free_image(current_image);

            current_image = new_image;

            refresh_image();

            IupMessage("Success",
                       "Image loaded successfully.");
        }
    }

    IupDestroy(filedlg);

    return IUP_DEFAULT;
}


/* Save button: shows a file picker, writes current_image to disk */
int save_callback(Ihandle *self)
{
    if (current_image == NULL)
    {
        IupMessage("Error", "No image is loaded.");
        return IUP_DEFAULT;
    }

    Ihandle *filedlg = IupFileDlg();

    IupSetAttribute(filedlg, "DIALOGTYPE", "SAVE");
    IupSetAttribute(filedlg, "FILTER", "*.bmp");
    IupSetAttribute(filedlg, "FILTERINFO", "BMP Image");

    IupPopup(filedlg, IUP_CENTER, IUP_CENTER);

    if (IupGetInt(filedlg, "STATUS") != -1)
    {
        char *filename = IupGetAttribute(filedlg, "VALUE");

        if (save_bmp(filename, current_image))
        {
            IupMessage("Success",
                       "Image saved successfully.");
        }
        else
        {
            IupMessage("Error",
                       "Could not save image.");
        }
    }

    IupDestroy(filedlg);

    return IUP_DEFAULT;
}


int grayscale_callback(Ihandle *self)
{
    if (current_image == NULL)
    {
        IupMessage("Error", "No image is loaded.");
        return IUP_DEFAULT;
    }

    save_undo(current_image);      /* remember state before change */
    apply_grayscale(current_image);
    refresh_image();

    return IUP_DEFAULT;
}


int invert_callback(Ihandle *self)
{
    if (current_image == NULL)
    {
        IupMessage("Error", "No image is loaded.");
        return IUP_DEFAULT;
    }

    save_undo(current_image);
    invert_image(current_image);
    refresh_image();

    return IUP_DEFAULT;
}


int horizontal_flip_callback(Ihandle *self)
{
    if (current_image == NULL)
    {
        IupMessage("Error", "No image is loaded.");
        return IUP_DEFAULT;
    }

    save_undo(current_image);
    flip_horizontal(current_image);
    refresh_image();

    return IUP_DEFAULT;
}


int vertical_flip_callback(Ihandle *self)
{
    if (current_image == NULL)
    {
        IupMessage("Error", "No image is loaded.");
        return IUP_DEFAULT;
    }

    save_undo(current_image);
    flip_vertical(current_image);
    refresh_image();

    return IUP_DEFAULT;
}


int blur_callback(Ihandle *self)
{
    if (current_image == NULL)
    {
        IupMessage("Error", "No image is loaded.");
        return IUP_DEFAULT;
    }

    save_undo(current_image);
    apply_blur(current_image);
    refresh_image();

    return IUP_DEFAULT;
}


/* Rotate is special: the output image has swapped width/height,
   so we get a brand new Image* back instead of editing in place. */
int rotate_callback(Ihandle *self)
{
    if (current_image == NULL)
    {
        IupMessage("Error", "No image is loaded.");
        return IUP_DEFAULT;
    }

    save_undo(current_image);

    Image *rotated = rotate_90_clockwise(current_image);

    if (rotated == NULL)
    {
        IupMessage("Error", "Could not rotate image.");
        return IUP_DEFAULT;
    }

    free_image(current_image);   /* discard the old image */
    current_image = rotated;     /* switch to the new one */

    refresh_image();

    return IUP_DEFAULT;
}


/* Undo restores whatever was saved by the most recent save_undo() call */
int undo_callback(Ihandle *self)
{
    if (current_image == NULL)
    {
        IupMessage("Error", "No image is loaded.");
        return IUP_DEFAULT;
    }

    undo_image(current_image);
    refresh_image();

    return IUP_DEFAULT;
}


int brightness_callback(Ihandle *self)
{
    if (current_image == NULL)
    {
        IupMessage("Error", "No image is loaded.");
        return IUP_DEFAULT;
    }

    /* Read whatever the user typed in the brightness text box */
    char *value =
        IupGetAttribute(brightness_input, "VALUE");

    int brightness = atoi(value);

    save_undo(current_image);

    adjust_brightness(current_image, brightness);

    refresh_image();

    return IUP_DEFAULT;
}


/* Crop is also special: it produces a smaller Image* with new
   dimensions, so again we swap current_image for a new pointer. */
int crop_callback(Ihandle *self)
{
    if (current_image == NULL)
    {
        IupMessage("Error", "No image is loaded.");
        return IUP_DEFAULT;
    }

    int x = atoi(IupGetAttribute(crop_x_input, "VALUE"));
    int y = atoi(IupGetAttribute(crop_y_input, "VALUE"));
    int width = atoi(IupGetAttribute(crop_w_input, "VALUE"));
    int height = atoi(IupGetAttribute(crop_h_input, "VALUE"));

    Image *cropped =
        crop_image(current_image, x, y, width, height);

    if (cropped == NULL)
    {
        IupMessage(
            "Error",
            "Invalid crop region."
        );

        return IUP_DEFAULT;
    }

    save_undo(current_image);

    free_image(current_image);

    current_image = cropped;

    refresh_image();

    return IUP_DEFAULT;
}


/* ---------------------------------------------------------
   main()

   Builds the GUI: creates every button/text box, wires up
   callbacks, arranges everything into rows, then shows the
   window and hands control over to IUP's event loop.
   --------------------------------------------------------- */
int main(int argc, char **argv)
{
    Ihandle *open_button;
    Ihandle *save_button;
    Ihandle *grayscale_button;
    Ihandle *invert_button;
    Ihandle *horizontal_button;
    Ihandle *vertical_button;
    Ihandle *blur_button;
    Ihandle *rotate_button;
    Ihandle *undo_button;

    Ihandle *dlg;
    Ihandle *brightness_button;
    Ihandle *crop_button;

    Ihandle *row1;
    Ihandle *row2;
    Ihandle *row3;
    Ihandle *crop_row;

    /* NOTE: brightness_input is NOT re-declared here.
       We deliberately reuse the GLOBAL brightness_input declared
       at the top of the file. If you declare "Ihandle
       *brightness_input;" again inside main(), it creates a local
       variable that *shadows* the global one -- main() would then
       set up the local copy, while brightness_callback() keeps
       reading the (still NULL) global copy, causing a crash. */

    IupOpen(&argc, &argv);   /* Always call this first, before
                                 any other IUP function. */

    /* ---------- Buttons ---------- */
    open_button = IupButton("Open", NULL);
    save_button = IupButton("Save", NULL);
    grayscale_button = IupButton("Grayscale", NULL);
    invert_button = IupButton("Invert", NULL);
    horizontal_button = IupButton("H-Flip", NULL);
    vertical_button = IupButton("V-Flip", NULL);
    blur_button = IupButton("Blur", NULL);
    rotate_button = IupButton("Rotate", NULL);
    undo_button = IupButton("Undo", NULL);

    /* ---------- Brightness controls ---------- */
    brightness_input = IupText(NULL);

    IupSetAttribute(brightness_input, "VALUE", "20");
    IupSetAttribute(brightness_input, "SIZE", "60");

    brightness_button = IupButton("Brightness", NULL);

    /* ---------- Crop controls ---------- */
    crop_x_input = IupText(NULL);
    crop_y_input = IupText(NULL);
    crop_w_input = IupText(NULL);
    crop_h_input = IupText(NULL);

    IupSetAttribute(crop_x_input, "VALUE", "0");
    IupSetAttribute(crop_y_input, "VALUE", "0");
    IupSetAttribute(crop_w_input, "VALUE", "100");
    IupSetAttribute(crop_h_input, "VALUE", "100");

    crop_button = IupButton("Crop", NULL);

    /* ---------- Wire up callbacks ----------
       Each button's "ACTION" event (triggered on click) is
       linked to the matching *_callback function above. */
    IupSetCallback(open_button, "ACTION", open_callback);
    IupSetCallback(save_button, "ACTION", save_callback);
    IupSetCallback(grayscale_button, "ACTION", grayscale_callback);
    IupSetCallback(invert_button, "ACTION", invert_callback);
    IupSetCallback(horizontal_button, "ACTION", horizontal_flip_callback);
    IupSetCallback(vertical_button, "ACTION", vertical_flip_callback);
    IupSetCallback(blur_button, "ACTION", blur_callback);
    IupSetCallback(rotate_button, "ACTION", rotate_callback);
    IupSetCallback(undo_button, "ACTION", undo_callback);
    IupSetCallback(brightness_button, "ACTION", brightness_callback);
    IupSetCallback(crop_button, "ACTION", crop_callback);

    /* ---------- Arrange buttons into rows ---------- */
    row1 = IupHbox(
        open_button,
        save_button,
        grayscale_button,
        invert_button,
        NULL   /* IUP box functions must end with NULL */
    );

    /* GAP adds pixel spacing BETWEEN the children of this box,
       so buttons aren't touching each other edge-to-edge. */
    IupSetAttribute(row1, "GAP", "8");

    row2 = IupHbox(
        horizontal_button,
        vertical_button,
        blur_button,
        rotate_button,
        undo_button,
        NULL
    );

    IupSetAttribute(row2, "GAP", "8");

    row3 = IupHbox(
        IupLabel("Brightness:"),
        brightness_input,
        brightness_button,
        NULL
    );

    IupSetAttribute(row3, "GAP", "8");

    crop_row = IupHbox(
        IupLabel("X:"), crop_x_input,
        IupLabel("Y:"), crop_y_input,
        IupLabel("W:"), crop_w_input,
        IupLabel("H:"), crop_h_input,
        crop_button,
        NULL
    );

    IupSetAttribute(crop_row, "GAP", "8");

    /* Wrap the crop controls in a titled frame/border, so they
       read as a clearly separate group in the window instead of
       just another row of buttons. Purely cosmetic -- doesn't
       change any functionality. */
    Ihandle *crop_frame = IupFrame(crop_row);
    IupSetAttribute(crop_frame, "TITLE", "Crop Settings");

    /* ---------- Image display area ----------

       KEY FIX: give the label a real (tiny) placeholder image
       *before* the window is shown. This puts image_label into
       "image mode" from the start. If we left it as IupLabel(NULL)
       (no image, no text) and only tried to set IMAGE later
       inside refresh_image(), IUP would silently refuse the
       change once the dialog is mapped -- because a label's
       behavior (text vs image vs separator) can only be chosen
       once, before mapping. */
    unsigned char blank_pixel[3] = {40, 40, 40}; /* dark grey, 1x1 */

    Ihandle *blank_image = IupImageRGB(1, 1, blank_pixel);

    IupSetHandle("BLANK_IMAGE", blank_image);

    image_label = IupLabel(NULL);

    IupSetAttribute(image_label, "IMAGE", "BLANK_IMAGE");
    IupSetAttribute(image_label, "ALIGNMENT", "ACENTER");
    IupSetAttribute(image_label, "RASTERSIZE", "500x400");

    /* Let the image area grow if the user resizes the window,
       instead of staying locked at exactly 500x400. */
    IupSetAttribute(image_label, "EXPAND", "YES");

    /* ---------- Main window ---------- */
    Ihandle *main_vbox = IupVbox(
        IupLabel("Image Manipulation Software"),

        row1,
        row2,
        row3,
        crop_frame,

        image_label,

        NULL
    );

    /* MARGIN adds empty space between the window's edges and its
       content. GAP adds empty space BETWEEN each row (row1, row2,
       row3, crop_frame, image_label) stacked in this Vbox. Together
       these turn a "cramped, edge-to-edge" layout into a properly
       spaced one. */
    IupSetAttribute(main_vbox, "MARGIN", "15x15");
    IupSetAttribute(main_vbox, "GAP", "10");

    dlg = IupDialog(main_vbox);

    IupSetAttribute(dlg, "TITLE", "C Image Editor");

    IupShowXY(dlg, IUP_CENTER, IUP_CENTER);

    IupMainLoop();   /* blocks here, handling clicks etc.,
                        until the window is closed */

    /* ---------- Cleanup on exit ---------- */
    if (display_image != NULL)
        IupDestroy(display_image);

    if (current_image != NULL)
        free_image(current_image);

    IupClose();

    return 0;
}