/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006 Brian Tarricone, <bjt23@cornell.edu>
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

#include <glib-object.h>

#include <libxfcegui4/libxfcegui4.h>

#include "xfdesktop-icon-view.h"
#include "xfdesktop-file-icon.h"
#include "xfdesktop-file-icon-manager.h"


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
    XfdesktopIconView *icon_view;
    
    GdkScreen *gscreen;
    
    ThunarVfsPath *folder;
    ThunarVfsMonitor *monitor;
    ThunarVfsMonitorHandle *handle;
    ThunarVfsJob *list_job;
    
    GHashTable *icons;
};


G_DEFINE_TYPE_EXTENDED(XfdesktopFileIconManager,
                       xfdesktop_file_icon_manager,
                       G_TYPE_OBJECT, 0,
                       G_IMPLEMENT_INTERFACE(XFDESKTOP_TYPE_ICON_VIEW_MANAGER,
                                             xfdesktop_file_icon_manager_icon_view_manager_init))

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

    g_free(fmanager->priv);
    
    G_OBJECT_CLASS(xfdesktop_file_icon_manager_parent_class)->finalize(obj);
}

static void
xfdesktop_file_icon_manager_icon_view_manager_init(XfdesktopIconViewManagerIface *iface)
{
    iface->manager_init = xfdesktop_file_icon_manager_real_init;
    iface->manager_fini = xfdesktop_file_icon_manager_fini;
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
    
    switch(event) {
        case THUNAR_VFS_MONITOR_EVENT_CHANGED:
            DBG("got changed event");
            break;
        
        case THUNAR_VFS_MONITOR_EVENT_CREATED:
            DBG("got created event");
            thunar_vfs_path_ref(event_path);
            icon = xfdesktop_file_icon_new(event_path,
                                           fmanager->priv->gscreen);                
            g_hash_table_insert(fmanager->priv->icons, event_path, icon);
            xfdesktop_icon_view_add_item(fmanager->priv->icon_view,
                                         XFDESKTOP_ICON(icon));
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
    XfdesktopFileIcon *icon;
    
    g_return_val_if_fail(job == fmanager->priv->list_job, FALSE);
    
    TRACE("entering");
    
    for(l = infos; l; l = l->next) {
        info = l->data;
        
        DBG("got a ThunarVfsInfo: %s", info->display_name);
        
        thunar_vfs_path_ref(info->path);
        icon = xfdesktop_file_icon_new(info->path, fmanager->priv->gscreen);
            
        g_hash_table_insert(fmanager->priv->icons, info->path, icon);
        xfdesktop_icon_view_add_item(fmanager->priv->icon_view,
                                     XFDESKTOP_ICON(icon));
    }
    
    return FALSE;
}

static void
xfdesktop_file_icon_manager_listdir_finished_cb(ThunarVfsJob *job,
                                                gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    
    g_return_if_fail(job == fmanager->priv->list_job);
    
    TRACE("entering");
    
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


/* virtual functions */

static gboolean
xfdesktop_file_icon_manager_real_init(XfdesktopIconViewManager *manager,
                                      XfdesktopIconView *icon_view)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    
    fmanager->priv->icon_view = icon_view;
    fmanager->priv->icons = g_hash_table_new_full(thunar_vfs_path_hash,
                                                  thunar_vfs_path_equal,
                                                  (GDestroyNotify)thunar_vfs_path_unref,
                                                  (GDestroyNotify)g_object_unref);
    
    if(!GTK_WIDGET_REALIZED(GTK_WIDGET(icon_view)))
        gtk_widget_realize(GTK_WIDGET(icon_view));
    fmanager->priv->gscreen = gtk_widget_get_screen(GTK_WIDGET(icon_view));
    
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
    
    return TRUE;
}

static void
xfdesktop_file_icon_manager_fini(XfdesktopIconViewManager *manager)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    
    if(fmanager->priv->handle) {
        thunar_vfs_monitor_remove(fmanager->priv->monitor,
                                  fmanager->priv->handle);
        fmanager->priv->handle = NULL;
    }
    
    g_object_unref(G_OBJECT(fmanager->priv->monitor));
    
    thunar_vfs_shutdown();
    
    g_hash_table_destroy(fmanager->priv->icons);
}


/* public api */

XfdesktopIconViewManager *
xfdesktop_file_icon_manager_new(ThunarVfsPath *folder)
{
    return g_object_new(XFDESKTOP_TYPE_FILE_ICON_MANAGER,
                        "folder", folder,
                        NULL);
}
