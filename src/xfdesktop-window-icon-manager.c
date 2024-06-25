/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006 Brian Tarricone, <brian@tarricone.org>
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <libxfce4windowingui/libxfce4windowingui.h>

#include "xfce-desktop.h"
#include "xfdesktop-icon-view-holder.h"
#include "xfdesktop-icon-view-manager.h"
#include "xfdesktop-icon-view-model.h"
#include "xfdesktop-icon-view.h"
#include "xfdesktop-window-icon-manager.h"
#include "xfdesktop-window-icon-model.h"

#define TARGET_TEXT 1000

struct _XfdesktopWindowIconManager {
    XfdesktopIconViewManager parent;

    XfdesktopWindowIconModel *model;
    GHashTable *monitor_data;  // XfwMonitor -> MonitorData
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

static GtkMenu *xfdesktop_window_icon_manager_get_context_menu(XfdesktopIconViewManager *manager,
                                                               GtkWidget *widget);
static void xfdesktop_window_icon_manager_sort_icons(XfdesktopIconViewManager *manager,
                                                     GtkSortType sort_type);

static XfwWindow *window_for_filter_path(XfdesktopWindowIconManager *wmanager,
                                         MonitorData *mdata,
                                         GtkTreePath *filt_path);

static void create_icon_view(XfdesktopWindowIconManager *wmanager,
                             XfceDesktop *desktop);
static void screen_window_closed(XfwScreen *screen,
                                 XfwWindow *window,
                                 XfdesktopWindowIconManager *wmanager);
static void workspace_group_created(XfwWorkspaceGroup *group,
                                    XfdesktopWindowIconManager *wmanager);
static void workspace_group_destroyed(XfwWorkspaceGroup *group,
                                      XfdesktopWindowIconManager *wmanager);
static void workspace_destroyed(XfwWorkspace *workspace,
                                XfdesktopWindowIconManager *wmanager);
static void desktops_changed(XfdesktopWindowIconManager *wmanager);

G_DEFINE_TYPE(XfdesktopWindowIconManager, xfdesktop_window_icon_manager, XFDESKTOP_TYPE_ICON_VIEW_MANAGER)


static void
xfdesktop_window_icon_manager_class_init(XfdesktopWindowIconManagerClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->constructed = xfdesktop_window_icon_manager_constructed;
    gobject_class->finalize = xfdesktop_window_icon_manager_finalize;

    XfdesktopIconViewManagerClass *ivm_class = XFDESKTOP_ICON_VIEW_MANAGER_CLASS(klass);
    ivm_class->get_context_menu = xfdesktop_window_icon_manager_get_context_menu;
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

    XfwScreen *screen = xfdesktop_icon_view_manager_get_screen(manager);
    g_signal_connect(screen, "window-closed",
                     G_CALLBACK(screen_window_closed), wmanager);

    wmanager->model = xfdesktop_window_icon_model_new(screen);

    desktops_changed(wmanager);
    g_signal_connect(wmanager, "notify::desktops",
                     G_CALLBACK(desktops_changed), NULL);

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

    G_OBJECT_CLASS(xfdesktop_window_icon_manager_parent_class)->finalize(object);
}

static GtkMenu *
xfdesktop_window_icon_manager_get_context_menu(XfdesktopIconViewManager *manager, GtkWidget *widget) {
    g_return_val_if_fail(XFCE_IS_DESKTOP(widget), NULL);

    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(manager);
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, wmanager->monitor_data);

    XfceDesktop *desktop = XFCE_DESKTOP(widget);
    MonitorData *mdata;
    while (g_hash_table_iter_next(&iter, NULL, (gpointer)&mdata)) {
        XfceDesktop *a_desktop = xfdesktop_icon_view_holder_get_desktop(mdata->holder);
        if (a_desktop == desktop) {
            XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
            GList *selected = xfdesktop_icon_view_get_selected_items(icon_view);
            XfwWindow *window = window_for_filter_path(wmanager, mdata, g_list_nth_data(selected, 0));
            if (window != NULL) {
                return GTK_MENU(xfw_window_action_menu_new(window));
            }

            g_list_free(selected);
            break;
        }
    }

    return NULL;
}

static void
xfdesktop_window_icon_manager_sort_icons(XfdesktopIconViewManager *manager, GtkSortType sort_type) {
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(manager);
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, wmanager->monitor_data);

    MonitorData *mdata;
    while (g_hash_table_iter_next(&iter, NULL, (gpointer)&mdata)) {
        XfdesktopIconView *icon_view = xfdesktop_icon_view_holder_get_icon_view(mdata->holder);
        xfdesktop_icon_view_sort_icons(icon_view, sort_type);
    }
}

static MonitorData *
monitor_data_for_icon_view(XfdesktopWindowIconManager *wmanager, XfdesktopIconView *icon_view) {
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, wmanager->monitor_data);

    MonitorData *mdata;
    while (g_hash_table_iter_next(&iter, NULL, (gpointer)&mdata)) {
        if (xfdesktop_icon_view_holder_get_icon_view(mdata->holder) == icon_view) {
            return mdata;
        }
    }

    g_assert_not_reached();
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
icon_view_icon_selection_changed(XfdesktopIconView *icon_view, XfdesktopWindowIconManager *wmanager) {
    DBG("entering");

    MonitorData *mdata = monitor_data_for_icon_view(wmanager, icon_view);
    XfwWorkspace *active_workspace = mdata->group != NULL ? xfw_workspace_group_get_active_workspace(mdata->group) : NULL;

    if (active_workspace != NULL) {
        GList *selected = xfdesktop_icon_view_get_selected_items(icon_view);
        XfwWindow *window = window_for_filter_path(wmanager, mdata, selected != NULL ? selected->data : NULL);
        if (window != NULL) {
            g_hash_table_insert(mdata->selected_icons, active_workspace, window);
        } else {
            g_hash_table_remove(mdata->selected_icons, active_workspace);
        }
        g_list_free(selected);
    }
}

static void
icon_view_icon_activated(XfdesktopIconView *icon_view, XfdesktopWindowIconManager *wmanager) {
    MonitorData *mdata = monitor_data_for_icon_view(wmanager, icon_view);
    GList *selected = xfdesktop_icon_view_get_selected_items(icon_view);
    XfwWindow *window = window_for_filter_path(wmanager, mdata, selected != NULL ? selected->data : NULL);
    if (window != NULL) {
        xfw_window_activate(window, gtk_get_current_event_time(), NULL);
    }
    g_list_free(selected);
}

static void
icon_view_icon_moved(XfdesktopIconView *icon_view,
                     GtkTreeIter *filt_iter,
                     gint row,
                     gint col,
                     XfdesktopWindowIconManager *wmanager)
{
    GtkTreeModel *filter = xfdesktop_icon_view_get_model(icon_view);
    GtkTreeIter real_iter;
    gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(filter), &real_iter, filt_iter);
    xfdesktop_window_icon_model_set_position(wmanager->model, &real_iter, row, col);
}

static GdkDragAction
icon_view_drag_actions_get(XfdesktopIconView *icon_view, GtkTreeIter *filt_iter, XfdesktopWindowIconManager *wmanager) {
    GtkTreeModel *filter = xfdesktop_icon_view_get_model(icon_view);
    GtkTreeIter real_iter;
    gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(filter), &real_iter, filt_iter);
    XfwWindow *window = xfdesktop_window_icon_model_get_window(wmanager->model, &real_iter);
    return window != NULL ? GDK_ACTION_COPY : 0;
}

static void
icon_view_drag_data_get(GtkWidget *icon_view,
                        GdkDragContext *context,
                        GtkSelectionData *data,
                        guint info,
                        guint time_,
                        XfdesktopWindowIconManager *wmanager)
{
    if (info == TARGET_TEXT) {
        MonitorData *mdata = monitor_data_for_icon_view(wmanager, XFDESKTOP_ICON_VIEW(icon_view));
        GList *selected = xfdesktop_icon_view_get_selected_items(XFDESKTOP_ICON_VIEW(icon_view));
        XfwWindow *window = window_for_filter_path(wmanager, mdata, selected != NULL ? selected->data : NULL);

        if (window != NULL) {
            const gchar *name = xfw_window_get_name(window);

            if (name != NULL && name[0] != '\0') {
                gtk_selection_data_set_text(data, name, strlen(name));
            }
        }

        g_list_free(selected);
    }
}

static GdkDragAction
icon_view_drop_propose_action(XfdesktopIconView *icon_view,
                              GdkDragContext *context,
                              GtkTreeIter *iter,
                              GtkSelectionData *data,
                              guint info,
                              XfdesktopWindowIconManager *wmanager)
{
    return iter == NULL ? GDK_ACTION_MOVE : 0;
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
        XfwMonitor *monitor = xfce_desktop_get_monitor(desktop);

        gboolean visible = !xfw_window_is_skip_tasklist(window)
            && xfw_window_is_minimized(window)
            && (window_workspace == active_workspace || xfw_window_is_pinned(window))
            && g_list_find(xfw_window_get_monitors(window), monitor) != NULL;
        TRACE("filtering %s \"%s\"", visible ? "IN" : "OUT", xfw_window_get_name(window));
        return visible;
    } else {
        return FALSE;
    }
}

static void
create_icon_view(XfdesktopWindowIconManager *wmanager, XfceDesktop *desktop) {
    XfwScreen *screen = xfdesktop_icon_view_manager_get_screen(XFDESKTOP_ICON_VIEW_MANAGER(wmanager));
    XfconfChannel *channel = xfdesktop_icon_view_manager_get_channel(XFDESKTOP_ICON_VIEW_MANAGER(wmanager));

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

    GtkTargetList *text_target_list = gtk_target_list_new(NULL, 0);
    gtk_target_list_add_text_targets(text_target_list, TARGET_TEXT);

    gint n_text_targets = 0;
    GtkTargetEntry *text_targets = gtk_target_table_new_from_list(text_target_list, &n_text_targets);
    gtk_target_list_unref(text_target_list);

    xfdesktop_icon_view_set_selection_mode(icon_view, GTK_SELECTION_SINGLE);
    xfdesktop_icon_view_enable_drag_source(icon_view,
                                           GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_BUTTON1_MASK,
                                           text_targets, n_text_targets,
                                           GDK_ACTION_COPY);
    gtk_target_table_free(text_targets, n_text_targets);

    g_signal_connect(G_OBJECT(icon_view), "icon-selection-changed",
                     G_CALLBACK(icon_view_icon_selection_changed), wmanager);
    g_signal_connect(icon_view, "icon-activated",
                     G_CALLBACK(icon_view_icon_activated), wmanager);
    g_signal_connect(icon_view, "icon-moved",
                     G_CALLBACK(icon_view_icon_moved), wmanager);
    g_signal_connect(icon_view, "drag-actions-get",
                     G_CALLBACK(icon_view_drag_actions_get), wmanager);
    g_signal_connect(icon_view, "drag-data-get",
                     G_CALLBACK(icon_view_drag_data_get), wmanager);
    g_signal_connect(icon_view, "drop-propose-action",
                     G_CALLBACK(icon_view_drop_propose_action), wmanager);

    MonitorData *mdata = g_new0(MonitorData, 1);
    mdata->wmanager = wmanager;
    mdata->holder = xfdesktop_icon_view_holder_new(screen, desktop, icon_view);
    mdata->selected_icons = g_hash_table_new(g_direct_hash, g_direct_equal);

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
workspace_group_created(XfwWorkspaceGroup *group, XfdesktopWindowIconManager *wmanager) {
    DBG("entering");

    for (GList *l = xfw_workspace_group_get_monitors(group); l != NULL; l = l->next) {
        XfwMonitor *monitor = XFW_MONITOR(l->data);
        workspace_group_monitor_added(group, monitor, wmanager);
    }

    g_signal_connect(group, "monitor-added",
                     G_CALLBACK(workspace_group_monitor_removed), wmanager);
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
desktops_changed(XfdesktopWindowIconManager *wmanager) {
    TRACE("entering");

    XfdesktopIconViewManager *manager = XFDESKTOP_ICON_VIEW_MANAGER(wmanager);
    GList *desktops = g_list_copy(xfdesktop_icon_view_manager_get_desktops(manager));

    GHashTableIter iter;
    g_hash_table_iter_init(&iter, wmanager->monitor_data);

    XfwMonitor *monitor;
    MonitorData *mdata;
    while (g_hash_table_iter_next(&iter, (gpointer)&monitor, (gpointer)&mdata)) {
        XfceDesktop *desktop = xfdesktop_icon_view_holder_get_desktop(mdata->holder);

        GList *ld = g_list_find(desktops, desktop);
        if (ld == NULL) {
            DBG("removing desktop");
            g_hash_table_iter_remove(&iter);
        } else {
            DBG("keeping desktop");
            desktops = g_list_delete_link(desktops, ld);
        }
    }

    for (GList *l = desktops; l != NULL; l = l->next) {
        XfceDesktop *desktop = XFCE_DESKTOP(l->data);
        DBG("adding desktop");
        create_icon_view(wmanager, desktop);
    }
}

XfdesktopIconViewManager *
xfdesktop_window_icon_manager_new(XfwScreen *screen, XfconfChannel *channel, GList *desktops) {
    g_return_val_if_fail(XFW_IS_SCREEN(screen), NULL);
    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel), NULL);

    return g_object_new(XFDESKTOP_TYPE_WINDOW_ICON_MANAGER,
                        "screen", screen,
                        "channel", channel,
                        "desktops", desktops,
                        NULL);
}
