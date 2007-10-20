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
#include <libxfcegui4/libxfcegui4.h>

#include "xfdesktop-common.h"
#include "xfce-desktop.h"

struct _XfceDesktopPriv
{
    GdkScreen *gscreen;
    gboolean updates_frozen;
    
    GdkPixmap *bg_pixmap;
    
    guint nbackdrops;
    XfceBackdrop **backdrops;
    
    gboolean xinerama_stretch;
    
    SessionLogoutFunc session_logout_func;
    
#ifdef ENABLE_DESKTOP_ICONS
    XfceDesktopIconStyle icons_style;
    gboolean icons_use_system_font;
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


static void xfce_desktop_class_init(XfceDesktopClass *klass);
static void xfce_desktop_init(XfceDesktop *desktop);
static void xfce_desktop_finalize(GObject *object);
static void xfce_desktop_realize(GtkWidget *widget);
static void xfce_desktop_unrealize(GtkWidget *widget);
static gboolean xfce_desktop_button_press_event(GtkWidget *widget,
                                                GdkEventButton *evt);
static gboolean xfce_desktop_popup_menu(GtkWidget *widget);

static gboolean xfce_desktop_expose(GtkWidget *w,
                                    GdkEventExpose *evt);
static gboolean xfce_desktop_delete_event(GtkWidget *w,
                                          GdkEventAny *evt);
static void style_set_cb(GtkWidget *w,
                         GtkStyle *old_style,
                         gpointer user_data);

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
                ThunarVfsPath *path;
                gchar *desktop_path = xfce_get_homefile("Desktop",
                                                        NULL);
                
                path = thunar_vfs_path_new(desktop_path, NULL);
                if(path) {
                    XfceRc *rcfile;
                    /* defaults */
                    gboolean show_removable = TRUE, show_filesystem = TRUE;
                    gboolean show_home = TRUE, show_trash = TRUE;
                    
                    manager = xfdesktop_file_icon_manager_new(path);
                    thunar_vfs_path_unref(path);
                    
                    rcfile = xfce_rc_config_open(XFCE_RESOURCE_CONFIG,
                                                 "xfce4/desktop/xfdesktoprc",
                                                 TRUE);
                    if(rcfile) {
                        if(xfce_rc_has_group(rcfile, "file-icons")) {
                            xfce_rc_set_group(rcfile, "file-icons");
                            show_filesystem = xfce_rc_read_bool_entry(rcfile,
                                                                      "show-filesystem",
                                                                      show_filesystem);
                            show_home = xfce_rc_read_bool_entry(rcfile,
                                                                "show-home",
                                                                show_home);
                            show_trash = xfce_rc_read_bool_entry(rcfile,
                                                                 "show-trash",
                                                                 show_trash);
                            show_removable = xfce_rc_read_bool_entry(rcfile,
                                                                     "show-removable",
                                                                     show_removable);
                        }
                        
                        xfce_rc_close(rcfile);
                    }
                    
                    xfdesktop_file_icon_manager_set_show_special_file(XFDESKTOP_FILE_ICON_MANAGER(manager),
                                                                      XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM,
                                                                      show_filesystem);
                    xfdesktop_file_icon_manager_set_show_special_file(XFDESKTOP_FILE_ICON_MANAGER(manager),
                                                                      XFDESKTOP_SPECIAL_FILE_ICON_HOME,
                                                                      show_home);
                    xfdesktop_file_icon_manager_set_show_special_file(XFDESKTOP_FILE_ICON_MANAGER(manager),
                                                                      XFDESKTOP_SPECIAL_FILE_ICON_TRASH,
                                                                      show_trash);
                    xfdesktop_file_icon_manager_set_show_removable_media(XFDESKTOP_FILE_ICON_MANAGER(manager),
                                                                         show_removable);
                } else {
                    g_critical("Unable to create ThunarVfsPath for '%s'",
                               desktop_path);
                }
                g_free(desktop_path);
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
                                          (desktop->priv->icons_use_system_font
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
}

static void
backdrop_changed_cb(XfceBackdrop *backdrop, gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);
    GdkPixmap *pmap = desktop->priv->bg_pixmap;
    GdkScreen *gscreen = desktop->priv->gscreen;
    GdkPixbuf *pix;
    GdkRectangle rect;
    gint i, monitor = -1;
    
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
    
    if(desktop->priv->nbackdrops == 1) {
        /* single monitor */
        rect.x = rect.y = 0;
        rect.width = gdk_screen_get_width(gscreen);
        rect.height = gdk_screen_get_height(gscreen);
    } else
        gdk_screen_get_monitor_geometry(gscreen, monitor, &rect);

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
    
    g_object_unref(G_OBJECT(desktop->priv->bg_pixmap));
    desktop->priv->bg_pixmap = gdk_pixmap_new(GDK_DRAWABLE(GTK_WIDGET(desktop)->window),
                                              w, h, -1);
    set_real_root_window_pixmap(desktop->priv->gscreen,
                                desktop->priv->bg_pixmap);
    gdk_window_set_back_pixmap(GTK_WIDGET(desktop)->window,
                               desktop->priv->bg_pixmap, FALSE);
    
    /* special case for 1 backdrop to handle xinerama stretching properly.
     * this is broken if it ever becomes possible to change the number of
     * monitors on the fly. */
    if(desktop->priv->nbackdrops == 1) {
        xfce_backdrop_set_size(desktop->priv->backdrops[0], w, h);
        backdrop_changed_cb(desktop->priv->backdrops[0], desktop);
    } else {
        for(i = 0; i < desktop->priv->nbackdrops; i++) {
            gdk_screen_get_monitor_geometry(gscreen, i, &rect);
            xfce_backdrop_set_size(desktop->priv->backdrops[i], rect.width,
                                   rect.height);
            backdrop_changed_cb(desktop->priv->backdrops[i], desktop);
        }
    }
}

static void
handle_xinerama_stretch(XfceDesktop *desktop)
{
    XfceBackdrop *backdrop0;
    gint i;
    
    if(desktop->priv->nbackdrops <= 1)
        return;
    
    for(i = 1; i < desktop->priv->nbackdrops; i++)
        g_object_unref(G_OBJECT(desktop->priv->backdrops[i]));
    
    backdrop0 = desktop->priv->backdrops[0];
    desktop->priv->backdrops = g_realloc(desktop->priv->backdrops,
                                         sizeof(XfceBackdrop *));
    desktop->priv->backdrops[0] = backdrop0;
    desktop->priv->nbackdrops = 1;
    
    xfce_backdrop_set_size(backdrop0,
            gdk_screen_get_width(desktop->priv->gscreen),
            gdk_screen_get_height(desktop->priv->gscreen));
}

static void
handle_xinerama_unstretch(XfceDesktop *desktop)
{
    XfceBackdrop *backdrop0 = desktop->priv->backdrops[0];
    GdkRectangle rect;
    GdkVisual *visual;
    gint i;
    
    if(desktop->priv->nbackdrops > 1)
        return;
    
    desktop->priv->nbackdrops = gdk_screen_get_n_monitors(desktop->priv->gscreen);
    desktop->priv->backdrops = g_realloc(desktop->priv->backdrops,
                                         sizeof(XfceBackdrop *) * desktop->priv->nbackdrops);
    
    desktop->priv->backdrops[0] = backdrop0;
    gdk_screen_get_monitor_geometry(desktop->priv->gscreen, 0, &rect);
    xfce_backdrop_set_size(backdrop0, rect.width, rect.height);
    
    visual = gtk_widget_get_visual(GTK_WIDGET(desktop));
    for(i = 1; i < desktop->priv->nbackdrops; i++) {
        gdk_screen_get_monitor_geometry(desktop->priv->gscreen, i, &rect);
        desktop->priv->backdrops[i] = xfce_backdrop_new_with_size(visual,
                rect.width, rect.height);
    }
    
    backdrop_changed_cb(backdrop0, desktop);
    for(i = 1; i < desktop->priv->nbackdrops; i++) {
        g_signal_connect(G_OBJECT(desktop->priv->backdrops[i]), "changed",
                G_CALLBACK(backdrop_changed_cb), desktop);
        backdrop_changed_cb(desktop->priv->backdrops[i], desktop);
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
    
    widget_class->realize = xfce_desktop_realize;
    widget_class->unrealize = xfce_desktop_unrealize;
    widget_class->button_press_event = xfce_desktop_button_press_event;
    widget_class->expose_event = xfce_desktop_expose;
    widget_class->delete_event = xfce_desktop_delete_event;
    widget_class->popup_menu = xfce_desktop_popup_menu;
    
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
}

static void
xfce_desktop_init(XfceDesktop *desktop)
{
    desktop->priv = G_TYPE_INSTANCE_GET_PRIVATE(desktop, XFCE_TYPE_DESKTOP,
                                                XfceDesktopPriv);
    GTK_WINDOW(desktop)->type = GTK_WINDOW_TOPLEVEL;
#ifdef ENABLE_DESKTOP_ICONS
    desktop->priv->icons_use_system_font = TRUE;
#endif
    
    gtk_window_set_type_hint(GTK_WINDOW(desktop), GDK_WINDOW_TYPE_HINT_DESKTOP);
    gtk_window_set_accept_focus(GTK_WINDOW(desktop), FALSE);
}

static void
xfce_desktop_finalize(GObject *object)
{
    XfceDesktop *desktop = XFCE_DESKTOP(object);
    
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    
    G_OBJECT_CLASS(xfce_desktop_parent_class)->finalize(object);
}

static void
xfce_desktop_realize(GtkWidget *widget)
{
    XfceDesktop *desktop = XFCE_DESKTOP(widget);
    GdkAtom atom;
    gint i, sw, sh;
    Window xid;
    GdkDisplay *gdpy;
    GdkWindow *groot;
    GdkVisual *visual;
    
    TRACE("entering");
    
    gtk_window_set_screen(GTK_WINDOW(desktop), desktop->priv->gscreen);
    sw = gdk_screen_get_width(desktop->priv->gscreen);
    sh = gdk_screen_get_height(desktop->priv->gscreen);
    
    /* chain up */
    GTK_WIDGET_CLASS(xfce_desktop_parent_class)->realize(widget);
    
    gtk_window_set_title(GTK_WINDOW(desktop), _("Desktop"));
    if(GTK_WIDGET_DOUBLE_BUFFERED(GTK_WIDGET(desktop)))
        gtk_widget_set_double_buffered(GTK_WIDGET(desktop), FALSE);
    
    gtk_widget_set_size_request(GTK_WIDGET(desktop), sw, sh);
    gtk_window_move(GTK_WINDOW(desktop), 0, 0);
    
    atom = gdk_atom_intern("_NET_WM_WINDOW_TYPE_DESKTOP", FALSE);
    gdk_property_change(GTK_WIDGET(desktop)->window,
            gdk_atom_intern("_NET_WM_WINDOW_TYPE", FALSE),
            gdk_atom_intern("ATOM", FALSE), 32,
            GDK_PROP_MODE_REPLACE, (guchar *)&atom, 1);
    
    gdpy = gdk_screen_get_display(desktop->priv->gscreen);
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
    
    visual = gtk_widget_get_visual(GTK_WIDGET(desktop));
    desktop->priv->nbackdrops = gdk_screen_get_n_monitors(desktop->priv->gscreen);
    desktop->priv->backdrops = g_new(XfceBackdrop *, desktop->priv->nbackdrops);
    for(i = 0; i < desktop->priv->nbackdrops; i++) {
        GdkRectangle rect;
        gdk_screen_get_monitor_geometry(desktop->priv->gscreen, i, &rect);
        desktop->priv->backdrops[i] = xfce_backdrop_new_with_size(visual,
                rect.width, rect.height);
    }
    
    desktop->priv->bg_pixmap = gdk_pixmap_new(GDK_DRAWABLE(GTK_WIDGET(desktop)->window),
                                              sw, sh, -1);
    set_real_root_window_pixmap(desktop->priv->gscreen,
                                desktop->priv->bg_pixmap);
    gdk_window_set_back_pixmap(GTK_WIDGET(desktop)->window,
                               desktop->priv->bg_pixmap, FALSE);
    
    /* doing this here is a little wasteful, but easier */
    if(desktop->priv->xinerama_stretch)
        handle_xinerama_stretch(desktop);
    
    g_signal_connect(G_OBJECT(desktop), "style-set",
                     G_CALLBACK(style_set_cb), NULL);
    
    for(i = 0; i < desktop->priv->nbackdrops; i++) {
        g_signal_connect(G_OBJECT(desktop->priv->backdrops[i]), "changed",
                G_CALLBACK(backdrop_changed_cb), desktop);
        backdrop_changed_cb(desktop->priv->backdrops[i], desktop);
    }
    
    g_signal_connect(G_OBJECT(desktop->priv->gscreen), "size-changed",
            G_CALLBACK(screen_size_changed_cb), desktop);
    
    gtk_widget_add_events(GTK_WIDGET(desktop), GDK_EXPOSURE_MASK);
    
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
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(desktop),
                                         G_CALLBACK(style_set_cb), NULL);
    
    if(GTK_WIDGET_MAPPED(widget))
        gtk_widget_unmap(widget);
    GTK_WIDGET_UNSET_FLAGS(widget, GTK_MAPPED);
    
    gtk_container_forall(GTK_CONTAINER(widget),
                         (GtkCallback)gtk_widget_unrealize,
                         NULL);
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(desktop->priv->gscreen),
            G_CALLBACK(screen_size_changed_cb), desktop);
    
    groot = gdk_screen_get_root_window(desktop->priv->gscreen);
    gdk_property_delete(groot, gdk_atom_intern("XFCE_DESKTOP_WINDOW", FALSE));
    gdk_property_delete(groot, gdk_atom_intern("NAUTILUS_DESKTOP_WINDOW_ID", FALSE));
    gdk_property_delete(groot, gdk_atom_intern("_XROOTPMAP_ID", FALSE));
    gdk_property_delete(groot, gdk_atom_intern("ESETROOT_PMAP_ID", FALSE));
    gdk_window_set_back_pixmap(groot, NULL, FALSE);
    
    if(desktop->priv->backdrops) {
        for(i = 0; i < desktop->priv->nbackdrops; i++) {
            g_snprintf(property_name, 128, XFDESKTOP_IMAGE_FILE_FMT, i);
            gdk_property_delete(groot, gdk_atom_intern(property_name, FALSE));
            g_object_unref(G_OBJECT(desktop->priv->backdrops[i]));
        }
        g_free(desktop->priv->backdrops);
        desktop->priv->backdrops = NULL;
    }
    
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
    
    if(evt->type == GDK_BUTTON_PRESS) {
        if(button == 2 || (button == 1 && (state & GDK_SHIFT_MASK)
                           && (state & GDK_CONTROL_MASK)))
        {
            xfce_desktop_popup_secondary_root_menu(XFCE_DESKTOP(w),
                                                   button, evt->time);
            return TRUE;
        } else if(button == 3 || (button == 1 && (state & GDK_SHIFT_MASK))) {
            xfce_desktop_popup_root_menu(XFCE_DESKTOP(w),
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
    XfceDesktop *desktop;
    GList *children, *l;
    
    TRACE("entering");
    
    if(evt->count != 0)
        return FALSE;
    
    desktop = XFCE_DESKTOP(w);
    
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
style_set_cb(GtkWidget *w,
             GtkStyle *old_style,
             gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(w);
    
    gdk_window_set_back_pixmap(w->window, desktop->priv->bg_pixmap, FALSE);
    gtk_widget_queue_draw(w);
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
xfce_desktop_new(GdkScreen *gscreen)
{
    XfceDesktop *desktop = g_object_new(XFCE_TYPE_DESKTOP, NULL);
    
    if(!gscreen)
        gscreen = gdk_display_get_default_screen(gdk_display_get_default());
    GTK_WINDOW(desktop)->screen = gscreen;
    desktop->priv->gscreen = gscreen;
    
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
    
    if(!desktop->priv->updates_frozen) {
        if(stretch)
            handle_xinerama_stretch(desktop);
        else
            handle_xinerama_unstretch(desktop);
        
        backdrop_changed_cb(desktop->priv->backdrops[0], desktop);
    }
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
    
    if(desktop->priv->icons_use_system_font)
        return;
    
    if(desktop->priv->icon_view) {
        xfdesktop_icon_view_set_font_size(XFDESKTOP_ICON_VIEW(desktop->priv->icon_view),
                                           font_size_points);
    }
#endif
}

void
xfce_desktop_set_icon_use_system_font_size(XfceDesktop *desktop,
                                           gboolean use_system)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    
#ifdef ENABLE_DESKTOP_ICONS
    if(use_system == desktop->priv->icons_use_system_font)
        return;
    
    desktop->priv->icons_use_system_font = use_system;
    
    if(desktop->priv->icon_view) {
        if(use_system) {
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
    
    if(desktop->priv->xinerama_stretch) {
        if(desktop->priv->nbackdrops > 1)
            handle_xinerama_stretch(desktop);
        backdrop_changed_cb(desktop->priv->backdrops[0], desktop);
    } else {
        if(desktop->priv->nbackdrops == 1)
            handle_xinerama_unstretch(desktop);
        else {
            gint i;
            for(i = 0; i < desktop->priv->nbackdrops; ++i)
                backdrop_changed_cb(desktop->priv->backdrops[i], desktop);
        }
    }
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
    
    menu = gtk_menu_new();
    gtk_menu_set_screen(GTK_MENU(menu), screen);
    gtk_widget_show(menu);
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
    
    if(xfdesktop_popup_grab_available(gdk_screen_get_root_window(screen),
                                      activate_time))
    {
        gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, button,
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
