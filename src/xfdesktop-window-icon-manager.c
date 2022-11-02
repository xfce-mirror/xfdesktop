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
#include "xfdesktop-window-icon-manager.h"
#include "xfdesktop-window-icon-model.h"
#include "xfdesktop-common.h"

static void xfdesktop_window_icon_manager_constructed(GObject *object);
static void xfdesktop_window_icon_manager_dispose(GObject *obj);
static void xfdesktop_window_icon_manager_finalize(GObject *obj);

static gboolean xfdesktop_window_icon_manager_query_icon_tooltip(XfdesktopIconView *icon_view,
                                                                 GtkTreeIter *iter,
                                                                 gint x,
                                                                 gint y,
                                                                 gboolean keyboard_tooltip,
                                                                 GtkTooltip *tooltip,
                                                                 XfdesktopWindowIconManager *wmanager);
static void xfdesktop_window_icon_manager_icon_selection_changed_cb(XfdesktopIconView *icon_view,
                                                                    gpointer user_data);
static void xfdesktop_window_icon_manager_icon_activated(XfdesktopIconView *icon_view,
                                                         XfdesktopWindowIconManager *wmanager);
static void xfdesktop_window_icon_manager_icon_moved(XfdesktopIconView *icon_view,
                                                     GtkTreeIter *iter,
                                                     gint row,
                                                     gint col,
                                                     XfdesktopWindowIconManager *wmanager);
static GdkDragAction xfdesktop_window_icon_manager_drop_propose_action(XfdesktopIconView *icon_view,
                                                                       GdkDragContext *context,
                                                                       GtkTreeIter *iter,
                                                                       GtkSelectionData *data,
                                                                       guint info,
                                                                       XfdesktopWindowIconManager *wmanager);

static void xfdesktop_window_icon_manager_populate_workspaces(XfdesktopWindowIconManager *wmanager);

static void xfdesktop_window_icon_manager_populate_context_menu(XfdesktopIconViewManager *manager,
                                                                GtkMenuShell *menu);
static void xfdesktop_window_icon_manager_sort_icons(XfdesktopIconViewManager *manager,
                                                     GtkSortType sort_type);

static void workspace_group_created_cb(XfwWorkspaceManager *manager,
                                       XfwWorkspaceGroup *group,
                                       gpointer user_data);
static void workspace_group_destroyed_cb(XfwWorkspaceManager *manager,
                                         XfwWorkspaceGroup *group,
                                         gpointer user_data);
static void workspace_created_cb(XfwWorkspaceGroup *group,
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

    gint nworkspaces;
    gint active_ws_num;
    XfdesktopWindowIconWorkspace *icon_workspaces;
};


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

    manager_class->populate_context_menu = xfdesktop_window_icon_manager_populate_context_menu;
    manager_class->sort_icons = xfdesktop_window_icon_manager_sort_icons;
}

static void
xfdesktop_window_icon_manager_init(XfdesktopWindowIconManager *wmanager)
{
    wmanager->priv = xfdesktop_window_icon_manager_get_instance_private(wmanager);
}

static void
xfdesktop_window_icon_manager_constructed(GObject *object)
{
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(object);
    GList *workspace_groups;
    GtkWidget *parent;

    DBG("entering");

    G_OBJECT_CLASS(xfdesktop_window_icon_manager_parent_class)->constructed(object);

    parent = xfdesktop_icon_view_manager_get_parent(XFDESKTOP_ICON_VIEW_MANAGER(wmanager));
    wmanager->priv->icon_view = g_object_new(XFDESKTOP_TYPE_ICON_VIEW,
                                             "channel", xfdesktop_icon_view_manager_get_channel(XFDESKTOP_ICON_VIEW_MANAGER(wmanager)),
                                             "pixbuf-column", XFDESKTOP_WINDOW_ICON_MODEL_COLUMN_SURFACE,
                                             "text-column", XFDESKTOP_WINDOW_ICON_MODEL_COLUMN_LABEL,
                                             "search-column", XFDESKTOP_WINDOW_ICON_MODEL_COLUMN_LABEL,
                                             "row-column", XFDESKTOP_WINDOW_ICON_MODEL_COLUMN_ROW,
                                             "col-column", XFDESKTOP_WINDOW_ICON_MODEL_COLUMN_COL,
                                             NULL);
    xfdesktop_icon_view_set_selection_mode(XFDESKTOP_ICON_VIEW(wmanager->priv->icon_view),
                                           GTK_SELECTION_SINGLE);
    gtk_widget_show(wmanager->priv->icon_view);
    gtk_container_add(GTK_CONTAINER(parent), wmanager->priv->icon_view);

    g_signal_connect(wmanager->priv->icon_view, "query-icon-tooltip",
                     G_CALLBACK(xfdesktop_window_icon_manager_query_icon_tooltip), wmanager);
    g_signal_connect(G_OBJECT(wmanager->priv->icon_view), "icon-selection-changed",
                     G_CALLBACK(xfdesktop_window_icon_manager_icon_selection_changed_cb),
                     wmanager);
    g_signal_connect(wmanager->priv->icon_view, "icon-activated",
                     G_CALLBACK(xfdesktop_window_icon_manager_icon_activated), wmanager);
    g_signal_connect(wmanager->priv->icon_view, "icon-moved",
                     G_CALLBACK(xfdesktop_window_icon_manager_icon_moved), wmanager);
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

    wmanager->priv->nworkspaces = 0;
    wmanager->priv->active_ws_num = -1;
    workspace_groups = xfw_workspace_manager_list_workspace_groups(wmanager->priv->workspace_manager);
    for (GList *l = workspace_groups; l != NULL; l = l->next) {
        XfwWorkspaceGroup *group = XFW_WORKSPACE_GROUP(l->data);
        DBG("got a workspace group");
        workspace_group_created_cb(wmanager->priv->workspace_manager, group, wmanager);
        wmanager->priv->nworkspaces += xfw_workspace_group_get_workspace_count(group);
    }

    DBG("initial workspaces: %d", wmanager->priv->nworkspaces);
    if (wmanager->priv->nworkspaces > 0) {
        wmanager->priv->icon_workspaces = g_malloc0(wmanager->priv->nworkspaces
                                                    * sizeof(XfdesktopWindowIconWorkspace));
        for (gint i = 0; i < wmanager->priv->nworkspaces; ++i) {
            wmanager->priv->icon_workspaces[i].model = xfdesktop_window_icon_model_new();
            g_object_bind_property(wmanager->priv->icon_view, "icon-size",
                                   wmanager->priv->icon_workspaces[i].model, "icon-size",
                                   G_BINDING_SYNC_CREATE);
            g_object_bind_property(wmanager->priv->icon_view, "scale-factor",
                                   wmanager->priv->icon_workspaces[i].model, "scale-factor",
                                   G_BINDING_SYNC_CREATE);
        }

        for (GList *l = workspace_groups; l != NULL; l = l->next) {
            workspace_changed_cb(XFW_WORKSPACE_GROUP(l->data), NULL, wmanager);
            if (wmanager->priv->active_ws_num >= 0) {
                break;
            }
        }
    }

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
    gint i;

    TRACE("entering");

    g_signal_handlers_disconnect_by_func(G_OBJECT(wmanager->priv->workspace_manager),
                                         G_CALLBACK(workspace_group_created_cb), wmanager);
    g_signal_handlers_disconnect_by_func(G_OBJECT(wmanager->priv->workspace_manager),
                                         G_CALLBACK(workspace_group_destroyed_cb), wmanager);
    for (GList *l = xfw_workspace_manager_list_workspace_groups(wmanager->priv->workspace_manager);
         l != NULL;
         l = l->next)
    {
        workspace_group_destroyed_cb(wmanager->priv->workspace_manager, XFW_WORKSPACE_GROUP(l->data), wmanager);
    }

    g_signal_handlers_disconnect_by_func(G_OBJECT(wmanager->priv->xfw_screen),
                                         G_CALLBACK(window_created_cb),
                                         wmanager);

    for (GList *l = xfw_screen_get_windows(wmanager->priv->xfw_screen); l; l = l->next) {
        g_signal_handlers_disconnect_by_func(G_OBJECT(l->data),
                                             G_CALLBACK(window_attr_changed_cb),
                                             wmanager);
        g_signal_handlers_disconnect_by_func(G_OBJECT(l->data),
                                             G_CALLBACK(window_state_changed_cb),
                                             wmanager);
        g_signal_handlers_disconnect_by_func(G_OBJECT(l->data),
                                             G_CALLBACK(window_workspace_changed_cb),
                                             wmanager);
        g_signal_handlers_disconnect_by_func(G_OBJECT(l->data),
                                             G_CALLBACK(window_closed_cb),
                                             wmanager);
    }

    g_object_unref(wmanager->priv->xfw_screen);

    for(i = 0; i < wmanager->priv->nworkspaces; ++i) {
        g_object_unref(wmanager->priv->icon_workspaces[i].model);
    }
    g_free(wmanager->priv->icon_workspaces);

    G_OBJECT_CLASS(xfdesktop_window_icon_manager_parent_class)->finalize(obj);
}

static XfwWindow *
xfdesktop_window_icon_manager_get_nth_window(XfdesktopWindowIconManager *wmanager,
                                             gint n)
{
    XfdesktopWindowIconModel *model = wmanager->priv->icon_workspaces[wmanager->priv->active_ws_num].model;
    GtkTreeIter iter;

    if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(model), &iter, NULL, n)) {
        return xfdesktop_window_icon_model_get_window(model, &iter);
    } else {
        return NULL;
    }
}

static void
xfdesktop_window_icon_manager_icon_selection_changed_cb(XfdesktopIconView *icon_view,
                                                        gpointer user_data)
{
    XfdesktopWindowIconManager *wmanager = user_data;
    GList *selected;
    gboolean set_selection = FALSE;

    DBG("entering");

    selected = xfdesktop_icon_view_get_selected_items(icon_view);
    if (selected != NULL) {
        GtkTreePath *path = (GtkTreePath *)selected->data;
        XfwWindow *window = xfdesktop_window_icon_manager_get_nth_window(wmanager,
                                                                         gtk_tree_path_get_indices(path)[0]);

        if (window != NULL) {
            wmanager->priv->icon_workspaces[wmanager->priv->active_ws_num].selected_icon = window;
            set_selection = TRUE;
        }

        g_list_free(selected);
    }

    if (!set_selection) {
        wmanager->priv->icon_workspaces[wmanager->priv->active_ws_num].selected_icon = NULL;
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
    xfdesktop_window_icon_model_set_position(wmanager->priv->icon_workspaces[wmanager->priv->active_ws_num].model,
                                             iter, row, col);
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
xfdesktop_window_icon_manager_add_icon(XfdesktopWindowIconManager *wmanager,
                                       XfwWindow *window,
                                       gint ws_num)
{
    XfdesktopWindowIconWorkspace *xwiw = &wmanager->priv->icon_workspaces[ws_num];

    DBG("entering");

    xfdesktop_window_icon_model_append(xwiw->model, window, NULL);
}

static void
workspace_changed_cb(XfwWorkspaceGroup *group,
                     XfwWorkspace *previously_active_space,
                     gpointer user_data)
{
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(user_data);
    gint ws_num, tot_ws;
    XfwWorkspace *ws;

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
        gint old_n = -1, total;
        if (xfdesktop_workspace_get_number_and_total(wmanager->priv->workspace_manager, previously_active_space, &old_n, &total)) {
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
    }

    if (xfdesktop_workspace_get_number_and_total(wmanager->priv->workspace_manager, ws, &ws_num, &tot_ws)) {
        XfdesktopWindowIconModel *model = wmanager->priv->icon_workspaces[ws_num].model;

        wmanager->priv->active_ws_num = ws_num;
        DBG("setting active ws num to %d; has %d minimized windows", ws_num, gtk_tree_model_iter_n_children(GTK_TREE_MODEL(wmanager->priv->icon_workspaces[ws_num].model), NULL));
        xfdesktop_icon_view_set_model(XFDESKTOP_ICON_VIEW(wmanager->priv->icon_view),
                                      GTK_TREE_MODEL(model));

        if (wmanager->priv->icon_workspaces[ws_num].selected_icon != NULL) {
            GtkTreeIter iter;
            if (xfdesktop_window_icon_model_get_window_iter(model,
                                                            wmanager->priv->icon_workspaces[ws_num].selected_icon,
                                                            &iter))
            {
                xfdesktop_icon_view_select_item(XFDESKTOP_ICON_VIEW(wmanager->priv->icon_view), &iter);
            }
        }
    }
}

static void
workspace_created_cb(XfwWorkspaceGroup *group,
                     XfwWorkspace *workspace,
                     gpointer user_data)
{
    XfdesktopWindowIconManager *wmanager = user_data;
    gint ws_num, n_ws;

    DBG("entering");

    if (xfdesktop_workspace_get_number_and_total(wmanager->priv->workspace_manager, workspace, &ws_num, &n_ws)) {
        wmanager->priv->nworkspaces = n_ws;

        wmanager->priv->icon_workspaces = g_realloc(wmanager->priv->icon_workspaces,
                                                    sizeof(XfdesktopWindowIconWorkspace) * n_ws);

        if(ws_num != n_ws - 1) {
            memmove(wmanager->priv->icon_workspaces + ws_num + 1,
                    wmanager->priv->icon_workspaces + ws_num,
                    sizeof(XfdesktopWindowIconWorkspace) * (n_ws - ws_num - 1));
        }

        memset(&wmanager->priv->icon_workspaces[ws_num], 0, sizeof(XfdesktopWindowIconWorkspace));
        wmanager->priv->icon_workspaces[ws_num].model = xfdesktop_window_icon_model_new();
        g_object_bind_property(wmanager->priv->icon_view, "icon-size",
                               wmanager->priv->icon_workspaces[ws_num].model, "icon-size",
                               G_BINDING_SYNC_CREATE);
        g_object_bind_property(wmanager->priv->icon_view, "scale-factor",
                               wmanager->priv->icon_workspaces[ws_num].model, "scale-factor",
                               G_BINDING_SYNC_CREATE);
    }
}

static void
workspace_destroyed_cb(XfwWorkspaceGroup *group,
                       XfwWorkspace *workspace,
                       gpointer user_data)
{
    /* TODO: check if we get workspace-destroyed before or after all the
     * windows on that workspace were moved and we got workspace-changed
     * for each one.  preferably that is the case. */
    XfdesktopWindowIconManager *wmanager = user_data;
    gint ws_num, n_ws;

    // FIXME: i don't think this works properly
    if (xfdesktop_workspace_get_number_and_total(wmanager->priv->workspace_manager, workspace, &ws_num, &n_ws)) {
        wmanager->priv->nworkspaces = n_ws;

        g_object_unref(wmanager->priv->icon_workspaces[ws_num].model);

        if(ws_num != n_ws) {
            memmove(wmanager->priv->icon_workspaces + ws_num,
                    wmanager->priv->icon_workspaces + ws_num + 1,
                    sizeof(XfdesktopWindowIconWorkspace) * (n_ws - ws_num));
        }

        wmanager->priv->icon_workspaces = g_realloc(wmanager->priv->icon_workspaces,
                                                    sizeof(XfdesktopWindowIconWorkspace) * n_ws);
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
    g_signal_connect(G_OBJECT(group), "workspace-created",
                     G_CALLBACK(workspace_created_cb), wmanager);
    g_signal_connect(G_OBJECT(group), "workspace-destroyed",
                     G_CALLBACK(workspace_destroyed_cb), wmanager);
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
    if (xfw_window_is_minimized(window) && !xfw_window_is_skip_tasklist(window)) {
        XfdesktopWindowIconModel *model = wmanager->priv->icon_workspaces[wmanager->priv->active_ws_num].model;

        if (xfdesktop_window_icon_model_get_window_iter(model, window, NULL)) {
            xfdesktop_window_icon_model_changed(model, window);
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
    gint ws_num = -1, n_ws, i, max_i;
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
    if (ws) {
        xfdesktop_workspace_get_number_and_total(wmanager->priv->workspace_manager, ws, &ws_num, &n_ws);
        //DBG("got window's workspace, number might be %d", ws_num);
    }

    is_add = (minimized_changed && is_minimized) || (is_minimized && skip_tasklist_changed && !is_skip_tasklist);
    //DBG("is_add == %s", is_add?"TRUE":"FALSE");

    /* this is a cute way of handling adding/removing from *all* workspaces
     * when we're dealing with a sticky windows, and just adding/removing
     * from a single workspace otherwise, without duplicating code */
    if (xfw_window_is_pinned(window)) {
        i = 0;
        max_i = wmanager->priv->nworkspaces;
    } else {
        g_return_if_fail(ws_num >= 0);
        i = ws_num;
        max_i = i + 1;
    }

    if(is_add) {
        for(; i < max_i; i++) {
            XF_DEBUG("loop: %d", i);
            if (!xfdesktop_window_icon_model_get_window_iter(wmanager->priv->icon_workspaces[i].model, window, NULL)) {
                DBG("adding to WS %d", i);
                xfdesktop_window_icon_manager_add_icon(wmanager, window, i);
            }
        }
    } else {
        for(; i < max_i; i++) {
            XfdesktopWindowIconWorkspace *xwiw = &wmanager->priv->icon_workspaces[i];

            if (xwiw->selected_icon == window) {
                xwiw->selected_icon = NULL;
            }
            xfdesktop_window_icon_model_remove(xwiw->model, window);
        }
    }
}

static void
window_workspace_changed_cb(XfwWindow *window,
                            gpointer user_data)
{
    XfdesktopWindowIconManager *wmanager = user_data;
    XfwWorkspace *new_ws;
    gint i, new_ws_num = -1, n_ws;

    TRACE("entering");

    if(!xfw_window_is_minimized(window))
        return;

    new_ws = xfw_window_get_workspace(window);
    if (new_ws) {
        xfdesktop_workspace_get_number_and_total(wmanager->priv->workspace_manager, new_ws, &new_ws_num, &n_ws);
    }
    n_ws = wmanager->priv->nworkspaces;

    for(i = 0; i < n_ws; i++) {
        XfdesktopWindowIconWorkspace *xwiw = &wmanager->priv->icon_workspaces[i];
        gboolean found_window = xfdesktop_window_icon_model_get_window_iter(xwiw->model, window, NULL);

        if (new_ws != NULL) {
            if (i != new_ws_num && found_window) {
                if (xwiw->selected_icon == window) {
                    xwiw->selected_icon = NULL;
                }
                xfdesktop_window_icon_model_remove(xwiw->model, window);
            } else if (i == new_ws_num && !found_window) {
                xfdesktop_window_icon_manager_add_icon(wmanager, window, i);
            }
        } else {
            /* window is sticky */
            if (!found_window) {
                xfdesktop_window_icon_manager_add_icon(wmanager, window, i);
            }
        }
    }
}

static void
window_closed_cb(XfwWindow *window,
                 gpointer user_data)
{
    XfdesktopWindowIconManager *wmanager = user_data;
    gint i;

    g_signal_handlers_disconnect_by_func(G_OBJECT(window),
                                         G_CALLBACK(window_attr_changed_cb),
                                         wmanager);
    g_signal_handlers_disconnect_by_func(G_OBJECT(window),
                                         G_CALLBACK(window_state_changed_cb),
                                         wmanager);
    g_signal_handlers_disconnect_by_func(G_OBJECT(window),
                                         G_CALLBACK(window_workspace_changed_cb),
                                         wmanager);
    g_signal_handlers_disconnect_by_func(G_OBJECT(window),
                                         G_CALLBACK(window_closed_cb),
                                         wmanager);

    for(i = 0; i < wmanager->priv->nworkspaces; i++) {
        XfdesktopWindowIconWorkspace *xwiw = &wmanager->priv->icon_workspaces[i];

        if (xwiw->selected_icon == window) {
            xwiw->selected_icon = NULL;
        }
        xfdesktop_window_icon_model_remove(xwiw->model, window);
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

static gboolean
xfdesktop_window_icon_manager_query_icon_tooltip(XfdesktopIconView *icon_view,
                                                 GtkTreeIter *iter,
                                                 gint x,
                                                 gint y,
                                                 gboolean keyboard_tooltip,
                                                 GtkTooltip *tooltip,
                                                 XfdesktopWindowIconManager *wmanager)
{
    XfwWindow *window = xfdesktop_window_icon_model_get_window(wmanager->priv->icon_workspaces[wmanager->priv->active_ws_num].model,
                                                               iter);

    if (window != NULL) {
        GtkWidget *box, *label;
        guint tooltip_size;
        const gchar *tip_text;
        gchar *padded_tip_text;

        box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

        tooltip_size = xfdesktop_icon_view_manager_get_tooltip_icon_size(XFDESKTOP_ICON_VIEW_MANAGER(wmanager),
                                                                         GTK_WIDGET(icon_view));
        if (tooltip_size > 0) {
            gint scale_factor = gtk_widget_get_scale_factor(GTK_WIDGET(icon_view));
            GdkPixbuf *tip_pix = xfw_window_get_icon(window, tooltip_size * scale_factor);
            cairo_surface_t *surface = gdk_cairo_surface_create_from_pixbuf(tip_pix, scale_factor, gtk_widget_get_window(GTK_WIDGET(icon_view)));
            GtkWidget *img = gtk_image_new_from_surface(surface);
            cairo_surface_destroy(surface);
            gtk_box_pack_start(GTK_BOX(box), img, FALSE, FALSE, 0);
        }

        tip_text = xfw_window_get_name(window);
        padded_tip_text = g_strdup_printf("%s\t", tip_text);
        label = gtk_label_new(padded_tip_text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_label_set_yalign(GTK_LABEL(label), 0.5);
        gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);

        gtk_widget_show_all(box);
        gtk_tooltip_set_custom(tooltip, box);

        g_free(padded_tip_text);

        return TRUE;
    } else {
        return FALSE;
    }
}

static void
xfdesktop_window_icon_manager_populate_workspaces(XfdesktopWindowIconManager *wmanager)
{
    GHashTable *workspaces;
    GList *windows;

    workspaces = g_hash_table_new(g_direct_hash, g_direct_equal);

    windows = xfw_screen_get_windows(wmanager->priv->xfw_screen);
    for (GList *l = windows; l != NULL; l = l->next) {
        XfwWindow *window = XFW_WINDOW(l->data);

        if (xfw_window_is_minimized(window) && !xfw_window_is_skip_tasklist(window)) {
            XfwWorkspace *workspace = xfw_window_get_workspace(window);

            if (workspace != NULL) {
                gboolean found = FALSE;
                gint ws_num = -1, total;

                if (g_hash_table_contains(workspaces, workspace)) {
                    ws_num = GPOINTER_TO_INT(g_hash_table_lookup(workspaces, workspace));
                    found = ws_num >= 0;
                } else if (xfdesktop_workspace_get_number_and_total(wmanager->priv->workspace_manager, workspace, &ws_num, &total)) {
                    g_hash_table_insert(workspaces, workspace, GINT_TO_POINTER(ws_num));
                    found = TRUE;
                }

                if (found) {
                    xfdesktop_window_icon_manager_add_icon(wmanager, window, ws_num);
                }
            } else if (xfw_window_is_pinned(window)) {
                for (gint i = 0; i < wmanager->priv->nworkspaces; ++i) {
                    xfdesktop_window_icon_manager_add_icon(wmanager, window, i);
                }
            }
        }
    }

    g_hash_table_destroy(workspaces);
}

static void
xfdesktop_window_icon_manager_populate_context_menu(XfdesktopIconViewManager *manager,
                                                    GtkMenuShell *menu)
{
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(manager);
    XfdesktopWindowIconWorkspace *wiws = &wmanager->priv->icon_workspaces[wmanager->priv->active_ws_num];

    if (wiws->selected_icon != NULL) {
        GtkWidget *amenu = xfw_window_action_menu_new(wiws->selected_icon);
        GtkWidget *mi, *img;

        img = gtk_image_new_from_icon_name("", GTK_ICON_SIZE_MENU);
        mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("_Window Actions"), img);
        gtk_menu_item_set_submenu (GTK_MENU_ITEM(mi), amenu);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        gtk_widget_show(mi);
    }
}

static void
xfdesktop_window_icon_manager_sort_icons(XfdesktopIconViewManager *manager,
                                         GtkSortType sort_type)
{
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(manager);

    xfdesktop_icon_view_sort_icons(XFDESKTOP_ICON_VIEW(wmanager->priv->icon_view), sort_type);
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
