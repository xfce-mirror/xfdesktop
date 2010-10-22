/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006 Brian Tarricone, <bjt23@cornell.edu>
 *  Copyright(c) 2006 Benedikt Meurer, <benny@xfce.org>
 *  Copyright(c) 2010 Jannis Pohlmann, <jannis@xfce.org>
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

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_LIBEXO
#include <exo/exo.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include <gio/gio.h>

#include <libxfce4ui/libxfce4ui.h>

#ifdef HAVE_THUNARX
#include <thunarx/thunarx.h>
#endif

#include "xfdesktop-file-utils.h"
#include "xfdesktop-common.h"
#include "xfdesktop-regular-file-icon.h"

#define EMBLEM_SYMLINK  "emblem-symbolic-link"

struct _XfdesktopRegularFileIconPrivate
{
    GdkPixbuf *pix;
    gchar *tooltip;
    guint pix_opacity;
    gint cur_pix_size;
    GFileInfo *file_info;
    GFileInfo *filesystem_info;
    GFile *file;
    ThunarVfsInfo *info;
    GdkScreen *gscreen;
};

static void xfdesktop_regular_file_icon_finalize(GObject *obj);

static GdkPixbuf *xfdesktop_regular_file_icon_peek_pixbuf(XfdesktopIcon *icon,
                                                          gint size);
static G_CONST_RETURN gchar *xfdesktop_regular_file_icon_peek_label(XfdesktopIcon *icon);
static G_CONST_RETURN gchar *xfdesktop_regular_file_icon_peek_tooltip(XfdesktopIcon *icon);
static GdkDragAction xfdesktop_regular_file_icon_get_allowed_drag_actions(XfdesktopIcon *icon);
static GdkDragAction xfdesktop_regular_file_icon_get_allowed_drop_actions(XfdesktopIcon *icon);
static gboolean xfdesktop_regular_file_icon_do_drop_dest(XfdesktopIcon *icon,
                                                         XfdesktopIcon *src_icon,
                                                         GdkDragAction action);

static G_CONST_RETURN ThunarVfsInfo *xfdesktop_regular_file_icon_peek_info(XfdesktopFileIcon *icon);
static GFileInfo *xfdesktop_regular_file_icon_peek_file_info(XfdesktopFileIcon *icon);
static GFileInfo *xfdesktop_regular_file_icon_peek_filesystem_info(XfdesktopFileIcon *icon);
static GFile *xfdesktop_regular_file_icon_peek_file(XfdesktopFileIcon *icon);
static void xfdesktop_regular_file_icon_update_info(XfdesktopFileIcon *icon,
                                                    ThunarVfsInfo *info);
static void xfdesktop_regular_file_icon_update_file_info(XfdesktopFileIcon *icon,
                                                         GFileInfo *info);
static gboolean xfdesktop_regular_file_can_write_parent(XfdesktopFileIcon *icon);
static gboolean xfdesktop_regular_file_icon_delete_file(XfdesktopFileIcon *icon);

#ifdef HAVE_THUNARX
static void xfdesktop_regular_file_icon_tfi_init(ThunarxFileInfoIface *iface);
#endif

static void xfdesktop_delete_file_finished(ThunarVfsJob *job,
                                           gpointer user_data);

static inline void xfdesktop_regular_file_icon_invalidate_pixbuf(XfdesktopRegularFileIcon *icon);


#ifdef HAVE_THUNARX
G_DEFINE_TYPE_EXTENDED(XfdesktopRegularFileIcon, xfdesktop_regular_file_icon,
                       XFDESKTOP_TYPE_FILE_ICON, 0,
                       G_IMPLEMENT_INTERFACE(THUNARX_TYPE_FILE_INFO,
                                             xfdesktop_regular_file_icon_tfi_init)
                       )
#else
G_DEFINE_TYPE(XfdesktopRegularFileIcon, xfdesktop_regular_file_icon,
              XFDESKTOP_TYPE_FILE_ICON)
#endif



static void
xfdesktop_regular_file_icon_class_init(XfdesktopRegularFileIconClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    XfdesktopIconClass *icon_class = (XfdesktopIconClass *)klass;
    XfdesktopFileIconClass *file_icon_class = (XfdesktopFileIconClass *)klass;
    
    g_type_class_add_private(klass, sizeof(XfdesktopRegularFileIconPrivate));
    
    gobject_class->finalize = xfdesktop_regular_file_icon_finalize;
    
    icon_class->peek_pixbuf = xfdesktop_regular_file_icon_peek_pixbuf;
    icon_class->peek_label = xfdesktop_regular_file_icon_peek_label;
    icon_class->peek_tooltip = xfdesktop_regular_file_icon_peek_tooltip;
    icon_class->get_allowed_drag_actions = xfdesktop_regular_file_icon_get_allowed_drag_actions;
    icon_class->get_allowed_drop_actions = xfdesktop_regular_file_icon_get_allowed_drop_actions;
    icon_class->do_drop_dest = xfdesktop_regular_file_icon_do_drop_dest;
    
    file_icon_class->peek_info = xfdesktop_regular_file_icon_peek_info;
    file_icon_class->peek_file_info = xfdesktop_regular_file_icon_peek_file_info;
    file_icon_class->peek_filesystem_info = xfdesktop_regular_file_icon_peek_filesystem_info;
    file_icon_class->peek_file = xfdesktop_regular_file_icon_peek_file;
    file_icon_class->update_info = xfdesktop_regular_file_icon_update_info;
    file_icon_class->update_file_info = xfdesktop_regular_file_icon_update_file_info;
    file_icon_class->can_rename_file = xfdesktop_regular_file_can_write_parent;
    file_icon_class->can_delete_file = xfdesktop_regular_file_can_write_parent;
    file_icon_class->delete_file = xfdesktop_regular_file_icon_delete_file;
}

static void
xfdesktop_regular_file_icon_init(XfdesktopRegularFileIcon *icon)
{
    icon->priv = G_TYPE_INSTANCE_GET_PRIVATE(icon,
                                             XFDESKTOP_TYPE_REGULAR_FILE_ICON,
                                             XfdesktopRegularFileIconPrivate);
    icon->priv->pix_opacity = 100;
}

static void
xfdesktop_regular_file_icon_finalize(GObject *obj)
{
    XfdesktopRegularFileIcon *icon = XFDESKTOP_REGULAR_FILE_ICON(obj);
    GtkIconTheme *itheme = gtk_icon_theme_get_for_screen(icon->priv->gscreen);
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(itheme),
                                         G_CALLBACK(xfdesktop_regular_file_icon_invalidate_pixbuf),
                                         icon);
    
    if(icon->priv->pix)
        g_object_unref(G_OBJECT(icon->priv->pix));
    
    if(icon->priv->info)
        thunar_vfs_info_unref(icon->priv->info);

    if(icon->priv->file_info)
        g_object_unref(icon->priv->file_info);

    if(icon->priv->filesystem_info)
        g_object_unref(icon->priv->filesystem_info);

    g_object_unref(icon->priv->file);
    
    if(icon->priv->tooltip)
        g_free(icon->priv->tooltip);
    
    G_OBJECT_CLASS(xfdesktop_regular_file_icon_parent_class)->finalize(obj);
}

#ifdef HAVE_THUNARX
static void
xfdesktop_regular_file_icon_tfi_init(ThunarxFileInfoIface *iface)
{
    iface->get_name = xfdesktop_thunarx_file_info_get_name;
    iface->get_uri = xfdesktop_thunarx_file_info_get_uri;
    iface->get_parent_uri = xfdesktop_thunarx_file_info_get_parent_uri;
    iface->get_uri_scheme = xfdesktop_thunarx_file_info_get_uri_scheme_file;
    iface->get_mime_type = xfdesktop_thunarx_file_info_get_mime_type;
    iface->has_mime_type = xfdesktop_thunarx_file_info_has_mime_type;
    iface->is_directory = xfdesktop_thunarx_file_info_is_directory;
    iface->get_file_info = xfdesktop_thunarx_file_info_get_file_info;
    iface->get_filesystem_info = xfdesktop_thunarx_file_info_get_filesystem_info;
    iface->get_location = xfdesktop_thunarx_file_info_get_location;
}
#endif

static inline void
xfdesktop_regular_file_icon_invalidate_pixbuf(XfdesktopRegularFileIcon *icon)
{
    if(icon->priv->pix) {
        g_object_unref(G_OBJECT(icon->priv->pix));
        icon->priv->pix = NULL;
    }
}


static GdkPixbuf *
xfdesktop_regular_file_icon_peek_pixbuf(XfdesktopIcon *icon,
                                        gint size)
{
    XfdesktopRegularFileIcon *file_icon = XFDESKTOP_REGULAR_FILE_ICON(icon);
    const gchar *icon_name = NULL;
    GdkPixbuf *emblem_pix = NULL;
    
    if(size != file_icon->priv->cur_pix_size)
        xfdesktop_regular_file_icon_invalidate_pixbuf(file_icon);
    
    if(!file_icon->priv->pix) {
        /* check the application's binary name like thunar does (bug 1956) */
        if(!file_icon->priv->pix
           && file_icon->priv->info->flags & THUNAR_VFS_FILE_FLAGS_EXECUTABLE)
        {
            icon_name = thunar_vfs_path_get_name(file_icon->priv->info->path);
        }
        
        if(file_icon->priv->info->flags & THUNAR_VFS_FILE_FLAGS_SYMLINK) {
            GtkIconTheme *itheme = gtk_icon_theme_get_default();
            gint sym_pix_size = size * 2 / 3;
            
            emblem_pix = gtk_icon_theme_load_icon(itheme, EMBLEM_SYMLINK,
                                                  sym_pix_size, ITHEME_FLAGS,
                                                  NULL);
            if(emblem_pix) {
                if(gdk_pixbuf_get_width(emblem_pix) != sym_pix_size
                   || gdk_pixbuf_get_height(emblem_pix) != sym_pix_size)
                {
                    GdkPixbuf *tmp = gdk_pixbuf_scale_simple(emblem_pix,
                                                             sym_pix_size,
                                                             sym_pix_size,
                                                             GDK_INTERP_BILINEAR);
                    g_object_unref(G_OBJECT(emblem_pix));
                    emblem_pix = tmp;
                }
            }
        }
        
        file_icon->priv->pix = xfdesktop_file_utils_get_file_icon(icon_name,
                                                                  file_icon->priv->info,
                                                                  size,
                                                                  emblem_pix,
                                                                  file_icon->priv->pix_opacity);
        
        if(emblem_pix)
             g_object_unref(G_OBJECT(emblem_pix));
        
        file_icon->priv->cur_pix_size = size;
    }
    
    return file_icon->priv->pix;
}

static G_CONST_RETURN gchar *
xfdesktop_regular_file_icon_peek_label(XfdesktopIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_REGULAR_FILE_ICON(icon), NULL);
    return XFDESKTOP_REGULAR_FILE_ICON(icon)->priv->info->display_name;
}

static GdkDragAction
xfdesktop_regular_file_icon_get_allowed_drag_actions(XfdesktopIcon *icon)
{
    const ThunarVfsInfo *info = xfdesktop_file_icon_peek_info(XFDESKTOP_FILE_ICON(icon));
    GdkDragAction actions = GDK_ACTION_LINK;  /* we can always link */
    
    if(!info)
        return 0;
    
    if(info->flags & THUNAR_VFS_FILE_FLAGS_READABLE) {
        ThunarVfsPath *parent_path;
        ThunarVfsInfo *parent_info;
        
        actions |= GDK_ACTION_COPY;
        
        /* we can only move if the parent is writable */
        parent_path = thunar_vfs_path_get_parent(info->path);
        parent_info = thunar_vfs_info_new_for_path(parent_path, NULL);
        if(parent_info) {
            if(parent_info->flags & THUNAR_VFS_FILE_FLAGS_WRITABLE)
                actions |= GDK_ACTION_MOVE;
            thunar_vfs_info_unref(parent_info);
        }
        
        /* |parent_path| is owned by |info| */
    }
    
    return actions;
}

static GdkDragAction
xfdesktop_regular_file_icon_get_allowed_drop_actions(XfdesktopIcon *icon)
{
    const ThunarVfsInfo *info = xfdesktop_file_icon_peek_info(XFDESKTOP_FILE_ICON(icon));
    
    if(!info)
        return 0;
    
    /* if it's executable we can 'copy'.  if it's a folder we can do anything
     * if it's writable. */
    if(info->flags & THUNAR_VFS_FILE_FLAGS_EXECUTABLE)
        return GDK_ACTION_COPY;
    else if(THUNAR_VFS_FILE_TYPE_DIRECTORY == info->type
            && info->flags & THUNAR_VFS_FILE_FLAGS_WRITABLE)
    {
        return GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK;
    } else
        return 0;
}

static void
xfdesktop_regular_file_icon_drag_job_error(ThunarVfsJob *job,
                                           GError *error,
                                           gpointer user_data)
{
    XfdesktopRegularFileIcon *regular_file_icon = XFDESKTOP_REGULAR_FILE_ICON(user_data);
    XfdesktopFileIcon *src_file_icon = g_object_get_data(G_OBJECT(job),
                                                         "--xfdesktop-src-file-icon");
    XfdesktopFileUtilsFileop fileop = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(job),
                                                                        "--xfdesktop-file-icon-action"));
    
    g_return_if_fail(regular_file_icon);
    
    if(!src_file_icon)
        return;
    
    xfdesktop_file_utils_handle_fileop_error(NULL,
                                             xfdesktop_file_icon_peek_info(src_file_icon),
                                             regular_file_icon->priv->info, fileop,
                                             error);
}

static ThunarVfsInteractiveJobResponse
xfdesktop_regular_file_icon_interactive_job_ask(ThunarVfsJob *job,
                                                const gchar *message,
                                                ThunarVfsInteractiveJobResponse choices,
                                                gpointer user_data)
{
    GtkWidget *icon_view = xfdesktop_icon_peek_icon_view(XFDESKTOP_ICON(user_data));
    GtkWidget *toplevel = gtk_widget_get_toplevel(icon_view);
    return xfdesktop_file_utils_interactive_job_ask(GTK_WINDOW(toplevel),
                                                    message, choices);
}

static void
xfdesktop_regular_file_icon_drag_job_finished(ThunarVfsJob *job,
                                              gpointer user_data)
{
    XfdesktopRegularFileIcon *regular_file_icon = XFDESKTOP_REGULAR_FILE_ICON(user_data);
    XfdesktopFileIcon *src_file_icon = g_object_get_data(G_OBJECT(job),
                                                         "--xfdesktop-src-file-icon");
    
    if(!xfdesktop_file_icon_remove_active_job(XFDESKTOP_FILE_ICON(regular_file_icon), job))
        g_critical("ThunarVfsJob 0x%p not found in active jobs list", job);
    
    if(!xfdesktop_file_icon_remove_active_job(src_file_icon, job))
        g_critical("ThunarVfsJob 0x%p not found in active jobs list", job);
    
    g_object_unref(G_OBJECT(job));
    
    g_object_unref(G_OBJECT(src_file_icon));
    g_object_unref(G_OBJECT(regular_file_icon));
}

gboolean
xfdesktop_regular_file_icon_do_drop_dest(XfdesktopIcon *icon,
                                         XfdesktopIcon *src_icon,
                                         GdkDragAction action)
{
    XfdesktopRegularFileIcon *regular_file_icon = XFDESKTOP_REGULAR_FILE_ICON(icon);
    XfdesktopFileIcon *src_file_icon = XFDESKTOP_FILE_ICON(src_icon);
    const ThunarVfsInfo *src_info;
    
    DBG("entering");
    
    g_return_val_if_fail(regular_file_icon && src_file_icon, FALSE);
    g_return_val_if_fail(xfdesktop_regular_file_icon_get_allowed_drop_actions(icon) != 0,
                         FALSE);
    
    src_info = xfdesktop_file_icon_peek_info(src_file_icon);
    if(!src_info)
        return FALSE;
    
    if(regular_file_icon->priv->info->flags & THUNAR_VFS_FILE_FLAGS_EXECUTABLE) {
        GList *path_list = g_list_prepend(NULL, src_info->path);
        GError *error = NULL;
        gboolean succeeded;
        
        succeeded = thunar_vfs_info_execute(regular_file_icon->priv->info,
                                            regular_file_icon->priv->gscreen,
                                            path_list,
                                            xfce_get_homedir(),
                                            &error);
        g_list_free(path_list);
        
        if(!succeeded) {
            gchar *primary = g_markup_printf_escaped(_("Failed to run \"%s\":"),
                                                     regular_file_icon->priv->info->display_name);
            xfce_message_dialog(NULL, _("Run Error"), GTK_STOCK_DIALOG_ERROR,
                                primary, error->message,
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
            g_free(primary);
            g_error_free(error);
            
            return FALSE;
        }
        
        return TRUE;
    } else {
        ThunarVfsJob *job = NULL;
        const gchar *name;
        ThunarVfsPath *dest_path;
        
        if(thunar_vfs_path_is_root(src_info->path))
            return FALSE;
        
        name = thunar_vfs_path_get_name(src_info->path);
        if(!name)
            return FALSE;
        
        dest_path = thunar_vfs_path_relative(regular_file_icon->priv->info->path,
                                             name);
        if(!dest_path)
            return FALSE;
        
        switch(action) {
            case GDK_ACTION_MOVE:
                DBG("doing move");
                job = thunar_vfs_move_file(src_info->path, dest_path, NULL);
                break;
            
            case GDK_ACTION_COPY:
                DBG("doing copy");
                job = thunar_vfs_copy_file(src_info->path, dest_path, NULL);
                break;
            
            case GDK_ACTION_LINK:
                DBG("doing link");
                job = thunar_vfs_link_file(src_info->path, dest_path, NULL);
                break;
            
            default:
                g_warning("Unsupported drag action: %d", action);
        }
        
        thunar_vfs_path_unref(dest_path);
        
        if(job) {
            DBG("got job, action initiated");
            
            /* ensure they aren't destroyed until the job is finished */
            g_object_ref(G_OBJECT(src_file_icon));
            g_object_ref(G_OBJECT(regular_file_icon));
            
            g_object_set_data(G_OBJECT(job), "--xfdesktop-file-icon-callback",
                              G_CALLBACK(xfdesktop_regular_file_icon_drag_job_finished));
            g_object_set_data(G_OBJECT(job), "--xfdesktop-file-icon-data", icon);
            xfdesktop_file_icon_add_active_job(XFDESKTOP_FILE_ICON(regular_file_icon),
                                               job);
            xfdesktop_file_icon_add_active_job(src_file_icon, job);
            
            g_object_set_data(G_OBJECT(job), "--xfdesktop-src-file-icon",
                              src_icon);
            g_object_set_data(G_OBJECT(job), "--xfdesktop-file-icon-action",
                              GINT_TO_POINTER(action == GDK_ACTION_MOVE
                                              ? XFDESKTOP_FILE_UTILS_FILEOP_MOVE
                                              : (action == GDK_ACTION_COPY
                                                 ? XFDESKTOP_FILE_UTILS_FILEOP_COPY
                                                 : XFDESKTOP_FILE_UTILS_FILEOP_LINK)));
            g_signal_connect(G_OBJECT(job), "error",
                             G_CALLBACK(xfdesktop_regular_file_icon_drag_job_error),
                             regular_file_icon);
            g_signal_connect(G_OBJECT(job), "ask",
                             G_CALLBACK(xfdesktop_regular_file_icon_interactive_job_ask),
                             regular_file_icon);
            g_signal_connect(G_OBJECT(job), "finished",
                             G_CALLBACK(xfdesktop_regular_file_icon_drag_job_finished),
                             regular_file_icon);
            
            return TRUE;
        } else
            return FALSE;
    }
    
    return FALSE;
}

static G_CONST_RETURN gchar *
xfdesktop_regular_file_icon_peek_tooltip(XfdesktopIcon *icon)
{
    XfdesktopRegularFileIcon *regular_file_icon = XFDESKTOP_REGULAR_FILE_ICON(icon);
    
    if(!regular_file_icon->priv->info)
       return NULL;  /* FIXME: implement something here */
    
    if(!regular_file_icon->priv->tooltip) {
        gchar mod[64], *kind, sizebuf[64], *size;
        struct tm *tm = localtime(&regular_file_icon->priv->info->mtime);

        strftime(mod, 64, "%Y-%m-%d %H:%M:%S", tm);
        kind = xfdesktop_file_utils_get_file_kind(regular_file_icon->priv->info, NULL);
        thunar_vfs_humanize_size(regular_file_icon->priv->info->size, sizebuf, 64);
        size = g_strdup_printf(_("%s (%" G_GINT64_FORMAT " Bytes)"), sizebuf,
                              (gint64)regular_file_icon->priv->info->size);
        
        regular_file_icon->priv->tooltip = g_strdup_printf(_("Kind: %s\nModified:%s\nSize: %s"),
                                                   kind, mod, size);
        
        g_free(kind);
        g_free(size);
    }
    
    return regular_file_icon->priv->tooltip;
}

static gboolean
xfdesktop_regular_file_can_write_parent(XfdesktopFileIcon *icon)
{
    XfdesktopRegularFileIcon *file_icon = XFDESKTOP_REGULAR_FILE_ICON(icon);
    ThunarVfsPath *parent;
    ThunarVfsInfo *parent_info;
    gboolean writable;

    g_return_val_if_fail(file_icon && file_icon->priv->info, FALSE);

    parent = thunar_vfs_path_get_parent(file_icon->priv->info->path);
    if(!parent)
        return FALSE;

    parent_info = thunar_vfs_info_new_for_path(parent, NULL);
    if(!parent_info)
        return FALSE;

    writable = (parent_info->flags & THUNAR_VFS_FILE_FLAGS_WRITABLE);
    thunar_vfs_info_unref(parent_info);

    return writable;

}

static void
xfdesktop_delete_file_error(ThunarVfsJob *job,
                            GError *error,
                            gpointer user_data)
{
    XfdesktopRegularFileIcon *icon = XFDESKTOP_REGULAR_FILE_ICON(user_data);
    GtkWidget *icon_view = xfdesktop_icon_peek_icon_view(XFDESKTOP_ICON(icon));
    GtkWidget *toplevel = gtk_widget_get_toplevel(icon_view);
    gchar *primary = g_markup_printf_escaped("There was an error deleting \"%s\":",
                                             icon->priv->info->display_name);
                                     
    xfce_message_dialog(GTK_WINDOW(toplevel), _("Error"),
                        GTK_STOCK_DIALOG_ERROR, primary,
                        error->message, GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                        NULL);
    
    g_free(primary);
}

static void
xfdesktop_delete_file_finished(ThunarVfsJob *job,
                               gpointer user_data)
{
    XfdesktopRegularFileIcon *icon = XFDESKTOP_REGULAR_FILE_ICON(user_data);
    
    if(!xfdesktop_file_icon_remove_active_job(XFDESKTOP_FILE_ICON(icon), job))
        g_critical("ThunarVfsJob 0x%p not found in active jobs list", job);
    
    g_object_unref(G_OBJECT(job));
    g_object_unref(G_OBJECT(icon));
}

static gboolean
xfdesktop_regular_file_icon_delete_file(XfdesktopFileIcon *icon)
{
    XfdesktopRegularFileIcon *regular_file_icon = XFDESKTOP_REGULAR_FILE_ICON(icon);
    ThunarVfsJob *job;
    
    job = thunar_vfs_unlink_file(regular_file_icon->priv->info->path, NULL);
    
    if(job) {
        g_object_set_data(G_OBJECT(job), "--xfdesktop-file-icon-callback",
                          G_CALLBACK(xfdesktop_delete_file_finished));
        g_object_set_data(G_OBJECT(job), "--xfdesktop-file-icon-data", icon);
        xfdesktop_file_icon_add_active_job(XFDESKTOP_FILE_ICON(regular_file_icon),
                                           job);
        
        g_signal_connect(G_OBJECT(job), "error",
                         G_CALLBACK(xfdesktop_delete_file_error), icon);
        g_signal_connect(G_OBJECT(job), "finished",
                         G_CALLBACK(xfdesktop_delete_file_finished), icon);
        
        g_object_ref(G_OBJECT(icon));
    }
    
    /* no real way to signal success or failure at this point */
    return (job != NULL);
}

static G_CONST_RETURN ThunarVfsInfo *
xfdesktop_regular_file_icon_peek_info(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_REGULAR_FILE_ICON(icon), NULL);
    return XFDESKTOP_REGULAR_FILE_ICON(icon)->priv->info;
}

static GFileInfo *
xfdesktop_regular_file_icon_peek_file_info(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_REGULAR_FILE_ICON(icon), NULL);
    return XFDESKTOP_REGULAR_FILE_ICON(icon)->priv->file_info;
}

static GFileInfo *
xfdesktop_regular_file_icon_peek_filesystem_info(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_REGULAR_FILE_ICON(icon), NULL);
    return XFDESKTOP_REGULAR_FILE_ICON(icon)->priv->filesystem_info;
}

static GFile *
xfdesktop_regular_file_icon_peek_file(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_REGULAR_FILE_ICON(icon), NULL);
    return XFDESKTOP_REGULAR_FILE_ICON(icon)->priv->file;
}

static void
xfdesktop_regular_file_icon_update_info(XfdesktopFileIcon *icon,
                                        ThunarVfsInfo *info)
{
    XfdesktopRegularFileIcon *regular_file_icon = XFDESKTOP_REGULAR_FILE_ICON(icon);
    gboolean label_changed = TRUE;
    
    g_return_if_fail(XFDESKTOP_IS_REGULAR_FILE_ICON(icon) && info);
    
    if(!strcmp(regular_file_icon->priv->info->display_name, info->display_name))
        label_changed = FALSE;
    
    thunar_vfs_info_unref(regular_file_icon->priv->info);
    regular_file_icon->priv->info = thunar_vfs_info_ref(info);
    
    if(label_changed)
        xfdesktop_icon_label_changed(XFDESKTOP_ICON(icon));
    
    /* not really easy to check if this changed or not, so just invalidate it */
    xfdesktop_regular_file_icon_invalidate_pixbuf(regular_file_icon);
    xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(icon));
}

static void
xfdesktop_regular_file_icon_update_file_info(XfdesktopFileIcon *icon,
                                             GFileInfo *info)
{
    XfdesktopRegularFileIcon *regular_file_icon = XFDESKTOP_REGULAR_FILE_ICON(icon);
    const gchar *old_display_name, *new_display_name;
    gboolean label_changed = FALSE;
    
    g_return_if_fail(XFDESKTOP_IS_REGULAR_FILE_ICON(icon));
    g_return_if_fail(G_IS_FILE_INFO(info));

    if(regular_file_icon->priv->file_info) { 
        old_display_name = g_file_info_get_attribute_string(regular_file_icon->priv->file_info,
                                                            G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
        new_display_name = g_file_info_get_attribute_string(info,
                                                            G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);

        label_changed = (g_strcmp0(old_display_name, new_display_name) != 0);

        /* release the old file info */
        g_object_unref(regular_file_icon->priv->file_info);
        regular_file_icon->priv->file_info = NULL;
    }

    regular_file_icon->priv->info = g_object_ref(info);
    
    if(label_changed)
        xfdesktop_icon_label_changed(XFDESKTOP_ICON(icon));
    
    /* not really easy to check if this changed or not, so just invalidate it */
    xfdesktop_regular_file_icon_invalidate_pixbuf(regular_file_icon);
    xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(icon));
}


/* public API */

XfdesktopRegularFileIcon *
xfdesktop_regular_file_icon_new(ThunarVfsInfo *info,
                                GdkScreen *screen)
{
    gchar *path;

    XfdesktopRegularFileIcon *regular_file_icon = g_object_new(XFDESKTOP_TYPE_REGULAR_FILE_ICON, NULL);

    regular_file_icon->priv->info = thunar_vfs_info_ref(info);
    regular_file_icon->priv->gscreen = screen;
    
    /* convert the ThunarVfsPath into a GFile */
    path = thunar_vfs_path_dup_string(info->path);
    regular_file_icon->priv->file = g_file_new_for_path(path);
    g_free(path);

    /* query file information from GIO */
    regular_file_icon->priv->file_info = g_file_query_info(regular_file_icon->priv->file,
                                                           XFDESKTOP_FILE_INFO_NAMESPACE,
                                                           G_FILE_QUERY_INFO_NONE,
                                                           NULL, NULL);

    /* query file system information from GIO */
    regular_file_icon->priv->filesystem_info = g_file_query_filesystem_info(regular_file_icon->priv->file,
                                                                            XFDESKTOP_FILESYSTEM_INFO_NAMESPACE,
                                                                            NULL, NULL);

    g_signal_connect_swapped(G_OBJECT(gtk_icon_theme_get_for_screen(screen)),
                             "changed",
                             G_CALLBACK(xfdesktop_regular_file_icon_invalidate_pixbuf),
                             regular_file_icon);
    
    return regular_file_icon;
}

void
xfdesktop_regular_file_icon_set_pixbuf_opacity(XfdesktopRegularFileIcon *icon,
                                       guint opacity)
{
    g_return_if_fail(XFDESKTOP_IS_REGULAR_FILE_ICON(icon) && opacity <= 100);
    
    if(opacity == icon->priv->pix_opacity)
        return;
    
    icon->priv->pix_opacity = opacity;
    
    xfdesktop_regular_file_icon_invalidate_pixbuf(icon);
    xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(icon));
}
