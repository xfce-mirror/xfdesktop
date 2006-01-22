/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004-2006 Brian Tarricone, <bjt23@cornell.edu>
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
#include <pango/pango.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <libxfcegui4/netk-window-action-menu.h>

#include "xfce-desktop.h"
#include "xfdesktop-common.h"
#include "main.h"


#ifdef ENABLE_WINDOW_ICONS

/********
 * TODO *
 ********
 * + theme/font changes
 * + keyboard navigation, allow kbd focus
 */

#define ICON_SIZE     32
#define CELL_SIZE     112
#define TEXT_WIDTH    100
#define CELL_PADDING  6
#define SPACING       8
#define SCREEN_MARGIN 16

typedef struct _XfceDesktopIcon
{
    guint16 row;
    guint16 col;
    GdkPixbuf *pix;
    gchar *label;
    GdkRectangle extents;
    NetkWindow *window;
    XfceDesktop *desktop;
} XfceDesktopIcon;

typedef struct _XfceDesktopIconWorkspace
{
    GHashTable *icons;
    XfceDesktopIcon *selected_icon;
    gint xorigin,
         yorigin,
         width,
         height;
    guint16 nrows;
    guint16 ncols;
    guint8 *grid_layout;
} XfceDesktopIconWorkspace;

#endif /* defined(ENABLE_WINDOW_ICONS) */

struct _XfceDesktopPriv
{
    GdkScreen *gscreen;
    McsClient *mcs_client;
    
    GdkPixmap *bg_pixmap;
    
    guint nbackdrops;
    XfceBackdrop **backdrops;
    
#ifdef ENABLE_WINDOW_ICONS
    gboolean use_window_icons;
    
    NetkScreen *netk_screen;
    gint cur_ws_num;
    PangoLayout *playout;
    
    XfceDesktopIconWorkspace **icon_workspaces;
    
    guint grid_resize_timeout;
    guint icon_repaint_id;
    
    gboolean maybe_begin_drag;
    gboolean definitely_dragging;
    gint press_start_x;
    gint press_start_y;
    XfceDesktopIcon *last_clicked_item;
    GtkTargetList *source_targets;
#endif
};

#ifdef ENABLE_WINDOW_ICONS
static void xfce_desktop_icon_paint(XfceDesktopIcon *icon);
static void xfce_desktop_icon_add(XfceDesktop *desktop, NetkWindow *window, guint idx);
static void xfce_desktop_icon_remove(XfceDesktop *desktop, XfceDesktopIcon *icon, NetkWindow *window, guint idx);
static void xfce_desktop_icon_free(XfceDesktopIcon *icon);
static void xfce_desktop_setup_icons(XfceDesktop *desktop);
static void xfce_desktop_unsetup_icons(XfceDesktop *desktop);
static void xfce_desktop_paint_icons(XfceDesktop *desktop, GdkRectangle *area);
static void xfce_desktop_window_name_changed_cb(NetkWindow *window, gpointer user_data);
static void xfce_desktop_window_icon_changed_cb(NetkWindow *window, gpointer user_data);
static gboolean xfce_desktop_button_press(GtkWidget *widget, GdkEventButton *evt);
static gboolean xfce_desktop_button_release(GtkWidget *widget, GdkEventButton *evt);
static gboolean xfce_desktop_motion_notify(GtkWidget *widfet, GdkEventMotion *evt);
static void xfce_desktop_drag_begin(GtkWidget *widget, GdkDragContext *context);
static gboolean xfce_desktop_drag_motion(GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time);
static void xfce_desktop_drag_leave(GtkWidget *widget, GdkDragContext *context, guint time);
static gboolean xfce_desktop_drag_drop(GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time);
/* utility funcs */
static void desktop_setup_grids(XfceDesktop *desktop, gint nws);
static gboolean grid_get_next_free_position(XfceDesktopIconWorkspace *icon_workspace, guint16 *row, guint16 *col);
static gboolean grid_is_free_position(XfceDesktopIconWorkspace *icon_workspace, guint16 row, guint16 col);
static void grid_set_position_free(XfceDesktopIconWorkspace *icon_workspace, guint16 row, guint16 col);
static void grid_unset_position_free(XfceDesktopIconWorkspace *icon_workspace, guint16 row, guint16 col);
static gboolean xfce_desktop_maybe_begin_drag(XfceDesktop *desktop, GdkEventMotion *evt);

static const GtkTargetEntry targets[] = {
    { "XFCE_DESKTOP_WINDOW_ICON", GTK_TARGET_SAME_APP, 0 },
};
static const gint n_targets = 1;
#endif

static void xfce_desktop_class_init(XfceDesktopClass *klass);
static void xfce_desktop_init(XfceDesktop *desktop);
static void xfce_desktop_finalize(GObject *object);
static void xfce_desktop_realize(GtkWidget *widget);
static void xfce_desktop_unrealize(GtkWidget *widget);

static gboolean xfce_desktop_expose(GtkWidget *w, GdkEventExpose *evt, gpointer user_data);

static void load_initial_settings(XfceDesktop *desktop, McsClient *mcs_client);


GtkWindowClass *parent_class = NULL;


#ifdef ENABLE_WINDOW_ICONS

#if defined(DEBUG) && DEBUG > 0
#define dump_grid_layout(my_icon_workspace) \
{\
    gint my_i, my_maxi;\
    \
    g_printerr("\nDBG[%s:%d] %s\n", __FILE__, __LINE__, __FUNCTION__);\
    my_maxi = my_icon_workspace->nrows * my_icon_workspace->ncols / 8 + 1;\
    for(my_i = 0; my_i < my_maxi; my_i++)\
        g_printerr("%02hhx ", my_icon_workspace->grid_layout[my_i]);\
    g_printerr("\n\n");\
}
#else
#define dump_grid_layout(icon_workspace)
#endif

static void
xfce_desktop_icon_free(XfceDesktopIcon *icon)
{
    if(icon->pix)
        g_object_unref(G_OBJECT(icon->pix));
    if(icon->label)
        g_free(icon->label);
    g_free(icon);
}

static void
xfce_desktop_icon_add(XfceDesktop *desktop,
                      NetkWindow *window,
                      guint idx)
{
    XfceDesktopIcon *icon = g_new0(XfceDesktopIcon, 1);
    gchar data_name[256];
    guint16 old_row, old_col;
    gboolean got_pos = FALSE;
    NetkWorkspace *active_ws;
            
    /* check for availability of old position (if any) */
    g_snprintf(data_name, 256, "xfdesktop-last-row-%d", idx);
    old_row = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(window),
                                                 data_name));
    g_snprintf(data_name, 256, "xfdesktop-last-col-%d", idx);
    old_col = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(window),
                                                 data_name));
    if(old_row && old_col) {
        icon->row = old_row - 1;
        icon->col = old_col - 1;
    
        if(grid_is_free_position(desktop->priv->icon_workspaces[idx],
                                 icon->row, icon->col))
        {
            DBG("old position (%d,%d) is free", icon->row, icon->col);
            got_pos = TRUE;
        }
    }
    
    if(!got_pos) {
       if(!grid_get_next_free_position(desktop->priv->icon_workspaces[idx],
                                       &icon->row, &icon->col)) 
        {
           g_free(icon);
           return;
       } else {
           DBG("old position didn't exist or isn't free, got (%d,%d) instead", icon->row, icon->col);
       }
    }
    
    grid_unset_position_free(desktop->priv->icon_workspaces[idx],
                             icon->row, icon->col);

    icon->pix = netk_window_get_icon(window);
    if(icon->pix) {
        if(gdk_pixbuf_get_width(icon->pix) != ICON_SIZE) {
            icon->pix = gdk_pixbuf_scale_simple(icon->pix,
                                                ICON_SIZE,
                                                ICON_SIZE,
                                                GDK_INTERP_BILINEAR);
        }
        g_object_ref(G_OBJECT(icon->pix));
    }
    icon->label = g_strdup(netk_window_get_name(window));
    icon->window = window;
    icon->desktop = desktop;
    g_hash_table_insert(desktop->priv->icon_workspaces[idx]->icons,
                        window, icon);
    
    g_signal_connect(G_OBJECT(window), "name-changed",
                     G_CALLBACK(xfce_desktop_window_name_changed_cb), icon);
    g_signal_connect(G_OBJECT(window), "icon-changed",
                     G_CALLBACK(xfce_desktop_window_icon_changed_cb), icon);
    
    active_ws = netk_screen_get_active_workspace(desktop->priv->netk_screen);
    if(idx == netk_workspace_get_number(active_ws))
        xfce_desktop_icon_paint(icon);
}

static void
xfce_desktop_icon_remove(XfceDesktop *desktop,
                         XfceDesktopIcon *icon,
                         NetkWindow *window,
                         guint idx)
{
    GdkRectangle area;
    guint16 row, col;
    gchar data_name[256];
    NetkWorkspace *active_ws;
    gint active_ws_num;
    
    active_ws = netk_screen_get_active_workspace(desktop->priv->netk_screen);
    active_ws_num = netk_workspace_get_number(active_ws);
    
    row = icon->row;
    col = icon->col;
    memcpy(&area, &icon->extents, sizeof(area));
    
    if(desktop->priv->icon_workspaces[idx]->selected_icon == icon)
        desktop->priv->icon_workspaces[idx]->selected_icon = NULL;
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(window),
                                         G_CALLBACK(xfce_desktop_window_name_changed_cb),
                                         icon);
    g_signal_handlers_disconnect_by_func(G_OBJECT(window),
                                         G_CALLBACK(xfce_desktop_window_icon_changed_cb),
                                         icon);
    
    g_hash_table_remove(desktop->priv->icon_workspaces[idx]->icons,
                        window);
    
    if(idx == active_ws_num) {
        DBG("clearing %dx%d+%d+%d", area.width, area.height,
            area.x, area.y);
        gdk_window_clear_area(GTK_WIDGET(desktop)->window,
                              area.x, area.y,
                              area.width, area.height);
    }
    
    /* save the old positions for later */
    g_snprintf(data_name, 256, "xfdesktop-last-row-%d", idx);
    g_object_set_data(G_OBJECT(window), data_name,
                      GUINT_TO_POINTER(row+1));
    g_snprintf(data_name, 256, "xfdesktop-last-col-%d", idx);
    g_object_set_data(G_OBJECT(window), data_name,
                      GUINT_TO_POINTER(col+1));
    
    grid_set_position_free(desktop->priv->icon_workspaces[idx], row, col);
}

static gboolean
find_icon_below_from_hash(gpointer key,
                          gpointer value,
                          gpointer user_data)
{
    XfceDesktopIcon *icon = user_data;
    XfceDesktopIcon *icon_maybe_below = value;
    
    if(icon_maybe_below->row == icon->row + 1)
        return TRUE;
    else
        return FALSE;
}

static XfceDesktopIcon *
find_icon_below(XfceDesktop *desktop,
                XfceDesktopIcon *icon)
{
    XfceDesktopIcon *icon_below = NULL;
    NetkWorkspace *active_ws;
    gint active_ws_num;
    
    active_ws = netk_screen_get_active_workspace(desktop->priv->netk_screen);
    active_ws_num = netk_workspace_get_number(active_ws);
    
    if(icon->row == desktop->priv->icon_workspaces[active_ws_num]->nrows - 1)
        return NULL;
    
    icon_below = g_hash_table_find(desktop->priv->icon_workspaces[active_ws_num]->icons,
                                   find_icon_below_from_hash, icon);
    
    return icon_below;
}

static void
xfce_desktop_icon_paint(XfceDesktopIcon *icon)
{
    XfceDesktop *desktop = icon->desktop;
    GtkWidget *widget = GTK_WIDGET(desktop);
    gint pix_w, pix_h, pix_x, pix_y, text_x, text_y, text_w, text_h,
         cell_x, cell_y, state, active_ws_num;
    PangoLayout *playout;
    GdkRectangle area;
    
    TRACE("entering");
    
    active_ws_num = netk_workspace_get_number(netk_screen_get_active_workspace(desktop->priv->netk_screen));
    
    if(icon->extents.width > 0 && icon->extents.height > 0) {
        /* FIXME: this is really only needed when going from selected <-> not
         * selected.  should fix for optimisation. */
        gdk_window_clear_area(widget->window, icon->extents.x, icon->extents.y,
                              icon->extents.width, icon->extents.height);
        
        /* check and make sure we didn't used to be too large for the cell.
         * if so, repaint the one below it first. */
        if(icon->extents.height + 3 * CELL_PADDING > CELL_SIZE) {
            XfceDesktopIcon *icon_below = find_icon_below(desktop, icon);
            if(icon_below)
                xfce_desktop_icon_paint(icon_below);
        }
    }
    
    if(desktop->priv->icon_workspaces[desktop->priv->cur_ws_num]->selected_icon == icon)
        state = GTK_STATE_SELECTED;
    else
        state = GTK_STATE_NORMAL;
    
    pix_w = gdk_pixbuf_get_width(icon->pix);
    pix_h = gdk_pixbuf_get_height(icon->pix);
    
    playout = desktop->priv->playout;
    pango_layout_set_alignment(playout, PANGO_ALIGN_CENTER);
    pango_layout_set_width(playout, -1);
    pango_layout_set_text(playout, icon->label, -1);
    pango_layout_get_size(playout, &text_w, &text_h);
    DBG("unadjusted size: %dx%d", text_w/PANGO_SCALE, text_h/PANGO_SCALE);
    if(text_w > TEXT_WIDTH * PANGO_SCALE) {
        if(state == GTK_STATE_NORMAL) {
#if GTK_CHECK_VERSION(2, 6, 0)  /* can't find a way to get pango version info */
            pango_layout_set_ellipsize(playout, PANGO_ELLIPSIZE_END);
#endif
        } else {
            pango_layout_set_wrap(playout, PANGO_WRAP_WORD_CHAR);
#if GTK_CHECK_VERSION(2, 6, 0)  /* can't find a way to get pango version info */
            pango_layout_set_ellipsize(playout, PANGO_ELLIPSIZE_NONE);
#endif
        }
        pango_layout_set_width(playout, TEXT_WIDTH * PANGO_SCALE);
    }
    pango_layout_get_pixel_size(playout, &text_w, &text_h);
    DBG("adjusted size: %dx%d", text_w, text_h);
    
    cell_x = desktop->priv->icon_workspaces[active_ws_num]->xorigin;
    cell_x += icon->col * CELL_SIZE + CELL_PADDING;
    cell_y = desktop->priv->icon_workspaces[active_ws_num]->yorigin;
    cell_y += icon->row * CELL_SIZE + CELL_PADDING;
    
    pix_x = cell_x + ((CELL_SIZE - 2 * CELL_PADDING) - pix_w) / 2;
    pix_y = cell_y + 2 * CELL_PADDING;
    
    /*
    DBG("computing text_x:\n\tcell_x=%d\n\tcell width: %d\n\ttext_w: %d\n\tnon-text space: %d\n\tdiv 2: %d",
        cell_x,
        CELL_SIZE - 2 * CELL_PADDING,
        text_w,
        ((CELL_SIZE - 2 * CELL_PADDING) - text_w),
        ((CELL_SIZE - 2 * CELL_PADDING) - text_w) / 2);
    */
    
    text_x = cell_x + ((CELL_SIZE - 2 * CELL_PADDING) - text_w) / 2;
    text_y = cell_y + 2 * CELL_PADDING + pix_h + SPACING + 2;
    
    DBG("drawing pixbuf at (%d,%d)", pix_x, pix_y);
    
    gdk_draw_pixbuf(GDK_DRAWABLE(widget->window), widget->style->black_gc,
                    icon->pix, 0, 0, pix_x, pix_y, pix_w, pix_h,
                    GDK_RGB_DITHER_NORMAL, 0, 0);
    
    DBG("painting layout: area: %dx%d+%d+%d", text_w, text_h, text_x, text_y);
    
    area.x = text_x - 2;
    area.y = text_y - 2;
    area.width = text_w + 4;
    area.height = text_h + 4;
    
    gtk_paint_box(widget->style, widget->window, state,
                  GTK_SHADOW_IN, &area, widget, "background",
                  area.x, area.y, area.width, area.height);
    
    gtk_paint_layout(widget->style, widget->window, state, FALSE,
                     &area, widget, "label", text_x, text_y, playout);
    
#if 0 /* debug */
    gdk_draw_rectangle(GDK_DRAWABLE(widget->window),
                       widget->style->white_gc,
                       FALSE,
                       cell_x - CELL_PADDING,
                       cell_y - CELL_PADDING,
                       CELL_SIZE,
                       CELL_SIZE);
#endif
    
    icon->extents.x = (pix_w > text_w + 4 ? pix_x : text_x - 2);
    icon->extents.y = cell_y + (2 * CELL_PADDING);
    icon->extents.width = (pix_w > text_w + 4 ? pix_w : text_w + 4);
    icon->extents.height = (text_y + text_h + 2) - icon->extents.y;
}

static void
xfce_desktop_icon_paint_delayed(NetkWindow *window,
                                NetkWindowState changed_mask,
                                NetkWindowState new_state,
                                gpointer user_data)
{
    DBG("repainting under icon");
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(window),
                                         G_CALLBACK(xfce_desktop_icon_paint_delayed),
                                         user_data);
    g_object_unref(G_OBJECT(window));
    
    xfce_desktop_icon_paint((XfceDesktopIcon *)user_data);
}

static gboolean
xfce_desktop_icon_paint_idled(gpointer user_data)
{
    XfceDesktopIcon *icon = (XfceDesktopIcon *)user_data;
    
    icon->desktop->priv->icon_repaint_id = 0;
    
    xfce_desktop_icon_paint(icon);
    
    return FALSE;
}

static void
xfce_desktop_window_name_changed_cb(NetkWindow *window,
                                    gpointer user_data)
{
    XfceDesktopIcon *icon = user_data;
    
    g_free(icon->label);
    icon->label = g_strdup(netk_window_get_name(window));
    
    xfce_desktop_icon_paint(icon);
}

static void
xfce_desktop_window_icon_changed_cb(NetkWindow *window,
                                    gpointer user_data)
{
    XfceDesktopIcon *icon = user_data;
    
    if(icon->pix)
        g_object_unref(G_OBJECT(icon->pix));
    
    icon->pix = netk_window_get_icon(window);
    if(icon->pix) {
        if(gdk_pixbuf_get_width(icon->pix) != ICON_SIZE) {
            icon->pix = gdk_pixbuf_scale_simple(icon->pix,
                                                ICON_SIZE,
                                                ICON_SIZE,
                                                GDK_INTERP_BILINEAR);
        }
        g_object_ref(G_OBJECT(icon->pix));
    }
    
    xfce_desktop_icon_paint(icon);
}

#endif /* defined(ENABLE_WINDOW_ICONS) */

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

inline gint
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
    cmap = gdk_drawable_get_colormap(GDK_DRAWABLE(GTK_WIDGET(desktop)->window));
    
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
    
    /* clear the old pixmap, if any */
    if(XFCE_DESKTOP(desktop)->priv->bg_pixmap)
        g_object_unref(G_OBJECT(XFCE_DESKTOP(desktop)->priv->bg_pixmap));
    
    /* set the new pixmap and tell gtk to redraw it */
    XFCE_DESKTOP(desktop)->priv->bg_pixmap = pmap;
    gdk_window_set_back_pixmap(desktop->window, pmap, FALSE);
    gtk_widget_queue_draw_area(desktop, rect.x, rect.y, rect.width, rect.height);
    
    gdk_error_trap_pop();
}

#ifdef ENABLE_WINDOW_ICONS

static gboolean
desktop_icons_constrain(gpointer key,
                        gpointer value,
                        gpointer user_data)
{
    XfceDesktopIcon *icon = value;
    XfceDesktopIconWorkspace *icon_workspace = user_data;
    
    if(icon->row >= icon_workspace->nrows
       || icon->col >= icon_workspace->ncols)
    {
        if(!grid_get_next_free_position(icon_workspace, &icon->row, &icon->col))
            return TRUE;
        
        DBG("icon %s moved to (%d,%d)", icon->label, icon->row, icon->col);
        
        grid_unset_position_free(icon_workspace, icon->row, icon->col);
    }
    
    return FALSE;
}

static void
desktop_grid_do_resize(XfceDesktop *desktop)
{
    gint i, nws, *old_rows, *old_cols;
    
    nws = netk_screen_get_workspace_count(desktop->priv->netk_screen);
    
    /* remember the old sizes */
    old_rows = g_new(gint, nws);
    old_cols = g_new(gint, nws);
    for(i = 0; i < nws; i++) {
        old_rows[i] = desktop->priv->icon_workspaces[i]->nrows;
        old_cols[i] = desktop->priv->icon_workspaces[i]->ncols;
    }
    
    DBG("old geom: %dx%d", old_rows[0], old_cols[0]);
    
    desktop_setup_grids(desktop, nws);
    
    DBG("new geom: %dx%d", desktop->priv->icon_workspaces[0]->nrows,
        desktop->priv->icon_workspaces[0]->ncols);
    
    /* make sure we don't lose any icons off the screen */
    for(i = 0; i < nws; i++) {
        if(old_rows[i] > desktop->priv->icon_workspaces[i]->nrows
           || old_cols[i] > desktop->priv->icon_workspaces[i]->ncols)
        {
            g_hash_table_foreach_remove(desktop->priv->icon_workspaces[i]->icons,
                                        desktop_icons_constrain,
                                        desktop->priv->icon_workspaces[i]);
        }
    }
    
    g_free(old_rows);
    g_free(old_cols);
    
    gtk_widget_queue_draw(GTK_WIDGET(desktop));
}

static gboolean
desktop_grid_resize_timeout(gpointer user_data)
{
    XfceDesktop *desktop = user_data;
    
    desktop_grid_do_resize(desktop);
    
    desktop->priv->grid_resize_timeout = 0;
    return FALSE;
}

#endif  /* #ifdef ENABLE_WINDOW_ICONS */

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
    
#ifdef ENABLE_WINDOW_ICONS
    /* this is kinda icky.  we want to use _NET_WORKAREA to reset the size of
     * the grid, but we can never be sure it'll actually change.  so let's
     * give it 7 seconds, and then fix it manually */
    if(desktop->priv->grid_resize_timeout)
        g_source_remove(desktop->priv->grid_resize_timeout);
    desktop->priv->grid_resize_timeout = g_timeout_add(7000,
                                                       desktop_grid_resize_timeout,
                                                       desktop);
#endif
}

static void
handle_xinerama_stretch(XfceDesktop *desktop)
{
    XfceBackdrop *backdrop0;
    gint i;
    
    for(i = 1; i < desktop->priv->nbackdrops; i++)
        g_object_unref(G_OBJECT(desktop->priv->backdrops[i]));
    
    backdrop0 = desktop->priv->backdrops[0];
    g_free(desktop->priv->backdrops);
    desktop->priv->backdrops = g_new(XfceBackdrop *, 1);
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
    
    desktop->priv->nbackdrops = gdk_screen_get_n_monitors(desktop->priv->gscreen);
    g_free(desktop->priv->backdrops);
    desktop->priv->backdrops = g_new(XfceBackdrop *, desktop->priv->nbackdrops);
    
    desktop->priv->backdrops[0] = backdrop0;
    gdk_screen_get_monitor_geometry(desktop->priv->gscreen, 0, &rect);
    xfce_backdrop_set_size(backdrop0, rect.width, rect.height);
    
    visual = gtk_widget_get_visual(GTK_WIDGET(desktop));
    for(i = 1; i < desktop->priv->nbackdrops; i++) {
        gdk_screen_get_monitor_geometry(desktop->priv->gscreen, i, &rect);
        desktop->priv->backdrops[i] = xfce_backdrop_new_with_size(visual,
                rect.width, rect.height);
    }
    
    if(desktop->priv->mcs_client)
        load_initial_settings(desktop, desktop->priv->mcs_client);
    
    backdrop_changed_cb(backdrop0, desktop);
    for(i = 1; i < desktop->priv->nbackdrops; i++) {
        g_signal_connect(G_OBJECT(desktop->priv->backdrops[i]), "changed",
                G_CALLBACK(backdrop_changed_cb), desktop);
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
    
    if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, "xineramastretch",
            BACKDROP_CHANNEL, &setting))
    {
        if(setting->data.v_int)
            handle_xinerama_stretch(desktop);
        mcs_setting_free(setting);
        setting = NULL;
    }
    
#ifdef ENABLE_WINDOW_ICONS
    if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, "usewindowicons",
            BACKDROP_CHANNEL, &setting))
    {
        if(setting->data.v_int)
            desktop->priv->use_window_icons = TRUE;
        else
            desktop->priv->use_window_icons = FALSE;
        mcs_setting_free(setting);
        setting = NULL;
    } else
        desktop->priv->use_window_icons = TRUE;
    
    if(desktop->priv->use_window_icons && GTK_WIDGET_REALIZED(GTK_WIDGET(desktop)))
        xfce_desktop_setup_icons(desktop);
#endif
    
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

static void
desktop_style_set_cb(GtkWidget *w, GtkStyle *old, gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(w);
    
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    
    if(desktop->priv->bg_pixmap) {
        gdk_window_set_back_pixmap(w->window, desktop->priv->bg_pixmap, FALSE);
        gtk_widget_queue_draw(w);
    }
    
#ifdef ENABLE_WINDOW_ICONS
    
    /* TODO: see if the font has changed and redraw text */
    
#endif
}

#ifdef ENABLE_WINDOW_ICONS

static gboolean
desktop_get_workarea_single(XfceDesktop *desktop,
                            guint ws_num,
                            gint *xorigin,
                            gint *yorigin,
                            gint *width,
                            gint *height)
{
    gboolean ret = FALSE;
    Display *dpy;
    Window root;
    Atom property, actual_type = None;
    gint actual_format = 0, first_id;
    gulong nitems = 0, bytes_after = 0, offset = 0;
    unsigned char *data_p = NULL;
    
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop) && xorigin && yorigin
                         && width && height, FALSE);
    
    dpy = GDK_DISPLAY_XDISPLAY(gtk_widget_get_display(GTK_WIDGET(desktop)));
    root = GDK_WINDOW_XID(gdk_screen_get_root_window(desktop->priv->gscreen));
    property = XInternAtom(dpy, "_NET_WORKAREA", False);
    
    first_id = ws_num * 4;
    
    gdk_error_trap_push();
    
    do {
        if(Success == XGetWindowProperty(dpy, root, property, offset,
                                         G_MAXULONG, False, XA_CARDINAL,
                                         &actual_type, &actual_format, &nitems,
                                         &bytes_after, &data_p))
        {
            gint i;
            gulong *data = (gulong *)data_p;
            
            if(actual_format != 32 || actual_type != XA_CARDINAL) {
                XFree(data);
                break;
            }
            
            i = offset / 32;  /* first element id in this batch */
            
            /* there's probably a better way to do this. */
            if(i + nitems >= first_id && first_id - offset >= 0)
                *xorigin = data[first_id - offset] + SCREEN_MARGIN;
            if(i + nitems >= first_id + 1 && first_id - offset + 1 >= 0)
                *yorigin = data[first_id - offset + 1] + SCREEN_MARGIN;
            if(i + nitems >= first_id + 2 && first_id - offset + 2 >= 0)
                *width = data[first_id - offset + 2] - 2 * SCREEN_MARGIN;
            if(i + nitems >= first_id + 3 && first_id - offset + 3 >= 0) {
                *height = data[first_id - offset + 3] - 2 * SCREEN_MARGIN;
                ret = TRUE;
                break;
            }
            
            offset += actual_format * nitems;
        } else
            break;
    } while(bytes_after > 0);
    
    gdk_error_trap_pop();
    
    return ret;
}

static gboolean
desktop_get_workarea(XfceDesktop *desktop,
                     guint nworkspaces,
                     gint *xorigins,
                     gint *yorigins,
                     gint *widths,
                     gint *heights)
{
    gboolean ret = FALSE;
    Display *dpy;
    Window root;
    Atom property, actual_type = None;
    gint actual_format = 0;
    gulong nitems = 0, bytes_after = 0, *data = NULL, offset = 0;
    gint *full_data, i = 0, j;
    unsigned char *data_p = NULL;
    
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop) && xorigins && yorigins
                         && widths && heights, FALSE);
    
    full_data = g_new0(gint, nworkspaces * 4);
    
    dpy = GDK_DISPLAY_XDISPLAY(gtk_widget_get_display(GTK_WIDGET(desktop)));
    root = GDK_WINDOW_XID(gdk_screen_get_root_window(desktop->priv->gscreen));
    property = XInternAtom(dpy, "_NET_WORKAREA", False);
    
    gdk_error_trap_push();
    
    do {
        if(Success == XGetWindowProperty(dpy, root, property, offset,
                                         G_MAXULONG, False, XA_CARDINAL,
                                         &actual_type, &actual_format, &nitems,
                                         &bytes_after, &data_p))
        {
            if(!data_p)
                break;
            
            if(actual_format != 32 || actual_type != XA_CARDINAL) {
                XFree(data_p);
                break;
            }
            
            data = (gulong *)data_p;
            for(j = 0; j < nitems; j++, i++)
                full_data[i] = data[j];
            XFree(data_p);
            data_p = NULL;
            
            if(i == nworkspaces * 4)
                ret = TRUE;
            
            offset += actual_format * nitems;
            
        } else
            break;
    } while(bytes_after > 0);
    
    gdk_error_trap_pop();
    
    if(ret) {
        for(i = 0; i < nworkspaces*4; i += 4) {
            xorigins[i/4] = full_data[i] + SCREEN_MARGIN;
            yorigins[i/4] = full_data[i+1] + SCREEN_MARGIN;
            widths[i/4] = full_data[i+2] - 2 * SCREEN_MARGIN;
            heights[i/4] = full_data[i+3] - 2 * SCREEN_MARGIN;
        }
    }
    
    g_free(full_data);
    
    return ret;
}

static void
workspace_changed_cb(NetkScreen *netk_screen,
                     gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);
    gint cur_col = 0, cur_row = 0, n;
    NetkWorkspace *ws;
    
    ws = netk_screen_get_active_workspace(desktop->priv->netk_screen);
    desktop->priv->cur_ws_num = n = netk_workspace_get_number(ws);
    if(!desktop->priv->icon_workspaces[n]->icons) {
        GList *windows, *l;
        
        desktop->priv->icon_workspaces[n]->icons =
            g_hash_table_new_full(g_direct_hash,
                                  g_direct_equal,
                                  NULL,
                                  (GDestroyNotify)xfce_desktop_icon_free);
        
        windows = netk_screen_get_windows(desktop->priv->netk_screen);
        for(l = windows; l; l = l->next) {
            NetkWindow *window = l->data;
            
            if((ws == netk_window_get_workspace(window)
                || netk_window_is_pinned(window))
               && netk_window_is_minimized(window)
               && !netk_window_is_skip_tasklist(window))
            {
                XfceDesktopIcon *icon;
                
                icon = g_new0(XfceDesktopIcon, 1);
                icon->row = cur_row;
                icon->col = cur_col;
                icon->pix = netk_window_get_icon(window);
                if(icon->pix) {
                    if(gdk_pixbuf_get_width(icon->pix) != ICON_SIZE) {
                        icon->pix = gdk_pixbuf_scale_simple(icon->pix,
                                                            ICON_SIZE,
                                                            ICON_SIZE,
                                                            GDK_INTERP_BILINEAR);
                    }
                    g_object_ref(G_OBJECT(icon->pix));
                }
                icon->label = g_strdup(netk_window_get_name(window));
                icon->window = window;
                icon->desktop = desktop;
                g_hash_table_insert(desktop->priv->icon_workspaces[n]->icons,
                                    window, icon);
                g_signal_connect(G_OBJECT(window), "name-changed",
                                 G_CALLBACK(xfce_desktop_window_name_changed_cb),
                                 icon);
                g_signal_connect(G_OBJECT(window), "icon-changed",
                                 G_CALLBACK(xfce_desktop_window_icon_changed_cb),
                                 icon);
                
                grid_unset_position_free(desktop->priv->icon_workspaces[n],
                                         cur_row, cur_col);
                
                cur_row++;
                if(cur_row >= desktop->priv->icon_workspaces[n]->nrows) {
                    cur_col++;
                    if(cur_col >= desktop->priv->icon_workspaces[n]->ncols)
                        break;
                    cur_row = 0;
                }
            }
        }
    }
    
    gtk_widget_queue_draw(GTK_WIDGET(desktop));
}

static void
workspace_created_cb(NetkScreen *netk_screen,
                     NetkWorkspace *workspace,
                     gpointer user_data)
{
    XfceDesktop *desktop = user_data;
    gint ws_num, n_ws, xo, yo, w, h;
    
    n_ws = netk_screen_get_workspace_count(netk_screen);
    ws_num = netk_workspace_get_number(workspace);
    
    desktop->priv->icon_workspaces = g_realloc(desktop->priv->icon_workspaces,
                                               sizeof(XfceDesktopIconWorkspace *) * n_ws);
    
    if(ws_num != n_ws - 1) {
        g_memmove(desktop->priv->icon_workspaces + ws_num + 1,
                  desktop->priv->icon_workspaces + ws_num,
                  sizeof(XfceDesktopIconWorkspace *) * (n_ws - ws_num - 1));
    }
    
    desktop->priv->icon_workspaces[ws_num] = g_new0(XfceDesktopIconWorkspace, 1);
    
    if(desktop_get_workarea_single(desktop, ws_num, &xo, &yo, &w, &h)) {
        DBG("got workarea: %dx%d+%d+%d", w, h, xo, yo);
        desktop->priv->icon_workspaces[ws_num]->xorigin = xo;
        desktop->priv->icon_workspaces[ws_num]->yorigin = yo;
        desktop->priv->icon_workspaces[ws_num]->width = w;
        desktop->priv->icon_workspaces[ws_num]->height = h;
    } else {
        desktop->priv->icon_workspaces[ws_num]->xorigin = 0;
        desktop->priv->icon_workspaces[ws_num]->yorigin = 0;
        desktop->priv->icon_workspaces[ws_num]->width =
                gdk_screen_get_width(desktop->priv->gscreen);
        desktop->priv->icon_workspaces[ws_num]->height =
                gdk_screen_get_height(desktop->priv->gscreen);
    }
    
    desktop->priv->icon_workspaces[ws_num]->nrows = 
        desktop->priv->icon_workspaces[ws_num]->height / CELL_SIZE;
    desktop->priv->icon_workspaces[ws_num]->ncols = 
        desktop->priv->icon_workspaces[ws_num]->width / CELL_SIZE;
    desktop->priv->icon_workspaces[ws_num]->grid_layout =
        g_malloc0(desktop->priv->icon_workspaces[ws_num]->nrows
                  * desktop->priv->icon_workspaces[ws_num]->ncols / 8 + 1);
}

static void
workspace_destroyed_cb(NetkScreen *netk_screen,
                       NetkWorkspace *workspace,
                       gpointer user_data)
{
    /* TODO: check if we get workspace-destroyed before or after all the
     * windows on that workspace were moved and we got workspace-changed
     * for each one.  preferably that is the case. */
    
    XfceDesktop *desktop = user_data;
    gint ws_num, n_ws;
    
    n_ws = netk_screen_get_workspace_count(netk_screen);
    ws_num = netk_workspace_get_number(workspace);
    
    if(desktop->priv->icon_workspaces[ws_num]->icons)
        g_hash_table_destroy(desktop->priv->icon_workspaces[ws_num]->icons);
    g_free(desktop->priv->icon_workspaces[ws_num]);
    
    if(ws_num != n_ws) {
        g_memmove(desktop->priv->icon_workspaces + ws_num,
                  desktop->priv->icon_workspaces + ws_num + 1,
                  sizeof(XfceDesktopIconWorkspace *) * (n_ws - ws_num));
    }
    
    desktop->priv->icon_workspaces = g_realloc(desktop->priv->icon_workspaces,
                                               sizeof(XfceDesktopIconWorkspace *) * n_ws);
}

static gboolean
grid_is_free_position(XfceDesktopIconWorkspace *icon_workspace,
                      guint16 row,
                      guint16 col)
{
    guint abit = col * icon_workspace->nrows + row;
    guint8 abyte = icon_workspace->grid_layout[abit/8];
    gboolean is_set = abyte & (0x80 >> (abit % 8));
    
    return !is_set;
}

static gboolean
grid_get_next_free_position(XfceDesktopIconWorkspace *icon_workspace,
                            guint16 *row,
                            guint16 *col)
{
    gint i, maxi, k;
    guint8 j;
    guint8 *grid_layout = icon_workspace->grid_layout;
    
    maxi = icon_workspace->nrows * icon_workspace->ncols + 1;
    for(i = 0; i < maxi; ++i) {
       if(grid_layout[i] != 0xff) {
           for(j = 0x80, k = 0; j; j >>= 1, ++k) {
               if(~grid_layout[i] & j) {
                   guint abit = i * 8 + k;
                   *row = abit % icon_workspace->nrows;
                   *col = abit / icon_workspace->nrows;
                   return TRUE;
               }
           }
       }
   }
   
   return FALSE;
}

static void
grid_set_position_free(XfceDesktopIconWorkspace *icon_workspace,
                       guint16 row,
                       guint16 col)
{
    guint8 *grid_layout = icon_workspace->grid_layout;
    guint abit = col * icon_workspace->nrows + row;
    guint abyte = abit / 8;
    guint8 mask = ~(0x80 >> (abit % 8));
    dump_grid_layout(icon_workspace);
    grid_layout[abyte] &= mask;
    dump_grid_layout(icon_workspace);
}

static void
grid_unset_position_free(XfceDesktopIconWorkspace *icon_workspace,
                         guint16 row,
                         guint16 col)
{
    guint8 *grid_layout = icon_workspace->grid_layout;
    guint abit = col * icon_workspace->nrows + row;
    guint abyte = abit / 8;
    guint8 mask = 0x80 >> (abit % 8);
    dump_grid_layout(icon_workspace);
    grid_layout[abyte] |= mask;
    dump_grid_layout(icon_workspace);
}

static void
window_state_changed_cb(NetkWindow *window,
                        NetkWindowState changed_mask,
                        NetkWindowState new_state,
                        gpointer user_data)
{
    XfceDesktop *desktop = user_data;
    NetkWorkspace *ws;
    gint ws_num = -1, i, max_i;
    gboolean is_add = FALSE;
    XfceDesktopIcon *icon;
    
    TRACE("entering");
    
    if(!(changed_mask & (NETK_WINDOW_STATE_MINIMIZED |
                         NETK_WINDOW_STATE_SKIP_TASKLIST)))
    {
        return;
    }
    
    ws = netk_window_get_workspace(window);
    if(ws)
        ws_num = netk_workspace_get_number(ws);
    
    if(   (changed_mask & NETK_WINDOW_STATE_MINIMIZED
           && new_state & NETK_WINDOW_STATE_MINIMIZED)
       || (changed_mask & NETK_WINDOW_STATE_SKIP_TASKLIST
           && !(new_state & NETK_WINDOW_STATE_SKIP_TASKLIST)))
    {
        is_add = TRUE;
    }
    
    /* this is a cute way of handling adding/removing from *all* workspaces
     * when we're dealing with a sticky windows, and just adding/removing
     * from a single workspace otherwise, without duplicating code */
    if(netk_window_is_pinned(window)) {
        i = 0;
        max_i = netk_screen_get_workspace_count(desktop->priv->netk_screen);
    } else {
        g_return_if_fail(ws_num != -1);
        i = ws_num;
        max_i = i + 1;
    }
    
    if(is_add) {
        for(; i < max_i; i++) {
            if(!desktop->priv->icon_workspaces[i]->icons
               || g_hash_table_lookup(desktop->priv->icon_workspaces[i]->icons,
                                      window))
            {
                continue;
            }
            
            xfce_desktop_icon_add(desktop, window, i);
        }
    } else {
        for(; i < max_i; i++) {
            if(!desktop->priv->icon_workspaces[i]->icons)
                continue;
            
            icon = g_hash_table_lookup(desktop->priv->icon_workspaces[i]->icons,
                                       window);
            if(icon)
                xfce_desktop_icon_remove(desktop, icon, window, i);
        }
    }
}

static void
window_workspace_changed_cb(NetkWindow *window,
                            gpointer user_data)
{
    XfceDesktop *desktop = user_data;
    NetkWorkspace *new_ws;
    gint i, new_ws_num = -1, n_ws;
    XfceDesktopIcon *icon;
    
    TRACE("entering");
    
    if(!netk_window_is_minimized(window))
        return;
    
    n_ws = netk_screen_get_workspace_count(desktop->priv->netk_screen);
    
    new_ws = netk_window_get_workspace(window);
    if(new_ws)
        new_ws_num = netk_workspace_get_number(new_ws);
    
    for(i = 0; i < n_ws; i++) {
        if(!desktop->priv->icon_workspaces[i]->icons)
            continue;
        
        icon = g_hash_table_lookup(desktop->priv->icon_workspaces[i]->icons,
                                   window);
        
        if(new_ws) {
            /* window is not sticky */
            if(i != new_ws_num && icon)
                xfce_desktop_icon_remove(desktop, icon, window, i);
            else if(i == new_ws_num && !icon)
                xfce_desktop_icon_add(desktop, window, i);
        } else {
            /* window is sticky */
            if(!icon)
                xfce_desktop_icon_add(desktop, window, i);
        }
    }
}

static void
window_destroyed_cb(gpointer data,
                    GObject *where_the_object_was)
{
    XfceDesktop *desktop = data;
    NetkWindow *window = (NetkWindow *)where_the_object_was;
    gint nws, i;
    XfceDesktopIcon *icon;
    
    nws = netk_screen_get_workspace_count(desktop->priv->netk_screen);
    for(i = 0; i < nws; i++) {
        if(!desktop->priv->icon_workspaces[i]->icons)
            continue;
        
        icon = g_hash_table_lookup(desktop->priv->icon_workspaces[i]->icons,
                                   window);
        if(icon)
            xfce_desktop_icon_remove(desktop, icon, window, i);
    }
}

static void
window_created_cb(NetkScreen *netk_screen,
                  NetkWindow *window,
                  gpointer user_data)
{
    XfceDesktop *desktop = user_data;
    
    g_signal_connect(G_OBJECT(window), "state-changed",
                     G_CALLBACK(window_state_changed_cb), desktop);
    g_signal_connect(G_OBJECT(window), "workspace-changed",
                     G_CALLBACK(window_workspace_changed_cb), desktop);
    g_object_weak_ref(G_OBJECT(window), window_destroyed_cb, desktop);
}

#endif  /* defined(ENABLE_WINDOW_ICONS) */

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
    GtkWidgetClass *widget_class;
    
    gobject_class = (GObjectClass *)klass;
    widget_class = (GtkWidgetClass *)klass;
    
    parent_class = g_type_class_peek_parent(klass);
    
    gobject_class->finalize = xfce_desktop_finalize;
    
    widget_class->realize = xfce_desktop_realize;
    widget_class->unrealize = xfce_desktop_unrealize;
#ifdef ENABLE_WINDOW_ICONS
    widget_class->button_press_event = xfce_desktop_button_press;
    widget_class->button_release_event = xfce_desktop_button_release;
    widget_class->motion_notify_event = xfce_desktop_motion_notify;
    widget_class->drag_begin = xfce_desktop_drag_begin;
    widget_class->drag_motion = xfce_desktop_drag_motion;
    widget_class->drag_leave = xfce_desktop_drag_leave;
    widget_class->drag_drop = xfce_desktop_drag_drop;
#endif
}

static void
xfce_desktop_init(XfceDesktop *desktop)
{
    desktop->priv = g_new0(XfceDesktopPriv, 1);
    GTK_WINDOW(desktop)->type = GTK_WINDOW_TOPLEVEL;
    
    gtk_widget_add_events(GTK_WIDGET(desktop), GDK_EXPOSURE_MASK);
    gtk_window_set_type_hint(GTK_WINDOW(desktop), GDK_WINDOW_TYPE_HINT_DESKTOP);
    gtk_window_set_accept_focus(GTK_WINDOW(desktop), FALSE);
    
#ifdef ENABLE_WINDOW_ICONS
    gtk_widget_add_events(GTK_WIDGET(desktop),
                          GDK_BUTTON_PRESS_MASK
                          | GDK_BUTTON_RELEASE_MASK
                          | GDK_POINTER_MOTION_MASK);
    desktop->priv->source_targets = gtk_target_list_new(targets, n_targets);
    gtk_drag_dest_set(GTK_WIDGET(desktop), 0, targets, n_targets, GDK_ACTION_MOVE); 
#endif
}

static void
xfce_desktop_finalize(GObject *object)
{
    XfceDesktop *desktop = XFCE_DESKTOP(object);
    
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    
#ifdef ENABLE_WINDOW_ICONS
    gtk_target_list_unref(desktop->priv->source_targets);
#endif
    
    g_free(desktop->priv);
    desktop->priv = NULL;
    
    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
xfce_desktop_realize(GtkWidget *widget)
{
    XfceDesktop *desktop = XFCE_DESKTOP(widget);
    GdkAtom atom;
    gint i;
    Window xid;
    GdkDisplay *gdpy;
    GdkWindow *groot;
    GdkVisual *visual;
    
    gtk_window_set_screen(GTK_WINDOW(desktop), desktop->priv->gscreen);
    
    /* chain up */
    GTK_WIDGET_CLASS(parent_class)->realize(widget);
    
    gtk_window_set_title(GTK_WINDOW(desktop), _("Desktop"));
    if(GTK_WIDGET_DOUBLE_BUFFERED(GTK_WIDGET(desktop)))
        gtk_widget_set_double_buffered(GTK_WIDGET(desktop), FALSE);
    
    gtk_widget_set_size_request(GTK_WIDGET(desktop),
                      gdk_screen_get_width(desktop->priv->gscreen),
                      gdk_screen_get_height(desktop->priv->gscreen));
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
    
    if(desktop->priv->mcs_client)
        load_initial_settings(desktop, desktop->priv->mcs_client);
    
    for(i = 0; i < desktop->priv->nbackdrops; i++) {
        g_signal_connect(G_OBJECT(desktop->priv->backdrops[i]), "changed",
                G_CALLBACK(backdrop_changed_cb), desktop);
        backdrop_changed_cb(desktop->priv->backdrops[i], desktop);
    }
    
    g_signal_connect(G_OBJECT(desktop->priv->gscreen), "size-changed",
            G_CALLBACK(screen_size_changed_cb), desktop);
    
    gtk_widget_add_events(GTK_WIDGET(desktop), GDK_EXPOSURE_MASK);
    g_signal_connect(G_OBJECT(desktop), "expose-event",
            G_CALLBACK(xfce_desktop_expose), NULL);
    
    g_signal_connect(G_OBJECT(desktop), "style-set",
            G_CALLBACK(desktop_style_set_cb), NULL);
    
#ifdef ENABLE_WINDOW_ICONS
    if(desktop->priv->use_window_icons)
        xfce_desktop_setup_icons(desktop);
#endif
}

static void
xfce_desktop_unrealize(GtkWidget *widget)
{
    XfceDesktop *desktop = XFCE_DESKTOP(widget);
    gint i;
    GdkWindow *groot;
    gchar property_name[128];
    GdkColor c;
    
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    
    if(GTK_WIDGET_MAPPED(widget))
        gtk_widget_unmap(widget);
    GTK_WIDGET_UNSET_FLAGS(widget, GTK_MAPPED);
    
#ifdef ENABLE_WINDOW_ICONS
    if(desktop->priv->use_window_icons)
        xfce_desktop_unsetup_icons(desktop);
#endif
    
    gtk_container_forall(GTK_CONTAINER(widget),
                         (GtkCallback)gtk_widget_unrealize,
                         NULL);
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(desktop),
            G_CALLBACK(desktop_style_set_cb), NULL);
    g_signal_handlers_disconnect_by_func(G_OBJECT(desktop),
            G_CALLBACK(xfce_desktop_expose), NULL);
    g_signal_handlers_disconnect_by_func(G_OBJECT(desktop->priv->gscreen),
            G_CALLBACK(screen_size_changed_cb), desktop);
    
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
    
    if(desktop->priv->bg_pixmap) {
        g_object_unref(G_OBJECT(desktop->priv->bg_pixmap));
        desktop->priv->bg_pixmap = NULL;
    }
    
    gtk_window_set_icon(GTK_WINDOW(widget), NULL);
    
    gtk_style_detach(widget->style);
    g_object_unref(G_OBJECT(widget->window));
    widget->window = NULL;
    
    gtk_selection_remove_all(widget);
    
    /* blank out the root window */
    gdk_window_set_back_pixmap(groot, NULL, FALSE);
    c.red = c.blue = c.green = 0;
    gdk_window_set_background(groot, &c);
    gdk_window_clear(groot);
    GdkRectangle rect;
    rect.x = rect.y = 0;
    gdk_drawable_get_size(GDK_DRAWABLE(groot), &rect.x, &rect.y);
    gdk_window_invalidate_rect(groot, &rect, FALSE);
    
    GTK_WIDGET_UNSET_FLAGS(widget, GTK_REALIZED);
}

#ifdef ENABLE_WINDOW_ICONS

static inline gboolean
xfce_desktop_rectangle_contains_point(GdkRectangle *rect, gint x, gint y)
{
    if(x > rect->x + rect->width
            || x < rect->x
            || y > rect->y + rect->height
            || y < rect->y)
    {
        return FALSE;
    }
    
    return TRUE;
}

static gboolean
check_icon_clicked(gpointer key,
                   gpointer value,
                   gpointer user_data)
{
    XfceDesktopIcon *icon = value;
    GdkEventButton *evt = user_data;
    
    if(xfce_desktop_rectangle_contains_point(&icon->extents, evt->x, evt->y))
        return TRUE;
    else
        return FALSE;
}

static gboolean
action_menu_destroy_idled(gpointer data)
{
    gtk_widget_destroy(GTK_WIDGET(data));
    return FALSE;
}

static void
action_menu_deactivate_cb(GtkMenu *menu, gpointer user_data)
{
    g_idle_add(action_menu_destroy_idled, menu);
}

static gboolean
xfce_desktop_button_press(GtkWidget *widget,
                          GdkEventButton *evt)
{
    XfceDesktop *desktop = XFCE_DESKTOP(widget);
    XfceDesktopPriv *priv = desktop->priv;
    XfceDesktopIcon *icon;
    gint cur_ws_num = desktop->priv->cur_ws_num;
    
    TRACE("entering, type is %s", evt->type == GDK_BUTTON_PRESS ? "GDK_BUTTON_PRESS" : (evt->type == GDK_2BUTTON_PRESS ? "GDK_2BUTTON_PRESS" : "i dunno"));
    
    if(!desktop->priv->use_window_icons)
        return FALSE;
    
    if(evt->type == GDK_BUTTON_PRESS) {
        g_return_val_if_fail(desktop->priv->icon_workspaces[cur_ws_num]->icons,
                             FALSE);
        
        icon = g_hash_table_find(desktop->priv->icon_workspaces[cur_ws_num]->icons,
                                 check_icon_clicked, evt);
        if(icon) {
            /* check old selected icon, paint it as normal */
            if(priv->icon_workspaces[priv->cur_ws_num]->selected_icon) {
                XfceDesktopIcon *old_sel = priv->icon_workspaces[priv->cur_ws_num]->selected_icon;
                priv->icon_workspaces[priv->cur_ws_num]->selected_icon = NULL;
                xfce_desktop_icon_paint(old_sel);
            }
            
            priv->icon_workspaces[priv->cur_ws_num]->selected_icon = icon;
            xfce_desktop_icon_paint(icon);
            priv->last_clicked_item = icon;
            
            if(evt->button == 1) {
                /* we might be the start of a drag */
                DBG("setting stuff");
                priv->maybe_begin_drag = TRUE;
                priv->definitely_dragging = FALSE;
                priv->press_start_x = evt->x;
                priv->press_start_y = evt->y;
                
                return TRUE;
            } else if(evt->button == 3) {
                /* if we're a right click, pop up a window menu and eat
                 * the event */
                GtkWidget *menu = netk_create_window_action_menu(icon->window);
                gtk_widget_show(menu);
                g_signal_connect(G_OBJECT(menu), "deactivate",
                                 G_CALLBACK(action_menu_deactivate_cb), NULL);
                gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                               evt->button, evt->time);
                
                return TRUE;
            }
        } else {
            /* unselect previously selected icon if we didn't click one */
            XfceDesktopIcon *old_sel = desktop->priv->icon_workspaces[cur_ws_num]->selected_icon;
            if(old_sel) {
                desktop->priv->icon_workspaces[cur_ws_num]->selected_icon = NULL;
                xfce_desktop_icon_paint(old_sel);
            }
            desktop->priv->last_clicked_item = NULL;
        }
    } else if(evt->type == GDK_2BUTTON_PRESS) {
        g_return_val_if_fail(desktop->priv->icon_workspaces[cur_ws_num]->icons,
                             FALSE);
        
        icon = g_hash_table_find(desktop->priv->icon_workspaces[cur_ws_num]->icons,
                                 check_icon_clicked, evt);
        if(icon) {
            XfceDesktopIcon *icon_below = NULL;
            
            if(icon->extents.height + 3 * CELL_PADDING > CELL_SIZE)
                icon_below = find_icon_below(desktop, icon);
            netk_window_activate(icon->window);
            desktop->priv->icon_workspaces[cur_ws_num]->selected_icon = NULL;
            if(icon_below) {
                /* delay repaint of below icon to avoid visual bugs */
                g_object_ref(G_OBJECT(icon->window));
                g_signal_connect_after(G_OBJECT(icon->window), "state-changed",
                                       G_CALLBACK(xfce_desktop_icon_paint_delayed),
                                       icon_below);
            }
        }
    }
    
    return FALSE;
}

static gboolean
xfce_desktop_button_release(GtkWidget *widget,
                            GdkEventButton *evt)
{
    XfceDesktop *desktop = XFCE_DESKTOP(widget);
    
    TRACE("entering btn=%d", evt->button);
    
    if(evt->button == 1) {
        DBG("unsetting stuff");
        desktop->priv->definitely_dragging = FALSE;
        desktop->priv->maybe_begin_drag = FALSE;
    }
    
    return FALSE;
}

#endif

static gboolean
xfce_desktop_expose(GtkWidget *w,
                    GdkEventExpose *evt,
                    gpointer user_data)
{
    TRACE("entering");
    
    if(evt->count != 0)
        return FALSE;
    
    gdk_window_clear_area(w->window, evt->area.x, evt->area.y,
            evt->area.width, evt->area.height);
    
#ifdef ENABLE_WINDOW_ICONS
    if(XFCE_DESKTOP(w)->priv->use_window_icons)
        xfce_desktop_paint_icons(XFCE_DESKTOP(w), &evt->area);
#endif
    
    return TRUE;
}

#ifdef ENABLE_WINDOW_ICONS

static void
check_icon_needs_repaint(gpointer key,
                         gpointer value,
                         gpointer user_data)
{
    XfceDesktopIcon *icon = (XfceDesktopIcon *)value;
    GdkRectangle *area = user_data, dummy;
    
    if(icon->extents.width == 0 || icon->extents.height == 0
       || gdk_rectangle_intersect(area, &icon->extents, &dummy))
    {
        if(icon == icon->desktop->priv->icon_workspaces[icon->desktop->priv->cur_ws_num]->selected_icon) {
            /* save it for last to avoid painting another icon over top of
             * part of this one if it has an overly-large label */
            if(icon->desktop->priv->icon_repaint_id)
                g_source_remove(icon->desktop->priv->icon_repaint_id);
            icon->desktop->priv->icon_repaint_id = g_idle_add(xfce_desktop_icon_paint_idled,
                                                              icon);
        } else
            xfce_desktop_icon_paint(icon);
    }
}

static void
xfce_desktop_paint_icons(XfceDesktop *desktop, GdkRectangle *area)
{
    TRACE("entering");
    
    g_return_if_fail(desktop->priv->icon_workspaces[desktop->priv->cur_ws_num]->icons);
    
    g_hash_table_foreach(desktop->priv->icon_workspaces[desktop->priv->cur_ws_num]->icons,
                         check_icon_needs_repaint, area);
}

static void
desktop_setup_grids(XfceDesktop *desktop,
                    gint nws)
{
    gint i, *xorigins, *yorigins, *widths, *heights, tmp;
    
    xorigins = g_new(gint, nws);
    yorigins = g_new(gint, nws);
    widths = g_new(gint, nws);
    heights = g_new(gint, nws);
    
    if(desktop_get_workarea(desktop, nws, xorigins, yorigins, widths, heights)) {
        for(i = 0; i < nws; i++) {
            if(G_LIKELY(!desktop->priv->icon_workspaces[i]))
                desktop->priv->icon_workspaces[i] = g_new0(XfceDesktopIconWorkspace, 1);
            
            desktop->priv->icon_workspaces[i]->xorigin = xorigins[i];
            desktop->priv->icon_workspaces[i]->yorigin = yorigins[i];
            desktop->priv->icon_workspaces[i]->width = widths[i];
            desktop->priv->icon_workspaces[i]->height = heights[i];
            
            desktop->priv->icon_workspaces[i]->nrows = heights[i] / CELL_SIZE;
            desktop->priv->icon_workspaces[i]->ncols = widths[i] / CELL_SIZE;
            
            tmp = 1 + desktop->priv->icon_workspaces[i]->nrows
                  * desktop->priv->icon_workspaces[i]->ncols / 8;
            
            if(G_UNLIKELY(desktop->priv->icon_workspaces[i]->grid_layout)) {
                desktop->priv->icon_workspaces[i]->grid_layout = g_realloc(
                    desktop->priv->icon_workspaces[i]->grid_layout,
                    tmp
                );
            } else
                desktop->priv->icon_workspaces[i]->grid_layout = g_malloc0(tmp);
            
            DBG("created grid_layout with %d bytes (%d positions)", tmp,
                desktop->priv->icon_workspaces[i]->nrows
                * desktop->priv->icon_workspaces[i]->ncols);
            dump_grid_layout(desktop->priv->icon_workspaces[i]);
        }
    } else {
        gint w = gdk_screen_get_width(desktop->priv->gscreen);
        gint h = gdk_screen_get_height(desktop->priv->gscreen);
        for(i = 0; i < nws; i++) {
            if(G_LIKELY(!desktop->priv->icon_workspaces[i]))
                desktop->priv->icon_workspaces[i] = g_new0(XfceDesktopIconWorkspace, 1);
            
            desktop->priv->icon_workspaces[i]->xorigin = 0;
            desktop->priv->icon_workspaces[i]->yorigin = 0;
            desktop->priv->icon_workspaces[i]->width = w;
            desktop->priv->icon_workspaces[i]->height = h;
            
            desktop->priv->icon_workspaces[i]->nrows = h / CELL_SIZE;
            desktop->priv->icon_workspaces[i]->ncols = w / CELL_SIZE;
            
            tmp = 1 + desktop->priv->icon_workspaces[i]->nrows
                  * desktop->priv->icon_workspaces[i]->ncols / 8;
            
            if(G_UNLIKELY(desktop->priv->icon_workspaces[i]->grid_layout)) {
                desktop->priv->icon_workspaces[i]->grid_layout = g_realloc(
                    desktop->priv->icon_workspaces[i]->grid_layout,
                    tmp
                );
            } else
                desktop->priv->icon_workspaces[i]->grid_layout = g_malloc0(tmp);
            
            DBG("created grid_layout with %d bytes (%d positions)", tmp,
                desktop->priv->icon_workspaces[i]->nrows
                * desktop->priv->icon_workspaces[i]->ncols);
            dump_grid_layout(desktop->priv->icon_workspaces[i]);
        }
    }
    
    g_free(xorigins);
    g_free(yorigins);
    g_free(widths);
    g_free(heights);
}

static GdkFilterReturn
desktop_rootwin_watch_workarea(GdkXEvent *gxevent,
                               GdkEvent *event,
                               gpointer user_data)
{
    XfceDesktop *desktop = user_data;
    XPropertyEvent *xevt = (XPropertyEvent *)gxevent;
    
    if(xevt->type == PropertyNotify
       && XInternAtom(GDK_DISPLAY(), "_NET_WORKAREA", False) == xevt->atom)
    {
        DBG("got _NET_WORKAREA change on rootwin!");
        if(desktop->priv->grid_resize_timeout) {
            g_source_remove(desktop->priv->grid_resize_timeout);
            desktop->priv->grid_resize_timeout = 0;
        }
        desktop_grid_do_resize(desktop);
    }
    
    return GDK_FILTER_CONTINUE;
}

static void
xfce_desktop_setup_icons(XfceDesktop *desktop)
{
    PangoContext *pctx;
    GList *windows, *l;
    gint nws;
    GdkWindow *groot;
    
    if(desktop->priv->icon_workspaces)
        return;
    
    if(!desktop->priv->netk_screen) {
        gint screen = gdk_screen_get_number(desktop->priv->gscreen);
        desktop->priv->netk_screen = netk_screen_get(screen);
    }
    netk_screen_force_update(desktop->priv->netk_screen);
    g_signal_connect(G_OBJECT(desktop->priv->netk_screen),
                     "active-workspace-changed",
                     G_CALLBACK(workspace_changed_cb), desktop);
    g_signal_connect(G_OBJECT(desktop->priv->netk_screen), "window-opened",
                     G_CALLBACK(window_created_cb), desktop);
    g_signal_connect(G_OBJECT(desktop->priv->netk_screen), "workspace-created",
                     G_CALLBACK(workspace_created_cb), desktop);
    g_signal_connect(G_OBJECT(desktop->priv->netk_screen),
                     "workspace-destroyed",
                     G_CALLBACK(workspace_destroyed_cb), desktop);
    
    nws = netk_screen_get_workspace_count(desktop->priv->netk_screen);
    desktop->priv->icon_workspaces = g_new0(XfceDesktopIconWorkspace *, nws);
    desktop_setup_grids(desktop, nws);
    
    pctx = gtk_widget_get_pango_context(GTK_WIDGET(desktop));
    desktop->priv->playout = pango_layout_new(pctx);
    
    windows = netk_screen_get_windows(desktop->priv->netk_screen);
    for(l = windows; l; l = l->next) {
        NetkWindow *window = l->data;
        
        g_signal_connect(G_OBJECT(window), "state-changed",
                         G_CALLBACK(window_state_changed_cb), desktop);
        g_signal_connect(G_OBJECT(window), "workspace-changed",
                         G_CALLBACK(window_workspace_changed_cb), desktop);
        g_object_weak_ref(G_OBJECT(window), window_destroyed_cb, desktop);
    }
    
    workspace_changed_cb(desktop->priv->netk_screen, desktop);
    
    /* not sure why i need this, since it's also in _init()... */
    gtk_widget_add_events(GTK_WIDGET(desktop), GDK_POINTER_MOTION_MASK
                          | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
    
    /* watch for _NET_WORKAREA changes */
    groot = gdk_screen_get_root_window(desktop->priv->gscreen);
    gdk_window_set_events(groot, gdk_window_get_events(groot)
                                 | GDK_PROPERTY_CHANGE_MASK);
    gdk_window_add_filter(groot, desktop_rootwin_watch_workarea, desktop);
}

static void
xfce_desktop_unsetup_icons(XfceDesktop *desktop)
{
    GList *windows, *l;
    gint nws, i;
    GdkWindow *groot;
    
    if(!desktop->priv->icon_workspaces)
        return;
    
    groot = gdk_screen_get_root_window(desktop->priv->gscreen);
    gdk_window_remove_filter(groot, desktop_rootwin_watch_workarea, desktop);
    
    if(desktop->priv->grid_resize_timeout) {
        g_source_remove(desktop->priv->grid_resize_timeout);
        desktop->priv->grid_resize_timeout = 0;
    }
    
    if(desktop->priv->icon_repaint_id) {
        g_source_remove(desktop->priv->icon_repaint_id);
        desktop->priv->icon_repaint_id = 0;
    }
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(desktop->priv->netk_screen),
                                         G_CALLBACK(workspace_changed_cb),
                                         desktop);
    g_signal_handlers_disconnect_by_func(G_OBJECT(desktop->priv->netk_screen),
                                         G_CALLBACK(window_created_cb),
                                         desktop);
    g_signal_handlers_disconnect_by_func(G_OBJECT(desktop->priv->netk_screen),
                                         G_CALLBACK(workspace_created_cb),
                                         desktop);
    g_signal_handlers_disconnect_by_func(G_OBJECT(desktop->priv->netk_screen),
                                         G_CALLBACK(workspace_destroyed_cb),
                                         desktop);
    
    windows = netk_screen_get_windows(desktop->priv->netk_screen);
    for(l = windows; l; l = l->next) {
        g_signal_handlers_disconnect_by_func(G_OBJECT(l->data),
                                             G_CALLBACK(window_state_changed_cb),
                                             desktop);
        g_signal_handlers_disconnect_by_func(G_OBJECT(l->data),
                                             G_CALLBACK(window_workspace_changed_cb),
                                             desktop);
        g_object_weak_unref(G_OBJECT(l->data), window_destroyed_cb, desktop);
    }
    
    nws = netk_screen_get_workspace_count(desktop->priv->netk_screen);
    for(i = 0; i < nws; i++) {
        if(desktop->priv->icon_workspaces[i]->icons)
            g_hash_table_destroy(desktop->priv->icon_workspaces[i]->icons);
        g_free(desktop->priv->icon_workspaces[i]->grid_layout);
        g_free(desktop->priv->icon_workspaces[i]);
    }
    g_free(desktop->priv->icon_workspaces);
    desktop->priv->icon_workspaces = NULL;
    
    g_object_unref(G_OBJECT(desktop->priv->playout));
    desktop->priv->playout = NULL;
}

static gboolean
xfce_desktop_motion_notify(GtkWidget *widget,
                           GdkEventMotion *evt)
{
    XfceDesktop *desktop = XFCE_DESKTOP(widget);
    XfceDesktopPriv *priv = desktop->priv;
    gboolean ret = FALSE;
    
    if(priv->maybe_begin_drag && !priv->definitely_dragging) {
        priv->definitely_dragging = xfce_desktop_maybe_begin_drag(desktop, evt);
        if(priv->definitely_dragging)
            ret = TRUE;
    }
    
    return ret;
}

static gboolean
xfce_desktop_maybe_begin_drag(XfceDesktop *desktop,
                              GdkEventMotion *evt)
{
    GdkDragContext *context;
    
    /* sanity check */
    g_return_val_if_fail(desktop->priv->last_clicked_item, FALSE);
    
    if(!gtk_drag_check_threshold(GTK_WIDGET(desktop),
                                 desktop->priv->press_start_x,
                                 desktop->priv->press_start_y,
                                 evt->x, evt->y))
    {
        return FALSE;
    }
    
    context = gtk_drag_begin(GTK_WIDGET(desktop),
                             desktop->priv->source_targets,
                             GDK_ACTION_MOVE,
                             1, (GdkEvent *)evt);
    
    DBG("DRAG BEGIN!");
    
    return TRUE;
}

static void
xfce_desktop_drag_begin(GtkWidget *widget,
                        GdkDragContext *context)
{
    XfceDesktop *desktop = XFCE_DESKTOP(widget);
    XfceDesktopIcon *icon;
    gint x, y;
    
    icon = desktop->priv->last_clicked_item;
    g_return_if_fail(icon);
    
    x = desktop->priv->press_start_x - icon->extents.x + 1;
    y = desktop->priv->press_start_y - icon->extents.y + 1;
    
    gtk_drag_set_icon_pixbuf(context, icon->pix, x, y);
}

static inline void
desktop_xy_to_rowcol(XfceDesktop *desktop,
                     gint idx,
                     gint x,
                     gint y,
                     gint *row,
                     gint *col)
{
    g_return_if_fail(row && col);
    
    *row = y - desktop->priv->icon_workspaces[idx]->yorigin - CELL_PADDING;
    *row /= CELL_SIZE;
    
    *col = x - desktop->priv->icon_workspaces[idx]->xorigin - CELL_PADDING;
    *col /= CELL_SIZE;
}

static gboolean
xfce_desktop_drag_motion(GtkWidget *widget,
                         GdkDragContext *context,
                         gint x,
                         gint y,
                         guint time)
{
    XfceDesktop *desktop = XFCE_DESKTOP(widget);
    GdkAtom target = GDK_NONE;
    NetkWorkspace *active_ws;
    gint active_ws_num, row, col;
    GdkRectangle *cell_highlight;
    
    /*TRACE("entering: (%d,%d)", x, y);*/
    
    target = gtk_drag_dest_find_target(widget, context,
                                       desktop->priv->source_targets);
    if(target == GDK_NONE)
        return FALSE;
    
    gdk_drag_status(context, GDK_ACTION_MOVE, time);
    
    active_ws = netk_screen_get_active_workspace(desktop->priv->netk_screen);
    active_ws_num = netk_workspace_get_number(active_ws);
    
    cell_highlight = g_object_get_data(G_OBJECT(context),
                                       "xfce-desktop-cell-highlight");
    
    desktop_xy_to_rowcol(desktop, active_ws_num, x, y, &row, &col);
    if(row < desktop->priv->icon_workspaces[active_ws_num]->nrows
       && col < desktop->priv->icon_workspaces[active_ws_num]->ncols
       && grid_is_free_position(desktop->priv->icon_workspaces[active_ws_num],
                                row, col))
    {
        gint newx, newy;
        
        newx = desktop->priv->icon_workspaces[active_ws_num]->xorigin;
        newx += col * CELL_SIZE + CELL_PADDING;
        newy = desktop->priv->icon_workspaces[active_ws_num]->yorigin;
        newy += row * CELL_SIZE + CELL_PADDING;
        
        if(cell_highlight) {
            DBG("have old cell higlight: (%d,%d)", cell_highlight->x, cell_highlight->y);
            if(cell_highlight->x != newx || cell_highlight->y != newy) {
                gdk_window_clear_area(widget->window,
                                      cell_highlight->x,
                                      cell_highlight->y,
                                      cell_highlight->width + 1,
                                      cell_highlight->height + 1);
            }
        } else {
            cell_highlight = g_new0(GdkRectangle, 1);
            g_object_set_data_full(G_OBJECT(context),
                                   "xfce-desktop-cell-highlight",
                                   cell_highlight, (GDestroyNotify)g_free);
        }
        
        cell_highlight->x = newx;
        cell_highlight->y = newy;
        cell_highlight->width = cell_highlight->height = CELL_SIZE;
        
        DBG("painting highlight: (%d,%d)", newx, newy);
        
        gdk_draw_rectangle(GDK_DRAWABLE(widget->window),
                           widget->style->bg_gc[GTK_STATE_SELECTED], FALSE,
                           newx, newy, CELL_SIZE, CELL_SIZE);
        
        return TRUE;
    } else {
        if(cell_highlight) {
            gdk_window_clear_area(widget->window,
                                  cell_highlight->x,
                                  cell_highlight->y,
                                  cell_highlight->width + 1,
                                  cell_highlight->height + 1);
        }
        return FALSE;
    }
}

static void
xfce_desktop_drag_leave(GtkWidget *widget,
                        GdkDragContext *context,
                        guint time)
{
    GdkRectangle *cell_highlight = g_object_get_data(G_OBJECT(context),
                                                     "xfce-desktop-cell-highlight");
    if(cell_highlight) {
        gdk_window_clear_area(widget->window,
                              cell_highlight->x,
                              cell_highlight->y,
                              cell_highlight->width + 1,
                              cell_highlight->height + 1);
    }
}

static gboolean
xfce_desktop_drag_drop(GtkWidget *widget,
                       GdkDragContext *context,
                       gint x,
                       gint y,
                       guint time)
{
    XfceDesktop *desktop = XFCE_DESKTOP(widget);
    GdkAtom target = GDK_NONE;
    XfceDesktopIcon *icon;
    NetkWorkspace *active_ws;
    gint active_ws_num, row, col, cell_x, cell_y;
    
    TRACE("entering: (%d,%d)", x, y);
    
    target = gtk_drag_dest_find_target(widget, context,
                                       desktop->priv->source_targets);
    if(target == GDK_NONE)
        return FALSE;
        
    active_ws = netk_screen_get_active_workspace(desktop->priv->netk_screen);
    active_ws_num = netk_workspace_get_number(active_ws);
    
    desktop_xy_to_rowcol(desktop, active_ws_num, x, y, &row, &col);
    if(row >= desktop->priv->icon_workspaces[active_ws_num]->nrows
       || col >= desktop->priv->icon_workspaces[active_ws_num]->ncols
       || !grid_is_free_position(desktop->priv->icon_workspaces[active_ws_num],
                                 row, col))
    {
        gtk_drag_finish(context, FALSE, FALSE, time);
        return FALSE;
    }
    
    /* clear highlight box */
    cell_x = desktop->priv->icon_workspaces[active_ws_num]->xorigin;
    cell_x += col * CELL_SIZE + CELL_PADDING;
    cell_y = desktop->priv->icon_workspaces[active_ws_num]->yorigin;
    cell_y += row * CELL_SIZE + CELL_PADDING;
    gdk_window_clear_area(widget->window, cell_x, cell_y,
                          CELL_SIZE + 1, CELL_SIZE + 1);
    
    icon = desktop->priv->last_clicked_item;
    g_return_val_if_fail(icon, FALSE);
    
    grid_set_position_free(desktop->priv->icon_workspaces[active_ws_num],
                           icon->row, icon->col);
    grid_unset_position_free(desktop->priv->icon_workspaces[active_ws_num],
                             row, col);
    
    /* set new position */
    icon->row = row;
    icon->col = col;
    
    /* clear out old position */
    gdk_window_clear_area(widget->window, icon->extents.x, icon->extents.y,
                          icon->extents.width, icon->extents.height);
    icon->extents.x = icon->extents.y = 0;
    xfce_desktop_icon_paint(icon);
    
    DBG("drag succeeded");
    
    gtk_drag_finish(context, TRUE, FALSE, time);
    
    return TRUE;
}

#endif  /* defined(ENABLE_WINDOW_ICONS) */


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
    
    if(!gscreen)
        gscreen = gdk_display_get_default_screen(gdk_display_get_default());
    desktop->priv->gscreen = gscreen;
    
    desktop->priv->mcs_client = mcs_client;
    
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
    
    if(!strcmp(setting->name, "xineramastretch")) {
        if(setting->data.v_int && desktop->priv->nbackdrops > 1) {
            handle_xinerama_stretch(desktop);
            backdrop_changed_cb(desktop->priv->backdrops[0], desktop);
        } else if(!setting->data.v_int)
            handle_xinerama_unstretch(desktop);
        return TRUE;
    }
    
#ifdef ENABLE_WINDOW_ICONS
    if(!strcmp(setting->name, "usewindowicons")) {
        if(setting->data.v_int) {
            desktop->priv->use_window_icons = TRUE;
            if(GTK_WIDGET_REALIZED(GTK_WIDGET(desktop)))
                xfce_desktop_setup_icons(desktop);
        } else {
            desktop->priv->use_window_icons = FALSE;
            if(GTK_WIDGET_REALIZED(GTK_WIDGET(desktop))) {
                xfce_desktop_unsetup_icons(desktop);
                gtk_widget_queue_draw(GTK_WIDGET(desktop));
            }
        }
        return TRUE;
    }
#endif
    
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
