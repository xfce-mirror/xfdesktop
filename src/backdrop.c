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

#include <math.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libxfce4mcs/mcs-client.h>
#include <libxfce4util/debug.h>
#include <libxfce4util/i18n.h>
#include <libxfcegui4/dialogs.h>
#include <libxfcegui4/netk-screen.h>

#include "backdrop-common.h"

#include "main.h"
#include "settings.h"
#include "backdrop.h"

static gboolean init_settings = FALSE;

static void remove_old_pixmap (XfceBackdrop *backdrop);
static GdkFilterReturn monitor_backdrop(GdkXEvent *ev, GdkEvent *gev,
		XfceBackdrop *backdrop);

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
	DBG ("no listfile\n");
	g_free (prevfile);
	prevfile = NULL;
	return NULL;
    }

    if (stat (listfile, &st) < 0)
    {
	DBG ("stat listfile failed\n");
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
	    DBG ("no files in list\n");
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
create_image (XfceBackdrop *backdrop)
{
    GError *error = NULL;
    const char *path = NULL;
    GdkPixbuf *pixbuf;

    if (!backdrop->path || !strlen (backdrop->path))
	return NULL;

    if (is_backdrop_list (backdrop->path))
	path = get_path_from_listfile (backdrop->path);
    else
	path = (const char *) backdrop->path;

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
create_pixmap (XfceBackdrop *backdrop, GdkPixbuf * image)
{
    GdkPixmap *pixmap = NULL;
    GdkPixbuf *pixbuf = NULL;
	GdkColormap *cmap;
    int width, height;
    gboolean has_composite;
    XfceBackdropStyle style = backdrop->style;

    TRACE ("dummy");

    if (!image)
    {
	pixbuf = create_solid (&(backdrop->color1), 1, 1);
    }
    else
    {
	width = gdk_pixbuf_get_width (image);
	height = gdk_pixbuf_get_height (image);
	has_composite = gdk_pixbuf_get_has_alpha (image);

	if (style == AUTO)
	{
	    /* if height and width are both less than 1/2 the screen
	     * -> tiled, else -> scaled */

	    if (height <= gdk_screen_get_height(backdrop->gscreen) / 2 &&
		width <= gdk_screen_get_width(backdrop->gscreen) / 2)
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
		    pixbuf = create_solid (&(backdrop->color1),
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
		    int x = MAX ((gdk_screen_get_width(backdrop->gscreen) - width) / 2, 0);
		    int y = MAX ((gdk_screen_get_height(backdrop->gscreen) - height) / 2, 0);
		    int x_offset = MIN ((gdk_screen_get_width(backdrop->gscreen) - width) / 2, x);
		    int y_offset =
			MIN ((gdk_screen_get_height(backdrop->gscreen) - height) / 2, y);

		    pixbuf = create_solid (&(backdrop->color1),
					   gdk_screen_get_width(backdrop->gscreen),
					   gdk_screen_get_height(backdrop->gscreen));

		    if (has_composite)
		    {
			gdk_pixbuf_composite (image, pixbuf, x, y,
					      MIN (gdk_screen_get_width(backdrop->gscreen),
						   width),
					      MIN (gdk_screen_get_height(backdrop->gscreen),
						   height),
					      x_offset, y_offset, 1.0, 1.0,
					      GDK_INTERP_NEAREST, 255);
		    }
		    else
		    {
			gdk_pixbuf_copy_area (image,
					      MAX (-x_offset, 0),
					      MAX (-y_offset, 0),
					      MIN (gdk_screen_get_width(backdrop->gscreen),
						   width),
					      MIN (gdk_screen_get_height(backdrop->gscreen),
						   height), pixbuf, x, y);
		    }
		}

		break;

	    case SCALED:
		{
		    int x, y, w, h;
		    double wratio, hratio;
		    GdkPixbuf *scaled;

		    wratio = (double) width / (double) gdk_screen_get_width(backdrop->gscreen);
		    hratio = (double) height / (double) gdk_screen_get_height(backdrop->gscreen);

		    if (hratio > wratio)
		    {
			h = gdk_screen_get_height(backdrop->gscreen);
			w = rint (width / hratio);

			y = 0;
			x = (gdk_screen_get_width(backdrop->gscreen) - w) / 2;
		    }
		    else
		    {
			w = gdk_screen_get_width(backdrop->gscreen);
			h = rint (height / wratio);

			x = 0;
			y = (gdk_screen_get_height(backdrop->gscreen) - h) / 2;
		    }

		    DBG ("scaling: %d,%d+%dx%d\n", x, y, w, h);

		    scaled = gdk_pixbuf_scale_simple (image, w, h,
						      GDK_INTERP_BILINEAR);

		    pixbuf = create_solid (&(backdrop->color1),
					   gdk_screen_get_width(backdrop->gscreen),
					   gdk_screen_get_height(backdrop->gscreen));

		    if (has_composite)
		    {
			gdk_pixbuf_composite (scaled, pixbuf,
					      x, y, w, h,
					      x, y, 1.0, 1.0,
					      GDK_INTERP_NEAREST, 255);
		    }
		    else
		    {
			gdk_pixbuf_copy_area (scaled, 0, 0, w, h,
					      pixbuf, x, y);
		    }

		    g_object_unref (scaled);
		}
		break;

	    case STRETCHED:
		{
		    pixbuf =
			gdk_pixbuf_scale_simple (image, gdk_screen_get_width(backdrop->gscreen),
						 gdk_screen_get_height(backdrop->gscreen),
						 GDK_INTERP_BILINEAR);
		    if (has_composite)
		    {
			GdkPixbuf *scaled = pixbuf;

			pixbuf = create_solid (&(backdrop->color1),
					       gdk_screen_get_width(backdrop->gscreen),
					       gdk_screen_get_height(backdrop->gscreen));

			gdk_pixbuf_composite (scaled, pixbuf, 0, 0,
					      gdk_screen_get_width(backdrop->gscreen),
					      gdk_screen_get_height(backdrop->gscreen),
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

	cmap = gdk_screen_get_system_colormap(backdrop->gscreen);
    gdk_pixbuf_render_pixmap_and_mask_for_colormap (pixbuf, cmap, &pixmap, NULL, 0);
    g_object_unref (pixbuf);

    return pixmap;
}

static void
update_window_style (GtkWidget * win, GdkPixmap * pixmap)
{
    GtkStyle *style;

    /*  free any previously allocated pixmap  */
	gtk_widget_show(win);
	style = gtk_widget_get_style(win);
	if(style->bg_pixmap[GTK_STATE_NORMAL])
		g_object_unref(style->bg_pixmap[GTK_STATE_NORMAL]);
	
	/* set new pixmap */
    style->bg_pixmap[GTK_STATE_NORMAL] = pixmap;
    gtk_widget_set_style (win, style);
    gtk_widget_queue_draw (win);
}

static void
update_root_window (GdkWindow *root, GtkWidget * win, GdkPixmap * pixmap)
{
    gdk_error_trap_push ();
    if (pixmap)
    {
	XID id = GDK_DRAWABLE_XID (pixmap);
	XID *idptr = &id;

	gdk_property_change (root,
			     gdk_atom_intern ("_XROOTPMAP_ID", FALSE),
			     gdk_atom_intern ("PIXMAP", FALSE),
			     32, GDK_PROP_MODE_REPLACE, (guchar *) idptr, 1);
    }
    else
    {
	gdk_property_delete (root,
			     gdk_atom_intern ("_XROOTPMAP_ID", FALSE));
    }

    gdk_error_trap_pop ();
    gdk_flush ();
}

/* note: always return FALSE here, as this is also used as a GSourceFunc via
 * g_idle_add(), and shouldn't be run more than once. */
static gboolean
set_backdrop(XfceBackdrop *backdrop)
{
	GdkPixmap *pixmap;
	GdkPixbuf *pixbuf;
	GdkWindow *root;
	
	if(!backdrop->set_backdrop)
		return FALSE;
	
	if(!backdrop->color_only)
		pixbuf = create_image(backdrop);
	else
		pixbuf = NULL;
	
	pixmap = create_pixmap(backdrop, pixbuf);
	
	update_window_style(backdrop->win, pixmap);
	root = gdk_screen_get_root_window(backdrop->gscreen);
	update_root_window(root, backdrop->win, pixmap);
	
	if(pixbuf)
		g_object_unref(pixbuf);
	
	return FALSE;
}

/* monitor _XROOT_PMAP_ID property */
static void
set_backdrop_from_root_property(XfceBackdrop *backdrop)
{
    Window w = backdrop->root;
    Atom type;
    int format;
    unsigned long length, after;
    unsigned char *data;

    TRACE ("dummy");
    XGrabServer (GDK_DISPLAY());

#if 0
    /* first try _ESETROOT_PMAP_ID */
    XGetWindowProperty (GDK_DISPLAY(), w, backdrop->e_atom, 0L, 1L, False,
			AnyPropertyType, &type, &format, &length, &after,
			&data);

    if ((type == XA_PIXMAP) && (format == 32) && (length == 1))
    {
	GdkPixmap *pixmap = gdk_pixmap_foreign_new (*((Pixmap *) data));

	DBG ("Update background from _XROOTPMAP_ID property");

	if (pixmap)
	{
	    update_window_style (backdrop->win, pixmap);
	    goto UNGRAB;
	}
    }
#endif
    /* _XROOTPMAP_ID */
    DBG ("Update background from _XROOTPMAP_ID property");

    XGetWindowProperty (GDK_DISPLAY(), w, backdrop->atom, 0L, 1L, False,
			AnyPropertyType, &type, &format, &length, &after,
			&data);

	if((type == XA_PIXMAP) && (format == 32) && (length == 1) && data) {
		GdkPixmap *pixmap;
		if(!(pixmap=gdk_pixmap_lookup(*((Pixmap *)data))))
			pixmap = gdk_pixmap_foreign_new (*((Pixmap *) data));
		else
			g_object_ref(G_OBJECT(pixmap));

		if(pixmap)
			update_window_style (backdrop->win, pixmap);
		else {
			DBG ("Unable to obtain pixmap from _XROOTPMAP_ID property");
		}
	}

/*UNGRAB:*/
    XUngrabServer (GDK_DISPLAY());
}

static GdkFilterReturn
monitor_backdrop(GdkXEvent *ev, GdkEvent *gev, XfceBackdrop *backdrop)
{
	XEvent *xevent = (XEvent *) ev;

	if(backdrop->set_backdrop)
		return GDK_FILTER_CONTINUE;

	if(xevent->type == PropertyNotify
			&& xevent->xproperty.atom == backdrop->atom
			&& xevent->xproperty.window == backdrop->root)
	{
		set_backdrop_from_root_property(backdrop);
		return GDK_FILTER_REMOVE;
	}

	return GDK_FILTER_CONTINUE;
}

/* settings */
static void
update_backdrop_channel(const char *channel_name, McsClient *client,
		McsAction action, McsSetting *setting)
{
	gint screen_num = -1;
	gchar *p;
	XfceDesktop *xfdesktop = NULL;
	XfceBackdrop *backdrop = NULL;
	
	TRACE ("dummy");
	DBG ("processing change in \"%s\"", channel_name);
	
	if(strcmp(channel_name, BACKDROP_CHANNEL))
		return;
	
	switch (action) {
		case MCS_ACTION_NEW:
			if(init_settings)
				return;
			/* fall through if not in init state */
		case MCS_ACTION_CHANGED:
			p = g_strrstr(setting->name, "_");
			if(!p)
				return;
			screen_num = atoi(p+1);
			if(screen_num < 0)
				return;
			
			xfdesktop = g_list_nth_data(desktops, screen_num);
			if(!xfdesktop)
				return;
			backdrop = xfdesktop->backdrop;
			
			if(strstr(setting->name, "setbackdrop_") == setting->name) {
				backdrop->set_backdrop = setting->data.v_int;
				
				if(backdrop->set_backdrop)
					remove_old_pixmap(backdrop);  /* we don't use _ESETROOT_PMAP_ID */
				else
					set_backdrop_from_root_property(backdrop);
			} else if(strstr(setting->name, "style_") == setting->name)
				backdrop->style = setting->data.v_int;
			else if(strstr(setting->name, "path_") == setting->name) {
				if(backdrop->path)
					g_free(backdrop->path);
				backdrop->path = g_strdup(setting->data.v_string);
			} else if(strstr(setting->name, "color1_") == setting->name) {
				backdrop->color1.red = setting->data.v_color.red;
				backdrop->color1.green = setting->data.v_color.green;
				backdrop->color1.blue = setting->data.v_color.blue;
			} else if(strstr(setting->name, "color2_") == setting->name) {
				backdrop->color2.red = setting->data.v_color.red;
				backdrop->color2.green = setting->data.v_color.green;
				backdrop->color2.blue = setting->data.v_color.blue;
			} else if(strstr(setting->name, "coloronly_") == setting->name)
				backdrop->color_only = setting->data.v_int;

			if(backdrop->set_backdrop)
				g_idle_add((GSourceFunc)set_backdrop, backdrop);

			break;

		case MCS_ACTION_DELETED:
			/* We don't use this now. Perhaps revert to default? */
			break;
	}
}

/* removes _ESETROOT_PMAP_ID */
static void
remove_old_pixmap(XfceBackdrop *backdrop)
{
    Window w = backdrop->root;
    Atom e_prop = backdrop->e_atom;
    Atom type;
    int format;
    unsigned long length, after;
    unsigned char *data;

    TRACE ("dummy");
    XGrabServer (GDK_DISPLAY());

    XGetWindowProperty (GDK_DISPLAY(), w, e_prop, 0L, 1L, False, AnyPropertyType,
			&type, &format, &length, &after, &data);

    if ((type == XA_PIXMAP) && (format == 32) && (length == 1))
    {
	gdk_error_trap_push ();
	XKillClient (GDK_DISPLAY(), *((Pixmap *) data));
	gdk_flush ();
	gdk_error_trap_pop ();
	XDeleteProperty (GDK_DISPLAY(), w, e_prop);
    }

    XUngrabServer (GDK_DISPLAY());
}

XfceBackdrop *
backdrop_new(gint screen, GtkWidget *fullscreen, McsClient *client)
{
    GdkWindow *root;
	XfceBackdrop *backdrop;
	GdkRectangle rect;
	GdkGC *root_gc;
	GdkColor black;
	GdkColormap *cmap;

    TRACE ("dummy");

    backdrop = g_new0(XfceBackdrop, 1);

	backdrop->client = client;
	
	backdrop->xscreen = screen;
	backdrop->gscreen = gdk_display_get_screen(gdk_display_get_default(), screen);
	
	root = gdk_screen_get_root_window(backdrop->gscreen);
    backdrop->root = GDK_WINDOW_XID(root);

    backdrop->atom = XInternAtom(GDK_DISPLAY(), "_XROOTPMAP_ID", False);
    backdrop->e_atom = XInternAtom(GDK_DISPLAY(), "ESETROOT_PMAP_ID", False);
	
    backdrop->win = fullscreen;

    backdrop->set_backdrop = TRUE;
    backdrop->color_only = FALSE;
    backdrop->style = TILED;

    set_backdrop(backdrop);
	
    /* color the root window black */
	cmap = gdk_screen_get_system_colormap(backdrop->gscreen);
	black.red = black.green = black.blue = 0;
	if(gdk_colormap_alloc_color(cmap, &black, FALSE, TRUE)) {
		root_gc = gdk_gc_new(GDK_DRAWABLE(root));
		gdk_gc_set_background(root_gc, &black);
		gdk_colormap_free_colors(cmap, &black, 1);
		
		rect.x = rect.y = 0;
		rect.width = gdk_screen_get_width(backdrop->gscreen);
		rect.height = gdk_screen_get_height(backdrop->gscreen);
		gdk_window_begin_paint_rect(root, &rect);
		gdk_draw_rectangle(GDK_DRAWABLE(root), root_gc, TRUE, 0, 0,
				gdk_screen_get_width(backdrop->gscreen),
				gdk_screen_get_height(backdrop->gscreen));
		gdk_window_end_paint(root);
		g_object_unref(G_OBJECT(root_gc));
	}

    /* watch _XROOTPMAP_ID root property */
    gdk_window_add_filter(root, (GdkFilterFunc)monitor_backdrop, backdrop);

    gdk_window_set_events (root,
			   gdk_window_get_events (root) |
			   GDK_PROPERTY_CHANGE_MASK);

	return backdrop;
}

void
backdrop_settings_init()
{
	/* connect callback for settings changes */
	init_settings = TRUE;
    register_channel(BACKDROP_CHANNEL, (ChannelCallback)update_backdrop_channel);
	init_settings = FALSE;
}

void
backdrop_load_settings(XfceBackdrop *backdrop)
{
    McsClient *client = backdrop->client;
    McsSetting *setting;
	gchar setting_name[128];

    TRACE ("dummy");

	g_snprintf(setting_name, 128, "setbackdrop_%d", backdrop->xscreen);
	if(MCS_SUCCESS == mcs_client_get_setting(client, setting_name,
			BACKDROP_CHANNEL, &setting))
	{
		backdrop->set_backdrop = setting->data.v_int == 0 ? FALSE : TRUE;
		mcs_setting_free(setting);
	} else
		backdrop->set_backdrop = TRUE;

	g_snprintf(setting_name, 128, "style_%d", backdrop->xscreen);
	if(MCS_SUCCESS == mcs_client_get_setting(client, setting_name,
			BACKDROP_CHANNEL, &setting))
	{
		backdrop->style = setting->data.v_int;
		mcs_setting_free(setting);
	} else
		backdrop->style = STRETCHED;

	g_snprintf(setting_name, 128, "path_%d", backdrop->xscreen);
	if(MCS_SUCCESS == mcs_client_get_setting (client, setting_name,
			BACKDROP_CHANNEL, &setting))
	{
		if(setting->data.v_string) {
			g_free(backdrop->path);
			backdrop->path = g_strdup(setting->data.v_string);
		}
		mcs_setting_free(setting);
	} else
		;//backdrop->path = g_strdup(DEFAULT_BACKDROP);

	g_snprintf(setting_name, 128, "color1_%d", backdrop->xscreen);
	if(MCS_SUCCESS == mcs_client_get_setting(client, setting_name,
			BACKDROP_CHANNEL, &setting))
	{
		backdrop->color1.red = setting->data.v_color.red;
		backdrop->color1.green = setting->data.v_color.green;
		backdrop->color1.blue = setting->data.v_color.blue;
		mcs_setting_free (setting);
	}
	
	g_snprintf(setting_name, 128, "color2_%d", backdrop->xscreen);
	if(MCS_SUCCESS == mcs_client_get_setting(client, setting_name,
			BACKDROP_CHANNEL, &setting))
	{
		backdrop->color2.red = setting->data.v_color.red;
		backdrop->color2.green = setting->data.v_color.green;
		backdrop->color2.blue = setting->data.v_color.blue;

		mcs_setting_free (setting);
	}

	g_snprintf(setting_name, 128, "coloronly_%d", backdrop->xscreen);
	if(MCS_SUCCESS == mcs_client_get_setting(client, setting_name,
			BACKDROP_CHANNEL, &setting))
	{
		backdrop->color_only = setting->data.v_int == 0 ? FALSE : TRUE;
		mcs_setting_free(setting);
	} else
		backdrop->color_only = FALSE;

	DBG("set backdrop %s", backdrop->set_backdrop ? "from settings" : "from root property");
	if(backdrop->set_backdrop) {
		/* we don't use _ESETROOT_PMAP_ID */
		remove_old_pixmap(backdrop);
		set_backdrop(backdrop);
	} else
		set_backdrop_from_root_property(backdrop);
}

void
backdrop_cleanup(XfceBackdrop *backdrop)
{
	gdk_window_remove_filter(gdk_screen_get_root_window(backdrop->gscreen),
			(GdkFilterFunc)monitor_backdrop, backdrop);
	g_free(backdrop);
}
