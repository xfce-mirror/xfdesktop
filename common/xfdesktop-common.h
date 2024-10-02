/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004 Brian Tarricone, <brian@tarricone.org>
 *  Copyright (c) 2010 Jannis Pohlmann, <jannis@xfce.org>
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

#ifndef _XFDESKTOP_COMMON_H_
#define _XFDESKTOP_COMMON_H_

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <xfconf/xfconf.h>
#include <libxfce4windowing/libxfce4windowing.h>

#include <stdarg.h>

#define XFDESKTOP_CHANNEL  "xfce4-desktop"

#define MIN_ICON_SIZE               16
#define MAX_ICON_SIZE              192
#define DEFAULT_ICON_SIZE           48
#define MIN_ICON_FONT_SIZE           2
#define MAX_ICON_FONT_SIZE         144
#define DEFAULT_ICON_FONT_SIZE      12
#define MIN_TOOLTIP_ICON_SIZE        0
#define MAX_TOOLTIP_ICON_SIZE      256
#define DEFAULT_TOOLTIP_ICON_SIZE   64
#define MIN_GRAVITY                  0
#define MAX_GRAVITY                  7
#define DEFAULT_GRAVITY              0

#define DEFAULT_SINGLE_CLICK        FALSE
#define DEFAULT_SINGLE_CLICK_ULINE  FALSE
#define DEFAULT_ICONS_ON_PRIMARY    FALSE
#define DEFAULT_ICON_FONT_SIZE_SET  FALSE
#define DEFAULT_ICON_CENTER_TEXT    TRUE
#define DEFAULT_SHOW_TOOLTIPS       TRUE

#define ITHEME_FLAGS             (GTK_ICON_LOOKUP_USE_BUILTIN | GTK_ICON_LOOKUP_FORCE_SIZE)

#define LIST_TEXT                "# xfce backdrop list"
#define XFDESKTOP_SELECTION_FMT  "XFDESKTOP_SELECTION_%d"
#define XFDESKTOP_IMAGE_FILE_FMT "XFDESKTOP_IMAGE_FILE_%d"
#define XFDESKTOP_RC_VERSION_STAMP "xfdesktop-version-4.10.3+-rcfile_format"

#define RELOAD_MESSAGE     "reload"
#define MENU_MESSAGE       "menu"
#define WINDOWLIST_MESSAGE "windowlist"
#define ARRANGE_MESSAGE    "arrange"
#define QUIT_MESSAGE       "quit"

#define SINGLE_WORKSPACE_MODE     "/backdrop/single-workspace-mode"
#define SINGLE_WORKSPACE_NUMBER   "/backdrop/single-workspace-number"

#define DESKTOP_ICONS_ON_PRIMARY_PROP        "/desktop-icons/primary"
#define DESKTOP_ICONS_STYLE_PROP             "/desktop-icons/style"
#define DESKTOP_ICONS_ICON_SIZE_PROP         "/desktop-icons/icon-size"
#define DESKTOP_ICONS_FONT_SIZE_PROP         "/desktop-icons/font-size"
#define DESKTOP_ICONS_CUSTOM_FONT_SIZE_PROP  "/desktop-icons/use-custom-font-size"
#define DESKTOP_ICONS_SHOW_TOOLTIP_PROP      "/desktop-icons/show-tooltips"
#define DESKTOP_ICONS_TOOLTIP_SIZE_PROP      "/desktop-icons/tooltip-size"
#define DESKTOP_ICONS_SINGLE_CLICK_PROP      "/desktop-icons/single-click"
#define DESKTOP_ICONS_SINGLE_CLICK_ULINE_PROP "/desktop-icons/single-click-underline-hover"
#define DESKTOP_ICONS_GRAVITY_PROP           "/desktop-icons/gravity"

#define DESKTOP_ICONS_SHOW_THUMBNAILS        "/desktop-icons/show-thumbnails"
#define DESKTOP_ICONS_SHOW_HIDDEN_FILES      "/desktop-icons/show-hidden-files"
#define DESKTOP_ICONS_SHOW_NETWORK_REMOVABLE "/desktop-icons/file-icons/show-network-removable"
#define DESKTOP_ICONS_SHOW_DEVICE_REMOVABLE  "/desktop-icons/file-icons/show-device-removable"
#define DESKTOP_ICONS_SHOW_DEVICE_FIXED      "/desktop-icons/file-icons/show-device-fixed"
#define DESKTOP_ICONS_SHOW_UNKNWON_REMOVABLE "/desktop-icons/file-icons/show-unknown-removable"
#define DESKTOP_ICONS_SHOW_HOME              "/desktop-icons/file-icons/show-home"
#define DESKTOP_ICONS_SHOW_TRASH             "/desktop-icons/file-icons/show-trash"
#define DESKTOP_ICONS_SHOW_FILESYSTEM        "/desktop-icons/file-icons/show-filesystem"
#define DESKTOP_ICONS_SHOW_REMOVABLE         "/desktop-icons/file-icons/show-removable"

#define DESKTOP_MENU_MAX_TEMPLATE_FILES      "/desktop-menu/max-template-files"
#define DESKTOP_MENU_DELETE                  "/desktop-menu/show-delete"

/**
 * File information namespaces queried for #GFileInfo objects.
 */
#define XFDESKTOP_FILE_INFO_NAMESPACE \
  "access::*," \
  "id::*," \
  "mountable::*," \
  "preview::*," \
  "standard::*," \
  "time::*," \
  "thumbnail::*," \
  "trash::*," \
  "unix::*," \
  "metadata::*"

/**
 * Filesystem information namespaces queried for #GFileInfo * objects.
 */
#define XFDESKTOP_FILESYSTEM_INFO_NAMESPACE \
  "filesystem::*"

#define XFCE_TYPE_BACKDROP_COLOR_STYLE (xfce_backdrop_color_style_get_type())
#define XFCE_TYPE_BACKDROP_IMAGE_STYLE (xfce_backdrop_image_style_get_type())
#define XFCE_TYPE_BACKDROP_CYCLE_PERIOD (xfce_backdrop_cycle_period_get_type())
#define XFCE_TYPE_DESKTOP_ICON_STYLE (xfce_desktop_icon_style_get_type())


#ifdef ENABLE_DESKTOP_ICONS
#ifdef ENABLE_FILE_ICONS
#define ICON_STYLE_DEFAULT XFCE_DESKTOP_ICON_STYLE_FILES
#else  /* !ENABLE_FILE_ICONS */
#define ICON_STYLE_DEFAULT XFCE_DESKTOP_ICON_STYLE_WINDOWS
#endif /* ENABLE_FILE_ICONS */
#else  /* !ENABLE_DESKTOP_ICONS */
#define ICON_STYLE_DEFAULT XFCE_DESKTOP_ICON_STYLE_NONE
#endif  /* ENABLE_DESKTOP_ICONS */

G_BEGIN_DECLS

typedef enum {
    XFCE_BACKDROP_COLOR_INVALID = -1,
    XFCE_BACKDROP_COLOR_SOLID = 0,
    XFCE_BACKDROP_COLOR_HORIZ_GRADIENT,
    XFCE_BACKDROP_COLOR_VERT_GRADIENT,
    XFCE_BACKDROP_COLOR_TRANSPARENT,
} XfceBackdropColorStyle;

typedef enum {
    XFCE_BACKDROP_IMAGE_INVALID = -1,
    XFCE_BACKDROP_IMAGE_NONE = 0,
    XFCE_BACKDROP_IMAGE_CENTERED,
    XFCE_BACKDROP_IMAGE_TILED,
    XFCE_BACKDROP_IMAGE_STRETCHED,
    XFCE_BACKDROP_IMAGE_SCALED,
    XFCE_BACKDROP_IMAGE_ZOOMED,
    XFCE_BACKDROP_IMAGE_SPANNING_SCREENS,
} XfceBackdropImageStyle;

typedef enum {
    XFCE_BACKDROP_PERIOD_INVALID = -1,
    XFCE_BACKDROP_PERIOD_SECONDS = 0,
    XFCE_BACKDROP_PERIOD_MINUTES,
    XFCE_BACKDROP_PERIOD_HOURS,
    XFCE_BACKDROP_PERIOD_STARTUP,
    XFCE_BACKDROP_PERIOD_HOURLY,
    XFCE_BACKDROP_PERIOD_DAILY,
    XFCE_BACKDROP_PERIOD_CHRONOLOGICAL,
} XfceBackdropCyclePeriod;

typedef enum {
    XFCE_DESKTOP_ICON_STYLE_NONE = 0,
    XFCE_DESKTOP_ICON_STYLE_WINDOWS,
    XFCE_DESKTOP_ICON_STYLE_FILES,
} XfceDesktopIconStyle;

GType xfce_backdrop_color_style_get_type(void) G_GNUC_CONST;
GType xfce_backdrop_image_style_get_type(void) G_GNUC_CONST;
GType xfce_backdrop_cycle_period_get_type(void) G_GNUC_CONST;
GType xfce_desktop_icon_style_get_type(void) G_GNUC_CONST;

gchar* xfdesktop_get_monitor_name_from_gtk_widget(GtkWidget *widget,
                                                  gint monitor_num);

gint xfdesktop_compare_paths(GFile *a, GFile *b);

gboolean xfdesktop_image_file_is_valid(const gchar *filename);

gchar *xfdesktop_get_file_mimetype(const gchar *file);

gint xfce_translate_image_styles(gint input);

gchar* xfdesktop_remove_whitspaces(gchar* str);

GtkWidget* xfdesktop_menu_create_menu_item_with_markup(const gchar *name,
                                                       GtkWidget   *image);

GtkWidget* xfdesktop_menu_create_menu_item_with_mnemonic(const gchar *name,
                                                         GtkWidget   *image);

gint xfdesktop_get_monitor_num(GdkDisplay *display,
                               GdkMonitor *monitor);

gint xfdesktop_get_current_monitor_num(GdkDisplay *display);

gboolean xfdesktop_workspace_get_number_and_total(XfwWorkspaceManager *workspace_manager,
                                                  XfwWorkspace *workspace,
                                                  gint *workspace_number,
                                                  gint *total_workspace_count);

GtkWindow * xfdesktop_find_toplevel(GtkWidget *widget);

void xfdesktop_tree_path_free(gpointer data);

void xfdesktop_widget_unrealize(GtkWidget *widget,
                                gpointer data);

void xfdesktop_object_ref(gpointer data,
                          gpointer user_data);

void xfdesktop_object_unref(gpointer data,
                            GClosure *closure);

#if defined(G_HAVE_ISO_VARARGS)

#define XF_DEBUG(...) xfdesktop_debug (__func__, __FILE__, __LINE__, __VA_ARGS__)

void xfdesktop_debug(const char *func, const char *file, int line, const char *format, ...) __attribute__((format (printf,4,5)));

#else /* defined(G_HAVE_ISO_VARARGS) */

#define XF_DEBUG(...)

#endif /* defined(G_HAVE_ISO_VARARGS) */

void xfdesktop_debug_set(gboolean debug);

void xfdesktop_migrate_backdrop_settings(GdkDisplay *display,
                                         XfconfChannel *channel);
G_END_DECLS

#endif
