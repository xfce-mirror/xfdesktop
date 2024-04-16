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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib-object.h>
#include <cairo-gobject.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4windowing/libxfce4windowing.h>
#include <libxfce4windowingui/libxfce4windowingui.h>

#include <libxfce4ui/libxfce4ui.h>

#include "xfdesktop-cell-renderer-icon-label.h"
#include "xfdesktop-icon-view.h"
#include "xfdesktop-icon-view-model.h"
#include "xfdesktop-window-icon-manager.h"
#include "xfdesktop-window-icon-model.h"
#include "xfdesktop-common.h"

#define TARGET_TEXT 1000

typedef struct
{
    XfdesktopWindowIconModel *model;
    XfwWindow *selected_icon;
} XfdesktopWindowIconWorkspace;

struct _XfdesktopWindowIconManagerPrivate
{
    XfwScreen *xfw_screen;
    XfwWorkspaceManager *workspace_manager;

    GtkWidget *icon_view;

    GHashTable *icon_workspaces;  // XfwWorkspace -> XfdesktopWindowIconWorkspace
    XfdesktopWindowIconWorkspace *active_icon_workspace;
};

static void xfdesktop_window_icon_manager_constructed(GObject *object);
static void xfdesktop_window_icon_manager_dispose(GObject *obj);
static void xfdesktop_window_icon_manager_finalize(GObject *obj);

static void xfdesktop_window_icon_manager_icon_selection_changed_cb(XfdesktopIconView *icon_view,
                                                                    gpointer user_data);
static void xfdesktop_window_icon_manager_icon_activated(XfdesktopIconView *icon_view,
                                                         XfdesktopWindowIconManager *wmanager);
static void xfdesktop_window_icon_manager_icon_moved(XfdesktopIconView *icon_view,
                                                     GtkTreeIter *iter,
                                                     gint row,
                                                     gint col,
                                                     XfdesktopWindowIconManager *wmanager);

static GdkDragAction xfdesktop_window_icon_manager_drag_actions_get(XfdesktopIconView *icon_view,
                                                                    GtkTreeIter *iter,
                                                                    XfdesktopWindowIconManager *wmanager);
static void xfdesktop_window_icon_manager_drag_data_get(GtkWidget *icon_view,
                                                        GdkDragContext *context,
                                                        GtkSelectionData *data,
                                                        guint info,
                                                        guint time_,
                                                        XfdesktopWindowIconManager *wmanager);
static GdkDragAction xfdesktop_window_icon_manager_drop_propose_action(XfdesktopIconView *icon_view,
                                                                       GdkDragContext *context,
                                                                       GtkTreeIter *iter,
                                                                       GtkSelectionData *data,
                                                                       guint info,
                                                                       XfdesktopWindowIconManager *wmanager);

static void xfdesktop_window_icon_manager_workarea_changed(XfdesktopWindowIconManager *wmanager);

static void xfdesktop_window_icon_manager_populate_workspaces(XfdesktopWindowIconManager *wmanager);

static GtkMenu *xfdesktop_window_icon_manager_get_context_menu(XfdesktopIconViewManager *manager);
static void xfdesktop_window_icon_manager_sort_icons(XfdesktopIconViewManager *manager,
                                                     GtkSortType sort_type);
static void xfdesktop_window_icon_manager_update_workarea(XfdesktopIconViewManager *manager);

static void window_icon_workspace_free(XfdesktopWindowIconWorkspace *icon_workspace);

static void workspace_group_created_cb(XfwWorkspaceManager *manager,
                                       XfwWorkspaceGroup *group,
                                       gpointer user_data);
static void workspace_group_destroyed_cb(XfwWorkspaceManager *manager,
                                         XfwWorkspaceGroup *group,
                                         gpointer user_data);
static void workspace_created_cb(XfwWorkspaceManager *manager,
                                 XfwWorkspace *workspace,
                                 gpointer user_data);
static void workspace_destroyed_cb(XfwWorkspaceManager *manager,
                                   XfwWorkspace *workspace,
                                   gpointer user_data);
static void workspace_changed_cb(XfwWorkspaceGroup *group,
                                 XfwWorkspace *previously_active_space,
                                 gpointer user_data);
static void window_created_cb(XfwScreen *screen,
                              XfwWindow *window,
                              gpointer user_data);
static void window_attr_changed_cb(XfwWindow *window,
                                   XfdesktopWindowIconManager *wmanager);
static void window_state_changed_cb(XfwWindow *window,
                                    XfwWindowState changed_mask,
                                    XfwWindowState new_state,
                                    gpointer user_data);
static void window_workspace_changed_cb(XfwWindow *window,
                                        gpointer user_data);
static void window_closed_cb(XfwWindow *window,
                             gpointer user_data);


G_DEFINE_TYPE_WITH_PRIVATE(XfdesktopWindowIconManager,
                           xfdesktop_window_icon_manager,
                           XFDESKTOP_TYPE_ICON_VIEW_MANAGER)

static void
xfdesktop_window_icon_manager_class_init(XfdesktopWindowIconManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    XfdesktopIconViewManagerClass *manager_class = XFDESKTOP_ICON_VIEW_MANAGER_CLASS(klass);

    gobject_class->constructed = xfdesktop_window_icon_manager_constructed;
    gobject_class->dispose = xfdesktop_window_icon_manager_dispose;
    gobject_class->finalize = xfdesktop_window_icon_manager_finalize;

    manager_class->get_context_menu = xfdesktop_window_icon_manager_get_context_menu;
    manager_class->sort_icons = xfdesktop_window_icon_manager_sort_icons;
    manager_class->update_workarea = xfdesktop_window_icon_manager_update_workarea;
}

static void
xfdesktop_window_icon_manager_init(XfdesktopWindowIconManager *wmanager)
{
    wmanager->priv = xfdesktop_window_icon_manager_get_instance_private(wmanager);
    wmanager->priv->icon_workspaces = g_hash_table_new_full(g_direct_hash,
                                                            g_direct_equal,
                                                            g_object_unref,
                                                            (GDestroyNotify)window_icon_workspace_free);
}

static void
xfdesktop_window_icon_manager_constructed(GObject *object)
{
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(object);
    GtkFixed *container;
    GdkRectangle workarea;
    GtkTargetList *text_target_list;
    GtkTargetEntry *text_targets;
    gint n_text_targets = 0;
    GList *groups;

    DBG("entering");

    G_OBJECT_CLASS(xfdesktop_window_icon_manager_parent_class)->constructed(object);

    text_target_list = gtk_target_list_new(NULL, 0);
    gtk_target_list_add_text_targets(text_target_list, TARGET_TEXT);
    text_targets = gtk_target_table_new_from_list(text_target_list, &n_text_targets);
    gtk_target_list_unref(text_target_list);

    container = xfdesktop_icon_view_manager_get_container(XFDESKTOP_ICON_VIEW_MANAGER(wmanager));
    xfdesktop_icon_view_manager_get_workarea(XFDESKTOP_ICON_VIEW_MANAGER(wmanager), &workarea);

    wmanager->priv->icon_view = g_object_new(XFDESKTOP_TYPE_ICON_VIEW,
                                             "channel", xfdesktop_icon_view_manager_get_channel(XFDESKTOP_ICON_VIEW_MANAGER(wmanager)),
                                             "pixbuf-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_IMAGE,
                                             "text-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_LABEL,
                                             "search-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_LABEL,
                                             "tooltip-icon-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_TOOLTIP_IMAGE,
                                             "tooltip-text-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_TOOLTIP_TEXT,
                                             "row-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_ROW,
                                             "col-column", XFDESKTOP_ICON_VIEW_MODEL_COLUMN_COL,
                                             NULL);
    xfdesktop_icon_view_set_selection_mode(XFDESKTOP_ICON_VIEW(wmanager->priv->icon_view),
                                           GTK_SELECTION_SINGLE);
    xfdesktop_icon_view_enable_drag_source(XFDESKTOP_ICON_VIEW(wmanager->priv->icon_view),
                                           GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_BUTTON1_MASK,
                                           text_targets, n_text_targets,
                                           GDK_ACTION_COPY);
    gtk_target_table_free(text_targets, n_text_targets);
    if (workarea.width > 0 && workarea.height > 0) {
        gtk_widget_set_size_request(GTK_WIDGET(wmanager->priv->icon_view), workarea.width, workarea.height);
    }
    gtk_fixed_put(container, GTK_WIDGET(wmanager->priv->icon_view), workarea.x, workarea.y);
    gtk_widget_show(GTK_WIDGET(wmanager->priv->icon_view));

    g_signal_connect(wmanager, "notify::workarea",
                     G_CALLBACK(xfdesktop_window_icon_manager_workarea_changed), NULL);
    xfdesktop_window_icon_manager_workarea_changed(wmanager);

    g_signal_connect(G_OBJECT(wmanager->priv->icon_view), "icon-selection-changed",
                     G_CALLBACK(xfdesktop_window_icon_manager_icon_selection_changed_cb),
                     wmanager);
    g_signal_connect(wmanager->priv->icon_view, "icon-activated",
                     G_CALLBACK(xfdesktop_window_icon_manager_icon_activated), wmanager);
    g_signal_connect(wmanager->priv->icon_view, "icon-moved",
                     G_CALLBACK(xfdesktop_window_icon_manager_icon_moved), wmanager);
    g_signal_connect(wmanager->priv->icon_view, "drag-actions-get",
                     G_CALLBACK(xfdesktop_window_icon_manager_drag_actions_get), wmanager);
    g_signal_connect(wmanager->priv->icon_view, "drag-data-get",
                     G_CALLBACK(xfdesktop_window_icon_manager_drag_data_get), wmanager);
    g_signal_connect(wmanager->priv->icon_view, "drop-propose-action",
                     G_CALLBACK(xfdesktop_window_icon_manager_drop_propose_action), wmanager);

    wmanager->priv->xfw_screen = xfw_screen_get_default();
    g_signal_connect(G_OBJECT(wmanager->priv->xfw_screen), "window-opened",
                     G_CALLBACK(window_created_cb), wmanager);

    wmanager->priv->workspace_manager = xfw_screen_get_workspace_manager(wmanager->priv->xfw_screen);

    g_signal_connect(G_OBJECT(wmanager->priv->workspace_manager), "workspace-group-created",
                     G_CALLBACK(workspace_group_created_cb), wmanager);
    g_signal_connect(G_OBJECT(wmanager->priv->workspace_manager), "workspace-group-destroyed",
                     G_CALLBACK(workspace_group_destroyed_cb), wmanager);

    for (GList *l = xfw_workspace_manager_list_workspace_groups(wmanager->priv->workspace_manager);
         l != NULL;
         l = l->next)
    {
        workspace_group_created_cb(wmanager->priv->workspace_manager, XFW_WORKSPACE_GROUP(l->data), wmanager);
    }

    g_signal_connect(G_OBJECT(wmanager->priv->workspace_manager), "workspace-created",
                     G_CALLBACK(workspace_created_cb), wmanager);
    g_signal_connect(G_OBJECT(wmanager->priv->workspace_manager), "workspace-destroyed",
                     G_CALLBACK(workspace_destroyed_cb), wmanager);
    for (GList *l = xfw_workspace_manager_list_workspaces(wmanager->priv->workspace_manager);
         l != NULL;
         l = l->next)
    {
        workspace_created_cb(wmanager->priv->workspace_manager, XFW_WORKSPACE(l->data), wmanager);
    }

    groups = g_list_reverse(g_list_copy(xfw_workspace_manager_list_workspace_groups(wmanager->priv->workspace_manager)));
    for (GList *l = groups; l != NULL; l = l->next) {
        workspace_changed_cb(XFW_WORKSPACE_GROUP(l->data), NULL, wmanager);
    }
    g_list_free(groups);

    xfdesktop_window_icon_manager_populate_workspaces(wmanager);
}

static void
xfdesktop_window_icon_manager_dispose(GObject *obj)
{
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(obj);

    if (wmanager->priv->icon_view != NULL) {
        gtk_widget_destroy(wmanager->priv->icon_view);
        wmanager->priv->icon_view = NULL;
    }

    G_OBJECT_CLASS(xfdesktop_window_icon_manager_parent_class)->dispose(obj);
}

static void
xfdesktop_window_icon_manager_finalize(GObject *obj)
{
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(obj);

    TRACE("entering");

    g_signal_handlers_disconnect_by_data(G_OBJECT(wmanager->priv->workspace_manager), wmanager);

    for (GList *l = xfw_workspace_manager_list_workspaces(wmanager->priv->workspace_manager);
         l != NULL;
         l = l->next)
    {
        workspace_destroyed_cb(wmanager->priv->workspace_manager, XFW_WORKSPACE(l->data), wmanager);
    }

    for (GList *l = xfw_workspace_manager_list_workspace_groups(wmanager->priv->workspace_manager);
         l != NULL;
         l = l->next)
    {
        workspace_group_destroyed_cb(wmanager->priv->workspace_manager, XFW_WORKSPACE_GROUP(l->data), wmanager);
    }

    g_signal_handlers_disconnect_by_data(G_OBJECT(wmanager->priv->xfw_screen), wmanager);

    for (GList *l = xfw_screen_get_windows(wmanager->priv->xfw_screen); l; l = l->next) {
        g_signal_handlers_disconnect_by_data(G_OBJECT(l->data), wmanager);
    }

    g_object_unref(wmanager->priv->xfw_screen);

    g_hash_table_destroy(wmanager->priv->icon_workspaces);

    G_OBJECT_CLASS(xfdesktop_window_icon_manager_parent_class)->finalize(obj);
}

static XfwWindow *
xfdesktop_window_icon_manager_get_nth_window(XfdesktopWindowIconManager *wmanager,
                                             gint n)
{
    XfdesktopWindowIconWorkspace *icon_workspace = wmanager->priv->active_icon_workspace;
    if (icon_workspace != NULL) {
        GtkTreeIter iter;
        if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(icon_workspace->model), &iter, NULL, n)) {
            return xfdesktop_window_icon_model_get_window(icon_workspace->model, &iter);
        }
    }

    return NULL;
}

static void
xfdesktop_window_icon_manager_icon_selection_changed_cb(XfdesktopIconView *icon_view,
                                                        gpointer user_data)
{
    XfdesktopWindowIconManager *wmanager = user_data;

    DBG("entering");

    XfdesktopWindowIconWorkspace *icon_workspace = wmanager->priv->active_icon_workspace;
    if (icon_workspace != NULL) {
        gboolean set_selection = FALSE;
        GList *selected = xfdesktop_icon_view_get_selected_items(icon_view);

        if (selected != NULL) {
            GtkTreePath *path = (GtkTreePath *)selected->data;
            XfwWindow *window = xfdesktop_window_icon_manager_get_nth_window(wmanager,
                                                                             gtk_tree_path_get_indices(path)[0]);

            if (window != NULL) {
                icon_workspace->selected_icon = window;
                set_selection = TRUE;
            }

            g_list_free(selected);
        }

        if (!set_selection) {
            icon_workspace->selected_icon = NULL;
        }
    }
}

static void
xfdesktop_window_icon_manager_icon_activated(XfdesktopIconView *icon_view,
                                             XfdesktopWindowIconManager *wmanager)
{
    GList *selected = xfdesktop_icon_view_get_selected_items(icon_view);

    if (selected != NULL) {
        GtkTreePath *path = (GtkTreePath *)selected->data;
        XfwWindow *window = xfdesktop_window_icon_manager_get_nth_window(wmanager,
                                                                         gtk_tree_path_get_indices(path)[0]);

        if (window != NULL) {
            xfw_window_activate(window, gtk_get_current_event_time(), NULL);
        }

        g_list_free(selected);
    }
}

static void
xfdesktop_window_icon_manager_icon_moved(XfdesktopIconView *icon_view,
                                         GtkTreeIter *iter,
                                         gint row,
                                         gint col,
                                         XfdesktopWindowIconManager *wmanager)
{
    XfdesktopWindowIconWorkspace *icon_workspace = wmanager->priv->active_icon_workspace;
    if (icon_workspace != NULL) {
        xfdesktop_window_icon_model_set_position(icon_workspace->model, iter, row, col);
    }
}

static GdkDragAction
xfdesktop_window_icon_manager_drag_actions_get(XfdesktopIconView *icon_view,
                                             GtkTreeIter *iter,
                                             XfdesktopWindowIconManager *wmanager)
{
    XfdesktopWindowIconWorkspace *icon_workspace = wmanager->priv->active_icon_workspace;
    if (icon_workspace != NULL) {
        XfwWindow *window = xfdesktop_window_icon_model_get_window(icon_workspace->model, iter);

        if (window != NULL) {
            return GDK_ACTION_COPY;
        }
    }

    return 0;
}

static void
xfdesktop_window_icon_manager_drag_data_get(GtkWidget *icon_view,
                                            GdkDragContext *context,
                                            GtkSelectionData *data,
                                            guint info,
                                            guint time_,
                                            XfdesktopWindowIconManager *wmanager)
{
    XfdesktopWindowIconWorkspace *icon_workspace = wmanager->priv->active_icon_workspace;
    if (icon_workspace != NULL && info == TARGET_TEXT) {
        GList *selected_items = xfdesktop_icon_view_get_selected_items(XFDESKTOP_ICON_VIEW(icon_view));

        if (selected_items != NULL) {
            GtkTreePath *path = (GtkTreePath *)selected_items->data;
            GtkTreeIter iter;

            if (gtk_tree_model_get_iter(GTK_TREE_MODEL(icon_workspace->model), &iter, path)) {
                XfwWindow *window = xfdesktop_window_icon_model_get_window(icon_workspace->model, &iter);

                if (window != NULL) {
                    const gchar *name = xfw_window_get_name(window);

                    if (name != NULL && name[0] != '\0') {
                        gtk_selection_data_set_text(data, name, strlen(name));
                    }
                }
            }

            g_list_free(selected_items);
        }
    }
}

static GdkDragAction
xfdesktop_window_icon_manager_drop_propose_action(XfdesktopIconView *icon_view,
                                                  GdkDragContext *context,
                                                  GtkTreeIter *iter,
                                                  GtkSelectionData *data,
                                                  guint info,
                                                  XfdesktopWindowIconManager *wmanager)
{
    if (iter == NULL) {
        return GDK_ACTION_MOVE;
    } else {
        return 0;
    }
}

static void
window_icon_workspace_free(XfdesktopWindowIconWorkspace *icon_workspace) {
    g_object_unref(icon_workspace->model);
    g_free(icon_workspace);
}


static void
workspace_changed_cb(XfwWorkspaceGroup *group,
                     XfwWorkspace *previously_active_space,
                     gpointer user_data)
{
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(user_data);
    XfwWorkspace *ws;
    XfdesktopWindowIconWorkspace *icon_workspace;

    DBG("entering");

    // TODO: handle multiple workspace groups somehow
    ws = xfw_workspace_group_get_active_workspace(group);
    if(!XFW_IS_WORKSPACE(ws)) {
        DBG("got weird failure of xfw_workspace_group_get_active_workspace(), bailing");
        return;
    }

    if (ws == previously_active_space) {
        return;
    }

    if (previously_active_space != NULL) {
        // Block so we don't clear out the selected_icon pointer on the old workspace
        // when its icon gets removed.
        g_signal_handlers_block_by_func(wmanager->priv->icon_view,
                                        G_CALLBACK(xfdesktop_window_icon_manager_icon_selection_changed_cb),
                                        wmanager);
        xfdesktop_icon_view_set_model(XFDESKTOP_ICON_VIEW(wmanager->priv->icon_view), NULL);
        g_signal_handlers_unblock_by_func(wmanager->priv->icon_view,
                                          G_CALLBACK(xfdesktop_window_icon_manager_icon_selection_changed_cb),
                                          wmanager);
    }

    icon_workspace = g_hash_table_lookup(wmanager->priv->icon_workspaces, ws);
    wmanager->priv->active_icon_workspace = icon_workspace;
    if (icon_workspace != NULL) {
        DBG("setting active ws num to %d; has %d minimized windows",
            xfw_workspace_get_number(ws),
            gtk_tree_model_iter_n_children(GTK_TREE_MODEL(icon_workspace->model), NULL));

        xfdesktop_icon_view_set_model(XFDESKTOP_ICON_VIEW(wmanager->priv->icon_view),
                                      GTK_TREE_MODEL(icon_workspace->model));

        if (icon_workspace->selected_icon != NULL) {
            GtkTreeIter iter;
            if (xfdesktop_window_icon_model_get_window_iter(icon_workspace->model,
                                                            icon_workspace->selected_icon,
                                                            &iter))
            {
                xfdesktop_icon_view_select_item(XFDESKTOP_ICON_VIEW(wmanager->priv->icon_view), &iter);
            }
        }
    }
}

static void
workspace_created_cb(XfwWorkspaceManager *manager,
                     XfwWorkspace *workspace,
                     gpointer user_data)
{
    XfdesktopWindowIconManager *wmanager = user_data;
    XfdesktopWindowIconWorkspace *icon_workspace;

    DBG("entering");

    icon_workspace = g_new0(XfdesktopWindowIconWorkspace, 1);
    icon_workspace->model = xfdesktop_window_icon_model_new();
    g_hash_table_insert(wmanager->priv->icon_workspaces, g_object_ref(workspace), icon_workspace);
}

static void
workspace_destroyed_cb(XfwWorkspaceManager *manager,
                       XfwWorkspace *workspace,
                       gpointer user_data)
{
    /* TODO: check if we get workspace-destroyed before or after all the
     * windows on that workspace were moved and we got workspace-changed
     * for each one.  preferably that is the case. */
    XfdesktopWindowIconManager *wmanager = user_data;
    XfdesktopWindowIconWorkspace *icon_workspace;

    icon_workspace = g_hash_table_lookup(wmanager->priv->icon_workspaces, workspace);
    if (icon_workspace != NULL) {
        if (wmanager->priv->active_icon_workspace == icon_workspace) {
            wmanager->priv->active_icon_workspace = NULL;
            if (wmanager->priv->icon_view != NULL) {
                gtk_icon_view_set_model(GTK_ICON_VIEW(wmanager->priv->icon_view), NULL);
            }
        }

        g_hash_table_remove(wmanager->priv->icon_workspaces, workspace);
    }
}

static void
workspace_group_created_cb(XfwWorkspaceManager *workspace_manager,
                           XfwWorkspaceGroup *group,
                           gpointer user_data)
{
    XfdesktopWindowIconManager *wmanager = user_data;

    DBG("entering");

    g_signal_connect(G_OBJECT(group), "active-workspace-changed",
                     G_CALLBACK(workspace_changed_cb), wmanager);
}

static void
workspace_group_destroyed_cb(XfwWorkspaceManager *workspace_manager,
                           XfwWorkspaceGroup *group,
                           gpointer user_data)
{
    XfdesktopWindowIconManager *wmanager = user_data;

    g_signal_handlers_disconnect_by_func(G_OBJECT(group),
                                         G_CALLBACK(workspace_changed_cb),
                                         wmanager);
    g_signal_handlers_disconnect_by_func(G_OBJECT(group),
                                         G_CALLBACK(workspace_created_cb),
                                         wmanager);
    g_signal_handlers_disconnect_by_func(G_OBJECT(group),
                                         G_CALLBACK(workspace_destroyed_cb),
                                         wmanager);
}

static void
window_attr_changed_cb(XfwWindow *window,
                       XfdesktopWindowIconManager *wmanager)
{
    XfdesktopWindowIconWorkspace *icon_workspace = wmanager->priv->active_icon_workspace;
    if (icon_workspace != NULL && xfw_window_is_minimized(window) && !xfw_window_is_skip_tasklist(window)) {
        if (xfdesktop_window_icon_model_get_window_iter(icon_workspace->model, window, NULL)) {
            xfdesktop_window_icon_model_changed(icon_workspace->model, window);
        }
    }
}

static void
window_state_changed_cb(XfwWindow *window,
                        XfwWindowState changed_mask,
                        XfwWindowState new_state,
                        gpointer user_data)
{
    XfdesktopWindowIconManager *wmanager = user_data;
    XfwWorkspace *ws;
    XfdesktopWindowIconWorkspace *icon_workspace;
    gboolean is_minimized, minimized_changed;
    gboolean is_skip_tasklist, skip_tasklist_changed;
    gboolean is_add = FALSE;

    DBG("entering");

    minimized_changed = (changed_mask & XFW_WINDOW_STATE_MINIMIZED) != 0;
    skip_tasklist_changed = (changed_mask & XFW_WINDOW_STATE_SKIP_TASKLIST) != 0;
    is_minimized = (new_state & XFW_WINDOW_STATE_MINIMIZED) != 0;
    is_skip_tasklist = (new_state & XFW_WINDOW_STATE_SKIP_TASKLIST) != 0;

    if (!minimized_changed && !skip_tasklist_changed) {
        return;
    }

    XF_DEBUG("changed_mask indicates an action");

    ws = xfw_window_get_workspace(window);
    icon_workspace = g_hash_table_lookup(wmanager->priv->icon_workspaces, ws);

    is_add = (minimized_changed && is_minimized) || (is_minimized && skip_tasklist_changed && !is_skip_tasklist);
    //DBG("is_add == %s", is_add?"TRUE":"FALSE");

    if (is_add) {
        if (xfw_window_is_pinned(window)) {
            GHashTableIter iter;
            g_hash_table_iter_init(&iter, wmanager->priv->icon_workspaces);
            while (g_hash_table_iter_next(&iter, NULL, (gpointer)&icon_workspace)) {
                xfdesktop_window_icon_model_append(icon_workspace->model, window, NULL);
            }
        } else if (icon_workspace) {
            xfdesktop_window_icon_model_append(icon_workspace->model, window, NULL);
        }
    } else {
        if (xfw_window_is_pinned(window)) {
            GHashTableIter iter;
            g_hash_table_iter_init(&iter, wmanager->priv->icon_workspaces);
            while (g_hash_table_iter_next(&iter, NULL, (gpointer)&icon_workspace)) {
                if (icon_workspace->selected_icon == window) {
                    icon_workspace->selected_icon = NULL;
                }
                xfdesktop_window_icon_model_remove(icon_workspace->model, window);
            }
        } else if (icon_workspace) {
            if (icon_workspace->selected_icon == window) {
                icon_workspace->selected_icon = NULL;
            }
            xfdesktop_window_icon_model_remove(icon_workspace->model, window);
        }
    }
}

static void
window_workspace_changed_cb(XfwWindow *window,
                            gpointer user_data)
{
    XfdesktopWindowIconManager *wmanager = user_data;
    XfwWorkspace *new_ws;
    XfdesktopWindowIconWorkspace *new_icon_workspace, *icon_workspace;
    GHashTableIter iter;

    TRACE("entering");

    if(!xfw_window_is_minimized(window))
        return;

    new_ws = xfw_window_get_workspace(window);
    new_icon_workspace = g_hash_table_lookup(wmanager->priv->icon_workspaces, new_ws);

    g_hash_table_iter_init(&iter, wmanager->priv->icon_workspaces);
    while (g_hash_table_iter_next(&iter, NULL, (gpointer)&icon_workspace)) {
        gboolean found_window = xfdesktop_window_icon_model_get_window_iter(icon_workspace->model, window, NULL);

        if (new_icon_workspace != NULL) {
            if (new_icon_workspace != icon_workspace && found_window) {
                if (icon_workspace->selected_icon == window) {
                    icon_workspace->selected_icon = NULL;
                }
                xfdesktop_window_icon_model_remove(icon_workspace->model, window);
            } else if (new_icon_workspace == icon_workspace && !found_window) {
                xfdesktop_window_icon_model_append(icon_workspace->model, window, NULL);
            }
        } else {
            /* window is sticky */
            if (!found_window) {
                xfdesktop_window_icon_model_append(icon_workspace->model, window, NULL);
            }
        }
    }
}

static void
window_closed_cb(XfwWindow *window,
                 gpointer user_data)
{
    XfdesktopWindowIconManager *wmanager = user_data;
    GHashTableIter iter;
    XfdesktopWindowIconWorkspace *icon_workspace;

    g_signal_handlers_disconnect_by_data(G_OBJECT(window), wmanager);

    g_hash_table_iter_init(&iter, wmanager->priv->icon_workspaces);
    while (g_hash_table_iter_next(&iter, NULL, (gpointer)&icon_workspace)) {
        if (icon_workspace->selected_icon == window) {
            icon_workspace->selected_icon = NULL;
        }
        xfdesktop_window_icon_model_remove(icon_workspace->model, window);
    }
}

static void
window_created_cb(XfwScreen *xfw_screen,
                  XfwWindow *window,
                  gpointer user_data)
{
    XfdesktopWindowIconManager *wmanager = user_data;

    DBG("entering");

    g_signal_connect(G_OBJECT(window), "name-changed",
                     G_CALLBACK(window_attr_changed_cb), wmanager);
    g_signal_connect(G_OBJECT(window), "icon-changed",
                     G_CALLBACK(window_attr_changed_cb), wmanager);
    g_signal_connect(G_OBJECT(window), "state-changed",
                     G_CALLBACK(window_state_changed_cb), wmanager);
    g_signal_connect(G_OBJECT(window), "workspace-changed",
                     G_CALLBACK(window_workspace_changed_cb), wmanager);
    g_signal_connect(G_OBJECT(window), "closed", 
                     G_CALLBACK(window_closed_cb), wmanager);

    window_state_changed_cb(window,
                            XFW_WINDOW_STATE_MINIMIZED | XFW_WINDOW_STATE_SKIP_TASKLIST,
                            xfw_window_get_state(window),
                            wmanager);
}

static void
xfdesktop_window_icon_manager_workarea_changed(XfdesktopWindowIconManager *wmanager)
{
    GdkRectangle workarea;

    xfdesktop_icon_view_manager_get_workarea(XFDESKTOP_ICON_VIEW_MANAGER(wmanager), &workarea);
    DBG("moving icon view to %dx%d+%d+%d", workarea.width, workarea.height, workarea.x, workarea.y);
    gtk_widget_set_size_request(GTK_WIDGET(wmanager->priv->icon_view), workarea.width, workarea.height);
    gtk_fixed_move(xfdesktop_icon_view_manager_get_container(XFDESKTOP_ICON_VIEW_MANAGER(wmanager)),
                   GTK_WIDGET(wmanager->priv->icon_view),
                   workarea.x,
                   workarea.y);
}

static void
xfdesktop_window_icon_manager_populate_workspaces(XfdesktopWindowIconManager *wmanager)
{
    for (GList *l = xfw_screen_get_windows(wmanager->priv->xfw_screen); l != NULL; l = l->next) {
        XfwWindow *window = XFW_WINDOW(l->data);

        if (xfw_window_is_minimized(window) && !xfw_window_is_skip_tasklist(window)) {
            XfwWorkspace *workspace = xfw_window_get_workspace(window);
            XfdesktopWindowIconWorkspace *icon_workspace = g_hash_table_lookup(wmanager->priv->icon_workspaces, workspace);

            if (icon_workspace != NULL) {
                xfdesktop_window_icon_model_append(icon_workspace->model, window, NULL);
            } else if (xfw_window_is_pinned(window)) {
                GHashTableIter iter;
                g_hash_table_iter_init(&iter, wmanager->priv->icon_workspaces);
                while (g_hash_table_iter_next(&iter, NULL, (gpointer)&icon_workspace)) {
                    xfdesktop_window_icon_model_append(icon_workspace->model, window, NULL);
                }
            }
        }
    }
}

static GtkMenu *
xfdesktop_window_icon_manager_get_context_menu(XfdesktopIconViewManager *manager)
{
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(manager);
    XfdesktopWindowIconWorkspace *icon_workspace = wmanager->priv->active_icon_workspace;

    if (icon_workspace != NULL && icon_workspace->selected_icon != NULL) {
        return GTK_MENU(xfw_window_action_menu_new(icon_workspace->selected_icon));
    } else {
        return NULL;
    }
}

static void
xfdesktop_window_icon_manager_sort_icons(XfdesktopIconViewManager *manager,
                                         GtkSortType sort_type)
{
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(manager);

    xfdesktop_icon_view_sort_icons(XFDESKTOP_ICON_VIEW(wmanager->priv->icon_view), sort_type);
}

static void
xfdesktop_window_icon_manager_update_workarea(XfdesktopIconViewManager *manager) {
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(manager);

    if (wmanager->priv->icon_view != NULL) {
        GdkRectangle workarea;
        xfdesktop_icon_view_manager_get_workarea(manager, &workarea);

        if (workarea.width > 0 && workarea.height > 0) {
            gtk_widget_set_size_request(GTK_WIDGET(wmanager->priv->icon_view), workarea.width, workarea.height);
        }

        GtkFixed *container = xfdesktop_icon_view_manager_get_container(manager);
        gtk_fixed_move(container, GTK_WIDGET(wmanager->priv->icon_view), workarea.x, workarea.y);
    }
}



/* public api */


XfdesktopIconViewManager *
xfdesktop_window_icon_manager_new(XfconfChannel *channel,
                                  GtkWidget *parent)
{
    g_return_val_if_fail(GTK_IS_CONTAINER(parent), NULL);
    return g_object_new(XFDESKTOP_TYPE_WINDOW_ICON_MANAGER,
                        "channel", channel,
                        "parent", parent,
                        NULL);
}
