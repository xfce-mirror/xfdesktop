/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006      Brian Tarricone, <bjt23@cornell.edu>
 *  Copyright(c) 2006      Benedikt Meurer, <benny@xfce.org>
 *  Copyright(c) 2010-2011 Jannis Pohlmann, <jannis@xfce.org>
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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include <gio/gio.h>

#include <libxfce4ui/libxfce4ui.h>

#ifdef HAVE_THUNARX
#include <thunarx/thunarx.h>
#endif

#ifdef HAVE_LIBNOTIFY
#include "xfdesktop-notify.h"
#endif

#include <exo/exo.h>

#include "xfdesktop-common.h"
#include "xfdesktop-file-utils.h"
#include "xfdesktop-volume-icon.h"

struct _XfdesktopVolumeIconPrivate
{
    gchar *tooltip;
    gchar *label;
    GVolume *volume;
    GFileInfo *file_info;
    GFileInfo *filesystem_info;
    GFile *file;
    GdkScreen *gscreen;

    guint changed_timeout_id;
    guint changed_timeout_count;
};

static void xfdesktop_volume_icon_finalize(GObject *obj);

static GdkPixbuf *xfdesktop_volume_icon_peek_pixbuf(XfdesktopIcon *icon,
                                                    gint width, gint height);
static const gchar *xfdesktop_volume_icon_peek_label(XfdesktopIcon *icon);
static gchar *xfdesktop_volume_icon_get_identifier(XfdesktopIcon *icon);
static GdkPixbuf *xfdesktop_volume_icon_peek_tooltip_pixbuf(XfdesktopIcon *icon,
                                                            gint width, gint height);
static const gchar *xfdesktop_volume_icon_peek_tooltip(XfdesktopIcon *icon);
static GdkDragAction xfdesktop_volume_icon_get_allowed_drag_actions(XfdesktopIcon *icon);
static GdkDragAction xfdesktop_volume_icon_get_allowed_drop_actions(XfdesktopIcon *icon,
                                                                    GdkDragAction *suggested_action);
static gboolean xfdesktop_volume_icon_do_drop_dest(XfdesktopIcon *icon,
                                                   XfdesktopIcon *src_icon,
                                                   GdkDragAction action);
static gboolean xfdesktop_volume_icon_populate_context_menu(XfdesktopIcon *icon,
                                                            GtkWidget *menu);

static GFileInfo *xfdesktop_volume_icon_peek_file_info(XfdesktopFileIcon *icon);
static GFileInfo *xfdesktop_volume_icon_peek_filesystem_info(XfdesktopFileIcon *icon);
static GFile *xfdesktop_volume_icon_peek_file(XfdesktopFileIcon *icon);
static void xfdesktop_volume_icon_update_file_info(XfdesktopFileIcon *icon,
                                                   GFileInfo *info);
static gboolean xfdesktop_volume_icon_activated(XfdesktopIcon *icon);
static gboolean volume_icon_changed_timeout(XfdesktopVolumeIcon *icon);
static void xfdesktop_volume_icon_changed(GVolume *volume, 
                                          XfdesktopVolumeIcon *volume_icon);

#ifdef HAVE_THUNARX
static void xfdesktop_volume_icon_tfi_init(ThunarxFileInfoIface *iface);

G_DEFINE_TYPE_EXTENDED(XfdesktopVolumeIcon, xfdesktop_volume_icon,
                       XFDESKTOP_TYPE_FILE_ICON, 0,
                       G_IMPLEMENT_INTERFACE(THUNARX_TYPE_FILE_INFO,
                                             xfdesktop_volume_icon_tfi_init)
                       )
#else
G_DEFINE_TYPE(XfdesktopVolumeIcon, xfdesktop_volume_icon,
              XFDESKTOP_TYPE_FILE_ICON)
#endif



static GQuark xfdesktop_volume_icon_activated_quark;



static void
xfdesktop_volume_icon_class_init(XfdesktopVolumeIconClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    XfdesktopIconClass *icon_class = (XfdesktopIconClass *)klass;
    XfdesktopFileIconClass *file_icon_class = (XfdesktopFileIconClass *)klass;

    xfdesktop_volume_icon_activated_quark = g_quark_from_static_string("xfdesktop-volume-icon-activated");
    
    g_type_class_add_private(klass, sizeof(XfdesktopVolumeIconClass));
    
    gobject_class->finalize = xfdesktop_volume_icon_finalize;
    
    icon_class->peek_pixbuf = xfdesktop_volume_icon_peek_pixbuf;
    icon_class->peek_label = xfdesktop_volume_icon_peek_label;
    icon_class->get_identifier = xfdesktop_volume_icon_get_identifier;
    icon_class->peek_tooltip_pixbuf = xfdesktop_volume_icon_peek_tooltip_pixbuf;
    icon_class->peek_tooltip = xfdesktop_volume_icon_peek_tooltip;
    icon_class->get_allowed_drag_actions = xfdesktop_volume_icon_get_allowed_drag_actions;
    icon_class->get_allowed_drop_actions = xfdesktop_volume_icon_get_allowed_drop_actions;
    icon_class->do_drop_dest = xfdesktop_volume_icon_do_drop_dest;
    icon_class->populate_context_menu = xfdesktop_volume_icon_populate_context_menu;
    icon_class->activated = xfdesktop_volume_icon_activated;
    
    file_icon_class->peek_file_info = xfdesktop_volume_icon_peek_file_info;
    file_icon_class->peek_filesystem_info = xfdesktop_volume_icon_peek_filesystem_info;
    file_icon_class->peek_file = xfdesktop_volume_icon_peek_file;
    file_icon_class->update_file_info = xfdesktop_volume_icon_update_file_info;
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

    /* remove pending change timeouts */
    if(icon->priv->changed_timeout_id > 0)
        g_source_remove(icon->priv->changed_timeout_id);
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(itheme),
                                         G_CALLBACK(xfdesktop_icon_invalidate_pixbuf),
                                         icon);
    
    if(icon->priv->label) {
        g_free(icon->priv->label);
        icon->priv->label = NULL;
    }

    if(icon->priv->file_info)
        g_object_unref(icon->priv->file_info);

    if(icon->priv->filesystem_info)
        g_object_unref(icon->priv->filesystem_info);

    if(icon->priv->file)
        g_object_unref(icon->priv->file);

    if(icon->priv->volume)
        g_object_unref(G_OBJECT(icon->priv->volume));

    if(icon->priv->tooltip)
        g_free(icon->priv->tooltip);
    
    G_OBJECT_CLASS(xfdesktop_volume_icon_parent_class)->finalize(obj);
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
    iface->get_file_info = xfdesktop_thunarx_file_info_get_file_info;
    iface->get_filesystem_info = xfdesktop_thunarx_file_info_get_filesystem_info;
    iface->get_location = xfdesktop_thunarx_file_info_get_location;
}
#endif  /* HAVE_THUNARX */


static gboolean
xfdesktop_volume_icon_is_mounted(XfdesktopIcon *icon)
{
    GVolume *volume = NULL;
    GMount *mount = NULL;
    gboolean ret = FALSE;
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(icon);

    g_return_val_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon), FALSE);

    volume = xfdesktop_volume_icon_peek_volume(volume_icon);

    if(volume != NULL)
        mount = g_volume_get_mount(volume);

    if(mount != NULL) {
        ret = TRUE;
        g_object_unref(mount);
    } else {
        ret = FALSE;
    }

    return ret;
}

static GIcon *
xfdesktop_volume_icon_load_icon(XfdesktopIcon *icon)
{
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(icon);
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    GIcon *gicon = NULL;

    TRACE("entering");

    /* load icon and keep a ref to it */
    if(volume_icon->priv->volume) {
        gicon = g_volume_get_icon(volume_icon->priv->volume);

        if(G_IS_ICON(gicon))
            g_object_ref(gicon);

        g_object_set(file_icon, "gicon", gicon, NULL);

        /* Add any user set emblems */
        gicon = xfdesktop_file_icon_add_emblems(file_icon);
    }

    return gicon;
}

static GdkPixbuf *
xfdesktop_volume_icon_peek_pixbuf(XfdesktopIcon *icon,
                                  gint width, gint height)
{
    gint opacity = 100;
    GIcon *gicon = NULL;
    GdkPixbuf *pix = NULL;
    
    g_return_val_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon), NULL);

    if(!xfdesktop_file_icon_has_gicon(XFDESKTOP_FILE_ICON(icon)))
        gicon = xfdesktop_volume_icon_load_icon(icon);
    else
        g_object_get(XFDESKTOP_FILE_ICON(icon), "gicon", &gicon, NULL);

    /* If the volume isn't mounted show it as semi-transparent */
    if(!xfdesktop_volume_icon_is_mounted(icon))
        opacity = 50;

    pix = xfdesktop_file_utils_get_icon(gicon, height, height, opacity);

    return pix;
}

static GdkPixbuf *
xfdesktop_volume_icon_peek_tooltip_pixbuf(XfdesktopIcon *icon,
                                          gint width, gint height)
{
    GIcon *gicon = NULL;
    GdkPixbuf *tooltip_pix = NULL;

    g_return_val_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon), NULL);

    if(!xfdesktop_file_icon_has_gicon(XFDESKTOP_FILE_ICON(icon)))
        gicon = xfdesktop_volume_icon_load_icon(icon);
    else
        g_object_get(XFDESKTOP_FILE_ICON(icon), "gicon", &gicon, NULL);

    tooltip_pix = xfdesktop_file_utils_get_icon(gicon, height, height, 100);

    return tooltip_pix;
}

const gchar *
xfdesktop_volume_icon_peek_label(XfdesktopIcon *icon)
{
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(icon);

    g_return_val_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon), NULL);
    
    if(!volume_icon->priv->label) {
            volume_icon->priv->label = g_volume_get_name(volume_icon->priv->volume);
    }

    return volume_icon->priv->label;
}

static gchar *
xfdesktop_volume_icon_get_identifier(XfdesktopIcon *icon)
{
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(icon);
    gchar *uuid;

    uuid = g_volume_get_identifier(volume_icon->priv->volume, G_VOLUME_IDENTIFIER_KIND_UUID);

    if(uuid == NULL)
        return g_strdup(xfdesktop_volume_icon_peek_label(icon));

    return uuid;
}

static GdkDragAction
xfdesktop_volume_icon_get_allowed_drag_actions(XfdesktopIcon *icon)
{
    /* volume icons more or less represent the volume's mount point, usually
     * (hopefully) a local path.  so when it's mounted, we certainly can't move
     * the mount point, but copying and linking should be OK.  when not mounted,
     * we should just disallow everything, since, even if its ThunarVfsInfo
     * is valid, we can't guarantee it won't change after mounting. */
    
    /* FIXME: should i allow all actions if not mounted as well, and try to
     * mount and resolve on drop? */
    
    if(xfdesktop_volume_icon_is_mounted(icon)) {
        GFileInfo *info = xfdesktop_file_icon_peek_file_info(XFDESKTOP_FILE_ICON(icon));
        if(info) {
            if(g_file_info_get_attribute_boolean(info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ))
                return GDK_ACTION_COPY | GDK_ACTION_LINK;
            else
                return GDK_ACTION_LINK;
        }
    }
    
    return 0;
}

static GdkDragAction
xfdesktop_volume_icon_get_allowed_drop_actions(XfdesktopIcon *icon,
                                               GdkDragAction *suggested_action)
{
    /* if not mounted, it doesn't really make sense to allow any operations
     * here.  if mounted, we should allow everything if it's writable. */
    
    /* FIXME: should i allow all actions if not mounted as well, and try to
     * mount and resolve on drop? */

    if(xfdesktop_volume_icon_is_mounted(icon)) {
        GFileInfo *info = xfdesktop_file_icon_peek_file_info(XFDESKTOP_FILE_ICON(icon));
        if(info) {
            if(g_file_info_get_attribute_boolean(info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE)) {
                if(suggested_action)
                    *suggested_action = GDK_ACTION_COPY;
                return GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_ASK;
            }
        }
    }

    if(suggested_action)
        *suggested_action = 0;
    
    return 0;
}

static gboolean
xfdesktop_volume_icon_do_drop_dest(XfdesktopIcon *icon,
                                 XfdesktopIcon *src_icon,
                                 GdkDragAction action)
{
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(icon);
    XfdesktopFileIcon *src_file_icon = XFDESKTOP_FILE_ICON(src_icon);
    GFileInfo *src_info;
    GFile *src_file, *parent, *dest_file = NULL;
    gboolean result = FALSE;
    gchar *name;
    
    TRACE("entering");
    
    g_return_val_if_fail(volume_icon && src_file_icon, FALSE);
    g_return_val_if_fail(xfdesktop_volume_icon_get_allowed_drop_actions(icon, NULL),
                         FALSE);
    
    src_file = xfdesktop_file_icon_peek_file(src_file_icon);

    src_info = xfdesktop_file_icon_peek_file_info(src_file_icon);
    if(!src_info)
        return FALSE;

    if(!volume_icon->priv->file_info)
        return FALSE;
   
    parent = g_file_get_parent(src_file);
    if(!parent)
        return FALSE;
    g_object_unref(parent);
        
    name = g_file_get_basename(src_file);
    if(!name)
        return FALSE;
    
    switch(action) {
        case GDK_ACTION_MOVE:
            XF_DEBUG("doing move");
            dest_file = g_object_ref(volume_icon->priv->file);
            break;
        
        case GDK_ACTION_COPY:
            XF_DEBUG("doing copy");
            dest_file = g_file_get_child(volume_icon->priv->file, name);
            break;
        
        case GDK_ACTION_LINK:
            XF_DEBUG("doing link");
            dest_file = g_object_ref(volume_icon->priv->file);
            break;
        
        default:
            g_warning("Unsupported drag action: %d", action);
    }

    if(dest_file) {
        xfdesktop_file_utils_transfer_file(action, src_file, dest_file,
                                           volume_icon->priv->gscreen);
    
        g_object_unref(dest_file);

        result = TRUE;
    }

    g_free(name);
        
    return result;
}

static const gchar *
xfdesktop_volume_icon_peek_tooltip(XfdesktopIcon *icon)
{
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(icon);
    GFileInfo *fs_info = xfdesktop_file_icon_peek_filesystem_info(XFDESKTOP_FILE_ICON(icon));
    GFile *file = xfdesktop_file_icon_peek_file(XFDESKTOP_FILE_ICON(icon));
    
    if(!volume_icon->priv->tooltip) {
        guint64 size, free_space;
        gchar *mount_point = NULL, *size_string = NULL, *free_space_string = NULL;

        if(file && fs_info) {
            mount_point = g_file_get_parse_name(file);

            size = g_file_info_get_attribute_uint64(fs_info,
                                                    G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
            free_space = g_file_info_get_attribute_uint64(fs_info,
                                                          G_FILE_ATTRIBUTE_FILESYSTEM_FREE);

            size_string = g_format_size(size);
            free_space_string = g_format_size(free_space);

            volume_icon->priv->tooltip =
                g_strdup_printf(_("Removable Volume\nMounted in \"%s\"\n%s left (%s total)"),
                                mount_point, free_space_string, size_string);
    
            g_free(free_space_string);
            g_free(size_string);
            g_free(mount_point);
        } else {
            volume_icon->priv->tooltip = g_strdup(_("Removable Volume\nNot mounted yet"));
        }
    }

    return volume_icon->priv->tooltip;
}

static void
xfdesktop_volume_icon_eject_finish(GObject *object,
                                   GAsyncResult *result,
                                   gpointer user_data)
{
    XfdesktopVolumeIcon *icon = XFDESKTOP_VOLUME_ICON(user_data);
    GtkWidget *icon_view = xfdesktop_icon_peek_icon_view(XFDESKTOP_ICON(icon));
    GtkWidget *toplevel = icon_view ? gtk_widget_get_toplevel(icon_view) : NULL;
    GVolume *volume = G_VOLUME(object);
    GError *error = NULL;
    gboolean eject_successful;
      
    g_return_if_fail(G_IS_VOLUME(object));
    g_return_if_fail(G_IS_ASYNC_RESULT(result));
    g_return_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon));

    eject_successful = g_volume_eject_with_operation_finish(volume, result, &error);

    if(!eject_successful) {
        /* ignore GIO errors handled internally */
        if(error->domain != G_IO_ERROR || error->code != G_IO_ERROR_FAILED_HANDLED) {
            gchar *volume_name = g_volume_get_name(volume);
            gchar *primary = g_markup_printf_escaped(_("Failed to eject \"%s\""), 
                                                     volume_name);

            /* display an error dialog to inform the user */
            xfce_message_dialog(toplevel ? GTK_WINDOW(toplevel) : NULL,
                                _("Eject Failed"), GTK_STOCK_DIALOG_ERROR, 
                                primary, error->message,
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);

            g_free(primary);
            g_free(volume_name);
        }

        g_clear_error(&error);
    }

#ifdef HAVE_LIBNOTIFY
    xfdesktop_notify_eject_finish(volume, eject_successful);
#endif

    g_object_unref(icon);
}

static void
xfdesktop_volume_icon_unmount_finish(GObject *object,
                                     GAsyncResult *result,
                                     gpointer user_data)
{
    XfdesktopVolumeIcon *icon = XFDESKTOP_VOLUME_ICON(user_data);
    GtkWidget *icon_view = xfdesktop_icon_peek_icon_view(XFDESKTOP_ICON(icon));
    GtkWidget *toplevel = gtk_widget_get_toplevel(icon_view);
    GMount *mount = G_MOUNT(object);
    GError *error = NULL;
    gboolean unmount_successful;
      
    g_return_if_fail(G_IS_MOUNT(object));
    g_return_if_fail(G_IS_ASYNC_RESULT(result));
    g_return_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon));

    unmount_successful = g_mount_unmount_with_operation_finish(mount, result, &error);

    if(!unmount_successful) {
        /* ignore GIO errors handled internally */
        if(error->domain != G_IO_ERROR || error->code != G_IO_ERROR_FAILED_HANDLED) {
            gchar *mount_name = g_mount_get_name(mount);
            gchar *primary = g_markup_printf_escaped(_("Failed to eject \"%s\""), 
                                                     mount_name);

            /* display an error dialog to inform the user */
            xfce_message_dialog(toplevel ? GTK_WINDOW(toplevel) : NULL,
                                _("Eject Failed"), GTK_STOCK_DIALOG_ERROR, 
                                primary, error->message,
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);

            g_free(primary);
            g_free(mount_name);
        }

        g_clear_error(&error);
    }

#ifdef HAVE_LIBNOTIFY
    xfdesktop_notify_unmount_finish(mount, unmount_successful);
#endif

    g_object_unref(icon);
}

static void
xfdesktop_volume_icon_mount_finish(GObject *object,
                                   GAsyncResult *result,
                                   gpointer user_data)
{
    XfdesktopVolumeIcon *icon = XFDESKTOP_VOLUME_ICON(user_data);
    GtkWidget *icon_view = xfdesktop_icon_peek_icon_view(XFDESKTOP_ICON(icon));
    GtkWidget *toplevel = gtk_widget_get_toplevel(icon_view);
    GVolume *volume = G_VOLUME(object);
    GError *error = NULL;

    if(!g_volume_mount_finish(volume, result, &error)) {
        if(error->domain != G_IO_ERROR || error->code != G_IO_ERROR_FAILED_HANDLED) {
            gchar *volume_name = g_volume_get_name(volume);
            gchar *primary = g_markup_printf_escaped(_("Failed to mount \"%s\""),
                                                     volume_name);
            xfce_message_dialog(toplevel ? GTK_WINDOW(toplevel) : NULL,
                                _("Mount Failed"), GTK_STOCK_DIALOG_ERROR, 
                                primary, error->message,
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
            g_free(primary);
            g_free(volume_name);
        }
        
        g_clear_error(&error);
    } else {
        GMount *mount = g_volume_get_mount(volume);
        GFile *file = NULL;
        GFileInfo *info = NULL;

        if(mount) {
            file = g_mount_get_root(mount);
            info = g_file_query_info(file,
                                     XFDESKTOP_FILE_INFO_NAMESPACE,
                                     G_FILE_QUERY_INFO_NONE,
                                     NULL, NULL);
            g_object_unref(mount);
        }

        if(file && info) {
            gboolean activated = FALSE;

            if(icon->priv->file)
                g_object_unref(icon->priv->file);
            icon->priv->file = g_object_ref(file);

            xfdesktop_file_icon_update_file_info(XFDESKTOP_FILE_ICON(icon), info);

            activated = GPOINTER_TO_UINT(g_object_get_qdata(G_OBJECT(icon), 
                                                            xfdesktop_volume_icon_activated_quark));
            if(activated) {
                XfdesktopIcon *icon_p = XFDESKTOP_ICON(icon);
                XFDESKTOP_ICON_CLASS(xfdesktop_volume_icon_parent_class)->activated(icon_p);
            }
            g_object_set_qdata(G_OBJECT(icon), xfdesktop_volume_icon_activated_quark, NULL);
        } else {
            if(icon->priv->file)
                g_object_unref(icon->priv->file);
            icon->priv->file = NULL;

            xfdesktop_file_icon_update_file_info(XFDESKTOP_FILE_ICON(icon), NULL);
        }
            
        if(file)
            g_object_unref(file);

        if(info)
            g_object_unref(info);
    }
}

static void
xfdesktop_volume_icon_menu_mount(GtkWidget *widget, gpointer user_data)
{
    XfdesktopVolumeIcon *icon = XFDESKTOP_VOLUME_ICON(user_data);
    GtkWidget *icon_view = xfdesktop_icon_peek_icon_view(XFDESKTOP_ICON(icon));
    GtkWidget *toplevel = gtk_widget_get_toplevel(icon_view);
    GVolume *volume;
    GMount *mount;
    GMountOperation *operation;

    volume = xfdesktop_volume_icon_peek_volume(icon);
    mount = g_volume_get_mount(volume);

    if(mount) {
        g_object_unref(mount);
        return;
    }

    operation = gtk_mount_operation_new(toplevel ? GTK_WINDOW(toplevel) : NULL);
    gtk_mount_operation_set_screen(GTK_MOUNT_OPERATION(operation),
                                   icon->priv->gscreen);

    g_volume_mount(volume, G_MOUNT_MOUNT_NONE, operation, NULL,
                   xfdesktop_volume_icon_mount_finish,
                   g_object_ref(icon));

    g_object_unref(operation);
}

static void
xfdesktop_volume_icon_menu_unmount(GtkWidget *widget, gpointer user_data)
{
    XfdesktopVolumeIcon *icon = XFDESKTOP_VOLUME_ICON(user_data);
    GtkWidget *icon_view = xfdesktop_icon_peek_icon_view(XFDESKTOP_ICON(icon));
    GtkWidget *toplevel = gtk_widget_get_toplevel(icon_view);
    GVolume *volume;
    GMount *mount;
    GMountOperation *operation;

    volume = xfdesktop_volume_icon_peek_volume(icon);
    mount = g_volume_get_mount(volume);

    if(!mount)
        return;

#ifdef HAVE_LIBNOTIFY
    xfdesktop_notify_unmount(mount);
#endif

    operation = gtk_mount_operation_new(toplevel ? GTK_WINDOW(toplevel) : NULL);
    gtk_mount_operation_set_screen(GTK_MOUNT_OPERATION(operation),
                                   icon->priv->gscreen);

    g_mount_unmount_with_operation(mount,
                                   G_MOUNT_UNMOUNT_NONE,
                                   operation,
                                   NULL,
                                   xfdesktop_volume_icon_unmount_finish,
                                   g_object_ref(icon));

    g_object_unref(mount);
    g_object_unref(operation);
}

static void
xfdesktop_volume_icon_menu_eject(GtkWidget *widget,
                                 gpointer user_data)
{
    XfdesktopVolumeIcon *icon = XFDESKTOP_VOLUME_ICON(user_data);
    GtkWidget *icon_view = xfdesktop_icon_peek_icon_view(XFDESKTOP_ICON(icon));
    GtkWidget *toplevel = gtk_widget_get_toplevel(icon_view);
    GVolume *volume;
    GMount *mount;
    GMountOperation *operation = NULL;

    volume = xfdesktop_volume_icon_peek_volume(icon);
    mount = g_volume_get_mount(volume);

    if(!mount)
        return;

    if(g_volume_can_eject(volume)) {
#ifdef HAVE_LIBNOTIFY
        xfdesktop_notify_eject(volume);
#endif
        operation = gtk_mount_operation_new(toplevel ? GTK_WINDOW(toplevel) : NULL);
        gtk_mount_operation_set_screen(GTK_MOUNT_OPERATION(operation),
                                       icon->priv->gscreen);

        g_volume_eject_with_operation(volume,
                                      G_MOUNT_UNMOUNT_NONE,
                                      operation,
                                      NULL,
                                      xfdesktop_volume_icon_eject_finish,
                                      g_object_ref(icon));
    } else {
        /* If we can't eject the volume try to unmount it */
        xfdesktop_volume_icon_menu_unmount(widget, user_data);
    }

    g_object_unref(mount);
    if(operation != NULL)
        g_object_unref(operation);
}

static void
xfdesktop_volume_icon_menu_properties(GtkWidget *widget,
                                      gpointer user_data)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(user_data);        
    GFile *file;
    
    file = xfdesktop_file_icon_peek_file(icon);
    xfdesktop_file_utils_show_properties_dialog(file, 
                                                XFDESKTOP_VOLUME_ICON(icon)->priv->gscreen, 
                                                NULL);
}

static void
xfdesktop_volume_icon_add_context_menu_option(XfdesktopIcon *icon,
                                              const gchar* icon_name,
                                              const gchar* icon_label,
                                              GtkWidget *menu,
                                              GCallback callback)
{
    GtkWidget *mi, *img;

    img = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_MENU);
    gtk_widget_show(img);
    mi = gtk_image_menu_item_new_with_mnemonic(icon_label);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    if(callback != NULL) {
        g_signal_connect(G_OBJECT(mi), "activate",
                         callback,
                         icon);
    } else {
        gtk_widget_set_sensitive(mi, FALSE);
    }
}

static gboolean
xfdesktop_volume_icon_populate_context_menu(XfdesktopIcon *icon,
                                            GtkWidget *menu)
{
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(icon);
    GVolume *volume = volume_icon->priv->volume;
    GtkWidget *mi, *img;
    GMount *mount;
    const gchar *icon_name, *icon_label;

    icon_name = GTK_STOCK_OPEN;

    img = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
    gtk_widget_show(img);
    mi = gtk_image_menu_item_new_with_mnemonic(_("_Open"));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect_swapped(G_OBJECT(mi), "activate",
                             G_CALLBACK(xfdesktop_icon_activated), icon);
    
    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    
    mount = g_volume_get_mount(volume);

    if(mount && g_volume_can_eject(volume)) {
        icon_name = "media-eject";
        icon_label = _("E_ject Volume");
        xfdesktop_volume_icon_add_context_menu_option(icon, icon_name, icon_label,
                        menu, G_CALLBACK(xfdesktop_volume_icon_menu_eject));
    }

    if(mount && g_mount_can_unmount(mount)) {
        icon_name = NULL;
        icon_label = _("_Unmount Volume");
        xfdesktop_volume_icon_add_context_menu_option(icon, icon_name, icon_label,
                        menu, G_CALLBACK(xfdesktop_volume_icon_menu_unmount));
    }

    if(!mount && g_volume_can_mount(volume)) {
        icon_name = NULL;
        icon_label = _("_Mount Volume");
        xfdesktop_volume_icon_add_context_menu_option(icon, icon_name, icon_label,
                        menu, G_CALLBACK(xfdesktop_volume_icon_menu_mount));
    }

    if(mount)
        g_object_unref(mount);

    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

    icon_name = GTK_STOCK_PROPERTIES;
    icon_label = _("P_roperties...");

    if(!volume_icon->priv->file_info)
        xfdesktop_volume_icon_add_context_menu_option(icon, icon_name, icon_label,
                                                      menu, NULL);
    else {
        xfdesktop_volume_icon_add_context_menu_option(icon, icon_name, icon_label,
                    menu, G_CALLBACK(xfdesktop_volume_icon_menu_properties));
    }
    
    return TRUE;
}


static GFileInfo *
xfdesktop_volume_icon_peek_file_info(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon), NULL);
    return XFDESKTOP_VOLUME_ICON(icon)->priv->file_info;
}

static GFileInfo *
xfdesktop_volume_icon_peek_filesystem_info(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon), NULL);
    return XFDESKTOP_VOLUME_ICON(icon)->priv->filesystem_info;
}

static GFile *
xfdesktop_volume_icon_peek_file(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon), NULL);
    return XFDESKTOP_VOLUME_ICON(icon)->priv->file;
}

static void
xfdesktop_volume_icon_update_file_info(XfdesktopFileIcon *icon,
                                       GFileInfo *info)
{
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(icon);

    g_return_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon));

    TRACE("entering");

    /* just replace the file info here */
    if(volume_icon->priv->file_info)
        g_object_unref(volume_icon->priv->file_info);
    volume_icon->priv->file_info = info ? g_object_ref(info) : NULL;

    /* update the filesystem info as well */
    if(volume_icon->priv->filesystem_info)
        g_object_unref(volume_icon->priv->filesystem_info);
    if(volume_icon->priv->file) {
        volume_icon->priv->filesystem_info = g_file_query_filesystem_info(volume_icon->priv->file,
                                                                          XFDESKTOP_FILE_INFO_NAMESPACE,
                                                                          NULL, NULL);
    }

    /* invalidate the tooltip */
    if(volume_icon->priv->tooltip) {
        g_free(volume_icon->priv->tooltip);
        volume_icon->priv->tooltip = NULL;
    }

    /* not really easy to check if this changed or not, so just invalidate it */
    xfdesktop_file_icon_invalidate_icon(XFDESKTOP_FILE_ICON(icon));
    xfdesktop_icon_invalidate_pixbuf(XFDESKTOP_ICON(icon));
    xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(icon));
}

static gboolean
xfdesktop_volume_icon_activated(XfdesktopIcon *icon_p)
{
    XfdesktopVolumeIcon *icon = XFDESKTOP_VOLUME_ICON(icon_p);
    GVolume *volume = xfdesktop_volume_icon_peek_volume(icon);
    GMount *mount;
    
    TRACE("entering");

    mount = g_volume_get_mount(volume);
    
    if(!mount) {
        /* set the activated flag so we can chain the event up to the 
         * parent class in the mount finish callback */
        g_object_set_qdata(G_OBJECT(icon), xfdesktop_volume_icon_activated_quark,
                           GUINT_TO_POINTER(TRUE));

        /* mount the volume and open the folder in the mount finish callback */
        xfdesktop_volume_icon_menu_mount(NULL, icon);

        return TRUE;
    } else {
        g_object_unref(mount);

        /* chain up to the parent class (where the mount point folder is
         * opened in the file manager) */
        return XFDESKTOP_ICON_CLASS(xfdesktop_volume_icon_parent_class)->activated(icon_p);
    }
}

static gboolean
volume_icon_changed_timeout(XfdesktopVolumeIcon *volume_icon)
{
    GMount *mount;
    gboolean mounted_before = FALSE;
    gboolean mounted_after = FALSE;

    g_return_val_if_fail(XFDESKTOP_IS_VOLUME_ICON(volume_icon), FALSE);

    XF_DEBUG("TIMEOUT");

    /* reset the icon's mount point information */
    if(volume_icon->priv->file) {
        g_object_unref(volume_icon->priv->file);
        volume_icon->priv->file = NULL;

        /* apparently the volume was mounted before, otherwise
         * we wouldn't have had a mount point for it */
        mounted_before = TRUE;
    }
    if(volume_icon->priv->file_info) {
        g_object_unref(volume_icon->priv->file_info);
        volume_icon->priv->file_info = NULL;
    }
    if(volume_icon->priv->filesystem_info) {
        g_object_unref(volume_icon->priv->filesystem_info);
        volume_icon->priv->filesystem_info = NULL;
    }

    /* check if we have a valid mount now */
    mount = g_volume_get_mount(volume_icon->priv->volume);
    if(mount) {
        /* load mount point information */
        volume_icon->priv->file = g_mount_get_root(mount);
        volume_icon->priv->file_info = 
            g_file_query_info(volume_icon->priv->file, 
                              XFDESKTOP_FILE_INFO_NAMESPACE,
                              G_FILE_QUERY_INFO_NONE,
                              NULL, NULL);
        volume_icon->priv->filesystem_info = 
            g_file_query_filesystem_info(volume_icon->priv->file,
                                         XFDESKTOP_FILESYSTEM_INFO_NAMESPACE,
                                         NULL, NULL);

        /* release the mount itself */
        g_object_unref(mount);

        /* the device is mounted now (we have a mount point for it) */
        mounted_after = TRUE;
    }

    XF_DEBUG("MOUNTED BEFORE: %d, MOUNTED AFTER: %d", mounted_before, mounted_after);

    if(mounted_before != mounted_after) {
        /* invalidate the tooltip */
        if(volume_icon->priv->tooltip) {
            g_free(volume_icon->priv->tooltip);
            volume_icon->priv->tooltip = NULL;
        }

        /* not really easy to check if this changed or not, so just invalidate it */
        xfdesktop_icon_invalidate_pixbuf(XFDESKTOP_ICON(volume_icon));
        xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(volume_icon));

        /* finalize the timeout source */
        volume_icon->priv->changed_timeout_id = 0;
        return FALSE;
    } else {
        /* increment the timeout counter */
        volume_icon->priv->changed_timeout_count += 1;

        if(volume_icon->priv->changed_timeout_count >= 5) {
            /* finalize the timeout source */
            volume_icon->priv->changed_timeout_id = 0;
            return FALSE;
        } else {
            XF_DEBUG("TRY AGAIN");
            return TRUE;
        }
    }
}

static void
xfdesktop_volume_icon_changed(GVolume *volume,
                              XfdesktopVolumeIcon *volume_icon)
{
    g_return_if_fail(G_IS_VOLUME(volume));
    g_return_if_fail(XFDESKTOP_IS_VOLUME_ICON(volume_icon));

    XF_DEBUG("VOLUME CHANGED");

    /**
     * NOTE: We use a timeout here to check if the volume is 
     * now mounted (or has been unmounted). This timeout seems
     * to be needed because when the "changed" signal is emitted,
     * the GMount is always NULL. In a 500ms timeout we check
     * at most 5 times for a valid mount until we give up. This
     * hopefully is a suitable workaround for most machines and
     * drives. 
     */

    /* abort an existing timeout, we may have to run it a few times
     * once again for the new event */
    if(volume_icon->priv->changed_timeout_id > 0) {
        g_source_remove(volume_icon->priv->changed_timeout_id);
        volume_icon->priv->changed_timeout_id = 0;
    }

    /* reset timeout information and start a timeout */
    volume_icon->priv->changed_timeout_count = 0;
    volume_icon->priv->changed_timeout_id =
        g_timeout_add_full(G_PRIORITY_LOW, 500, 
                           (GSourceFunc) volume_icon_changed_timeout, 
                           g_object_ref(volume_icon),
                           g_object_unref);
}

XfdesktopVolumeIcon *
xfdesktop_volume_icon_new(GVolume *volume,
                          GdkScreen *screen)
{
    XfdesktopVolumeIcon *volume_icon;
    GMount *mount;
    
    g_return_val_if_fail(G_IS_VOLUME(volume), NULL);
    
    volume_icon = g_object_new(XFDESKTOP_TYPE_VOLUME_ICON, NULL);
    volume_icon->priv->volume = g_object_ref(G_OBJECT(volume));
    volume_icon->priv->gscreen = screen;

    mount = g_volume_get_mount(volume);
    if(mount) {
        volume_icon->priv->file = g_mount_get_root(mount);
        volume_icon->priv->file_info = g_file_query_info(volume_icon->priv->file,
                                                         XFDESKTOP_FILE_INFO_NAMESPACE,
                                                         G_FILE_QUERY_INFO_NONE,
                                                         NULL, NULL);
        volume_icon->priv->filesystem_info = g_file_query_filesystem_info(volume_icon->priv->file,
                                                                          XFDESKTOP_FILESYSTEM_INFO_NAMESPACE,
                                                                          NULL, NULL);
        g_object_unref(mount);
    }

    g_signal_connect_swapped(G_OBJECT(gtk_icon_theme_get_for_screen(screen)),
                             "changed",
                             G_CALLBACK(xfdesktop_icon_invalidate_pixbuf),
                             volume_icon);

    g_signal_connect(volume, "changed", 
                     G_CALLBACK(xfdesktop_volume_icon_changed), 
                     volume_icon);
    
    return volume_icon;
}

GVolume *
xfdesktop_volume_icon_peek_volume(XfdesktopVolumeIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon), NULL);
    return icon->priv->volume;
}
