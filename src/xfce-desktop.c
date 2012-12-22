/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004-2007 Brian Tarricone, <bjt23@cornell.edu>
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

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <glib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#ifdef ENABLE_DESKTOP_ICONS
#include "xfdesktop-icon-view.h"
#include "xfdesktop-window-icon-manager.h"
# ifdef ENABLE_FILE_ICONS
# include "xfdesktop-file-icon-manager.h"
# include "xfdesktop-special-file-icon.h"
# endif
#endif

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include <xfconf/xfconf.h>

#include "xfdesktop-common.h"
#include "xfce-desktop.h"
#include "xfce-desktop-enum-types.h"

/* disable setting the x background for bug 7442 */
//#define DISABLE_FOR_BUG7442

struct _XfceDesktopPriv
{
    GdkScreen *gscreen;
    gboolean updates_frozen;

    XfconfChannel *channel;
    gchar *property_prefix;
    
    GdkPixmap *bg_pixmap;
    
    guint nbackdrops;
    XfceBackdrop **backdrops;
    
    gboolean xinerama_stretch;
    
    SessionLogoutFunc session_logout_func;
    
#ifdef ENABLE_DESKTOP_ICONS
    XfceDesktopIconStyle icons_style;
    gboolean icons_font_size_set;
    guint icons_font_size;
    guint icons_size;
    GtkWidget *icon_view;
    gdouble system_font_size;
#endif
};

enum
{
    SIG_POPULATE_ROOT_MENU = 0,
    SIG_POPULATE_SECONDARY_ROOT_MENU,
    N_SIGNALS
};

enum
{
    PROP_0 = 0,
    PROP_XINERAMA_STRETCH,
#ifdef ENABLE_DESKTOP_ICONS
    PROP_ICON_STYLE,
    PROP_ICON_SIZE,
    PROP_ICON_FONT_SIZE,
    PROP_ICON_FONT_SIZE_SET,
#endif
};


static void xfce_desktop_finalize(GObject *object);
static void xfce_desktop_set_property(GObject *object,
                                      guint property_id,
                                      const GValue *value,
                                      GParamSpec *pspec);
static void xfce_desktop_get_property(GObject *object,
                                      guint property_id,
                                      GValue *value,
                                      GParamSpec *pspec);

static void xfce_desktop_realize(GtkWidget *widget);
static void xfce_desktop_unrealize(GtkWidget *widget);
static gboolean xfce_desktop_button_press_event(GtkWidget *widget,
                                                GdkEventButton *evt);
static gboolean xfce_desktop_popup_menu(GtkWidget *widget);

static gboolean xfce_desktop_expose(GtkWidget *w,
                                    GdkEventExpose *evt);
static gboolean xfce_desktop_delete_event(GtkWidget *w,
                                          GdkEventAny *evt);
static void xfce_desktop_style_set(GtkWidget *w,
                                   GtkStyle *old_style);

static void xfce_desktop_connect_backdrop_settings(XfceDesktop *desktop,
                                                   XfceBackdrop *backdrop,
                                                   guint monitor);

static guint signals[N_SIGNALS] = { 0, };

/* private functions */

#ifdef ENABLE_DESKTOP_ICONS
static gdouble
xfce_desktop_ensure_system_font_size(XfceDesktop *desktop)
{
    GdkScreen *gscreen;
    GtkSettings *settings;
    gchar *font_name = NULL;
    PangoFontDescription *pfd;
    
    gscreen = gtk_widget_get_screen(GTK_WIDGET(desktop));
    /* FIXME: needed? */
    if(!gscreen)
        gscreen = gdk_display_get_default_screen(gdk_display_get_default());
    
    settings = gtk_settings_get_for_screen(gscreen);
    g_object_get(G_OBJECT(settings), "gtk-font-name", &font_name, NULL);
    
    pfd = pango_font_description_from_string(font_name);
    desktop->priv->system_font_size = pango_font_description_get_size(pfd);
    /* FIXME: this seems backwards from the documentation */
    if(!pango_font_description_get_size_is_absolute(pfd)) {
        DBG("dividing by PANGO_SCALE");
        desktop->priv->system_font_size /= PANGO_SCALE;
    }
    DBG("system font size is %.05f", desktop->priv->system_font_size);
    
    g_free(font_name);
    pango_font_description_free(pfd);
    
    return desktop->priv->system_font_size;
}

static void
xfce_desktop_setup_icon_view(XfceDesktop *desktop)
{
    XfdesktopIconViewManager *manager = NULL;
    
    switch(desktop->priv->icons_style) {
        case XFCE_DESKTOP_ICON_STYLE_NONE:
            /* nada */
            break;
        
        case XFCE_DESKTOP_ICON_STYLE_WINDOWS:
            manager = xfdesktop_window_icon_manager_new(desktop->priv->gscreen);
            break;
        
#ifdef ENABLE_FILE_ICONS
        case XFCE_DESKTOP_ICON_STYLE_FILES:
            {
                GFile *file;
                const gchar *desktop_path;
                
                desktop_path = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
                file = g_file_new_for_path(desktop_path);
                manager = xfdesktop_file_icon_manager_new(file, desktop->priv->channel);
                g_object_unref(file);
            }
            break;
#endif
        
        default:
            g_critical("Unusable XfceDesktopIconStyle: %d.  Unable to " \
                       "display desktop icons.",
                       desktop->priv->icons_style);
            break;
    }
    
    if(manager) {
        xfce_desktop_ensure_system_font_size(desktop);
        
        desktop->priv->icon_view = xfdesktop_icon_view_new(manager);
        xfdesktop_icon_view_set_font_size(XFDESKTOP_ICON_VIEW(desktop->priv->icon_view),
                                          (!desktop->priv->icons_font_size_set
                                           || !desktop->priv->icons_font_size)
                                          ? desktop->priv->system_font_size
                                          : desktop->priv->icons_font_size);
        if(desktop->priv->icons_size > 0) {
            xfdesktop_icon_view_set_icon_size(XFDESKTOP_ICON_VIEW(desktop->priv->icon_view),
                                              desktop->priv->icons_size);
        }
        gtk_widget_show(desktop->priv->icon_view);
        gtk_container_add(GTK_CONTAINER(desktop), desktop->priv->icon_view);
    }
    
    gtk_widget_queue_draw(GTK_WIDGET(desktop));
}
#endif

static void
set_imgfile_root_property(XfceDesktop *desktop, const gchar *filename,
        gint monitor)
{
    gchar property_name[128];
    
    gdk_error_trap_push();
    
    g_snprintf(property_name, 128, XFDESKTOP_IMAGE_FILE_FMT, monitor);
    if(filename) {
        gdk_property_change(gdk_screen_get_root_window(desktop->priv->gscreen),
                            gdk_atom_intern(property_name, FALSE),
                            gdk_x11_xatom_to_atom(XA_STRING), 8,
                            GDK_PROP_MODE_REPLACE,
                            (guchar *)filename, strlen(filename)+1);
    } else {
        gdk_property_delete(gdk_screen_get_root_window(desktop->priv->gscreen),
                            gdk_atom_intern(property_name, FALSE));
    }
    
    gdk_error_trap_pop();
}

static void
set_real_root_window_pixmap(GdkScreen *gscreen,
                            GdkPixmap *pmap)
{
#ifndef DISABLE_FOR_BUG7442
    Window xid;
    GdkWindow *groot;
    
    xid = GDK_DRAWABLE_XID(pmap);
    groot = gdk_screen_get_root_window(gscreen);
    
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
#endif
}

static void
backdrop_changed_cb(XfceBackdrop *backdrop, gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);
    GdkPixmap *pmap = desktop->priv->bg_pixmap;
    GdkScreen *gscreen = desktop->priv->gscreen;
    GdkPixbuf *pix;
    GdkRectangle rect;
    guint i;
    gint monitor = -1;
    
    TRACE("entering");
    
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    
    if(desktop->priv->updates_frozen || !GTK_WIDGET_REALIZED(GTK_WIDGET(desktop)))
        return;
    
    TRACE("really entering");
    
    for(i = 0; i < XFCE_DESKTOP(desktop)->priv->nbackdrops; i++) {
        if(backdrop == XFCE_DESKTOP(desktop)->priv->backdrops[i]) {
            monitor = i;
            break;
        }
    }
    if(monitor == -1)
        return;
    
    /* create/get the composited backdrop pixmap */
    pix = xfce_backdrop_get_pixbuf(backdrop);
    if(!pix)
        return;

    if(desktop->priv->xinerama_stretch) {
        GdkRectangle monitor_rect;

        gdk_screen_get_monitor_geometry(gscreen, 0, &rect);

        /* Get the lowest x and y value for all the monitors in
         * case none of them start at 0,0 for whatever reason.
         */
        for(i = 1; i < (guint)gdk_screen_get_n_monitors(gscreen); i++) {
            gdk_screen_get_monitor_geometry(gscreen, i, &monitor_rect);

            if(monitor_rect.x < rect.x)
                rect.x = monitor_rect.x;
            if(monitor_rect.y < rect.y)
                rect.y = monitor_rect.y;
        }

        rect.width = gdk_screen_get_width(gscreen);
        rect.height = gdk_screen_get_height(gscreen);
    } else {
        gdk_screen_get_monitor_geometry(gscreen, monitor, &rect);
    }

    gdk_draw_pixbuf(GDK_DRAWABLE(pmap), GTK_WIDGET(desktop)->style->black_gc,
                    pix, 0, 0, rect.x, rect.y,
                    gdk_pixbuf_get_width(pix), gdk_pixbuf_get_height(pix),
                    GDK_RGB_DITHER_MAX, 0, 0);
    g_object_unref(G_OBJECT(pix));
    
    /* tell gtk to redraw the repainted area */
    gtk_widget_queue_draw_area(GTK_WIDGET(desktop), rect.x, rect.y,
                               rect.width, rect.height);
    
    set_imgfile_root_property(desktop,
                              xfce_backdrop_get_image_filename(backdrop),
                              monitor);
    
    /* do this again so apps watching the root win notice the update */
    set_real_root_window_pixmap(gscreen, pmap);
}

static void
backdrop_cycle_cb(XfceBackdrop *backdrop, gpointer user_data)
{
    const gchar* backdrop_list;

    TRACE("entering");

    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));

    backdrop_list = xfce_backdrop_get_list(backdrop);

    if(xfdesktop_backdrop_list_is_valid(backdrop_list)) {
        gchar *backdrop_file;
        GError *error = NULL;

        backdrop_file = xfdesktop_backdrop_list_choose_random(backdrop_list,
                                                              &error);

        xfce_backdrop_set_image_filename(backdrop, backdrop_file);
        g_free(backdrop_file);
        backdrop_changed_cb(backdrop, user_data);
    }
}

static void
screen_size_changed_cb(GdkScreen *gscreen, gpointer user_data)
{
    XfceDesktop *desktop = user_data;
    gint w, h;
    
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    
    w = gdk_screen_get_width(gscreen);
    h = gdk_screen_get_height(gscreen);
    gtk_widget_set_size_request(GTK_WIDGET(desktop), w, h);
    gtk_window_resize(GTK_WINDOW(desktop), w, h);
    
    if(desktop->priv->bg_pixmap)
        g_object_unref(G_OBJECT(desktop->priv->bg_pixmap));
    desktop->priv->bg_pixmap = gdk_pixmap_new(GDK_DRAWABLE(GTK_WIDGET(desktop)->window),
                                              w, h, -1);
    set_real_root_window_pixmap(desktop->priv->gscreen,
                                desktop->priv->bg_pixmap);
    gdk_window_set_back_pixmap(GTK_WIDGET(desktop)->window,
                               desktop->priv->bg_pixmap, FALSE);
    
    /* special case for 1 backdrop to handle xinerama stretching */
    if(desktop->priv->xinerama_stretch) {
        xfce_backdrop_set_size(desktop->priv->backdrops[0], w, h);
        backdrop_changed_cb(desktop->priv->backdrops[0], desktop);
    } else {
        GdkRectangle rect;
        guint i;

        for(i = 0; i < desktop->priv->nbackdrops; i++) {
            gdk_screen_get_monitor_geometry(gscreen, i, &rect);
            xfce_backdrop_set_size(desktop->priv->backdrops[i], rect.width,
                                   rect.height);
            backdrop_changed_cb(desktop->priv->backdrops[i], desktop);
        }
    }
}

static void
screen_composited_changed_cb(GdkScreen *gscreen,
                             gpointer user_data)
{
    /* fake a screen size changed, so the background is properly set */
    screen_size_changed_cb(gscreen, user_data);
}

static void
xfce_desktop_monitors_changed(GdkScreen *gscreen,
                              gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);
    guint i;

    if(desktop->priv->xinerama_stretch) {
        if(desktop->priv->nbackdrops > 1) {
            for(i = 1; i < desktop->priv->nbackdrops; ++i)
                g_object_unref(G_OBJECT(desktop->priv->backdrops[i]));
        }

        if(desktop->priv->nbackdrops != 1) {
            desktop->priv->backdrops = g_realloc(desktop->priv->backdrops,
                                                 sizeof(XfceBackdrop *));
            if(!desktop->priv->nbackdrops) {
                GdkVisual *vis = gtk_widget_get_visual(GTK_WIDGET(desktop));
                desktop->priv->backdrops[0] = xfce_backdrop_new(vis);
                xfce_desktop_connect_backdrop_settings(desktop,
                                                       desktop->priv->backdrops[0],
                                                       0);
                g_signal_connect(G_OBJECT(desktop->priv->backdrops[0]),
                                 "changed",
                                 G_CALLBACK(backdrop_changed_cb), desktop);
                g_signal_connect(G_OBJECT(desktop->priv->backdrops[0]),
                                 "cycle",
                                 G_CALLBACK(backdrop_cycle_cb), desktop);
            }
            desktop->priv->nbackdrops = 1;
        }
    } else {
        guint n_monitors = gdk_screen_get_n_monitors(gscreen);

        if(n_monitors < desktop->priv->nbackdrops) {
            for(i = n_monitors; i < desktop->priv->nbackdrops; ++i)
                g_object_unref(G_OBJECT(desktop->priv->backdrops[i]));
        }

        if(n_monitors != desktop->priv->nbackdrops) {
            desktop->priv->backdrops = g_realloc(desktop->priv->backdrops,
                                                 sizeof(XfceBackdrop *) * n_monitors);
            if(n_monitors > desktop->priv->nbackdrops) {
                GdkVisual *vis = gtk_widget_get_visual(GTK_WIDGET(desktop));
                for(i = desktop->priv->nbackdrops; i < n_monitors; ++i) {
                    desktop->priv->backdrops[i] = xfce_backdrop_new(vis);
                    xfce_desktop_connect_backdrop_settings(desktop,
                                                           desktop->priv->backdrops[i],
                                                           i);
                    g_signal_connect(G_OBJECT(desktop->priv->backdrops[i]),
                                     "changed",
                                     G_CALLBACK(backdrop_changed_cb),
                                     desktop);
                    g_signal_connect(G_OBJECT(desktop->priv->backdrops[i]),
                                     "cycle",
                                     G_CALLBACK(backdrop_cycle_cb),
                                     desktop);
                }
            }
            desktop->priv->nbackdrops = n_monitors;
        }
    }

    /* update the total size of the screen and the size of each backdrop */
    screen_size_changed_cb(gscreen, desktop);
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

    /* the previous check in src/main.c occurs too early, so workaround by
     * adding this one. */
   if(XGetSelectionOwner(GDK_DISPLAY(), selection_atom) != None) {
       g_warning("%s: already running, quitting.", PACKAGE);
       exit(0);
   }

    XSelectInput(GDK_DISPLAY(), xwin, PropertyChangeMask | ButtonPressMask);
    XSetSelectionOwner(GDK_DISPLAY(), selection_atom, xwin, GDK_CURRENT_TIME);

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


G_DEFINE_TYPE(XfceDesktop, xfce_desktop, GTK_TYPE_WINDOW)


static void
xfce_desktop_class_init(XfceDesktopClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;
    
    g_type_class_add_private(klass, sizeof(XfceDesktopPriv));
    
    gobject_class->finalize = xfce_desktop_finalize;
    gobject_class->set_property = xfce_desktop_set_property;
    gobject_class->get_property = xfce_desktop_get_property;
    
    widget_class->realize = xfce_desktop_realize;
    widget_class->unrealize = xfce_desktop_unrealize;
    widget_class->button_press_event = xfce_desktop_button_press_event;
    widget_class->expose_event = xfce_desktop_expose;
    widget_class->delete_event = xfce_desktop_delete_event;
    widget_class->popup_menu = xfce_desktop_popup_menu;
    widget_class->style_set = xfce_desktop_style_set;
    
    signals[SIG_POPULATE_ROOT_MENU] = g_signal_new("populate-root-menu",
                                                   XFCE_TYPE_DESKTOP,
                                                   G_SIGNAL_RUN_LAST,
                                                   G_STRUCT_OFFSET(XfceDesktopClass,
                                                                   populate_root_menu),
                                                   NULL, NULL,
                                                   g_cclosure_marshal_VOID__OBJECT,
                                                   G_TYPE_NONE, 1,
                                                   GTK_TYPE_MENU_SHELL);
    
    signals[SIG_POPULATE_SECONDARY_ROOT_MENU] = g_signal_new("populate-secondary-root-menu",
                                                             XFCE_TYPE_DESKTOP,
                                                             G_SIGNAL_RUN_LAST,
                                                             G_STRUCT_OFFSET(XfceDesktopClass,
                                                                             populate_secondary_root_menu),
                                                             NULL, NULL,
                                                             g_cclosure_marshal_VOID__OBJECT,
                                                             G_TYPE_NONE, 1,
                                                             GTK_TYPE_MENU_SHELL);

#define XFDESKTOP_PARAM_FLAGS  (G_PARAM_READWRITE \
                                | G_PARAM_CONSTRUCT \
                                | G_PARAM_STATIC_NAME \
                                | G_PARAM_STATIC_NICK \
                                | G_PARAM_STATIC_BLURB)

    g_object_class_install_property(gobject_class, PROP_XINERAMA_STRETCH,
                                    g_param_spec_boolean("xinerama-stretch",
                                                         "xinerama stretch",
                                                         "xinerama stretch",
                                                         FALSE,
                                                         XFDESKTOP_PARAM_FLAGS));

#ifdef ENABLE_DESKTOP_ICONS
    g_object_class_install_property(gobject_class, PROP_ICON_STYLE,
                                    g_param_spec_enum("icon-style",
                                                      "icon style",
                                                      "icon style",
                                                      XFCE_TYPE_DESKTOP_ICON_STYLE,
#ifdef ENABLE_FILE_ICONS
                                                      XFCE_DESKTOP_ICON_STYLE_FILES,
#else
                                                      XFCE_DESKTOP_ICON_STYLE_WINDOWS,
#endif
                                                      XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_ICON_SIZE,
                                    g_param_spec_uint("icon-size",
                                                      "icon size",
                                                      "icon size",
                                                      8, 192, 36,
                                                      XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_ICON_FONT_SIZE,
                                    g_param_spec_uint("icon-font-size",
                                                      "icon font size",
                                                      "icon font size",
                                                      4, 144, 12,
                                                      XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_ICON_FONT_SIZE_SET,
                                    g_param_spec_boolean("icon-font-size-set",
                                                         "icon font size set",
                                                         "icon font size set",
                                                         FALSE,
                                                         XFDESKTOP_PARAM_FLAGS));
#endif
#undef XFDESKTOP_PARAM_FLAGS
}

static void
xfce_desktop_init(XfceDesktop *desktop)
{
    desktop->priv = G_TYPE_INSTANCE_GET_PRIVATE(desktop, XFCE_TYPE_DESKTOP,
                                                XfceDesktopPriv);
    GTK_WINDOW(desktop)->type = GTK_WINDOW_TOPLEVEL;
    
    gtk_window_set_type_hint(GTK_WINDOW(desktop), GDK_WINDOW_TYPE_HINT_DESKTOP);
    gtk_window_set_accept_focus(GTK_WINDOW(desktop), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(desktop), FALSE);
}

static void
xfce_desktop_finalize(GObject *object)
{
    XfceDesktop *desktop = XFCE_DESKTOP(object);
    
    g_object_unref(G_OBJECT(desktop->priv->channel));
    g_free(desktop->priv->property_prefix);

    G_OBJECT_CLASS(xfce_desktop_parent_class)->finalize(object);
}

static void
xfce_desktop_set_property(GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
    XfceDesktop *desktop = XFCE_DESKTOP(object);

    switch(property_id) {
        case PROP_XINERAMA_STRETCH:
            xfce_desktop_set_xinerama_stretch(desktop,
                                              g_value_get_boolean(value));
            break;

#ifdef ENABLE_DESKTOP_ICONS
        case PROP_ICON_STYLE:
            xfce_desktop_set_icon_style(desktop,
                                        g_value_get_enum(value));
            break;

        case PROP_ICON_SIZE:
            xfce_desktop_set_icon_size(desktop,
                                       g_value_get_uint(value));
            break;

        case PROP_ICON_FONT_SIZE:
            xfce_desktop_set_icon_font_size(desktop,
                                            g_value_get_uint(value));
            break;

        case PROP_ICON_FONT_SIZE_SET:
            xfce_desktop_set_use_icon_font_size(desktop,
                                                g_value_get_boolean(value));
            break;

#endif
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfce_desktop_get_property(GObject *object,
                          guint property_id,
                          GValue *value,
                          GParamSpec *pspec)
{
    XfceDesktop *desktop = XFCE_DESKTOP(object);

    switch(property_id) {
        case PROP_XINERAMA_STRETCH:
            g_value_set_boolean(value, desktop->priv->xinerama_stretch);
            break;

#ifdef ENABLE_DESKTOP_ICONS
        case PROP_ICON_STYLE:
            g_value_set_enum(value, desktop->priv->icons_style);
            break;

        case PROP_ICON_SIZE:
            g_value_set_uint(value, desktop->priv->icons_size);
            break;

        case PROP_ICON_FONT_SIZE:
            g_value_set_uint(value, desktop->priv->icons_font_size);
            break;

        case PROP_ICON_FONT_SIZE_SET:
            g_value_set_boolean(value, desktop->priv->icons_font_size_set);
            break;

#endif
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfce_desktop_realize(GtkWidget *widget)
{
    XfceDesktop *desktop = XFCE_DESKTOP(widget);
    GdkAtom atom;
    gint sw, sh;
    Window xid;
    GdkWindow *groot;
    
    TRACE("entering");

    gtk_window_set_screen(GTK_WINDOW(desktop), desktop->priv->gscreen);
    sw = gdk_screen_get_width(desktop->priv->gscreen);
    sh = gdk_screen_get_height(desktop->priv->gscreen);
    if(gtk_major_version > 2
       || (gtk_major_version == 2 && gtk_minor_version >= 13))
    {
        g_signal_connect(G_OBJECT(desktop->priv->gscreen),
                         "monitors-changed",
                         G_CALLBACK(xfce_desktop_monitors_changed),
                         desktop);
    }
    
    /* chain up */
    GTK_WIDGET_CLASS(xfce_desktop_parent_class)->realize(widget);
    
    gtk_window_set_title(GTK_WINDOW(desktop), _("Desktop"));
    
    gtk_widget_set_size_request(GTK_WIDGET(desktop), sw, sh);
    gtk_window_move(GTK_WINDOW(desktop), 0, 0);
    
    atom = gdk_atom_intern("_NET_WM_WINDOW_TYPE_DESKTOP", FALSE);
    gdk_property_change(GTK_WIDGET(desktop)->window,
            gdk_atom_intern("_NET_WM_WINDOW_TYPE", FALSE),
            gdk_atom_intern("ATOM", FALSE), 32,
            GDK_PROP_MODE_REPLACE, (guchar *)&atom, 1);

    xid = GDK_WINDOW_XID(GTK_WIDGET(desktop)->window);
    groot = gdk_screen_get_root_window(desktop->priv->gscreen);
    
    gdk_property_change(groot,
            gdk_atom_intern("XFCE_DESKTOP_WINDOW", FALSE),
            gdk_atom_intern("WINDOW", FALSE), 32,
            GDK_PROP_MODE_REPLACE, (guchar *)&xid, 1);
    
    gdk_property_change(groot,
            gdk_atom_intern("NAUTILUS_DESKTOP_WINDOW_ID", FALSE),
            gdk_atom_intern("WINDOW", FALSE), 32,
            GDK_PROP_MODE_REPLACE, (guchar *)&xid, 1);
    
    screen_set_selection(desktop);

    xfce_desktop_monitors_changed(desktop->priv->gscreen, desktop);
    
    g_signal_connect(G_OBJECT(desktop->priv->gscreen), "size-changed",
            G_CALLBACK(screen_size_changed_cb), desktop);
    g_signal_connect(G_OBJECT(desktop->priv->gscreen), "composited-changed",
            G_CALLBACK(screen_composited_changed_cb), desktop);
    
    gtk_widget_add_events(GTK_WIDGET(desktop), GDK_EXPOSURE_MASK);
    
#ifdef ENABLE_DESKTOP_ICONS
    xfce_desktop_setup_icon_view(desktop);
#endif

    TRACE("exiting");
}

static void
xfce_desktop_unrealize(GtkWidget *widget)
{
    XfceDesktop *desktop = XFCE_DESKTOP(widget);
    guint i;
    GdkWindow *groot;
    gchar property_name[128];
    
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    
    if(gtk_major_version > 2
       || (gtk_major_version == 2 && gtk_minor_version >= 13))
    {
        g_signal_handlers_disconnect_by_func(G_OBJECT(desktop->priv->gscreen),
                                             G_CALLBACK(xfce_desktop_monitors_changed),
                                             desktop);
    }
    
    if(GTK_WIDGET_MAPPED(widget))
        gtk_widget_unmap(widget);
    GTK_WIDGET_UNSET_FLAGS(widget, GTK_MAPPED);
    
    gtk_container_forall(GTK_CONTAINER(widget),
                         (GtkCallback)gtk_widget_unrealize,
                         NULL);
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(desktop->priv->gscreen),
            G_CALLBACK(screen_size_changed_cb), desktop);
    g_signal_handlers_disconnect_by_func(G_OBJECT(desktop->priv->gscreen),
            G_CALLBACK(screen_composited_changed_cb), desktop);

    gdk_error_trap_push();
    
    groot = gdk_screen_get_root_window(desktop->priv->gscreen);
    gdk_property_delete(groot, gdk_atom_intern("XFCE_DESKTOP_WINDOW", FALSE));
    gdk_property_delete(groot, gdk_atom_intern("NAUTILUS_DESKTOP_WINDOW_ID", FALSE));

#ifndef DISABLE_FOR_BUG7442
    gdk_property_delete(groot, gdk_atom_intern("_XROOTPMAP_ID", FALSE));
    gdk_property_delete(groot, gdk_atom_intern("ESETROOT_PMAP_ID", FALSE));
    gdk_window_set_back_pixmap(groot, NULL, FALSE);
#endif

    if(desktop->priv->backdrops) {
        for(i = 0; i < desktop->priv->nbackdrops; i++) {
            g_snprintf(property_name, 128, XFDESKTOP_IMAGE_FILE_FMT, i);
            gdk_property_delete(groot, gdk_atom_intern(property_name, FALSE));
            g_object_unref(G_OBJECT(desktop->priv->backdrops[i]));
        }
        g_free(desktop->priv->backdrops);
        desktop->priv->backdrops = NULL;
    }

    gdk_flush();
    gdk_error_trap_pop();

    g_object_unref(G_OBJECT(desktop->priv->bg_pixmap));
    desktop->priv->bg_pixmap = NULL;
    
    gtk_window_set_icon(GTK_WINDOW(widget), NULL);
    
    gtk_style_detach(widget->style);
    g_object_unref(G_OBJECT(widget->window));
    widget->window = NULL;
    
    gtk_selection_remove_all(widget);
    
    GTK_WIDGET_UNSET_FLAGS(widget, GTK_REALIZED);
}

static gboolean
xfce_desktop_button_press_event(GtkWidget *w,
                                GdkEventButton *evt)
{
    guint button = evt->button;
    guint state = evt->state;
    g_return_val_if_fail(XFCE_IS_DESKTOP(w), FALSE);

    if(evt->type == GDK_BUTTON_PRESS) {
        if(button == 3 || (button == 1 && (state & GDK_SHIFT_MASK))) {
#ifdef ENABLE_DESKTOP_ICONS
            if(XFCE_DESKTOP(w)->priv->icons_style != XFCE_DESKTOP_ICON_STYLE_NONE)
                return FALSE;
#endif
            xfce_desktop_popup_root_menu(XFCE_DESKTOP(w),
                                         button,
                                         evt->time);
        } else if(button == 2 || (button == 1 && (state & GDK_SHIFT_MASK)
                                  && (state & GDK_CONTROL_MASK)))
        {
            xfce_desktop_popup_secondary_root_menu(XFCE_DESKTOP(w),
                                                   button, evt->time);
            return TRUE;
        }
    }
    
    return FALSE;
}

static gboolean
xfce_desktop_popup_menu(GtkWidget *w)
{
    GdkEventButton *evt;
    guint button, etime;
    
    evt = (GdkEventButton *)gtk_get_current_event();
    if(evt && GDK_BUTTON_PRESS == evt->type) {
        button = evt->button;
        etime = evt->time;
    } else {
        button = 0;
        etime = gtk_get_current_event_time();
    }
    
    xfce_desktop_popup_root_menu(XFCE_DESKTOP(w), button, etime);
    
    return TRUE;
}

static gboolean
xfce_desktop_expose(GtkWidget *w,
                    GdkEventExpose *evt)
{
    GList *children, *l;
    
    /*TRACE("entering");*/
    
    if(evt->count != 0)
        return FALSE;
    
    gdk_window_clear_area(w->window, evt->area.x, evt->area.y,
                          evt->area.width, evt->area.height);
    
    children = gtk_container_get_children(GTK_CONTAINER(w));
    for(l = children; l; l = l->next) {
        gtk_container_propagate_expose(GTK_CONTAINER(w),
                                       GTK_WIDGET(l->data),
                                       evt);
    }
    g_list_free(children);
    
    return FALSE;
}

static gboolean
xfce_desktop_delete_event(GtkWidget *w,
                          GdkEventAny *evt)
{
    if(XFCE_DESKTOP(w)->priv->session_logout_func)
        XFCE_DESKTOP(w)->priv->session_logout_func();
    
    return TRUE;
}

static void
xfce_desktop_style_set(GtkWidget *w,
                       GtkStyle *old_style)
{
    XfceDesktop *desktop = XFCE_DESKTOP(w);
#ifdef ENABLE_DESKTOP_ICONS
    gdouble old_font_size;
#endif
    
    if(GDK_IS_WINDOW(desktop->priv->bg_pixmap))
        gdk_window_set_back_pixmap(w->window, desktop->priv->bg_pixmap, FALSE);
    gtk_widget_queue_draw(w);

#ifdef ENABLE_DESKTOP_ICONS
    old_font_size = desktop->priv->system_font_size;
    if(xfce_desktop_ensure_system_font_size(desktop) != old_font_size
       && desktop->priv->icon_view && !desktop->priv->icons_font_size_set)
    {
        xfdesktop_icon_view_set_font_size(XFDESKTOP_ICON_VIEW(desktop->priv->icon_view),
                                          desktop->priv->system_font_size);
    }
#endif
}

static void
xfce_desktop_connect_settings(XfceDesktop *desktop)
{
    XfconfChannel *channel = desktop->priv->channel;
    gchar buf[1024];

    xfce_desktop_freeze_updates(desktop);

    g_strlcpy(buf, desktop->priv->property_prefix, sizeof(buf));
    g_strlcat(buf, "xinerama-stretch", sizeof(buf));
    xfconf_g_property_bind(channel, buf, G_TYPE_BOOLEAN,
                           G_OBJECT(desktop), "xinerama-stretch");

#ifdef ENABLE_DESKTOP_ICONS
#define ICONS_PREFIX "/desktop-icons/"

    xfconf_g_property_bind(channel, ICONS_PREFIX "style",
                           XFCE_TYPE_DESKTOP_ICON_STYLE,
                           G_OBJECT(desktop), "icon-style");
    xfconf_g_property_bind(channel, ICONS_PREFIX "icon-size", G_TYPE_UINT,
                           G_OBJECT(desktop), "icon-size");
    xfconf_g_property_bind(channel, ICONS_PREFIX "font-size", G_TYPE_UINT,
                           G_OBJECT(desktop), "icon-font-size");
    xfconf_g_property_bind(channel, ICONS_PREFIX "use-custom-font-size",
                           G_TYPE_BOOLEAN,
                           G_OBJECT(desktop), "icon-font-size-set");
#undef ICONS_PREFIX
#endif

    xfce_desktop_thaw_updates(desktop);
}

static void
xfce_desktop_image_filename_changed(XfconfChannel *channel,
                                    const gchar *property,
                                    const GValue *value,
                                    gpointer user_data)
{
    XfceDesktop *desktop = user_data;
    gchar *p;
    const gchar *filename;
    gint monitor;
    XfceBackdrop *backdrop;

    p = strstr(property, "/monitor");
    if(!p)
        return;

    monitor = atoi(p + 8);
    if(monitor < 0 || monitor >= gdk_screen_get_n_monitors(desktop->priv->gscreen))
        return;

    if(desktop->priv->xinerama_stretch && monitor != 0)
        return;
    backdrop = desktop->priv->backdrops[monitor];

    if(!G_VALUE_HOLDS_STRING(value))
        filename = DEFAULT_BACKDROP;
    else
        filename = g_value_get_string(value);
    if(G_LIKELY(filename && *filename)) {
        if(xfdesktop_backdrop_list_is_valid(filename)) {
            gchar *backdrop_file;
            GError *error = NULL;
            
            backdrop_file = xfdesktop_backdrop_list_choose_random(filename,
                                                                  &error);
#if 0
            if(!backdrop_file && !xfdesktop_backdrop_list_is_valid(filename)) {
                gchar *primary = g_strdup_printf(_("Unable to load image from backdrop list file \"%s\""),
                                                 filename);
                xfce_message_dialog(GTK_WINDOW(desktop), _("Desktop Error"),
                                    GTK_STOCK_DIALOG_ERROR, primary,
                                    error->message,
                                    GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                                    NULL);
                g_error_free(error);
                g_free(primary);
            }
#endif

            xfce_backdrop_set_image_filename(backdrop, backdrop_file);
            g_free(backdrop_file);

            xfce_backdrop_set_list(backdrop, g_strdup(filename));
        } else {
            xfce_backdrop_set_image_filename(backdrop, filename);

            xfce_backdrop_set_list(backdrop, NULL);
        }
    }
}

static void
xfce_desktop_connect_backdrop_settings(XfceDesktop *desktop,
                                       XfceBackdrop *backdrop,
                                       guint monitor)
{
    XfconfChannel *channel = desktop->priv->channel;
    char buf[1024], buf1[1024];
    gint pp_len;
    GValue value = { 0, };

    g_snprintf(buf, sizeof(buf), "%smonitor%d/",
               desktop->priv->property_prefix, monitor);
    pp_len = strlen(buf);

    g_strlcat(buf, "color-style", sizeof(buf));
    xfconf_g_property_bind(channel, buf, XFCE_TYPE_BACKDROP_COLOR_STYLE,
                           G_OBJECT(backdrop), "color-style");

    buf[pp_len] = 0;
    g_strlcat(buf, "color1", sizeof(buf));
    xfconf_g_property_bind_gdkcolor(channel, buf,
                                    G_OBJECT(backdrop), "first-color");

    buf[pp_len] = 0;
    g_strlcat(buf, "color2", sizeof(buf));
    xfconf_g_property_bind_gdkcolor(channel, buf,
                                    G_OBJECT(backdrop), "second-color");

    buf[pp_len] = 0;
    g_strlcat(buf, "image-show", sizeof(buf));
    xfconf_g_property_bind(channel, buf, G_TYPE_BOOLEAN,
                           G_OBJECT(backdrop), "show-image");

    buf[pp_len] = 0;
    g_strlcat(buf, "image-style", sizeof(buf));
    xfconf_g_property_bind(channel, buf, XFCE_TYPE_BACKDROP_IMAGE_STYLE,
                           G_OBJECT(backdrop), "image-style");

    buf[pp_len] = 0;
    g_strlcat(buf, "brightness", sizeof(buf));
    xfconf_g_property_bind(channel, buf, G_TYPE_INT,
                           G_OBJECT(backdrop), "brightness");

    buf[pp_len] = 0;
    g_strlcat(buf, "saturation", sizeof(buf));
    xfconf_g_property_bind(channel, buf, G_TYPE_DOUBLE,
                           G_OBJECT(backdrop), "saturation");

    buf[pp_len] = 0;
    g_strlcat(buf, "backdrop-cycle-enable", sizeof(buf));
    xfconf_g_property_bind(channel, buf, G_TYPE_BOOLEAN,
                           G_OBJECT(backdrop), "backdrop-cycle-enable");

    buf[pp_len] = 0;
    g_strlcat(buf, "backdrop-cycle-timer", sizeof(buf));
    xfconf_g_property_bind(channel, buf, G_TYPE_UINT,
                           G_OBJECT(backdrop), "backdrop-cycle-timer");

    /* the image filename could be an image or a backdrop list, so we
     * can't just bind the property directly */
    buf[pp_len] = 0;
    g_strlcat(buf, "image-path", sizeof(buf));
    g_strlcpy(buf1, "property-changed::", sizeof(buf1));
    g_strlcat(buf1, buf, sizeof(buf1));
    g_signal_connect(G_OBJECT(channel), buf1,
                     G_CALLBACK(xfce_desktop_image_filename_changed), desktop);
    if(xfconf_channel_get_property(channel, buf, &value)) {
        xfce_desktop_image_filename_changed(channel, buf, &value, desktop);
        g_value_unset(&value);
    }
}



/* public api */

/**
 * xfce_desktop_new:
 * @gscreen: The current #GdkScreen.
 * @channel: An #XfconfChannel to use for settings.
 * @property_prefix: String prefix for per-screen properties.
 *
 * Creates a new #XfceDesktop for the specified #GdkScreen.  If @gscreen is
 * %NULL, the default screen will be used.
 *
 * Return value: A new #XfceDesktop.
 **/
GtkWidget *
xfce_desktop_new(GdkScreen *gscreen,
                 XfconfChannel *channel,
                 const gchar *property_prefix)
{
    XfceDesktop *desktop;
    
    g_return_val_if_fail(channel && property_prefix, NULL);

    desktop = g_object_new(XFCE_TYPE_DESKTOP, NULL);

    if(!gscreen)
        gscreen = gdk_display_get_default_screen(gdk_display_get_default());
    gtk_window_set_screen(GTK_WINDOW(desktop), gscreen);
    desktop->priv->gscreen = gscreen;
    
    desktop->priv->channel = g_object_ref(G_OBJECT(channel));
    desktop->priv->property_prefix = g_strdup(property_prefix);

    xfce_desktop_connect_settings(desktop);
    
    return GTK_WIDGET(desktop);
}

guint
xfce_desktop_get_n_monitors(XfceDesktop *desktop)
{
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), 0);
    
    return desktop->priv->nbackdrops;
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

void
xfce_desktop_set_xinerama_stretch(XfceDesktop *desktop,
                                  gboolean stretch)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    
    if(stretch == desktop->priv->xinerama_stretch)
        return;
    
    desktop->priv->xinerama_stretch = stretch;
    
    if(!desktop->priv->updates_frozen)
        xfce_desktop_monitors_changed(desktop->priv->gscreen, desktop);
}

gboolean
xfce_desktop_get_xinerama_stretch(XfceDesktop *desktop)
{
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), FALSE);
    return desktop->priv->xinerama_stretch;
}

void
xfce_desktop_set_icon_style(XfceDesktop *desktop,
                            XfceDesktopIconStyle style)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop)
                     && style <= XFCE_DESKTOP_ICON_STYLE_FILES);
    
#ifdef ENABLE_DESKTOP_ICONS
    if(style == desktop->priv->icons_style)
        return;
    
    if(desktop->priv->icon_view) {
        gtk_widget_destroy(desktop->priv->icon_view);
        desktop->priv->icon_view = NULL;
    }
    
    desktop->priv->icons_style = style;
    if(GTK_WIDGET_REALIZED(desktop))
        xfce_desktop_setup_icon_view(desktop);
#endif
}

XfceDesktopIconStyle
xfce_desktop_get_icon_style(XfceDesktop *desktop)
{
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), XFCE_DESKTOP_ICON_STYLE_NONE);
    
#ifdef ENABLE_DESKTOP_ICONS
    return desktop->priv->icons_style;
#else
    return XFCE_DESKTOP_ICON_STYLE_NONE;
#endif
}

void
xfce_desktop_set_icon_size(XfceDesktop *desktop,
                           guint icon_size)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    
#ifdef ENABLE_DESKTOP_ICONS
    if(icon_size == desktop->priv->icons_size)
        return;
    
    desktop->priv->icons_size = icon_size;
    
    if(desktop->priv->icon_view) {
        xfdesktop_icon_view_set_icon_size(XFDESKTOP_ICON_VIEW(desktop->priv->icon_view),
                                          icon_size);
    }
#endif
}

void
xfce_desktop_set_icon_font_size(XfceDesktop *desktop,
                                guint font_size_points)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    
#ifdef ENABLE_DESKTOP_ICONS
    if(font_size_points == desktop->priv->icons_font_size)
        return;
    
    desktop->priv->icons_font_size = font_size_points;
    
    if(desktop->priv->icons_font_size_set && desktop->priv->icon_view) {
        xfdesktop_icon_view_set_font_size(XFDESKTOP_ICON_VIEW(desktop->priv->icon_view),
                                          font_size_points);
    }
#endif
}

void
xfce_desktop_set_use_icon_font_size(XfceDesktop *desktop,
                                    gboolean use_icon_font_size)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    
#ifdef ENABLE_DESKTOP_ICONS
    if(use_icon_font_size == desktop->priv->icons_font_size_set)
        return;
    
    desktop->priv->icons_font_size_set = use_icon_font_size;
    
    if(desktop->priv->icon_view) {
        if(!use_icon_font_size) {
            xfce_desktop_ensure_system_font_size(desktop);
            xfdesktop_icon_view_set_font_size(XFDESKTOP_ICON_VIEW(desktop->priv->icon_view),
                                              desktop->priv->system_font_size);
        } else {
            xfdesktop_icon_view_set_font_size(XFDESKTOP_ICON_VIEW(desktop->priv->icon_view),
                                              desktop->priv->icons_font_size);
        }
    }
#endif
}

void
xfce_desktop_set_session_logout_func(XfceDesktop *desktop,
                                     SessionLogoutFunc logout_func)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    desktop->priv->session_logout_func = logout_func;
}

void
xfce_desktop_freeze_updates(XfceDesktop *desktop)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    desktop->priv->updates_frozen = TRUE;
}

void
xfce_desktop_thaw_updates(XfceDesktop *desktop)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    
    desktop->priv->updates_frozen = FALSE;
    if(GTK_WIDGET_REALIZED(desktop))
        xfce_desktop_monitors_changed(desktop->priv->gscreen, desktop);
}

XfceBackdrop *
xfce_desktop_peek_backdrop(XfceDesktop *desktop,
                           guint monitor)
{
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop)
                         && GTK_WIDGET_REALIZED(GTK_WIDGET(desktop))
                         && monitor < desktop->priv->nbackdrops, NULL);
    return desktop->priv->backdrops[monitor];
}

static gboolean
xfce_desktop_menu_destroy_idled(gpointer data)
{
    gtk_widget_destroy(GTK_WIDGET(data));
    return FALSE;
}

static void
xfce_desktop_do_menu_popup(XfceDesktop *desktop,
                           guint button,
                           guint activate_time,
                           guint populate_signal)
{
    GdkScreen *screen;
    GtkWidget *menu;
    GList *menu_children;
    
    TRACE("entering");
    
    if(gtk_widget_has_screen(GTK_WIDGET(desktop)))
        screen = gtk_widget_get_screen(GTK_WIDGET(desktop));
    else
        screen = gdk_display_get_default_screen(gdk_display_get_default());

    if(xfdesktop_popup_grab_available(gdk_screen_get_root_window(screen),
                                      activate_time))
    {
        menu = gtk_menu_new();
        gtk_menu_set_screen(GTK_MENU(menu), screen);
        g_signal_connect_swapped(G_OBJECT(menu), "deactivate",
                                 G_CALLBACK(g_idle_add),
                                 (gpointer)xfce_desktop_menu_destroy_idled);

        g_signal_emit(G_OBJECT(desktop), populate_signal, 0, menu);

        /* if nobody populated the menu, don't do anything */
        menu_children = gtk_container_get_children(GTK_CONTAINER(menu));
        if(!menu_children) {
            gtk_widget_destroy(menu);
            return;
        }

        g_list_free(menu_children);

        gtk_menu_attach_to_widget(GTK_MENU(menu), GTK_WIDGET(desktop), NULL);

        /* bug #3652: for some reason passing the correct button here breaks
         * on some systems but not others.  always pass 0 for now. */
        gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0,
                       activate_time);
    } else
        g_critical("Unable to get keyboard/mouse grab. Unable to pop up menu");
}

void
xfce_desktop_popup_root_menu(XfceDesktop *desktop,
                             guint button,
                             guint activate_time)
{
    xfce_desktop_do_menu_popup(desktop, button, activate_time,
                               signals[SIG_POPULATE_ROOT_MENU]);
}

void
xfce_desktop_popup_secondary_root_menu(XfceDesktop *desktop,
                                       guint button,
                                       guint activate_time)
{
    xfce_desktop_do_menu_popup(desktop, button, activate_time,
                               signals[SIG_POPULATE_SECONDARY_ROOT_MENU]);
}

void
xfce_desktop_refresh(XfceDesktop *desktop)
{
    gchar buf[256];
    guint i, max;

    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    if(!GTK_WIDGET_REALIZED(desktop))
        return;

    /* reload image */
    if(desktop->priv->xinerama_stretch)
        max = 1;
    else
        max = desktop->priv->nbackdrops;
    for(i = 0; i < max; ++i) {
        GValue val = { 0, };

        g_snprintf(buf, sizeof(buf), "%smonitor%d/image-path",
                   desktop->priv->property_prefix, i);
        xfconf_channel_get_property(desktop->priv->channel, buf, &val);

        xfce_desktop_image_filename_changed(desktop->priv->channel, buf,
                                            &val, desktop);

        if(G_VALUE_TYPE(&val))
            g_value_unset(&val);
    }

#ifdef ENABLE_DESKTOP_ICONS
    /* reload icon view */
    if(desktop->priv->icon_view) {
        gtk_widget_destroy(desktop->priv->icon_view);
        desktop->priv->icon_view = NULL;
    }
    xfce_desktop_setup_icon_view(desktop);
#endif
}

void xfce_desktop_arrange_icons(XfceDesktop *desktop)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

#ifdef ENABLE_DESKTOP_ICONS
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(desktop->priv->icon_view));

    xfdesktop_icon_view_sort_icons(XFDESKTOP_ICON_VIEW(desktop->priv->icon_view));
#endif
}
