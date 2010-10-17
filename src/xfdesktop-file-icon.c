/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006 Brian Tarricone, <bjt23@cornell.edu>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>

#include <libxfce4ui/libxfce4ui.h>

#include "xfdesktop-file-utils.h"
#include "xfdesktop-file-icon.h"

struct _XfdesktopFileIconPrivate
{
    GList *active_jobs;
};

static void xfdesktop_file_icon_finalize(GObject *obj);

static gboolean xfdesktop_file_icon_activated(XfdesktopIcon *icon);


G_DEFINE_ABSTRACT_TYPE(XfdesktopFileIcon, xfdesktop_file_icon,
                       XFDESKTOP_TYPE_ICON)


static void
xfdesktop_file_icon_class_init(XfdesktopFileIconClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    XfdesktopIconClass *icon_class = (XfdesktopIconClass *)klass;
    
    g_type_class_add_private(klass, sizeof(XfdesktopFileIconPrivate));
    
    gobject_class->finalize = xfdesktop_file_icon_finalize;
    
    icon_class->activated = xfdesktop_file_icon_activated;
}

static void
xfdesktop_file_icon_init(XfdesktopFileIcon *icon)
{
    icon->priv = G_TYPE_INSTANCE_GET_PRIVATE(icon, XFDESKTOP_TYPE_FILE_ICON,
                                             XfdesktopFileIconPrivate);
}

static void
xfdesktop_file_icon_finalize(GObject *obj)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(obj);
    
    if(icon->priv->active_jobs) {
        GList *l;
        ThunarVfsJob *job;
        GCallback cb;
        
        for(l = icon->priv->active_jobs; l; l = l->next) {
            job = THUNAR_VFS_JOB(l->data);
            cb = g_object_get_data(G_OBJECT(job),
                                             "--xfdesktop-file-icon-callback");
            if(cb) {
                gpointer data = g_object_get_data(obj,
                                                  "--xfdesktop-file-icon-data");
                g_signal_handlers_disconnect_by_func(G_OBJECT(job),
                                                     G_CALLBACK(cb),
                                                     data);
                g_object_set_data(G_OBJECT(job),
                                  "--xfdesktop-file-icon-callback", NULL);
            }
            thunar_vfs_job_cancel(job);
            g_object_unref(G_OBJECT(job));
        }
        g_list_free(icon->priv->active_jobs);
    }
    
    G_OBJECT_CLASS(xfdesktop_file_icon_parent_class)->finalize(obj);
}

static gboolean
xfdesktop_file_icon_activated(XfdesktopIcon *icon)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    const ThunarVfsInfo *info = xfdesktop_file_icon_peek_info(file_icon);
    GtkWidget *icon_view, *toplevel;
    GdkScreen *gscreen;
    gboolean success = FALSE;
    
    TRACE("entering");
    
    if(!info)
        return FALSE;
    
    icon_view = xfdesktop_icon_peek_icon_view(icon);
    toplevel = gtk_widget_get_toplevel(icon_view);
    gscreen = gtk_widget_get_screen(icon_view);
    
    if(info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY) {
        xfdesktop_file_utils_open_folder(info, gscreen, GTK_WINDOW(toplevel));
        success = TRUE;
    } else {
        if(info->flags & THUNAR_VFS_FILE_FLAGS_EXECUTABLE) {
            success = thunar_vfs_info_execute(info, gscreen, NULL,
                                                xfce_get_homedir(), NULL);
        }
        
        if(!success) {
            ThunarVfsMimeDatabase *mime_database;
            ThunarVfsMimeApplication *mime_app;
            
            mime_database = thunar_vfs_mime_database_get_default();
            
            mime_app = thunar_vfs_mime_database_get_default_application(mime_database,
                                                                        info->mime_info);
            if(mime_app) {
                GList *path_list = g_list_prepend(NULL, info->path);
                
                DBG("executing");
                
                success = thunar_vfs_mime_handler_exec(THUNAR_VFS_MIME_HANDLER(mime_app),
                                                       gscreen, path_list, NULL);
                if(!success) {
                    gchar *primary = g_markup_printf_escaped(_("Unable to launch \"%s\":"),
                                                             info->display_name);
                    xfce_message_dialog(GTK_WINDOW(toplevel), _("Launch Error"),
                                        GTK_STOCK_DIALOG_ERROR, primary,
                                        _("The associated application could not be found or executed."),
                                        GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
                    g_free(primary);
                }
                
                g_object_unref(G_OBJECT(mime_app));
                g_list_free(path_list);
            } else {
                success = xfdesktop_file_utils_launch_fallback(info,
                                                               gscreen,
                                                               GTK_WINDOW(toplevel));
            }
            
            g_object_unref(G_OBJECT(mime_database));
        }
    }
    
    return success;
}


G_CONST_RETURN ThunarVfsInfo *
xfdesktop_file_icon_peek_info(XfdesktopFileIcon *icon)
{
    XfdesktopFileIconClass *klass;
    
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), NULL);
    
    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    
    if(klass->peek_info)
       return klass->peek_info(icon);
    else
        return NULL;
}

GFileInfo *
xfdesktop_file_icon_peek_file_info(XfdesktopFileIcon *icon)
{
    XfdesktopFileIconClass *klass;
    
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), NULL);
    
    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    
    if(klass->peek_file_info)
       return klass->peek_file_info(icon);
    else
        return NULL;
}

GFileInfo *
xfdesktop_file_icon_peek_filesystem_info(XfdesktopFileIcon *icon)
{
    XfdesktopFileIconClass *klass;
    
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), NULL);
    
    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    
    if(klass->peek_filesystem_info)
       return klass->peek_filesystem_info(icon);
    else
        return NULL;
}

GFile *
xfdesktop_file_icon_peek_file(XfdesktopFileIcon *icon)
{
    XfdesktopFileIconClass *klass;
    
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), NULL);
    
    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    
    if(klass->peek_file)
       return klass->peek_file(icon);
    else
        return NULL;
}

void
xfdesktop_file_icon_update_info(XfdesktopFileIcon *icon,
                                ThunarVfsInfo *info)
{
    XfdesktopFileIconClass *klass;
    
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON(icon));
    
    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    
    if(klass->update_info)
       klass->update_info(icon, info);
}

gboolean
xfdesktop_file_icon_can_rename_file(XfdesktopFileIcon *icon)
{
    XfdesktopFileIconClass *klass;
    
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), FALSE);
    
    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    
    if(klass->can_rename_file)
       return klass->can_rename_file(icon);
    else
        return FALSE;
}

gboolean
xfdesktop_file_icon_rename_file(XfdesktopFileIcon *icon,
                                const gchar *new_name)
{
    XfdesktopFileIconClass *klass;
    
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), FALSE);
    g_return_val_if_fail(new_name && *new_name, FALSE);
    
    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    
    if(klass->rename_file)
       return klass->rename_file(icon, new_name);
    else
        return FALSE;
}

gboolean
xfdesktop_file_icon_can_delete_file(XfdesktopFileIcon *icon)
{
    XfdesktopFileIconClass *klass;
    
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), FALSE);
    
    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    
    if(klass->can_delete_file)
       return klass->can_delete_file(icon);
    else
        return FALSE;
}

gboolean
xfdesktop_file_icon_delete_file(XfdesktopFileIcon *icon)
{
    XfdesktopFileIconClass *klass;
    
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), FALSE);
    
    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    
    if(klass->delete_file)
       return klass->delete_file(icon);
    else
        return FALSE;
}


void
xfdesktop_file_icon_add_active_job(XfdesktopFileIcon *icon,
                                   ThunarVfsJob *job)
{
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON(icon) && job);
    
    icon->priv->active_jobs = g_list_prepend(icon->priv->active_jobs,
                                             g_object_ref(G_OBJECT(job)));
}

gboolean
xfdesktop_file_icon_remove_active_job(XfdesktopFileIcon *icon,
                                      ThunarVfsJob *job)
{
    if(g_list_find(icon->priv->active_jobs, job)) {
        icon->priv->active_jobs = g_list_remove(icon->priv->active_jobs, job);
        g_object_unref(G_OBJECT(job));
        return TRUE;
    } else
        return FALSE;
}
