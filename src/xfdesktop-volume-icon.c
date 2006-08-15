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
#include "xfdesktop-file-properties-dialog.h"
#include "xfdesktop-volume-icon.h"

struct _XfdesktopVolumeIconPrivate
{
    GdkPixbuf *pix;
    gchar *tooltip;
    gint cur_pix_size;
    ThunarVfsVolume *volume;
    ThunarVfsInfo *info;
    GdkScreen *gscreen;
};

static void xfdesktop_volume_icon_finalize(GObject *obj);

static void xfdesktop_volume_icon_icon_init(XfdesktopIconIface *iface);
static GdkPixbuf *xfdesktop_volume_icon_peek_pixbuf(XfdesktopIcon *icon,
                                                    gint size);
static G_CONST_RETURN gchar *xfdesktop_volume_icon_peek_label(XfdesktopIcon *icon);
static G_CONST_RETURN gchar *xfdesktop_volume_icon_peek_tooltip(XfdesktopIcon *icon);
static gboolean xfdesktop_volume_icon_is_drop_dest(XfdesktopIcon *icon);
static XfdesktopIconDragResult xfdesktop_volume_icon_do_drop_dest(XfdesktopIcon *icon,
                                                                  XfdesktopIcon *src_icon,
                                                                  GdkDragAction action);
static GtkWidget *xfdesktop_volume_icon_get_popup_menu(XfdesktopIcon *icon);

static G_CONST_RETURN ThunarVfsInfo *xfdesktop_volume_icon_peek_info(XfdesktopFileIcon *icon);
static void xfdesktop_volume_icon_update_info(XfdesktopFileIcon *icon,
                                              ThunarVfsInfo *info);

#ifdef HAVE_THUNARX
static void xfdesktop_volume_icon_tfi_init(ThunarxFileInfoIface *iface);
#endif

static inline void xfdesktop_volume_icon_invalidate_pixbuf(XfdesktopVolumeIcon *icon);


#ifdef HAVE_THUNARX
G_DEFINE_TYPE_EXTENDED(XfdesktopVolumeIcon, xfdesktop_volume_icon,
                       XFDESKTOP_TYPE_FILE_ICON, 0,
                       G_IMPLEMENT_INTERFACE(XFDESKTOP_TYPE_ICON,
                                             xfdesktop_volume_icon_icon_init)
                       G_IMPLEMENT_INTERFACE(THUNARX_TYPE_FILE_INFO,
                                             xfdesktop_volume_icon_tfi_init)
                       )
#else
G_DEFINE_TYPE_EXTENDED(XfdesktopVolumeIcon, xfdesktop_volume_icon,
                       XFDESKTOP_TYPE_FILE_ICON, 0,
                       G_IMPLEMENT_INTERFACE(XFDESKTOP_TYPE_ICON,
                                             xfdesktop_volume_icon_icon_init)
                       )
#endif



static void
xfdesktop_volume_icon_class_init(XfdesktopVolumeIconClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    XfdesktopFileIconClass *file_icon_class = (XfdesktopFileIconClass *)klass;
    
    g_type_class_add_private(klass, sizeof(XfdesktopVolumeIconClass));
    
    gobject_class->finalize = xfdesktop_volume_icon_finalize;
    
    file_icon_class->peek_info = xfdesktop_volume_icon_peek_info;
    file_icon_class->update_info = xfdesktop_volume_icon_update_info;
    file_icon_class->can_rename_file = (gboolean (*)(XfdesktopFileIcon *))gtk_false;
    file_icon_class->can_delete_file = (gboolean (*)(XfdesktopFileIcon *))gtk_false;
}

static void
xfdesktop_volume_icon_init(XfdesktopVolumeIcon *icon)
{
    icon->priv = G_TYPE_INSTANCE_GET_PRIVATE(icon, XFDESKTOP_TYPE_VOLUME_ICON,
                                             XfdesktopVolumeIconPrivate);
}

static void
xfdesktop_volume_icon_finalize(GObject *obj)
{
    XfdesktopVolumeIcon *icon = XFDESKTOP_VOLUME_ICON(obj);
    GtkIconTheme *itheme = gtk_icon_theme_get_for_screen(icon->priv->gscreen);
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(itheme),
                                         G_CALLBACK(xfdesktop_volume_icon_invalidate_pixbuf),
                                         icon);
    
    if(icon->priv->pix)
        g_object_unref(G_OBJECT(icon->priv->pix));
    
    if(icon->priv->info)
        thunar_vfs_info_unref(icon->priv->info);
    
    if(icon->priv->volume)
        g_object_unref(G_OBJECT(icon->priv->volume));
    
    if(icon->priv->tooltip)
        g_free(icon->priv->tooltip);
    
    G_OBJECT_CLASS(xfdesktop_volume_icon_parent_class)->finalize(obj);
}

static void
xfdesktop_volume_icon_icon_init(XfdesktopIconIface *iface)
{
    iface->peek_pixbuf = xfdesktop_volume_icon_peek_pixbuf;
    iface->peek_label = xfdesktop_volume_icon_peek_label;
    iface->peek_tooltip = xfdesktop_volume_icon_peek_tooltip;
    iface->is_drop_dest = xfdesktop_volume_icon_is_drop_dest;
    iface->do_drop_dest = xfdesktop_volume_icon_do_drop_dest;
    iface->get_popup_menu = xfdesktop_volume_icon_get_popup_menu;
}

#ifdef HAVE_THUNARX
static void
xfdesktop_volume_icon_tfi_init(ThunarxFileInfoIface *iface)
{
    iface->get_name = xfdesktop_thunarx_file_info_get_name;
    iface->get_uri = xfdesktop_thunarx_file_info_get_uri;
    iface->get_parent_uri = xfdesktop_thunarx_file_info_get_parent_uri;
    iface->get_uri_scheme = xfdesktop_thunarx_file_info_get_uri_scheme_file;
    iface->get_mime_type = xfdesktop_thunarx_file_info_get_mime_type;
    iface->has_mime_type = xfdesktop_thunarx_file_info_has_mime_type;
    iface->is_directory = xfdesktop_thunarx_file_info_is_directory;
    iface->get_vfs_info = xfdesktop_thunarx_file_info_get_vfs_info;
}
#endif  /* HAVE_THUNARX */

static inline void
xfdesktop_volume_icon_invalidate_pixbuf(XfdesktopVolumeIcon *icon)
{
    if(icon->priv->pix) {
        g_object_unref(G_OBJECT(icon->priv->pix));
        icon->priv->pix = NULL;
    }
}


static GdkPixbuf *
xfdesktop_volume_icon_peek_pixbuf(XfdesktopIcon *icon,
                                  gint size)
{
    XfdesktopVolumeIcon *file_icon = XFDESKTOP_VOLUME_ICON(icon);
    const gchar *icon_name;
    
    g_return_val_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon), NULL);
    
    if(size != file_icon->priv->cur_pix_size)
        xfdesktop_volume_icon_invalidate_pixbuf(file_icon);
    
    if(!file_icon->priv->pix) {
        icon_name = thunar_vfs_volume_lookup_icon_name(file_icon->priv->volume,
                                                       gtk_icon_theme_get_default());
        
        file_icon->priv->pix = xfdesktop_file_utils_get_file_icon(icon_name,
                                                                  file_icon->priv->info,
                                                                  size,
                                                                  NULL,
                                                                  100);
        
        file_icon->priv->cur_pix_size = size;
    }
    
    return file_icon->priv->pix;
}

G_CONST_RETURN gchar *
xfdesktop_volume_icon_peek_label(XfdesktopIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon), NULL);
    return thunar_vfs_volume_get_name(XFDESKTOP_VOLUME_ICON(icon)->priv->volume);
}

static gboolean
xfdesktop_volume_icon_is_drop_dest(XfdesktopIcon *icon)
{
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(icon);
    /* FIXME: return TRUE, and attempt to mount later if it isn't? */
    return thunar_vfs_volume_is_mounted(volume_icon->priv->volume);
}

static void
xfdesktop_volume_icon_drag_job_error(ThunarVfsJob *job,
                                     GError *error,
                                     gpointer user_data)
{
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(user_data);
    XfdesktopFileIcon *src_file_icon = g_object_get_data(G_OBJECT(job),
                                                         "--xfdesktop-src-file-icon");
    XfdesktopFileUtilsFileop fileop = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(job),
                                                                        "--xfdesktop-file-icon-action"));
    const ThunarVfsInfo *src_info = xfdesktop_file_icon_peek_info(src_file_icon);
    
    g_return_if_fail(volume_icon && src_file_icon);
    
    xfdesktop_file_utils_handle_fileop_error(NULL, src_info,
                                             volume_icon->priv->info,
                                             fileop, error);
}

static ThunarVfsInteractiveJobResponse
xfdesktop_volume_icon_interactive_job_ask(ThunarVfsJob *job,
                                          const gchar *message,
                                          ThunarVfsInteractiveJobResponse choices,
                                          gpointer user_data)
{
    return xfdesktop_file_utils_interactive_job_ask(NULL, message, choices);
}

static void
xfdesktop_volume_icon_drag_job_finished(ThunarVfsJob *job,
                                        gpointer user_data)
{
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(user_data);
    XfdesktopFileIcon *src_file_icon = g_object_get_data(G_OBJECT(job),
                                                         "--xfdesktop-src-file-icon");
    
    if(!xfdesktop_file_icon_remove_active_job(XFDESKTOP_FILE_ICON(volume_icon), job))
        g_critical("ThunarVfsJob 0x%p not found in active jobs list", job);
    
    if(!xfdesktop_file_icon_remove_active_job(src_file_icon, job))
        g_critical("ThunarVfsJob 0x%p not found in active jobs list", job);
    
    g_object_unref(G_OBJECT(job));
    
    g_object_unref(G_OBJECT(src_file_icon));
    g_object_unref(G_OBJECT(volume_icon));
}

static XfdesktopIconDragResult
xfdesktop_volume_icon_do_drop_dest(XfdesktopIcon *icon,
                                 XfdesktopIcon *src_icon,
                                 GdkDragAction action)
{
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(icon);
    XfdesktopFileIcon *src_file_icon = XFDESKTOP_FILE_ICON(src_icon);
    const ThunarVfsInfo *src_info;
    ThunarVfsJob *job = NULL;
    const gchar *name;
    ThunarVfsPath *dest_path;
    
    DBG("entering");
    
    g_return_val_if_fail(volume_icon && src_file_icon,
                         XFDESKTOP_ICON_DRAG_FAILED);
    g_return_val_if_fail(xfdesktop_volume_icon_is_drop_dest(icon),
                         XFDESKTOP_ICON_DRAG_FAILED);
    
    src_info = xfdesktop_file_icon_peek_info(src_file_icon);
    if(!src_info)
        return XFDESKTOP_ICON_DRAG_FAILED;
    
    name = thunar_vfs_path_get_name(src_info->path);
    g_return_val_if_fail(name, XFDESKTOP_ICON_DRAG_FAILED);
        
    dest_path = thunar_vfs_path_relative(volume_icon->priv->info->path,
                                         name);
    g_return_val_if_fail(dest_path, XFDESKTOP_ICON_DRAG_FAILED);
    
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
        g_object_ref(G_OBJECT(volume_icon));
        
        g_object_set_data(G_OBJECT(job), "--xfdesktop-file-icon-callback",
                          G_CALLBACK(xfdesktop_volume_icon_drag_job_finished));
        g_object_set_data(G_OBJECT(job), "--xfdesktop-file-icon-data", icon);
        xfdesktop_file_icon_add_active_job(XFDESKTOP_FILE_ICON(volume_icon),
                                           job);
        xfdesktop_file_icon_add_active_job(src_file_icon, job);
        
        g_object_set_data(G_OBJECT(job), "--xfdesktop-src-file-icon",
                          src_file_icon);
        g_object_set_data(G_OBJECT(job), "--xfdesktop-file-icon-action",
                          GINT_TO_POINTER(action == GDK_ACTION_MOVE
                                          ? XFDESKTOP_FILE_UTILS_FILEOP_MOVE
                                          : (action == GDK_ACTION_COPY
                                             ? XFDESKTOP_FILE_UTILS_FILEOP_COPY
                                             : XFDESKTOP_FILE_UTILS_FILEOP_LINK)));
        g_signal_connect(G_OBJECT(job), "error",
                         G_CALLBACK(xfdesktop_volume_icon_drag_job_error),
                         volume_icon);
        g_signal_connect(G_OBJECT(job), "ask",
                         G_CALLBACK(xfdesktop_volume_icon_interactive_job_ask),
                         volume_icon);
        g_signal_connect(G_OBJECT(job), "finished",
                         G_CALLBACK(xfdesktop_volume_icon_drag_job_finished),
                         volume_icon);
        
        g_object_ref(G_OBJECT(job));
        
        return XFDESKTOP_ICON_DRAG_SUCCEEDED_NO_ACTION;
    } else
        return XFDESKTOP_ICON_DRAG_FAILED;
    
    return XFDESKTOP_ICON_DRAG_FAILED;
}

static G_CONST_RETURN gchar *
xfdesktop_volume_icon_peek_tooltip(XfdesktopIcon *icon)
{
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(icon);
    
    if(!volume_icon->priv->info)
        return NULL;
    
    /* FIXME: something different? */
    
    if(!volume_icon->priv->tooltip) {
        gchar mod[64], *kind, sizebuf[64], *size;
        struct tm *tm = localtime(&volume_icon->priv->info->mtime);

        strftime(mod, 64, "%Y-%m-%d %H:%M:%S", tm);
        kind = xfdesktop_file_utils_get_file_kind(volume_icon->priv->info, NULL);
        thunar_vfs_humanize_size(volume_icon->priv->info->size, sizebuf, 64);
        size = g_strdup_printf(_("%s (%" G_GINT64_FORMAT " Bytes)"), sizebuf,
                              (gint64)volume_icon->priv->info->size);
        
        volume_icon->priv->tooltip = g_strdup_printf(_("Kind: %s\nModified:%s\nSize: %s"),
                                                   kind, mod, size);
        
        g_free(kind);
        g_free(size);
    }
    
    return volume_icon->priv->tooltip;
}

static void
xfdesktop_volume_icon_menu_toggle_mount(GtkWidget *widget,
                                        gpointer user_data)
{
    XfdesktopVolumeIcon *icon = XFDESKTOP_VOLUME_ICON(user_data);
    GtkWidget *toplevel = NULL;
    GError *error = NULL;
    gboolean is_mount;
    
    /* FIXME */
    /* toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view)); */
    
    is_mount = !thunar_vfs_volume_is_mounted(icon->priv->volume);
    
    if(!is_mount)
        thunar_vfs_volume_unmount(icon->priv->volume, toplevel, &error);
    else {
        if(thunar_vfs_volume_mount(icon->priv->volume, toplevel, &error)) {
            const ThunarVfsInfo *info = icon->priv->info;
            ThunarVfsPath *new_path = thunar_vfs_volume_get_mount_point(icon->priv->volume);
            
            if(!info || !thunar_vfs_path_equal(info->path, new_path)) {
                ThunarVfsInfo *new_info = thunar_vfs_info_new_for_path(new_path,
                                                                       NULL);
                if(new_info) {
                    xfdesktop_file_icon_update_info(XFDESKTOP_FILE_ICON(icon),
                                                    new_info);
                    thunar_vfs_info_unref(new_info);
                }
            }
        }
    }
    
    if(error) {
        gchar *primary = g_markup_printf_escaped(is_mount ? _("Unable to mount \"%s\":")
                                                          : _("Unable to unmount \"%s\":"),
                                                 thunar_vfs_volume_get_name(icon->priv->volume));
        xfce_message_dialog(toplevel ? GTK_WINDOW(toplevel) : NULL,
                            is_mount ? _("Mount Failed") : _("Unmount Failed"),
                            GTK_STOCK_DIALOG_ERROR, primary, error->message,
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        g_free(primary);
        g_error_free(error);
    }
}

static void
xfdesktop_volume_icon_menu_eject(GtkWidget *widget,
                                 gpointer user_data)
{
    XfdesktopVolumeIcon *icon = XFDESKTOP_VOLUME_ICON(user_data);
    GtkWidget *toplevel = NULL;
    GError *error = NULL;
    
    /* FIXME */
    /* toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view)); */
    
    if(!thunar_vfs_volume_eject(icon->priv->volume, toplevel, &error)) {
        gchar *primary = g_markup_printf_escaped(_("Unable to eject \"%s\":"),
                                                 thunar_vfs_volume_get_name(icon->priv->volume));
        xfce_message_dialog(toplevel ? GTK_WINDOW(toplevel) : NULL,
                            _("Eject Failed"), GTK_STOCK_DIALOG_ERROR,
                            primary, error->message,
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        g_free(primary);
        g_error_free(error);
    }
}

static void
xfdesktop_volume_icon_menu_properties(GtkWidget *widget,
                                      gpointer user_data)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(user_data);        
    xfdesktop_file_properties_dialog_show(NULL, icon, NULL);
}

static void
xfdesktop_volume_icon_menu_open(GtkWidget *widget,
                                gpointer user_data)
{
    XfdesktopVolumeIcon *icon = XFDESKTOP_VOLUME_ICON(user_data);
    const ThunarVfsInfo *info;
    ThunarVfsVolume *volume = (ThunarVfsVolume *)xfdesktop_volume_icon_peek_volume(icon);
    GError *error = NULL;
    ThunarVfsPath *new_path = NULL;
    
    info = xfdesktop_file_icon_peek_info(XFDESKTOP_FILE_ICON(icon));
    
    if(!thunar_vfs_volume_is_mounted(volume)) {
        /* FIXME: need toplevel parent here */
        if(!thunar_vfs_volume_mount(volume, NULL, &error)) {
            gchar *primary = g_markup_printf_escaped(_("Unable to mount \"%s\":"),
                                                     thunar_vfs_volume_get_name(volume));
            GtkWidget *dlg = xfce_message_dialog_new(NULL, _("Mount Failed"),
                                                     GTK_STOCK_DIALOG_ERROR,
                                                     primary,
                                                     error ? error->message
                                                           : _("Unknown error."),
                                                     GTK_STOCK_CLOSE,
                                                     GTK_RESPONSE_ACCEPT, NULL);
            gtk_window_set_screen(GTK_WINDOW(dlg), icon->priv->gscreen);
            gtk_dialog_run(GTK_DIALOG(dlg));
            gtk_widget_destroy(dlg);
            g_free(primary);
            g_error_free(error);
        }
    }
    
    new_path = thunar_vfs_volume_get_mount_point(volume);
        
    if(new_path && (!info
                    || !thunar_vfs_path_equal(info->path, new_path)))
    {
        ThunarVfsInfo *new_info = thunar_vfs_info_new_for_path(new_path,
                                                               NULL);
        if(new_info) {
            xfdesktop_file_icon_update_info(XFDESKTOP_FILE_ICON(icon),
                                            new_info);
            thunar_vfs_info_unref(new_info);
        }
    }
    
    info = xfdesktop_file_icon_peek_info(XFDESKTOP_FILE_ICON(icon));
    if(!info || !new_path) {
        /* if |new_path| is NULL, but |info| isn't, it's possible we have
         * a stale |info|, and we shouldn't continue */
        return;
    }
    
    
    if(!xfdesktop_file_utils_launch_external(info, icon->priv->gscreen)) {
        gchar *primary = g_markup_printf_escaped(_("Unable to launch \"%s\":"),
                                                 info->display_name);
        GtkWidget *dlg = xfce_message_dialog_new(NULL, _("Launch Error"),
                                                 GTK_STOCK_DIALOG_ERROR,
                                                 primary,
                                                 _("The associated application could not be found or executed."),
                                                 GTK_STOCK_CLOSE,
                                                 GTK_RESPONSE_ACCEPT, NULL);
        gtk_window_set_screen(GTK_WINDOW(dlg), icon->priv->gscreen);
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
        g_free(primary);
    }
}

static GtkWidget *
xfdesktop_volume_icon_get_popup_menu(XfdesktopIcon *icon)
{
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(icon);
    ThunarVfsVolume *volume = volume_icon->priv->volume;
    GtkWidget *menu, *mi, *img;
    
    menu = gtk_menu_new();
    
    img = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
    gtk_widget_show(img);
    mi = gtk_image_menu_item_new_with_mnemonic(_("_Open"));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_volume_icon_menu_open), icon);
    
    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    
    mi = gtk_image_menu_item_new_with_mnemonic(_("_Mount Volume"));
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    if(thunar_vfs_volume_is_mounted(volume))
        gtk_widget_set_sensitive(mi, FALSE);
    else {
        g_signal_connect(G_OBJECT(mi), "activate",
                         G_CALLBACK(xfdesktop_volume_icon_menu_toggle_mount),
                         icon);
    }
    
    mi = gtk_image_menu_item_new_with_mnemonic(_("_Unmount Volume"));
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    if(!thunar_vfs_volume_is_mounted(volume))
        gtk_widget_set_sensitive(mi, FALSE);
    else {
        g_signal_connect(G_OBJECT(mi), "activate",
                         G_CALLBACK(xfdesktop_volume_icon_menu_toggle_mount),
                         icon);
    }
    
    if(thunar_vfs_volume_is_disc(volume)
       && thunar_vfs_volume_is_ejectable(volume))
    {
        mi = gtk_image_menu_item_new_with_mnemonic(_("E_ject Volume"));
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        g_signal_connect(G_OBJECT(mi), "activate",
                         G_CALLBACK(xfdesktop_volume_icon_menu_eject),
                         icon);
    }
    
    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    
    img = gtk_image_new_from_stock(GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_MENU);
    gtk_widget_show(img);
    mi = gtk_image_menu_item_new_with_mnemonic(_("_Properties..."));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    if(!volume_icon->priv->info)
        gtk_widget_set_sensitive(mi, FALSE);
    else {
        g_signal_connect(G_OBJECT(mi), "activate",
                         G_CALLBACK(xfdesktop_volume_icon_menu_properties),
                         icon);
    }
    
    return menu;
}


static G_CONST_RETURN ThunarVfsInfo *
xfdesktop_volume_icon_peek_info(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon), NULL);
    return XFDESKTOP_VOLUME_ICON(icon)->priv->info;
}

static void
xfdesktop_volume_icon_update_info(XfdesktopFileIcon *icon,
                                  ThunarVfsInfo *info)
{
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(icon);
    gboolean label_changed = TRUE;
    
    g_return_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon));
    
    if(volume_icon->priv->info) {
        if(info && !strcmp(volume_icon->priv->info->display_name,
                           info->display_name))
        {
            label_changed = FALSE;
        }
    
        thunar_vfs_info_unref(volume_icon->priv->info);
    }
    
    volume_icon->priv->info = info ? thunar_vfs_info_ref(info) : NULL;
    
    if(label_changed)
        xfdesktop_icon_label_changed(XFDESKTOP_ICON(icon));
    
    /* not really easy to check if this changed or not, so just invalidate it */
    xfdesktop_volume_icon_invalidate_pixbuf(volume_icon);
    xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(icon));
}

XfdesktopVolumeIcon *
xfdesktop_volume_icon_new(ThunarVfsVolume *volume,
                          GdkScreen *screen)
{
    XfdesktopVolumeIcon *volume_icon;
    ThunarVfsPath *path;
    
    g_return_val_if_fail(THUNAR_VFS_IS_VOLUME(volume), NULL);
    
    volume_icon = g_object_new(XFDESKTOP_TYPE_VOLUME_ICON, NULL);
    volume_icon->priv->volume = g_object_ref(G_OBJECT(volume));
    volume_icon->priv->gscreen = screen;
    
    path = thunar_vfs_volume_get_mount_point(volume);
    if(path) {
        volume_icon->priv->info = thunar_vfs_info_new_for_path(path, NULL);
        /* |path| is owned by |volume|, do not free */
    }
    
    return volume_icon;
}

G_CONST_RETURN ThunarVfsVolume *
xfdesktop_volume_icon_peek_volume(XfdesktopVolumeIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon), NULL);
    return icon->priv->volume;
}
