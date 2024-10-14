/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2024 Brian Tarricone, <brian@tarricone.org>
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

#include <glib.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4windowingui/libxfce4windowingui.h>

#include "common/xfdesktop-common.h"
#include "common/xfdesktop-keyboard-shortcuts.h"
#include "xfce-desktop.h"
#include "xfdesktop-backdrop-manager.h"
#include "xfdesktop-icon-view-holder.h"
#include "xfdesktop-icon-view-manager.h"
#include "xfdesktop-icon-view-model.h"
#include "xfdesktop-icon-view.h"
#include "xfdesktop-window-icon-manager.h"
#include "xfdesktop-window-icon-model.h"

#define TARGET_TEXT 1000
#define WINDOW_MONITOR_OVERRIDE_KEY "xfdesktop-window-monitor-override"

struct _XfdesktopWindowIconManager {
    XfdesktopIconViewManager parent;

    XfdesktopWindowIconModel *model;
    GHashTable *monitor_data;  // XfwMonitor -> MonitorData

    GtkTargetList *source_targets;
    GtkTargetList *dest_targets;
};

typedef struct {
    XfdesktopWindowIconManager *wmanager;
    XfdesktopIconViewHolder *holder;
    XfwWorkspaceGroup *group;
    GHashTable *selected_icons;  // XfwWorkspace -> XfwWindow
} MonitorData;

static void
monitor_data_free(MonitorData *mdata) {
    g_hash_table_destroy(mdata->selected_icons);
    g_object_unref(mdata->holder);
    g_free(mdata);
}

static void xfdesktop_window_icon_manager_constructed(GObject *object);
static void xfdesktop_window_icon_manager_finalize(GObject *object);

static void xfdesktop_window_icon_manager_desktop_added(XfdesktopIconViewManager *manager,
                                                        XfceDesktop *desktop);
static void xfdesktop_window_icon_manager_desktop_removed(XfdesktopIconViewManager *manager,
                                                          XfceDesktop *desktop);
static XfceDesktop *xfdesktop_window_icon_manager_get_focused_desktop(XfdesktopIconViewManager *manager);
static GtkMenu *xfdesktop_window_icon_manager_get_context_menu(XfdesktopIconViewManager *manager,
                                                               XfceDesktop *desktop,
                                                               gint popup_x,
                                                               gint popup_y);
static void xfdesktop_window_icon_manager_activate_icons(XfdesktopIconViewManager *manager);
static void xfdesktop_window_icon_manager_toggle_cursor_icon(XfdesktopIconViewManager *manager);
static void xfdesktop_window_icon_manager_unselect_all_icons(XfdesktopIconViewManager *manager);
static void xfdesktop_window_icon_manager_sort_icons(XfdesktopIconViewManager *manager,
                                                     GtkSortType sort_type,
                                                     XfdesktopIconViewManagerSortFlags flags);

static XfwWindow *window_for_filter_path(XfdesktopWindowIconManager *wmanager,
                                         MonitorData *mdata,
                                         GtkTreePath *filt_path);

static void create_icon_view(XfdesktopWindowIconManager *wmanager,
                             XfceDesktop *desktop);

static void refresh_workspace_group_monitors(XfdesktopWindowIconManager *wmanager,
                                             XfwWorkspaceGroup *group);

static void screen_window_closed(XfwScreen *screen,
                                 XfwWindow *window,
                                 XfdesktopWindowIconManager *wmanager);
static void workspace_group_created(XfwWorkspaceGroup *group,
                                    XfdesktopWindowIconManager *wmanager);
static void workspace_group_destroyed(XfwWorkspaceGroup *group,
                                      XfdesktopWindowIconManager *wmanager);
static void workspace_destroyed(XfwWorkspace *workspace,
                                XfdesktopWindowIconManager *wmanager);

static void icon_view_icon_activated(XfdesktopIconView *icon_view,
                                     MonitorData *mdata);

static void window_icon_manager_action_fixup(XfceGtkActionEntry *entry);
static void accel_map_changed(XfdesktopWindowIconManager *wmanager);

G_DEFINE_TYPE(XfdesktopWindowIconManager, xfdesktop_window_icon_manager, XFDESKTOP_TYPE_ICON_VIEW_MANAGER)


static void
xfdesktop_window_icon_manager_class_init(XfdesktopWindowIconManagerClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->constructed = xfdesktop_window_icon_manager_constructed;
    gobject_class->finalize = xfdesktop_window_icon_manager_finalize;

    XfdesktopIconViewManagerClass *ivm_class = XFDESKTOP_ICON_VIEW_MANAGER_CLASS(klass);
    ivm_class->desktop_added = xfdesktop_window_icon_manager_desktop_added;
    ivm_class->desktop_removed = xfdesktop_window_icon_manager_desktop_removed;
    ivm_class->get_focused_desktop = xfdesktop_window_icon_manager_get_focused_desktop;
    ivm_class->get_context_menu = xfdesktop_window_icon_manager_get_context_menu;
    ivm_class->activate_icons = xfdesktop_window_icon_manager_activate_icons;
    ivm_class->toggle_cursor_icon = xfdesktop_window_icon_manager_toggle_cursor_icon;
    ivm_class->unselect_all_icons = xfdesktop_window_icon_manager_unselect_all_icons;
    ivm_class->sort_icons = xfdesktop_window_icon_manager_sort_icons;
}

static void
xfdesktop_window_icon_manager_init(XfdesktopWindowIconManager *wmanager) {
    wmanager->monitor_data = g_hash_table_new_full(g_direct_hash, g_direct_equal, g_object_unref, (GDestroyNotify)monitor_data_free);
}

static void
xfdesktop_window_icon_manager_constructed(GObject *object) {
    G_OBJECT_CLASS(xfdesktop_window_icon_manager_parent_class)->constructed(object);

    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(object);
    XfdesktopIconViewManager *manager = XFDESKTOP_ICON_VIEW_MANAGER(wmanager);

    accel_map_changed(wmanager);
    g_signal_connect_data(gtk_accel_map_get(),
                          "changed",
                          G_CALLBACK(accel_map_changed),
                          wmanager,
                          NULL,
                          G_CONNECT_SWAPPED | G_CONNECT_AFTER);

    wmanager->source_targets = gtk_target_list_new(NULL, 0);
    gtk_target_list_add_text_targets(wmanager->source_targets, TARGET_TEXT);

    XfwScreen *screen = xfdesktop_icon_view_manager_get_screen(manager);
    g_signal_connect(screen, "window-closed",
                     G_CALLBACK(screen_window_closed), wmanager);

    wmanager->model = xfdesktop_window_icon_model_new(screen);

    GList *desktops = xfdesktop_icon_view_manager_get_desktops(manager);
    for (GList *l = desktops; l != NULL; l = l->next) {
        xfdesktop_window_icon_manager_desktop_added(manager, XFCE_DESKTOP(l->data));
    }

    XfwWorkspaceManager *workspace_manager = xfw_screen_get_workspace_manager(screen);
    GList *groups = xfw_workspace_manager_list_workspace_groups(workspace_manager);
    for (GList *l = groups; l != NULL; l = l->next) {
        XfwWorkspaceGroup *group = XFW_WORKSPACE_GROUP(l->data);
        workspace_group_created(group, wmanager);
    }
    g_signal_connect(workspace_manager, "workspace-group-created",
                     G_CALLBACK(workspace_group_created), wmanager);
    g_signal_connect(workspace_manager, "workspace-group-destroyed",
                     G_CALLBACK(workspace_group_destroyed), wmanager);
    g_signal_connect(workspace_manager, "workspace-destroyed",
                     G_CALLBACK(workspace_destroyed), wmanager);
}

static void
xfdesktop_window_icon_manager_finalize(GObject *object) {
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(object);
    XfdesktopIconViewManager *manager = XFDESKTOP_ICON_VIEW_MANAGER(wmanager);

    g_signal_handlers_disconnect_by_func(gtk_accel_map_get(), accel_map_changed, wmanager);

    gsize n_actions;
    XfceGtkActionEntry *actions = xfdesktop_get_window_icon_manager_actions(window_icon_manager_action_fixup, &n_actions);
    GtkAccelGroup *accel_group = xfdesktop_icon_view_manager_get_accel_group(XFDESKTOP_ICON_VIEW_MANAGER(wmanager));
    xfce_gtk_accel_group_disconnect_action_entries(accel_group, actions, n_actions);

    XfwScreen *screen = xfdesktop_icon_view_manager_get_screen(manager);
    g_signal_handlers_disconnect_by_data(screen, wmanager);

    XfwWorkspaceManager *workspace_manager = xfw_screen_get_workspace_manager(screen);
    g_signal_handlers_disconnect_by_data(workspace_manager, wmanager);

    GList *groups = xfw_workspace_manager_list_workspace_groups(workspace_manager);
    for (GList *l = groups; l != NULL; l = l->next) {
        XfwWorkspaceGroup *group = XFW_WORKSPACE_GROUP(l->data);
        g_signal_handlers_disconnect_by_data(group, wmanager);
    }

    g_hash_table_destroy(wmanager->monitor_data);
    g_object_unref(wmanager->model);

    gtk_target_list_unref(wmanager->source_targets);

    G_OBJECT_CLASS(xfdesktop_window_icon_manager_parent_class)->finalize(object);
}

static void
xfdesktop_window_icon_manager_desktop_added(XfdesktopIconViewManager *manager, XfceDesktop *desktop) {
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(manager);
    create_icon_view(wmanager, desktop);
}

static void
xfdesktop_window_icon_manager_desktop_removed(XfdesktopIconViewManager *manager, XfceDesktop *desktop) {
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(manager);
    g_hash_table_remove(wmanager->monitor_data, xfce_desktop_get_monitor(desktop));
}

static XfceDesktop *
xfdesktop_window_icon_manager_get_focused_desktop(XfdesktopIconViewManager *manager) {
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(manager);
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, wmanager->monitor_data);

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

static void
xfce4_appfinder_launch(GtkWidget *mi, const gchar *appfinder_path) {
    gchar *cmdline = g_strconcat(appfinder_path, " --collapsed", NULL);
    xfce_spawn_command_line(gtk_widget_get_screen(mi), cmdline, FALSE, TRUE, FALSE, NULL);
    g_free(cmdline);
}

static void
xfdesktop_settings_launch(GtkWidget *mi, const gchar *xfdesktop_settings_path) {
    xfce_spawn_command_line(gtk_widget_get_screen(mi), xfdesktop_settings_path, FALSE, FALSE, TRUE, NULL);
}

static void
menu_arrange_icons(GtkWidget *mi, MonitorData *mdata) {
    xfdesktop_icon_view_manager_sort_icons(XFDESKTOP_ICON_VIEW_MANAGER(mdata->wmanager),
                                           GTK_SORT_ASCENDING,
                                           XFDESKTOP_ICON_VIEW_MANAGER_SORT_NONE);
}

static void
menu_next_background(GtkWidget *mi, MonitorData *mdata) {
    // FIXME: need to handle spanning in a special way?
    XfceDesktop *desktop = xfdesktop_icon_view_holder_get_desktop(mdata->holder);
    xfce_desktop_cycle_backdrop(desktop);
}

static GtkWidget *
add_menu_item(GtkWidget *menu,
              const gchar *label,
              GIcon *icon,
              GCallback callback,
              gpointer callback_user_data)
{
    GtkWidget *image = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_MENU);
    GtkWidget *mi = xfdesktop_menu_create_menu_item_with_mnemonic(label, image);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(mi, "activate", callback, callback_user_data);
    g_object_unref(icon);
    return mi;
}

static GtkMenu *
build_root_context_menu(MonitorData *mdata) {
    GtkWidget *menu = gtk_menu_new();
    gtk_menu_set_reserve_toggle_size(GTK_MENU(menu), FALSE);

    gchar *appfinder_path = g_find_program_in_path("xfce4-appfinder");
    if (appfinder_path != NULL) {
        GIcon *icon = g_themed_icon_new("org.xfce.appfinder");
        g_themed_icon_append_name(G_THEMED_ICON(icon), "system-run");
        GtkWidget *mi = add_menu_item(menu,
                                      _("_Run Program..."),
                                      icon,
                                      G_CALLBACK(xfce4_appfinder_launch),
                                      appfinder_path);
        g_object_set_data_full(G_OBJECT(mi), "--xfdesktop-menu-callback-data", appfinder_path, g_free);

        GtkWidget *separator = gtk_separator_menu_item_new();
        gtk_widget_show(separator);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
    }

    add_menu_item(menu,
                  _("Arrange Desktop _Icons"),
                  g_themed_icon_new("view-sort-ascending"),
                  G_CALLBACK(menu_arrange_icons),
                  mdata);

    XfwScreen *xfw_screen = xfdesktop_icon_view_manager_get_screen(XFDESKTOP_ICON_VIEW_MANAGER(mdata->wmanager));
    XfwMonitor *monitor = xfce_desktop_get_monitor(xfdesktop_icon_view_holder_get_desktop(mdata->holder));
    XfwWorkspace *workspace = xfdesktop_find_active_workspace_on_monitor(xfw_screen, monitor);
    XfdesktopBackdropManager *backdrop_manager = xfdesktop_icon_view_manager_get_backdrop_manager(XFDESKTOP_ICON_VIEW_MANAGER(mdata->wmanager));
    if (monitor != NULL
        && workspace != NULL
        && xfdesktop_backdrop_manager_can_cycle_backdrop(backdrop_manager, monitor, workspace))
    {
        add_menu_item(menu,
                      _("_Next Background"),
                      g_themed_icon_new("go-next"),
                      G_CALLBACK(menu_next_background),
                      mdata);
    }

    gchar *xfdesktop_settings_path = g_find_program_in_path("xfdesktop-settings");
    if (xfdesktop_settings_path != NULL) {
        GtkWidget *separator = gtk_separator_menu_item_new();
        gtk_widget_show(separator);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);

        GtkWidget *mi = add_menu_item(menu,
                                      _("Desktop _Settings..."),
                                      g_themed_icon_new("preferences-desktop-wallpaper"),
                                      G_CALLBACK(xfdesktop_settings_launch),
                                      xfdesktop_settings_path);
        g_object_set_data_full(G_OBJECT(mi), "--xfdesktop-menu-callback-data", xfdesktop_settings_path, g_free);
    }

    return GTK_MENU(menu);
}

static GtkMenu *
xfdesktop_window_icon_manager_get_context_menu(XfdesktopIconViewManager *manager,
                                               XfceDesktop *desktop,
                                               gint popup_x,
                                               gint popup_y)
{
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(manager);
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, wmanager->monitor_data);

    MonitorData *mdata;
    while (g_hash_table_iter_next(&iter, NULL, (gpointer)&mdata)) {
        XfceDesktop *a_desktop = xfdesktop_icon_view_holder_get_desktop(mdata->holder);
        if (a_desktop == desktop) {
            XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
            GList *selected = xfdesktop_icon_view_get_selected_items(icon_view);
            XfwWindow *window = window_for_filter_path(wmanager, mdata, g_list_nth_data(selected, 0));

            GtkMenu *menu;
            if (window != NULL) {
                menu = GTK_MENU(xfw_window_action_menu_new(window));
            } else {
                menu = build_root_context_menu(mdata);
            }

            g_list_free_full(selected, (GDestroyNotify)gtk_tree_path_free);
            return menu;
        }
    }

    return NULL;
}

static MonitorData *
find_active_monitor_data(XfdesktopWindowIconManager *wmanager) {
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, wmanager->monitor_data);

    MonitorData *mdata;
    while (g_hash_table_iter_next(&iter, NULL, (gpointer)&mdata)) {
        XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
        GtkWindow *toplevel = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(icon_view)));
        if (gtk_window_has_toplevel_focus(toplevel)) {
            return mdata;
        }
    }

    return NULL;
}

static void
xfdesktop_window_icon_manager_activate_icons(XfdesktopIconViewManager *manager) {
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(manager);
    MonitorData *mdata = find_active_monitor_data(wmanager);
    if (mdata != NULL) {
        icon_view_icon_activated(xfdesktop_icon_view_holder_get_icon_view(mdata->holder), mdata);
    }
}

static void
xfdesktop_window_icon_manager_toggle_cursor_icon(XfdesktopIconViewManager *manager) {
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(manager);
    MonitorData *mdata = find_active_monitor_data(wmanager);
    if (mdata != NULL) {
        XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
        xfdesktop_icon_view_toggle_cursor(icon_view);
    }
}

static void
xfdesktop_window_icon_manager_unselect_all_icons(XfdesktopIconViewManager *manager) {
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(manager);
    MonitorData *mdata = find_active_monitor_data(wmanager);
    if (mdata != NULL) {
        xfdesktop_icon_view_unselect_all(xfdesktop_icon_view_holder_get_icon_view(mdata->holder));
    }
}

static void
xfdesktop_window_icon_manager_sort_icons(XfdesktopIconViewManager *manager,
                                         GtkSortType sort_type,
                                         XfdesktopIconViewManagerSortFlags flags)
{
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(manager);
    gboolean sort_all = (flags & XFDESKTOP_ICON_VIEW_MANAGER_SORT_ALL_DESKTOPS) != 0;

    GHashTableIter iter;
    g_hash_table_iter_init(&iter, wmanager->monitor_data);

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

static XfwWindow *
window_for_filter_path(XfdesktopWindowIconManager *wmanager, MonitorData *mdata, GtkTreePath *filt_path) {
    if (filt_path != NULL) {
        XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
        GtkTreeModel *filter = xfdesktop_icon_view_get_model(icon_view);
        GtkTreeIter filt_iter;
        if (gtk_tree_model_get_iter(filter, &filt_iter, filt_path)) {
            GtkTreeIter real_iter;
            gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(filter), &real_iter, &filt_iter);
            return xfdesktop_window_icon_model_get_window(wmanager->model, &real_iter);
        }
    }

    return NULL;
}

static void
icon_view_icon_selection_changed(XfdesktopIconView *icon_view, MonitorData *mdata) {
    DBG("entering");

    XfwWorkspace *active_workspace = mdata->group != NULL ? xfw_workspace_group_get_active_workspace(mdata->group) : NULL;

    if (active_workspace != NULL) {
        GList *selected = xfdesktop_icon_view_get_selected_items(icon_view);
        XfwWindow *window = window_for_filter_path(mdata->wmanager, mdata, selected != NULL ? selected->data : NULL);
        if (window != NULL) {
            g_hash_table_insert(mdata->selected_icons, active_workspace, window);
        } else {
            g_hash_table_remove(mdata->selected_icons, active_workspace);
        }
        g_list_free_full(selected, (GDestroyNotify)gtk_tree_path_free);
    }
}

static void
icon_view_icon_activated(XfdesktopIconView *icon_view, MonitorData *mdata) {
    GList *selected = xfdesktop_icon_view_get_selected_items(icon_view);
    XfwWindow *window = window_for_filter_path(mdata->wmanager, mdata, selected != NULL ? selected->data : NULL);
    if (window != NULL) {
        XfwSeat *seat = NULL;
        GdkDevice *device = gtk_get_current_event_device();
        if (device != NULL) {
            XfwScreen *screen = xfdesktop_icon_view_manager_get_screen(XFDESKTOP_ICON_VIEW_MANAGER(mdata->wmanager));
            GdkSeat *gdk_seat = gdk_device_get_seat(device);
            seat = xfdesktop_find_xfw_seat_for_gdk_seat(screen, gdk_seat);
        }
        xfw_window_activate(window, seat, gtk_get_current_event_time(), NULL);
    }
    g_list_free_full(selected, (GDestroyNotify)gtk_tree_path_free);
}

static void
window_clear_monitor_override(gpointer data, GObject *where_the_object_was) {
    XfwWindow *window = XFW_WINDOW(data);
    g_object_set_data(G_OBJECT(window), WINDOW_MONITOR_OVERRIDE_KEY, NULL);
}

static void
icon_view_icon_moved(XfdesktopIconView *icon_view,
                     XfdesktopIconView *source_icon_view,
                     GtkTreeIter *source_iter,
                     gint row,
                     gint col,
                     MonitorData *mdata)
{
    TRACE("entering");

    GtkTreeModel *filter = xfdesktop_icon_view_get_model(icon_view);
    GtkTreeIter real_iter;
    gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(filter), &real_iter, source_iter);

    XfwWindow *window = xfdesktop_window_icon_model_get_window(mdata->wmanager->model, &real_iter);
    if (window != NULL && icon_view != source_icon_view) {
        XfwMonitor *monitor = xfce_desktop_get_monitor(xfdesktop_icon_view_holder_get_desktop(mdata->holder));
        gboolean has_override = g_object_get_data(G_OBJECT(window), WINDOW_MONITOR_OVERRIDE_KEY) != NULL;

        if (has_override) {
            g_object_weak_unref(G_OBJECT(monitor), window_clear_monitor_override, window);
        }

        if (g_list_find(xfw_window_get_monitors(window), monitor) != NULL) {
            g_object_set_data(G_OBJECT(window), WINDOW_MONITOR_OVERRIDE_KEY, NULL);
        } else {
            g_object_set_data(G_OBJECT(window), WINDOW_MONITOR_OVERRIDE_KEY, monitor);
            g_object_weak_ref(G_OBJECT(monitor), window_clear_monitor_override, window);
        }
    }

    xfdesktop_window_icon_model_set_position(mdata->wmanager->model, &real_iter, row, col);

    GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(mdata->wmanager->model), &real_iter);
    if (path != NULL) {
        gtk_tree_model_row_changed(GTK_TREE_MODEL(mdata->wmanager->model), path, &real_iter);
        gtk_tree_path_free(path);
    }
}

static void
icon_view_drag_data_get(GtkWidget *icon_view,
                        GdkDragContext *context,
                        GtkSelectionData *data,
                        guint info,
                        guint time_,
                        MonitorData *mdata)
{
    if (info == TARGET_TEXT) {
        GList *selected = xfdesktop_icon_view_get_selected_items(XFDESKTOP_ICON_VIEW(icon_view));
        XfwWindow *window = window_for_filter_path(mdata->wmanager, mdata, selected != NULL ? selected->data : NULL);

        if (window != NULL) {
            const gchar *name = xfw_window_get_name(window);
            if (name != NULL && name[0] != '\0') {
                gtk_selection_data_set_text(data, name, strlen(name));
            }
        }

        g_list_free_full(selected, (GDestroyNotify)gtk_tree_path_free);
    }
}

static gboolean
filter_visible_func(GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
    XfwWindow *window = xfdesktop_window_icon_model_get_window(XFDESKTOP_WINDOW_ICON_MODEL(model), iter);
    g_return_val_if_fail(window != NULL, FALSE);

    MonitorData *mdata = data;
    if (mdata->group != NULL) {
        XfwWorkspace *active_workspace = xfw_workspace_group_get_active_workspace(mdata->group);
        XfwWorkspace *window_workspace = xfw_window_get_workspace(window);

        XfceDesktop *desktop = xfdesktop_icon_view_holder_get_desktop(mdata->holder);
        XfwMonitor *desktop_monitor = xfce_desktop_get_monitor(desktop);
        XfwMonitor *monitor_override = g_object_get_data(G_OBJECT(window), WINDOW_MONITOR_OVERRIDE_KEY);

        gboolean visible = !xfw_window_is_skip_tasklist(window)
            && xfw_window_is_minimized(window)
            && (window_workspace == active_workspace || xfw_window_is_pinned(window))
            && (
                (monitor_override != NULL && monitor_override == desktop_monitor)
                || (monitor_override == NULL && g_list_find(xfw_window_get_monitors(window), desktop_monitor) != NULL)
            );
        TRACE("filtering %s \"%s\"", visible ? "IN" : "OUT", xfw_window_get_name(window));
        return visible;
    } else {
        return FALSE;
    }
}

static XfwWorkspaceGroup *
find_workspace_group_for_monitor(XfdesktopWindowIconManager *wmanager, XfwMonitor *monitor) {
    XfwScreen *screen = xfdesktop_icon_view_manager_get_screen(XFDESKTOP_ICON_VIEW_MANAGER(wmanager));
    XfwWorkspaceManager *workspace_manager = xfw_screen_get_workspace_manager(screen);
    GList *groups = xfw_workspace_manager_list_workspace_groups(workspace_manager);
    for (GList *l = groups; l != NULL; l = l->next) {
        XfwWorkspaceGroup *group = XFW_WORKSPACE_GROUP(l->data);
        if (g_list_find(xfw_workspace_group_get_monitors(group), monitor) != NULL) {
            return group;
        }
    }

    return NULL;
}

static void
create_icon_view(XfdesktopWindowIconManager *wmanager, XfceDesktop *desktop) {
    XfwScreen *screen = xfdesktop_icon_view_manager_get_screen(XFDESKTOP_ICON_VIEW_MANAGER(wmanager));
    XfconfChannel *channel = xfdesktop_icon_view_manager_get_channel(XFDESKTOP_ICON_VIEW_MANAGER(wmanager));

    MonitorData *mdata = g_new0(MonitorData, 1);
    mdata->wmanager = wmanager;
    mdata->selected_icons = g_hash_table_new(g_direct_hash, g_direct_equal);
    mdata->group = find_workspace_group_for_monitor(wmanager, xfce_desktop_get_monitor(desktop));

    XfdesktopIconView *icon_view = g_object_new(XFDESKTOP_TYPE_ICON_VIEW,
                                                "screen", screen,
                                                "channel", channel,
                                                "pixbuf-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_IMAGE,
                                                "icon-opacity-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_IMAGE_OPACITY,
                                                "text-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_LABEL,
                                                "search-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_LABEL,
                                                "tooltip-icon-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_TOOLTIP_IMAGE,
                                                "tooltip-text-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_TOOLTIP_TEXT,
                                                "row-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_ROW,
                                                "col-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_COL,
                                                NULL);
    xfdesktop_icon_view_set_selection_mode(icon_view, GTK_SELECTION_SINGLE);

    gint n_targets = 0;
    GtkTargetEntry *targets = gtk_target_table_new_from_list(wmanager->source_targets, &n_targets);
    xfdesktop_icon_view_enable_drag_source(icon_view,
                                           GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_BUTTON1_MASK,
                                           targets, n_targets,
                                           GDK_ACTION_COPY);
    gtk_target_table_free(targets, n_targets);

    g_signal_connect(G_OBJECT(icon_view), "icon-selection-changed",
                     G_CALLBACK(icon_view_icon_selection_changed), mdata);
    g_signal_connect(icon_view, "icon-activated",
                     G_CALLBACK(icon_view_icon_activated), mdata);
    g_signal_connect(icon_view, "icon-moved",
                     G_CALLBACK(icon_view_icon_moved), mdata);

    // DnD source signals
    g_signal_connect(icon_view, "drag-data-get",
                     G_CALLBACK(icon_view_drag_data_get), mdata);

    GtkAccelGroup *accel_group = xfdesktop_icon_view_manager_get_accel_group(XFDESKTOP_ICON_VIEW_MANAGER(wmanager));
    mdata->holder = xfdesktop_icon_view_holder_new(screen, desktop, icon_view, accel_group);
    if (mdata->group != NULL) {
        refresh_workspace_group_monitors(wmanager, mdata->group);
    }

    GtkTreeModel *filter = gtk_tree_model_filter_new(GTK_TREE_MODEL(wmanager->model), NULL);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(filter), filter_visible_func, mdata, NULL);
    xfdesktop_icon_view_set_model(icon_view, filter);
    g_object_unref(filter);

    g_hash_table_insert(wmanager->monitor_data, g_object_ref(xfce_desktop_get_monitor(desktop)), mdata);
}

static void
refilter_model(XfdesktopWindowIconManager *wmanager, MonitorData *mdata) {
    DBG("refiltering model on monitor %s", xfw_monitor_get_connector(xfce_desktop_get_monitor(xfdesktop_icon_view_holder_get_desktop(mdata->holder))));
    XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
    GtkTreeModel *filter = xfdesktop_icon_view_get_model(icon_view);
    // gtk_tree_model_refilter() is usually what we'd go for, but that can
    // remove and add windows in any order, even interleaved, which can mess up
    // their positions.  Instead, we'll unset the model, refilter, and reset
    // it, which will remove everything, and then re-add only the ones we want.
    g_object_ref(filter);
    xfdesktop_icon_view_set_model(icon_view, NULL);
    gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(filter));
    xfdesktop_icon_view_set_model(icon_view, filter);
    g_object_unref(filter);
}

static void
workspace_group_monitor_added(XfwWorkspaceGroup *group, XfwMonitor *monitor, XfdesktopWindowIconManager *wmanager) {
    MonitorData *mdata = g_hash_table_lookup(wmanager->monitor_data, monitor);
    if (mdata != NULL) {
        mdata->group = group;
        refilter_model(wmanager, mdata);
    }
}

static void
workspace_group_monitor_removed(XfwWorkspaceGroup *group, XfwMonitor *monitor, XfdesktopWindowIconManager *wmanager) {
    MonitorData *mdata = g_hash_table_lookup(wmanager->monitor_data, monitor);
    if (mdata != NULL) {
        mdata->group = NULL;
        refilter_model(wmanager, mdata);
    }
}

static void
workspace_group_active_workspace_changed(XfwWorkspaceGroup *group,
                                         XfwWorkspace *previously_active_workspace,
                                         XfdesktopWindowIconManager *wmanager)
{
    DBG("entering");
    for (GList *l = xfw_workspace_group_get_monitors(group); l != NULL; l = l->next) {
        XfwMonitor *monitor = XFW_MONITOR(l->data);
        DBG("checking monitor %s", xfw_monitor_get_connector(monitor));
        MonitorData *mdata = g_hash_table_lookup(wmanager->monitor_data, monitor);
        if (mdata != NULL) {
            DBG("got mdata");
            refilter_model(wmanager, mdata);

            XfwWorkspace *active_workspace = xfw_workspace_group_get_active_workspace(group);
            XfwWindow *selected_window = g_hash_table_lookup(mdata->selected_icons, active_workspace);
            if (selected_window != NULL) {
                GtkTreeIter real_iter;
                if (xfdesktop_window_icon_model_get_window_iter(wmanager->model, selected_window, &real_iter)) {
                    XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
                    GtkTreeModel *filter = xfdesktop_icon_view_get_model(icon_view);
                    GtkTreeIter filt_iter;
                    if (gtk_tree_model_filter_convert_child_iter_to_iter(GTK_TREE_MODEL_FILTER(filter), &filt_iter, &real_iter)) {
                        xfdesktop_icon_view_select_item(icon_view, &filt_iter);
                    }
                }
            }
        }
    }
}

static void
screen_window_closed(XfwScreen *screen, XfwWindow *window, XfdesktopWindowIconManager *wmanager) {
    XfwMonitor *monitor = g_object_get_data(G_OBJECT(window), WINDOW_MONITOR_OVERRIDE_KEY);
    if (monitor != NULL) {
        g_object_weak_unref(G_OBJECT(monitor), window_clear_monitor_override, window);
        g_object_set_data(G_OBJECT(window), WINDOW_MONITOR_OVERRIDE_KEY, NULL);
    }

    GHashTableIter mdata_iter;
    g_hash_table_iter_init(&mdata_iter, wmanager->monitor_data);

    MonitorData *mdata;
    while (g_hash_table_iter_next(&mdata_iter, NULL, (gpointer)&mdata)) {
        GHashTableIter win_iter;
        g_hash_table_iter_init(&win_iter, mdata->selected_icons);

        XfwWindow *a_window;
        while (g_hash_table_iter_next(&win_iter, NULL, (gpointer)&a_window)) {
            if (a_window == window) {
                g_hash_table_iter_remove(&win_iter);
            }
        }
    }
}

static void
refresh_workspace_group_monitors(XfdesktopWindowIconManager *wmanager, XfwWorkspaceGroup *group) {
    for (GList *l = xfw_workspace_group_get_monitors(group); l != NULL; l = l->next) {
        XfwMonitor *monitor = XFW_MONITOR(l->data);
        workspace_group_monitor_added(group, monitor, wmanager);
    }
}

static void
workspace_group_created(XfwWorkspaceGroup *group, XfdesktopWindowIconManager *wmanager) {
    refresh_workspace_group_monitors(wmanager, group);

    g_signal_connect(group, "monitor-added",
                     G_CALLBACK(workspace_group_monitor_added), wmanager);
    g_signal_connect(group, "monitor-removed",
                     G_CALLBACK(workspace_group_monitor_removed), wmanager);
    g_signal_connect(group, "active-workspace-changed",
                     G_CALLBACK(workspace_group_active_workspace_changed), wmanager);
}

static void
workspace_group_destroyed(XfwWorkspaceGroup *group, XfdesktopWindowIconManager *wmanager) {
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, wmanager->monitor_data);

    MonitorData *mdata;
    while (g_hash_table_iter_next(&iter, NULL, (gpointer)&mdata)) {
        if (mdata->group == group) {
            mdata->group = NULL;
        }
    }
}

static void
workspace_destroyed(XfwWorkspace *workspace, XfdesktopWindowIconManager *wmanager) {
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, wmanager->monitor_data);

    MonitorData *mdata;
    while (g_hash_table_iter_next(&iter, NULL, (gpointer)&mdata)) {
        g_hash_table_remove(mdata->selected_icons, workspace);
    }
}

static void
window_icon_manager_action_unminimize(XfdesktopWindowIconManager *wmanager) {
    MonitorData *mdata = find_active_monitor_data(wmanager);
    if (mdata != NULL) {
        XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
        GList *selected = xfdesktop_icon_view_get_selected_items(icon_view);
        XfwWindow *window = window_for_filter_path(mdata->wmanager, mdata, selected != NULL ? selected->data : NULL);
        if (window != NULL) {
            xfw_window_set_minimized(window, FALSE, NULL);
        }
        g_list_free(selected);
    }
}

static void
window_icon_manager_action_close(XfdesktopWindowIconManager *wmanager) {
    MonitorData *mdata = find_active_monitor_data(wmanager);
    if (mdata != NULL) {
        XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
        GList *selected = xfdesktop_icon_view_get_selected_items(icon_view);
        XfwWindow *window = window_for_filter_path(mdata->wmanager, mdata, selected != NULL ? selected->data : NULL);
        if (window != NULL) {
            xfw_window_close(window, GDK_CURRENT_TIME, NULL);
        }
        g_list_free(selected);
    }
}

static void
window_icon_manager_action_fixup(XfceGtkActionEntry *entry) {
    switch (entry->id) {
        case XFDESKTOP_WINDOW_ICON_MANAGER_ACTION_UNMINIMIZE:
            entry->callback = G_CALLBACK(window_icon_manager_action_unminimize);
            break;
        case XFDESKTOP_WINDOW_ICON_MANAGER_ACTION_CLOSE:
            entry->callback = G_CALLBACK(window_icon_manager_action_close);
            break;
        default:
            g_assert_not_reached();
            break;
    }
}

static void
accel_map_changed(XfdesktopWindowIconManager *wmanager) {
    gsize n_actions;
    XfceGtkActionEntry *actions = xfdesktop_get_window_icon_manager_actions(window_icon_manager_action_fixup, &n_actions);
    GtkAccelGroup *accel_group = xfdesktop_icon_view_manager_get_accel_group(XFDESKTOP_ICON_VIEW_MANAGER(wmanager));

    xfce_gtk_accel_group_disconnect_action_entries(accel_group, actions, n_actions);
    xfce_gtk_accel_group_connect_action_entries(accel_group, actions, n_actions, wmanager);
}

XfdesktopIconViewManager *
xfdesktop_window_icon_manager_new(XfwScreen *screen,
                                  XfconfChannel *channel,
                                  GtkAccelGroup *accel_group,
                                  XfdesktopBackdropManager *backdrop_manager,
                                  GList *desktops)
{
    g_return_val_if_fail(XFW_IS_SCREEN(screen), NULL);
    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel), NULL);
    g_return_val_if_fail(GTK_IS_ACCEL_GROUP(accel_group), NULL);
    g_return_val_if_fail(XFDESKTOP_IS_BACKDROP_MANAGER(backdrop_manager), NULL);

    return g_object_new(XFDESKTOP_TYPE_WINDOW_ICON_MANAGER,
                        "screen", screen,
                        "channel", channel,
                        "backdrop-manager", backdrop_manager,
                        "accel-group", accel_group,
                        "desktops", desktops,
                        NULL);
}
