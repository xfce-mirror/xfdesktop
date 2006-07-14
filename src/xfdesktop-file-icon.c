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

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_LIBEXO
#define EXO_API_SUBJECT_TO_CHANGE
#include <exo/exo.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include <libxfcegui4/libxfcegui4.h>

#ifdef HAVE_THUNARX
#include <thunarx/thunarx.h>
#endif

#include "xfdesktop-file-utils.h"
#include "xfdesktop-icon.h"
#include "xfdesktop-file-icon.h"

#define EMBLEM_SYMLINK  "emblem-symbolic-link"

enum
{
    SIG_POS_CHANGED = 0,
    N_SIGS
};

struct _XfdesktopFileIconPrivate
{
    gint16 row;
    gint16 col;
    GdkPixbuf *pix;
    gchar *tooltip;
    guint pix_opacity;
    gint cur_pix_size;
    GdkRectangle extents;
    ThunarVfsInfo *info;
    GdkScreen *gscreen;
    GList *active_jobs;
    ThunarVfsVolume *volume;
};

static void xfdesktop_file_icon_finalize(GObject *obj);

static void xfdesktop_file_icon_icon_init(XfdesktopIconIface *iface);
static GdkPixbuf *xfdesktop_file_icon_peek_pixbuf(XfdesktopIcon *icon,
                                                  gint size);
static G_CONST_RETURN gchar *xfdesktop_file_icon_peek_label(XfdesktopIcon *icon);
static G_CONST_RETURN gchar *xfdesktop_file_icon_peek_tooltip(XfdesktopIcon *icon);
static void xfdesktop_file_icon_set_position(XfdesktopIcon *icon,
                                             gint16 row,
                                             gint16 col);
static gboolean xfdesktop_file_icon_get_position(XfdesktopIcon *icon,
                                                 gint16 *row,
                                                 gint16 *col);
static void xfdesktop_file_icon_set_extents(XfdesktopIcon *icon,
                                            const GdkRectangle *extents);
static gboolean xfdesktop_file_icon_get_extents(XfdesktopIcon *icon,
                                                GdkRectangle *extents);
static gboolean xfdesktop_file_icon_is_drop_dest(XfdesktopIcon *icon);
static XfdesktopIconDragResult xfdesktop_file_icon_do_drop_dest(XfdesktopIcon *icon,
                                                                XfdesktopIcon *src_icon,
                                                                GdkDragAction action);

#ifdef HAVE_THUNARX
static void xfdesktop_file_icon_tfi_init(ThunarxFileInfoIface *iface);
static gchar *xfdesktop_file_icon_tfi_get_name(ThunarxFileInfo *file_info);
static gchar *xfdesktop_file_icon_tfi_get_uri(ThunarxFileInfo *file_info);
static gchar *xfdesktop_file_icon_tfi_get_parent_uri(ThunarxFileInfo *file_info);
static gchar *xfdesktop_file_icon_tfi_get_uri_scheme(ThunarxFileInfo *file_info);
static gchar *xfdesktop_file_icon_tfi_get_mime_type(ThunarxFileInfo *file_info);
static gboolean xfdesktop_file_icon_tfi_has_mime_type(ThunarxFileInfo *file_info,
                                                      const gchar *mime_type);
static gboolean xfdesktop_file_icon_tfi_is_directory(ThunarxFileInfo *file_info);
static ThunarVfsInfo *xfdesktop_file_icon_tfi_get_vfs_info(ThunarxFileInfo *file_info);
#endif


static void xfdesktop_delete_file_finished(ThunarVfsJob *job,
                                           gpointer user_data);

static inline void xfdesktop_file_icon_invalidate_pixbuf(XfdesktopFileIcon *icon);

static guint __signals[N_SIGS] = { 0, };
static GdkPixbuf *xfdesktop_fallback_icon = NULL;


#ifdef HAVE_THUNARX
G_DEFINE_TYPE_EXTENDED(XfdesktopFileIcon, xfdesktop_file_icon,
                       G_TYPE_OBJECT, 0,
                       G_IMPLEMENT_INTERFACE(XFDESKTOP_TYPE_ICON,
                                             xfdesktop_file_icon_icon_init)
                       G_IMPLEMENT_INTERFACE(THUNARX_TYPE_FILE_INFO,
                                             xfdesktop_file_icon_tfi_init)
                       )
#else
G_DEFINE_TYPE_EXTENDED(XfdesktopFileIcon, xfdesktop_file_icon,
                       G_TYPE_OBJECT, 0,
                       G_IMPLEMENT_INTERFACE(XFDESKTOP_TYPE_ICON,
                                             xfdesktop_file_icon_icon_init)
                       )
#endif



static void
xfdesktop_file_icon_class_init(XfdesktopFileIconClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    
    gobject_class->finalize = xfdesktop_file_icon_finalize;
    
    __signals[SIG_POS_CHANGED] = g_signal_new("position-changed",
                                              XFDESKTOP_TYPE_FILE_ICON,
                                              G_SIGNAL_RUN_LAST,
                                              G_STRUCT_OFFSET(XfdesktopFileIconClass,
                                                              position_changed),
                                              NULL, NULL,
                                              g_cclosure_marshal_VOID__VOID,
                                              G_TYPE_NONE, 0);
}

static void
xfdesktop_file_icon_init(XfdesktopFileIcon *icon)
{
    icon->priv = g_new0(XfdesktopFileIconPrivate, 1);
    icon->priv->pix_opacity = 100;
}

static void
xfdesktop_file_icon_finalize(GObject *obj)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(obj);
    GtkIconTheme *itheme = gtk_icon_theme_get_for_screen(icon->priv->gscreen);
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(itheme),
                                         G_CALLBACK(xfdesktop_file_icon_invalidate_pixbuf),
                                         icon);
    
    if(icon->priv->active_jobs) {
        GList *l;
        ThunarVfsJob *job;
        GCallback cb;
        for(l = icon->priv->active_jobs; l; l = l->next) {
            job = THUNAR_VFS_JOB(l->data);
            cb = g_object_get_data(G_OBJECT(job),
                                             "--xfdesktop-file-icon-callback");
            if(cb) {
                gpointer data = g_object_get_data(G_OBJECT(obj),
                                                  "-xfdesktop-file-icon-data");
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
    
    if(icon->priv->pix)
        g_object_unref(G_OBJECT(icon->priv->pix));
    
    if(icon->priv->info)
        thunar_vfs_info_unref(icon->priv->info);
    
    if(icon->priv->volume)
        g_object_unref(G_OBJECT(icon->priv->volume));
    
    if(icon->priv->tooltip)
        g_free(icon->priv->tooltip);
    
    g_free(icon->priv);
    
    G_OBJECT_CLASS(xfdesktop_file_icon_parent_class)->finalize(obj);
}

static void
xfdesktop_file_icon_icon_init(XfdesktopIconIface *iface)
{
    iface->peek_pixbuf = xfdesktop_file_icon_peek_pixbuf;
    iface->peek_label = xfdesktop_file_icon_peek_label;
    iface->peek_tooltip = xfdesktop_file_icon_peek_tooltip;
    iface->set_position = xfdesktop_file_icon_set_position;
    iface->get_position = xfdesktop_file_icon_get_position;
    iface->set_extents = xfdesktop_file_icon_set_extents;
    iface->get_extents = xfdesktop_file_icon_get_extents;
    iface->is_drop_dest = xfdesktop_file_icon_is_drop_dest;
    iface->do_drop_dest = xfdesktop_file_icon_do_drop_dest;
}

static inline void
xfdesktop_file_icon_invalidate_pixbuf(XfdesktopFileIcon *icon)
{
    if(icon->priv->pix) {
        g_object_unref(G_OBJECT(icon->priv->pix));
        icon->priv->pix = NULL;
    }
}



static XfdesktopFileIcon *
_xfdesktop_file_icon_new_internal(ThunarVfsInfo *info,
                                  ThunarVfsVolume *volume,
                                  GdkScreen *screen)
{
    XfdesktopFileIcon *file_icon = g_object_new(XFDESKTOP_TYPE_FILE_ICON, NULL);
    file_icon->priv->info = info;
    file_icon->priv->volume = volume ? g_object_ref(G_OBJECT(volume)) : NULL;
    file_icon->priv->gscreen = screen;
    
    g_signal_connect_swapped(G_OBJECT(gtk_icon_theme_get_for_screen(screen)),
                             "changed",
                             G_CALLBACK(xfdesktop_file_icon_invalidate_pixbuf),
                             file_icon);
    
    return file_icon;

}

XfdesktopFileIcon *
xfdesktop_file_icon_new(ThunarVfsInfo *info,
                        GdkScreen *screen)
{
    g_return_val_if_fail(info, NULL);
    return _xfdesktop_file_icon_new_internal(thunar_vfs_info_ref(info), NULL,
                                             screen);
}

XfdesktopFileIcon *
xfdesktop_file_icon_new_for_volume(ThunarVfsVolume *volume,
                                   GdkScreen *screen)
{
    ThunarVfsPath *path;
    ThunarVfsInfo *info = NULL;
    
    g_return_val_if_fail(THUNAR_VFS_IS_VOLUME(volume), NULL);
    
    path = thunar_vfs_volume_get_mount_point(volume);
    if(path)
        info = thunar_vfs_info_new_for_path(path, NULL);
    
    return _xfdesktop_file_icon_new_internal(info, volume, screen);
}

void
xfdesktop_file_icon_set_pixbuf_opacity(XfdesktopFileIcon *icon,
                                       guint opacity)
{
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON(icon) && opacity <= 100);
    
    if(opacity == icon->priv->pix_opacity)
        return;
    
    icon->priv->pix_opacity = opacity;
    if(icon->priv->pix) {
        g_object_unref(G_OBJECT(icon->priv->pix));
        icon->priv->pix = NULL;
    }
    
    xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(icon));
}

static GdkPixbuf *
xfdesktop_file_icon_peek_pixbuf(XfdesktopIcon *icon,
                                gint size)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    const gchar *icon_name;
    gboolean loaded_new = FALSE;
    
    if(size != file_icon->priv->cur_pix_size) {
        if(file_icon->priv->pix) {
            g_object_unref(G_OBJECT(file_icon->priv->pix));
            file_icon->priv->pix = NULL;
        }
    }
    
    if(!file_icon->priv->pix) {
        /* if we're a removable volume, first try to get the volume's icon */
        if(file_icon->priv->volume) {
            icon_name = thunar_vfs_volume_lookup_icon_name(file_icon->priv->volume,
                                                           gtk_icon_theme_get_default());
            if(icon_name) {
                file_icon->priv->pix = xfce_themed_icon_load(icon_name, size);
                if(file_icon->priv->pix)
                    loaded_new = TRUE;
            }
        }
        
        /* check the application's binary name like thunar does (bug 1956) */
        if(!file_icon->priv->pix && file_icon->priv->info
           && file_icon->priv->info->flags & THUNAR_VFS_FILE_FLAGS_EXECUTABLE)
        {
            icon_name = thunar_vfs_path_get_name(file_icon->priv->info->path);
            file_icon->priv->pix = xfce_themed_icon_load(icon_name, size);
            if(file_icon->priv->pix)
                loaded_new = TRUE;
        }
        
        if(!file_icon->priv->pix && file_icon->priv->info) {
            icon_name = thunar_vfs_info_get_custom_icon(file_icon->priv->info);
            if(icon_name) {
                file_icon->priv->pix = xfce_themed_icon_load(icon_name, size);
                if(file_icon->priv->pix)
                    loaded_new = TRUE;
            }
        }
            
        if(G_UNLIKELY(!file_icon->priv->pix) && file_icon->priv->info) {
            icon_name = thunar_vfs_mime_info_lookup_icon_name(file_icon->priv->info->mime_info,
                                                              gtk_icon_theme_get_default());
            
            if(icon_name) {
                file_icon->priv->pix = xfce_themed_icon_load(icon_name, size);
                if(file_icon->priv->pix)
                    loaded_new = TRUE;
            }
        }
    }
    
    /* fallback */
    if(!file_icon->priv->pix) {
        if(xfdesktop_fallback_icon) {
            if(gdk_pixbuf_get_width(xfdesktop_fallback_icon) != size) {
                g_object_unref(G_OBJECT(xfdesktop_fallback_icon));
                xfdesktop_fallback_icon = NULL;
            }
        }
        if(!xfdesktop_fallback_icon) {
            xfdesktop_fallback_icon = gdk_pixbuf_new_from_file_at_size(DATADIR "/pixmaps/xfdesktop/xfdesktop-fallback-icon.png",
                                                                       size,
                                                                       size,
                                                                       NULL);
        }
        
        file_icon->priv->pix = g_object_ref(G_OBJECT(xfdesktop_fallback_icon));
        loaded_new = TRUE;
    }
    
    if(file_icon->priv->pix)
        file_icon->priv->cur_pix_size = size;
    
    if(loaded_new) {
        if(file_icon->priv->info
           && file_icon->priv->info->flags & THUNAR_VFS_FILE_FLAGS_SYMLINK)
        {
            GdkPixbuf *sym_pix;
            gint sym_pix_size = size * 2 / 3;
            
            sym_pix = xfce_themed_icon_load(EMBLEM_SYMLINK, sym_pix_size);
            if(sym_pix) {
                gdk_pixbuf_composite(sym_pix, file_icon->priv->pix,
                                     size - sym_pix_size, size - sym_pix_size,
                                     sym_pix_size, sym_pix_size,
                                     size - sym_pix_size, size - sym_pix_size,
                                     1.0, 1.0, GDK_INTERP_BILINEAR, 255);
                g_object_unref(G_OBJECT(sym_pix));
            }
        }
#ifdef HAVE_LIBEXO
        
        if(file_icon->priv->pix_opacity != 100) {
            GdkPixbuf *tmp = exo_gdk_pixbuf_lucent(file_icon->priv->pix,
                                                   file_icon->priv->pix_opacity);
            g_object_unref(G_OBJECT(file_icon->priv->pix));
            file_icon->priv->pix = tmp;
        }
#endif
    }
    
    return file_icon->priv->pix;
}

static G_CONST_RETURN gchar *
xfdesktop_file_icon_peek_label(XfdesktopIcon *icon)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    
    if(file_icon->priv->volume)
        return thunar_vfs_volume_get_name(file_icon->priv->volume);
    else
        return file_icon->priv->info->display_name;
}

static void
xfdesktop_file_icon_set_position(XfdesktopIcon *icon,
                                 gint16 row,
                                 gint16 col)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    
    file_icon->priv->row = row;
    file_icon->priv->col = col;
    
    g_signal_emit(G_OBJECT(file_icon), __signals[SIG_POS_CHANGED], 0, NULL);
}

static gboolean
xfdesktop_file_icon_get_position(XfdesktopIcon *icon,
                                 gint16 *row,
                                 gint16 *col)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    
    *row = file_icon->priv->row;
    *col = file_icon->priv->col;
    
    return TRUE;
}

static void
xfdesktop_file_icon_set_extents(XfdesktopIcon *icon,
                                const GdkRectangle *extents)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    
    memcpy(&file_icon->priv->extents, extents, sizeof(GdkRectangle));
}

static gboolean
xfdesktop_file_icon_get_extents(XfdesktopIcon *icon,
                                GdkRectangle *extents)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    
    if(file_icon->priv->extents.width > 0
       && file_icon->priv->extents.height > 0)
    {
        memcpy(extents, &file_icon->priv->extents, sizeof(GdkRectangle));
        return TRUE;
    }
    
    return FALSE;
}

static gboolean
xfdesktop_file_icon_is_drop_dest(XfdesktopIcon *icon)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    return (file_icon->priv->info
            && (file_icon->priv->info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY
                || file_icon->priv->info->flags & THUNAR_VFS_FILE_FLAGS_EXECUTABLE));
}

static void
xfdesktop_file_icon_drag_job_error(ThunarVfsJob *job,
                                   GError *error,
                                   gpointer user_data)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(user_data);
    XfdesktopFileIcon *src_file_icon = g_object_get_data(G_OBJECT(job),
                                                         "--xfdesktop-src-file-icon");
    XfdesktopFileUtilsFileop fileop = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(job),
                                                                        "--xfdesktop-file-icon-action"));
    
    g_return_if_fail(file_icon && src_file_icon);
    
    xfdesktop_file_utils_handle_fileop_error(NULL, src_file_icon->priv->info,
                                             file_icon->priv->info, fileop,
                                             error);
}

static ThunarVfsInteractiveJobResponse
xfdesktop_file_icon_interactive_job_ask(ThunarVfsJob *job,
                                        const gchar *message,
                                        ThunarVfsInteractiveJobResponse choices,
                                        gpointer user_data)
{
    return xfdesktop_file_utils_interactive_job_ask(NULL, message, choices);
}

static void
xfdesktop_file_icon_drag_job_finished(ThunarVfsJob *job,
                                      gpointer user_data)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(user_data);
    XfdesktopFileIcon *src_file_icon = g_object_get_data(G_OBJECT(job),
                                                         "--xfdesktop-src-file-icon");
    
    if(g_list_find(file_icon->priv->active_jobs, job)) {
        file_icon->priv->active_jobs = g_list_remove(file_icon->priv->active_jobs,
                                                     job);
    } else
        g_critical("ThunarVfsJob 0x%p not found in active jobs list", job);
    
    if(g_list_find(src_file_icon->priv->active_jobs, job)) {
        src_file_icon->priv->active_jobs = g_list_remove(src_file_icon->priv->active_jobs,
                                                         job);
    } else
        g_critical("ThunarVfsJob 0x%p not found in active jobs list", job);
    
    /* yes, twice is correct */
    g_object_unref(G_OBJECT(job));
    g_object_unref(G_OBJECT(job));
    
    g_object_unref(G_OBJECT(src_file_icon));
    g_object_unref(G_OBJECT(file_icon));
}

static XfdesktopIconDragResult
xfdesktop_file_icon_do_drop_dest(XfdesktopIcon *icon,
                                 XfdesktopIcon *src_icon,
                                 GdkDragAction action)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    XfdesktopFileIcon *src_file_icon = XFDESKTOP_FILE_ICON(src_icon);
    
    DBG("entering");
    
    g_return_val_if_fail(file_icon && src_file_icon,
                         XFDESKTOP_ICON_DRAG_FAILED);
    g_return_val_if_fail(xfdesktop_file_icon_is_drop_dest(icon),
                         XFDESKTOP_ICON_DRAG_FAILED);
    
    if(file_icon->priv->info->flags & THUNAR_VFS_FILE_FLAGS_EXECUTABLE) {
        GList *path_list = g_list_prepend(NULL, src_file_icon->priv->info->path);
        GError *error = NULL;
        gboolean succeeded;
        
        succeeded = thunar_vfs_info_execute(file_icon->priv->info,
                                            file_icon->priv->gscreen,
                                            path_list,
                                            xfce_get_homedir(),
                                            &error);
        g_list_free(path_list);
        
        if(!succeeded) {
            gchar *primary = g_strdup_printf(_("Failed to run \"%s\":"),
                                             file_icon->priv->info->display_name);
            xfce_message_dialog(NULL, _("Run Error"), GTK_STOCK_DIALOG_ERROR,
                                primary, error->message,
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
            g_free(primary);
            g_error_free(error);
            
            return XFDESKTOP_ICON_DRAG_FAILED;
        }
        
        return XFDESKTOP_ICON_DRAG_SUCCEEDED_NO_ACTION;
    } else {
        ThunarVfsJob *job = NULL;
        const gchar *name;
        ThunarVfsPath *dest_path;
        
        name = thunar_vfs_path_get_name(src_file_icon->priv->info->path);
        g_return_val_if_fail(name, XFDESKTOP_ICON_DRAG_FAILED);
        
        dest_path = thunar_vfs_path_relative(file_icon->priv->info->path,
                                             name);
        g_return_val_if_fail(dest_path, XFDESKTOP_ICON_DRAG_FAILED);
        
        switch(action) {
            case GDK_ACTION_MOVE:
                DBG("doing move");
                job = thunar_vfs_move_file(src_file_icon->priv->info->path,
                                           dest_path, NULL);
                break;
            
            case GDK_ACTION_COPY:
                DBG("doing copy");
                job = thunar_vfs_copy_file(src_file_icon->priv->info->path,
                                           dest_path, NULL);
                break;
            
            case GDK_ACTION_LINK:
                DBG("doing link");
                job = thunar_vfs_link_file(src_file_icon->priv->info->path,
                                           dest_path, NULL);
                break;
            
            default:
                g_warning("Unsupported drag action: %d", action);
        }
        
        thunar_vfs_path_unref(dest_path);
        
        if(job) {
            DBG("got job, action initiated");
            
            /* ensure they aren't destroyed until the job is finished */
            g_object_ref(G_OBJECT(src_file_icon));
            g_object_ref(G_OBJECT(file_icon));
            
            g_object_set_data(G_OBJECT(job), "--xfdesktop-file-icon-callback",
                              G_CALLBACK(xfdesktop_file_icon_drag_job_finished));
            g_object_set_data(G_OBJECT(job), "--xfdesktop-file-icon-data", icon);
            file_icon->priv->active_jobs = g_list_prepend(file_icon->priv->active_jobs,
                                                          job);
            src_file_icon->priv->active_jobs = g_list_prepend(src_file_icon->priv->active_jobs,
                                                              job);
            
            g_object_set_data(G_OBJECT(job), "--xfdesktop-src-file-icon",
                              src_file_icon);
            g_object_set_data(G_OBJECT(job), "--xfdesktop-file-icon-action",
                              GINT_TO_POINTER(action == GDK_ACTION_MOVE
                                              ? XFDESKTOP_FILE_UTILS_FILEOP_MOVE
                                              : (action == GDK_ACTION_COPY
                                                 ? XFDESKTOP_FILE_UTILS_FILEOP_COPY
                                                 : XFDESKTOP_FILE_UTILS_FILEOP_LINK)));
            g_signal_connect(G_OBJECT(job), "error",
                             G_CALLBACK(xfdesktop_file_icon_drag_job_error),
                             file_icon);
            g_signal_connect(G_OBJECT(job), "ask",
                             G_CALLBACK(xfdesktop_file_icon_interactive_job_ask),
                             file_icon);
            g_signal_connect(G_OBJECT(job), "finished",
                             G_CALLBACK(xfdesktop_file_icon_drag_job_finished),
                             file_icon);
            
            g_object_ref(G_OBJECT(job));
            
            return XFDESKTOP_ICON_DRAG_SUCCEEDED_NO_ACTION;
        } else
            return XFDESKTOP_ICON_DRAG_FAILED;
    }
    
    return XFDESKTOP_ICON_DRAG_FAILED;
}

static G_CONST_RETURN gchar *
xfdesktop_file_icon_peek_tooltip(XfdesktopIcon *icon)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    
    if(!file_icon->priv->info)
       return NULL;  /* FIXME: implement something here */
    
    if(!file_icon->priv->tooltip) {
        gchar mod[64], *kind, sizebuf[64], *size;
        struct tm *tm = localtime(&file_icon->priv->info->mtime);

        strftime(mod, 64, "%Y-%m-%d %H:%M:%S", tm);
        kind = xfdesktop_file_utils_get_file_kind(file_icon->priv->info, NULL);
        thunar_vfs_humanize_size(file_icon->priv->info->size, sizebuf, 64);
        size = g_strdup_printf(_("%s (%" G_GINT64_FORMAT " Bytes)"), sizebuf,
                              (gint64)file_icon->priv->info->size);
        
        file_icon->priv->tooltip = g_strdup_printf(_("Kind: %s\nModified:%s\nSize: %s"),
                                                   kind, mod, size);
        
        g_free(kind);
        g_free(size);
    }
    
    return file_icon->priv->tooltip;
}

static void
xfdesktop_delete_file_error(ThunarVfsJob *job,
                            GError *error,
                            gpointer user_data)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(user_data);
    gchar *primary = g_strdup_printf("There was an error deleting \"%s\":",
                                     icon->priv->info->display_name);
                                     
    xfce_message_dialog(NULL, _("Error"), GTK_STOCK_DIALOG_ERROR, primary,
                        error->message, GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                        NULL);
    
    g_free(primary);
}

static void
xfdesktop_delete_file_finished(ThunarVfsJob *job,
                               gpointer user_data)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(user_data);
    
    if(g_list_find(icon->priv->active_jobs, job))
        icon->priv->active_jobs = g_list_remove(icon->priv->active_jobs, job);
    else
        g_critical("ThunarVfsJob 0x%p not found in active jobs list", job);
    
    g_object_unref(G_OBJECT(job));
    g_object_unref(G_OBJECT(icon));
}

void
xfdesktop_file_icon_delete_file(XfdesktopFileIcon *icon)
{
    ThunarVfsJob *job;
    
    g_return_if_fail(!icon->priv->volume);
    
    job = thunar_vfs_unlink_file(icon->priv->info->path, NULL);
    
    g_object_set_data(G_OBJECT(job), "--xfdesktop-file-icon-callback",
                      G_CALLBACK(xfdesktop_delete_file_finished));
    g_object_set_data(G_OBJECT(job), "--xfdesktop-file-icon-data", icon);
    icon->priv->active_jobs = g_list_prepend(icon->priv->active_jobs, job);
    
    g_signal_connect(G_OBJECT(job), "error",
                     G_CALLBACK(xfdesktop_delete_file_error), icon);
    g_signal_connect(G_OBJECT(job), "finished",
                     G_CALLBACK(xfdesktop_delete_file_finished), icon);
    
    g_object_ref(G_OBJECT(icon));
}

gboolean
xfdesktop_file_icon_rename_file(XfdesktopFileIcon *icon,
                                const gchar *new_name)
{
    GError *error = NULL;
    
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon) && new_name && *new_name
                         && !icon->priv->volume,
                         FALSE);
    
    if(!thunar_vfs_info_rename(icon->priv->info, new_name, &error)) {
        gchar *primary = g_strdup_printf(_("Failed to rename \"%s\" to \"%s\":"),
                                         icon->priv->info->display_name,
                                         new_name);
        xfce_message_dialog(NULL, _("Error"), GTK_STOCK_DIALOG_ERROR,
                            primary, error->message,
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        g_free(primary);
        g_error_free(error);
        
        return FALSE;
    }
    
    return TRUE;
}

G_CONST_RETURN ThunarVfsInfo *
xfdesktop_file_icon_peek_info(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), NULL);
    return icon->priv->info;
}

void
xfdesktop_file_icon_update_info(XfdesktopFileIcon *icon,
                                ThunarVfsInfo *info)
{
    gboolean label_changed = TRUE;
    
    g_return_if_fail(XFDESKTOP_IS_ICON(icon) && info);
    
    if(icon->priv->info) {
        if(!strcmp(icon->priv->info->display_name, info->display_name))
            label_changed = FALSE;
        thunar_vfs_info_unref(icon->priv->info);
    }
    icon->priv->info = thunar_vfs_info_ref(info);
    
    if(label_changed)
        xfdesktop_icon_label_changed(XFDESKTOP_ICON(icon));
    
    /* not really easy to check if this changed or not, so just invalidate it */
    if(icon->priv->pix) {
        g_object_unref(G_OBJECT(icon->priv->pix));
        icon->priv->pix = NULL;
    }
    xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(icon));
}

GList *
xfdesktop_file_icon_list_to_path_list(GList *icon_list)
{
    GList *path_list = NULL, *l;
    
    for(l = icon_list; l; l = l->next) {
        path_list = g_list_prepend(path_list,
                                   thunar_vfs_path_ref(XFDESKTOP_FILE_ICON(l->data)->priv->info->path));
    }
    
    return g_list_reverse(path_list);
}

ThunarVfsVolume *
xfdesktop_file_icon_peek_volume(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), NULL);
    return icon->priv->volume;
}



/* thunar extension interface stuff: ThunarxFileInfo implementation */

#ifdef HAVE_THUNARX

static void
xfdesktop_file_icon_tfi_init(ThunarxFileInfoIface *iface)
{
    iface->get_name = xfdesktop_file_icon_tfi_get_name;
    iface->get_uri = xfdesktop_file_icon_tfi_get_uri;
    iface->get_parent_uri = xfdesktop_file_icon_tfi_get_parent_uri;
    iface->get_uri_scheme = xfdesktop_file_icon_tfi_get_uri_scheme;
    iface->get_mime_type = xfdesktop_file_icon_tfi_get_mime_type;
    iface->has_mime_type = xfdesktop_file_icon_tfi_has_mime_type;
    iface->is_directory = xfdesktop_file_icon_tfi_is_directory;
    iface->get_vfs_info = xfdesktop_file_icon_tfi_get_vfs_info;
}

static gchar *
xfdesktop_file_icon_tfi_get_name(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    return g_strdup(thunar_vfs_path_get_name(icon->priv->info->path));
}

static gchar *
xfdesktop_file_icon_tfi_get_uri(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    gchar buf[PATH_MAX];
    
    g_return_val_if_fail(thunar_vfs_path_to_uri(icon->priv->info->path,
                                                buf, PATH_MAX, NULL) > 0,
                         NULL);
    
    return g_strdup(buf);
}

static gchar *
xfdesktop_file_icon_tfi_get_parent_uri(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    ThunarVfsPath *parent = thunar_vfs_path_get_parent(icon->priv->info->path);
    gchar buf[PATH_MAX];
    
    if(G_UNLIKELY(!parent))
        return NULL;
    
    g_return_val_if_fail(thunar_vfs_path_to_uri(parent, buf, PATH_MAX,
                                                NULL) > 0,
                         NULL);
    
    return g_strdup(buf);
}

static gchar *
xfdesktop_file_icon_tfi_get_uri_scheme(ThunarxFileInfo *file_info)
{
    return g_strdup("file");  /* FIXME: safe bet? */
}
    
static gchar *
xfdesktop_file_icon_tfi_get_mime_type(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    
    if(!icon->priv->info->mime_info)
        return NULL;
    
    return g_strdup(thunar_vfs_mime_info_get_name(icon->priv->info->mime_info));
}

static gboolean
xfdesktop_file_icon_tfi_has_mime_type(ThunarxFileInfo *file_info,
                                      const gchar *mime_type)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    ThunarVfsMimeDatabase *mime_db;
    GList *mime_infos, *l;
    ThunarVfsMimeInfo *minfo;
    gboolean has_type = FALSE;
    
    if(!icon->priv->info->mime_info)
        return FALSE;
    
    mime_db = thunar_vfs_mime_database_get_default();
    
    mime_infos = thunar_vfs_mime_database_get_infos_for_info(mime_db,
                                                             icon->priv->info->mime_info);
    for(l = mime_infos; l; l = l->next) {
        minfo = (ThunarVfsMimeInfo *)l->data;
        if(!g_ascii_strcasecmp(mime_type, thunar_vfs_mime_info_get_name(minfo))) {
            has_type = TRUE;
            break;
        }
    }
    thunar_vfs_mime_info_list_free(mime_infos);
    
    g_object_unref(G_OBJECT(mime_db));
    
    return has_type;
}

static gboolean
xfdesktop_file_icon_tfi_is_directory(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    return (icon->priv->info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY);
}

static ThunarVfsInfo *
xfdesktop_file_icon_tfi_get_vfs_info(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    return thunar_vfs_info_ref(icon->priv->info);
}

#endif  /* HAVE_THUNARX */
