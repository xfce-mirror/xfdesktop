/*  xfce4
 *  
 *  Copyright (C) 2002 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef GDK_MULTIHEAD_SAFE
#undef GDK_MULTIHEAD_SAFE
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <errno.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libxfce4mcs/mcs-client.h>
#include <libxfce4util/debug.h>
#include <libxfce4util/i18n.h>
#include <libxfcegui4/dialogs.h>
#include <libxfcegui4/netk-screen.h>

#include <backdrop.h>

#include "main.h"
#include "settings.h"

static GtkWidget *fullscreen_window = NULL;
static int screen_width = 0;
static int screen_height = 0;

static char *backdrop_path = NULL;
static int backdrop_style = TILED;
static int showimage = 1;
static GdkColor backdrop_color = { 0, 0, 0, 0 };

static void set_backdrop (const char *path, int style, int show,
			  GdkColor * color);

/* this is a quick hack to fix a crashing issue when setting an invalid
 * backdrop from the X event handler.  the function needs to be run in a g_idle
 * context otherwise Bad Things happen.  this is fixed properly post-4.0.x */
static gboolean
set_backdrop_idled(gpointer data)
{
	set_backdrop(backdrop_path, backdrop_style, showimage, &backdrop_color);
	
	return FALSE;
}

static gboolean
is_backdrop_list (const char *path)
{
    FILE *fp;
    char buf[512];
    int size;
    gboolean is_list = FALSE;

    TRACE ("dummy");
    size = sizeof (LIST_TEXT);

    if ((fp = fopen (path, "r")) == NULL)
	return FALSE;

    if (fgets (buf, size, fp) && strncmp (LIST_TEXT, buf, size - 1) == 0)
	is_list = TRUE;

    fclose (fp);
    return is_list;
}

static void
remove_old_pixmap (void)
{
    Display *dpy = GDK_DISPLAY ();
    Window w = GDK_ROOT_WINDOW ();
    Atom type;
    int format;
    unsigned long length, after;
    unsigned char *data;
    Atom e_prop = XInternAtom (dpy, "ESETROOT_PMAP_ID", False);

    TRACE ("dummy");
    XGrabServer (dpy);

    XGetWindowProperty (dpy, w, e_prop, 0L, 1L, False, AnyPropertyType,
			&type, &format, &length, &after, &data);

    if ((type == XA_PIXMAP) && (format == 32) && (length == 1))
    {
	gdk_error_trap_push ();
	XKillClient (dpy, *((Pixmap *) data));
	gdk_flush ();
	gdk_error_trap_pop ();
	XDeleteProperty (dpy, w, e_prop);
    }

    XUngrabServer (dpy);
}

void
backdrop_init (GtkWidget * window)
{
    TRACE ("dummy");
    fullscreen_window = window;

    screen_width = gdk_screen_width ();
    screen_height = gdk_screen_height ();

    remove_old_pixmap ();
}

/* settings client */
static void
update_backdrop_channel (const char *name, McsAction action,
			 McsSetting * setting)
{
    TRACE ("dummy");
    switch (action)
    {
	case MCS_ACTION_NEW:
	    /* fall through unless we are in init state */
	    if (init_settings)
	    {
		return;
	    }
	case MCS_ACTION_CHANGED:
	    if (strcmp (name, "style") == 0)
	    {
		backdrop_style = setting->data.v_int;
	    }
	    else if (strcmp (name, "path") == 0)
	    {
		g_free (backdrop_path);
		backdrop_path = (setting->data.v_string) ?
		    g_strdup (setting->data.v_string) : NULL;
	    }
	    else if (strcmp (name, "color") == 0)
	    {
		backdrop_color.red = setting->data.v_color.red;
		backdrop_color.green = setting->data.v_color.green;
		backdrop_color.blue = setting->data.v_color.blue;
	    }
	    else if (strcmp (name, "showimage") == 0)
	    {
		showimage = setting->data.v_int;
	    }

	    g_idle_add(set_backdrop_idled, NULL);
	    break;
	case MCS_ACTION_DELETED:
	    /* We don't use this now. Perhaps revert to default? */
	    break;
    }
}

void
add_backdrop_callback (GHashTable * ht)
{
    TRACE ("dummy");
    g_hash_table_insert (ht, BACKDROP_CHANNEL, update_backdrop_channel);
}

void
backdrop_load_settings (McsClient * client)
{
    McsSetting *setting;

    TRACE ("dummy");
    if (MCS_SUCCESS == mcs_client_get_setting (client, "style",
					       BACKDROP_CHANNEL, &setting))
    {
	backdrop_style = setting->data.v_int;
	mcs_setting_free (setting);
    }

    if (MCS_SUCCESS == mcs_client_get_setting (client, "path",
					       BACKDROP_CHANNEL, &setting))
    {
	if (setting->data.v_string)
	{
	    g_free (backdrop_path);
	    backdrop_path = g_strdup (setting->data.v_string);
	}

	mcs_setting_free (setting);
    }

    if (MCS_SUCCESS == mcs_client_get_setting (client, "color",
					       BACKDROP_CHANNEL, &setting))
    {
	backdrop_color.red = setting->data.v_color.red;
	backdrop_color.green = setting->data.v_color.green;
	backdrop_color.blue = setting->data.v_color.blue;

	mcs_setting_free (setting);
    }

    if (MCS_SUCCESS ==
	mcs_client_get_setting (client, "showimage", BACKDROP_CHANNEL,
				&setting))
    {
	showimage = setting->data.v_int;
	mcs_setting_free (setting);
    }

    set_backdrop (backdrop_path, backdrop_style, showimage, &backdrop_color);
}

static int
count_elements (char **list)
{
    char **c;

    TRACE ("dummy");
    for (c = list; *c; c++)
	;

    return (c - list);
}

static char *
get_path_from_listfile (const gchar * listfile)
{
    static gboolean __initialized = FALSE;
    static gchar *prevfile = NULL;
    static gchar **files = NULL;
    static gint previndex = -1;
    static time_t ctime = 0;
    struct stat st;
    int i, n;

    TRACE ("dummy");

    if (!listfile)
    {
	g_free (prevfile);
	prevfile = NULL;
	return NULL;
    }

    if (stat (listfile, &st) < 0)
    {
	g_free (prevfile);
	prevfile = NULL;
	ctime = 0;
	return NULL;
    }

    if (!prevfile || strcmp (listfile, prevfile) != 0 || ctime < st.st_ctime)
    {
	g_strfreev (files);
	g_free (prevfile);

	files = get_list_from_file (listfile);
	prevfile = g_strdup (listfile);
	previndex = -1;
	ctime = st.st_ctime;
    }

    n = count_elements (files);

    switch (n)
    {
	case 0:
	    return NULL;

	case 1:
	    return *files;
    }

    /* NOTE: 4.3BSD random()/srandom() are a) stronger and b) faster than
     * ANSI-C rand()/srand(). So we use random() if available
     */
    if (!__initialized)
    {
#ifdef HAVE_SRANDOM
	srandom (time (NULL));
#else
	srand (time (NULL));
#endif
    }

    do
    {
#ifdef HAVE_SRANDOM
	i = random () % n;
#else
	i = rand () % n;
#endif
    }
    while (i == previndex);

    return files[(previndex = i)];
}

static GdkPixmap *
create_background_pixmap (GdkPixbuf * pixbuf, int style, GdkColor * color)
{
    GdkPixmap *pixmap = NULL;
    GdkPixbuf *solid;
    int width, height;
    guint32 rgba;
    gboolean has_composite;

    TRACE ("dummy");
    /* First, fill the pixbuf with solid color */
    solid =
	gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, screen_width,
			screen_height);
    rgba =
	(((color->red & 0xff00) << 8) | ((color->green & 0xff00)) | ((color->
								      blue &
								      0xff00)
								     >> 8)) <<
	8;
    gdk_pixbuf_fill (solid, rgba);

    if (pixbuf)
    {
	width = gdk_pixbuf_get_width (pixbuf);

	height = gdk_pixbuf_get_height (pixbuf);
	has_composite = gdk_pixbuf_get_has_alpha (pixbuf);

	if (style == AUTO)
	{
	    /* if height and width are both less than half the screen
	     * -> tiled, else -> scaled */

	    if (height <= screen_height / 2 && width <= screen_width)
	    {
		style = TILED;
	    }
	    else
	    {
		style = SCALED;
	    }
	}

	switch (style)
	{
	    case CENTERED:
		{
		    gint x = MAX ((screen_width - width) / 2, 0);
		    gint y = MAX ((screen_height - height) / 2, 0);
		    if (has_composite)
		    {
			gdk_pixbuf_composite (pixbuf, solid, x, y,
					      MIN (screen_width, width),
					      MIN (screen_height, height), x,
					      y, 1.0, 1.0, GDK_INTERP_NEAREST,
					      255);
		    }
		    else
		    {
			gdk_pixbuf_copy_area (pixbuf, 0, 0,
					      MIN (screen_width, width),
					      MIN (screen_height, height),
					      solid, x, y);
		    }
		}
		break;
	    case SCALED:
		{
		    GdkPixbuf *new = pixbuf;
		    new =
			gdk_pixbuf_scale_simple (pixbuf, screen_width,
						 screen_height,
						 GDK_INTERP_BILINEAR);
		    if (has_composite)
		    {
			gdk_pixbuf_composite (new, solid, 0, 0, screen_width,
					      screen_height, 0, 0, 1.0, 1.0,
					      GDK_INTERP_NEAREST, 255);
		    }
		    else
		    {
			gdk_pixbuf_copy_area (new, 0, 0, screen_width,
					      screen_height, solid, 0, 0);
		    }
		    g_object_unref (new);
		}
		break;
	    default:
	    case TILED:
		{
		    gdouble cx, cy;

		    for (cy = 0; cy < screen_height; cy += height)
		    {
			for (cx = 0; cx < screen_width; cx += width)
			{
			    if (has_composite)
			    {
				gdk_pixbuf_composite (pixbuf, solid, cx, cy,
						      MIN (width,
							   screen_width - cx),
						      MIN (height,
							   screen_height -
							   cy), cx, cy, 1.0,
						      1.0, GDK_INTERP_TILES,
						      255);
			    }
			    else
			    {
				gdk_pixbuf_copy_area (pixbuf, 0, 0,
						      MIN (width,
							   screen_width - cx),
						      MIN (height,
							   screen_height -
							   cy), solid, cx,
						      cy);
			    }
			}
		    }
		}
		break;
	}
    }
    gdk_pixbuf_render_pixmap_and_mask (solid, &pixmap, NULL, 0);
    g_object_unref (solid);

    return pixmap;
}

/* The code below is initially based on ROX Filer
 * (c) by Thomas Leonard. See http://rox.sf.net.
 */
static void
set_backdrop (const char *path, int style, int show, GdkColor * color)
{
    GdkPixmap *pixmap;
    GdkPixbuf *pixbuf;
    GtkStyle *gstyle;

    /* This call is used to free any previous allocated pixmap */
    gtk_widget_set_style (fullscreen_window, NULL);
    gstyle = gtk_style_new ();

    if (show && path && *path)
    {
	GError *error = NULL;

	if (is_backdrop_list (path))
	{
	    path = get_path_from_listfile (path);
	    /*style = AUTO; */
	}

	pixbuf = gdk_pixbuf_new_from_file (path, &error);

	if (error)
	{
	    xfce_err (_("xfdesktop error: loading backdrop image: %s\n"),
		      error->message);

	    g_error_free (error);
	    pixbuf = NULL;
	}
    }
    else
	pixbuf = NULL;

    pixmap = create_background_pixmap (pixbuf, style, color);

    /* Now we can safely free the pixbuf */
    if (pixbuf)
	g_object_unref (pixbuf);

    if (pixmap)
    {
	XID id = GDK_DRAWABLE_XID (pixmap);

	gstyle->bg_pixmap[GTK_STATE_NORMAL] = pixmap;
	gdk_window_set_back_pixmap (gdk_get_default_root_window (), pixmap,
				    0);
	gdk_window_clear (gdk_get_default_root_window ());

	gdk_property_change (gdk_get_default_root_window (),
			     gdk_atom_intern ("_XROOTPMAP_ID", FALSE),
			     gdk_atom_intern ("PIXMAP", FALSE),
			     32, GDK_PROP_MODE_REPLACE, (guchar *) & id, 1);
    }
    else
    {
	gstyle->bg_pixmap[GTK_STATE_NORMAL] = NULL;
	gdk_property_delete (gdk_get_default_root_window (),
			     gdk_atom_intern ("_XROOTPMAP_ID", FALSE));
	gdk_window_clear (gdk_get_default_root_window ());
    }

    gdk_flush ();
    /* (un)set background */
    gtk_widget_set_style (fullscreen_window, gstyle);
    g_object_unref (gstyle);

    gtk_widget_queue_draw (fullscreen_window);
}
