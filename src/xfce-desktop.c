/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004 Brian Tarricone, <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  Random portions taken from or inspired by the original xfdesktop for xfce4:
 *     Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *     Copyright (C) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <ctype.h>
#include <errno.h>

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <glib.h>
#include <gdk/gdkx.h>
#include <gtk/gtkwindow.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "xfce-desktop.h"
#include "xfdesktop-common.h"
#include "main.h"

static void xfce_desktop_class_init(XfceDesktopClass *klass);
static void xfce_desktop_init(XfceDesktop *desktop);
static void xfce_desktop_finalize(GObject *object);

struct _XfceDesktopPriv
{
    GdkScreen *gscreen;
    NetkScreen *netk_screen;
    
    GdkPixmap *bg_pixmap;
    
    guint nbackdrops;
    XfceBackdrop **backdrops;
};

GtkWindowClass *parent_class = NULL;

/* private functions */

static void
set_imgfile_root_property(XfceDesktop *desktop, const gchar *filename,
        gint monitor)
{
    gchar property_name[128];
    
    g_snprintf(property_name, 128, XFDESKTOP_IMAGE_FILE_FMT, monitor);
    gdk_property_change(gdk_screen_get_root_window(desktop->priv->gscreen),
                gdk_atom_intern(property_name, FALSE),
                gdk_x11_xatom_to_atom(XA_STRING), 8, GDK_PROP_MODE_REPLACE,
                (guchar *)filename, strlen(filename)+1);
}

static void
save_list_file_minus_one(const gchar *filename, const gchar **files, gint badi)
{
    FILE *fp;
    gint fd, i;

#ifdef O_EXLOCK
    if((fd = open (filename, O_CREAT|O_EXLOCK|O_TRUNC|O_WRONLY, 0640)) < 0) {
#else
    if((fd = open (filename, O_CREAT| O_TRUNC|O_WRONLY, 0640)) < 0) {
#endif
        xfce_err (_("Could not save file %s: %s\n\n"
                "Please choose another location or press "
                "cancel in the dialog to discard your changes"),
                filename, g_strerror(errno));
        return;
    }

    if((fp = fdopen (fd, "w")) == NULL) {
        g_warning ("Unable to fdopen(%s). This should not happen!\n", filename);
        close(fd);
        return;
    }

    fprintf (fp, "%s\n", LIST_TEXT);
    
    for(i = 0; files[i] && *files[i] && *files[i] != '\n'; i++) {
        if(i != badi)
            fprintf(fp, "%s\n", files[i]);
    }
    
    fclose(fp);
}

G_INLINE_FUNC gint
count_elements(const gchar **list)
{
    gint i, c = 0;
    
    for(i = 0; list[i]; i++) {
        if(*list[i] && *list[i] != '\n')
            c++;
    }
    
    return c;
}

static const gchar **
get_listfile_contents(const gchar *listfile)
{
    static gchar *prevfile = NULL;
    static gchar **files = NULL;
    static time_t mtime = 0;
    struct stat st;
    
    if(!listfile) {
        if(prevfile) {
            g_free(prevfile);
            prevfile = NULL;
        }
        return NULL;
    }
    
    if(stat(listfile, &st) < 0) {
        if(prevfile) {
            g_free(prevfile);
            prevfile = NULL;
        }
        mtime = 0;
        return NULL;
    }
    
    if(!prevfile || strcmp(listfile, prevfile) || mtime < st.st_mtime) {
        if(files)
            g_strfreev(files);
        if(prevfile)
            g_free(prevfile);
    
        files = get_list_from_file(listfile);
        prevfile = g_strdup(listfile);
        mtime = st.st_mtime;
    }
    
    return (const gchar **)files;
}

static const gchar *
get_path_from_listfile(const gchar *listfile)
{
    static gboolean __initialized = FALSE;
    static gint previndex = -1;
    gint i, n;
    const gchar **files;
    
    /* NOTE: 4.3BSD random()/srandom() are a) stronger and b) faster than
    * ANSI-C rand()/srand(). So we use random() if available
    */
    if (!__initialized)    {
        guint seed = time(NULL) ^ (getpid() + (getpid() << 15));
#ifdef HAVE_SRANDOM
        srandom(seed);
#else
        srand(seed);
#endif
        __initialized = TRUE;
    }
    
    do {
        /* get the contents of the list file */
        files = get_listfile_contents(listfile);
        
        /* if zero or one item, return immediately */
        n = count_elements(files);
        if(!n)
            return NULL;
        else if(n == 1)
            return (const gchar *)files[0];
        
        /* pick a random item */
        do {
#ifdef HAVE_SRANDOM
            i = random() % n;
#else
            i = rand() % n;
#endif
            if(i != previndex) /* same as last time? */
                break;
        } while(1);
        
        g_print("picked i=%d, %s\n", i, files[i]);
        /* validate the image; if it's good, return it */
        if(xfdesktop_check_image_file(files[i]))
            break;
        
        g_print("file not valid, ditching\n");
        
        /* bad image: remove it from the list and write it out */
        save_list_file_minus_one(listfile, files, i);
        previndex = -1;
        /* loop and try again */
    } while(1);
    
    return (const gchar *)files[(previndex = i)];
}

static void
backdrop_changed_cb(XfceBackdrop *backdrop, gpointer user_data)
{
    GtkWidget *desktop = user_data;
    GdkPixbuf *pix;
    GdkPixmap *pmap = NULL;
    GdkColormap *cmap;
    GdkScreen *gscreen;
    GdkRectangle rect;
    Pixmap xid;
    GdkWindow *groot;
    
    TRACE("dummy");
    
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    
    /* create/get the composited backdrop pixmap */
    pix = xfce_backdrop_get_pixbuf(backdrop);
    if(!pix)
        return;
    
    gscreen = XFCE_DESKTOP(desktop)->priv->gscreen;
    cmap = gdk_screen_get_system_colormap(gscreen);
    
    if(XFCE_DESKTOP(desktop)->priv->nbackdrops == 1) {    
        /* optimised for single monitor: just dump the pixbuf into a pixmap */
        gdk_pixbuf_render_pixmap_and_mask_for_colormap(pix, cmap, &pmap, NULL, 0);
        g_object_unref(G_OBJECT(pix));
        if(!pmap)
            return;
        rect.x = rect.y = 0;
        rect.width = gdk_screen_get_width(gscreen);
        rect.height = gdk_screen_get_height(gscreen);
    } else {
        /* multiple monitors (xinerama): download the current backdrop, paint
         * over the correct area, and upload it back.  this is slow, but
         * probably still faster than redoing the whole thing. */
        GdkPixmap *cur_pmap = NULL;
        GdkPixbuf *cur_pbuf = NULL;
        gint i, n = -1, swidth, sheight;
        
        for(i = 0; i < XFCE_DESKTOP(desktop)->priv->nbackdrops; i++) {
            if(backdrop == XFCE_DESKTOP(desktop)->priv->backdrops[i]) {
                n = i;
                break;
            }
        }
        if(n == -1) {
            g_object_unref(G_OBJECT(pix));
            return;
        }
        
        swidth = gdk_screen_get_width(gscreen);
        sheight = gdk_screen_get_height(gscreen);
        
        cur_pmap = XFCE_DESKTOP(desktop)->priv->bg_pixmap;
        if(cur_pmap) {
            gint pw, ph;
            gdk_drawable_get_size(GDK_DRAWABLE(cur_pmap), &pw, &ph);
            if(pw == swidth && ph == sheight) {
                cur_pbuf = gdk_pixbuf_get_from_drawable(NULL, 
                        GDK_DRAWABLE(cur_pmap), cmap, 0, 0, 0, 0, swidth,
                        sheight);
            } else
                cur_pmap = NULL;
        }
        /* if the style's bg_pixmap was empty, or the above failed... */
        if(!cur_pmap) {
            cur_pbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                    swidth, sheight);
        }
        
        gdk_screen_get_monitor_geometry(gscreen, n, &rect);
        gdk_pixbuf_copy_area(pix, 0, 0, gdk_pixbuf_get_width(pix),
                gdk_pixbuf_get_height(pix), cur_pbuf, rect.x, rect.y);
        g_object_unref(G_OBJECT(pix));
        pmap = NULL;
        gdk_pixbuf_render_pixmap_and_mask_for_colormap(cur_pbuf, cmap,
                &pmap, NULL, 0);
        g_object_unref(G_OBJECT(cur_pbuf));
        if(!pmap)
            return;
    }
    
    xid = GDK_DRAWABLE_XID(pmap);
    groot = gdk_screen_get_root_window(XFCE_DESKTOP(desktop)->priv->gscreen);
    gdk_error_trap_push();
    
    /* set root property for transparent Eterms */
    gdk_property_change(groot,
            gdk_atom_intern("_XROOTPMAP_ID", FALSE),
            gdk_atom_intern("PIXMAP", FALSE), 32,
            GDK_PROP_MODE_REPLACE, (guchar *)&xid, 1);
    /* set this other property because someone might need it sometime. */
    gdk_property_change(groot,
            gdk_atom_intern("ESETROOT_PMAP_ID", FALSE),
            gdk_atom_intern("PIXMAP", FALSE), 32,
            GDK_PROP_MODE_REPLACE, (guchar *)&xid, 1);
    /* and set the root window's BG pixmap, because aterm is somewhat lame. */
    gdk_window_set_back_pixmap(groot, pmap, FALSE);
    /* there really should be a standard for this crap... */
    
    gdk_error_trap_pop();
    
    /* clear the old pixmap, if any */
    if(XFCE_DESKTOP(desktop)->priv->bg_pixmap)
        g_object_unref(G_OBJECT(XFCE_DESKTOP(desktop)->priv->bg_pixmap));
    
    /* set the new pixmap and tell gtk to redraw it */
    XFCE_DESKTOP(desktop)->priv->bg_pixmap = pmap;
    gdk_window_set_back_pixmap(desktop->window, pmap, FALSE);
    gtk_widget_queue_draw_area(desktop, rect.x, rect.y, rect.width, rect.height);
}

static void
screen_size_changed_cb(GdkScreen *gscreen, gpointer user_data)
{
    XfceDesktop *desktop = user_data;
    gint w, h, i;
    GdkRectangle rect;
    
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    
    w = gdk_screen_get_width(gscreen);
    h = gdk_screen_get_height(gscreen);
    gtk_widget_set_size_request(GTK_WIDGET(desktop), w, h);
    gtk_window_resize(GTK_WINDOW(desktop), w, h);
    
    /* clear out the old pixmap so we don't use its size anymore */
    gtk_widget_set_style(GTK_WIDGET(desktop), NULL);
    for(i = 0; i < desktop->priv->nbackdrops; i++) {
        gdk_screen_get_monitor_geometry(gscreen, i, &rect);
        xfce_backdrop_set_size(desktop->priv->backdrops[i], rect.width, rect.height);
        backdrop_changed_cb(desktop->priv->backdrops[i], desktop);
    }
}
static void
load_initial_settings(XfceDesktop *desktop, McsClient *mcs_client)
{
    gchar setting_name[64];
    McsSetting *setting = NULL;
    gint screen, i;
    XfceBackdrop *backdrop;
    GdkColor color;
    
    screen = gdk_screen_get_number(desktop->priv->gscreen);
    
    for(i = 0; i < desktop->priv->nbackdrops; i++) {
        backdrop = desktop->priv->backdrops[i];
        
        g_snprintf(setting_name, 64, "showimage_%d_%d", screen, i);
        if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, setting_name,
                BACKDROP_CHANNEL, &setting))
        {
            xfce_backdrop_set_show_image(backdrop, setting->data.v_int);
            mcs_setting_free(setting);
            setting = NULL;
        }
        
        g_snprintf(setting_name, 64, "imagepath_%d_%d", screen, i);
        if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, setting_name,
                BACKDROP_CHANNEL, &setting))
        {
            if(is_backdrop_list(setting->data.v_string)) {
                const gchar *imgfile = get_path_from_listfile(setting->data.v_string);
                xfce_backdrop_set_image_filename(backdrop, imgfile);
                set_imgfile_root_property(desktop, imgfile, i);
            } else {
                xfce_backdrop_set_image_filename(backdrop, setting->data.v_string);
                set_imgfile_root_property(desktop, setting->data.v_string, i);
            }
            mcs_setting_free(setting);
            setting = NULL;
        } else
            xfce_backdrop_set_image_filename(backdrop, DEFAULT_BACKDROP);
        
        g_snprintf(setting_name, 64, "imagestyle_%d_%d", screen, i);
        if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, setting_name,
                BACKDROP_CHANNEL, &setting))
        {
            xfce_backdrop_set_image_style(backdrop, setting->data.v_int);
            mcs_setting_free(setting);
            setting = NULL;
        } else
            xfce_backdrop_set_image_style(backdrop, XFCE_BACKDROP_IMAGE_STRETCHED);
        
        g_snprintf(setting_name, 64, "color1_%d_%d", screen, i);
        if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, setting_name,
                BACKDROP_CHANNEL, &setting))
        {
            color.red = setting->data.v_color.red;
            color.green = setting->data.v_color.green;
            color.blue = setting->data.v_color.blue;
            xfce_backdrop_set_first_color(backdrop, &color);
            mcs_setting_free(setting);
            setting = NULL;
        } else {
            /* default color1 is #6985b7 */
            color.red = (guint16)0x6900;
            color.green = (guint16)0x8500;
            color.blue = (guint16)0xb700;
            xfce_backdrop_set_first_color(backdrop, &color);
        }
        
        g_snprintf(setting_name, 64, "color2_%d_%d", screen, i);
        if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, setting_name,
                BACKDROP_CHANNEL, &setting))
        {
            color.red = setting->data.v_color.red;
            color.green = setting->data.v_color.green;
            color.blue = setting->data.v_color.blue;
            xfce_backdrop_set_second_color(backdrop, &color);
            mcs_setting_free(setting);
            setting = NULL;
        } else {
            /* default color2 is #dbe8ff */
            color.red = (guint16)0xdb00;
            color.green = (guint16)0xe800;
            color.blue = (guint16)0xff00;
            xfce_backdrop_set_second_color(backdrop, &color);
        }
        
        g_snprintf(setting_name, 64, "colorstyle_%d_%d", screen, i);
        if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, setting_name,
                BACKDROP_CHANNEL, &setting))
        {
            xfce_backdrop_set_color_style(backdrop, setting->data.v_int);
            mcs_setting_free(setting);
            setting = NULL;
        } else
            xfce_backdrop_set_color_style(backdrop, XFCE_BACKDROP_COLOR_HORIZ_GRADIENT);
        
        g_snprintf(setting_name, 64, "brightness_%d_%d", screen, i);
        if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, setting_name,
                BACKDROP_CHANNEL, &setting))
        {
            xfce_backdrop_set_brightness(backdrop, setting->data.v_int);
            mcs_setting_free(setting);
            setting = NULL;
        } else
            xfce_backdrop_set_brightness(backdrop, 0);
    }
}

static void
screen_set_selection(XfceDesktop *desktop)
{
    Window xwin;
    gint xscreen;
    gchar selection_name[100];
    Atom selection_atom, manager_atom;
    
    xwin = GDK_WINDOW_XID(GTK_WIDGET(desktop)->window);
    xscreen = gdk_screen_get_number(desktop->priv->gscreen);
    
    g_snprintf(selection_name, 100, XFDESKTOP_SELECTION_FMT, xscreen);
    selection_atom = XInternAtom(GDK_DISPLAY(), selection_name, False);
    manager_atom = XInternAtom(GDK_DISPLAY(), "MANAGER", False);

    XSelectInput(GDK_DISPLAY(), xwin, PropertyChangeMask | ButtonPressMask);
    XSetSelectionOwner(GDK_DISPLAY(), selection_atom, xwin, GDK_CURRENT_TIME);

    /* listen for client messages */
    g_signal_connect(G_OBJECT(desktop), "client-event",
            G_CALLBACK(client_message_received), NULL);

    /* Check to see if we managed to claim the selection. If not,
     * we treat it as if we got it then immediately lost it */
    if(XGetSelectionOwner(GDK_DISPLAY(), selection_atom) == xwin) {
        XClientMessageEvent xev;
        Window xroot = GDK_WINDOW_XID(gdk_screen_get_root_window(desktop->priv->gscreen));
        
        xev.type = ClientMessage;
        xev.window = xroot;
        xev.message_type = manager_atom;
        xev.format = 32;
        xev.data.l[0] = GDK_CURRENT_TIME;
        xev.data.l[1] = selection_atom;
        xev.data.l[2] = xwin;
        xev.data.l[3] = 0;    /* manager specific data */
        xev.data.l[4] = 0;    /* manager specific data */

        XSendEvent(GDK_DISPLAY(), xroot, False, StructureNotifyMask, (XEvent *)&xev);
    } else {
        g_error("%s: could not set selection ownership", PACKAGE);
        exit(1);
    }
}

/* gobject-related functions */

GType
xfce_desktop_get_type()
{
    static GType desktop_type = 0;
    
    if(!desktop_type) {
        static const GTypeInfo desktop_info = {
            sizeof(XfceDesktopClass),
            NULL,
            NULL,
            (GClassInitFunc)xfce_desktop_class_init,
            NULL,
            NULL,
            sizeof(XfceDesktop),
            0,
            (GInstanceInitFunc)xfce_desktop_init
        };
        
        desktop_type = g_type_register_static(GTK_TYPE_WINDOW, "XfceDesktop",
                &desktop_info, 0);
    }
    
    return desktop_type;
}

static void
xfce_desktop_class_init(XfceDesktopClass *klass)
{
    GObjectClass *gobject_class;
    
    gobject_class = (GObjectClass *)klass;
    
    parent_class = g_type_class_peek_parent(klass);
    
    gobject_class->finalize = xfce_desktop_finalize;
}

static void
xfce_desktop_init(XfceDesktop *desktop)
{
    desktop->priv = g_new0(XfceDesktopPriv, 1);
    GTK_WINDOW(desktop)->type = GTK_WINDOW_TOPLEVEL;
}

static void
xfce_desktop_finalize(GObject *object)
{
    XfceDesktop *desktop = XFCE_DESKTOP(object);
    gint i;
    GdkWindow *groot;
    gchar property_name[128];
    
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    
    groot = gdk_screen_get_root_window(desktop->priv->gscreen);
    gdk_property_delete(groot, gdk_atom_intern("XFCE_DESKTOP_WINDOW", FALSE));
    gdk_property_delete(groot, gdk_atom_intern("NAUTILUS_DESKTOP_WINDOW_ID", FALSE));
    gdk_property_delete(groot, gdk_atom_intern("_XROOTPMAP_ID", FALSE));
    gdk_property_delete(groot, gdk_atom_intern("ESETROOT_PMAP_ID", FALSE));
    
    if(desktop->priv->backdrops) {
        for(i = 0; i < desktop->priv->nbackdrops; i++) {
            g_snprintf(property_name, 128, XFDESKTOP_IMAGE_FILE_FMT, i);
            gdk_property_delete(groot, gdk_atom_intern(property_name, FALSE));
            g_object_unref(G_OBJECT(desktop->priv->backdrops[i]));
        }
        g_free(desktop->priv->backdrops);
        desktop->priv->backdrops = NULL;
    }
    
    if(desktop->priv->bg_pixmap)
        g_object_unref(G_OBJECT(desktop->priv->bg_pixmap));
    
    g_free(desktop->priv);
    desktop->priv = NULL;
    
    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static gboolean
desktop_expose_cb(GtkWidget *w, GdkEventExpose *evt, gpointer user_data)
{    
    gdk_window_clear_area(w->window, evt->area.x, evt->area.y,
            evt->area.width, evt->area.height);
    
    return TRUE;
}

static void
style_set_cb(GtkWidget *w, GtkStyle *old, gpointer user_data)
{
    XfceDesktop *desktop = user_data;
    
    if(desktop->priv->bg_pixmap) {
        gdk_window_set_back_pixmap(w->window, desktop->priv->bg_pixmap, FALSE);
        gtk_widget_queue_draw(w);
    }
}


/* public api */

/**
 * xfce_desktop_new:
 * @gscreen: The current #GdkScreen.
 *
 * Creates a new #XfceDesktop for the specified #GdkScreen.  If @gscreen is
 * %NULL, the default screen will be used.
 *
 * Return value: A new #XfceDesktop.
 **/
GtkWidget *
xfce_desktop_new(GdkScreen *gscreen, McsClient *mcs_client)
{
    XfceDesktop *desktop = g_object_new(XFCE_TYPE_DESKTOP, NULL);
    GdkAtom atom;
    gint i;
    Window xid;
    GdkDisplay *gdpy;
    GdkWindow *groot;
    
    if(!gscreen)
        gscreen = gdk_display_get_default_screen(gdk_display_get_default());
    desktop->priv->gscreen = gscreen;
    
    gtk_window_set_screen(GTK_WINDOW(desktop), gscreen);
    
    desktop->priv->netk_screen = netk_screen_get(gdk_screen_get_number(gscreen));
    netk_screen_force_update(desktop->priv->netk_screen);
    
    netk_gtk_window_avoid_input(GTK_WINDOW(desktop));
    gtk_widget_set_size_request(GTK_WIDGET(desktop),
            gdk_screen_get_width(gscreen), gdk_screen_get_height(gscreen));
    gtk_window_move(GTK_WINDOW(desktop), 0, 0);
    
    gtk_widget_realize(GTK_WIDGET(desktop));
    
    gtk_window_set_title(GTK_WINDOW(desktop), _("Desktop"));
    if(GTK_WIDGET_DOUBLE_BUFFERED(GTK_WIDGET(desktop)))
        gtk_widget_set_double_buffered(GTK_WIDGET(desktop), FALSE);
    gtk_window_set_type_hint(GTK_WINDOW(desktop), GDK_WINDOW_TYPE_HINT_DESKTOP);
    
    atom = gdk_atom_intern("_NET_WM_WINDOW_TYPE_DESKTOP", FALSE);
    gdk_property_change(GTK_WIDGET(desktop)->window,
            gdk_atom_intern("_NET_WM_WINDOW_TYPE", FALSE),
            gdk_atom_intern("ATOM", FALSE), 32,
            GDK_PROP_MODE_REPLACE, (guchar *)&atom, 1);
    
    gdpy = gdk_screen_get_display(gscreen);
    xid = GDK_WINDOW_XID(GTK_WIDGET(desktop)->window);
    groot = gdk_screen_get_root_window(gscreen);
    
    gdk_property_change(groot,
            gdk_atom_intern("XFCE_DESKTOP_WINDOW", FALSE),
            gdk_atom_intern("WINDOW", FALSE), 32,
            GDK_PROP_MODE_REPLACE, (guchar *)&xid, 1);
    
    gdk_property_change(groot,
            gdk_atom_intern("NAUTILUS_DESKTOP_WINDOW_ID", FALSE),
            gdk_atom_intern("WINDOW", FALSE), 32,
            GDK_PROP_MODE_REPLACE, (guchar *)&xid, 1);
    
    screen_set_selection(desktop);
    
    desktop->priv->nbackdrops = gdk_screen_get_n_monitors(gscreen);
    desktop->priv->backdrops = g_new(XfceBackdrop *, desktop->priv->nbackdrops);
    for(i = 0; i < desktop->priv->nbackdrops; i++) {
        GdkRectangle rect;
        gdk_screen_get_monitor_geometry(gscreen, i, &rect);
        desktop->priv->backdrops[i] = xfce_backdrop_new_with_size(rect.width,
                rect.height);
    }
    
    if(mcs_client)
        load_initial_settings(desktop, mcs_client);
    
    for(i = 0; i < desktop->priv->nbackdrops; i++) {
        g_signal_connect(G_OBJECT(desktop->priv->backdrops[i]), "changed",
                G_CALLBACK(backdrop_changed_cb), desktop);
        backdrop_changed_cb(desktop->priv->backdrops[i], desktop);
    }
    
    g_signal_connect(G_OBJECT(gscreen), "size-changed",
            G_CALLBACK(screen_size_changed_cb), desktop);
    
    gtk_widget_add_events(GTK_WIDGET(desktop), GDK_EXPOSE);
    g_signal_connect(G_OBJECT(desktop), "expose-event",
            G_CALLBACK(desktop_expose_cb), NULL);
    
    g_signal_connect(G_OBJECT(desktop), "style-set",
            G_CALLBACK(style_set_cb), desktop);
    
    return GTK_WIDGET(desktop);
}

guint
xfce_desktop_get_n_monitors(XfceDesktop *desktop)
{
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), 0);
    
    return desktop->priv->nbackdrops;
}

XfceBackdrop *
xfce_desktop_get_backdrop(XfceDesktop *desktop, guint n)
{
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), NULL);
    g_return_val_if_fail(n < desktop->priv->nbackdrops, NULL);
    
    return desktop->priv->backdrops[n];
}

gint
xfce_desktop_get_width(XfceDesktop *desktop)
{
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), -1);
    
    return gdk_screen_get_width(desktop->priv->gscreen);
}

gint
xfce_desktop_get_height(XfceDesktop *desktop)
{
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), -1);
    
    return gdk_screen_get_height(desktop->priv->gscreen);
}

gboolean
xfce_desktop_settings_changed(McsClient *client, McsAction action,
        McsSetting *setting, gpointer user_data)
{
    XfceDesktop *desktop = user_data;
    XfceBackdrop *backdrop;
    gchar *sname, *p, *q;
    gint screen, monitor;
    GdkColor color;
    gboolean handled = FALSE;
    
    TRACE("dummy");
    
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), FALSE);
    
    /* get the screen and monitor number */
    sname = g_strdup(setting->name);
    q = g_strrstr(sname, "_");
    if(!q || q == sname) {
        g_free(sname);
        return FALSE;
    }
    p = strstr(sname, "_");
    if(!p || p == q) {
        g_free(sname);
        return FALSE;
    }
    *q = 0;
    screen = atoi(p+1);
    monitor = atoi(q+1);
    g_free(sname);
    
    if(screen == -1 || monitor == -1
            || screen != gdk_screen_get_number(desktop->priv->gscreen)
            || monitor >= desktop->priv->nbackdrops)
    {
        /* not ours */
        return FALSE;
    }
    
    backdrop = desktop->priv->backdrops[monitor];
    if(!backdrop)
        return FALSE;
    
    switch(action) {
        case MCS_ACTION_NEW:
        case MCS_ACTION_CHANGED:
            if(strstr(setting->name, "showimage") == setting->name) {
                xfce_backdrop_set_show_image(backdrop, setting->data.v_int);
                handled = TRUE;
            } else if(strstr(setting->name, "imagepath") == setting->name) {
                if(is_backdrop_list(setting->data.v_string)) {
                    const gchar *imgfile = get_path_from_listfile(setting->data.v_string);
                    xfce_backdrop_set_image_filename(backdrop, imgfile);
                    set_imgfile_root_property(desktop, imgfile, monitor);
                } else {
                    xfce_backdrop_set_image_filename(backdrop,
                            setting->data.v_string);
                    set_imgfile_root_property(desktop, setting->data.v_string,
                            monitor);
                }
                handled = TRUE;
            } else if(strstr(setting->name, "imagestyle") == setting->name) {
                xfce_backdrop_set_image_style(backdrop, setting->data.v_int);
                handled = TRUE;
            } else if(strstr(setting->name, "color1") == setting->name) {
                color.red = setting->data.v_color.red;
                color.blue = setting->data.v_color.blue;
                color.green = setting->data.v_color.green;
                xfce_backdrop_set_first_color(backdrop, &color);
                handled = TRUE;
            } else if(strstr(setting->name, "color2") == setting->name) {
                color.red = setting->data.v_color.red;
                color.blue = setting->data.v_color.blue;
                color.green = setting->data.v_color.green;
                xfce_backdrop_set_second_color(backdrop, &color);
                handled = TRUE;
            } else if(strstr(setting->name, "colorstyle") == setting->name) {
                xfce_backdrop_set_color_style(backdrop, setting->data.v_int);
                handled = TRUE;
            } else if(strstr(setting->name, "brightness") == setting->name) {
                xfce_backdrop_set_brightness(backdrop, setting->data.v_int);
                handled = TRUE;
            }
            
            break;
        
        case MCS_ACTION_DELETED:
            break;
    }
    
    return handled;
}
