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
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
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
#include <libwnck/libwnck.h>

#include "xfdesktop-common.h"
#include "xfce-desktop.h"
#include "xfce-desktop-enum-types.h"
#include "xfce-workspace.h"

/* disable setting the x background for bug 7442 */
//#define DISABLE_FOR_BUG7442

struct _XfceDesktopPriv
{
    GdkScreen *gscreen;
    WnckScreen *wnck_screen;
    gboolean updates_frozen;

    XfconfChannel *channel;
    gchar *property_prefix;
    
    GdkPixmap *bg_pixmap;
    
    gint nworkspaces;
    XfceWorkspace **workspaces;
    gint current_workspace;

    gboolean single_workspace_mode;
    gint single_workspace_num;

    SessionLogoutFunc session_logout_func;

    guint32 grab_time;

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
#ifdef ENABLE_DESKTOP_ICONS
    PROP_ICON_STYLE,
    PROP_ICON_SIZE,
    PROP_ICON_FONT_SIZE,
    PROP_ICON_FONT_SIZE_SET,
#endif
    PROP_SINGLE_WORKSPACE_MODE,
    PROP_SINGLE_WORKSPACE_NUMBER,
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
static gboolean xfce_desktop_button_release_event(GtkWidget *widget,
                                                  GdkEventButton *evt);
static gboolean xfce_desktop_popup_menu(GtkWidget *widget);

static gboolean xfce_desktop_expose(GtkWidget *w,
                                    GdkEventExpose *evt);
static gboolean xfce_desktop_delete_event(GtkWidget *w,
                                          GdkEventAny *evt);
static void xfce_desktop_style_set(GtkWidget *w,
                                   GtkStyle *old_style);

static void xfce_desktop_set_single_workspace_mode(XfceDesktop *desktop,
                                                   gboolean single_workspace);
static void xfce_desktop_set_single_workspace_number(XfceDesktop *desktop,
                                                     gint workspace_num);

static gboolean xfce_desktop_get_single_workspace_mode(XfceDesktop *desktop);
static gint xfce_desktop_get_current_workspace(XfceDesktop *desktop);

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
        /* If the user set a custom font size, use it. Otherwise use the system
         * font size */
        xfdesktop_icon_view_set_font_size(XFDESKTOP_ICON_VIEW(desktop->priv->icon_view),
                                          (!desktop->priv->icons_font_size_set)
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

static GdkPixmap *
create_bg_pixmap(GdkScreen *gscreen, gpointer user_data)
{
    XfceDesktop *desktop = user_data;
    gint w, h;

    TRACE("entering");

    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), NULL);

    /* If the workspaces haven't been created yet there's no need to do the
     * background pixmap */
    if(desktop->priv->workspaces == NULL) {
        DBG("exiting, desktop->priv->workspaces == NULL");
        return NULL;
    }

    TRACE("really entering");

    w = gdk_screen_get_width(gscreen);
    h = gdk_screen_get_height(gscreen);
    gtk_widget_set_size_request(GTK_WIDGET(desktop), w, h);
    gtk_window_resize(GTK_WINDOW(desktop), w, h);

    if(desktop->priv->bg_pixmap)
        g_object_unref(G_OBJECT(desktop->priv->bg_pixmap));
    desktop->priv->bg_pixmap = gdk_pixmap_new(GDK_DRAWABLE(gtk_widget_get_window(GTK_WIDGET(desktop))),
                                              w, h, -1);

    if(!GDK_IS_PIXMAP(desktop->priv->bg_pixmap))
        return NULL;

    set_real_root_window_pixmap(desktop->priv->gscreen,
                                desktop->priv->bg_pixmap);
    gdk_window_set_back_pixmap(gtk_widget_get_window(GTK_WIDGET(desktop)),
                               desktop->priv->bg_pixmap, FALSE);

    return desktop->priv->bg_pixmap;
}

static void
backdrop_changed_cb(XfceBackdrop *backdrop, gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);
    GdkPixmap *pmap = desktop->priv->bg_pixmap;
    GdkScreen *gscreen = desktop->priv->gscreen;
    GdkRectangle rect;
    GdkRegion *clip_region = NULL;
    gint i, monitor = -1, current_workspace;
#ifdef G_ENABLE_DEBUG
    gchar *monitor_name = NULL;
#endif

    TRACE("entering");
    
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    
    if(desktop->priv->updates_frozen || !gtk_widget_get_realized(GTK_WIDGET(desktop)))
        return;

    TRACE("really entering");

    current_workspace = xfce_desktop_get_current_workspace(desktop);

    /* Find out which monitor the backdrop is on */
    for(i = 0; i < xfce_desktop_get_n_monitors(desktop); i++) {
        if(backdrop == xfce_workspace_get_backdrop(desktop->priv->workspaces[current_workspace], i)) {
            monitor = i;
            break;
        }
    }
    if(monitor == -1)
        return;

#ifdef G_ENABLE_DEBUG
    monitor_name = gdk_screen_get_monitor_plug_name(gscreen, monitor);

    DBG("backdrop changed for workspace %d, monitor %d (%s)", current_workspace, monitor, monitor_name);

    g_free(monitor_name);
#endif

    if(xfce_desktop_get_n_monitors(desktop) > 1
       && xfce_workspace_get_xinerama_stretch(desktop->priv->workspaces[current_workspace])) {
        /* Spanning screens */
        GdkRectangle monitor_rect;

        gdk_screen_get_monitor_geometry(gscreen, 0, &rect);
        /* Get the lowest x and y value for all the monitors in
         * case none of them start at 0,0 for whatever reason.
         */
        for(i = 1; i < xfce_desktop_get_n_monitors(desktop); i++) {
            gdk_screen_get_monitor_geometry(gscreen, i, &monitor_rect);

            if(monitor_rect.x < rect.x)
                rect.x = monitor_rect.x;
            if(monitor_rect.y < rect.y)
                rect.y = monitor_rect.y;
        }

        rect.width = gdk_screen_get_width(gscreen);
        rect.height = gdk_screen_get_height(gscreen);
        DBG("xinerama_stretch x %d, y %d, width %d, height %d",
            rect.x, rect.y, rect.width, rect.height);
    } else {
        gdk_screen_get_monitor_geometry(gscreen, monitor, &rect);
        DBG("monitor x %d, y %d, width %d, height %d",
            rect.x, rect.y, rect.width, rect.height);
    }

    xfce_backdrop_set_size(backdrop, rect.width, rect.height);

    if(monitor > 0
       && !xfce_workspace_get_xinerama_stretch(desktop->priv->workspaces[current_workspace])) {
        clip_region = gdk_region_rectangle(&rect);

        DBG("clip_region: x: %d, y: %d, w: %d, h: %d",
            rect.x, rect.y, rect.width, rect.height);

        /* If we are not monitor 0 on a multi-monitor setup we need to subtract
         * all the previous monitor regions so we don't draw over them. This
         * should prevent the overlap and double backdrop drawing bugs.
         */
        for(i = 0; i < monitor; i++) {
            GdkRectangle previous_monitor;
            GdkRegion *previous_region;
            gdk_screen_get_monitor_geometry(gscreen, i, &previous_monitor);

            DBG("previous_monitor: x: %d, y: %d, w: %d, h: %d",
                previous_monitor.x, previous_monitor.y,
                previous_monitor.width, previous_monitor.height);

            previous_region = gdk_region_rectangle(&previous_monitor);

            gdk_region_subtract(clip_region, previous_region);

            gdk_region_destroy(previous_region);
        }
    }

    if(clip_region != NULL) {
        /* Update the area to redraw to limit the icons/area painted */
        gdk_region_get_clipbox(clip_region, &rect);
        DBG("area to update: x: %d, y: %d, w: %d, h: %d",
            rect.x, rect.y, rect.width, rect.height);
    }

    if(rect.width != 0 && rect.height != 0) {
        /* get the composited backdrop pixmap */
        GdkPixbuf *pix = xfce_backdrop_get_pixbuf(backdrop);
        cairo_t *cr;

        /* create the backdrop if needed */
        if(!pix) {
            xfce_backdrop_generate_async(backdrop);

            if(clip_region != NULL)
                gdk_region_destroy(clip_region);

            return;
        }

        /* Create the background pixmap if it isn't already */
        if(!GDK_IS_PIXMAP(pmap)) {
            pmap = create_bg_pixmap(gscreen, desktop);

            if(!GDK_IS_PIXMAP(pmap)) {
                g_object_unref(pix);

                if(clip_region != NULL)
                    gdk_region_destroy(clip_region);

                return;
            }
        }

        cr = gdk_cairo_create(GDK_DRAWABLE(pmap));
        gdk_cairo_set_source_pixbuf(cr, pix, rect.x, rect.y);

        /* clip the area so we don't draw over a previous wallpaper */
        if(clip_region != NULL) {
            gdk_cairo_region(cr, clip_region);
            cairo_clip(cr);
        }

        cairo_paint(cr);

        /* tell gtk to redraw the repainted area */
        gtk_widget_queue_draw_area(GTK_WIDGET(desktop), rect.x, rect.y,
                                   rect.width, rect.height);

        set_imgfile_root_property(desktop,
                                  xfce_backdrop_get_image_filename(backdrop),
                                  monitor);

        /* do this again so apps watching the root win notice the update */
        set_real_root_window_pixmap(gscreen, pmap);

        g_object_unref(G_OBJECT(pix));
        cairo_destroy(cr);
        gtk_widget_show(GTK_WIDGET(desktop));
    }

    if(clip_region != NULL)
        gdk_region_destroy(clip_region);
}

static void
screen_size_changed_cb(GdkScreen *gscreen, gpointer user_data)
{
    XfceDesktop *desktop = user_data;
    gint current_workspace;

    TRACE("entering");

    current_workspace = xfce_desktop_get_current_workspace(desktop);

    if(desktop->priv->nworkspaces <= current_workspace)
        return;

    if(current_workspace < 0)
        return;

    /* special case for 1 backdrop to handle xinerama stretching */
    if(xfce_workspace_get_xinerama_stretch(desktop->priv->workspaces[current_workspace])) {
       backdrop_changed_cb(xfce_workspace_get_backdrop(desktop->priv->workspaces[current_workspace], 0), desktop);
    } else {
        gint i;

        for(i = 0; i < xfce_desktop_get_n_monitors(desktop); i++) {
            XfceBackdrop *current_backdrop;
            current_backdrop = xfce_workspace_get_backdrop(desktop->priv->workspaces[current_workspace], i);
            backdrop_changed_cb(current_backdrop, desktop);
        }
    }
}

static void
screen_composited_changed_cb(GdkScreen *gscreen,
                             gpointer user_data)
{
    TRACE("entering");
    /* fake a screen size changed, so the background is properly set */
    screen_size_changed_cb(gscreen, user_data);
}

static void
xfce_desktop_monitors_changed(GdkScreen *gscreen,
                              gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);
    gint i;

    TRACE("entering");

    /* Update the workspaces */
    for(i = 0; i < desktop->priv->nworkspaces; i++) {
        xfce_workspace_monitors_changed(desktop->priv->workspaces[i],
                                        gscreen);
    }

    /* fake a screen size changed, so the background is properly set */
    screen_size_changed_cb(gscreen, user_data);
}

static void
workspace_backdrop_changed_cb(XfceWorkspace *workspace,
                              XfceBackdrop  *backdrop,
                              gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);

    TRACE("entering");

    g_return_if_fail(XFCE_IS_WORKSPACE(workspace) && XFCE_IS_BACKDROP(backdrop));

    if(xfce_desktop_get_current_workspace(desktop) == xfce_workspace_get_workspace_num(workspace))
        backdrop_changed_cb(backdrop, user_data);
}

static void
workspace_changed_cb(WnckScreen *wnck_screen,
                     WnckWorkspace *previously_active_space,
                     gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);
    gint current_workspace, new_workspace, i;
    XfceBackdrop *current_backdrop, *new_backdrop;

    TRACE("entering");

    /* Ignore workspace changes in single workspace mode so long as we
     * already have the bg_pixmap loaded */
    if(xfce_desktop_get_single_workspace_mode(desktop) && desktop->priv->bg_pixmap)
        return;

    current_workspace = desktop->priv->current_workspace;
    new_workspace = xfce_desktop_get_current_workspace(desktop);

    if(new_workspace < 0 || new_workspace >= desktop->priv->nworkspaces)
        return;

    desktop->priv->current_workspace = new_workspace;

    DBG("current_workspace %d, new_workspace %d",
        current_workspace, new_workspace);

    for(i = 0; i < xfce_desktop_get_n_monitors(desktop); i++) {
        /* We want to compare the current workspace backdrop with the new one
         * and see if we can avoid changing them if they are the same image/style */
        if(current_workspace < desktop->priv->nworkspaces && current_workspace >= 0) {
            current_backdrop = xfce_workspace_get_backdrop(desktop->priv->workspaces[current_workspace], i);
            new_backdrop = xfce_workspace_get_backdrop(desktop->priv->workspaces[new_workspace], i);

            if(!xfce_backdrop_compare_backdrops(current_backdrop, new_backdrop) || !desktop->priv->bg_pixmap) {
                /* only update monitors that require it */
                backdrop_changed_cb(new_backdrop, user_data);
            }
        } else {
            /* If current_workspace was removed or never existed, get the new
             * backdrop and apply it */
            new_backdrop = xfce_workspace_get_backdrop(desktop->priv->workspaces[new_workspace], i);
            backdrop_changed_cb(new_backdrop, user_data);
        }

        /* When we're spanning screens we only care about the first monitor */
        if(xfce_workspace_get_xinerama_stretch(desktop->priv->workspaces[new_workspace]))
            break;
    }
}

static void
workspace_created_cb(WnckScreen *wnck_screen,
                     WnckWorkspace *new_workspace,
                     gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);
    gint nlast_workspace;
    TRACE("entering");

    nlast_workspace = desktop->priv->nworkspaces;

    /* add one more workspace */
    desktop->priv->nworkspaces = nlast_workspace + 1;

    /* allocate size for it */
    desktop->priv->workspaces = g_realloc(desktop->priv->workspaces,
                                          desktop->priv->nworkspaces * sizeof(XfceWorkspace *));

    /* create the new workspace and set it up */
    desktop->priv->workspaces[nlast_workspace] = xfce_workspace_new(desktop->priv->gscreen,
                                                                    desktop->priv->channel,
                                                                    desktop->priv->property_prefix,
                                                                    nlast_workspace);

    /* Tell workspace whether to cache pixbufs */
    xfce_workspace_set_cache_pixbufs(desktop->priv->workspaces[nlast_workspace],
                                     !desktop->priv->single_workspace_mode);

    xfce_workspace_monitors_changed(desktop->priv->workspaces[nlast_workspace],
                                    desktop->priv->gscreen);

    g_signal_connect(desktop->priv->workspaces[nlast_workspace],
                     "workspace-backdrop-changed",
                     G_CALLBACK(workspace_backdrop_changed_cb), desktop);
}

static void
workspace_destroyed_cb(WnckScreen *wnck_screen,
                     WnckWorkspace *old_workspace,
                     gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);
    gint nlast_workspace;
    TRACE("entering");

    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    g_return_if_fail(desktop->priv->nworkspaces - 1 >= 0);
    g_return_if_fail(XFCE_IS_WORKSPACE(desktop->priv->workspaces[desktop->priv->nworkspaces-1]));

    nlast_workspace = desktop->priv->nworkspaces - 1;

    g_signal_handlers_disconnect_by_func(desktop->priv->workspaces[nlast_workspace],
                                         G_CALLBACK(workspace_backdrop_changed_cb),
                                         desktop);

    g_object_unref(desktop->priv->workspaces[nlast_workspace]);

    /* Remove one workspace */
    desktop->priv->nworkspaces = nlast_workspace;

    /* deallocate it */
    desktop->priv->workspaces = g_realloc(desktop->priv->workspaces,
                                          desktop->priv->nworkspaces * sizeof(XfceWorkspace *));

    /* Make sure we stay within bounds now that we removed a workspace */
    if(desktop->priv->current_workspace > desktop->priv->nworkspaces)
        desktop->priv->current_workspace = desktop->priv->nworkspaces;
}

static void
screen_set_selection(XfceDesktop *desktop)
{
    Window xwin;
    gint xscreen;
    gchar selection_name[100];
    Atom selection_atom, manager_atom;
    
    xwin = GDK_WINDOW_XID(gtk_widget_get_window(GTK_WIDGET(desktop)));
    xscreen = gdk_screen_get_number(desktop->priv->gscreen);
    
    g_snprintf(selection_name, 100, XFDESKTOP_SELECTION_FMT, xscreen);
    selection_atom = XInternAtom(gdk_x11_get_default_xdisplay(), selection_name, False);
    manager_atom = XInternAtom(gdk_x11_get_default_xdisplay(), "MANAGER", False);

    /* the previous check in src/main.c occurs too early, so workaround by
     * adding this one. */
   if(XGetSelectionOwner(gdk_x11_get_default_xdisplay(), selection_atom) != None) {
       g_warning("%s: already running, quitting.", PACKAGE);
       exit(0);
   }

    XSelectInput(gdk_x11_get_default_xdisplay(), xwin, PropertyChangeMask | ButtonPressMask);
    XSetSelectionOwner(gdk_x11_get_default_xdisplay(), selection_atom, xwin, GDK_CURRENT_TIME);

    /* Check to see if we managed to claim the selection. If not,
     * we treat it as if we got it then immediately lost it */
    if(XGetSelectionOwner(gdk_x11_get_default_xdisplay(), selection_atom) == xwin) {
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

        XSendEvent(gdk_x11_get_default_xdisplay(), xroot, False, StructureNotifyMask, (XEvent *)&xev);
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
    widget_class->button_release_event = xfce_desktop_button_release_event;
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
#endif /* ENABLE_FILE_ICONS */
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
                                                      0, 144, 12,
                                                      XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_ICON_FONT_SIZE_SET,
                                    g_param_spec_boolean("icon-font-size-set",
                                                         "icon font size set",
                                                         "icon font size set",
                                                         FALSE,
                                                         XFDESKTOP_PARAM_FLAGS));

#endif /* ENABLE_DESKTOP_ICONS */

    g_object_class_install_property(gobject_class, PROP_SINGLE_WORKSPACE_MODE,
                                    g_param_spec_boolean("single-workspace-mode",
                                                         "single-workspace-mode",
                                                         "single-workspace-mode",
                                                         TRUE,
                                                         XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_SINGLE_WORKSPACE_NUMBER,
                                    g_param_spec_int("single-workspace-number",
                                                     "single-workspace-number",
                                                     "single-workspace-number",
                                                     0, G_MAXINT16, 0,
                                                     XFDESKTOP_PARAM_FLAGS));

#undef XFDESKTOP_PARAM_FLAGS
}

static void
xfce_desktop_init(XfceDesktop *desktop)
{
    desktop->priv = G_TYPE_INSTANCE_GET_PRIVATE(desktop, XFCE_TYPE_DESKTOP,
                                                XfceDesktopPriv);
    
    gtk_window_set_type_hint(GTK_WINDOW(desktop), GDK_WINDOW_TYPE_HINT_DESKTOP);
    /* Accept focus is needed for the menu pop up either by the menu key on
     * the keyboard or Shift+F10. */
    gtk_window_set_accept_focus(GTK_WINDOW(desktop), TRUE);
    /* Can focus is needed for the gtk_grab_add/remove commands */
    gtk_widget_set_can_focus(GTK_WIDGET(desktop), TRUE);
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
        case PROP_SINGLE_WORKSPACE_MODE:
            xfce_desktop_set_single_workspace_mode(desktop,
                                                   g_value_get_boolean(value));
            break;

        case PROP_SINGLE_WORKSPACE_NUMBER:
            xfce_desktop_set_single_workspace_number(desktop,
                                                     g_value_get_int(value));
            break;

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
        case PROP_SINGLE_WORKSPACE_MODE:
            g_value_set_boolean(value, desktop->priv->single_workspace_mode);
            break;

        case PROP_SINGLE_WORKSPACE_NUMBER:
            g_value_set_int(value, desktop->priv->single_workspace_num);
            break;

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
    WnckScreen *wnck_screen;
    
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
    gdk_property_change(gtk_widget_get_window(GTK_WIDGET(desktop)),
            gdk_atom_intern("_NET_WM_WINDOW_TYPE", FALSE),
            gdk_atom_intern("ATOM", FALSE), 32,
            GDK_PROP_MODE_REPLACE, (guchar *)&atom, 1);

    xid = GDK_WINDOW_XID(gtk_widget_get_window(GTK_WIDGET(desktop)));
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

    wnck_screen = wnck_screen_get(gdk_screen_get_number(desktop->priv->gscreen));
    desktop->priv->wnck_screen = wnck_screen;

    /* Watch for single workspace setting changes */
    xfconf_g_property_bind(desktop->priv->channel,
                           SINGLE_WORKSPACE_MODE, G_TYPE_BOOLEAN,
                           G_OBJECT(desktop), "single-workspace-mode");
    xfconf_g_property_bind(desktop->priv->channel,
                           SINGLE_WORKSPACE_NUMBER, G_TYPE_INT,
                           G_OBJECT(desktop), "single-workspace-number");

    /* watch for workspace changes */
    g_signal_connect(desktop->priv->wnck_screen, "active-workspace-changed",
                     G_CALLBACK(workspace_changed_cb), desktop);
    g_signal_connect(desktop->priv->wnck_screen, "workspace-created",
                     G_CALLBACK(workspace_created_cb), desktop);
    g_signal_connect(desktop->priv->wnck_screen, "workspace-destroyed",
                     G_CALLBACK(workspace_destroyed_cb), desktop);

    /* watch for screen changes */
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
    gint i;
    GdkWindow *groot;
    gchar property_name[128];
    
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    /* disconnect all the xfconf settings to this desktop */
    xfconf_g_property_unbind_all(G_OBJECT(desktop));

    g_signal_handlers_disconnect_by_func(G_OBJECT(desktop->priv->gscreen),
                                         G_CALLBACK(xfce_desktop_monitors_changed),
                                         desktop);
    
    if(gtk_widget_get_mapped(widget))
        gtk_widget_unmap(widget);
    gtk_widget_set_mapped(widget, FALSE);
    
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

    if(desktop->priv->workspaces) {
        for(i = 0; i < desktop->priv->nworkspaces; i++) {
            g_snprintf(property_name, 128, XFDESKTOP_IMAGE_FILE_FMT, i);
            gdk_property_delete(groot, gdk_atom_intern(property_name, FALSE));
            g_object_unref(G_OBJECT(desktop->priv->workspaces[i]));
        }
        g_free(desktop->priv->workspaces);
        desktop->priv->workspaces = NULL;
    }

    gdk_flush();
    gdk_error_trap_pop();

    if(desktop->priv->bg_pixmap) {
        g_object_unref(G_OBJECT(desktop->priv->bg_pixmap));
        desktop->priv->bg_pixmap = NULL;
    }
    
    gtk_window_set_icon(GTK_WINDOW(widget), NULL);

    gtk_style_detach(gtk_widget_get_style(widget));
    g_object_unref(G_OBJECT(gtk_widget_get_window(widget)));
    gtk_widget_set_window(widget, NULL);

    gtk_selection_remove_all(widget);
    
    gtk_widget_set_realized(widget, FALSE);
}

static gboolean
xfce_desktop_button_press_event(GtkWidget *w,
                                GdkEventButton *evt)
{
    guint button = evt->button;
    guint state = evt->state;
    XfceDesktop *desktop = XFCE_DESKTOP(w);

    TRACE("entering");

    g_return_val_if_fail(XFCE_IS_DESKTOP(w), FALSE);

    if(evt->type == GDK_BUTTON_PRESS) {
        if(button == 3 || (button == 1 && (state & GDK_SHIFT_MASK))) {
#ifdef ENABLE_DESKTOP_ICONS
            /* Let the icon view handle these menu pop ups */
            if(desktop->priv->icons_style != XFCE_DESKTOP_ICON_STYLE_NONE)
                return FALSE;
#endif
            /* no icons on the desktop, grab the focus and pop up the menu */
            if(!gtk_widget_has_grab(w))
                gtk_grab_add(w);

                xfce_desktop_popup_root_menu(desktop, button, evt->time);
                return TRUE;
        } else if(button == 2 || (button == 1 && (state & GDK_SHIFT_MASK)
                                  && (state & GDK_CONTROL_MASK)))
        {
            /* always grab the focus and pop up the menu */
            if(!gtk_widget_has_grab(w))
                gtk_grab_add(w);

            xfce_desktop_popup_secondary_root_menu(desktop, button, evt->time);
            return TRUE;
        }
    }
    
    return FALSE;
}

static gboolean
xfce_desktop_button_release_event(GtkWidget *w,
                                  GdkEventButton *evt)
{
    TRACE("entering");

    gtk_grab_remove(w);

    return FALSE;
}

/* This function gets called when the user presses the menu key on the keyboard.
 * Or Shift+F10 or whatever key binding the user has chosen. */
static gboolean
xfce_desktop_popup_menu(GtkWidget *w)
{
    GdkEventButton *evt;
    guint button, etime;

    TRACE("entering");

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
    
    gdk_window_clear_area(gtk_widget_get_window(w), evt->area.x, evt->area.y,
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
        gdk_window_set_back_pixmap(gtk_widget_get_window(w), desktop->priv->bg_pixmap, FALSE);
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
#ifdef ENABLE_DESKTOP_ICONS
#define ICONS_PREFIX "/desktop-icons/"
    XfconfChannel *channel = desktop->priv->channel;

    xfce_desktop_freeze_updates(desktop);

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

    xfce_desktop_thaw_updates(desktop);
#undef ICONS_PREFIX
#endif
}

static gboolean
xfce_desktop_get_single_workspace_mode(XfceDesktop *desktop)
{
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), TRUE);

    return desktop->priv->single_workspace_mode;
}

static gint
xfce_desktop_get_current_workspace(XfceDesktop *desktop)
{
    WnckWorkspace *wnck_workspace;
    gint workspace_num, current_workspace;

    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), -1);

    wnck_workspace = wnck_screen_get_active_workspace(desktop->priv->wnck_screen);

    if(wnck_workspace != NULL) {
        workspace_num = wnck_workspace_get_number(wnck_workspace);
    } else {
        workspace_num = desktop->priv->nworkspaces;
    }

    /* If we're in single_workspace mode we need to return the workspace that
     * it was set to, if possible, otherwise return the current workspace */
    if(xfce_desktop_get_single_workspace_mode(desktop) &&
       desktop->priv->single_workspace_num < desktop->priv->nworkspaces) {
        current_workspace = desktop->priv->single_workspace_num;
    } else {
        current_workspace = workspace_num;
    }

    DBG("workspace_num %d, single_workspace_num %d, current_workspace %d, max workspaces %d",
        workspace_num, desktop->priv->single_workspace_num, current_workspace,
        desktop->priv->nworkspaces);

    return current_workspace;
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

gint
xfce_desktop_get_n_monitors(XfceDesktop *desktop)
{
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), 0);
    
    return gdk_screen_get_n_monitors(desktop->priv->gscreen);
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
    if(gtk_widget_get_realized(GTK_WIDGET(desktop)))
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

static void
xfce_desktop_set_single_workspace_mode(XfceDesktop *desktop,
                                       gboolean single_workspace)
{
    gint i;

    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    if(single_workspace == desktop->priv->single_workspace_mode)
        return;

    desktop->priv->single_workspace_mode = single_workspace;

    DBG("single_workspace_mode now %s", single_workspace ? "TRUE" : "FALSE");

    /* update the workspaces & backdrops if they should cache their pixbuf */
    if(desktop->priv->workspaces) {
        for(i = 0; i < desktop->priv->nworkspaces; i++) {
            /* set cache to TRUE when single_workspace is FALSE */
            xfce_workspace_set_cache_pixbufs(desktop->priv->workspaces[i], !single_workspace);
        }
    }

    /* If the desktop has been realized then fake a screen size change to
     * update the backdrop. There's no reason to if there's no desktop yet */
    if(gtk_widget_get_realized(GTK_WIDGET(desktop)))
        screen_size_changed_cb(desktop->priv->gscreen, desktop);
}

static void
xfce_desktop_set_single_workspace_number(XfceDesktop *desktop,
                                         gint workspace_num)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    if(workspace_num == desktop->priv->single_workspace_num)
        return;

    DBG("single_workspace_num now %d", workspace_num);

    desktop->priv->single_workspace_num = workspace_num;

    if(xfce_desktop_get_single_workspace_mode(desktop)) {
        /* Fake a screen size changed to update the backdrop */
        screen_size_changed_cb(desktop->priv->gscreen, desktop);
    }
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
    if(gtk_widget_get_realized(GTK_WIDGET(desktop)))
        xfce_desktop_monitors_changed(desktop->priv->gscreen, desktop);
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

    /* Per gtk_menu_popup's documentation "for conflict-resolve initiation of
     * concurrent requests for mouse/keyboard grab requests." */
    if(activate_time == 0)
        activate_time = gtk_get_current_event_time();

    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, button, activate_time);
}

void
xfce_desktop_popup_root_menu(XfceDesktop *desktop,
                             guint button,
                             guint activate_time)
{
    TRACE("entering");

    /* If it's launched by xfdesktop --menu we won't have an event time.
     * Grab the keyboard focus */
    if(activate_time == 0)
        activate_time = xfdesktop_popup_keyboard_grab_available(gtk_widget_get_window(GTK_WIDGET(desktop)));

    xfce_desktop_do_menu_popup(desktop, button, activate_time,
                               signals[SIG_POPULATE_ROOT_MENU]);

}

void
xfce_desktop_popup_secondary_root_menu(XfceDesktop *desktop,
                                       guint button,
                                       guint activate_time)
{
    /* If it's launched by xfdesktop --windowlist we won't have an event time.
     * Grab the keyboard focus */
    if(activate_time == 0)
        activate_time = xfdesktop_popup_keyboard_grab_available(gtk_widget_get_window(GTK_WIDGET(desktop)));

    xfce_desktop_do_menu_popup(desktop, button, activate_time,
                               signals[SIG_POPULATE_SECONDARY_ROOT_MENU]);
}
void
xfce_desktop_refresh(XfceDesktop *desktop)
{
    gint i, current_workspace;

    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    if(!gtk_widget_get_realized(GTK_WIDGET(desktop)))
        return;

    current_workspace = xfce_desktop_get_current_workspace(desktop);

    /* reload backgrounds */
    for(i = 0; i < xfce_desktop_get_n_monitors(desktop); i++) {
        XfceBackdrop *backdrop;

        backdrop = xfce_workspace_get_backdrop(desktop->priv->workspaces[current_workspace], i);

        backdrop_changed_cb(backdrop, desktop);
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
