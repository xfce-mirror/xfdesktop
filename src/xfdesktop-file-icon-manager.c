/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006 Brian Tarricone, <bjt23@cornell.edu>
 *  Copyright(c) 2006 Benedikt Meurer, <benny@xfce.org>
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include <glib-object.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "xfdesktop-icon-view.h"
#include "xfdesktop-file-icon.h"
#include "xfdesktop-file-icon-manager.h"

#define SAVE_DELAY 7000

static void xfdesktop_file_icon_manager_set_property(GObject *object,
                                                     guint property_id,
                                                     const GValue *value,
                                                     GParamSpec *pspec);
static void xfdesktop_file_icon_manager_get_property(GObject *object,
                                                     guint property_id,
                                                     GValue *value,
                                                     GParamSpec *pspec);
static void xfdesktop_file_icon_manager_finalize(GObject *obj);
static void xfdesktop_file_icon_manager_icon_view_manager_init(XfdesktopIconViewManagerIface *iface);

static gboolean xfdesktop_file_icon_manager_real_init(XfdesktopIconViewManager *manager,
                                                      XfdesktopIconView *icon_view);
static void xfdesktop_file_icon_manager_fini(XfdesktopIconViewManager *manager);

enum
{
    PROP0 = 0,
    PROP_FOLDER,
};

struct _XfdesktopFileIconManagerPrivate
{
    gboolean inited;
    
    XfdesktopIconView *icon_view;
    
    GdkScreen *gscreen;
    
    ThunarVfsPath *folder;
    ThunarVfsMonitor *monitor;
    ThunarVfsMonitorHandle *handle;
    ThunarVfsJob *list_job;
    
    GHashTable *icons;
    
    GList *icons_to_save;
    guint save_icons_id;
    
    GList *deferred_icons;
    
    GtkTargetList *targets;
};


G_DEFINE_TYPE_EXTENDED(XfdesktopFileIconManager,
                       xfdesktop_file_icon_manager,
                       G_TYPE_OBJECT, 0,
                       G_IMPLEMENT_INTERFACE(XFDESKTOP_TYPE_ICON_VIEW_MANAGER,
                                             xfdesktop_file_icon_manager_icon_view_manager_init))

enum
{
    TARGET_GNOME_COPIED_FILES = 0,
    TARGET_UTF8_STRING,
};

static const GtkTargetEntry targets[] = {
    { "x-special/gnome-copied-files", 0, TARGET_GNOME_COPIED_FILES },
    { "UTF8_STRING", 0, TARGET_UTF8_STRING }
};
static const gint n_targets = 2;

static void
xfdesktop_file_icon_manager_class_init(XfdesktopFileIconManagerClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    
    gobject_class->set_property = xfdesktop_file_icon_manager_set_property;
    gobject_class->get_property = xfdesktop_file_icon_manager_get_property;
    gobject_class->finalize = xfdesktop_file_icon_manager_finalize;
    
    g_object_class_install_property(gobject_class, PROP_FOLDER,
                                    g_param_spec_boxed("folder", "Thunar VFS Folder",
                                                       "Folder this icon manager manages",
                                                       THUNAR_VFS_TYPE_PATH,
                                                       G_PARAM_READWRITE
                                                       | G_PARAM_CONSTRUCT_ONLY));
}

static void
xfdesktop_file_icon_manager_init(XfdesktopFileIconManager *fmanager)
{
    fmanager->priv = g_new0(XfdesktopFileIconManagerPrivate, 1);
    
    /* be safe */
    fmanager->priv->gscreen = gdk_screen_get_default();
    fmanager->priv->targets = gtk_target_list_new(targets, n_targets);
}

static void
xfdesktop_file_icon_manager_set_property(GObject *object,
                                         guint property_id,
                                         const GValue *value,
                                         GParamSpec *pspec)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(object);
    
    switch(property_id) {
        case PROP_FOLDER:
            fmanager->priv->folder = thunar_vfs_path_ref((ThunarVfsPath *)g_value_get_boxed(value));
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
        case PROP_FOLDER:
            g_value_set_boxed(value, fmanager->priv->folder);
            break;
        
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
xfdesktop_file_icon_manager_finalize(GObject *obj)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(obj);
    
    gtk_target_list_unref(fmanager->priv->targets);
    
    g_free(fmanager->priv);
    
    G_OBJECT_CLASS(xfdesktop_file_icon_manager_parent_class)->finalize(obj);
}

static void
xfdesktop_file_icon_manager_icon_view_manager_init(XfdesktopIconViewManagerIface *iface)
{
    iface->manager_init = xfdesktop_file_icon_manager_real_init;
    iface->manager_fini = xfdesktop_file_icon_manager_fini;
}


static gboolean
xfdesktop_file_icon_manager_save_icons(gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    gchar relpath[PATH_MAX];
    XfceRc *rcfile;
    GList *l;
    XfdesktopIcon *icon;
    guint16 row, col;
    
    fmanager->priv->save_icons_id = 0;
    
    g_return_val_if_fail(fmanager->priv->icons_to_save, FALSE);
    
    g_snprintf(relpath, PATH_MAX, "xfce4/desktop/icons.screen%d.rc",
               gdk_screen_get_number(fmanager->priv->gscreen));
    
    rcfile = xfce_rc_config_open(XFCE_RESOURCE_CACHE, relpath, FALSE);
    if(!rcfile) {
        g_critical("Unable to determine location of icon position cache file.  " \
                   "Icon positions will not be saved.");
        return FALSE;
    }
    
    for(l = fmanager->priv->icons_to_save; l; l = l->next) {
        icon = XFDESKTOP_ICON(l->data);
        if(xfdesktop_icon_get_position(icon, &row, &col)) {
            xfce_rc_set_group(rcfile, xfdesktop_icon_peek_label(icon));
            xfce_rc_write_int_entry(rcfile, "row", row);
            xfce_rc_write_int_entry(rcfile, "col", col);
        }
        g_object_unref(G_OBJECT(icon));
    }
    g_list_free(fmanager->priv->icons_to_save);
    fmanager->priv->icons_to_save = NULL;
    
    xfce_rc_flush(rcfile);
    xfce_rc_close(rcfile);
    
    return FALSE;
}

static void
xfdesktop_file_icon_manager_icon_position_changed(XfdesktopFileIcon *icon,
                                                  gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    
    if(fmanager->priv->save_icons_id)
        g_source_remove(fmanager->priv->save_icons_id);
    
    fmanager->priv->icons_to_save = g_list_prepend(fmanager->priv->icons_to_save,
                                                   g_object_ref(G_OBJECT(icon)));
    
    fmanager->priv->save_icons_id = g_timeout_add(SAVE_DELAY,
                                                  xfdesktop_file_icon_manager_save_icons,
                                                  fmanager);
}

static void
xfdesktop_file_icon_manager_add_icon(XfdesktopFileIconManager *fmanager,
                                     ThunarVfsInfo *info,
                                     gboolean defer_if_missing)
{
    XfdesktopFileIcon *icon = NULL;
    gchar relpath[PATH_MAX];
    XfceRc *rcfile;
    gint16 row, col;
    gboolean do_add = FALSE;
    
    icon = xfdesktop_file_icon_new(info, fmanager->priv->gscreen);
    
    g_snprintf(relpath, PATH_MAX, "xfce4/desktop/icons.screen%d.rc",
               gdk_screen_get_number(fmanager->priv->gscreen));
    rcfile = xfce_rc_config_open(XFCE_RESOURCE_CACHE, relpath, TRUE);
    if(rcfile) {
        const gchar *group = xfdesktop_icon_peek_label(XFDESKTOP_ICON(icon));
        if(xfce_rc_has_group(rcfile, group)) {
            xfce_rc_set_group(rcfile, group);
            row = xfce_rc_read_int_entry(rcfile, "row", -1);
            col = xfce_rc_read_int_entry(rcfile, "col", -1);
            if(row >= 0 && col >= 0) {
                DBG("attempting to set icon '%s' to position (%d,%d)", group, row, col);
                xfdesktop_icon_set_position(XFDESKTOP_ICON(icon), row, col);
                do_add = TRUE;
            }
        }
        xfce_rc_close(rcfile);
    }
    
    if(!do_add) {
        if(defer_if_missing) {
            fmanager->priv->deferred_icons = g_list_prepend(fmanager->priv->deferred_icons,
                                                            thunar_vfs_info_ref(info));
        } else
            do_add = TRUE;
    }
    
    if(do_add) {
        g_signal_connect(G_OBJECT(icon), "position-changed",
                         G_CALLBACK(xfdesktop_file_icon_manager_icon_position_changed),
                         fmanager);
        xfdesktop_icon_view_add_item(fmanager->priv->icon_view,
                                     XFDESKTOP_ICON(icon));
        g_hash_table_insert(fmanager->priv->icons, info->path, icon);
    } else
        g_object_unref(G_OBJECT(icon));
}

static void
xfdesktop_file_icon_manager_volume_manager_cb(ThunarVfsMonitor *monitor,
                                              ThunarVfsMonitorHandle *handle,
                                              ThunarVfsMonitorEvent event,
                                              ThunarVfsPath *handle_path,
                                              ThunarVfsPath *event_path,
                                              gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon;
    ThunarVfsInfo *info;
    
    switch(event) {
        case THUNAR_VFS_MONITOR_EVENT_CHANGED:
            DBG("got changed event");
            break;
        
        case THUNAR_VFS_MONITOR_EVENT_CREATED:
            DBG("got created event");

            info = thunar_vfs_info_new_for_path(event_path, NULL);
            if(info) {
                if((info->flags & THUNAR_VFS_FILE_FLAGS_HIDDEN) == 0) {
                    thunar_vfs_path_ref(event_path);
                    xfdesktop_file_icon_manager_add_icon(fmanager,
                                                         info,
                                                         FALSE);
                }

                thunar_vfs_info_unref(info);
            }
            break;
        
        case THUNAR_VFS_MONITOR_EVENT_DELETED:
            DBG("got deleted event");
            icon = g_hash_table_lookup(fmanager->priv->icons, event_path);
            if(icon) {
                xfdesktop_icon_view_remove_item(fmanager->priv->icon_view,
                                                XFDESKTOP_ICON(icon));
                g_hash_table_remove(fmanager->priv->icons, event_path);
            }
            break;
        
    }
}


static gboolean
xfdesktop_file_icon_manager_listdir_infos_ready_cb(ThunarVfsJob *job,
                                                   GList *infos,
                                                   gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GList *l;
    ThunarVfsInfo *info;

    g_return_val_if_fail(job == fmanager->priv->list_job, FALSE);
    
    TRACE("entering");
    
    for(l = infos; l; l = l->next) {
        info = l->data;
        
        DBG("got a ThunarVfsInfo: %s", info->display_name);
        
        if((info->flags & THUNAR_VFS_FILE_FLAGS_HIDDEN) != 0)
            continue;
        
        thunar_vfs_path_ref(info->path);
        xfdesktop_file_icon_manager_add_icon(fmanager, info, TRUE);
    }
    
    return FALSE;
}

static void
xfdesktop_file_icon_manager_listdir_finished_cb(ThunarVfsJob *job,
                                                gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GList *l;
    
    g_return_if_fail(job == fmanager->priv->list_job);
    
    TRACE("entering");
    
    if(fmanager->priv->deferred_icons) {
        for(l = fmanager->priv->deferred_icons; l; l = l->next) {
            xfdesktop_file_icon_manager_add_icon(fmanager,
                                                 (ThunarVfsInfo *)l->data,
                                                 FALSE);
            thunar_vfs_info_unref((ThunarVfsInfo *)l->data);
        }
        g_list_free(fmanager->priv->deferred_icons);
        fmanager->priv->deferred_icons = NULL;
    }
    
    fmanager->priv->handle = thunar_vfs_monitor_add_directory(fmanager->priv->monitor,
                                                              fmanager->priv->folder,
                                                              xfdesktop_file_icon_manager_volume_manager_cb,
                                                              fmanager);
    
    g_object_unref(G_OBJECT(job));
    fmanager->priv->list_job = NULL;
}

static void
xfdesktop_file_icon_manager_listdir_error_cb(ThunarVfsJob *job,
                                             GError *error,
                                             gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    
    g_return_if_fail(job == fmanager->priv->list_job);
    
    g_warning("Got error from thunar-vfs on loading directory: %s",
              error->message);
}

static gboolean
xfdesktop_file_icon_manager_drag_drop(GtkWidget *widget,
                                      GdkDragContext *drag_context,
                                      gint x,
                                      gint y,
                                      guint time,
                                      gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GdkAtom target;
    
    target = gtk_drag_dest_find_target(widget, drag_context,
                                       fmanager->priv->targets);
    if(target == GDK_NONE)
        return FALSE;
    
    if(target == gdk_atom_intern("x-special/gnome-copied-files", FALSE))
        gtk_drag_get_data(widget, drag_context, target, time);
    else  /* handle utf8 string? */
        return FALSE;
    
    return TRUE;
}

static void
xfdesktop_file_icon_manager_drag_data_received(GtkWidget *widget,
                                               GdkDragContext *drag_context,
                                               gint x,
                                               gint y,
                                               GtkSelectionData *sel_data,
                                               guint info,
                                               guint time,
                                               gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(widget);
    gboolean copy_only = TRUE;
    GList *path_list;
    gchar *data;
    gboolean drop_ok = FALSE;
    
    if(sel_data->length > 0) {
        data = (gchar *)sel_data->data;
        data[sel_data->length] = 0;
        
        if(!g_ascii_strncasecmp(data, "copy\n", 5)) {
            copy_only = TRUE;
            data += 5;
        } else if(!g_ascii_strncasecmp(data, "cut\n", 4)) {
            copy_only = FALSE;
            data += 4;
        }
        
        path_list = thunar_vfs_path_list_from_string(data, NULL);
        
        if(path_list) {
            ThunarVfsJob *job = NULL;
            GList *dest_path_list = NULL;
            gint i, n;
            
            n = g_list_length(path_list);
            for(i = 0; i < n; ++i) {
                dest_path_list = g_list_prepend(dest_path_list,
                                                fmanager->priv->folder);
            }
            
            if(copy_only)
                job = thunar_vfs_copy_files(path_list, dest_path_list, NULL);
            else
                job = thunar_vfs_move_files(path_list, dest_path_list, NULL);
            
            if(job)
                drop_ok = TRUE;
            
            g_list_free(dest_path_list);
            g_list_free(path_list);
        }
    }
    
    gtk_drag_finish(drag_context, drop_ok, !copy_only, time);
}


/* virtual functions */

static gboolean
xfdesktop_file_icon_manager_real_init(XfdesktopIconViewManager *manager,
                                      XfdesktopIconView *icon_view)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    
    g_return_val_if_fail(!fmanager->priv->inited, FALSE);
    
    fmanager->priv->icon_view = icon_view;
    fmanager->priv->icons = g_hash_table_new_full(thunar_vfs_path_hash,
                                                  thunar_vfs_path_equal,
                                                  (GDestroyNotify)thunar_vfs_path_unref,
                                                  (GDestroyNotify)g_object_unref);
    
    if(!GTK_WIDGET_REALIZED(GTK_WIDGET(icon_view)))
        gtk_widget_realize(GTK_WIDGET(icon_view));
    fmanager->priv->gscreen = gtk_widget_get_screen(GTK_WIDGET(icon_view));
    
    xfdesktop_icon_view_set_allow_overlapping_drops(icon_view, TRUE);
    xfdesktop_icon_view_set_selection_mode(icon_view, GTK_SELECTION_MULTIPLE);
    xfdesktop_icon_view_enable_drag_source(icon_view,
                                           GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                                           targets, n_targets,
                                           GDK_ACTION_DEFAULT | GDK_ACTION_COPY
                                           | GDK_ACTION_MOVE);
    xfdesktop_icon_view_enable_drag_dest(icon_view, targets, n_targets,
                                         GDK_ACTION_DEFAULT | GDK_ACTION_COPY
                                         | GDK_ACTION_MOVE);
    g_signal_connect(G_OBJECT(icon_view), "drag-drop",
                     G_CALLBACK(xfdesktop_file_icon_manager_drag_drop),
                     fmanager);
    g_signal_connect(G_OBJECT(icon_view), "drag-data-received",
                     G_CALLBACK(xfdesktop_file_icon_manager_drag_data_received),
                     fmanager);
    
    thunar_vfs_init();
    
    DBG("Desktop path is '%s'", thunar_vfs_path_dup_string(fmanager->priv->folder));
    
    fmanager->priv->list_job = thunar_vfs_listdir(fmanager->priv->folder,
                                                  NULL);
    
    g_signal_connect(G_OBJECT(fmanager->priv->list_job), "error",
                     G_CALLBACK(xfdesktop_file_icon_manager_listdir_error_cb),
                     fmanager);
    g_signal_connect(G_OBJECT(fmanager->priv->list_job), "finished",
                     G_CALLBACK(xfdesktop_file_icon_manager_listdir_finished_cb),
                     fmanager);
    g_signal_connect(G_OBJECT(fmanager->priv->list_job), "infos-ready",
                     G_CALLBACK(xfdesktop_file_icon_manager_listdir_infos_ready_cb),
                     fmanager);
    
    fmanager->priv->monitor = thunar_vfs_monitor_get_default();
    
    fmanager->priv->inited = TRUE;
    
    return TRUE;
}

static void
xfdesktop_file_icon_manager_fini(XfdesktopIconViewManager *manager)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    
    g_return_if_fail(fmanager->priv->inited);
    
    fmanager->priv->inited = FALSE;
    
    if(fmanager->priv->deferred_icons) {
        g_list_foreach(fmanager->priv->deferred_icons,
                       (GFunc)thunar_vfs_info_unref, NULL);
        g_list_free(fmanager->priv->deferred_icons);
        fmanager->priv->deferred_icons = NULL;
    }
    
    if(fmanager->priv->handle) {
        thunar_vfs_monitor_remove(fmanager->priv->monitor,
                                  fmanager->priv->handle);
        fmanager->priv->handle = NULL;
    }
    
    g_object_unref(G_OBJECT(fmanager->priv->monitor));
    fmanager->priv->monitor = NULL;
    
    thunar_vfs_shutdown();
    
    g_hash_table_destroy(fmanager->priv->icons);
    fmanager->priv->icons = NULL;
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(fmanager->priv->icon_view),
                                         G_CALLBACK(xfdesktop_file_icon_manager_drag_drop),
                                         fmanager);
    g_signal_handlers_disconnect_by_func(G_OBJECT(fmanager->priv->icon_view),
                                         G_CALLBACK(xfdesktop_file_icon_manager_drag_data_received),
                                         fmanager);
    
    xfdesktop_icon_view_unset_drag_source(fmanager->priv->icon_view);
    xfdesktop_icon_view_unset_drag_dest(fmanager->priv->icon_view);
}


/* public api */

XfdesktopIconViewManager *
xfdesktop_file_icon_manager_new(ThunarVfsPath *folder)
{
    return g_object_new(XFDESKTOP_TYPE_FILE_ICON_MANAGER,
                        "folder", folder,
                        NULL);
}
