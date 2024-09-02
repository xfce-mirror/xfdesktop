/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006      Brian Tarricone, <brian@tarricone.org>
 *  Copyright(c) 2006      Benedikt Meurer, <benny@xfce.org>
 *  Copyright(c) 2010-2011 Jannis Pohlmann, <jannis@xfce.org>
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#ifdef HAVE_THUNARX
#include <thunarx/thunarx.h>
#endif

#include "xfce-desktop.h"
#include "xfdesktop-backdrop-manager.h"
#include "xfdesktop-clipboard-manager.h"
#include "xfdesktop-common.h"
#include "xfdesktop-file-icon.h"
#include "xfdesktop-file-icon-manager.h"
#include "xfdesktop-file-icon-model.h"
#include "xfdesktop-file-icon-model-filter.h"
#include "xfdesktop-file-utils.h"
#include "xfdesktop-icon-position-configs.h"
#include "xfdesktop-icon-position-migration.h"
#include "xfdesktop-icon-view-holder.h"
#include "xfdesktop-icon-view.h"
#include "xfdesktop-icon-view-model.h"
#include "xfdesktop-icon.h"
#include "xfdesktop-regular-file-icon.h"
#include "xfdesktop-special-file-icon.h"
#include "xfdesktop-volume-icon.h"

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4windowing/libxfce4windowing.h>

#define SAVE_DELAY  1000
#define BORDER         8

#define VOLUME_HASH_STR_PREFIX  "xfdesktop-volume-"

struct _XfdesktopFileIconManager
{
    XfdesktopIconViewManager parent;

    gboolean ready;

    XfdesktopFileIconModel *model;
    GHashTable *monitor_data;  // XfwMonitor (owner) -> MonitorData (owner)
    XfdesktopIconPositionConfigs *position_configs;

    GdkScreen *gscreen;
    XfdesktopBackdropManager *backdrop_manager;

    GFile *folder;
    XfdesktopFileIcon *desktop_icon;

    GtkTargetList *drag_targets;
    GtkTargetList *drop_targets;

#ifdef HAVE_THUNARX
    GList *thunarx_menu_providers;
    GList *thunarx_properties_providers;
#endif

    gboolean show_delete_menu;
    guint max_templates;
    guint templates_count;
};

enum {
    PROP0 = 0,
    PROP_GDK_SCREEN,
    PROP_BACKDROP_MANAGER,
    PROP_FOLDER,
    PROP_SHOW_DELETE_MENU,
    PROP_MAX_TEMPLATES,
};

typedef struct {
    XfdesktopFileIconManager *fmanager;
    XfdesktopIconViewHolder *holder;
    XfdesktopFileIconModelFilter *filter;
    XfdesktopIconPositionConfig *position_config;
} MonitorData;

static void
monitor_data_free(MonitorData *mdata) {
    if (mdata->position_config != NULL) {
        XfceDesktop *desktop = xfdesktop_icon_view_holder_get_desktop(mdata->holder);
        XfwMonitor *monitor = xfce_desktop_get_monitor(desktop);
        xfdesktop_icon_position_configs_unassign_monitor(mdata->fmanager->position_configs, monitor);
    }
    g_object_unref(mdata->holder);
    g_object_unref(mdata->filter);
    g_free(mdata);
}

static void xfdesktop_file_icon_manager_constructed(GObject *obj);
static void xfdesktop_file_icon_manager_set_property(GObject *object,
                                                     guint property_id,
                                                     const GValue *value,
                                                     GParamSpec *pspec);
static void xfdesktop_file_icon_manager_get_property(GObject *object,
                                                     guint property_id,
                                                     GValue *value,
                                                     GParamSpec *pspec);
static void xfdesktop_file_icon_manager_dispose(GObject *obj);
static void xfdesktop_file_icon_manager_finalize(GObject *obj);

static GdkDragAction xfdesktop_file_icon_manager_drag_actions_get(XfdesktopIconView *icon_view,
                                                                  GtkTreeIter *iter,
                                                                  MonitorData *mdata);
static GdkDragAction xfdesktop_file_icon_manager_drop_actions_get(XfdesktopIconView *icon_view,
                                                                  GtkTreeIter *iter,
                                                                  GdkDragAction *suggested_action,
                                                                  MonitorData *mdata);
static gboolean xfdesktop_file_icon_manager_drag_drop_item(XfdesktopIconView *icon_view,
                                                           GdkDragContext *context,
                                                           GtkTreeIter *iter,
                                                           gint row,
                                                           gint col,
                                                           guint time_,
                                                           MonitorData *mdata);
static gboolean xfdesktop_file_icon_manager_drag_drop_items(XfdesktopIconView *icon_view,
                                                            GdkDragContext *context,
                                                            GtkTreeIter *iter,
                                                            GList *dropped_item_paths,
                                                            GdkDragAction action,
                                                            MonitorData *mdata);
static GdkDragAction xfdesktop_file_icon_manager_drag_drop_ask(XfdesktopIconView *icon_view,
                                                               GdkDragContext *context,
                                                               GtkTreeIter *iter,
                                                               gint row,
                                                               gint col,
                                                               guint time_,
                                                               XfdesktopFileIconManager *fmanager);
static void xfdesktop_file_icon_manager_drag_item_data_received(XfdesktopIconView *icon_view,
                                                                GdkDragContext *context,
                                                                GtkTreeIter *iter,
                                                                gint row,
                                                                gint col,
                                                                GtkSelectionData *data,
                                                                guint info,
                                                                guint time_,
                                                                MonitorData *mdata);
static void xfdesktop_file_icon_manager_drag_data_get(GtkWidget *icon_view,
                                                      GdkDragContext *context,
                                                      GtkSelectionData *data,
                                                      guint info,
                                                      guint time_,
                                                      MonitorData *mdata);
static GdkDragAction xfdesktop_file_icon_manager_drop_propose_action(XfdesktopIconView *icon_view,
                                                                     GdkDragContext *context,
                                                                     GtkTreeIter *iter,
                                                                     GdkDragAction action,
                                                                     GtkSelectionData *data,
                                                                     guint info,
                                                                     MonitorData *mdata);

static void xfdesktop_file_icon_manager_desktop_added(XfdesktopIconViewManager *manager,
                                                      XfceDesktop *desktop);
static void xfdesktop_file_icon_manager_desktop_removed(XfdesktopIconViewManager *manager,
                                                        XfceDesktop *desktop);
static GtkMenu *xfdesktop_file_icon_manager_get_context_menu(XfdesktopIconViewManager *manager,
                                                             GtkWidget *widget);
static void xfdesktop_file_icon_manager_sort_icons(XfdesktopIconViewManager *manager,
                                                   GtkSortType sort_type);

static void xfdesktop_file_icon_manager_icon_moved(XfdesktopIconView *icon_view,
                                                   GtkTreeIter *iter,
                                                   gint new_row,
                                                   gint new_col,
                                                   MonitorData *mdata);
static void xfdesktop_file_icon_manager_activate_selected(GtkWidget *widget,
                                                          XfdesktopFileIconManager *fmanager);

static GList *xfdesktop_file_icon_manager_get_selected_icons(XfdesktopFileIconManager *fmanager);

static void xfdesktop_file_icon_manager_clipboard_changed(XfdesktopClipboardManager *cmanager,
                                                          XfdesktopFileIconManager *fmanager);
static gboolean xfdesktop_file_icon_manager_key_press(GtkWidget *widget,
                                                      GdkEventKey *evt,
                                                      XfdesktopFileIconManager *fmanager);

static void xfdesktop_file_icon_manager_icon_view_realized(GtkWidget *icon_view,
                                                           XfdesktopFileIconManager *fmanager);
static void xfdesktop_file_icon_manager_icon_view_unrealized(GtkWidget *icon_view,
                                                             XfdesktopFileIconManager *fmanager);

static void xfdesktop_file_icon_manager_start_grid_resize(XfdesktopIconView *icon_view,
                                                          gint new_rows,
                                                          gint new_cols,
                                                          MonitorData *mdata);
static void xfdesktop_file_icon_manager_end_grid_resize(XfdesktopIconView *icon_view,
                                                        MonitorData *mdata);

static XfwMonitor *xfdesktop_file_icon_manager_get_cached_icon_position(XfdesktopFileIconManager *fmanager,
                                                                        XfdesktopFileIcon *icon,
                                                                        gint16 *row,
                                                                        gint16 *col);

static void model_ready(XfdesktopFileIconModel *fmodel,
                        XfdesktopFileIconManager *fmanager);
static void model_error(XfdesktopFileIconModel *fmodel,
                        GError *error,
                        XfdesktopFileIconManager *fmanager);


G_DEFINE_TYPE(XfdesktopFileIconManager, xfdesktop_file_icon_manager, XFDESKTOP_TYPE_ICON_VIEW_MANAGER)


enum
{
    TARGET_TEXT_URI_LIST = 0,
    TARGET_XDND_DIRECT_SAVE0,
    TARGET_NETSCAPE_URL,
    TARGET_APPLICATION_OCTET_STREAM,
};

static const struct
{
    const gchar *setting;
    GType setting_type;
    const gchar *property;
} setting_bindings[] = {
    { DESKTOP_MENU_DELETE, G_TYPE_BOOLEAN, "show-delete-menu" },
    { DESKTOP_MENU_MAX_TEMPLATE_FILES, G_TYPE_INT, "max-templates" },
};

static const GtkTargetEntry drag_targets[] = {
    { "text/uri-list", 0, TARGET_TEXT_URI_LIST, },
};
static const GtkTargetEntry drop_targets[] = {
    { "text/uri-list", 0, TARGET_TEXT_URI_LIST, },
    { "application/octet-stream", 0, TARGET_APPLICATION_OCTET_STREAM, },
    { "XdndDirectSave0", 0, TARGET_XDND_DIRECT_SAVE0, },
    { "_NETSCAPE_URL", 0, TARGET_NETSCAPE_URL, },
};

static XfdesktopClipboardManager *clipboard_manager = NULL;

static GQuark xfdesktop_app_info_quark = 0;


static void
xfdesktop_file_icon_manager_class_init(XfdesktopFileIconManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    XfdesktopIconViewManagerClass *ivm_class = XFDESKTOP_ICON_VIEW_MANAGER_CLASS(klass);

    gobject_class->constructed = xfdesktop_file_icon_manager_constructed;
    gobject_class->set_property = xfdesktop_file_icon_manager_set_property;
    gobject_class->get_property = xfdesktop_file_icon_manager_get_property;
    gobject_class->dispose = xfdesktop_file_icon_manager_dispose;
    gobject_class->finalize = xfdesktop_file_icon_manager_finalize;

    ivm_class->desktop_added = xfdesktop_file_icon_manager_desktop_added;
    ivm_class->desktop_removed = xfdesktop_file_icon_manager_desktop_removed;
    ivm_class->get_context_menu = xfdesktop_file_icon_manager_get_context_menu;
    ivm_class->sort_icons = xfdesktop_file_icon_manager_sort_icons;

    g_object_class_install_property(gobject_class,
                                    PROP_GDK_SCREEN,
                                    g_param_spec_object("gdk-screen",
                                                        "gdk-screen",
                                                        "GdkScreen",
                                                        GDK_TYPE_SCREEN,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_BACKDROP_MANAGER,
                                    g_param_spec_object("backdrop-manager",
                                                        "backdrop-manager",
                                                        "backdrop manager",
                                                        XFDESKTOP_TYPE_BACKDROP_MANAGER,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_FOLDER,
                                    g_param_spec_object("folder", "Desktop Folder",
                                                       "Folder this icon manager manages",
                                                       G_TYPE_FILE,
                                                       G_PARAM_READWRITE
                                                       | G_PARAM_CONSTRUCT_ONLY
                                                       | G_PARAM_STATIC_NAME
                                                       | G_PARAM_STATIC_NICK
                                                       | G_PARAM_STATIC_BLURB));

    g_object_class_install_property(gobject_class, PROP_SHOW_DELETE_MENU,
                                    g_param_spec_boolean("show-delete-menu",
                                                         "show-delete-menu",
                                                         "show-delete-menu",
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_MAX_TEMPLATES,
                                    g_param_spec_uint("max-templates",
                                                      "max-templates",
                                                      "max-templates",
                                                      0, G_MAXUSHORT, 16,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    xfdesktop_app_info_quark = g_quark_from_static_string("xfdesktop-app-info-quark");
}

static void
xfdesktop_file_icon_manager_init(XfdesktopFileIconManager *fmanager)
{
    fmanager->monitor_data = g_hash_table_new_full(g_direct_hash,
                                                   g_direct_equal,
                                                   g_object_unref,
                                                   (GDestroyNotify)monitor_data_free);
    fmanager->ready = FALSE;
    fmanager->show_delete_menu = TRUE;
    fmanager->max_templates = 16;
}

static void
xfdesktop_file_icon_manager_constructed(GObject *obj)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(obj);

    G_OBJECT_CLASS(xfdesktop_file_icon_manager_parent_class)->constructed(obj);

    fmanager->drag_targets = gtk_target_list_new(drag_targets, G_N_ELEMENTS(drag_targets));
    fmanager->drop_targets = gtk_target_list_new(drop_targets, G_N_ELEMENTS(drop_targets));

    if (clipboard_manager == NULL) {
        GdkDisplay *gdpy = gdk_screen_get_display(fmanager->gscreen);
        clipboard_manager = xfdesktop_clipboard_manager_get_for_display(gdpy);
        g_object_add_weak_pointer(G_OBJECT(clipboard_manager),
                                  (gpointer)&clipboard_manager);
    } else
        g_object_ref(G_OBJECT(clipboard_manager));

    g_signal_connect(G_OBJECT(clipboard_manager), "changed",
                     G_CALLBACK(xfdesktop_file_icon_manager_clipboard_changed),
                     fmanager);

    if(!xfdesktop_file_utils_dbus_init())
        g_warning("Unable to initialise D-Bus.  Some xfdesktop features may be unavailable.");

#ifdef HAVE_THUNARX
    ThunarxProviderFactory *thunarx_pfac = thunarx_provider_factory_get_default();

    fmanager->thunarx_menu_providers =
        thunarx_provider_factory_list_providers(thunarx_pfac,
                                                THUNARX_TYPE_MENU_PROVIDER);
    fmanager->thunarx_properties_providers =
        thunarx_provider_factory_list_providers(thunarx_pfac,
                                                THUNARX_TYPE_PROPERTY_PAGE_PROVIDER);

    g_object_unref(G_OBJECT(thunarx_pfac));
#endif

    XfconfChannel *channel = xfdesktop_icon_view_manager_get_channel(XFDESKTOP_ICON_VIEW_MANAGER(fmanager));
    GFileInfo *desktop_info = g_file_query_info(fmanager->folder,
                                                XFDESKTOP_FILE_INFO_NAMESPACE,
                                                G_FILE_QUERY_INFO_NONE,
                                                NULL, NULL);
    fmanager->desktop_icon = XFDESKTOP_FILE_ICON(xfdesktop_regular_file_icon_new(channel,
                                                                                 fmanager->gscreen,
                                                                                 fmanager->folder,
                                                                                 desktop_info));
    g_object_unref(desktop_info);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gint screen_num = gdk_screen_get_number(fmanager->gscreen);
G_GNUC_END_IGNORE_DEPRECATIONS
    gchar *positions_file_relpath = g_strdup_printf("xfce4/desktop/icons.screen%d.yaml", screen_num);
    gchar *positions_file_path = xfce_resource_save_location(XFCE_RESOURCE_CONFIG, positions_file_relpath, TRUE);
    GFile *positions_file = g_file_new_for_path(positions_file_path);
    fmanager->position_configs = xfdesktop_icon_position_configs_new(positions_file);

    GError *error = NULL;
    if (!xfdesktop_icon_position_configs_load(fmanager->position_configs, &error)) {
        if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
            g_message("Unable to load icon positions: %s", error->message);
        }
        g_error_free(error);
    }

    g_free(positions_file_relpath);
    g_free(positions_file_path);
    g_object_unref(positions_file);

    fmanager->model = xfdesktop_file_icon_model_new(channel, fmanager->folder, fmanager->gscreen);
    g_signal_connect(fmanager->model, "ready",
                     G_CALLBACK(model_ready), fmanager);
    g_signal_connect(fmanager->model, "error",
                     G_CALLBACK(model_error), fmanager);
    g_signal_connect_swapped(fmanager->model, "icon-position-request",
                             G_CALLBACK(xfdesktop_file_icon_manager_get_cached_icon_position), fmanager);

    GList *desktops = xfdesktop_icon_view_manager_get_desktops(XFDESKTOP_ICON_VIEW_MANAGER(fmanager));
    for (GList *l = desktops; l != NULL; l = l->next) {
        XfceDesktop *desktop = XFCE_DESKTOP(l->data);
        xfdesktop_file_icon_manager_desktop_added(XFDESKTOP_ICON_VIEW_MANAGER(fmanager), desktop);
    }

    for (gsize i = 0; i < G_N_ELEMENTS(setting_bindings); ++i) {
        xfconf_g_property_bind(channel,
                               setting_bindings[i].setting, setting_bindings[i].setting_type,
                               G_OBJECT(fmanager), setting_bindings[i].property);
    }

    xfdesktop_file_icon_model_reload(fmanager->model);
}

static void
xfdesktop_file_icon_manager_set_property(GObject *object,
                                         guint property_id,
                                         const GValue *value,
                                         GParamSpec *pspec)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(object);

    switch(property_id) {
        case PROP_GDK_SCREEN:
            fmanager->gscreen = g_value_get_object(value);
            break;

        case PROP_BACKDROP_MANAGER:
            fmanager->backdrop_manager = g_value_get_object(value);
            break;

        case PROP_FOLDER:
            fmanager->folder = g_value_dup_object(value);
            break;

        case PROP_SHOW_DELETE_MENU:
            fmanager->show_delete_menu = g_value_get_boolean(value);
            break;

        case PROP_MAX_TEMPLATES:
            fmanager->max_templates = MIN(g_value_get_uint(value), G_MAXUINT16);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
xfdesktop_file_icon_manager_get_property(GObject *object,
                                         guint property_id,
                                         GValue *value,
                                         GParamSpec *pspec)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(object);

    switch(property_id) {
        case PROP_GDK_SCREEN:
            g_value_set_object(value, fmanager->gscreen);
            break;

        case PROP_BACKDROP_MANAGER:
            g_value_set_object(value, fmanager->backdrop_manager);
            break;

        case PROP_FOLDER:
            g_value_set_object(value, fmanager->folder);
            break;

        case PROP_SHOW_DELETE_MENU:
            g_value_set_boolean(value, fmanager->show_delete_menu);
            break;

        case PROP_MAX_TEMPLATES:
            g_value_set_int(value, fmanager->max_templates);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
xfdesktop_file_icon_manager_dispose(GObject *obj)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(obj);

    g_clear_pointer(&fmanager->monitor_data, g_hash_table_destroy);
    g_clear_object(&fmanager->model);

    G_OBJECT_CLASS(xfdesktop_file_icon_manager_parent_class)->dispose(obj);
}

static void
xfdesktop_file_icon_manager_finalize(GObject *obj)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(obj);

    g_signal_handlers_disconnect_by_func(G_OBJECT(clipboard_manager),
                                         G_CALLBACK(xfdesktop_file_icon_manager_clipboard_changed),
                                         fmanager);
    g_object_unref(G_OBJECT(clipboard_manager));

    g_object_unref(G_OBJECT(fmanager->desktop_icon));

#ifdef HAVE_THUNARX
    g_list_free_full(fmanager->thunarx_menu_providers, g_object_unref);
    g_list_free_full(fmanager->thunarx_properties_providers, g_object_unref);
#endif

    xfdesktop_file_utils_dbus_cleanup();

    gtk_target_list_unref(fmanager->drag_targets);
    gtk_target_list_unref(fmanager->drop_targets);

    g_object_unref(fmanager->folder);

    G_OBJECT_CLASS(xfdesktop_file_icon_manager_parent_class)->finalize(obj);
}

static void
xfdesktop_file_icon_manager_icon_view_realized(GtkWidget *icon_view, XfdesktopFileIconManager *fmanager) {
    GtkWidget *window_widget = xfdesktop_icon_view_get_window_widget(XFDESKTOP_ICON_VIEW(icon_view));
    g_signal_connect(window_widget, "key-press-event",
                     G_CALLBACK(xfdesktop_file_icon_manager_key_press), fmanager);
}

static void
xfdesktop_file_icon_manager_icon_view_unrealized(GtkWidget *icon_view, XfdesktopFileIconManager *fmanager) {
    GtkWidget *window_widget = xfdesktop_icon_view_get_window_widget(XFDESKTOP_ICON_VIEW(icon_view));
    g_signal_handlers_disconnect_by_data(window_widget, fmanager);
}

static GtkWindow *
toplevel_window_for_widget(GtkWidget *widget) {
    if (GTK_IS_MENU_ITEM(widget)) {
        GtkWidget *cur = widget;
        while (cur != NULL) {
            cur = gtk_widget_get_parent(cur);
            if (GTK_IS_MENU(cur)) {
                GtkWidget *attach_widget = gtk_menu_get_attach_widget(GTK_MENU(cur));
                if (GTK_IS_MENU_ITEM(attach_widget)) {
                    cur = attach_widget;
                } else if (XFCE_IS_DESKTOP(attach_widget)) {
                    return GTK_WINDOW(attach_widget);
                } else if (XFDESKTOP_IS_ICON_VIEW(attach_widget)) {
                    return GTK_WINDOW(gtk_widget_get_toplevel(attach_widget));
                }
            }
        }
    } else {
        return GTK_WINDOW(gtk_widget_get_toplevel(widget));
    }

    return NULL;
}

/* icon signal handlers */

static void
xfdesktop_file_icon_menu_executed(GtkWidget *widget, XfdesktopFileIconManager *fmanager) {
    GList *selected = xfdesktop_file_icon_manager_get_selected_icons(fmanager);
    if (selected != NULL && selected->next == NULL) {
        XfdesktopIcon *icon = XFDESKTOP_ICON(selected->data);
        xfdesktop_icon_activate(icon, toplevel_window_for_widget(widget));
    }

    g_list_free(selected);
}

static void
xfdesktop_file_icon_menu_open_all(GtkWidget *widget, XfdesktopFileIconManager *fmanager) {
    xfdesktop_file_icon_manager_activate_selected(widget, fmanager);
}

static void
xfdesktop_file_icon_menu_rename(GtkWidget *widget, XfdesktopFileIconManager *fmanager) {
    GList *selected = xfdesktop_file_icon_manager_get_selected_icons(fmanager);
    GList *files = NULL;
    for (GList *iter = selected; iter != NULL; iter = iter->next) {
        XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(iter->data);

        /* create a list of GFiles from selected icons that can be renamed */
        if(xfdesktop_file_icon_can_rename_file(icon))
            files = g_list_append(files, xfdesktop_file_icon_peek_file(icon));
    }

    GtkWindow *toplevel = toplevel_window_for_widget(widget);
    if(g_list_length(files) == 1) {
        /* rename dialog for a single icon */
        xfdesktop_file_utils_rename_file(g_list_first(files)->data, fmanager->gscreen, toplevel);
    } else if(g_list_length(files) > 1) {
        /* Bulk rename for multiple icons selected */
        GFile *desktop = xfdesktop_file_icon_peek_file(fmanager->desktop_icon);

        xfdesktop_file_utils_bulk_rename(desktop, files, fmanager->gscreen, toplevel);
    } else {
        /* Nothing valid to rename */
        xfce_message_dialog(toplevel,
                            _("Rename Error"), "dialog-error",
                            _("The files could not be renamed"),
                            _("None of the icons selected support being renamed."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }

    g_list_free(files);
    g_list_free(selected);
}

static void
xfdesktop_file_icon_manager_delete_files(XfdesktopFileIconManager *fmanager, GtkWidget *widget, GList *files) {
    GList *gfiles = NULL;
    for (GList *lp = g_list_last(files); lp != NULL; lp = lp->prev) {
        gfiles = g_list_prepend(gfiles, xfdesktop_file_icon_peek_file(lp->data));
    }

    xfdesktop_file_utils_unlink_files(gfiles, fmanager->gscreen, toplevel_window_for_widget(widget));

    g_list_free(gfiles);
}

static gboolean
xfdesktop_file_icon_manager_trash_files(XfdesktopFileIconManager *fmanager, GtkWidget *widget, GList *files) {
    GList *gfiles = NULL;
    for (GList *lp = g_list_last(files); lp != NULL; lp = lp->prev) {
        gfiles = g_list_prepend(gfiles, xfdesktop_file_icon_peek_file(lp->data));
    }

    xfdesktop_file_utils_trash_files(gfiles, fmanager->gscreen, toplevel_window_for_widget(widget));

    g_list_free(gfiles);
    return TRUE;
}

static void
xfdesktop_file_icon_manager_delete_selected(XfdesktopFileIconManager *fmanager, GtkWidget *widget, gboolean force_delete) {
    GList *selected = xfdesktop_file_icon_manager_get_selected_icons(fmanager);
    if(!selected)
        return;

    /* remove anybody that's not deletable */
    for(GList *l = selected; l; ) {
        if(!xfdesktop_file_icon_can_delete_file(XFDESKTOP_FILE_ICON(l->data))) {
            GList *next = l->next;

            if(l->prev)
                l->prev->next = l->next;
            else  /* this is the first item; reset |selected| */
                selected = l->next;

            if(l->next)
                l->next->prev = l->prev;

            l->next = l->prev = NULL;
            g_list_free_1(l);

            l = next;
        } else
            l = l->next;
    }

    if(G_UNLIKELY(!selected))
        return;

    /* make sure the icons don't get destroyed while we're working */
    g_list_foreach(selected, xfdesktop_object_ref, NULL);

    if(!force_delete) {
        xfdesktop_file_icon_manager_trash_files(fmanager, widget, selected);
    } else {
        xfdesktop_file_icon_manager_delete_files(fmanager, widget, selected);
    }

    g_list_free_full(selected, g_object_unref);
}

static void
xfdesktop_file_icon_menu_app_info_executed(GtkWidget *widget, XfdesktopFileIconManager *fmanager) {
    GList *selected = xfdesktop_file_icon_manager_get_selected_icons(fmanager);
    for (GList *l = selected; l != NULL; l = l->next) {
        XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(l->data);
        l->data = xfdesktop_file_icon_peek_file(icon);
    }

    /* get the app info related to this menu item */
    GAppInfo *app_info = g_object_get_qdata(G_OBJECT(widget), xfdesktop_app_info_quark);
    if (app_info != NULL) {
        /* prepare the launch context and configure its screen */
        GdkAppLaunchContext *context = gdk_display_get_app_launch_context(gtk_widget_get_display(widget));
        gdk_app_launch_context_set_screen(context, gtk_widget_get_screen(widget));

        /* try to launch the application */
        GError *error = NULL;
        if (!xfdesktop_file_utils_app_info_launch(app_info, fmanager->folder, selected,
                                                  G_APP_LAUNCH_CONTEXT(context), &error))
        {
            GtkWindow *toplevel = toplevel_window_for_widget(widget);
            gchar *primary = g_markup_printf_escaped(_("Unable to launch \"%s\":"),
                                                     g_app_info_get_name(app_info));
            xfce_message_dialog(toplevel, _("Launch Error"),
                                "dialog-error", primary, error->message,
                                XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                                NULL);
            g_free(primary);
            g_error_free(error);
        }
    }

    g_list_free(selected);
}

static void
xfdesktop_file_icon_menu_open_folder(GtkWidget *widget, XfdesktopFileIconManager *fmanager) {
    GList *selected = xfdesktop_file_icon_manager_get_selected_icons(fmanager);
    for (GList *l = selected; l != NULL; l = l->next) {
        XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(l->data);
        l->data = xfdesktop_file_icon_peek_file(icon);
    }

    if (selected != NULL) {
        xfdesktop_file_utils_open_folders(selected, fmanager->gscreen, toplevel_window_for_widget(widget));
        g_list_free(selected);
    }
}

static void
xfdesktop_file_icon_menu_open_desktop(GtkWidget *widget, XfdesktopFileIconManager *fmanager) {
    GList link = {
        .data = xfdesktop_file_icon_peek_file(fmanager->desktop_icon),
        .prev = NULL,
        .next = NULL,
    };

    if (link.data != NULL) {
        xfdesktop_file_utils_open_folders(&link, fmanager->gscreen, toplevel_window_for_widget(widget));
    }
}

static void
xfdesktop_file_icon_menu_other_app(GtkWidget *widget, XfdesktopFileIconManager *fmanager) {
    GList *selected = xfdesktop_file_icon_manager_get_selected_icons(fmanager);
    if (selected != NULL && selected->next == NULL) {
        XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(selected->data);
        GFile *file = xfdesktop_file_icon_peek_file(icon);
        GtkWindow *toplevel = toplevel_window_for_widget(widget);
        xfdesktop_file_utils_display_app_chooser_dialog(file, TRUE, FALSE, fmanager->gscreen, toplevel);
    }

    g_list_free(selected);
}

static void
xfdesktop_file_icon_menu_set_default_app(GtkWidget *widget, XfdesktopFileIconManager *fmanager) {
    GList *selected = xfdesktop_file_icon_manager_get_selected_icons(fmanager);
    if (selected != NULL && selected->next == NULL) {
        XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(selected->data);
        GFile *file = xfdesktop_file_icon_peek_file(icon);
        GtkWindow *toplevel = toplevel_window_for_widget(widget);
        xfdesktop_file_utils_display_app_chooser_dialog(file, TRUE, TRUE, fmanager->gscreen, toplevel);
    }

    g_list_free(selected);
}

static void
xfdesktop_file_icon_menu_cut(GtkWidget *widget, XfdesktopFileIconManager *fmanager) {
    GList *files = xfdesktop_file_icon_manager_get_selected_icons(fmanager);
    if (files != NULL) {
        xfdesktop_clipboard_manager_cut_files(clipboard_manager, files);
        g_list_free(files);
    }
}

static void
xfdesktop_file_icon_menu_copy(GtkWidget *widget, XfdesktopFileIconManager *fmanager) {
    GList *files = xfdesktop_file_icon_manager_get_selected_icons(fmanager);
    if (files != NULL) {
        xfdesktop_clipboard_manager_copy_files(clipboard_manager, files);
        g_list_free(files);
    }
}

static void
xfdesktop_file_icon_menu_trash(GtkWidget *widget, XfdesktopFileIconManager *fmanager) {
    xfdesktop_file_icon_manager_delete_selected(fmanager, widget, FALSE);
}

static void
xfdesktop_file_icon_menu_delete(GtkWidget *widget, XfdesktopFileIconManager *fmanager) {
    xfdesktop_file_icon_manager_delete_selected(fmanager, widget, TRUE);
}

static void
xfdesktop_file_icon_menu_paste(GtkWidget *widget, XfdesktopFileIconManager *fmanager) {
    xfdesktop_clipboard_manager_paste_files(clipboard_manager, fmanager->folder, widget, NULL);
}

static void
xfdesktop_file_icon_menu_paste_into_folder(GtkWidget *widget, XfdesktopFileIconManager *fmanager) {
    GList *selected = xfdesktop_file_icon_manager_get_selected_icons(fmanager);
    if (selected != NULL && selected->next == NULL) {
        XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(selected->data);
        GFileInfo *info = xfdesktop_file_icon_peek_file_info(icon);
        if (info != NULL && g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
            GFile *file = xfdesktop_file_icon_peek_file(icon);
            xfdesktop_clipboard_manager_paste_files(clipboard_manager, file, widget, NULL);
        }
    }

    g_list_free(selected);
}

static void
xfdesktop_file_icon_menu_arrange_icons(GtkWidget *widget, XfdesktopFileIconManager *fmanager) {
    GtkWidget *window = gtk_widget_get_toplevel(widget);
    if (xfce_dialog_confirm(GTK_WINDOW(window),
                            NULL,
                            _("_OK"),
                            NULL,
                            "%s",
                            _("This will reorder all desktop items and place them on different screen positions.\nAre you sure?")))
    {
        xfdesktop_file_icon_manager_sort_icons(XFDESKTOP_ICON_VIEW_MANAGER(fmanager), GTK_SORT_ASCENDING);
    }
}

static void
xfdesktop_file_icon_menu_next_background(GtkWidget *widget, XfdesktopFileIconManager *fmanager) {
    // FIXME: need to handle spanning in a special way?

    GHashTableIter iter;
    g_hash_table_iter_init(&iter, fmanager->monitor_data);

    MonitorData *mdata;
    while (g_hash_table_iter_next(&iter, NULL, (gpointer)&mdata)) {
        XfceDesktop *desktop = xfdesktop_icon_view_holder_get_desktop(mdata->holder);
        xfce_desktop_refresh(desktop, TRUE);
    }
}

static void
xfdesktop_file_icon_menu_properties(GtkWidget *widget, XfdesktopFileIconManager *fmanager) {
    GList *selected = xfdesktop_file_icon_manager_get_selected_icons(fmanager);
    for (GList *l = selected; l != NULL; l = l->next) {
        XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(l->data);
        l->data = xfdesktop_file_icon_peek_file(icon);
    }

    if (selected != NULL) {
        xfdesktop_file_utils_show_properties_dialog(selected, fmanager->gscreen, toplevel_window_for_widget(widget));
        g_list_free(selected);
    }
}

static void
xfdesktop_file_icon_manager_desktop_properties(GtkWidget *widget, XfdesktopFileIconManager *fmanager) {
    GFile *file = xfdesktop_file_icon_peek_file (fmanager->desktop_icon);
    GList file_l = {
        .data = file,
        .next = NULL,
    };

    xfdesktop_file_utils_show_properties_dialog(&file_l, fmanager->gscreen, toplevel_window_for_widget(widget));
}

static GtkWidget *
xfdesktop_menu_item_from_app_info(XfdesktopFileIconManager *fmanager,
                                  XfdesktopFileIcon *icon,
                                  GAppInfo *app_info,
                                  gboolean with_mnemonic,
                                  gboolean with_title_prefix)
{
    gchar *title;
    if(!with_title_prefix)
        title = g_strdup(g_app_info_get_name(app_info));
    else if(with_mnemonic) {
        title = g_strdup_printf(_("_Open With \"%s\""),
                                g_app_info_get_name(app_info));
    } else {
        title = g_strdup_printf(_("Open With \"%s\""),
                                g_app_info_get_name(app_info));
    }

    GIcon *gicon = g_app_info_get_icon(app_info);
    GtkWidget *img = gtk_image_new_from_gicon(gicon, GTK_ICON_SIZE_MENU);
    gtk_image_set_pixel_size(GTK_IMAGE(img), 16);

    GtkWidget *mi;
    if(with_mnemonic)
        mi = xfdesktop_menu_create_menu_item_with_mnemonic(title, img);
    else
        mi = xfdesktop_menu_create_menu_item_with_markup(title, img);
    g_free(title);

    g_object_set_qdata_full(G_OBJECT(mi), xfdesktop_app_info_quark,
                            g_object_ref(app_info), g_object_unref);

    gtk_widget_show(mi);

    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_file_icon_menu_app_info_executed),
                     fmanager);

    return mi;
}

static gboolean
xfdesktop_file_icon_menu_free_icon_list_idled(gpointer user_data) {
    GList *icon_list = user_data;
    g_list_free_full(icon_list, g_object_unref);
    return FALSE;
}

static void
xfdesktop_file_icon_menu_free_icon_list(GtkMenu *menu, gpointer user_data) {
    g_idle_add(xfdesktop_file_icon_menu_free_icon_list_idled, user_data);
}

static void
xfdesktop_file_icon_menu_create_launcher(GtkWidget *widget, XfdesktopFileIconManager *fmanager) {
    gchar *display_name = g_strdup (gdk_display_get_name (gdk_screen_get_display (fmanager->gscreen)));
    GFile *file = g_object_get_data(G_OBJECT(widget), "file");

    gchar *uri, *cmd;
    if(file) {
        uri = g_file_get_uri(file);
        cmd = g_strdup_printf("exo-desktop-item-edit \"--display=%s\" \"%s\"",
                              display_name, uri);
    } else {
        const gchar *type = g_object_get_data(G_OBJECT(widget), "xfdesktop-launcher-type");
        uri = g_file_get_uri(fmanager->folder);
        if(G_UNLIKELY(!type))
            type = "Application";
        cmd = g_strdup_printf("exo-desktop-item-edit \"--display=%s\" --create-new --type %s \"%s\"",
                              display_name, type, uri);
    }

    GError *error = NULL;
    if (!xfce_spawn_command_line(fmanager->gscreen, cmd, FALSE, FALSE, TRUE, &error)) {
        xfce_message_dialog(toplevel_window_for_widget(widget),
                            _("Launch Error"),
                            "dialog-error",
                            _("Unable to launch \"exo-desktop-item-edit\", which is required to create and edit launchers and links on the desktop."),
                            error->message,
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
        g_error_free(error);
    }

    g_free(display_name);
    g_free(uri);
    g_free(cmd);

}

static void
xfdesktop_file_icon_menu_create_folder(GtkWidget *widget, XfdesktopFileIconManager *fmanager) {
    xfdesktop_file_utils_create_file(fmanager->folder,
                                     "inode/directory",
                                     fmanager->gscreen,
                                     toplevel_window_for_widget(widget));
}

static void
xfdesktop_file_icon_template_item_activated(GtkWidget *mi, XfdesktopFileIconManager *fmanager) {
    GFile *file = g_object_get_data(G_OBJECT(mi), "file");

    if(file) {
        xfdesktop_file_utils_create_file_from_template(fmanager->folder, file,
                                                       fmanager->gscreen,
                                                       toplevel_window_for_widget(mi));
    } else {
        xfdesktop_file_utils_create_file(fmanager->folder, "text/plain",
                                         fmanager->gscreen,
                                         toplevel_window_for_widget(mi));
    }
}

static gint
compare_template_files(gconstpointer a, gconstpointer b) {
    GFileInfo *info_a = g_object_get_data(G_OBJECT(a), "info");
    GFileInfo *info_b = g_object_get_data(G_OBJECT(b), "info");
    GFileType type_a = g_file_info_get_file_type(info_a);
    GFileType type_b = g_file_info_get_file_type(info_b);
    const gchar* name_a = g_file_info_get_display_name(info_a);
    const gchar* name_b = g_file_info_get_display_name(info_b);

    if(!info_a || !info_b)
        return 0;

    if(type_a == type_b) {
        return g_strcmp0(name_a, name_b);
    } else {
        if(type_a == G_FILE_TYPE_DIRECTORY)
            return -1;
        else
            return 1;
    }
}

static void
xfdesktop_file_icon_menu_fill_template_menu(GtkWidget *menu,
                                            GFile *template_dir,
                                            XfdesktopFileIconManager *fmanager,
                                            gboolean recursive)
{
    g_return_if_fail(G_IS_FILE(template_dir));

    GFileEnumerator *enumerator = g_file_enumerate_children(template_dir,
                                                            XFDESKTOP_FILE_INFO_NAMESPACE,
                                                            G_FILE_QUERY_INFO_NONE,
                                                            NULL, NULL);

    if(enumerator == NULL)
        return;

    if(recursive == FALSE)
        fmanager->templates_count = 0;

    /* keep it under fmanager->max_templates otherwise the menu
     * could have tons of items and be unusable. Additionally this should
     * help in instances where the XDG_TEMPLATES_DIR has a large number of
     * files in it. */
    GList *files = NULL;
    GFileInfo *info;
    while((info = g_file_enumerator_next_file(enumerator, NULL, NULL))
          && fmanager->templates_count < fmanager->max_templates)
    {
        /* skip hidden & backup files */
        if(g_file_info_get_is_hidden(info) || g_file_info_get_is_backup(info)) {
            g_object_unref(info);
            continue;
        }

        GFile *file = g_file_get_child(template_dir, g_file_info_get_name(info));
        g_object_set_data_full(G_OBJECT(file), "info", info, g_object_unref);
        files = g_list_prepend(files, file);

        if(g_file_info_get_file_type(info) != G_FILE_TYPE_DIRECTORY)
            fmanager->templates_count++;
    }

    g_object_unref(enumerator);

    files = g_list_sort(files, compare_template_files);

    for (GList *lp = files; lp != NULL; lp = lp->next) {
        GFile *file = G_FILE(lp->data);
        info = G_FILE_INFO(g_object_get_data(G_OBJECT(file), "info"));

        /* create and fill template submenu */
        GtkWidget *submenu = NULL;
        if(g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
            submenu = gtk_menu_new();

            xfdesktop_file_icon_menu_fill_template_menu(submenu, file,
                                                        fmanager, TRUE);

            if(!gtk_container_get_children((GtkContainer*) submenu)) {
                g_object_unref(file);
                continue;
            }
        }

        /* generate a label by stripping off the extension */
        gchar *label = g_strdup(g_file_info_get_display_name(info));
        gchar *dot = g_utf8_strrchr(label, -1, '.');
        if(dot)
            *dot = '\0';

        /* determine the icon to display */
        GIcon *icon = g_file_info_get_icon(info);
        GtkWidget *image = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_MENU);

        /* allocate a new menu item */
        GtkWidget *item = xfdesktop_menu_create_menu_item_with_markup(label, image);
        g_free(label);

        /* add the item to the menu */
        gtk_widget_show(item);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

        if (submenu != NULL) {
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
        } else {
            g_object_set_data_full(G_OBJECT(item), "file",
                                   g_object_ref(file), g_object_unref);

            g_signal_connect(G_OBJECT(item), "activate",
                             G_CALLBACK(xfdesktop_file_icon_template_item_activated),
                             fmanager);
        }

        g_object_unref(file);
    }

    g_list_free(files);

    return;
}

#ifdef HAVE_THUNARX
static GtkWidget*
xfdesktop_menu_create_menu_item_from_thunarx_menu_item (GObject      *thunarx_menu_item,
                                                        GtkMenuShell *menu_to_append_item)
{
    g_return_val_if_fail(THUNARX_IS_MENU_ITEM(thunarx_menu_item), NULL);

    gchar *label, *tooltip_text, *icon_str;
    ThunarxMenu  *thunarx_menu;
    g_object_get (G_OBJECT   (thunarx_menu_item),
                  "label",   &label,
                  "tooltip", &tooltip_text,
                  "icon",    &icon_str,
                  "menu",    &thunarx_menu,
                  NULL);

    GtkWidget *img = NULL;
    if (icon_str != NULL) {
        GIcon *icon = g_icon_new_for_string(icon_str, NULL);
        if (icon != NULL) {
            img = gtk_image_new_from_gicon(icon,GTK_ICON_SIZE_MENU);
            g_object_unref(icon);
        }
    }
    GtkWidget *mi = xfce_gtk_image_menu_item_new(label, tooltip_text, NULL,
                                                 G_CALLBACK(thunarx_menu_item_activate),
                                                 G_OBJECT(thunarx_menu_item), img, menu_to_append_item);
    gtk_widget_show(mi);

    /* recursively add submenu items if any */
    if(mi != NULL && thunarx_menu != NULL) {
        GList *children = thunarx_menu_get_items(thunarx_menu);
        GtkWidget *submenu = gtk_menu_new();
        for (GList *lp = children; lp != NULL; lp = lp->next) {
            xfdesktop_menu_create_menu_item_from_thunarx_menu_item(lp->data, GTK_MENU_SHELL (submenu));
        }
        gtk_menu_item_set_submenu(GTK_MENU_ITEM (mi), submenu);
        thunarx_menu_item_list_free(children);
    }

    g_free (label);
    g_free (tooltip_text);
    g_free (icon_str);

    return mi;
}

#endif

static void
xfdesktop_settings_launch(GtkWidget *w, XfdesktopFileIconManager *fmanager) {
    gchar *cmd = g_find_program_in_path("xfdesktop-settings");
    if(!cmd)
        cmd = g_strdup(BINDIR "/xfdesktop-settings");

    GError *error = NULL;
    if (!xfce_spawn_command_line(fmanager->gscreen, cmd, FALSE, TRUE, TRUE, &error)) {
        /* printf is to be translator-friendly */
        gchar *primary = g_strdup_printf(_("Unable to launch \"%s\":"), cmd);
        xfce_message_dialog(toplevel_window_for_widget(w),
                            _("Launch Error"),
                            "dialog-error", primary, error->message,
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
        g_free(primary);
        g_clear_error(&error);
    }

    g_free(cmd);
}

static void
create_icon_view(XfdesktopFileIconManager *fmanager, XfceDesktop *desktop) {
    MonitorData *mdata = g_new0(MonitorData, 1);
    mdata->fmanager = fmanager;

    XfwScreen *screen = xfdesktop_icon_view_manager_get_screen(XFDESKTOP_ICON_VIEW_MANAGER(fmanager));
    XfconfChannel *channel = xfdesktop_icon_view_manager_get_channel(XFDESKTOP_ICON_VIEW_MANAGER(fmanager));
    XfdesktopIconView *icon_view = g_object_new(XFDESKTOP_TYPE_ICON_VIEW,
                                                "screen", screen,
                                                "channel", channel,
                                                "pixbuf-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_IMAGE,
                                                "icon-opacity-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_IMAGE_OPACITY,
                                                "text-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_LABEL,
                                                "search-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_LABEL,
                                                "sort-priority-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_SORT_PRIORITY,
                                                "tooltip-icon-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_TOOLTIP_IMAGE,
                                                "tooltip-text-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_TOOLTIP_TEXT,
                                                "row-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_ROW,
                                                "col-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_COL,
                                                NULL);
    xfdesktop_icon_view_set_selection_mode(icon_view, GTK_SELECTION_MULTIPLE);
    xfdesktop_icon_view_enable_drag_source(icon_view,
                                           GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_BUTTON1_MASK,
                                           drag_targets, G_N_ELEMENTS(drag_targets),
                                           GDK_ACTION_LINK | GDK_ACTION_COPY | GDK_ACTION_MOVE);
    xfdesktop_icon_view_enable_drag_dest(icon_view,
                                         drop_targets, G_N_ELEMENTS(drop_targets),
                                         GDK_ACTION_LINK | GDK_ACTION_COPY | GDK_ACTION_MOVE);

    g_signal_connect(icon_view, "icon-moved",
                     G_CALLBACK(xfdesktop_file_icon_manager_icon_moved), mdata);
    g_signal_connect(icon_view, "icon-activated",
                     G_CALLBACK(xfdesktop_file_icon_manager_activate_selected), fmanager);
    g_signal_connect(icon_view, "realize",
                     G_CALLBACK(xfdesktop_file_icon_manager_icon_view_realized), fmanager);
    g_signal_connect(icon_view, "unrealize",
                     G_CALLBACK(xfdesktop_file_icon_manager_icon_view_unrealized), fmanager);
    // DnD signals
    g_signal_connect(icon_view, "drag-actions-get",
                     G_CALLBACK(xfdesktop_file_icon_manager_drag_actions_get), mdata);
    g_signal_connect(icon_view, "drop-actions-get",
                     G_CALLBACK(xfdesktop_file_icon_manager_drop_actions_get), mdata);
    g_signal_connect(icon_view, "drag-drop-ask",
                     G_CALLBACK(xfdesktop_file_icon_manager_drag_drop_ask), fmanager);
    g_signal_connect(icon_view, "drag-drop-item",
                     G_CALLBACK(xfdesktop_file_icon_manager_drag_drop_item), mdata);
    g_signal_connect(icon_view, "drag-drop-items",
                     G_CALLBACK(xfdesktop_file_icon_manager_drag_drop_items), mdata);
    g_signal_connect(icon_view, "drag-data-get",
                     G_CALLBACK(xfdesktop_file_icon_manager_drag_data_get), mdata);
    g_signal_connect(icon_view, "drag-item-data-received",
                     G_CALLBACK(xfdesktop_file_icon_manager_drag_item_data_received), mdata);
    g_signal_connect(icon_view, "drop-propose-action",
                     G_CALLBACK(xfdesktop_file_icon_manager_drop_propose_action), mdata);
    // Below signals allow us to sort icons and replace them where they belong in the newly-sized view
    g_signal_connect(G_OBJECT(icon_view), "start-grid-resize",
                     G_CALLBACK(xfdesktop_file_icon_manager_start_grid_resize), mdata);
    g_signal_connect(G_OBJECT(icon_view), "end-grid-resize",
                     G_CALLBACK(xfdesktop_file_icon_manager_end_grid_resize), mdata);

    if (gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
        xfdesktop_file_icon_manager_icon_view_realized(GTK_WIDGET(icon_view), fmanager);
    }

    mdata->holder = xfdesktop_icon_view_holder_new(screen, desktop, icon_view);

    XfwMonitor *monitor = xfce_desktop_get_monitor(desktop);
    XfdesktopIconPositionLevel level;
    if (xfw_monitor_is_primary(monitor)) {
        level = XFDESKTOP_ICON_POSITION_LEVEL_PRIMARY;
    } else if (g_list_length(xfw_screen_get_monitors(screen)) == 2
               || g_list_nth_data(xfw_screen_get_monitors(screen), 0) == monitor
               || g_list_nth_data(xfw_screen_get_monitors(screen), 1) == monitor)
    {
        level = XFDESKTOP_ICON_POSITION_LEVEL_SECONDARY;
    } else {
        level = XFDESKTOP_ICON_POSITION_LEVEL_OTHER;
    }

    if (!xfdesktop_icon_position_configs_has_exact_monitor(fmanager->position_configs, monitor)) {
        mdata->position_config = xfdesktop_icon_positions_try_migrate(channel, screen, monitor, level);
        if (mdata->position_config != NULL) {
            xfdesktop_icon_position_configs_assign_monitor(fmanager->position_configs, mdata->position_config, monitor);
        }
    }

    GList *candidates = NULL;
    if (mdata->position_config == NULL) {
        mdata->position_config = xfdesktop_icon_position_configs_add_monitor(fmanager->position_configs,
                                                                             monitor,
                                                                             level,
                                                                             &candidates);
    }

    if (mdata->position_config == NULL) {
        g_assert(candidates != NULL);

        GtkBuilder *builder = gtk_builder_new_from_resource("/org/xfce/xfdesktop/monitor-candidates-chooser.glade");
        g_assert(builder != NULL);

        GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, "monitor_candidates_chooser"));

        gchar *monitor_question_text = g_strdup_printf( _("Would you like to assign an existing desktop icon layout to monitor %s?"),
                                                       xfw_monitor_get_description(monitor));
        GtkWidget *monitor_question_label = GTK_WIDGET(gtk_builder_get_object(builder, "monitor_question_label"));
        gtk_label_set_text(GTK_LABEL(monitor_question_label), monitor_question_text);
        g_free(monitor_question_text);

        GtkWidget *monitor_list_view = GTK_WIDGET(gtk_builder_get_object(builder, "monitor_list_view"));
        GtkCellRenderer *name_renderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes(N_("Monitors"),
                                                                          name_renderer,
                                                                          "text", 0,
                                                                          NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(monitor_list_view), col);

        GtkListStore *monitor_list = GTK_LIST_STORE(gtk_builder_get_object(builder, "monitor_list"));
        for (GList *lc = candidates; lc != NULL; lc = lc->next) {
            XfdesktopIconPositionConfig *candidate = lc->data;

            GList *names = xfdesktop_icon_position_config_get_monitor_display_names(candidate);
            for (GList *ln = names; ln != NULL; ln = ln->next) {
                const gchar *name = ln->data;
                GtkTreeIter iter;
                gtk_list_store_append(monitor_list, &iter);
                gtk_list_store_set(monitor_list, &iter,
                                   0, name,
                                   1, candidate,
                                   -1);
            }
            g_list_free(names);
        }

        GtkWidget *radio_auto_assign = GTK_WIDGET(gtk_builder_get_object(builder, "radio_auto_assign"));
        GtkWidget *chk_always_auto_assign = GTK_WIDGET(gtk_builder_get_object(builder, "chk_always_auto_assign"));
        g_object_bind_property(radio_auto_assign, "active", chk_always_auto_assign, "sensitive", G_BINDING_SYNC_CREATE);
        GtkWidget *radio_select_monitor = GTK_WIDGET(gtk_builder_get_object(builder, "radio_select_monitor"));

        gtk_widget_show_all(dialog);
        GtkResponseType response = gtk_dialog_run(GTK_DIALOG(dialog));
        if (response == GTK_RESPONSE_ACCEPT) {
            if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_select_monitor))) {
                GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(monitor_list_view));
                GtkTreeIter iter;
                if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
                    gtk_tree_model_get(GTK_TREE_MODEL(monitor_list), &iter, 1, &mdata->position_config, -1);
                }
            }
        }

        if (mdata->position_config == NULL) {
            // Either they selected auto-assign, didn't select anything, or
            // canceled the dialog, so we auto-assign for them.
            mdata->position_config = candidates->data;
        }

        xfdesktop_icon_position_configs_assign_monitor(fmanager->position_configs, mdata->position_config, monitor);

        g_list_free(candidates);
        g_object_unref(builder);
    }

    mdata->filter = xfdesktop_file_icon_model_filter_new(channel, fmanager->position_configs, monitor, fmanager->model);
    if (fmanager->ready) {
        xfdesktop_icon_view_set_model(icon_view, GTK_TREE_MODEL(mdata->filter));
    }

    g_hash_table_insert(fmanager->monitor_data, g_object_ref(monitor), mdata);
}

static void
update_icon_monitors(XfdesktopFileIconManager *fmanager) {
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(fmanager->model), &iter)) {
        do {
            XfdesktopFileIcon *icon = xfdesktop_file_icon_model_get_icon(fmanager->model, &iter);
            if (icon != NULL) {
                gboolean changed = FALSE;

                gchar *identifier = xfdesktop_icon_get_identifier(XFDESKTOP_ICON(icon));
                XfwMonitor *icon_monitor = NULL;
                gint row, col;
                if (xfdesktop_icon_position_configs_lookup(fmanager->position_configs, identifier, &icon_monitor, &row, &col)) {
                    changed |= xfdesktop_icon_set_monitor(XFDESKTOP_ICON(icon), icon_monitor);
                    changed |= xfdesktop_icon_set_position(XFDESKTOP_ICON(icon), row, col);
                } else {
                    XfwScreen *screen = xfdesktop_icon_view_manager_get_screen(XFDESKTOP_ICON_VIEW_MANAGER(fmanager));
                    XfwMonitor *primary = xfw_screen_get_primary_monitor(screen);
                    changed |= xfdesktop_icon_set_monitor(XFDESKTOP_ICON(icon), primary);
                }

                if (changed) {
                    GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(fmanager->model), &iter);
                    gtk_tree_model_row_changed(GTK_TREE_MODEL(fmanager->model), path, &iter);
                    gtk_tree_path_free(path);
                }
                g_free(identifier);
            }
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(fmanager->model), &iter));
    }
}

static void
xfdesktop_file_icon_manager_desktop_added(XfdesktopIconViewManager *manager, XfceDesktop *desktop) {
    TRACE("entering");

    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    create_icon_view(fmanager, desktop);
    update_icon_monitors(fmanager);
}

static void
xfdesktop_file_icon_manager_desktop_removed(XfdesktopIconViewManager *manager, XfceDesktop *desktop) {
    TRACE("entering");
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    XfwMonitor *monitor = xfce_desktop_get_monitor(desktop);
    xfdesktop_icon_position_configs_unassign_monitor(fmanager->position_configs, monitor);
    update_icon_monitors(fmanager);
    g_hash_table_remove(fmanager->monitor_data, monitor);
}

static GtkWidget *
add_menu_item(GtkWidget *menu,
              const gchar *mnemonic_text,
              GIcon *icon,
              GCallback activate_callback,
              gpointer callback_data)
{
    GtkWidget *image;
    if (icon == NULL) {
        image = gtk_image_new_from_icon_name("", GTK_ICON_SIZE_MENU);
    } else  {
        image = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_MENU);
        g_object_unref(icon);
    }

    GtkWidget *mi = xfdesktop_menu_create_menu_item_with_mnemonic(mnemonic_text, image);
    gtk_widget_show_all(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    if (activate_callback != NULL) {
        g_signal_connect(mi, "activate", activate_callback, callback_data);
    }

    return mi;
}

static void
add_menu_separator(GtkWidget *menu) {
    GtkWidget *mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
}

static GtkMenu *
xfdesktop_file_icon_manager_get_context_menu(XfdesktopIconViewManager *manager, GtkWidget *widget) {
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);

    TRACE("ENTERING");

    GtkWidget *menu = gtk_menu_new();
    gtk_menu_set_reserve_toggle_size(GTK_MENU(menu), FALSE);

    GList *selected = xfdesktop_file_icon_manager_get_selected_icons(fmanager);
    if(!selected) {
        /* assume click on the desktop itself */
        selected = g_list_append(selected, fmanager->desktop_icon);
    }

    XfdesktopFileIcon *file_icon = selected->data;
    GFileInfo *info = xfdesktop_file_icon_peek_file_info(file_icon);

    gboolean multi_sel = (g_list_length(selected) > 1);

    gboolean multi_sel_special = FALSE;
    if(multi_sel) {
        /* check if special icons are selected */
        for(GList *l = selected; l != NULL; l = l->next) {
            if(XFDESKTOP_IS_SPECIAL_FILE_ICON(l->data)
               || XFDESKTOP_IS_VOLUME_ICON(l->data))
            {
                multi_sel_special = TRUE;
                break;
            }
        }
    }

    gboolean got_custom_menu = FALSE;
    if(!multi_sel) {
        got_custom_menu = xfdesktop_icon_populate_context_menu(XFDESKTOP_ICON(selected->data),
                                                               GTK_WIDGET(menu));
    }

    /* make sure icons don't get destroyed while menu is open */
    g_list_foreach(selected, xfdesktop_object_ref, NULL);
    g_object_set_data(G_OBJECT(menu), "--xfdesktop-icon-list", selected);
    g_signal_connect(G_OBJECT(menu), "deactivate",
                     G_CALLBACK(xfdesktop_file_icon_menu_free_icon_list),
                     selected);

    if(!got_custom_menu) {
        gboolean same_app_infos = FALSE;

        if (!multi_sel) {
            same_app_infos = TRUE;
        } else if (info != NULL && g_file_info_get_content_type(info) != NULL) {
            GList *target_app_infos = g_app_info_get_all_for_type(g_file_info_get_content_type(info));

            same_app_infos = TRUE;
            for (GList *l = selected->next; l != NULL; l = l->next) {
                XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(l->data);
                GFileInfo *icon_info = xfdesktop_file_icon_peek_file_info(icon);

                if (icon_info != NULL) {
                    if (!XFDESKTOP_IS_REGULAR_FILE_ICON(icon)) {
                        same_app_infos = FALSE;
                        break;
                    } else if (g_file_info_get_content_type(icon_info) != NULL) {
                        GList *app_infos = g_app_info_get_all_for_type(g_file_info_get_content_type(icon_info));
                        GList *tail, *ail;

                        // TODO: This assumes that the app info entries are in the same order.  So far my minimal
                        // TODO: testing shows that seems to be the case, but not sure if we can rely on that.
                        for (tail = target_app_infos, ail = app_infos;
                             tail != NULL && ail != NULL;
                             tail = tail->next, ail = ail->next)
                        {
                            if (!g_app_info_equal(G_APP_INFO(tail->data), G_APP_INFO(ail->data))) {
                                same_app_infos = FALSE;
                                break;
                            }
                        }

                        g_list_free(app_infos);

                        if (!same_app_infos || tail != NULL || ail != NULL) {
                            same_app_infos = FALSE;
                            break;
                        }
                    } else {
                        same_app_infos = FALSE;
                        break;
                    }
                }
            }

            g_list_free_full(target_app_infos, g_object_unref);
        }

        if (!same_app_infos) {
            add_menu_item(menu,
                          _("_Open all"),
                          g_themed_icon_new("document-open"),
                          G_CALLBACK(xfdesktop_file_icon_menu_open_all),
                          fmanager);
            add_menu_separator(menu);
        } else if(info) {
            if(g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
                if(file_icon == fmanager->desktop_icon) {
                    /* Menu on the root desktop window */

                    /* create launcher item */
                    GtkWidget *create_launcher_mi = add_menu_item(menu,
                                                                  _("Create _Launcher..."),
                                                                  g_themed_icon_new("application-x-executable"),
                                                                  G_CALLBACK(xfdesktop_file_icon_menu_create_launcher),
                                                                  fmanager);
                    g_object_set_data(G_OBJECT(create_launcher_mi), "xfdesktop-launcher-type", "Application");

                    /* create link item */
                    GtkWidget *create_url_mi = add_menu_item(menu,
                                                             _("Create _URL Link..."),
                                                             g_themed_icon_new("insert-link"),
                                                             G_CALLBACK(xfdesktop_file_icon_menu_create_launcher),
                                                             fmanager);
                    g_object_set_data(G_OBJECT(create_url_mi), "xfdesktop-launcher-type", "Link");

                    /* create folder item */
                    add_menu_item(menu,
                                  _("Create _Folder..."),
                                  g_themed_icon_new("folder-new"),
                                  G_CALLBACK(xfdesktop_file_icon_menu_create_folder),
                                  fmanager);

                    /* create document submenu, 0 disables the sub-menu */
                    if(fmanager->max_templates > 0) {
                        GtkWidget *tmpl_mi = add_menu_item(menu,
                                                           _("Create _Document"),
                                                           g_themed_icon_new("document-new"),
                                                           NULL,
                                                           NULL);

                        GtkWidget *tmpl_menu = gtk_menu_new();
                        gtk_menu_set_reserve_toggle_size(GTK_MENU(tmpl_menu), FALSE);
                        gtk_menu_item_set_submenu(GTK_MENU_ITEM(tmpl_mi), tmpl_menu);

                        /* check if XDG_TEMPLATES_DIR="$HOME" and don't show
                         * templates if so. */
                        GFile *home_dir = g_file_new_for_path(xfce_get_homedir());
                        const gchar *templates_dir_path = g_get_user_special_dir(G_USER_DIRECTORY_TEMPLATES);
                        GFile *templates_dir = NULL;
                        if(templates_dir_path) {
                            templates_dir = g_file_new_for_path(templates_dir_path);
                        }

                        if(templates_dir && !g_file_equal(home_dir, templates_dir)) {
                            xfdesktop_file_icon_menu_fill_template_menu(tmpl_menu,
                                                                        templates_dir,
                                                                        fmanager,
                                                                        FALSE);
                        }

                        if(!gtk_container_get_children((GtkContainer*) tmpl_menu)) {
                            GtkWidget *no_tmpl_mi = add_menu_item(tmpl_menu,
                                                                  _("No templates installed"),
                                                                   NULL,
                                                                   NULL,
                                                                   NULL);
                            gtk_widget_set_sensitive(no_tmpl_mi, FALSE);
                        }

                        if (templates_dir) {
                            g_object_unref(templates_dir);
                        }
                        g_object_unref(home_dir);

                        add_menu_separator(tmpl_menu);

                        /* add the "Empty File" template option */
                        add_menu_item(tmpl_menu,
                                      _("_Empty File"),
                                      g_themed_icon_new("text-x-generic"),
                                      G_CALLBACK(xfdesktop_file_icon_template_item_activated),
                                      fmanager);
                    }
                } else {
                    /* Menu on folder icons */
                    add_menu_item(menu,
                                  multi_sel ? _("_Open All") : _("_Open"),
                                  g_themed_icon_new("document-open"),
                                  G_CALLBACK(xfdesktop_file_icon_menu_open_folder),
                                  fmanager);
                }

                add_menu_separator(menu);
            } else {
                /* Menu on non-folder icons */
                GList *app_infos = NULL;

                if(xfdesktop_file_utils_file_is_executable(info)) {
                    add_menu_item(menu,
                                  _("_Execute"),
                                  g_themed_icon_new("system-run"),
                                  G_CALLBACK(xfdesktop_file_icon_menu_executed),
                                  fmanager);

                    add_menu_separator(menu);

                    if(g_content_type_equals(g_file_info_get_content_type(info),
                                             "application/x-desktop"))
                    {
                        GFile *file = xfdesktop_file_icon_peek_file(file_icon);

                        GtkWidget *edit_launcher_mi = add_menu_item(menu,
                                                                    _("Edit _Launcher"),
                                                                    g_themed_icon_new("gtk-edit"),
                                                                    G_CALLBACK(xfdesktop_file_icon_menu_create_launcher),
                                                                    fmanager);
                        g_object_set_data_full(G_OBJECT(edit_launcher_mi), "file", g_object_ref(file), g_object_unref);

                        add_menu_separator(menu);
                    }
                }

                app_infos = g_app_info_get_all_for_type(g_file_info_get_content_type(info));
                if(app_infos) {
                    GAppInfo *app_info, *default_application;
                    GList *ap;
                    gint list_len = 0;

                    /* move any default application in front of the list */
                    default_application = g_app_info_get_default_for_type (g_file_info_get_content_type(info), FALSE);
                    if(G_LIKELY (default_application != NULL))
                    {
                        for (ap = app_infos; ap != NULL; ap = ap->next)
                        {
                            if(g_app_info_equal (ap->data, default_application))
                            {
                                g_object_unref (ap->data);
                                app_infos = g_list_delete_link (app_infos, ap);
                                break;
                            }
                        }
                        app_infos = g_list_prepend (app_infos, default_application);
                    }

                    app_info = G_APP_INFO(app_infos->data);

                    GtkWidget *app_info_mi = xfdesktop_menu_item_from_app_info(fmanager, file_icon,
                                                                               app_info, TRUE, TRUE);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), app_info_mi);

                    g_object_unref(app_info);

                    GtkWidget *app_infos_menu;
                    if(app_infos->next) {
                        list_len = g_list_length(app_infos->next);

                        if(!xfdesktop_file_utils_file_is_executable(info)
                           && list_len < 3)
                        {
                            add_menu_separator(menu);
                        }

                        if(list_len >= 3) {
                            GtkWidget *open_with_mi = add_menu_item(menu,
                                                                    _("Ope_n With"),
                                                                    NULL,
                                                                    NULL,
                                                                    NULL);

                            app_infos_menu = gtk_menu_new();
                            gtk_menu_set_reserve_toggle_size(GTK_MENU(app_infos_menu), FALSE);
                            gtk_menu_item_set_submenu(GTK_MENU_ITEM(open_with_mi), app_infos_menu);
                        } else {
                            app_infos_menu = menu;
                        }

                        for(GList *l = app_infos->next; l; l = l->next) {
                            app_info = G_APP_INFO(l->data);
                            GtkWidget *mi = xfdesktop_menu_item_from_app_info(fmanager,
                                                                              file_icon, app_info,
                                                                              FALSE, TRUE);
                            gtk_menu_shell_append(GTK_MENU_SHELL(app_infos_menu), mi);
                            g_object_unref(app_info);
                        }

                        if(list_len >= 3) {
                            add_menu_separator(app_infos_menu);
                        }
                    } else {
                        app_infos_menu = menu;
                    }

                    add_menu_item(list_len >= 3 ? app_infos_menu : menu,
                                  _("Ope_n With Other Application..."),
                                  NULL,
                                  G_CALLBACK(xfdesktop_file_icon_menu_other_app),
                                  fmanager);

                    add_menu_separator(menu);

                    add_menu_item(list_len >= 3 ? app_infos_menu : menu,
                                  _("Set Defa_ult Application..."),
                                  NULL,
                                  G_CALLBACK(xfdesktop_file_icon_menu_set_default_app),
                                  fmanager);

                    g_list_free(app_infos);
                } else {
                    add_menu_item(menu,
                                  _("Ope_n With Other Application..."),
                                  g_themed_icon_new("document-open"),
                                  G_CALLBACK(xfdesktop_file_icon_menu_other_app),
                                  fmanager);
                }

                add_menu_separator(menu);
            }
        }

        if(file_icon == fmanager->desktop_icon) {
            /* Menu on the root desktop window */
            /* Paste */
            GtkWidget *paste_mi = add_menu_item(menu,
                                                _("_Paste"),
                                                g_themed_icon_new("edit-paste"),
                                                G_CALLBACK(xfdesktop_file_icon_menu_paste),
                                                fmanager);
            gtk_widget_set_sensitive(paste_mi, xfdesktop_clipboard_manager_get_can_paste(clipboard_manager));

            add_menu_separator(menu);
        } else if(!multi_sel_special) {
            /* Menu popup on an icon */
            /* Cut */
            GtkWidget *cut_mi = add_menu_item(menu,
                                              _("Cu_t"),
                                              g_themed_icon_new("edit-cut"),
                                              G_CALLBACK(xfdesktop_file_icon_menu_cut),
                                              fmanager);
            gtk_widget_set_sensitive(cut_mi, multi_sel || xfdesktop_file_icon_can_delete_file(file_icon));

            /* Copy */
            add_menu_item(menu,
                          _("_Copy"),
                          g_themed_icon_new("edit-copy"),
                          G_CALLBACK(xfdesktop_file_icon_menu_copy),
                          fmanager);

            /* Paste Into Folder */
            if (!multi_sel
                && info != NULL
                && g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY
                && g_file_info_get_attribute_boolean(info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
            {
                GtkWidget *paste_mi = add_menu_item(menu,
                                                    _("Paste Into Folder"),
                                                    g_themed_icon_new("edit-paste"),
                                                    G_CALLBACK(xfdesktop_file_icon_menu_paste_into_folder),
                                                    fmanager);
                gtk_widget_set_sensitive(paste_mi, xfdesktop_clipboard_manager_get_can_paste(clipboard_manager));
            }

            add_menu_separator(menu);

            /* Trash */
            GtkWidget *trash_mi = add_menu_item(menu,
                                                _("Mo_ve to Trash"),
                                                g_themed_icon_new("user-trash"),
                                                G_CALLBACK(xfdesktop_file_icon_menu_trash),
                                                fmanager);
            gtk_widget_set_sensitive(trash_mi, multi_sel || xfdesktop_file_icon_can_delete_file(file_icon));

            /* Delete */
            if(fmanager->show_delete_menu) {
                GtkWidget *delete_mi = add_menu_item(menu,
                                                     _("_Delete"),
                                                     g_themed_icon_new("edit-delete"),
                                                     G_CALLBACK(xfdesktop_file_icon_menu_delete),
                                                     fmanager);
                gtk_widget_set_sensitive(delete_mi, multi_sel || xfdesktop_file_icon_can_delete_file(file_icon));
            }

            add_menu_separator(menu);

            /* Rename */
            GtkWidget *rename_mi = add_menu_item(menu,
                                                 _("_Rename..."),
                                                 NULL,
                                                 G_CALLBACK(xfdesktop_file_icon_menu_rename),
                                                 fmanager);
            gtk_widget_set_sensitive(rename_mi, multi_sel || xfdesktop_file_icon_can_rename_file(file_icon));

            add_menu_separator(menu);
        }

#ifdef HAVE_THUNARX
        if(!multi_sel_special && fmanager->thunarx_menu_providers) {
            GtkWidget *gtk_menu_item = NULL;

            for(GList *l = fmanager->thunarx_menu_providers; l; l = l->next) {
                ThunarxMenuProvider *provider = THUNARX_MENU_PROVIDER(l->data);

                GtkWidget *toplevel = GTK_WIDGET(toplevel_window_for_widget(widget));
                GList *menu_items = NULL;
                if(selected->data == fmanager->desktop_icon) {
                    /* click on the desktop itself, only show folder actions */
                    menu_items = thunarx_menu_provider_get_folder_menu_items(provider,
                                                                             toplevel,
                                                                             THUNARX_FILE_INFO(file_icon));
                }
                else {
                    /* thunar file specific actions (allows them to operate on folders
                        * that are on the desktop as well) */
                    menu_items = thunarx_menu_provider_get_file_menu_items(provider, toplevel, selected);
                }

                for (GList *lp_item = menu_items; lp_item != NULL; lp_item = lp_item->next) {
                    gtk_menu_item = xfdesktop_menu_create_menu_item_from_thunarx_menu_item(lp_item->data,
                                                                                           GTK_MENU_SHELL(menu));

                    /* Each thunarx_menu_item will be destroyed together with its related gtk_menu_item*/
                    g_signal_connect_swapped(G_OBJECT(gtk_menu_item), "destroy",
                                             G_CALLBACK(g_object_unref), lp_item->data);
                }

                g_list_free (menu_items);
            }

            if (gtk_menu_item != NULL) {
                add_menu_separator(menu);
            }
        }
#endif

        if(file_icon == fmanager->desktop_icon) {
            /* Menu on the root desktop window */
            add_menu_item(menu,
                          _("_Open in New Window"),
                          g_themed_icon_new("document-open"),
                          G_CALLBACK(xfdesktop_file_icon_menu_open_desktop),
                          fmanager);

            /* show arrange desktop icons option */
            add_menu_item(menu,
                          _("Arrange Desktop _Icons"),
                          g_themed_icon_new("view-sort-ascending"),
                          G_CALLBACK(xfdesktop_file_icon_menu_arrange_icons),
                          fmanager);

            g_assert(XFCE_IS_DESKTOP(widget));  // XXX
            XfceDesktop *desktop = XFCE_DESKTOP(widget);
            XfwMonitor *monitor = xfce_desktop_get_monitor(desktop);

            XfwScreen *xfw_screen = xfdesktop_icon_view_manager_get_screen(XFDESKTOP_ICON_VIEW_MANAGER(fmanager));
            XfwWorkspaceManager *workspace_manager = xfw_screen_get_workspace_manager(xfw_screen);
            XfwWorkspaceGroup *group = NULL;
            if (monitor != NULL) {
                for (GList *l = xfw_workspace_manager_list_workspace_groups(workspace_manager); l != NULL; l = l->next) {
                    if (g_list_find(xfw_workspace_group_get_monitors(XFW_WORKSPACE_GROUP(l->data)), monitor)) {
                        group = XFW_WORKSPACE_GROUP(l->data);
                        break;
                    }
                }
            }
            XfwWorkspace *workspace = group != NULL ? xfw_workspace_group_get_active_workspace(group) : NULL;

            if (monitor != NULL &&
                workspace != NULL &&
                xfdesktop_backdrop_manager_can_cycle_backdrop(fmanager->backdrop_manager, monitor, workspace))
            {
                /* show next background option */
                add_menu_item(menu,
                              _("_Next Background"),
                              g_themed_icon_new("go-next"),
                              G_CALLBACK(xfdesktop_file_icon_menu_next_background),
                              fmanager);
            }

            add_menu_separator(menu);

            /* Desktop settings window */
            add_menu_item(menu,
                          _("Desktop _Settings..."),
                          g_themed_icon_new("preferences-desktop-wallpaper"),
                          G_CALLBACK(xfdesktop_settings_launch),
                          fmanager);
        } else {
            /* Properties - applies only to icons on the desktop */
            GtkWidget *properties_mi = add_menu_item(menu,
                                                     _("_Properties..."),
                                                     g_themed_icon_new("document-properties"),
                                                     G_CALLBACK(file_icon == fmanager->desktop_icon
                                                                ? xfdesktop_file_icon_manager_desktop_properties
                                                                : xfdesktop_file_icon_menu_properties),
                                                     fmanager);
            gtk_widget_set_sensitive(properties_mi, info != NULL);
        }
    }

    /* don't free |selected|.  the menu deactivated handler does that */

    return GTK_MENU(menu);
}

static void
xfdesktop_file_icon_manager_sort_icons(XfdesktopIconViewManager *manager, GtkSortType sort_type) {
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    // XXX: should we rearrange within each monitor, or rearrange all onto the primary?

    GHashTableIter iter;
    g_hash_table_iter_init(&iter, fmanager->monitor_data);

    MonitorData *mdata;
    while (g_hash_table_iter_next(&iter, NULL, (gpointer)&mdata)) {
        XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
        xfdesktop_icon_view_sort_icons(icon_view, sort_type);
    }
}

static XfwMonitor *
xfdesktop_file_icon_manager_get_cached_icon_position(XfdesktopFileIconManager *fmanager,
                                                     XfdesktopFileIcon *icon,
                                                     gint16 *ret_row,
                                                     gint16 *ret_col)
{
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON_MANAGER(fmanager), NULL);
    g_return_val_if_fail(ret_row != NULL && ret_col != NULL, NULL);

    gchar *identifier = xfdesktop_icon_get_identifier(XFDESKTOP_ICON(icon));
    XfwMonitor *monitor = NULL;
    gint row = -1;
    gint col = -1;
    gboolean success = xfdesktop_icon_position_configs_lookup(fmanager->position_configs,
                                                              identifier,
                                                              &monitor,
                                                              &row,
                                                              &col);
    g_free(identifier);
    if (success) {
        *ret_row = row;
        *ret_col = col;
        return monitor;
    } else {
        *ret_row = -1;
        *ret_col = -1;
        return xfw_screen_get_primary_monitor(xfdesktop_icon_view_manager_get_screen(XFDESKTOP_ICON_VIEW_MANAGER(fmanager)));
    }
}

static void
update_icon_position(MonitorData *mdata, XfdesktopFileIcon *icon, gint row, gint col) {
    xfdesktop_icon_set_position(XFDESKTOP_ICON(icon), row, col);

    gchar *identifier = xfdesktop_icon_get_identifier(XFDESKTOP_ICON(icon));
    guint64 last_seen = XFDESKTOP_IS_VOLUME_ICON(icon) ? g_get_real_time() : 0;
    xfdesktop_icon_position_configs_set_icon_position(mdata->fmanager->position_configs,
                                                      mdata->position_config,
                                                      identifier,
                                                      row,
                                                      col,
                                                      last_seen);
    g_free(identifier);

    GtkTreeIter iter;
    if (xfdesktop_file_icon_model_get_icon_iter(mdata->fmanager->model, icon, &iter)) {
        GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(mdata->fmanager->model), &iter);
        gtk_tree_model_row_changed(GTK_TREE_MODEL(mdata->fmanager->model), path, &iter);
        gtk_tree_path_free(path);
    }
}

static void
clear_icon_position(MonitorData *mdata, XfdesktopFileIcon *icon) {
    xfdesktop_icon_set_position(XFDESKTOP_ICON(icon), -1, -1);
    
    gchar *identifier = xfdesktop_icon_get_identifier(XFDESKTOP_ICON(icon));
    xfdesktop_icon_position_configs_remove_icon(mdata->fmanager->position_configs, mdata->position_config, identifier);
    g_free(identifier);
}

static gint
linear_pos(gint row, gint nrows, gint col, gint ncols) {
    if (row < 0 || row >= nrows || col < 0 || col >= ncols) {
        return -1;
    } else {
        return col * nrows + row;
    }
}

static void
xfdesktop_file_icon_manager_start_grid_resize(XfdesktopIconView *icon_view,
                                              gint new_rows,
                                              gint new_cols,
                                              MonitorData *mdata)
{
    if (!mdata->fmanager->ready) {
        return;
    }

    GHashTable *placed_icons = g_hash_table_new(g_direct_hash, g_direct_equal);
    GQueue *pending_icons = g_queue_new();

    // Remove the model from the icon view so the changes we make here won't affect the view.
    xfdesktop_icon_view_set_model(icon_view, NULL);

    GtkTreeIter iter;
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(mdata->filter), &iter)) {
        do {
            XfdesktopFileIcon *icon = xfdesktop_file_icon_model_filter_get_icon(mdata->filter, &iter);
            gint16 row, col;

            if(xfdesktop_file_icon_manager_get_cached_icon_position(mdata->fmanager, icon, &row, &col)) {
                // If we have a cached position, we assume it's authoritative, unless it's invalid.
                gint pos = linear_pos(row, new_rows, col, new_cols);
                if (pos >= 0) {
                    update_icon_position(mdata, icon, row, col);
                    g_hash_table_insert(placed_icons, GINT_TO_POINTER(pos), icon);
                } else {
                    clear_icon_position(mdata, icon);
                }
            } else {
                // We'll try again after we've dealt with all the cached icons.
                g_queue_push_tail(pending_icons, icon);
            }
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(mdata->filter), &iter));
    }

    XfdesktopFileIcon *pending_icon;
    while ((pending_icon = g_queue_pop_head(pending_icons)) != NULL) {
        gint16 row, col;

        if (xfdesktop_icon_get_position(XFDESKTOP_ICON(pending_icon), &row, &col)) {
            // This icon was positioned pre-grid-resize, but didn't have a cached position
            // (perhaps it is new since we last used the new grid size), so we can attempt
            // to use its old position, but must allow another icon we've already placed
            // in the same spot to preempt it.
            gint pos = linear_pos(row, new_rows, col, new_cols);

            if (pos >= 0 && g_hash_table_lookup(placed_icons, GINT_TO_POINTER(pos)) == NULL) {
                update_icon_position(mdata, pending_icon, row, col);
                g_hash_table_insert(placed_icons, GINT_TO_POINTER(pos), pending_icon);
            } else {
                clear_icon_position(mdata, pending_icon);
            }
        }
    }

    g_hash_table_destroy(placed_icons);
    g_queue_free(pending_icons);

    // We leave the model unset until the grid resize ends (see below).
}

static void
xfdesktop_file_icon_manager_end_grid_resize(XfdesktopIconView *icon_view, MonitorData *mdata) {
    if (mdata->fmanager->ready) {
        // Re-set the model after the resize is done so the view can repopulate itself.
        xfdesktop_icon_view_set_model(icon_view, GTK_TREE_MODEL(mdata->filter));
    }
}

static void
xfdesktop_file_icon_manager_refresh_icons(XfdesktopFileIconManager *fmanager)
{
    fmanager->ready = FALSE;

    GHashTableIter iter;
    g_hash_table_iter_init(&iter, fmanager->monitor_data);

    MonitorData *mdata;
    while (g_hash_table_iter_next(&iter, NULL, (gpointer)&mdata)) {
        XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
        xfdesktop_icon_view_set_model(icon_view, NULL);
    }
    xfdesktop_file_icon_model_reload(fmanager->model);
}

static GList *
xfdesktop_file_icon_manager_get_selected_icons(XfdesktopFileIconManager *fmanager)
{
    GList *selected_icons = NULL;

    GHashTableIter hiter;
    g_hash_table_iter_init(&hiter, fmanager->monitor_data);

    MonitorData *mdata;
    while (g_hash_table_iter_next(&hiter, NULL, (gpointer)&mdata)) {

        GList *selected = xfdesktop_icon_view_get_selected_items(xfdesktop_icon_view_holder_get_icon_view(mdata->holder));
        for (GList *l = selected; l != NULL; l = l->next) {
            GtkTreePath *path = (GtkTreePath *)l->data;
            GtkTreeIter iter;

            if (gtk_tree_model_get_iter(GTK_TREE_MODEL(mdata->filter), &iter, path)) {
                XfdesktopFileIcon *icon = xfdesktop_file_icon_model_filter_get_icon(mdata->filter, &iter);
                if (icon != NULL) {
                    selected_icons = g_list_prepend(selected_icons, icon);
                }
            }
        }
        g_list_free(selected);
    }

    return selected_icons;
}

static gboolean
xfdesktop_file_icon_manager_key_press(GtkWidget *widget, GdkEventKey *evt, XfdesktopFileIconManager *fmanager) {
    switch(evt->keyval) {
        case GDK_KEY_Delete:
        case GDK_KEY_KP_Delete: {
            gboolean force_delete = evt->state & GDK_SHIFT_MASK;
            xfdesktop_file_icon_manager_delete_selected(fmanager, widget, force_delete);
            break;
        }

        case GDK_KEY_c:
        case GDK_KEY_C: {
            if(!(evt->state & GDK_CONTROL_MASK)
               || (evt->state & (GDK_SHIFT_MASK|GDK_MOD1_MASK|GDK_MOD4_MASK)))
            {
                return FALSE;
            }
            GList *selected_icons = xfdesktop_file_icon_manager_get_selected_icons(fmanager);
            if (selected_icons != NULL) {
                xfdesktop_clipboard_manager_copy_files(clipboard_manager,
                                                       selected_icons);
                g_list_free(selected_icons);
            }

            break;
        }

        case GDK_KEY_x:
        case GDK_KEY_X: {
            if(!(evt->state & GDK_CONTROL_MASK)
               || (evt->state & (GDK_SHIFT_MASK|GDK_MOD1_MASK|GDK_MOD4_MASK)))
            {
                return FALSE;
            }
            GList *selected_icons = xfdesktop_file_icon_manager_get_selected_icons(fmanager);
            if (selected_icons != NULL) {
                xfdesktop_clipboard_manager_cut_files(clipboard_manager,
                                                       selected_icons);
                g_list_free(selected_icons);
            }
            return TRUE;
        }

        case GDK_KEY_v:
        case GDK_KEY_V:
            if(!(evt->state & GDK_CONTROL_MASK)
               || (evt->state & (GDK_SHIFT_MASK|GDK_MOD1_MASK|GDK_MOD4_MASK)))
            {
                return FALSE;
            }
            if(xfdesktop_clipboard_manager_get_can_paste(clipboard_manager)) {
                xfdesktop_clipboard_manager_paste_files(clipboard_manager, fmanager->folder, widget, NULL);
            }
            return TRUE;

        case GDK_KEY_n:
        case GDK_KEY_N:
            if(!(evt->state & GDK_CONTROL_MASK && evt->state & GDK_SHIFT_MASK)
               || (evt->state & (GDK_MOD1_MASK|GDK_MOD4_MASK)))
            {
                return FALSE;
            }
            xfdesktop_file_icon_menu_create_folder(NULL, fmanager);
            return TRUE;

        case GDK_KEY_r:
        case GDK_KEY_R:
            if(!(evt->state & GDK_CONTROL_MASK)
               || (evt->state & (GDK_SHIFT_MASK|GDK_MOD1_MASK|GDK_MOD4_MASK)))
            {
                return FALSE;
            }
            /* fall through */
        case GDK_KEY_F5:
            xfdesktop_file_icon_manager_refresh_icons(fmanager);
            return TRUE;

        case GDK_KEY_F2: {
            GList *selected_icons = xfdesktop_file_icon_manager_get_selected_icons(fmanager);
            if (selected_icons != NULL && selected_icons->next == NULL) {
                XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(selected_icons->data);
                if(xfdesktop_file_icon_can_rename_file(icon)) {
                    xfdesktop_file_icon_menu_rename(NULL, fmanager);
                    return TRUE;
                }
            }
            g_list_free(selected_icons);
            break;
        }
    }

    return FALSE;
}

static void
xfdesktop_file_icon_manager_clipboard_changed(XfdesktopClipboardManager *cmanager, XfdesktopFileIconManager *fmanager) {
    GHashTableIter hiter;
    g_hash_table_iter_init(&hiter, fmanager->monitor_data);

    MonitorData *mdata;
    while (g_hash_table_iter_next(&hiter, NULL, (gpointer)&mdata)) {
        XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);

        TRACE("entering");

        GtkTreeIter iter;
        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(mdata->filter), &iter)) {
            do {
                XfdesktopFileIcon *icon = xfdesktop_file_icon_model_filter_get_icon(mdata->filter, &iter);
                if (icon != NULL) {
                    gboolean is_cut = xfdesktop_clipboard_manager_has_cutted_file(clipboard_manager, icon);
                    xfdesktop_icon_view_set_item_sensitive(icon_view, &iter, !is_cut);
                }
            } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(mdata->filter), &iter));
        }
    }
}

static GdkDragAction
xfdesktop_file_icon_manager_drag_actions_get(XfdesktopIconView *icon_view,
                                             GtkTreeIter *iter,
                                             MonitorData *mdata)
{
    XfdesktopFileIcon *icon = xfdesktop_file_icon_model_filter_get_icon(mdata->filter, iter);
    return icon != NULL ? xfdesktop_icon_get_allowed_drag_actions(XFDESKTOP_ICON(icon)) : 0;
}

static GdkDragAction
xfdesktop_file_icon_manager_drop_actions_get(XfdesktopIconView *icon_view,
                                             GtkTreeIter *iter,
                                             GdkDragAction *suggested_action,
                                             MonitorData *mdata)
{
    XfdesktopFileIcon *icon = xfdesktop_file_icon_model_filter_get_icon(mdata->filter, iter);
    return icon != NULL ? xfdesktop_icon_get_allowed_drop_actions(XFDESKTOP_ICON(icon), suggested_action) : 0;
}

static gboolean
xfdesktop_file_icon_manager_drag_drop_items(XfdesktopIconView *icon_view,
                                            GdkDragContext *context,
                                            GtkTreeIter *iter,
                                            GList *dropped_item_paths,
                                            GdkDragAction action,
                                            MonitorData *mdata)
{
    g_return_val_if_fail(iter != NULL, FALSE);
    g_return_val_if_fail(dropped_item_paths != NULL, FALSE);

    XfdesktopFileIcon *drop_icon = xfdesktop_file_icon_model_filter_get_icon(mdata->filter, iter);
    g_return_val_if_fail(drop_icon != NULL, FALSE);

    GList *dropped_icons = NULL;
    for (GList *l = dropped_item_paths; l != NULL; l = l->next) {
        GtkTreePath *path = (GtkTreePath *)l->data;
        GtkTreeIter drop_iter;

        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(mdata->filter), &drop_iter, path)) {
            XfdesktopFileIcon *icon = xfdesktop_file_icon_model_filter_get_icon(mdata->filter, &drop_iter);

            if (icon != NULL) {
                dropped_icons = g_list_prepend(dropped_icons, icon);
            }
        }
    }
    dropped_icons = g_list_reverse(dropped_icons);

    g_return_val_if_fail(dropped_icons != NULL, FALSE);

    gboolean ret = xfdesktop_icon_do_drop_dest(XFDESKTOP_ICON(drop_icon), dropped_icons, action);

    g_list_free(dropped_icons);

    return ret;
}

static gboolean
xfdesktop_file_icon_manager_drag_drop_item(XfdesktopIconView *icon_view,
                                           GdkDragContext *context,
                                           GtkTreeIter *iter,
                                           gint row,
                                           gint col,
                                           guint time_,
                                           MonitorData *mdata)
{
    TRACE("entering");

    XfdesktopFileIcon *drop_icon = iter != NULL
        ? xfdesktop_file_icon_model_filter_get_icon(mdata->filter, iter)
        : NULL;
    GtkWidget *widget = GTK_WIDGET(icon_view);
    GdkAtom target = gtk_drag_dest_find_target(widget, context, mdata->fmanager->drop_targets);
    if (target == GDK_NONE) {
        return FALSE;
    } else if (target == gdk_atom_intern("XdndDirectSave0", FALSE)) {
        /* X direct save protocol implementation copied more or less from
         * Thunar, Copyright (c) Benedikt Meurer */
        GFile *source_file;
        if(drop_icon) {
            GFileInfo *info = xfdesktop_file_icon_peek_file_info(XFDESKTOP_FILE_ICON(drop_icon));
            if(!info)
                return FALSE;

            if(g_file_info_get_file_type(info) != G_FILE_TYPE_DIRECTORY)
                return FALSE;

            source_file = xfdesktop_file_icon_peek_file(XFDESKTOP_FILE_ICON(drop_icon));

        } else
            source_file = mdata->fmanager->folder;

        gboolean ret = FALSE;
        guchar *prop_val = NULL;
        gint prop_len;
        if(gdk_property_get(gdk_drag_context_get_source_window(context),
                            gdk_atom_intern("XdndDirectSave0", FALSE),
                            gdk_atom_intern("text/plain", FALSE),
                            0, 1024, FALSE, NULL, NULL, &prop_len,
                            &prop_val)
           && prop_val != NULL)
        {
            gchar *prop_text = g_strndup((gchar *)prop_val, prop_len);

            GFile *file = g_file_resolve_relative_path(source_file, (const gchar *)prop_text);
            gchar *uri = g_file_get_uri(file);
            g_object_unref(file);

            gdk_property_change(gdk_drag_context_get_source_window(context),
                                gdk_atom_intern("XdndDirectSave0", FALSE),
                                gdk_atom_intern("text/plain", FALSE), 8,
                                GDK_PROP_MODE_REPLACE, (const guchar *)uri,
                                strlen(uri));

            g_free(prop_text);
            g_free(uri);
            ret = TRUE;
        }
        g_free(prop_val);

        return ret;
    } else if(target == gdk_atom_intern("_NETSCAPE_URL", FALSE)) {
        if(drop_icon) {
            /* don't allow a drop on an icon that isn't a folder (i.e., not
             * on an icon that's an executable */
            GFileInfo *info = xfdesktop_file_icon_peek_file_info(XFDESKTOP_FILE_ICON(drop_icon));
            if(!info || g_file_info_get_file_type(info) != G_FILE_TYPE_DIRECTORY)
                return FALSE;
        }
    }

    TRACE("target good");

    gtk_drag_get_data(widget, context, target, time_);

    return TRUE;
}

static void
xfdesktop_dnd_item(GtkWidget *item, GdkDragAction *action) {
    *action = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(item), "action"));
}

/**
 * Portions of this code was copied from thunar-dnd.c
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 **/
static GdkDragAction
xfdesktop_file_icon_manager_drag_drop_ask(XfdesktopIconView *icon_view,
                                          GdkDragContext *context,
                                          GtkTreeIter *iter,
                                          gint row,
                                          gint col,
                                          guint time_,
                                          XfdesktopFileIconManager *fmanager)
{
    static const struct {
        GdkDragAction drag_action;
        const gchar *name;
        const gchar *icon_name;
    } actions[] = {
        { GDK_ACTION_COPY, N_("Copy _Here"), "stock_folder-copy" },
        { GDK_ACTION_MOVE, N_("_Move Here"), "stock_folder-move" },
        { GDK_ACTION_LINK, N_("_Link Here"), "insert-link" },
    };

    GtkWidget *menu = gtk_menu_new();
    gtk_menu_set_reserve_toggle_size(GTK_MENU(menu), FALSE);

    /* This adds the Copy, Move, & Link options */
    GdkDragAction selected_action = 0;
    for(guint menu_item = 0; menu_item < G_N_ELEMENTS(actions); menu_item++) {
        GIcon *icon = g_themed_icon_new(actions[menu_item].icon_name);
        GtkWidget *item = add_menu_item(menu,
                                        _(actions[menu_item].name),
                                        icon,
                                        G_CALLBACK(xfdesktop_dnd_item),
                                        &selected_action);
        g_object_set_data(G_OBJECT(item), "action", GUINT_TO_POINTER(actions[menu_item].drag_action));
    }

    add_menu_separator(menu);

    /* Cancel option */
    GtkWidget *cancel_mi = add_menu_item(menu,
                                         _("_Cancel"),
                                         g_themed_icon_new("gtk-cancel"),
                                         G_CALLBACK(xfdesktop_dnd_item),
                                         &selected_action);
    g_object_set_data(G_OBJECT(cancel_mi), "action", GUINT_TO_POINTER(0));

    gtk_widget_show(menu);
    g_object_ref_sink(G_OBJECT(menu));

    /* Loop until we get a user selected action */
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    guint signal_id = g_signal_connect_swapped(G_OBJECT(menu), "deactivate", G_CALLBACK(g_main_loop_quit), loop);
    xfce_gtk_menu_popup_until_mapped(GTK_MENU(menu), NULL, NULL, NULL, NULL, 3, time_);
    g_main_loop_run(loop);
    g_signal_handler_disconnect(G_OBJECT(menu), signal_id);
    g_main_loop_unref(loop);

    g_object_unref(G_OBJECT(menu));

    return selected_action;
}

static void
xfdesktop_file_icon_manager_drag_item_data_received(XfdesktopIconView *icon_view,
                                                    GdkDragContext *context,
                                                    GtkTreeIter *iter,
                                                    gint row,
                                                    gint col,
                                                    GtkSelectionData *data,
                                                    guint info,
                                                    guint time_,
                                                    MonitorData *mdata)
{
    TRACE("entering");

    gboolean copy_only = TRUE, drop_ok = FALSE;
    XfdesktopFileIcon *drop_icon = iter != NULL
        ? xfdesktop_file_icon_model_filter_get_icon(mdata->filter, iter)
        : NULL;
    GdkDragAction action = gdk_drag_context_get_selected_action(context);

    if (action == GDK_ACTION_ASK) {
        action = xfdesktop_file_icon_manager_drag_drop_ask(icon_view, context, iter, row, col, time_, mdata->fmanager);
        if(action == 0) {
            gtk_drag_finish(context, FALSE, FALSE, time_);
            return;
        }
    }

    if(info == TARGET_XDND_DIRECT_SAVE0) {
        /* FIXME: we don't support XdndDirectSave stage 3, result F, i.e.,
         * the app has to save the data itself given the filename we provided
         * in stage 1 */
        if(8 == gtk_selection_data_get_format(data)
           && 1 == gtk_selection_data_get_length(data)
           && 'F' == gtk_selection_data_get_data(data)[0])
        {
            gdk_property_change(gdk_drag_context_get_source_window(context),
                                gdk_atom_intern("XdndDirectSave0", FALSE),
                                gdk_atom_intern("text/plain", FALSE), 8,
                                GDK_PROP_MODE_REPLACE, (const guchar *)"", 0);
        }

        drop_ok = TRUE;
    } else if(info == TARGET_NETSCAPE_URL) {
        /* data is "URL\nTITLE" */
        GFile *source_file = NULL;
        if(drop_icon) {
            GFileInfo *finfo = xfdesktop_file_icon_peek_file_info(XFDESKTOP_FILE_ICON(drop_icon));
            if(finfo != NULL && g_file_info_get_file_type(finfo) == G_FILE_TYPE_DIRECTORY)
                source_file = xfdesktop_file_icon_peek_file(XFDESKTOP_FILE_ICON(drop_icon));
        } else
            source_file = mdata->fmanager->folder;

        gchar *exo_desktop_item_edit = g_find_program_in_path("exo-desktop-item-edit");
        if(source_file && exo_desktop_item_edit) {
            gchar **parts = g_strsplit((const gchar *)gtk_selection_data_get_data(data), "\n", -1);

            if(2 == g_strv_length(parts)) {
                gchar *cwd = g_file_get_uri(source_file);
                gchar *myargv[16];
                gint i = 0;

                /* use the argv form so we don't have to worry about quoting
                 * the link title */
                myargv[i++] = exo_desktop_item_edit;
                myargv[i++] = "--type=Link";
                myargv[i++] = "--url";
                myargv[i++] = parts[0];
                myargv[i++] = "--name";
                myargv[i++] = parts[1];
                myargv[i++] = "--create-new";
                myargv[i++] = cwd;
                myargv[i++] = NULL;

                drop_ok = xfce_spawn(mdata->fmanager->gscreen, NULL, myargv,
                                     NULL, G_SPAWN_SEARCH_PATH, TRUE,
                                     gtk_get_current_event_time(),
                                     NULL, TRUE, NULL);

                g_free(cwd);
            }

            g_strfreev(parts);
        }

        g_free(exo_desktop_item_edit);
    } else if(info == TARGET_APPLICATION_OCTET_STREAM) {
        gchar *filename;
        gint length;
        guchar *prop_val = NULL;
        if(gdk_property_get(gdk_drag_context_get_source_window(context),
                            gdk_atom_intern("XdndDirectSave0", FALSE),
                            gdk_atom_intern("text/plain", FALSE), 0, 1024,
                            FALSE, NULL, NULL, &length,
                            &prop_val)
           && length > 0)
        {
            filename = g_strndup((gchar *)prop_val, length);
        } else {
            filename = g_strdup(_("Untitled document"));
        }
        g_free(prop_val);

        const gchar *folder = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
        /* get unique filename in case of duplicate */
        gchar *next_new_filename = xfdesktop_file_utils_next_new_file_name(filename, folder);
        g_free(filename);
        filename = next_new_filename;

        gchar *filepath = g_strdup_printf("%s/%s", folder, filename);

        GFile *dest = g_file_new_for_path(filepath);
        GFileOutputStream *out = g_file_create(dest, G_FILE_CREATE_NONE, NULL, NULL);

        if(out) {
            const gchar *content = (const gchar *)gtk_selection_data_get_data(data);
            length = gtk_selection_data_get_length(data);

            if (g_output_stream_write_all(G_OUTPUT_STREAM(out), content, length, NULL, NULL, NULL)) {
                g_output_stream_close(G_OUTPUT_STREAM(out), NULL, NULL);
            }

            g_object_unref(out);
        }

        g_free(filename);
        g_free(filepath);
        g_object_unref(dest);
    } else if(info == TARGET_TEXT_URI_LIST) {
        XfdesktopFileIcon *file_icon = NULL;
        GFileInfo *tinfo = NULL;
        GFile *tfile = NULL;
        if(drop_icon) {
            file_icon = XFDESKTOP_FILE_ICON(drop_icon);
            tfile = xfdesktop_file_icon_peek_file(file_icon);
            tinfo = xfdesktop_file_icon_peek_file_info(file_icon);
        }

        copy_only = (action == GDK_ACTION_COPY);

        if(tfile && g_file_has_uri_scheme(tfile, "trash") && copy_only) {
            gtk_drag_finish(context, FALSE, FALSE, time_);
            return;
        }

        GList *file_list = xfdesktop_file_utils_file_list_from_string((const gchar *)gtk_selection_data_get_data(data));
        if(file_list) {
            GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(icon_view));

            if(tinfo && xfdesktop_file_utils_file_is_executable(tinfo)) {
                drop_ok = xfdesktop_file_utils_execute(mdata->fmanager->folder,
                                                       tfile, file_list,
                                                       mdata->fmanager->gscreen,
                                                       GTK_WINDOW(toplevel));
            } else if(tfile && g_file_has_uri_scheme(tfile, "trash")) {
                /* move files to the trash */
                xfdesktop_file_utils_trash_files(file_list,
                                                 mdata->fmanager->gscreen,
                                                 GTK_WINDOW(toplevel));
            } else {
                gboolean dest_is_volume = drop_icon && XFDESKTOP_IS_VOLUME_ICON(drop_icon);
                /* if it's a volume, but we don't have |tinfo|, this just isn't
                 * going to work */
                if(!tinfo && dest_is_volume) {
                    xfdesktop_file_utils_file_list_free(file_list);
                    gtk_drag_finish(context, FALSE, FALSE, time_);
                    return;
                }

                GFile *base_dest_file;
                if(tinfo && g_file_info_get_file_type(tinfo) == G_FILE_TYPE_DIRECTORY) {
                    base_dest_file = g_object_ref(tfile);
                } else {
                    base_dest_file = g_object_ref(mdata->fmanager->folder);
                }

                gint cur_row = row;
                gint cur_col = col;
                GList *dest_file_list = NULL;
                for (GList *l = file_list; l; l = l->next) {
                    gchar *dest_basename = g_file_get_basename(l->data);

                    if(dest_basename && *dest_basename != '\0') {
                        /* If we copy a file, we need to use the new absolute filename
                         * as the destination. If we move or link, we need to use the destination
                         * directory. */
                        if(copy_only) {
                            GFile *dest_file = g_file_get_child(base_dest_file, dest_basename);
                            dest_file_list = g_list_prepend(dest_file_list, dest_file);
                        } else {
                            dest_file_list = g_list_prepend(dest_file_list, base_dest_file);
                        }

                        if (drop_icon == NULL && row != -1 && col != -1) {
                            // We are copying/moving/linking new files onto the desktop.  In order to later place them
                            // correctly (when the GFileMonitor gets notified about them), we need to store a little
                            // bit of data about the new files based on the drop location.
                            GFile *file = g_file_get_child(base_dest_file, dest_basename);
                            xfdesktop_file_icon_model_add_pending_new_file(mdata->fmanager->model, file, cur_row, cur_col);
                            if (!xfdesktop_icon_view_get_next_free_grid_position(icon_view,
                                                                                 cur_row, cur_col,
                                                                                 &cur_row, &cur_col))
                            {
                                cur_row = -1;
                                cur_col = -1;
                            }
                        }
                    }

                    g_free(dest_basename);
                }

                g_object_unref(base_dest_file);

                if(dest_file_list) {
                    dest_file_list = g_list_reverse(dest_file_list);
                    xfdesktop_file_utils_transfer_files(action, file_list, dest_file_list, mdata->fmanager->gscreen);
                    drop_ok = TRUE;
                }

                if(copy_only) {
                    xfdesktop_file_utils_file_list_free(dest_file_list);
                } else {
                    g_list_free(dest_file_list);
                }
            }
        }
    }

    XF_DEBUG("finishing drop on desktop from external source: drop_ok=%s, copy_only=%s",
             drop_ok?"TRUE":"FALSE", copy_only?"TRUE":"FALSE");

    gtk_drag_finish(context, drop_ok, !copy_only, time_);
}

static void
xfdesktop_file_icon_manager_drag_data_get(GtkWidget *icon_view,
                                          GdkDragContext *context,
                                          GtkSelectionData *data,
                                          guint info,
                                          guint time_,
                                          MonitorData *mdata)
{
    if (info == TARGET_TEXT_URI_LIST) {
        GList *selected_items = xfdesktop_icon_view_get_selected_items(XFDESKTOP_ICON_VIEW(icon_view));

        if (selected_items != NULL) {
            gchar **uris = g_new0(gchar *, g_list_length(selected_items) + 1);
            gint i = 0;

            for (GList *l = selected_items; l != NULL; l = l->next) {
                GtkTreePath *path = (GtkTreePath *)l->data;
                GtkTreeIter iter;

                if (gtk_tree_model_get_iter(GTK_TREE_MODEL(mdata->filter), &iter, path)) {
                    XfdesktopFileIcon *icon = xfdesktop_file_icon_model_filter_get_icon(mdata->filter, &iter);

                    if (icon != NULL) {
                        GFile *file = xfdesktop_file_icon_peek_file(icon);

                        if (file != NULL) {
                            uris[i] = g_file_get_uri(file);
                            ++i;
                        }
                    }
                }
            }
            g_list_free(selected_items);

            if (i > 0) {
                gtk_selection_data_set_uris(data, uris);
            }

            g_strfreev(uris);
        }
    }
}

static GdkDragAction
xfdesktop_file_icon_manager_drop_propose_action(XfdesktopIconView *icon_view,
                                                GdkDragContext *context,
                                                GtkTreeIter *iter,
                                                GdkDragAction action,
                                                GtkSelectionData *data,
                                                guint info,
                                                MonitorData *mdata)
{
    if (info == TARGET_TEXT_URI_LIST && action == GDK_ACTION_COPY
        && (gdk_drag_context_get_actions(context) & GDK_ACTION_MOVE) != 0)
    {

        XfdesktopFileIcon *drop_icon = iter != NULL
            ? xfdesktop_file_icon_model_filter_get_icon(mdata->filter, iter)
            : NULL;
        GFileInfo *tinfo = NULL;
        GFile *tfile = NULL;
        if(drop_icon) {
            XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(drop_icon);
            tinfo = xfdesktop_file_icon_peek_file_info(file_icon);

            /* if it's a volume, but we don't have |tinfo|, this just isn't
             * going to work */
            if(!tinfo && XFDESKTOP_IS_VOLUME_ICON(drop_icon)) {
                return 0;
            }

            tfile = xfdesktop_file_icon_peek_file(file_icon);
            if(tfile && !g_file_has_uri_scheme(tfile, "file")) {
                return action;
            }

            if(tinfo && g_file_info_get_file_type(tinfo) != G_FILE_TYPE_DIRECTORY) {
                return action;
            }
        }

        GList *file_list = xfdesktop_file_utils_file_list_from_string((const gchar *)gtk_selection_data_get_data(data));
        if(file_list) {
            GFile *base_dest_file = NULL;

            /* always move files from the trash */
            if(g_file_has_uri_scheme(file_list->data, "trash")) {
                xfdesktop_file_utils_file_list_free(file_list);
                return GDK_ACTION_MOVE;
            }

            /* source must be local file */
            if(!g_file_has_uri_scheme(file_list->data, "file")) {
                xfdesktop_file_utils_file_list_free(file_list);
                return action;
            }

            if(tinfo) {
                base_dest_file = g_object_ref(tfile);
            } else {
                base_dest_file = g_object_ref(mdata->fmanager->folder);
            }

            /* dropping on ourselves? */
            if(g_strcmp0(g_file_get_uri(file_list->data), g_file_get_uri(base_dest_file)) == 0) {
                g_object_unref(base_dest_file);
                xfdesktop_file_utils_file_list_free(file_list);
                return 0;
            }

            /* Determine if we should move/copy by checking if the files
             * are on the same filesystem and are writable by the user.
             */

            GFileInfo *dest_info = g_file_query_info(base_dest_file,
                                                     XFDESKTOP_FILE_INFO_NAMESPACE,
                                                     G_FILE_QUERY_INFO_NONE,
                                                     NULL,
                                                     NULL);
            GFileInfo *src_info = g_file_query_info(file_list->data,
                                                    XFDESKTOP_FILE_INFO_NAMESPACE,
                                                    G_FILE_QUERY_INFO_NONE,
                                                    NULL,
                                                    NULL);

            if(dest_info != NULL && src_info != NULL) {
                const gchar *dest_name = g_file_info_get_attribute_string(dest_info,
                                                                          G_FILE_ATTRIBUTE_ID_FILESYSTEM);
                const gchar *src_name = g_file_info_get_attribute_string(src_info,
                                                                         G_FILE_ATTRIBUTE_ID_FILESYSTEM);

                if((g_strcmp0(src_name, dest_name) == 0)
                   && g_file_info_get_attribute_boolean(src_info,
                                    G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
                {
                    action = GDK_ACTION_MOVE;
                }
            }

            if(dest_info != NULL)
                g_object_unref(dest_info);
            if(src_info != NULL)
                g_object_unref(src_info);

            g_object_unref(base_dest_file);
            xfdesktop_file_utils_file_list_free(file_list);

        }
    }

    return action;
}

static void
model_ready(XfdesktopFileIconModel *fmodel, XfdesktopFileIconManager *fmanager) {
    DBG("entering");
    fmanager->ready = TRUE;

    GHashTableIter iter;
    g_hash_table_iter_init(&iter, fmanager->monitor_data);

    MonitorData *mdata;
    while (g_hash_table_iter_next(&iter, NULL, (gpointer)&mdata)) {
        XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
        xfdesktop_icon_view_set_model(icon_view, GTK_TREE_MODEL(mdata->filter));
    }
}

static void
model_error(XfdesktopFileIconModel *fmodel, GError *error, XfdesktopFileIconManager *fmanager) {
    const gchar *icon_name = NULL;
    gchar *primary = NULL;

    if (error->domain == XFDESKTOP_FILE_ICON_MODEL_ERROR) {
        switch (error->code) {
            case XFDESKTOP_FILE_ICON_MODEL_ERROR_CANT_CREATE_DESKTOP_FOLDER:
            case XFDESKTOP_FILE_ICON_MODEL_ERROR_DESKTOP_NOT_FOLDER: {
                icon_name = "dialog-warning";

                gchar *uri = g_file_get_uri(fmanager->folder);
                gchar *display_name = g_filename_display_basename(uri);
                primary = g_markup_printf_escaped(_("Could not create the desktop folder \"%s\""), display_name);
                g_free(uri);
                g_free(display_name);
                break;
            }

            case XFDESKTOP_FILE_ICON_MODEL_ERROR_FOLDER_LIST_FAILED:
                icon_name = "dialog-warning";
                primary = g_strdup(_("Failed to load the desktop folder"));
                break;

            default:
                g_assert_not_reached();
                break;
        }
    } else {
        icon_name = "dialog-error";
        primary = g_strdup(_("A desktop file icon error occurred"));
    }

    xfce_message_dialog(NULL,
                        _("Desktop Folder Error"),
                        icon_name,
                        primary,
                        error->message,
                        XFCE_BUTTON_TYPE_MIXED,
                        "window-close",
                        _("_Close"),
                        GTK_RESPONSE_ACCEPT,
                        NULL);

    g_free(primary);
}

static void
xfdesktop_file_icon_manager_icon_moved(XfdesktopIconView *icon_view,
                                       GtkTreeIter *iter,
                                       gint new_row,
                                       gint new_col,
                                       MonitorData *mdata)
{
    XfdesktopFileIcon *icon = xfdesktop_file_icon_model_filter_get_icon(mdata->filter, iter);

    if (G_LIKELY(icon != NULL)) {
        gint16 old_row, old_col;

        if (!xfdesktop_icon_get_position(XFDESKTOP_ICON(icon), &old_row, &old_col)
            || old_row != new_row
            || old_col != new_col)
        {
            update_icon_position(mdata, icon, new_row, new_col);
        }
    }
}

static void
xfdesktop_file_icon_manager_activate_selected(GtkWidget *widget, XfdesktopFileIconManager *fmanager) {
    GList *selected_icons = xfdesktop_file_icon_manager_get_selected_icons(fmanager);

    for (GList *l = selected_icons; l != NULL; l = l->next) {
        XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(l->data);
        xfdesktop_icon_activate(XFDESKTOP_ICON(icon), toplevel_window_for_widget(widget));
    }

    g_list_free(selected_icons);
}


/* public api */


XfdesktopIconViewManager *
xfdesktop_file_icon_manager_new(XfwScreen *screen,
                                GdkScreen *gdkscreen,
                                XfconfChannel *channel,
                                XfdesktopBackdropManager *backdrop_manager,
                                GList *desktops,
                                GFile *folder)
{
    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel), NULL);
    g_return_val_if_fail(G_IS_FILE(folder), NULL);

    return g_object_new(XFDESKTOP_TYPE_FILE_ICON_MANAGER,
                        "screen", screen,
                        "gdk-screen", gdkscreen,
                        "channel", channel,
                        "backdrop-manager", backdrop_manager,
                        "desktops", desktops,
                        "folder", folder,
                        NULL);
}
