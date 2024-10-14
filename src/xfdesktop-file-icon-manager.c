/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006,2024 Brian Tarricone, <brian@tarricone.org>
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
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#ifdef HAVE_THUNARX
#include <thunarx/thunarx.h>
#endif

#include "common/xfdesktop-keyboard-shortcuts.h"
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
#include "xfdesktop-icon-view-manager.h"
#include "xfdesktop-icon-view.h"
#include "xfdesktop-icon-view-model.h"
#include "xfdesktop-icon.h"
#include "xfdesktop-regular-file-icon.h"
#include "xfdesktop-special-file-icon.h"
#include "xfdesktop-volume-icon.h"

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4windowing/libxfce4windowing.h>
#include <xfconf/xfconf.h>

#define SAVE_DELAY  1000
#define BORDER         8

#define VOLUME_HASH_STR_PREFIX  "xfdesktop-volume-"

#define LAUNCHER_TYPE_KEY "xfdesktop-file-icon-manager-launcher-type"
#define FILE_KEY "xfdesktop-file-icon-manager-file"
#define DEST_ROW_KEY "xfdesktop-file-icon-manager-dest-row"
#define DEST_COL_KEY "xfdesktop-file-icon-manager-dest-col"

#define TEXT_URI_LIST_NAME "text/uri-list"
#define XDND_DIRECT_SAVE0_NAME "XdndDirectSave0"
#define APPLICATION_OCTET_STREAM_NAME "application/octet-stream"
#define NETSCAPE_URL_NAME "_NETSCAPE_URL"

struct _XfdesktopFileIconManager
{
    XfdesktopIconViewManager parent;

    gboolean ready;

    XfdesktopFileIconModel *model;
    GHashTable *monitor_data;  // XfwMonitor (owner) -> MonitorData (owner)
    XfdesktopIconPositionConfigs *position_configs;

    GdkScreen *gscreen;

    GFile *folder;
    XfdesktopFileIcon *desktop_icon;

    GtkTargetList *drag_targets;
    GtkTargetList *drop_targets;

    gboolean drag_dropped;
    gboolean drag_data_received;
    XfdesktopFileIcon *icon_on_drop_dest;
    GList *dragged_files;
    GFile *xdnd_direct_save_destination;

    GList *pending_created_desktop_files;

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

static MonitorData *
monitor_data_for_icon_view(GHashTable *monitor_datas, XfdesktopIconView *icon_view) {
    GHashTableIter hiter;
    g_hash_table_iter_init(&hiter, monitor_datas);

    MonitorData *mdata;
    while (g_hash_table_iter_next(&hiter, NULL, (gpointer)&mdata)) {
        if (xfdesktop_icon_view_holder_get_icon_view(mdata->holder) == icon_view) {
            return mdata;
        }
    }

    return NULL;
}

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

typedef struct {
    GCancellable *cancellable;

    MonitorData *mdata;
    gint dest_row;
    gint dest_col;
} CreateDesktopFileWithPositionData;

static void
create_desktop_file_with_position_data_free(CreateDesktopFileWithPositionData *cdfwpdata) {
    if (cdfwpdata->mdata != NULL) {
        cdfwpdata->mdata->fmanager->pending_created_desktop_files =
            g_list_remove(cdfwpdata->mdata->fmanager->pending_created_desktop_files, cdfwpdata);
    }
    g_object_unref(cdfwpdata->cancellable);
    g_free(cdfwpdata);
}

typedef struct {
    GdkDragContext *context;
    GFile *new_file;
    guchar *file_data;
    gsize file_data_len;
} XdsDropData;

static void
xds_drop_data_free(XdsDropData *xddata) {
    g_object_unref(xddata->context);
    g_object_unref(xddata->new_file);
    g_free(xddata->file_data);
    g_free(xddata);
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

static void xfdesktop_file_icon_manager_drag_data_get(GtkWidget *icon_view,
                                                      GdkDragContext *context,
                                                      GtkSelectionData *data,
                                                      guint info,
                                                      guint time_,
                                                      MonitorData *mdata);

static gboolean xfdesktop_file_icon_manager_drag_motion(GtkWidget *icon_view,
                                                        GdkDragContext *context,
                                                        gint x,
                                                        gint y,
                                                        guint time_,
                                                        MonitorData *mdata);
static void xfdesktop_file_icon_manager_drag_data_received(GtkWidget *icon_view,
                                                           GdkDragContext *context,
                                                           gint x,
                                                           gint y,
                                                           GtkSelectionData *data,
                                                           guint info,
                                                           guint time_,
                                                           MonitorData *mdata);
static gboolean xfdesktop_file_icon_manager_drag_drop(GtkWidget *icon_view,
                                                      GdkDragContext *context,
                                                      gint x,
                                                      gint y,
                                                      guint time_,
                                                      MonitorData *mdata);
static void xfdesktop_file_icon_manager_drag_leave(GtkWidget *icon_view,
                                                   GdkDragContext *context,
                                                   guint time_,
                                                   MonitorData *mdata);

static GdkDragAction xfdesktop_file_icon_manager_drag_drop_ask(XfdesktopFileIconManager *fmanager,
                                                               GtkWidget *parent,
                                                               GdkDragAction allowed_actions,
                                                               guint time_);

static void xfdesktop_file_icon_manager_desktop_added(XfdesktopIconViewManager *manager,
                                                      XfceDesktop *desktop);
static void xfdesktop_file_icon_manager_desktop_removed(XfdesktopIconViewManager *manager,
                                                        XfceDesktop *desktop);
static XfceDesktop *xfdesktop_file_icon_manager_get_focused_desktop(XfdesktopIconViewManager *manager);
static GtkMenu *xfdesktop_file_icon_manager_get_context_menu(XfdesktopIconViewManager *manager,
                                                             XfceDesktop *desktop,
                                                             gint popup_x,
                                                             gint popup_y);
static void xfdesktop_file_icon_manager_activate_icons(XfdesktopIconViewManager *manager);
static void xfdesktop_file_icon_manager_toggle_cursor_icon(XfdesktopIconViewManager *manager);
static void xfdesktop_file_icon_manager_select_all_icons(XfdesktopIconViewManager *manager);
static void xfdesktop_file_icon_manager_unselect_all_icons(XfdesktopIconViewManager *manager);
static void xfdesktop_file_icon_manager_sort_icons(XfdesktopIconViewManager *manager,
                                                   GtkSortType sort_type,
                                                   XfdesktopIconViewManagerSortFlags flags);
static void xfdesktop_file_icon_manager_reload(XfdesktopIconViewManager *manager);

static void xfdesktop_file_icon_manager_icon_moved(XfdesktopIconView *icon_view,
                                                   XfdesktopIconView *source_icon_view,
                                                   GtkTreeIter *source_iter,
                                                   gint dest_row,
                                                   gint dest_col,
                                                   MonitorData *mdata);
static void xfdesktop_file_icon_manager_activate_selected(MonitorData *mdata);

static GList *xfdesktop_file_icon_manager_get_selected_icons(XfdesktopFileIconManager *fmanager,
                                                             XfdesktopIconView *icon_view);

static void xfdesktop_file_icon_manager_clipboard_changed(XfdesktopClipboardManager *cmanager,
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

static gboolean update_icon_position(MonitorData *mdata,
                                     XfdesktopFileIcon *icon,
                                     gint row,
                                     gint col);

static MonitorData *find_active_monitor_data(XfdesktopFileIconManager *fmanager);

static void file_icon_manager_action_fixup(XfceGtkActionEntry *entry);
static void accel_map_changed(XfdesktopFileIconManager *fmanager);


G_DEFINE_TYPE(XfdesktopFileIconManager, xfdesktop_file_icon_manager, XFDESKTOP_TYPE_ICON_VIEW_MANAGER)


enum
{
    TARGET_TEXT_URI_LIST = 1000,
    TARGET_XDND_DIRECT_SAVE0 = 1001,
    TARGET_APPLICATION_OCTET_STREAM = 1002,
    TARGET_NETSCAPE_URL = 1003,
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
    { TEXT_URI_LIST_NAME, 0, TARGET_TEXT_URI_LIST, },
};
static const GtkTargetEntry drop_targets[] = {
    { TEXT_URI_LIST_NAME, 0, TARGET_TEXT_URI_LIST, },
    { XDND_DIRECT_SAVE0_NAME, 0, TARGET_XDND_DIRECT_SAVE0 },
    { APPLICATION_OCTET_STREAM_NAME, 0, TARGET_APPLICATION_OCTET_STREAM },
    { NETSCAPE_URL_NAME, 0, TARGET_NETSCAPE_URL, },
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
    ivm_class->get_focused_desktop = xfdesktop_file_icon_manager_get_focused_desktop;
    ivm_class->get_context_menu = xfdesktop_file_icon_manager_get_context_menu;
    ivm_class->activate_icons = xfdesktop_file_icon_manager_activate_icons;
    ivm_class->toggle_cursor_icon = xfdesktop_file_icon_manager_toggle_cursor_icon;
    ivm_class->select_all_icons = xfdesktop_file_icon_manager_select_all_icons;
    ivm_class->unselect_all_icons = xfdesktop_file_icon_manager_unselect_all_icons;
    ivm_class->sort_icons = xfdesktop_file_icon_manager_sort_icons;
    ivm_class->reload = xfdesktop_file_icon_manager_reload;

    g_object_class_install_property(gobject_class,
                                    PROP_GDK_SCREEN,
                                    g_param_spec_object("gdk-screen",
                                                        "gdk-screen",
                                                        "GdkScreen",
                                                        GDK_TYPE_SCREEN,
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

    accel_map_changed(fmanager);
    g_signal_connect_data(gtk_accel_map_get(),
                          "changed",
                          G_CALLBACK(accel_map_changed),
                          fmanager,
                          NULL,
                          G_CONNECT_SWAPPED | G_CONNECT_AFTER);

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
            if (g_file_test(g_file_peek_path(positions_file), G_FILE_TEST_EXISTS)) {
                GDateTime *now = g_date_time_new_now_local();
                gchar *stamp = g_date_time_format_iso8601(now);
                gchar *backup_file_relpath = g_strdup_printf("xfce4/desktop/icons.screen%d.invalid-%s.yaml",
                                                             screen_num,
                                                             stamp);
                gchar *backup_file_path = xfce_resource_save_location(XFCE_RESOURCE_CONFIG, backup_file_relpath, TRUE);
                GFile *backup_file = g_file_new_for_path(backup_file_path);

                g_message("Backing up invalid icon positions configuration file to %s", backup_file_relpath);
                g_file_move(positions_file, backup_file, G_FILE_COPY_NONE, NULL, NULL, NULL, NULL);

                g_date_time_unref(now);
                g_free(stamp);
                g_free(backup_file_relpath);
                g_free(backup_file_path);
                g_object_unref(backup_file);
            }
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

    g_signal_handlers_disconnect_by_func(gtk_accel_map_get(), accel_map_changed, fmanager);

    gsize n_actions;
    XfceGtkActionEntry *actions = xfdesktop_get_file_icon_manager_actions(file_icon_manager_action_fixup, &n_actions);
    GtkAccelGroup *accel_group = xfdesktop_icon_view_manager_get_accel_group(XFDESKTOP_ICON_VIEW_MANAGER(fmanager));
    xfce_gtk_accel_group_disconnect_action_entries(accel_group, actions, n_actions);

    for (GList *l = fmanager->pending_created_desktop_files; l != NULL; l = l->next) {
        CreateDesktopFileWithPositionData *cdfwpdata = l->data;
        cdfwpdata->mdata = NULL;
        g_cancellable_cancel(cdfwpdata->cancellable);
    }
    g_clear_pointer(&fmanager->pending_created_desktop_files, g_list_free);

    g_clear_pointer(&fmanager->monitor_data, g_hash_table_destroy);
    g_clear_object(&fmanager->model);

    G_OBJECT_CLASS(xfdesktop_file_icon_manager_parent_class)->dispose(obj);
}

static void
xfdesktop_file_icon_manager_finalize(GObject *obj)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(obj);

    xfdesktop_icon_position_configs_free(fmanager->position_configs);

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

static GtkWindow *
toplevel_window_for_monitor_data(MonitorData *mdata) {
    XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(icon_view));
    return GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : NULL;
}

/* icon signal handlers */

static void
xfdesktop_file_icon_manager_open_icon(MonitorData *mdata) {
    XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
    GList *selected = xfdesktop_file_icon_manager_get_selected_icons(mdata->fmanager, icon_view);
    if (selected != NULL && selected->next == NULL) {
        XfdesktopIcon *icon = XFDESKTOP_ICON(selected->data);
        xfdesktop_icon_activate(icon, toplevel_window_for_monitor_data(mdata));
    }

    g_list_free(selected);
}

static void
xfdesktop_file_icon_menu_executed(GtkWidget *widget, MonitorData *mdata) {
    xfdesktop_file_icon_manager_open_icon(mdata);
}

static void
xfdesktop_file_icon_menu_open_all(GtkWidget *widget, MonitorData *mdata) {
    xfdesktop_file_icon_manager_activate_selected(mdata);
}

static void
xfdesktop_file_icon_manager_rename(MonitorData *mdata) {
    XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
    GList *selected = xfdesktop_file_icon_manager_get_selected_icons(mdata->fmanager, icon_view);
    GList *files = NULL;
    for (GList *iter = selected; iter != NULL; iter = iter->next) {
        XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(iter->data);

        /* create a list of GFiles from selected icons that can be renamed */
        if(xfdesktop_file_icon_can_rename_file(icon))
            files = g_list_append(files, xfdesktop_file_icon_peek_file(icon));
    }

    GtkWindow *toplevel = toplevel_window_for_monitor_data(mdata);
    if(g_list_length(files) == 1) {
        /* rename dialog for a single icon */
        xfdesktop_file_utils_rename_file(g_list_first(files)->data, mdata->fmanager->gscreen, toplevel);
    } else if(g_list_length(files) > 1) {
        /* Bulk rename for multiple icons selected */
        GFile *desktop = xfdesktop_file_icon_peek_file(mdata->fmanager->desktop_icon);

        xfdesktop_file_utils_bulk_rename(desktop, files, mdata->fmanager->gscreen, toplevel);
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
xfdesktop_file_icon_menu_rename(GtkWidget *widget, MonitorData *mdata) {
    xfdesktop_file_icon_manager_rename(mdata);
}

static void
xfdesktop_file_icon_manager_delete_selected(MonitorData *mdata, gboolean force_delete) {
    XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
    GList *selected = xfdesktop_file_icon_manager_get_selected_icons(mdata->fmanager, icon_view);

    GList *deletable_files = NULL;
    for (GList *l = selected; l != NULL; l = l->next) {
        XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(l->data);
        if (xfdesktop_file_icon_can_delete_file(icon) && xfdesktop_file_icon_peek_file(icon) != NULL) {
            GFile *file = xfdesktop_file_icon_peek_file(icon);
            deletable_files = g_list_prepend(deletable_files, g_object_ref(file));
        }
    }
    g_list_free(selected);

    if (deletable_files != NULL) {
        GtkWindow *toplevel = toplevel_window_for_monitor_data(mdata);
        if (!force_delete) {
            xfdesktop_file_utils_trash_files(deletable_files, mdata->fmanager->gscreen, toplevel);
        } else {
            xfdesktop_file_utils_unlink_files(deletable_files, mdata->fmanager->gscreen, toplevel);
        }

        g_list_free_full(deletable_files, g_object_unref);
    }
}

static void
xfdesktop_file_icon_menu_app_info_executed(GtkWidget *widget, MonitorData *mdata) {
    XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
    GList *selected = xfdesktop_file_icon_manager_get_selected_icons(mdata->fmanager, icon_view);
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
        if (!xfdesktop_file_utils_app_info_launch(app_info, mdata->fmanager->folder, selected,
                                                  G_APP_LAUNCH_CONTEXT(context), &error))
        {
            GtkWindow *toplevel = toplevel_window_for_monitor_data(mdata);
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
xfdesktop_file_icon_menu_open_folder(GtkWidget *widget, MonitorData *mdata) {
    XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
    GList *selected = xfdesktop_file_icon_manager_get_selected_icons(mdata->fmanager, icon_view);
    for (GList *l = selected; l != NULL; l = l->next) {
        XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(l->data);
        l->data = xfdesktop_file_icon_peek_file(icon);
    }

    if (selected != NULL) {
        xfdesktop_file_utils_open_folders(selected, mdata->fmanager->gscreen, toplevel_window_for_monitor_data(mdata));
        g_list_free(selected);
    }
}

static void
xfdesktop_file_icon_menu_open_desktop(GtkWidget *widget, MonitorData *mdata) {
    GList link = {
        .data = xfdesktop_file_icon_peek_file(mdata->fmanager->desktop_icon),
        .prev = NULL,
        .next = NULL,
    };

    if (link.data != NULL) {
        xfdesktop_file_utils_open_folders(&link, mdata->fmanager->gscreen, toplevel_window_for_monitor_data(mdata));
    }
}

static void
xfdesktop_file_icon_menu_other_app(GtkWidget *widget, MonitorData *mdata) {
    XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
    GList *selected = xfdesktop_file_icon_manager_get_selected_icons(mdata->fmanager, icon_view);
    if (selected != NULL && selected->next == NULL) {
        XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(selected->data);
        GFile *file = xfdesktop_file_icon_peek_file(icon);
        GtkWindow *toplevel = toplevel_window_for_monitor_data(mdata);
        xfdesktop_file_utils_display_app_chooser_dialog(file, TRUE, FALSE, mdata->fmanager->gscreen, toplevel);
    }

    g_list_free(selected);
}

static void
xfdesktop_file_icon_menu_set_default_app(GtkWidget *widget, MonitorData *mdata) {
    XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
    GList *selected = xfdesktop_file_icon_manager_get_selected_icons(mdata->fmanager, icon_view);
    if (selected != NULL && selected->next == NULL) {
        XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(selected->data);
        GFile *file = xfdesktop_file_icon_peek_file(icon);
        GtkWindow *toplevel = toplevel_window_for_monitor_data(mdata);
        xfdesktop_file_utils_display_app_chooser_dialog(file, TRUE, TRUE, mdata->fmanager->gscreen, toplevel);
    }

    g_list_free(selected);
}

static void
xfdesktop_file_icon_manager_cut_selected_files(MonitorData *mdata) {
    XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
    GList *selected_icons = xfdesktop_file_icon_manager_get_selected_icons(mdata->fmanager, icon_view);
    if (selected_icons != NULL) {
        xfdesktop_clipboard_manager_cut_files(clipboard_manager, selected_icons);
        g_list_free(selected_icons);
    }
}

static void
xfdesktop_file_icon_menu_cut(GtkWidget *widget, MonitorData *mdata) {
    xfdesktop_file_icon_manager_cut_selected_files(mdata);
}

static void
xfdesktop_file_icon_manager_copy_selected_files(MonitorData *mdata) {
    XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
    GList *selected = xfdesktop_file_icon_manager_get_selected_icons(mdata->fmanager, icon_view);
    if (selected != NULL) {
        xfdesktop_clipboard_manager_copy_files(clipboard_manager, selected);
        g_list_free(selected);
    }
}

static void
xfdesktop_file_icon_menu_copy(GtkWidget *widget, MonitorData *mdata) {
    xfdesktop_file_icon_manager_copy_selected_files(mdata);
}

static void
xfdesktop_file_icon_menu_trash(GtkWidget *widget, MonitorData *mdata) {
    xfdesktop_file_icon_manager_delete_selected(mdata, FALSE);
}

static void
xfdesktop_file_icon_menu_delete(GtkWidget *widget, MonitorData *mdata) {
    xfdesktop_file_icon_manager_delete_selected(mdata, TRUE);
}

static void
xfdesktop_file_icon_menu_paste(GtkWidget *widget, MonitorData *mdata) {
    xfdesktop_clipboard_manager_paste_files(clipboard_manager, mdata->fmanager->folder, widget, NULL);
}

static void
xfdesktop_file_icon_manager_paste_into_folder(MonitorData *mdata) {
    XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
    GList *selected = xfdesktop_file_icon_manager_get_selected_icons(mdata->fmanager, icon_view);
    if (selected != NULL && selected->next == NULL) {
        XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(selected->data);
        GFileInfo *info = xfdesktop_file_icon_peek_file_info(icon);
        if (info != NULL && g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
            GFile *file = xfdesktop_file_icon_peek_file(icon);
            xfdesktop_clipboard_manager_paste_files(clipboard_manager, file, GTK_WIDGET(icon_view), NULL);
        }
    }

    g_list_free(selected);
}

static void
xfdesktop_file_icon_menu_paste_into_folder(GtkWidget *widget, MonitorData *mdata) {
    xfdesktop_file_icon_manager_paste_into_folder(mdata);
}

static void
xfdesktop_file_icon_menu_arrange_icons(GtkWidget *widget, MonitorData *mdata) {
    xfdesktop_icon_view_manager_sort_icons(XFDESKTOP_ICON_VIEW_MANAGER(mdata->fmanager),
                                           GTK_SORT_ASCENDING,
                                           XFDESKTOP_ICON_VIEW_MANAGER_SORT_NONE);
}

static void
xfdesktop_file_icon_menu_next_background(GtkWidget *widget, MonitorData *mdata) {
    // FIXME: need to handle spanning in a special way?
    XfceDesktop *desktop = xfdesktop_icon_view_holder_get_desktop(mdata->holder);
    xfce_desktop_cycle_backdrop(desktop);
}

static void
xfdesktop_file_icon_manager_show_selected_properties(MonitorData *mdata) {
    XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
    GList *selected = xfdesktop_file_icon_manager_get_selected_icons(mdata->fmanager, icon_view);
    for (GList *l = selected; l != NULL; l = l->next) {
        XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(l->data);
        l->data = xfdesktop_file_icon_peek_file(icon);
    }

    if (selected != NULL) {
        GtkWindow *toplevel = toplevel_window_for_monitor_data(mdata);
        xfdesktop_file_utils_show_properties_dialog(selected, mdata->fmanager->gscreen, toplevel);
        g_list_free(selected);
    }
}

static void
xfdesktop_file_icon_menu_properties(GtkWidget *widget, MonitorData *mdata) {
    xfdesktop_file_icon_manager_show_selected_properties(mdata);
}

static void
xfdesktop_file_icon_manager_desktop_properties(GtkWidget *widget, MonitorData *mdata) {
    GFile *file = xfdesktop_file_icon_peek_file(mdata->fmanager->desktop_icon);
    GList file_l = {
        .data = file,
        .next = NULL,
    };

    xfdesktop_file_utils_show_properties_dialog(&file_l,
                                                mdata->fmanager->gscreen,
                                                toplevel_window_for_monitor_data(mdata));
}

static GtkWidget *
xfdesktop_menu_item_from_app_info(MonitorData *mdata,
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
                     mdata);

    return mi;
}

static gboolean
xfdesktop_file_icon_menu_free_icon_list_idled(gpointer user_data) {
    GList *icon_list = user_data;
    g_list_free_full(icon_list, g_object_unref);
    return FALSE;
}

static void
xfdesktop_file_icon_menu_free_icon_list(GtkMenu *menu, GList *icon_list) {
    g_idle_add(xfdesktop_file_icon_menu_free_icon_list_idled, icon_list);
}

static void
desktop_file_created(GFile *desktop_file, GError *error, gpointer data) {
    CreateDesktopFileWithPositionData *cdfwpdata = data;

    if (error != NULL) {
        if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)
            && !g_error_matches(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
        {
            xfce_message_dialog(toplevel_window_for_monitor_data(cdfwpdata->mdata),
                                _("Launch Error"),
                                "dialog-error",
                                _("Unable to launch \"exo-desktop-item-edit\", which is required to create and edit launchers and links on the desktop."),
                                error->message,
                                XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                                NULL);
        }
    } else if (desktop_file != NULL) {
        XfdesktopFileIcon *icon = xfdesktop_file_icon_model_get_icon_for_file(cdfwpdata->mdata->fmanager->model,
                                                                              desktop_file);
        if (icon != NULL) {
            update_icon_position(cdfwpdata->mdata, icon, cdfwpdata->dest_row, cdfwpdata->dest_col);
        } else {
            xfdesktop_icon_position_configs_set_icon_position(cdfwpdata->mdata->fmanager->position_configs,
                                                              cdfwpdata->mdata->position_config,
                                                              g_file_peek_path(desktop_file),
                                                              cdfwpdata->dest_row,
                                                              cdfwpdata->dest_col,
                                                              0);
        }
    }

    create_desktop_file_with_position_data_free(cdfwpdata);
}

static void
xfdesktop_file_icon_menu_edit_launcher(GtkWidget *widget, MonitorData *mdata) {
    GFile *file = g_object_get_data(G_OBJECT(widget), FILE_KEY);
    g_return_if_fail(G_IS_FILE(file));

    GStrvBuilder *argv_builder = g_strv_builder_new();
    g_strv_builder_add(argv_builder, "exo-desktop-item-edit");

    if (xfw_windowing_get() == XFW_WINDOWING_X11) {
        g_strv_builder_add(argv_builder, "--display");
        g_strv_builder_add(argv_builder, gdk_display_get_name(gdk_screen_get_display(mdata->fmanager->gscreen)));
    }

    gchar *uri = g_file_get_uri(file);
    g_strv_builder_add(argv_builder, uri);
    g_free(uri);

    gchar **argv = g_strv_builder_end(argv_builder);
    g_strv_builder_unref(argv_builder);

    GError *error = NULL;
    if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error)) {
        xfce_message_dialog(toplevel_window_for_monitor_data(mdata),
                            _("Launch Error"),
                            "dialog-error",
                            _("Unable to launch \"exo-desktop-item-edit\", which is required to create and edit launchers and links on the desktop."),
                            error->message,
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
        g_error_free(error);
    }

    g_strfreev(argv);
}

static void
xfdesktop_file_icon_menu_create_launcher(GtkWidget *widget, MonitorData *mdata) {
    const gchar *type = g_object_get_data(G_OBJECT(widget), LAUNCHER_TYPE_KEY);
    g_return_if_fail(type != NULL);

    CreateDesktopFileWithPositionData *cdfwpdata = g_new0(CreateDesktopFileWithPositionData, 1);
    cdfwpdata->cancellable = g_cancellable_new();
    cdfwpdata->mdata = mdata;
    cdfwpdata->dest_row = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), DEST_ROW_KEY));
    cdfwpdata->dest_col = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), DEST_COL_KEY));
    mdata->fmanager->pending_created_desktop_files = g_list_prepend(mdata->fmanager->pending_created_desktop_files,
                                                                    cdfwpdata);

    xfdesktop_file_utils_create_desktop_file(gtk_widget_get_screen(widget),
                                             mdata->fmanager->folder,
                                             type,
                                             NULL,
                                             NULL,
                                             cdfwpdata->cancellable,
                                             desktop_file_created,
                                             cdfwpdata);
}

static void
xfdesktop_file_icon_manager_create_folder(MonitorData *mdata, gint dest_row, gint dest_col, GtkWindow *parent) {
    GFile *new_folder = xfdesktop_file_utils_prompt_for_new_folder_name(mdata->fmanager->folder, parent);
    if (new_folder != NULL) {
        if (dest_row != -1 && dest_col != -1) {
            xfdesktop_icon_position_configs_set_icon_position(mdata->fmanager->position_configs,
                                                              mdata->position_config,
                                                              g_file_peek_path(new_folder),
                                                              dest_row,
                                                              dest_col,
                                                              0);
        }

        xfdesktop_file_utils_create_folder(new_folder, parent);
        g_object_unref(new_folder);
    }
}

static void
xfdesktop_file_icon_menu_create_folder(GtkWidget *widget, MonitorData *mdata) {
    GtkWindow *parent = toplevel_window_for_monitor_data(mdata);
    gint dest_row = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), DEST_ROW_KEY));
    gint dest_col = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), DEST_COL_KEY));
    xfdesktop_file_icon_manager_create_folder(mdata, dest_row, dest_col, parent);
}

static void
create_from_template(MonitorData *mdata, GFile *template_file, gint dest_row, gint dest_col) {
    GtkWindow *parent = toplevel_window_for_monitor_data(mdata);
    GFile *dest_file = xfdesktop_file_utils_prompt_for_template_file_name(mdata->fmanager->folder, template_file, parent);
    if (dest_file != NULL) {
        if (dest_row != -1 && dest_col != -1) {
            xfdesktop_icon_position_configs_set_icon_position(mdata->fmanager->position_configs,
                                                              mdata->position_config,
                                                              g_file_peek_path(dest_file),
                                                              dest_row,
                                                              dest_col,
                                                              0);
        }

        xfdesktop_file_utils_create_file_from_template(template_file, dest_file, parent);
        g_object_unref(dest_file);
    }
}

static void
xfdesktop_file_icon_template_item_activated(GtkWidget *mi, MonitorData *mdata) {
    GFile *file = g_object_get_data(G_OBJECT(mi), FILE_KEY);
    gint dest_row = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(mi), DEST_ROW_KEY));
    gint dest_col = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(mi), DEST_COL_KEY));
    create_from_template(mdata, file, dest_row, dest_col);
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
                                            MonitorData *mdata,
                                            gint dest_row,
                                            gint dest_col,
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
        mdata->fmanager->templates_count = 0;

    /* keep it under fmanager->max_templates otherwise the menu
     * could have tons of items and be unusable. Additionally this should
     * help in instances where the XDG_TEMPLATES_DIR has a large number of
     * files in it. */
    GList *files = NULL;
    GFileInfo *info;
    while((info = g_file_enumerator_next_file(enumerator, NULL, NULL))
          && mdata->fmanager->templates_count < mdata->fmanager->max_templates)
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
            mdata->fmanager->templates_count++;
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

            xfdesktop_file_icon_menu_fill_template_menu(submenu, file, mdata, dest_row, dest_col, TRUE);

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
            g_object_set_data_full(G_OBJECT(item), FILE_KEY, g_object_ref(file), g_object_unref);
            g_object_set_data(G_OBJECT(item), DEST_ROW_KEY, GINT_TO_POINTER(dest_row));
            g_object_set_data(G_OBJECT(item), DEST_COL_KEY, GINT_TO_POINTER(dest_col));

            g_signal_connect(G_OBJECT(item), "activate",
                             G_CALLBACK(xfdesktop_file_icon_template_item_activated),
                             mdata);
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
xfdesktop_settings_launch(GtkWidget *w, MonitorData *mdata) {
    gchar *cmd = g_find_program_in_path("xfdesktop-settings");
    if(!cmd)
        cmd = g_strdup(BINDIR "/xfdesktop-settings");

    GError *error = NULL;
    if (!xfce_spawn_command_line(mdata->fmanager->gscreen, cmd, FALSE, TRUE, TRUE, &error)) {
        /* printf is to be translator-friendly */
        gchar *primary = g_strdup_printf(_("Unable to launch \"%s\":"), cmd);
        xfce_message_dialog(toplevel_window_for_monitor_data(mdata),
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
    g_signal_connect_swapped(icon_view, "icon-activated",
                             G_CALLBACK(xfdesktop_file_icon_manager_activate_selected), mdata);

    // DnD src signals
    g_signal_connect(icon_view, "drag-data-get",
                     G_CALLBACK(xfdesktop_file_icon_manager_drag_data_get), mdata);

    // DnD dest signals
    g_signal_connect(icon_view, "drag-motion",
                     G_CALLBACK(xfdesktop_file_icon_manager_drag_motion), mdata);
    g_signal_connect(icon_view, "drag-data-received",
                     G_CALLBACK(xfdesktop_file_icon_manager_drag_data_received), mdata);
    g_signal_connect(icon_view, "drag-drop",
                     G_CALLBACK(xfdesktop_file_icon_manager_drag_drop), mdata);
    g_signal_connect(icon_view, "drag-leave",
                     G_CALLBACK(xfdesktop_file_icon_manager_drag_leave), mdata);

    // Below signals allow us to sort icons and replace them where they belong in the newly-sized view
    g_signal_connect(G_OBJECT(icon_view), "start-grid-resize",
                     G_CALLBACK(xfdesktop_file_icon_manager_start_grid_resize), mdata);
    g_signal_connect(G_OBJECT(icon_view), "end-grid-resize",
                     G_CALLBACK(xfdesktop_file_icon_manager_end_grid_resize), mdata);

    GtkAccelGroup *accel_group = xfdesktop_icon_view_manager_get_accel_group(XFDESKTOP_ICON_VIEW_MANAGER(fmanager));
    mdata->holder = xfdesktop_icon_view_holder_new(screen, desktop, icon_view, accel_group);

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

    GList *candidates = NULL;
    mdata->position_config = xfdesktop_icon_position_configs_add_monitor(fmanager->position_configs,
                                                                         monitor,
                                                                         level,
                                                                         &candidates);

    if (mdata->position_config == NULL && candidates == NULL) {
        mdata->position_config = xfdesktop_icon_positions_try_migrate(channel, screen, monitor, level);
        if (mdata->position_config == NULL) {
            mdata->position_config = xfdesktop_icon_position_config_new(level);
        }
        xfdesktop_icon_position_configs_assign_monitor(fmanager->position_configs, mdata->position_config, monitor);
    }

    if (mdata->position_config == NULL) {
        g_assert(candidates != NULL);

        GtkBuilder *builder = gtk_builder_new_from_resource("/org/xfce/xfdesktop/monitor-candidates-chooser.glade");
        g_assert(builder != NULL);

        GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, "monitor_candidates_chooser"));

        gchar *monitor_question_text = g_strdup_printf( _("Would you like to assign an existing desktop icon layout to monitor <b>%s</b>?"),
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

                const gchar *identifier = xfdesktop_icon_peek_identifier(XFDESKTOP_ICON(icon));
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

    MonitorData *mdata = g_hash_table_lookup(fmanager->monitor_data, monitor);
    if (mdata != NULL) {
        for (GList *l = fmanager->pending_created_desktop_files; l != NULL;) {
            CreateDesktopFileWithPositionData *cdfwpdata = l->data;
            l = l->next;

            if (cdfwpdata->mdata == mdata) {
                cdfwpdata->mdata = NULL;
                fmanager->pending_created_desktop_files = g_list_remove(fmanager->pending_created_desktop_files,
                                                                        cdfwpdata);
                g_cancellable_cancel(cdfwpdata->cancellable);
            }
        }

        g_hash_table_remove(fmanager->monitor_data, monitor);
    }
}

static XfceDesktop *
xfdesktop_file_icon_manager_get_focused_desktop(XfdesktopIconViewManager *manager) {
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, fmanager->monitor_data);

    MonitorData *mdata;
    while (g_hash_table_iter_next(&iter, NULL, (gpointer)&mdata)) {
        XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
        GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(icon_view));
        if (GTK_IS_WINDOW(toplevel) && gtk_window_has_toplevel_focus(GTK_WINDOW(toplevel))) {
            return xfdesktop_icon_view_holder_get_desktop(mdata->holder);
        }
    }

    return NULL;
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
xfdesktop_file_icon_manager_get_context_menu(XfdesktopIconViewManager *manager,
                                             XfceDesktop *desktop,
                                             gint popup_x,
                                             gint popup_y)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);

    TRACE("ENTERING");

    GtkWidget *menu = gtk_menu_new();
    gtk_menu_set_reserve_toggle_size(GTK_MENU(menu), FALSE);

    MonitorData *mdata = g_hash_table_lookup(fmanager->monitor_data, xfce_desktop_get_monitor(desktop));
    g_return_val_if_fail(mdata != NULL, GTK_MENU(menu));
    XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);

    gint dest_row = -1, dest_col = -1;
    xfdesktop_icon_view_widget_coords_to_slot_coords(icon_view, popup_x, popup_y, &dest_row, &dest_col);

    GList *selected = xfdesktop_file_icon_manager_get_selected_icons(fmanager, icon_view);
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
                          mdata);
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
                                                                  mdata);
                    g_object_set_data(G_OBJECT(create_launcher_mi), LAUNCHER_TYPE_KEY, "Application");
                    g_object_set_data(G_OBJECT(create_launcher_mi), DEST_ROW_KEY, GINT_TO_POINTER(dest_row));
                    g_object_set_data(G_OBJECT(create_launcher_mi), DEST_COL_KEY, GINT_TO_POINTER(dest_col));

                    /* create link item */
                    GtkWidget *create_url_mi = add_menu_item(menu,
                                                             _("Create _URL Link..."),
                                                             g_themed_icon_new("insert-link"),
                                                             G_CALLBACK(xfdesktop_file_icon_menu_create_launcher),
                                                             mdata);
                    g_object_set_data(G_OBJECT(create_url_mi), LAUNCHER_TYPE_KEY, "Link");
                    g_object_set_data(G_OBJECT(create_url_mi), DEST_ROW_KEY, GINT_TO_POINTER(dest_row));
                    g_object_set_data(G_OBJECT(create_url_mi), DEST_COL_KEY, GINT_TO_POINTER(dest_col));

                    /* create folder item */
                    GtkWidget *create_folder_mi = add_menu_item(menu,
                                                                _("Create _Folder..."),
                                                                g_themed_icon_new("folder-new"),
                                                                G_CALLBACK(xfdesktop_file_icon_menu_create_folder),
                                                                mdata);
                    g_object_set_data(G_OBJECT(create_folder_mi), DEST_ROW_KEY, GINT_TO_POINTER(dest_row));
                    g_object_set_data(G_OBJECT(create_folder_mi), DEST_COL_KEY, GINT_TO_POINTER(dest_col));

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
                        DBG("templates dir path: %s", templates_dir_path);
                        GFile *templates_dir = NULL;
                        if(templates_dir_path) {
                            templates_dir = g_file_new_for_path(templates_dir_path);
                        }

                        if(templates_dir && !g_file_equal(home_dir, templates_dir)) {
                            xfdesktop_file_icon_menu_fill_template_menu(tmpl_menu,
                                                                        templates_dir,
                                                                        mdata,
                                                                        dest_row,
                                                                        dest_col,
                                                                        FALSE);
                        }

                        GList *children = gtk_container_get_children(GTK_CONTAINER(tmpl_menu));
                        if (children == NULL) {
                            GtkWidget *no_tmpl_mi = add_menu_item(tmpl_menu,
                                                                  _("No templates installed"),
                                                                   NULL,
                                                                   NULL,
                                                                   NULL);
                            gtk_widget_set_sensitive(no_tmpl_mi, FALSE);
                        } else {
                            g_list_free(children);
                        }

                        if (templates_dir) {
                            g_object_unref(templates_dir);
                        }
                        g_object_unref(home_dir);

                        add_menu_separator(tmpl_menu);

                        /* add the "Empty File" template option */
                        GtkWidget *create_empty_file_mi = add_menu_item(tmpl_menu,
                                                                        _("_Empty File"),
                                                                        g_themed_icon_new("text-x-generic"),
                                                                        G_CALLBACK(xfdesktop_file_icon_template_item_activated),
                                                                        mdata);
                        g_object_set_data(G_OBJECT(create_empty_file_mi), DEST_ROW_KEY, GINT_TO_POINTER(dest_row));
                        g_object_set_data(G_OBJECT(create_empty_file_mi), DEST_COL_KEY, GINT_TO_POINTER(dest_col));
                    }
                } else {
                    /* Menu on folder icons */
                    add_menu_item(menu,
                                  multi_sel ? _("_Open All") : _("_Open"),
                                  g_themed_icon_new("document-open"),
                                  G_CALLBACK(xfdesktop_file_icon_menu_open_folder),
                                  mdata);
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
                                  mdata);

                    add_menu_separator(menu);

                    if(g_content_type_equals(g_file_info_get_content_type(info),
                                             "application/x-desktop"))
                    {
                        GFile *file = xfdesktop_file_icon_peek_file(file_icon);

                        GtkWidget *edit_launcher_mi = add_menu_item(menu,
                                                                    _("Edit _Launcher"),
                                                                    g_themed_icon_new("gtk-edit"),
                                                                    G_CALLBACK(xfdesktop_file_icon_menu_edit_launcher),
                                                                    mdata);
                        g_object_set_data_full(G_OBJECT(edit_launcher_mi), FILE_KEY, g_object_ref(file), g_object_unref);

                        add_menu_separator(menu);
                    }
                }

                app_infos = g_app_info_get_all_for_type(g_file_info_get_content_type(info));
                if(app_infos) {
                    GAppInfo *app_info, *default_application;
                    GList *ap;

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

                    GtkWidget *app_info_mi = xfdesktop_menu_item_from_app_info(mdata, file_icon, app_info, TRUE, TRUE);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), app_info_mi);

                    g_object_unref(app_info);

                    GtkWidget *app_infos_menu;
                    if(app_infos->next) {
                        gboolean use_submenu = (g_list_length(app_infos->next) >= 3);

                        if (use_submenu || !xfdesktop_file_utils_file_is_executable(info)) {
                            add_menu_separator(menu);
                        }

                        if (use_submenu) {
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
                            GtkWidget *mi = xfdesktop_menu_item_from_app_info(mdata, file_icon, app_info, FALSE, TRUE);
                            gtk_menu_shell_append(GTK_MENU_SHELL(app_infos_menu), mi);
                            g_object_unref(app_info);
                        }

                        if (use_submenu) {
                            add_menu_separator(app_infos_menu);
                        }
                    } else {
                        app_infos_menu = menu;
                    }

                    if (!multi_sel) {
                        add_menu_item(app_infos_menu,
                                      _("Ope_n With Other Application..."),
                                      NULL,
                                      G_CALLBACK(xfdesktop_file_icon_menu_other_app),
                                      mdata);

                        add_menu_separator(app_infos_menu);

                        add_menu_item(app_infos_menu,
                                      _("Set Defa_ult Application..."),
                                      NULL,
                                      G_CALLBACK(xfdesktop_file_icon_menu_set_default_app),
                                      mdata);
                    }

                    g_list_free(app_infos);
                } else {
                    add_menu_item(menu,
                                  _("Ope_n With Other Application..."),
                                  g_themed_icon_new("document-open"),
                                  G_CALLBACK(xfdesktop_file_icon_menu_other_app),
                                  mdata);
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
                                                mdata);
            gtk_widget_set_sensitive(paste_mi, xfdesktop_clipboard_manager_get_can_paste(clipboard_manager));
            g_object_set_data(G_OBJECT(paste_mi), DEST_ROW_KEY, GINT_TO_POINTER(dest_row));
            g_object_set_data(G_OBJECT(paste_mi), DEST_COL_KEY, GINT_TO_POINTER(dest_col));

            add_menu_separator(menu);
        } else if(!multi_sel_special) {
            /* Menu popup on an icon */
            /* Cut */
            GtkWidget *cut_mi = add_menu_item(menu,
                                              _("Cu_t"),
                                              g_themed_icon_new("edit-cut"),
                                              G_CALLBACK(xfdesktop_file_icon_menu_cut),
                                              mdata);
            gtk_widget_set_sensitive(cut_mi, multi_sel || xfdesktop_file_icon_can_delete_file(file_icon));

            /* Copy */
            add_menu_item(menu,
                          _("_Copy"),
                          g_themed_icon_new("edit-copy"),
                          G_CALLBACK(xfdesktop_file_icon_menu_copy),
                          mdata);

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
                                                    mdata);
                gtk_widget_set_sensitive(paste_mi, xfdesktop_clipboard_manager_get_can_paste(clipboard_manager));
            }

            add_menu_separator(menu);

            /* Trash */
            GtkWidget *trash_mi = add_menu_item(menu,
                                                _("Mo_ve to Trash"),
                                                g_themed_icon_new("user-trash"),
                                                G_CALLBACK(xfdesktop_file_icon_menu_trash),
                                                mdata);
            gtk_widget_set_sensitive(trash_mi, multi_sel || xfdesktop_file_icon_can_delete_file(file_icon));

            /* Delete */
            if(fmanager->show_delete_menu) {
                GtkWidget *delete_mi = add_menu_item(menu,
                                                     _("_Delete"),
                                                     g_themed_icon_new("edit-delete"),
                                                     G_CALLBACK(xfdesktop_file_icon_menu_delete),
                                                     mdata);
                gtk_widget_set_sensitive(delete_mi, multi_sel || xfdesktop_file_icon_can_delete_file(file_icon));
            }

            add_menu_separator(menu);

            /* Rename */
            GtkWidget *rename_mi = add_menu_item(menu,
                                                 _("_Rename..."),
                                                 NULL,
                                                 G_CALLBACK(xfdesktop_file_icon_menu_rename),
                                                 mdata);
            gtk_widget_set_sensitive(rename_mi, multi_sel || xfdesktop_file_icon_can_rename_file(file_icon));

            add_menu_separator(menu);
        }

#ifdef HAVE_THUNARX
        if(!multi_sel_special && fmanager->thunarx_menu_providers) {
            GtkWidget *gtk_menu_item = NULL;

            for(GList *l = fmanager->thunarx_menu_providers; l; l = l->next) {
                ThunarxMenuProvider *provider = THUNARX_MENU_PROVIDER(l->data);

                GtkWidget *toplevel = GTK_WIDGET(toplevel_window_for_monitor_data(mdata));
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
                          mdata);

            /* show arrange desktop icons option */
            add_menu_item(menu,
                          _("Arrange Desktop _Icons"),
                          g_themed_icon_new("view-sort-ascending"),
                          G_CALLBACK(xfdesktop_file_icon_menu_arrange_icons),
                          mdata);

            XfwScreen *xfw_screen = xfdesktop_icon_view_manager_get_screen(XFDESKTOP_ICON_VIEW_MANAGER(fmanager));
            XfwMonitor *monitor = xfce_desktop_get_monitor(desktop);
            XfwWorkspace *workspace = xfdesktop_find_active_workspace_on_monitor(xfw_screen, monitor);
            XfdesktopBackdropManager *backdrop_manager = xfdesktop_icon_view_manager_get_backdrop_manager(XFDESKTOP_ICON_VIEW_MANAGER(fmanager));
            if (monitor != NULL
                && workspace != NULL
                && xfdesktop_backdrop_manager_can_cycle_backdrop(backdrop_manager, monitor, workspace))
            {
                /* show next background option */
                add_menu_item(menu,
                              _("_Next Background"),
                              g_themed_icon_new("go-next"),
                              G_CALLBACK(xfdesktop_file_icon_menu_next_background),
                              mdata);
            }

            add_menu_separator(menu);

            /* Desktop settings window */
            add_menu_item(menu,
                          _("Desktop _Settings..."),
                          g_themed_icon_new("preferences-desktop-wallpaper"),
                          G_CALLBACK(xfdesktop_settings_launch),
                          mdata);
        } else {
            /* Properties - applies only to icons on the desktop */
            GtkWidget *properties_mi = add_menu_item(menu,
                                                     _("_Properties..."),
                                                     g_themed_icon_new("document-properties"),
                                                     G_CALLBACK(file_icon == fmanager->desktop_icon
                                                                ? xfdesktop_file_icon_manager_desktop_properties
                                                                : xfdesktop_file_icon_menu_properties),
                                                     mdata);
            gtk_widget_set_sensitive(properties_mi, info != NULL);
        }
    }

    /* don't free |selected|.  the menu deactivated handler does that */

    return GTK_MENU(menu);
}

static void
xfdesktop_file_icon_manager_activate_icons(XfdesktopIconViewManager *manager) {
    MonitorData *mdata = find_active_monitor_data(XFDESKTOP_FILE_ICON_MANAGER(manager));
    if (mdata != NULL) {
        xfdesktop_file_icon_manager_activate_selected(mdata);
    }
}

static void
xfdesktop_file_icon_manager_toggle_cursor_icon(XfdesktopIconViewManager *manager) {
    MonitorData *mdata = find_active_monitor_data(XFDESKTOP_FILE_ICON_MANAGER(manager));
    if (mdata != NULL) {
        XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
        xfdesktop_icon_view_toggle_cursor(icon_view);
    }
}

static void
xfdesktop_file_icon_manager_select_all_icons(XfdesktopIconViewManager *manager) {
    MonitorData *mdata = find_active_monitor_data(XFDESKTOP_FILE_ICON_MANAGER(manager));
    if (mdata != NULL) {
        XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
        xfdesktop_icon_view_select_all(icon_view);
    }
}

static void
xfdesktop_file_icon_manager_unselect_all_icons(XfdesktopIconViewManager *manager) {
    MonitorData *mdata = find_active_monitor_data(XFDESKTOP_FILE_ICON_MANAGER(manager));
    if (mdata != NULL) {
        XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
        xfdesktop_icon_view_unselect_all(icon_view);
    }
}

static void
xfdesktop_file_icon_manager_sort_icons(XfdesktopIconViewManager *manager,
                                       GtkSortType sort_type,
                                       XfdesktopIconViewManagerSortFlags flags)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    gboolean sort_all = (flags & XFDESKTOP_ICON_VIEW_MANAGER_SORT_ALL_DESKTOPS) != 0;

    GHashTableIter iter;
    g_hash_table_iter_init(&iter, fmanager->monitor_data);

    MonitorData *mdata;
    while (g_hash_table_iter_next(&iter, NULL, (gpointer)&mdata)) {
        XfceDesktop *desktop = xfdesktop_icon_view_holder_get_desktop(mdata->holder);
        if (sort_all || xfce_desktop_is_active(desktop)) {
            XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
            xfdesktop_icon_view_sort_icons(icon_view, sort_type);
            if (!sort_all) {
                break;
            }
        }
    }
}

static void
xfdesktop_file_icon_manager_reload(XfdesktopIconViewManager *manager) {
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);

    GHashTableIter iter;
    g_hash_table_iter_init(&iter, fmanager->monitor_data);

    MonitorData *mdata;
    while (g_hash_table_iter_next(&iter, NULL, (gpointer)&mdata)) {
        XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
        xfdesktop_icon_view_set_model(icon_view, NULL);
    }

    fmanager->ready = FALSE;
    xfdesktop_file_icon_model_reload(fmanager->model);
}

static XfwMonitor *
xfdesktop_file_icon_manager_get_cached_icon_position(XfdesktopFileIconManager *fmanager,
                                                     XfdesktopFileIcon *icon,
                                                     gint16 *ret_row,
                                                     gint16 *ret_col)
{
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON_MANAGER(fmanager), NULL);
    g_return_val_if_fail(ret_row != NULL && ret_col != NULL, NULL);

    const gchar *identifier = xfdesktop_icon_peek_identifier(XFDESKTOP_ICON(icon));
    XfwMonitor *monitor = NULL;
    gint row = -1;
    gint col = -1;
    gboolean success = xfdesktop_icon_position_configs_lookup(fmanager->position_configs,
                                                              identifier,
                                                              &monitor,
                                                              &row,
                                                              &col);
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

static gboolean
update_icon_position(MonitorData *mdata, XfdesktopFileIcon *icon, gint row, gint col) {
    XfceDesktop *desktop = xfdesktop_icon_view_holder_get_desktop(mdata->holder);
    XfwMonitor *monitor = xfce_desktop_get_monitor(desktop);
    gboolean changed = xfdesktop_icon_set_monitor(XFDESKTOP_ICON(icon), monitor);
    changed |= xfdesktop_icon_set_position(XFDESKTOP_ICON(icon), row, col);

    if (changed) {
        const gchar *identifier = xfdesktop_icon_peek_identifier(XFDESKTOP_ICON(icon));
        guint64 last_seen = XFDESKTOP_IS_VOLUME_ICON(icon) ? g_get_real_time() : 0;
        xfdesktop_icon_position_configs_set_icon_position(mdata->fmanager->position_configs,
                                                          mdata->position_config,
                                                          identifier,
                                                          row,
                                                          col,
                                                          last_seen);

        GtkTreeIter iter;
        if (xfdesktop_file_icon_model_get_icon_iter(mdata->fmanager->model, icon, &iter)) {
            GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(mdata->fmanager->model), &iter);
            gtk_tree_model_row_changed(GTK_TREE_MODEL(mdata->fmanager->model), path, &iter);
            gtk_tree_path_free(path);
        }
    }

    return changed;
}

static void
clear_icon_position(MonitorData *mdata, XfdesktopFileIcon *icon) {
    xfdesktop_icon_set_position(XFDESKTOP_ICON(icon), -1, -1);
    
    const gchar *identifier = xfdesktop_icon_peek_identifier(XFDESKTOP_ICON(icon));
    xfdesktop_icon_position_configs_remove_icon(mdata->fmanager->position_configs, mdata->position_config, identifier);
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

static GList *
xfdesktop_file_icon_manager_get_selected_icons(XfdesktopFileIconManager *fmanager, XfdesktopIconView *icon_view) {
    GList *selected_icons = NULL;

    XfdesktopFileIconModelFilter *filter = XFDESKTOP_FILE_ICON_MODEL_FILTER(xfdesktop_icon_view_get_model(icon_view));
    GList *selected = xfdesktop_icon_view_get_selected_items(icon_view);
    for (GList *l = selected; l != NULL; l = l->next) {
        GtkTreePath *path = (GtkTreePath *)l->data;
        GtkTreeIter iter;

        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(filter), &iter, path)) {
            XfdesktopFileIcon *icon = xfdesktop_file_icon_model_filter_get_icon(filter, &iter);
            if (icon != NULL) {
                selected_icons = g_list_prepend(selected_icons, icon);
            }
        }
    }
    g_list_free_full(selected, (GDestroyNotify)gtk_tree_path_free);

    return selected_icons;
}

static MonitorData *
find_active_monitor_data(XfdesktopFileIconManager *fmanager) {
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, fmanager->monitor_data);

    MonitorData *mdata;
    while (g_hash_table_iter_next(&iter, NULL, (gpointer)&mdata)) {
        GtkWindow *toplevel = toplevel_window_for_monitor_data(mdata);
        if (gtk_window_has_toplevel_focus(toplevel)) {
            return mdata;
        }
    }

    return NULL;
}

static void
file_icon_manager_action_create_folder(XfdesktopFileIconManager *fmanager) {
    MonitorData *mdata = find_active_monitor_data(fmanager);
    if (mdata != NULL) {
        xfdesktop_file_icon_manager_create_folder(mdata, -1, -1, toplevel_window_for_monitor_data(mdata));
    }
}

static void
file_icon_manager_action_create_document(XfdesktopFileIconManager *fmanager) {
    MonitorData *mdata = find_active_monitor_data(fmanager);
    if (mdata != NULL) {
        create_from_template(mdata, NULL, -1, -1);
    }
}

static void
file_icon_manager_action_open(XfdesktopFileIconManager *fmanager) {
    MonitorData *mdata = find_active_monitor_data(fmanager);
    if (mdata != NULL) {
        xfdesktop_file_icon_manager_open_icon(mdata);
    }
}

static void
file_icon_manager_action_open_with_other(XfdesktopFileIconManager *fmanager) {
    // TODO
}

static void
file_icon_manager_action_open_filesystem(XfdesktopFileIconManager *fmanager) {
    GFile *file = g_file_new_for_uri("file:///");
    GList link = {
        .data = file,
        .prev = NULL,
        .next = NULL,
    };
    MonitorData *mdata = find_active_monitor_data(fmanager);
    xfdesktop_file_utils_open_folders(&link,
                                      fmanager->gscreen,
                                      mdata != NULL ? toplevel_window_for_monitor_data(mdata) : NULL);
    g_object_unref(file);
}

static void
file_icon_manager_action_open_home(XfdesktopFileIconManager *fmanager) {
    GFile *file = g_file_new_for_path(g_get_home_dir());
    GList link = {
        .data = file,
        .prev = NULL,
        .next = NULL,
    };
    MonitorData *mdata = find_active_monitor_data(fmanager);
    xfdesktop_file_utils_open_folders(&link,
                                      fmanager->gscreen,
                                      mdata != NULL ? toplevel_window_for_monitor_data(mdata) : NULL);
    g_object_unref(file);
}

static void
file_icon_manager_action_open_trash(XfdesktopFileIconManager *fmanager) {
    GFile *file = g_file_new_for_uri("trash:///");
    GList link = {
        .data = file,
        .prev = NULL,
        .next = NULL,
    };
    MonitorData *mdata = find_active_monitor_data(fmanager);
    xfdesktop_file_utils_open_folders(&link,
                                      fmanager->gscreen,
                                      mdata != NULL ? toplevel_window_for_monitor_data(mdata) : NULL);
    g_object_unref(file);
}

static void
file_icon_manager_action_rename(XfdesktopFileIconManager *fmanager) {
    MonitorData *mdata = find_active_monitor_data(fmanager);
    if (mdata != NULL) {
        xfdesktop_file_icon_manager_rename(mdata);
    }
}

static void
file_icon_manager_action_cut(XfdesktopFileIconManager *fmanager) {
    MonitorData *mdata = find_active_monitor_data(fmanager);
    if (mdata != NULL) {
        xfdesktop_file_icon_manager_cut_selected_files(mdata);
    }
}

static void
file_icon_manager_action_copy(XfdesktopFileIconManager *fmanager) {
    MonitorData *mdata = find_active_monitor_data(fmanager);
    if (mdata != NULL) {
        xfdesktop_file_icon_manager_copy_selected_files(mdata);
    }
}

static void
file_icon_manager_action_paste(XfdesktopFileIconManager *fmanager) {
    MonitorData *mdata = find_active_monitor_data(fmanager);
    if (mdata != NULL) {
        xfdesktop_clipboard_manager_paste_files(clipboard_manager,
                                                mdata->fmanager->folder,
                                                GTK_WIDGET(xfdesktop_icon_view_holder_get_icon_view(mdata->holder)),
                                                NULL);
    }
}

static void
file_icon_manager_action_paste_into_folder(XfdesktopFileIconManager *fmanager) {
    MonitorData *mdata = find_active_monitor_data(fmanager);
    if (mdata != NULL) {
        xfdesktop_file_icon_manager_paste_into_folder(mdata);
    }
}

static void
file_icon_manager_action_trash(XfdesktopFileIconManager *fmanager) {
    MonitorData *mdata = find_active_monitor_data(fmanager);
    if (mdata != NULL) {
        xfdesktop_file_icon_manager_delete_selected(mdata, FALSE);
    }
}

static void
file_icon_manager_action_delete(XfdesktopFileIconManager *fmanager) {
    MonitorData *mdata = find_active_monitor_data(fmanager);
    if (mdata != NULL) {
        xfdesktop_file_icon_manager_delete_selected(mdata, TRUE);
    }
}

static void
file_icon_manager_action_empty_trash(XfdesktopFileIconManager *fmanager) {
    MonitorData *mdata = find_active_monitor_data(fmanager);
    xfdesktop_file_utils_empty_trash(fmanager->gscreen,
                                     mdata != NULL ? toplevel_window_for_monitor_data(mdata) : NULL);
}

static void
file_icon_manager_action_toggle_show_hidden(XfdesktopFileIconManager *fmanager) {
    XfconfChannel *channel = xfdesktop_icon_view_manager_get_channel(XFDESKTOP_ICON_VIEW_MANAGER(fmanager));
    gboolean value = xfconf_channel_get_bool(channel, DESKTOP_ICONS_SHOW_HIDDEN_FILES, FALSE);
    xfconf_channel_set_bool(channel, DESKTOP_ICONS_SHOW_HIDDEN_FILES, !value);
}

static void
file_icon_manager_action_properties(XfdesktopFileIconManager *fmanager) {
    MonitorData *mdata = find_active_monitor_data(fmanager);
    if (mdata != NULL) {
        xfdesktop_file_icon_manager_show_selected_properties(mdata);
    }
}

static void
file_icon_manager_action_fixup(XfceGtkActionEntry *entry) {
    switch (entry->id) {
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_CREATE_FOLDER:
            entry->callback = G_CALLBACK(file_icon_manager_action_create_folder);
            break;
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_CREATE_DOCUMENT:
            entry->callback = G_CALLBACK(file_icon_manager_action_create_document);
            break;
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_OPEN:
            entry->callback = G_CALLBACK(file_icon_manager_action_open);
            break;
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_OPEN_WITH_OTHER:
            entry->callback = G_CALLBACK(file_icon_manager_action_open_with_other);
            break;
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_OPEN_FILESYSTEM:
            entry->callback = G_CALLBACK(file_icon_manager_action_open_filesystem);
            break;
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_OPEN_HOME:
            entry->callback = G_CALLBACK(file_icon_manager_action_open_home);
            break;
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_OPEN_TRASH:
            entry->callback = G_CALLBACK(file_icon_manager_action_open_trash);
            break;
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_RENAME:
            entry->callback = G_CALLBACK(file_icon_manager_action_rename);
            break;
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_CUT:
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_CUT_ALT_1:
            entry->callback = G_CALLBACK(file_icon_manager_action_cut);
            break;
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_COPY:
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_COPY_ALT_1:
            entry->callback = G_CALLBACK(file_icon_manager_action_copy);
            break;
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_PASTE:
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_PASTE_ALT_1:
            entry->callback = G_CALLBACK(file_icon_manager_action_paste);
            break;
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_PASTE_INTO_FOLDER:
            entry->callback = G_CALLBACK(file_icon_manager_action_paste_into_folder);
            break;
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_TRASH:
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_TRASH_ALT_1:
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_TRASH_ALT_2:
            entry->callback = G_CALLBACK(file_icon_manager_action_trash);
            break;
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_EMPTY_TRASH:
            entry->callback = G_CALLBACK(file_icon_manager_action_empty_trash);
            break;
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_DELETE:
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_DELETE_ALT_1:
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_DELETE_ALT_2:
            entry->callback = G_CALLBACK(file_icon_manager_action_delete);
            break;
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_TOGGLE_SHOW_HIDDEN:
            entry->callback = G_CALLBACK(file_icon_manager_action_toggle_show_hidden);
            break;
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_PROPERTIES:
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_PROPERTIES_ALT_1:
        case XFDESKTOP_FILE_ICON_MANAGER_ACTION_PROPERTIES_ALT_2:
            entry->callback = G_CALLBACK(file_icon_manager_action_properties);
            break;
        default:
            g_assert_not_reached();
            break;
    }
}

static void
accel_map_changed(XfdesktopFileIconManager *fmanager) {
    gsize n_actions;
    XfceGtkActionEntry *actions = xfdesktop_get_file_icon_manager_actions(file_icon_manager_action_fixup, &n_actions);
    GtkAccelGroup *accel_group = xfdesktop_icon_view_manager_get_accel_group(XFDESKTOP_ICON_VIEW_MANAGER(fmanager));

    xfce_gtk_accel_group_disconnect_action_entries(accel_group, actions, n_actions);
    xfce_gtk_accel_group_connect_action_entries(accel_group, actions, n_actions, fmanager);
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

static gboolean
icon_is_writable_directory(XfdesktopFileIcon *icon) {
    GFileInfo *file_info = xfdesktop_file_icon_peek_file_info(icon);
    return file_info != NULL
        && g_file_info_get_file_type(file_info) == G_FILE_TYPE_DIRECTORY
        && (!g_file_info_has_attribute(file_info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE)
            || g_file_info_get_attribute_boolean(file_info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE));
}

static gboolean
files_are_on_same_filesystem(GFileInfo *a, GFileInfo *b) {
    const gchar *id_a = g_file_info_get_attribute_string(a, G_FILE_ATTRIBUTE_ID_FILESYSTEM);
    const gchar *id_b = g_file_info_get_attribute_string(b, G_FILE_ATTRIBUTE_ID_FILESYSTEM);
    DBG("a_fs_id=%s, b_fs_id=%s", id_a, id_b);
    return g_strcmp0(id_a, id_b) == 0;
}

static gboolean
file_is_writable_cached(GHashTable *cache, GFile *file) {
    gboolean parent_writable = FALSE;

    if (file != NULL) {
        if (g_hash_table_contains(cache, file)) {
            parent_writable = GPOINTER_TO_UINT(g_hash_table_lookup(cache, file));
        } else {
            GFileInfo *file_info = g_file_query_info(file, "access::can-write", G_FILE_QUERY_INFO_NONE, NULL, NULL);
            if (file_info != NULL) {
                parent_writable = g_file_info_get_attribute_boolean(file_info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
                DBG("got parent_writable: %d", parent_writable);
                g_hash_table_insert(cache, g_object_ref(file), GUINT_TO_POINTER(parent_writable));
                g_object_unref(file_info);
            }
        }
    }

    return parent_writable;
}

static GdkDragAction
select_drag_action(MonitorData *mdata, GdkDragContext *context, GList *src_files, guint time_, gboolean do_ask) {
    GFile *dest_file = xfdesktop_file_icon_peek_file(mdata->fmanager->icon_on_drop_dest);
    GFileInfo *dest_file_info = xfdesktop_file_icon_peek_file_info(mdata->fmanager->icon_on_drop_dest);

    DBG("df=%p, dfi=%p", dest_file, dest_file_info);
    if (dest_file == NULL || dest_file_info == NULL) {
        return 0;
    }

    GdkDragAction actions = gdk_drag_context_get_actions(context);

    GdkDragAction suggested_action;
    if ((actions & GDK_ACTION_ASK) != 0) {
        // If asking is an option, let's just do that.
        suggested_action = GDK_ACTION_ASK;
    } else {
        suggested_action = gdk_drag_context_get_suggested_action(context);
    }

    if (icon_is_writable_directory(mdata->fmanager->icon_on_drop_dest)) {
        // Mask out all but the actions we support.
        gchar *dest_uri = g_file_get_uri(dest_file);
        gboolean dest_is_trash = g_strcmp0(dest_uri, "trash:///") == 0;
        g_free(dest_uri);
        if (dest_is_trash) {
            actions &= GDK_ACTION_MOVE | GDK_ACTION_ASK;
        } else {
            actions &= GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_ASK;
        }

        // Don't allow drops when src_files contains the destination.
        for (GList *l = src_files; l != NULL; l = l->next) {
            GFile *src_file = G_FILE(l->data);
            if (g_file_equal(src_file, dest_file)) {
                actions = 0;
                break;
            }
        }

        DBG("is trash=%d, actions=0x%08x", dest_is_trash, actions);

        // GTK will often suggest _COPY, but if the srcs and dest are on the
        // same filesystem, we should default to _MOVE.
        if ((actions & (GDK_ACTION_MOVE | GDK_ACTION_COPY)) != 0 && suggested_action == GDK_ACTION_COPY) {
            GHashTable *parent_writable_cache = g_hash_table_new_full(g_file_hash,
                                                                      (GEqualFunc)g_file_equal,
                                                                      g_object_unref,
                                                                      NULL);  // GFileInfo -> gboolean

            for (GList *l = src_files; l != NULL; l = l->next) {
                GFile *src_file = G_FILE(l->data);

                GFile *parent = g_file_get_parent(src_file);
                gboolean parent_writable = parent != NULL && file_is_writable_cached(parent_writable_cache, parent);
                if (parent != NULL) {
                    g_object_unref(parent);
                }

                GFileInfo *src_file_info = g_file_query_info(src_file, "id::filesystem", G_FILE_QUERY_INFO_NONE, NULL, NULL);
                gboolean on_same_filesystem = src_file_info != NULL
                    && files_are_on_same_filesystem(src_file_info, dest_file_info);
                if (src_file_info != NULL) {
                    g_object_unref(src_file_info);
                }

                DBG("parent_writable=%d, on_same_fs=%d", parent_writable, on_same_filesystem);
                if (parent_writable && (on_same_filesystem || dest_is_trash)) {
                    // This file has a writable parent (so can be deleted), and
                    // is on the same file system as our destination, so _MOVE
                    // is possible, though we still may need to check more
                    // files.
                    suggested_action = GDK_ACTION_MOVE;
                } else {
                    // If the src file's parent is not writable (meaning we
                    // can't delete so we can't move) or if the src file and
                    // the destination are on different filesystems, we default
                    // to _COPY.
                    suggested_action = GDK_ACTION_COPY;
                    break;
                }
            }

            g_hash_table_destroy(parent_writable_cache);
        }
    } else if (xfdesktop_file_utils_file_is_executable(dest_file_info)) {
        actions &= GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_PRIVATE;
    } else {
        DBG("welp");
        actions = 0;
    }

    GdkDragAction selected_action;
    if ((actions & suggested_action) != 0) {
        selected_action = suggested_action;
    } else if ((actions & GDK_ACTION_ASK) != 0) {
        selected_action = GDK_ACTION_ASK;
    } else if ((actions & GDK_ACTION_COPY) != 0) {
        selected_action = GDK_ACTION_COPY;
    } else if ((actions & GDK_ACTION_LINK) != 0) {
        selected_action = GDK_ACTION_LINK;
    } else if ((actions & GDK_ACTION_MOVE) != 0) {
        selected_action = GDK_ACTION_MOVE;
    } else if ((actions & GDK_ACTION_PRIVATE) != 0) {
        selected_action = GDK_ACTION_PRIVATE;
    } else {
        selected_action = 0;
    }

    if (do_ask && selected_action == GDK_ACTION_ASK) {
        XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
        selected_action = xfdesktop_file_icon_manager_drag_drop_ask(mdata->fmanager,
                                                                    GTK_WIDGET(icon_view),
                                                                    actions,
                                                                    time_);
    }

    return selected_action;
}

static XfdesktopFileIcon *
determine_icon_on_drop_dest(MonitorData *mdata, gint x, gint y) {
    XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
    GtkTreeIter iter;
    if (xfdesktop_icon_view_widget_coords_to_item(XFDESKTOP_ICON_VIEW(icon_view), x, y, &iter)) {
        return xfdesktop_file_icon_model_filter_get_icon(mdata->filter, &iter);
    } else if (xfdesktop_icon_view_widget_coords_to_slot_coords(XFDESKTOP_ICON_VIEW(icon_view), x, y, NULL, NULL)) {
        return mdata->fmanager->desktop_icon;
    } else {
        return NULL;
    }
}

static void
handle_drag_highlight(XfdesktopIconView *icon_view, gint x, gint y, GdkDragAction action) {
    if (action != 0) {
        gint row, col;
        if (xfdesktop_icon_view_widget_coords_to_slot_coords(XFDESKTOP_ICON_VIEW(icon_view), x, y, &row, &col)) {
            xfdesktop_icon_view_draw_highlight(XFDESKTOP_ICON_VIEW(icon_view), row, col);
        } else {
            xfdesktop_icon_view_unset_highlight(icon_view);
        }
    } else {
        xfdesktop_icon_view_unset_highlight(icon_view);
    }
}

static gboolean
xfdesktop_file_icon_manager_drag_motion(GtkWidget *icon_view,
                                        GdkDragContext *context,
                                        gint x,
                                        gint y,
                                        guint time_,
                                        MonitorData *mdata)
{
    TRACE("entering");

    mdata->fmanager->icon_on_drop_dest = determine_icon_on_drop_dest(mdata, x, y);

    GtkTargetList *targets = xfdesktop_icon_view_get_drag_dest_targets(XFDESKTOP_ICON_VIEW(icon_view));
    GdkAtom target = gtk_drag_dest_find_target(icon_view, context, targets);

    if (mdata->fmanager->icon_on_drop_dest == NULL) {
        // Drag is outside any drop area (e.g. margin around the icon view).
        return FALSE;
    } else if (target == xfdesktop_icon_view_get_icon_drag_target()
               && mdata->fmanager->icon_on_drop_dest == mdata->fmanager->desktop_icon)
    {
        // Dragging icons from the view to an empty spot; icon view will handle
        // this itself.
        return FALSE;
    } else if (!mdata->fmanager->drag_data_received) {
        if (target == xfdesktop_icon_view_get_icon_drag_target()
            || target == gdk_atom_intern_static_string(TEXT_URI_LIST_NAME))
        {
            // We need drag data before we know what we can do.
            gtk_drag_get_data(icon_view, context, target, time_);
            gdk_drag_status(context, 0, time_);
            return TRUE;
        } else {
            if (icon_is_writable_directory(mdata->fmanager->icon_on_drop_dest)) {
                GdkDragAction action;
                if (target == gdk_atom_intern_static_string(XDND_DIRECT_SAVE0_NAME)
                    || target == gdk_atom_intern_static_string(APPLICATION_OCTET_STREAM_NAME))
                {
                    // Direct save is more or less a "copy" of the data.
                    action = GDK_ACTION_COPY;
                } else if (target == gdk_atom_intern_static_string(NETSCAPE_URL_NAME)) {
                    // URLs can only create .desktop files of type "Link".
                    action = GDK_ACTION_LINK;
                } else {
                    // Some other target; shouldn't happen, I don't think.
                    action = 0;
                }

                if (action != 0) {
                    gdk_drag_status(context, action, time_);
                    return TRUE;
                } else {
                    return FALSE;
                }
            } else {
                // These types can only be dropped on the desktop or on folder icons.
                return FALSE;
            }
        }
    } else {
        // We have our drag data, so we can tell GDK which action we want.
        GdkDragAction action = select_drag_action(mdata, context, mdata->fmanager->dragged_files, time_, FALSE);
        handle_drag_highlight(XFDESKTOP_ICON_VIEW(icon_view), x, y, action);
        gdk_drag_status(context, action, time_);
        return TRUE;
    }
}

static gchar *
fetch_xds_property(GdkDragContext *context, GdkAtom *out_actual_type) {
    GdkAtom actual_type;
    gint actual_format;
    gint actual_length;
    guchar *prop_data = NULL;

    if (gdk_property_get(gdk_drag_context_get_source_window(context),
                         gdk_atom_intern_static_string(XDND_DIRECT_SAVE0_NAME),
                         GDK_NONE,
                         0,
                         G_MAXLONG,
                         FALSE,
                         &actual_type,
                         &actual_format,
                         &actual_length,
                         &prop_data))
    {
        gchar *type = gdk_atom_name(actual_type);
        gchar *value = g_strcmp0(type, "text/plain") == 0 || g_str_has_prefix(type, "text/plain; charset=")
            ? g_strndup((gchar *)prop_data, actual_length)
            : NULL;

        g_free(type);
        g_free(prop_data);

        if (out_actual_type != NULL) {
            *out_actual_type = actual_type;
        }

        return value;
    } else {
        return NULL;
    }
}

static void
update_xds_property(GdkDragContext *context, GdkAtom type, const gchar *value) {
    gdk_property_change(gdk_drag_context_get_source_window(context),
                        gdk_atom_intern_static_string(XDND_DIRECT_SAVE0_NAME),
                        type,
                        8,
                        GDK_PROP_MODE_REPLACE,
                        (const guchar *)value,
                        strlen(value));
}

static gboolean
handle_files_drop_data(MonitorData *mdata,
                       GdkDragContext *context,
                       gint x,
                       gint y,
                       guint time_)
{
    g_return_val_if_fail(mdata->fmanager->icon_on_drop_dest != NULL, FALSE);
    g_return_val_if_fail(mdata->fmanager->dragged_files != NULL, FALSE);

    TRACE("entering");

    gboolean drop_ok = FALSE;

    GFile *tfile = xfdesktop_file_icon_peek_file(mdata->fmanager->icon_on_drop_dest);
    GFileInfo *tinfo = xfdesktop_file_icon_peek_file_info(mdata->fmanager->icon_on_drop_dest);
    if (tfile != NULL && tinfo != NULL) {
        XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
        GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(icon_view));
        GList *file_list = mdata->fmanager->dragged_files;
        GdkDragAction selected_action = select_drag_action(mdata, context, file_list, time_, TRUE);

        if (selected_action != 0) {
            if (g_file_has_uri_scheme(tfile, "trash")) {
                xfdesktop_file_utils_trash_files(file_list,
                                                 mdata->fmanager->gscreen,
                                                 GTK_WINDOW(toplevel));
                drop_ok = TRUE;
            } else if (xfdesktop_file_utils_file_is_executable(tinfo)) {
                drop_ok = xfdesktop_file_utils_execute(mdata->fmanager->folder,
                                                       tfile,
                                                       file_list,
                                                       mdata->fmanager->gscreen,
                                                       GTK_WINDOW(toplevel));
            } else if (icon_is_writable_directory(mdata->fmanager->icon_on_drop_dest)) {
                gint cur_row = -1, cur_col = -1;
                if (mdata->fmanager->icon_on_drop_dest == mdata->fmanager->desktop_icon) {
                    xfdesktop_icon_view_widget_coords_to_slot_coords(XFDESKTOP_ICON_VIEW(icon_view),
                                                                     x,
                                                                     y,
                                                                     &cur_row,
                                                                     &cur_col);
                }

                GList *dest_file_list = NULL;
                for (GList *l = file_list; l != NULL; l = l->next) {
                    gchar *dest_basename = g_file_get_basename(l->data);

                    if (dest_basename != NULL && dest_basename[0] != '\0') {
                        /* If we copy a file, we need to use the new absolute filename
                         * as the destination. If we move or link, we need to use the destination
                         * directory. */
                        if (selected_action == GDK_ACTION_COPY) {
                            GFile *dest_file = g_file_get_child(tfile, dest_basename);
                            dest_file_list = g_list_prepend(dest_file_list, dest_file);
                        } else {
                            dest_file_list = g_list_prepend(dest_file_list, g_object_ref(tfile));
                        }

                        if (cur_row != -1 && cur_col != -1) {
                            GFile *pending_file = g_file_get_child(tfile, dest_basename);
                            xfdesktop_icon_position_configs_set_icon_position(mdata->fmanager->position_configs,
                                                                              mdata->position_config,
                                                                              g_file_peek_path(pending_file),
                                                                              cur_row,
                                                                              cur_col,
                                                                              0);
                            g_object_unref(pending_file);
                        }
                    }

                    g_free(dest_basename);
                }

                if (dest_file_list != NULL) {
                    dest_file_list = g_list_reverse(dest_file_list);
                    xfdesktop_file_utils_transfer_files(selected_action,
                                                        file_list,
                                                        dest_file_list,
                                                        mdata->fmanager->gscreen);
                    g_list_free_full(dest_file_list, g_object_unref);
                    drop_ok = TRUE;
                }
            }
        }
    }

    return drop_ok;
}

static gboolean
handle_xdnd_direct_save_drop_data(GtkWidget *widget,
                                  GdkDragContext *context,
                                  GtkSelectionData *data,
                                  guint32 time_,
                                  gboolean *done)
{
    TRACE("entering");

    if (gtk_selection_data_get_length(data) == 1) {
        DBG("XDS message: %1s", gtk_selection_data_get_data(data));
        switch (gtk_selection_data_get_data(data)[0]) {
            case 'S':
                *done = TRUE;
                return TRUE;

            case 'F': {
                DBG("XDS source says they want us to save the file for them");
                GdkAtom application_octet_stream_atom = gdk_atom_intern_static_string(APPLICATION_OCTET_STREAM_NAME);
                GList *source_targets = gdk_drag_context_list_targets(context);
                for (GList *l = source_targets; l != NULL; l = l->next) {
                    GdkAtom target = GDK_POINTER_TO_ATOM(l->data);
                    if (target == application_octet_stream_atom) {
                        DBG("Ok, they support application/octet-stream; requesting data");
                        gtk_drag_get_data(widget, context, application_octet_stream_atom, time_);
                        *done = FALSE;
                        return TRUE;
                    }
                }

                *done = TRUE;
                update_xds_property(context, gdk_atom_intern_static_string("text/plain"), "");
                return FALSE;
            }

            case 'E':
            default:
                *done = TRUE;
                update_xds_property(context, gdk_atom_intern_static_string("text/plain"), "");
                return FALSE;
        }
    } else {
#ifdef DEBUG
        gchar *data_type = gdk_atom_name(gtk_selection_data_get_data_type(data));
        DBG("selection data type was %s, length was %d",
            data_type,
            gtk_selection_data_get_length(data));
        g_free(data_type);
#endif

        *done = TRUE;
        update_xds_property(context, gdk_atom_intern_static_string("text/plain"), "");
        return FALSE;
    }
}

static void
show_xds_drop_error(GFile *new_file, GError *error) {
    gchar *primary = g_strdup_printf(_("Failed to save dropped file to '%s'"), g_file_peek_path(new_file));
    xfce_message_dialog(NULL,
                        _("Drop Failure"),
                        "dialog-error",
                        primary,
                        error->message,
                        _("_OK"),
                        GTK_RESPONSE_ACCEPT,
                        NULL);
    g_free(primary);
}

static void
close_xds_file_ready(GObject *source, GAsyncResult *res, gpointer data) {
    GOutputStream *out = G_OUTPUT_STREAM(source);
    XdsDropData *xddata = data;

    GError *error = NULL;
    if (!g_output_stream_close_finish(out, res, &error)) {
        gtk_drag_finish(xddata->context, FALSE, FALSE, GDK_CURRENT_TIME);
        show_xds_drop_error(xddata->new_file, error);
        g_error_free(error);
        update_xds_property(xddata->context, gdk_atom_intern_static_string("text/plain"), "");
        g_file_delete(xddata->new_file, NULL, NULL);
    } else {
        gtk_drag_finish(xddata->context, TRUE, FALSE, GDK_CURRENT_TIME);
    }

    g_object_unref(out);
    xds_drop_data_free(xddata);
}

static void
write_all_xds_data_ready(GObject *source, GAsyncResult *res, gpointer data) {
    GOutputStream *out = G_OUTPUT_STREAM(source);
    XdsDropData *xddata = data;

    GError *error = NULL;
    if (!g_output_stream_write_all_finish(out, res, NULL, &error)) {
        gtk_drag_finish(xddata->context, FALSE, FALSE, GDK_CURRENT_TIME);
        show_xds_drop_error(xddata->new_file, error);
        g_error_free(error);
        update_xds_property(xddata->context, gdk_atom_intern_static_string("text/plain"), "");
        g_output_stream_close(out, NULL, NULL);
        g_object_unref(out);
        g_file_delete(xddata->new_file, NULL, NULL);
        xds_drop_data_free(xddata);
    } else {
        g_output_stream_close_async(out, G_PRIORITY_DEFAULT, NULL, close_xds_file_ready, xddata);
    }
}

static void
create_xds_file_ready(GObject *source, GAsyncResult *res, gpointer data) {
    GFile *new_file = G_FILE(source);
    XdsDropData *xddata = data;

    GError *error = NULL;
    GFileOutputStream *out = g_file_create_finish(new_file, res, &error);
    if (out == NULL) {
        gtk_drag_finish(xddata->context, FALSE, FALSE, GDK_CURRENT_TIME);
        show_xds_drop_error(new_file, error);
        g_error_free(error);
        update_xds_property(xddata->context, gdk_atom_intern_static_string("text/plain"), "");
        xds_drop_data_free(xddata);
    } else {
        g_output_stream_write_all_async(G_OUTPUT_STREAM(out),
                                        xddata->file_data,
                                        xddata->file_data_len,
                                        G_PRIORITY_DEFAULT,
                                        NULL,
                                        write_all_xds_data_ready,
                                        xddata);
    }
}

static gboolean
handle_application_octet_stream_drop_data(MonitorData *mdata,
                                          GdkDragContext *context,
                                          GtkSelectionData *data,
                                          gboolean *done)
{
    TRACE("entering");

    gboolean success = FALSE;

    if (mdata->fmanager->xdnd_direct_save_destination != NULL) {
        GFile *new_file = mdata->fmanager->xdnd_direct_save_destination;

        const guchar *file_data = gtk_selection_data_get_data(data);
        gint file_len = gtk_selection_data_get_length(data);
        DBG("got file data %p, len %d", file_data, file_len);

        if (file_data != NULL) {
            DBG("writing XDS file data to %s", g_file_peek_path(new_file));

            XdsDropData *xddata = g_new0(XdsDropData, 1);
            xddata->context = g_object_ref(context);
            xddata->new_file = g_object_ref(new_file);
            xddata->file_data = g_memdup2(file_data, file_len);
            xddata->file_data_len = file_len;

            g_file_create_async(new_file,
                                G_FILE_CREATE_NONE,
                                G_PRIORITY_DEFAULT,
                                NULL,
                                create_xds_file_ready,
                                xddata);
            success = TRUE;
            *done = FALSE;
        }
    }


    if (!success) {
        update_xds_property(context, gdk_atom_intern_static_string("text/plain"), "");
    }
    *done = !success;

    if (!*done) {
        // Even though we're not done, we don't need this data anymore, as
        // we've saved what we need in 'xddata'.
        XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
        xfdesktop_file_icon_manager_drag_leave(GTK_WIDGET(icon_view), context, 0, mdata);
    }

    return success;
}

static gboolean
handle_netscape_url_drop_data(MonitorData *mdata,
                              GdkDragContext *context,
                              gint x,
                              gint y,
                              GtkSelectionData *data,
                              guint32 time_)
{
    g_return_val_if_fail(mdata->fmanager->icon_on_drop_dest != NULL, FALSE);
    g_return_val_if_fail(icon_is_writable_directory(mdata->fmanager->icon_on_drop_dest), FALSE);

    TRACE("entering");

    gboolean drop_ok = FALSE;
    GFile *source_file = xfdesktop_file_icon_peek_file(mdata->fmanager->icon_on_drop_dest);
    gchar *exo_desktop_item_edit = g_find_program_in_path("exo-desktop-item-edit");
    if (source_file != NULL && exo_desktop_item_edit != NULL) {
        /* data is "URL\nTITLE" */
        gchar **parts = g_strsplit((const gchar *)gtk_selection_data_get_data(data), "\n", -1);

        if (2 == g_strv_length(parts)) {
            XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);

            CreateDesktopFileWithPositionData *cdfwpdata = NULL;
            if (mdata->fmanager->icon_on_drop_dest == mdata->fmanager->desktop_icon) {
                gint drop_row = -1, drop_col = -1;
                if (xfdesktop_icon_view_widget_coords_to_slot_coords(icon_view, x, y, &drop_row, &drop_col)) {
                    cdfwpdata = g_new0(CreateDesktopFileWithPositionData, 1);
                    cdfwpdata->cancellable = g_cancellable_new();
                    cdfwpdata->mdata = mdata;
                    cdfwpdata->dest_row = drop_row;
                    cdfwpdata->dest_col = drop_col;
                    mdata->fmanager->pending_created_desktop_files =
                        g_list_prepend(mdata->fmanager->pending_created_desktop_files, cdfwpdata);
                }
            }

            xfdesktop_file_utils_create_desktop_file(gtk_widget_get_screen(GTK_WIDGET(icon_view)),
                                                     source_file,
                                                     "Link",
                                                     parts[1],
                                                     parts[0],
                                                     cdfwpdata != NULL ? cdfwpdata->cancellable : NULL,
                                                     cdfwpdata != NULL ? desktop_file_created: NULL,
                                                     cdfwpdata);
            drop_ok = TRUE;
        }

        g_strfreev(parts);
    }

    g_free(exo_desktop_item_edit);

    return drop_ok;
}

static GList *
get_dragged_files_for_icons(MonitorData *mdata, GdkDragContext *context, GtkSelectionData *data) {
    g_assert(gtk_selection_data_get_length(data) == sizeof(XfdesktopDraggedIconList));

    XfdesktopDraggedIconList *icon_list = (gpointer)gtk_selection_data_get_data(data);

    XfdesktopIconView *dest_icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
    MonitorData *source_mdata = icon_list->source_icon_view == dest_icon_view
        ? mdata
        : monitor_data_for_icon_view(mdata->fmanager->monitor_data, icon_list->source_icon_view);
    g_return_val_if_fail(source_mdata != NULL, NULL);

    GList *dragged_files = NULL;
    for (GList *l = icon_list->dragged_icons; l != NULL; l = l->next) {
        GtkTreeIter *dragged_iter = l->data;
        XfdesktopFileIcon *dragged_icon = xfdesktop_file_icon_model_filter_get_icon(source_mdata->filter, dragged_iter);
        if (dragged_icon != NULL) {
            GFile *file = xfdesktop_file_icon_peek_file(dragged_icon);
            if (file != NULL) {
                dragged_files = g_list_prepend(dragged_files, g_object_ref(file));
            }
        }
    }

    return g_list_reverse(dragged_files);
}

static void
xfdesktop_file_icon_manager_drag_data_received(GtkWidget *icon_view,
                                               GdkDragContext *context,
                                               gint x,
                                               gint y,
                                               GtkSelectionData *data,
                                               guint info,
                                               guint time_,
                                               MonitorData *mdata)
{
#ifdef DEBUG_TRACE
    gchar *data_type = gdk_atom_name(gtk_selection_data_get_data_type(data));
    gchar *target_name = gdk_atom_name(gtk_selection_data_get_target(data));
    TRACE("entering, info: %d, selection data type: %s, target: %s", info, data_type, target_name);
    g_free(data_type);
    g_free(target_name);
#endif

    if (mdata->fmanager->icon_on_drop_dest != NULL) {
        if (!mdata->fmanager->drag_data_received) {
            if (info == xfdesktop_icon_view_get_icon_drag_info()
                && mdata->fmanager->icon_on_drop_dest != mdata->fmanager->desktop_icon)
            {
                mdata->fmanager->dragged_files = get_dragged_files_for_icons(mdata, context, data);
                mdata->fmanager->drag_data_received = TRUE;
            } else if (info == TARGET_TEXT_URI_LIST) {
                gchar *uri_list = g_strndup((gchar *)gtk_selection_data_get_data(data), gtk_selection_data_get_length(data));
                mdata->fmanager->dragged_files = xfdesktop_file_utils_file_list_from_string(uri_list);
                g_free(uri_list);
                mdata->fmanager->drag_data_received = TRUE;
            }
        }

        if (mdata->fmanager->drag_dropped) {
            gboolean success = FALSE;
            gboolean done = TRUE;

            if (info == xfdesktop_icon_view_get_icon_drag_info()
                && mdata->fmanager->icon_on_drop_dest != mdata->fmanager->desktop_icon)
            {
                success = handle_files_drop_data(mdata, context, x, y, time_);
            } else if (info == TARGET_XDND_DIRECT_SAVE0) {
                success = handle_xdnd_direct_save_drop_data(GTK_WIDGET(icon_view), context, data, time_, &done);
            } else if (info == TARGET_APPLICATION_OCTET_STREAM) {
                success = handle_application_octet_stream_drop_data(mdata, context, data, &done);
            } else if (info == TARGET_TEXT_URI_LIST) {
                success = handle_files_drop_data(mdata, context, x, y, time_);
            } else if (info == TARGET_NETSCAPE_URL) {
                success = handle_netscape_url_drop_data(mdata, context, x, y, data, time_);
            }

            if (done) {
                gtk_drag_finish(context, success, FALSE, time_);
                xfdesktop_file_icon_manager_drag_leave(icon_view, context, time_, mdata);
            }
        }
    }
}

static gboolean
handle_xdnd_direct_save_drop(MonitorData *mdata, GdkDragContext *context, gint x, gint y) {
    if (!icon_is_writable_directory(mdata->fmanager->icon_on_drop_dest)) {
        return FALSE;
    } else {
        gboolean drop_ok = FALSE;

        GdkAtom actual_type;
        gchar *filename = fetch_xds_property(context, &actual_type);
        if (filename != NULL) {
            // TODO: check charset

            if (strchr(filename, G_DIR_SEPARATOR) == NULL) {
                GFile *file = xfdesktop_file_icon_peek_file(mdata->fmanager->icon_on_drop_dest);

                GFile *save_file = g_file_get_child(file, filename);
                mdata->fmanager->xdnd_direct_save_destination = xfdesktop_file_utils_next_new_file_name(save_file);
                g_object_unref(save_file);

                if (mdata->fmanager->icon_on_drop_dest == mdata->fmanager->desktop_icon) {
                    gint drop_row, drop_col;
                    XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
                    if (xfdesktop_icon_view_widget_coords_to_slot_coords(icon_view, x, y, &drop_row, &drop_col)) {
                        DBG("adding pending new file '%s' at (%d, %d)",
                            g_file_peek_path(mdata->fmanager->xdnd_direct_save_destination),
                            drop_row, drop_col);
                        xfdesktop_icon_position_configs_set_icon_position(mdata->fmanager->position_configs,
                                                                          mdata->position_config,
                                                                          g_file_peek_path(mdata->fmanager->xdnd_direct_save_destination),
                                                                          drop_row,
                                                                          drop_col,
                                                                          0);
                    }
                }

                gchar *uri = g_file_get_uri(mdata->fmanager->xdnd_direct_save_destination);
                update_xds_property(context, actual_type, uri);
                g_free(uri);

                drop_ok = TRUE;
            } else {
                update_xds_property(context, gdk_atom_intern_static_string("text/plain"), "");
            }

            g_free(filename);
        }

        return drop_ok;
    }
}

static gboolean
xfdesktop_file_icon_manager_drag_drop(GtkWidget *icon_view,
                                      GdkDragContext *context,
                                      gint x,
                                      gint y,
                                      guint time_,
                                      MonitorData *mdata)
{
    // Re-check; drag-leave will have cleared this out.
    mdata->fmanager->icon_on_drop_dest = determine_icon_on_drop_dest(mdata, x, y);

    GtkTargetList *targets = xfdesktop_icon_view_get_drag_dest_targets(XFDESKTOP_ICON_VIEW(icon_view));
    GdkAtom target = gtk_drag_dest_find_target(icon_view, context, targets);

#ifdef DEBUG_TRACE
    gchar *target_name = gdk_atom_name(target);
    TRACE("entering, target=%s, drop_in_grid=%d, drop_is_desktop=%d",
          target_name,
          mdata->fmanager->icon_on_drop_dest != NULL,
          mdata->fmanager->icon_on_drop_dest == mdata->fmanager->desktop_icon);
    g_free(target_name);
#endif

    if (target == GDK_NONE
        || mdata->fmanager->icon_on_drop_dest == NULL
        || (target == xfdesktop_icon_view_get_icon_drag_target()
            && mdata->fmanager->icon_on_drop_dest == mdata->fmanager->desktop_icon))
    {
        // Either:
        // 1. Unknown target.
        // 2. Drop destination isn't valid at all (e.g. outside the grid in the
        //    margin).
        // 3. Icons in the view are being dragged to an empty space and the
        //    icon view will handle it.
        return FALSE;
    } else  {
        DBG("dropped, getting data for target %s", gdk_atom_name(target));
        mdata->fmanager->drag_dropped = TRUE;

        if (target == gdk_atom_intern_static_string(XDND_DIRECT_SAVE0_NAME)
            && !handle_xdnd_direct_save_drop(mdata, context, x, y))
        {
            return FALSE;
        } else {
            gtk_drag_get_data(icon_view, context, target, time_);
            return TRUE;
        }
    }
}

static void
xfdesktop_file_icon_manager_drag_leave(GtkWidget *icon_view,
                                       GdkDragContext *context,
                                       guint time_,
                                       MonitorData *mdata)
{
    mdata->fmanager->drag_dropped = FALSE;
    mdata->fmanager->drag_data_received = FALSE;
    mdata->fmanager->icon_on_drop_dest = NULL;

    g_list_free_full(mdata->fmanager->dragged_files, g_object_unref);
    mdata->fmanager->dragged_files = NULL;

    g_clear_object(&mdata->fmanager->xdnd_direct_save_destination);

    xfdesktop_icon_view_unset_highlight(XFDESKTOP_ICON_VIEW(icon_view));
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
static GdkDragAction G_GNUC_UNUSED
xfdesktop_file_icon_manager_drag_drop_ask(XfdesktopFileIconManager *fmanager,
                                          GtkWidget *parent,
                                          GdkDragAction allowed_actions,
                                          guint time_)
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
    gtk_menu_attach_to_widget(GTK_MENU(menu), parent, NULL);

    /* This adds the Copy, Move, & Link options */
    GdkDragAction selected_action = 0;
    for(guint menu_item = 0; menu_item < G_N_ELEMENTS(actions); menu_item++) {
        if ((actions[menu_item].drag_action & allowed_actions) != 0) {
            GIcon *icon = g_themed_icon_new(actions[menu_item].icon_name);
            GtkWidget *item = add_menu_item(menu,
                                            _(actions[menu_item].name),
                                            icon,
                                            G_CALLBACK(xfdesktop_dnd_item),
                                            &selected_action);
            g_object_set_data(G_OBJECT(item), "action", GUINT_TO_POINTER(actions[menu_item].drag_action));
        }
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
            g_list_free_full(selected_items, (GDestroyNotify)gtk_tree_path_free);

            if (i > 0) {
                gtk_selection_data_set_uris(data, uris);
            }

            g_strfreev(uris);
        }
    }
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
                                       XfdesktopIconView *source_icon_view,
                                       GtkTreeIter *source_iter,
                                       gint dest_row,
                                       gint dest_col,
                                       MonitorData *mdata)
{
    TRACE("entering, (%d, %d)", dest_row, dest_col);

    MonitorData *source_mdata = source_icon_view == icon_view
        ? mdata
        : monitor_data_for_icon_view(mdata->fmanager->monitor_data, source_icon_view);
    g_return_if_fail(source_mdata != NULL);

    XfdesktopFileIcon *icon = xfdesktop_file_icon_model_filter_get_icon(source_mdata->filter, source_iter);
    if (G_LIKELY(icon != NULL)) {
        update_icon_position(mdata, icon, dest_row, dest_col);
    }
}

static void
xfdesktop_file_icon_manager_activate_selected(MonitorData *mdata) {
    XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
    GList *selected_icons = xfdesktop_file_icon_manager_get_selected_icons(mdata->fmanager, icon_view);

    for (GList *l = selected_icons; l != NULL; l = l->next) {
        XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(l->data);
        xfdesktop_icon_activate(XFDESKTOP_ICON(icon), toplevel_window_for_monitor_data(mdata));
    }

    g_list_free(selected_icons);
}


/* public api */


XfdesktopIconViewManager *
xfdesktop_file_icon_manager_new(XfwScreen *screen,
                                GdkScreen *gdkscreen,
                                XfconfChannel *channel,
                                GtkAccelGroup *accel_group,
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
                        "accel-group", accel_group,
                        "backdrop-manager", backdrop_manager,
                        "desktops", desktops,
                        "folder", folder,
                        NULL);
}
