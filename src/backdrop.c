/*  xfce4
 *  
 *  Copyright (C) 2002 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *  Copyright (c) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
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

#include <common/background-common.h>

#include "main.h"
#include "settings.h"
#include "backdrop.h"

typedef struct
{
    Display *dpy;
    Window root;
    Atom atom;

    GtkWidget *win;
    
    guint set_background:1;
    guint show_image:1;

    GdkColor color;
    char *path;
    XfceBackgroundStyle style;
}
XfceBackground;

static gboolean init_settings = TRUE;

static XfceBackground *background = NULL;

static void set_background (XfceBackground * background);

static GdkFilterReturn monitor_background (GdkXEvent *ev, GdkEvent *gev, 
					   XfceBackground *background);

/* list files */
static int
count_elements (char **list)
{
    char **c;

    TRACE ("dummy");
    for (c = list; *c; c++)
	;

    return (c - list);
}

static const char *
get_path_from_listfile (const gchar * listfile)
{
    static gboolean __initialized = FALSE;
    static gchar *prevfile = NULL;
    static gchar **files = NULL;
    static gint previndex = -1;
    static time_t mtime = 0;
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
	mtime = 0;
	return NULL;
    }

    if (!prevfile || strcmp (listfile, prevfile) != 0 || mtime < st.st_mtime)
    {
	g_strfreev (files);
	g_free (prevfile);

	files = get_list_from_file (listfile);
	prevfile = g_strdup (listfile);
	previndex = -1;
	mtime = st.st_mtime;
    }

    n = count_elements (files);

    switch (n)
    {
	case 0:
	    return NULL;

	case 1:
	    return (const char *) files[0];
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

    return (const char *) files[(previndex = i)];
}

/* load image from file */
static GdkPixbuf *
create_image (XfceBackground * background)
{
    GError *error = NULL;
    const char *path = NULL;
    GdkPixbuf *pixbuf;

    if (!background->path || !strlen (background->path))
	return NULL;

    if (is_backdrop_list (background->path))
	path = get_path_from_listfile (path);
    else
	path = (const char *) background->path;

    pixbuf = gdk_pixbuf_new_from_file (path, &error);

    if (error)
    {
	xfce_err (_("%s: There was an error loading image:\n%s\n"),
		  PACKAGE, error->message);

	g_error_free (error);

	return NULL;
    }

    return pixbuf;
}

/* pixbuf for background color */
static GdkPixbuf *
create_solid (GdkColor * color, int width, int height)
{
    GdkPixbuf *solid;
    guint32 rgba;

    TRACE ("dummy");

    solid = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);

    rgba = (((color->red & 0xff00) << 8) |
	    ((color->green & 0xff00)) | ((color->blue & 0xff00) >> 8)) << 8;

    gdk_pixbuf_fill (solid, rgba);

    return solid;
}

/* This function does all of the work for compositing and scaling the image
 * onto a solid color background
 */
static GdkPixmap *
create_pixmap (XfceBackground * background, GdkPixbuf * image)
{
    GdkPixmap *pixmap = NULL;
    GdkPixbuf *pixbuf = NULL;
    int width, height;
    gboolean has_composite;
    XfceBackgroundStyle style = background->style;

    TRACE ("dummy");

    if (!image)
    {
	pixbuf = create_solid (&(background->color), 1, 1);
    }
    else
    {
	width = gdk_pixbuf_get_width (image);
	height = gdk_pixbuf_get_height (image);
	has_composite = gdk_pixbuf_get_has_alpha (image);

	if (style == AUTO)
	{
	    /* if height and width are both less than 1/3 the screen
	     * -> tiled, else -> scaled */

	    if (height <= gdk_screen_height () / 3 &&
		width <= gdk_screen_width () / 3)
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
	    case TILED:
		if (has_composite)
		{
		    pixbuf = create_solid (&(background->color),
					   width, height);
		    gdk_pixbuf_composite (image, pixbuf,
					  0, 0, width, height,
					  0, 0, 1.0, 1.0,
					  GDK_INTERP_NEAREST, 255);
		}
		else
		{
		    pixbuf = image;
		    g_object_ref (pixbuf);
		}
		
		break;

	    case CENTERED:
		{
		    gint x = MAX ((gdk_screen_width () - width) / 2, 0);
		    gint y = MAX ((gdk_screen_height () - height) / 2, 0);

		    pixbuf = create_solid (&(background->color),
					   gdk_screen_width (),
					   gdk_screen_height ());

		    if (has_composite)
		    {
			gdk_pixbuf_composite (image, pixbuf, x, y,
					      MIN (gdk_screen_width (),
						   width),
					      MIN (gdk_screen_height (),
						   height), x, y, 1.0, 1.0,
					      GDK_INTERP_NEAREST, 255);
		    }
		    else
		    {
			gdk_pixbuf_copy_area (image, 0, 0,
					      MIN (gdk_screen_width (),
						   width),
					      MIN (gdk_screen_height (),
						   height), pixbuf, x, y);
		    }
		}
		
		break;

	    case SCALED:
		/* FIXME: do scaling with aspect ratio here */

	    case STRETCHED:
		{
		    pixbuf =
			gdk_pixbuf_scale_simple (image, gdk_screen_width (),
						 gdk_screen_height (),
						 GDK_INTERP_BILINEAR);
		    if (has_composite)
		    {
			GdkPixbuf *scaled = pixbuf;

			pixbuf = create_solid (&(background->color),
					       gdk_screen_width (),
					       gdk_screen_height ());

			gdk_pixbuf_composite (scaled, pixbuf, 0, 0,
					      gdk_screen_width (),
					      gdk_screen_height (),
					      0, 0, 1.0, 1.0,
					      GDK_INTERP_NEAREST, 255);
			g_object_unref (scaled);
		    }
		}
		
		break;
		
	    default:
		g_assert_not_reached ();
	}
    }

    gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, NULL, 0);
    g_object_unref (pixbuf);

    return pixmap;
}

static void
update_window_style (GtkWidget *win, GdkPixmap * pixmap)
{
    GtkStyle *style;

    /* This call is used to free any previous allocated pixmap  */
    gtk_widget_set_style (win, NULL);
    
    style = gtk_style_new ();
    style->bg_pixmap[GTK_STATE_NORMAL] = pixmap;
    gtk_widget_set_style (win, style);
    g_object_unref (style);
    gtk_widget_queue_draw (win);
}

static void
update_root_window (GtkWidget *win, GdkPixmap * pixmap)
{
    if (pixmap)
    {
	XID id = GDK_DRAWABLE_XID (pixmap);
	XID *idptr = &id;
	
	gdk_property_change (gdk_get_default_root_window (),
			     gdk_atom_intern ("_XROOTPMAP_ID", FALSE),
			     gdk_atom_intern ("PIXMAP", FALSE),
			     32, GDK_PROP_MODE_REPLACE, (guchar *) idptr, 1);
    }
    else
    {
	gdk_property_delete (gdk_get_default_root_window (),
			     gdk_atom_intern ("_XROOTPMAP_ID", FALSE));
    }

    gdk_flush ();
}

static void
set_background (XfceBackground * background)
{
    GdkPixmap *pixmap;
    GdkPixbuf *pixbuf;

    if (!background->set_background)
	return;

    if (background->show_image)
	pixbuf = create_image (background);
    else
	pixbuf = NULL;

    pixmap = create_pixmap (background, pixbuf);

    update_window_style (background->win, pixmap);
    update_root_window (background->win, pixmap);

    if (pixbuf)
	g_object_unref (pixbuf);
}

/* monitor _XROOT_PMAP_ID property */
static void
set_background_from_root_property(XfceBackground *background)
{
    Display *dpy = background->dpy;
    Window w = background->root;
    Atom type;
    int format;
    unsigned long length, after;
    unsigned char *data;

    TRACE ("dummy");
    XGrabServer (dpy);

    XGetWindowProperty (dpy, w, background->atom, 0L, 1L, False, 
	    		AnyPropertyType, &type, &format, &length, &after, 
			&data);

    if ((type == XA_PIXMAP) && (format == 32) && (length == 1))
    {
	GdkPixmap *pixmap = gdk_pixmap_foreign_new (*((Pixmap *) data));
	update_window_style (background->win, pixmap);
    }

    XUngrabServer (dpy);
}

static GdkFilterReturn 
monitor_background (GdkXEvent *ev, GdkEvent *gev, XfceBackground *background)
{
    XEvent *xevent = (XEvent *)ev;

    if (background->set_background)
	return GDK_FILTER_CONTINUE;
    
    if (xevent->type == PropertyNotify &&
	xevent->xproperty.atom == background->atom &&
	xevent->xproperty.window == background->root)
    {
	set_background_from_root_property(background);
	return GDK_FILTER_REMOVE;
    }
    
    return GDK_FILTER_CONTINUE;
}

/* settings */
static void
update_backdrop_channel (const char *name, McsAction action,
			 McsSetting * setting, XfceDesktop *xfdesktop)
{
    TRACE ("dummy");
    DBG ("processing change in \"%s\"", name);

    switch (action)
    {
	case MCS_ACTION_NEW:
	    /* fall through unless we are in init state */
	    if (init_settings)
	    {
		return;
	    }
	case MCS_ACTION_CHANGED:
	    if (strcmp (name, "setbackground") == 0)
	    {
		background->set_background = setting->data.v_int;
		DBG ("set_background = %d", background->set_background);
	    }
	    else if (strcmp (name, "style") == 0)
	    {
		background->style = setting->data.v_int;
	    }
	    else if (strcmp (name, "path") == 0)
	    {
		g_free (background->path);
		background->path = (setting->data.v_string) ?
		    g_strdup (setting->data.v_string) : NULL;
	    }
	    else if (strcmp (name, "color") == 0)
	    {
		background->color.red = setting->data.v_color.red;
		background->color.green = setting->data.v_color.green;
		background->color.blue = setting->data.v_color.blue;
	    }
	    else if (strcmp (name, "showimage") == 0)
	    {
		background->show_image = setting->data.v_int;
	    }

	    if (background->set_background)
		set_background (background);

	    break;
	case MCS_ACTION_DELETED:
	    /* We don't use this now. Perhaps revert to default? */
	    break;
    }
}

/* removes _ESETROOT_PMAP_ID */
static void
remove_old_pixmap (Display *dpy, Window w)
{
    Atom e_prop = XInternAtom (dpy, "ESETROOT_PMAP_ID", False);
    Atom type;
    int format;
    unsigned long length, after;
    unsigned char *data;

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
background_init (XfceDesktop *xfdesktop)
{
    GdkWindow *root;
    
    TRACE ("dummy");

    background = g_new0 (XfceBackground, 1);

    background->dpy = xfdesktop->dpy;
    background->root = xfdesktop->root;
    background->atom = XInternAtom (xfdesktop->dpy, "_XROOTPMAP_ID", False);
    background->win = xfdesktop->fullscreen;
    background->set_background = TRUE;
    background->show_image = TRUE;
    background->style = TILED;

    /* we don't use _ESETROOT_PMAP_ID */
    remove_old_pixmap (xfdesktop->dpy, xfdesktop->root);

    /* connect callback for settings changes */
    register_channel_callback (BACKDROP_CHANNEL, 
	    		       (ChannelCallback) update_backdrop_channel);

    init_settings = TRUE;
    mcs_client_add_channel (xfdesktop->client, BACKDROP_CHANNEL);
    init_settings = FALSE;
    
    /* watch _XROOTPMAP_ID root property */
    root = gdk_get_default_root_window ();
    
    gdk_window_add_filter (root, (GdkFilterFunc)monitor_background, 
	    		   background);
    
    gdk_window_set_events (root,
	    		   gdk_window_get_events (root) | 
			   GDK_PROPERTY_CHANGE_MASK);
}

void
background_load_settings (XfceDesktop *xfdesktop)
{
    McsClient *client = xfdesktop->client;
    McsSetting *setting;

    TRACE ("dummy");
    if (MCS_SUCCESS == mcs_client_get_setting (client, "setbackground",
					       BACKDROP_CHANNEL, &setting))
    {
	background->set_background = setting->data.v_int;
	mcs_setting_free (setting);
    }

    if (MCS_SUCCESS == mcs_client_get_setting (client, "style",
					       BACKDROP_CHANNEL, &setting))
    {
	background->style = setting->data.v_int;
	mcs_setting_free (setting);
    }

    if (MCS_SUCCESS == mcs_client_get_setting (client, "path",
					       BACKDROP_CHANNEL, &setting))
    {
	if (setting->data.v_string)
	{
	    g_free (background->path);
	    background->path = g_strdup (setting->data.v_string);
	}

	mcs_setting_free (setting);
    }

    if (MCS_SUCCESS == mcs_client_get_setting (client, "color",
					       BACKDROP_CHANNEL, &setting))
    {
	background->color.red = setting->data.v_color.red;
	background->color.green = setting->data.v_color.green;
	background->color.blue = setting->data.v_color.blue;

	mcs_setting_free (setting);
    }

    if (MCS_SUCCESS ==
	mcs_client_get_setting (client, "showimage", BACKDROP_CHANNEL,
				&setting))
    {
	background->show_image = setting->data.v_int;
	mcs_setting_free (setting);
    }

    if (background->set_background)
	set_background (background);
    else
	set_background_from_root_property(background);

}

void
background_cleanup (XfceDesktop *xfdesktop)
{
    gdk_window_remove_filter (gdk_get_default_root_window (), 
	    		      (GdkFilterFunc)monitor_background, background);
    g_free (background);
}

