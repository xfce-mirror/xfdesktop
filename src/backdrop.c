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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "main.h"
#include "backdrop.h"

/* common stuff is defined here */
#include "settings/backdrop_settings.h"

static GtkWidget *fullscreen_window = NULL;
static int screen_width = 0;
static int screen_height = 0;

static char *backdrop_path = NULL;
static int backdrop_style = TILED;

static void set_backdrop(const char *path, int style);

static gboolean is_backdrop_list(const char *path)
{
    FILE *fp;
    char buf[512];
    static int size = strlen (LIST_TEXT);
    gboolean is_list = FALSE;

    fp = fopen (path, "r");

    if (!fp)
	return FALSE;

    if (fgets (buf, size + 1, fp) && strncmp(LIST_TEXT, buf, size) == 0)
      is_list = TRUE;
    
    fclose (fp);
    return is_list;
}

void backdrop_init(GtkWidget * window)
{
    fullscreen_window = window;

    screen_width = gdk_screen_width();
    screen_height = gdk_screen_height();
}

/* settings client */
static void backdrop_set_path(const char *path)
{
    g_free(backdrop_path);
    backdrop_path = g_strdup(path);

    set_backdrop(path, backdrop_style);
}

static void backdrop_set_style(int style)
{
    backdrop_style = style;

    set_backdrop(backdrop_path, style);
}

static void update_backdrop_channel(const char *name, McsAction action,
				    McsSetting *setting)
{
    switch (action)
    {
        case MCS_ACTION_NEW:
	    /* fall through */
        case MCS_ACTION_CHANGED:
	    if (strcmp(name, "style") == 0)
		backdrop_set_style(setting->data.v_int);
	    else if (strcmp(name, "path") == 0)
		backdrop_set_path(setting->data.v_string);
            break;
        case MCS_ACTION_DELETED:
	    /* We don't use this now. Perhaps revert to default? */
            break;
    }
}

void add_backdrop_callback(GHashTable *ht)
{
    g_hash_table_insert(ht, BACKDROP_CHANNEL, update_backdrop_channel);
}

void backdrop_load_settings(McsClient *client)
{
    McsSetting *setting;

    if (MCS_SUCCESS == mcs_client_get_setting(client, "style",  
					      BACKDROP_CHANNEL, &setting))
    {
	backdrop_style = setting->data.v_int;
	mcs_setting_free(setting);
    }
    
    if (MCS_SUCCESS == mcs_client_get_setting(client, "path", 
					      BACKDROP_CHANNEL, &setting))
    {
	if (setting->data.v_string)
	{
	    g_free(backdrop_path);
	    backdrop_path = g_strdup(setting->data.v_string);
	}
	
	mcs_setting_free(setting);
    }

    set_backdrop(backdrop_path, backdrop_style);
}

/* setting the background */
static char **get_list_from_file(const char *listfile)
{
    char *contents, *s1;
    GError *error = NULL;
    char **files = NULL;

    if (!g_file_get_contents(listfile, &contents, NULL, &error))
    {
	g_warning("xfdesktop: error reading backdrop list file: %s\n",
		  error->message);
	
	return NULL;
    }

    if (strncmp(LIST_TEXT, contents, strlen(LIST_TEXT)) != 0)
    {
	g_free(contents);
	g_warning("xfdesktop: not a backdrop list file: %s\n",
		  listfile);
	
	return NULL;
    }

    s1 = contents + strlen(LIST_TEXT) + 1;
        
    files = g_strsplit(s1, "\n", -1);
    g_free(contents);
    
    return files;
}

static int count_elements(char **list)
{
    char **c;
    int n = 0;

    for (c = list; *c; c++)
	n++;

    return n;
}

static char *get_path_from_listfile(const char *listfile)
{
    static char *prevfile = NULL;
    static int prevnumber = -1;
    static char **files = NULL;
    int i, n;

    if (!listfile)
    {
	g_free(prevfile);
	prevfile = NULL;
	return NULL;
    }

    if (!prevfile || strcmp(listfile, prevfile) != 0)
    {
	g_strfreev(files);
	g_free(prevfile);
	prevnumber = -1;
	
	prevfile = g_strdup(listfile);
	files = get_list_from_file(listfile);
    }
    
    n = count_elements(files);

    if (n == 0)
	return NULL;

    if (n == 1)
	return files[0];
    
    srand (time (0));
    i = rand () % n;

    return files[i];
}

static GdkPixmap *create_background_pixmap(GdkPixbuf *pixbuf, int style)
{
    GdkPixmap *pixmap = NULL;

    if (style == AUTO)
    {
	int height, width;
	/* if height and width are both less than half the screen
	 * -> tiled, else -> scaled */
	
        width = gdk_pixbuf_get_width(pixbuf);
        height = gdk_pixbuf_get_height(pixbuf);

	if (height <= screen_height / 2 && width <= screen_width)
	    style = TILED;
	else
	    style = SCALED;
    }
    
    if(style == SCALED)
    {
        GdkPixbuf *old = pixbuf;

        pixbuf = gdk_pixbuf_scale_simple(old, screen_width, screen_height,
                 	                 GDK_INTERP_HYPER);

        g_object_unref(old);
    }
    else if(style == CENTERED)
    {
        GdkPixbuf *old = pixbuf;
        int x, y, width, height;

        width = gdk_pixbuf_get_width(pixbuf);
        height = gdk_pixbuf_get_height(pixbuf);

        pixbuf = gdk_pixbuf_new(gdk_pixbuf_get_colorspace(old), 0, 8, 
				screen_width, screen_height);
        gdk_pixbuf_fill(pixbuf, 0);

        x = (screen_width - width) / 2;
        y = (screen_height - height) / 2;
        x = MAX(x, 0);
        y = MAX(y, 0);

        gdk_pixbuf_composite(old, pixbuf,
                             x, y,
                             MIN(screen_width, width),
                             MIN(screen_height, height),
                             x, y, 1, 1, GDK_INTERP_NEAREST, 255);
        g_object_unref(old);
    }

    gdk_pixbuf_render_pixmap_and_mask(pixbuf, &pixmap, NULL, 0);
    g_object_unref(pixbuf);

    return pixmap;
}

/* The code below is almost literally copied from ROX Filer
 * (c) by Thomas Leonard. See http://rox.sf.net. */
static void set_backdrop(const char *path, int style)
{
    GtkStyle *gstyle;
    GdkPixmap *pixmap;
    GdkPixbuf *pixbuf = NULL;
    GError *error = NULL;

    gstyle = gtk_style_copy(gtk_widget_get_style(fullscreen_window));
    
    if (path && *path)
    {
	if (is_backdrop_list(path))
	{
	    char *realpath = get_path_from_listfile(path);

	    set_backdrop(realpath, AUTO);
	    return;
	}

	pixbuf = gdk_pixbuf_new_from_file(path, &error);
    
	if(error)
	{
	    g_warning("xfdesktop: error loading backdrop image:\n%s\n",
		      error->message);
	    g_error_free(error);
	    set_backdrop(NULL, 0);
	    return;
	}

	pixmap = create_background_pixmap(pixbuf, style);
        gstyle->bg_pixmap[GTK_STATE_NORMAL] = pixmap;
    }
    else
    {
	gstyle->bg_pixmap[GTK_STATE_NORMAL] = NULL;
    }
   
    /* (un)set background */
    gtk_widget_set_style(fullscreen_window, gstyle);
    g_object_unref(gstyle);

    gtk_widget_queue_draw(fullscreen_window);

    /* Also update root window property (for transparent xterms, etc) */
    if(gstyle->bg_pixmap[GTK_STATE_NORMAL])
    {
        XID id = GDK_DRAWABLE_XID(gstyle->bg_pixmap[GTK_STATE_NORMAL]);
        gdk_property_change(gdk_get_default_root_window(),
                            gdk_atom_intern("_XROOTPMAP_ID", FALSE),
                            gdk_atom_intern("PIXMAP", FALSE),
                            32, GDK_PROP_MODE_REPLACE, (guchar *) & id, 1);
    }
    else
    {
        gdk_property_delete(gdk_get_default_root_window(),
                            gdk_atom_intern("_XROOTPMAP_ID", FALSE));
    }
}
