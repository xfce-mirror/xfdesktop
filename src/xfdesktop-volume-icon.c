/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006,2024 Brian Tarricone, <brian@tarricone.org>
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

struct _XfdesktopVolumeIcon
{
    XfdesktopFileIcon parent_instance;

    gchar *tooltip;
    gchar *label;
    GVolume *volume;
    GMount *mount;
    GFileInfo *file_info;
    GFileInfo *filesystem_info;
    GFile *file;
    GdkScreen *gscreen;

    GCancellable *file_info_op_handle;
    GCancellable *filesystem_info_op_handle;
};

static void xfdesktop_volume_icon_finalize(GObject *obj);

static const gchar *xfdesktop_volume_icon_peek_label(XfdesktopIcon *icon);
static gchar *xfdesktop_volume_icon_get_identifier(XfdesktopIcon *icon);
static const gchar *xfdesktop_volume_icon_peek_tooltip(XfdesktopIcon *icon);
static gboolean xfdesktop_volume_icon_populate_context_menu(XfdesktopIcon *icon,
                                                            GtkWidget *menu);

static GIcon * xfdesktop_volume_icon_get_gicon(XfdesktopFileIcon *icon);
static gdouble xfdesktop_volume_icon_get_opacity(XfdesktopFileIcon *icon);
static GFileInfo *xfdesktop_volume_icon_peek_file_info(XfdesktopFileIcon *icon);
static GFileInfo *xfdesktop_volume_icon_peek_filesystem_info(XfdesktopFileIcon *icon);
static GFile *xfdesktop_volume_icon_peek_file(XfdesktopFileIcon *icon);
static void xfdesktop_volume_icon_update_file_info(XfdesktopFileIcon *icon,
                                                   GFileInfo *info);
static gboolean xfdesktop_volume_icon_activate(XfdesktopIcon *icon,
                                               GtkWindow *window);

static guint xfdesktop_volume_icon_hash(XfdesktopFileIcon *icon);
static gchar *xfdesktop_volume_icon_get_sort_key(XfdesktopFileIcon *icon);

static void xfdesktop_volume_icon_fetch_file_info(XfdesktopVolumeIcon *volume_icon,
                                                  GAsyncReadyCallback callback);
static void xfdesktop_volume_icon_fetch_filesystem_info(XfdesktopVolumeIcon *volume_icon);

static void xfdesktop_volume_icon_file_info_ready(GObject *source,
                                                  GAsyncResult *result,
                                                  gpointer user_data);
static void xfdesktop_volume_icon_filesystem_info_ready(GObject *source,
                                                        GAsyncResult *result,
                                                        gpointer user_data);

#ifdef HAVE_THUNARX
static void xfdesktop_volume_icon_tfi_init(ThunarxFileInfoIface *iface);

G_DEFINE_TYPE_EXTENDED(XfdesktopVolumeIcon, xfdesktop_volume_icon,
                       XFDESKTOP_TYPE_FILE_ICON, 0,
                       G_IMPLEMENT_INTERFACE(THUNARX_TYPE_FILE_INFO,
                                             xfdesktop_volume_icon_tfi_init))
#else
G_DEFINE_TYPE(XfdesktopVolumeIcon, xfdesktop_volume_icon, XFDESKTOP_TYPE_FILE_ICON)
#endif



// Value type is GtkWindow*
static GQuark xfdesktop_volume_icon_activate_window_quark;

const gchar *idents_for_sort_key[] = {
    G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE,
    G_VOLUME_IDENTIFIER_KIND_UUID,
};


static void
xfdesktop_volume_icon_class_init(XfdesktopVolumeIconClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    XfdesktopIconClass *icon_class = (XfdesktopIconClass *)klass;
    XfdesktopFileIconClass *file_icon_class = (XfdesktopFileIconClass *)klass;

    xfdesktop_volume_icon_activate_window_quark = g_quark_from_static_string("xfdesktop-volume-icon-activate-window");

    gobject_class->finalize = xfdesktop_volume_icon_finalize;

    icon_class->peek_label = xfdesktop_volume_icon_peek_label;
    icon_class->get_identifier = xfdesktop_volume_icon_get_identifier;
    icon_class->peek_tooltip = xfdesktop_volume_icon_peek_tooltip;
    icon_class->populate_context_menu = xfdesktop_volume_icon_populate_context_menu;
    icon_class->activate = xfdesktop_volume_icon_activate;

    file_icon_class->get_gicon = xfdesktop_volume_icon_get_gicon;
    file_icon_class->get_icon_opacity = xfdesktop_volume_icon_get_opacity;
    file_icon_class->peek_file_info = xfdesktop_volume_icon_peek_file_info;
    file_icon_class->peek_filesystem_info = xfdesktop_volume_icon_peek_filesystem_info;
    file_icon_class->peek_file = xfdesktop_volume_icon_peek_file;
    file_icon_class->update_file_info = xfdesktop_volume_icon_update_file_info;
    file_icon_class->hash = xfdesktop_volume_icon_hash;
    file_icon_class->get_sort_key = xfdesktop_volume_icon_get_sort_key;
}

static void
xfdesktop_volume_icon_init(XfdesktopVolumeIcon *icon) {}

static void
xfdesktop_volume_icon_finalize(GObject *obj)
{
    XfdesktopVolumeIcon *icon = XFDESKTOP_VOLUME_ICON(obj);

    if(icon->label) {
        g_free(icon->label);
        icon->label = NULL;
    }

    if (icon->file_info_op_handle != NULL) {
        g_cancellable_cancel(icon->file_info_op_handle);
        g_clear_object(&icon->file_info_op_handle);
    }
    g_clear_object(&icon->file_info);

    if (icon->filesystem_info_op_handle != NULL) {
        g_cancellable_cancel(icon->filesystem_info_op_handle);
        g_clear_object(&icon->filesystem_info_op_handle);
    }
    g_clear_object(&icon->filesystem_info);

    if(icon->file)
        g_object_unref(icon->file);

    if (icon->mount != NULL) {
        g_object_unref(icon->mount);
    }

    if(icon->volume)
        g_object_unref(G_OBJECT(icon->volume));

    if(icon->tooltip)
        g_free(icon->tooltip);

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
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(icon);
    return volume_icon->mount != NULL;
}

static GIcon *
xfdesktop_volume_icon_get_gicon(XfdesktopFileIcon *icon)
{
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(icon);
    GIcon *gicon = NULL;

    TRACE("entering");

    GIcon *base_gicon;
    if (volume_icon->volume != NULL) {
        base_gicon = g_volume_get_icon(volume_icon->volume);
    } else if (volume_icon->mount != NULL) {
        base_gicon = g_mount_get_icon(volume_icon->mount);
    } else {
        g_assert_not_reached();
    }

    if (base_gicon != NULL) {
        gicon = xfdesktop_file_icon_add_emblems(icon, base_gicon);
    }

    return gicon;
}

static gdouble
xfdesktop_volume_icon_get_opacity(XfdesktopFileIcon *icon) {
    return xfdesktop_volume_icon_is_mounted(XFDESKTOP_ICON(icon)) ? 1.0 : 0.5;
}

const gchar *
xfdesktop_volume_icon_peek_label(XfdesktopIcon *icon)
{
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(icon);

    g_return_val_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon), NULL);

    if(!volume_icon->label) {
        if (volume_icon->volume != NULL) {
            volume_icon->label = g_volume_get_name(volume_icon->volume);
        } else if (volume_icon->mount != NULL) {
            volume_icon->label = g_mount_get_name(volume_icon->mount);
        } else {
            g_assert_not_reached();
        }
    }

    return volume_icon->label;
}

static gchar *
xfdesktop_volume_icon_get_identifier(XfdesktopIcon *icon)
{
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(icon);
    gchar *identifier = NULL;

    if (volume_icon->volume != NULL) {
        identifier = g_volume_get_identifier(volume_icon->volume, G_VOLUME_IDENTIFIER_KIND_UUID);
    }

    if (identifier == NULL && volume_icon->mount != NULL) {
        identifier = g_mount_get_uuid(volume_icon->mount);
    }

    if (identifier == NULL && volume_icon->mount != NULL) {
        GFile *location = g_mount_get_root(volume_icon->mount);
        if (location == NULL) {
            location = g_mount_get_default_location(volume_icon->mount);
        }

        if (location != NULL) {
            if (g_file_has_uri_scheme(location, "file")) {
                identifier = g_file_get_path(location);
            } else {
                identifier = g_file_get_uri(location);
            }
            g_object_unref(location);
        }
    }

    if (identifier == NULL) {
        identifier = g_strdup(xfdesktop_volume_icon_peek_label(icon));
    }

    return identifier;
}

static const gchar *
xfdesktop_volume_icon_peek_tooltip(XfdesktopIcon *icon)
{
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(icon);
    GFileInfo *fs_info = xfdesktop_file_icon_peek_filesystem_info(XFDESKTOP_FILE_ICON(icon));
    GFile *file = xfdesktop_file_icon_peek_file(XFDESKTOP_FILE_ICON(icon));

    if(!volume_icon->tooltip) {
        gchar *mount_point = NULL, *size_string = NULL, *free_space_string = NULL;

        if(file && fs_info) {
            guint64 size, free_space;

            mount_point = g_file_get_parse_name(file);

            size = g_file_info_get_attribute_uint64(fs_info,
                                                    G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
            free_space = g_file_info_get_attribute_uint64(fs_info,
                                                          G_FILE_ATTRIBUTE_FILESYSTEM_FREE);

            size_string = g_format_size(size);
            free_space_string = g_format_size(free_space);
        } else {
            mount_point = g_strdup(_("(not mounted)"));
            size_string = g_strdup(_("(unknown)"));
            free_space_string = g_strdup(_("(unknown)"));
        }

        volume_icon->tooltip =
            g_strdup_printf(_("Name: %s\nType: %s\nMounted at: %s\nSize: %s\nFree Space: %s"),
                            volume_icon->label,
                            _("Removable Volume"),
                            mount_point,
                            size_string,
                            free_space_string);

        g_free(free_space_string);
        g_free(size_string);
        g_free(mount_point);
    }

    return volume_icon->tooltip;
}

static void
xfdesktop_volume_icon_eject_finish(GObject *object,
                                   GAsyncResult *result,
                                   gpointer user_data)
{
    XfdesktopVolumeIcon *icon = XFDESKTOP_VOLUME_ICON(user_data);
    GError *error = NULL;
    gboolean eject_successful;

    g_return_if_fail(G_IS_VOLUME(object));
    g_return_if_fail(G_IS_ASYNC_RESULT(result));
    g_return_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon));

    if (G_IS_VOLUME(object)) {
        eject_successful = g_volume_eject_with_operation_finish(G_VOLUME(object), result, &error);
    } else if (G_IS_MOUNT(object)) {
        eject_successful = g_mount_eject_with_operation_finish(G_MOUNT(object), result, &error);
    } else {
        g_assert_not_reached();
    }

    if(!eject_successful) {
        /* ignore GIO errors handled internally */
        if(error->domain != G_IO_ERROR || error->code != G_IO_ERROR_FAILED_HANDLED) {
            gchar *name;
            if (G_IS_VOLUME(object)) {
                name = g_volume_get_name(G_VOLUME(object));
            } else if (G_IS_MOUNT(object)) {
                name = g_mount_get_name(G_MOUNT(object));
            } else {
                name = NULL;
                g_assert_not_reached();
            }

            gchar *primary = g_markup_printf_escaped(_("Failed to eject \"%s\""), name);

            /* display an error dialog to inform the user */
            xfce_message_dialog(NULL,
                                _("Eject Failed"), "dialog-error",
                                primary, error->message,
                                XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                                NULL);

            g_free(primary);
            g_free(name);
        }

        g_clear_error(&error);
    }

#ifdef HAVE_LIBNOTIFY
    if (G_IS_VOLUME(object)) {
        xfdesktop_notify_eject_volume_finish(G_VOLUME(object), eject_successful);
    } else if (G_IS_MOUNT(object)) {
        xfdesktop_notify_eject_mount_finish(G_MOUNT(object), eject_successful);
    }
#endif

    g_object_unref(icon);
}

static void
xfdesktop_volume_icon_unmount_finish(GObject *object,
                                     GAsyncResult *result,
                                     gpointer user_data)
{
    XfdesktopVolumeIcon *icon = XFDESKTOP_VOLUME_ICON(user_data);
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
            xfce_message_dialog(NULL,
                                _("Eject Failed"), "dialog-error",
                                primary, error->message,
                                XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                                NULL);

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
xfdesktop_volume_icon_mounted_file_info_ready(GObject *source,
                                              GAsyncResult *result,
                                              gpointer user_data)
{
    GFile *file = G_FILE(source);
    GWeakRef *volume_icon_ref = user_data;
    gpointer volume_icon_p = g_weak_ref_get(volume_icon_ref);

    g_weak_ref_clear(volume_icon_ref);
    g_slice_free(GWeakRef, volume_icon_ref);

    if (G_UNLIKELY(volume_icon_p == NULL)) {
        GFileInfo *file_info = g_file_query_info_finish(file, result, NULL);
        if (file_info != NULL) {
            g_object_unref(file_info);
        }
    } else {
        XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(volume_icon_p);
        GFileInfo *file_info;
        GError *error = NULL;

        g_clear_object(&volume_icon->file_info);
        g_clear_object(&volume_icon->file_info_op_handle);

        file_info = g_file_query_info_finish(file, result, &error);
        if (file_info != NULL) {
            GtkWindow *window;

            xfdesktop_file_icon_update_file_info(XFDESKTOP_FILE_ICON(volume_icon), file_info);

            window = g_object_get_qdata(G_OBJECT(volume_icon),
                                        xfdesktop_volume_icon_activate_window_quark);
            if (window != NULL) {
                XfdesktopIcon *icon_p = XFDESKTOP_ICON(volume_icon);
                XFDESKTOP_ICON_CLASS(xfdesktop_volume_icon_parent_class)->activate(icon_p, window);
            }
            g_object_set_qdata(G_OBJECT(volume_icon), xfdesktop_volume_icon_activate_window_quark, NULL);
        } else {
            if (error != NULL) {
                g_printerr("Failed to query volume file info after mount (%d, %d): %s\n",
                           error->domain, error->code, error->message);
                g_error_free(error);
            }
        }
    }
}

static void
xfdesktop_volume_icon_mount_finish(GObject *object,
                                   GAsyncResult *result,
                                   gpointer user_data)
{
    XfdesktopVolumeIcon *icon = XFDESKTOP_VOLUME_ICON(user_data);
    GVolume *volume = G_VOLUME(object);
    GError *error = NULL;

    if(!g_volume_mount_finish(volume, result, &error)) {
        if(error->domain != G_IO_ERROR || error->code != G_IO_ERROR_FAILED_HANDLED) {
            gchar *volume_name = g_volume_get_name(volume);
            gchar *primary = g_markup_printf_escaped(_("Failed to mount \"%s\""),
                                                     volume_name);
            xfce_message_dialog(NULL,
                                _("Mount Failed"), "dialog-error",
                                primary, error->message,
                                XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                                NULL);
            g_free(primary);
            g_free(volume_name);
        }

        g_clear_error(&error);
    } else {
        g_clear_object(&icon->mount);
        icon->mount = g_volume_get_mount(volume);

        if (icon->mount != NULL) {
            g_clear_object(&icon->file);
            icon->file = g_mount_get_root(icon->mount);

            xfdesktop_volume_icon_fetch_file_info(icon, xfdesktop_volume_icon_mounted_file_info_ready);
        } else {
            if(icon->file)
                g_object_unref(icon->file);
            icon->file = NULL;

            xfdesktop_file_icon_update_file_info(XFDESKTOP_FILE_ICON(icon), NULL);
        }
    }
}

static void
xfdesktop_volume_icon_fetch_file_info(XfdesktopVolumeIcon *volume_icon,
                                      GAsyncReadyCallback callback)
{
    GWeakRef *weak_ref;

    g_return_if_fail(G_IS_FILE(volume_icon->file));

    weak_ref = g_slice_new0(GWeakRef);
    g_weak_ref_init(weak_ref, G_OBJECT(volume_icon));

    if (volume_icon->file_info_op_handle != NULL) {
        g_cancellable_cancel(volume_icon->file_info_op_handle);
        g_object_unref(volume_icon->file_info_op_handle);
    }
    volume_icon->file_info_op_handle = g_cancellable_new();
    g_file_query_info_async(volume_icon->file,
                            XFDESKTOP_FILE_INFO_NAMESPACE,
                            G_FILE_QUERY_INFO_NONE,
                            G_PRIORITY_DEFAULT,
                            volume_icon->file_info_op_handle,
                            callback,
                            weak_ref);
}

static void
xfdesktop_volume_icon_fetch_filesystem_info(XfdesktopVolumeIcon *volume_icon)
{
    GWeakRef *weak_ref;

    g_return_if_fail(G_IS_FILE(volume_icon->file));

    weak_ref = g_slice_new0(GWeakRef);
    g_weak_ref_init(weak_ref, G_OBJECT(volume_icon));

    if (volume_icon->filesystem_info_op_handle != NULL) {
        g_cancellable_cancel(volume_icon->filesystem_info_op_handle);
        g_object_unref(volume_icon->filesystem_info_op_handle);
    }
    volume_icon->filesystem_info_op_handle = g_cancellable_new();
    g_file_query_filesystem_info_async(volume_icon->file,
                                       XFDESKTOP_FILESYSTEM_INFO_NAMESPACE,
                                       G_PRIORITY_DEFAULT,
                                       volume_icon->filesystem_info_op_handle,
                                       xfdesktop_volume_icon_filesystem_info_ready,
                                       weak_ref);
}

static void
xfdesktop_volume_icon_menu_mount(GtkWidget *widget, gpointer user_data)
{
    XfdesktopVolumeIcon *icon = XFDESKTOP_VOLUME_ICON(user_data);
    if (icon->volume != NULL && icon->mount == NULL) {
        GtkWindow *toplevel = xfdesktop_find_toplevel(widget);
        GMountOperation *operation = gtk_mount_operation_new(toplevel);
        gtk_mount_operation_set_screen(GTK_MOUNT_OPERATION(operation), icon->gscreen);

        g_volume_mount(icon->volume,
                       G_MOUNT_MOUNT_NONE,
                       operation,
                       NULL,
                       xfdesktop_volume_icon_mount_finish,
                       g_object_ref(icon));

        g_object_unref(operation);
    }
}

static void
xfdesktop_volume_icon_menu_unmount(GtkWidget *widget, gpointer user_data)
{
    XfdesktopVolumeIcon *icon = XFDESKTOP_VOLUME_ICON(user_data);
    if (icon->mount != NULL) {
#ifdef HAVE_LIBNOTIFY
        xfdesktop_notify_unmount(icon->mount);
#endif

        GtkWindow *toplevel = xfdesktop_find_toplevel(widget);
        GMountOperation *operation = gtk_mount_operation_new(toplevel);
        gtk_mount_operation_set_screen(GTK_MOUNT_OPERATION(operation), icon->gscreen);

        g_mount_unmount_with_operation(icon->mount,
                                       G_MOUNT_UNMOUNT_NONE,
                                       operation,
                                       NULL,
                                       xfdesktop_volume_icon_unmount_finish,
                                       g_object_ref(icon));

        g_object_unref(operation);
    }
}

static void
xfdesktop_volume_icon_menu_eject(GtkWidget *widget,
                                 gpointer user_data)
{
    XfdesktopVolumeIcon *icon = XFDESKTOP_VOLUME_ICON(user_data);

    GVolume *volume = icon->volume;
    GMount *mount = icon->mount;

    if ((volume != NULL && g_volume_can_eject(volume)) || (mount != NULL && g_mount_can_eject(mount))) {
        GtkWindow *toplevel = xfdesktop_find_toplevel(widget);
        GMountOperation *operation = gtk_mount_operation_new(toplevel);
        gtk_mount_operation_set_screen(GTK_MOUNT_OPERATION(operation),
                                       icon->gscreen);

        if (icon->volume != NULL && g_volume_can_eject(icon->volume)) {
#ifdef HAVE_LIBNOTIFY
            xfdesktop_notify_eject_volume(icon->volume);
#endif
            g_volume_eject_with_operation(icon->volume,
                                          G_MOUNT_UNMOUNT_NONE,
                                          operation,
                                          NULL,
                                          xfdesktop_volume_icon_eject_finish,
                                          g_object_ref(icon));
        } else if (icon->mount != NULL && g_mount_can_eject(icon->mount)) {
#ifdef HAVE_LIBNOTIFY
            xfdesktop_notify_eject_mount(icon->mount);
#endif
            g_mount_eject_with_operation(icon->mount,
                                         G_MOUNT_UNMOUNT_NONE,
                                         operation,
                                         NULL,
                                         xfdesktop_volume_icon_eject_finish,
                                         g_object_ref(icon));
        }
        g_object_unref(operation);
    } else {
        /* If we can't eject the volume try to unmount it */
        xfdesktop_volume_icon_menu_unmount(widget, user_data);
    }
}

static void
xfdesktop_volume_icon_menu_properties(GtkWidget *widget,
                                      gpointer user_data)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(user_data);
    GFile *file = xfdesktop_file_icon_peek_file(icon);
    GList file_l = {
        .data = file,
        .next = NULL,
    };

    xfdesktop_file_utils_show_properties_dialog(&file_l,
                                                XFDESKTOP_VOLUME_ICON(icon)->gscreen,
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
    mi = xfdesktop_menu_create_menu_item_with_mnemonic(icon_label, img);
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

static void
xfdesktop_volume_icon_open_activated(GtkWidget *item,
                                     XfdesktopIcon *icon)
{
    GtkWidget *toplevel = NULL;
    GtkWidget *menu = gtk_widget_get_parent(item);

    if (GTK_IS_MENU(menu)) {
        toplevel = gtk_menu_get_attach_widget(GTK_MENU(menu));
        if (!GTK_IS_WINDOW(toplevel)) {
            toplevel = NULL;
        }
    } else {
        toplevel = gtk_widget_get_toplevel(item);
    }

    xfdesktop_volume_icon_activate(icon, GTK_WINDOW(toplevel));
}

static gboolean
xfdesktop_volume_icon_populate_context_menu(XfdesktopIcon *icon,
                                            GtkWidget *menu)
{
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(icon);
    GVolume *volume = volume_icon->volume;
    GMount *mount = volume_icon->mount;
    GtkWidget *mi, *img;
    const gchar *icon_name, *icon_label;

    icon_name = "document-open";

    img = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_MENU);
    mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("_Open"), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_volume_icon_open_activated), icon);

    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

    if ((volume != NULL && g_volume_can_eject(volume)) || (mount != NULL && g_mount_can_eject(mount))) {
        GDrive              *drive;
        GDriveStartStopType  start_stop_type = G_DRIVE_START_STOP_TYPE_UNKNOWN;

        drive = volume != NULL ? g_volume_get_drive (volume) : (mount != NULL ? g_mount_get_drive(mount) : NULL);
        if (drive != NULL)
          {
            start_stop_type = g_drive_get_start_stop_type (drive);
            g_object_unref (drive);
          }

        switch (start_stop_type)
          {
          case G_DRIVE_START_STOP_TYPE_SHUTDOWN:
            icon_label = _("Safely _Remove Volume");
            break;
          case G_DRIVE_START_STOP_TYPE_NETWORK:
            icon_label = _("_Disconnect Volume");
            break;
          case G_DRIVE_START_STOP_TYPE_MULTIDISK:
            icon_label = _("Stop the _Multi-Disk Drive");
            break;
          case G_DRIVE_START_STOP_TYPE_PASSWORD:
            icon_label = _("_Lock Volume");
            break;
          default:
            icon_label = _("_Eject Volume");
          }
        icon_name = "media-eject";
        xfdesktop_volume_icon_add_context_menu_option(icon, icon_name, icon_label,
                        menu, G_CALLBACK(xfdesktop_volume_icon_menu_eject));
    }

    if(mount && g_mount_can_unmount(mount)) {
        icon_name = "drive-removable-media";
        icon_label = _("_Unmount Volume");
        xfdesktop_volume_icon_add_context_menu_option(icon, icon_name, icon_label,
                        menu, G_CALLBACK(xfdesktop_volume_icon_menu_unmount));
    }

    if (mount == NULL && volume != NULL) {
        icon_name = "drive-removable-media";
        icon_label = _("_Mount Volume");
        xfdesktop_volume_icon_add_context_menu_option(icon, icon_name, icon_label,
                        menu, G_CALLBACK(xfdesktop_volume_icon_menu_mount));
    }

    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

    icon_name = "document-properties";
    icon_label = _("_Properties...");

    if(!volume_icon->file_info)
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
    return XFDESKTOP_VOLUME_ICON(icon)->file_info;
}

static GFileInfo *
xfdesktop_volume_icon_peek_filesystem_info(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon), NULL);
    return XFDESKTOP_VOLUME_ICON(icon)->filesystem_info;
}

static GFile *
xfdesktop_volume_icon_peek_file(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon), NULL);
    return XFDESKTOP_VOLUME_ICON(icon)->file;
}

static void
xfdesktop_volume_icon_update_file_info(XfdesktopFileIcon *icon,
                                       GFileInfo *info)
{
    XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(icon);

    g_return_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon));

    TRACE("entering");

    /* just replace the file info here */
    if(volume_icon->file_info)
        g_object_unref(volume_icon->file_info);
    volume_icon->file_info = info ? g_object_ref(info) : NULL;

    /* update the filesystem info as well */
    g_clear_object(&volume_icon->filesystem_info);
    if (volume_icon->file != NULL) {
        xfdesktop_volume_icon_fetch_filesystem_info(volume_icon);
    }

    /* invalidate the tooltip */
    if(volume_icon->tooltip) {
        g_free(volume_icon->tooltip);
        volume_icon->tooltip = NULL;
    }

    /* not really easy to check if this changed or not, so just invalidate it */
    xfdesktop_file_icon_invalidate_icon(XFDESKTOP_FILE_ICON(icon));
    xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(icon));
}

static gboolean
xfdesktop_volume_icon_activate(XfdesktopIcon *icon_p,
                               GtkWindow *window)
{
    XfdesktopVolumeIcon *icon = XFDESKTOP_VOLUME_ICON(icon_p);

    TRACE("entering");

    if (icon->mount == NULL) {
        // Pass the window so the mount finish callback knows to activate as well
        g_object_set_qdata_full(G_OBJECT(icon), xfdesktop_volume_icon_activate_window_quark,
                                g_object_ref(window),
                                g_object_unref);

        /* mount the volume and open the folder in the mount finish callback */
        xfdesktop_volume_icon_menu_mount(NULL, icon);

        return TRUE;
    } else {
        /* chain up to the parent class (where the mount point folder is
         * opened in the file manager) */
        return XFDESKTOP_ICON_CLASS(xfdesktop_volume_icon_parent_class)->activate(icon_p, window);
    }
}

static guint
xfdesktop_volume_icon_hash(XfdesktopFileIcon *icon)
{
    return g_str_hash(xfdesktop_file_icon_peek_sort_key(icon));
}

static gchar *
xfdesktop_volume_icon_get_sort_key(XfdesktopFileIcon *icon)
{
    XfdesktopVolumeIcon *vicon = XFDESKTOP_VOLUME_ICON(icon);
    g_return_val_if_fail(vicon->volume != NULL || vicon->mount != NULL, NULL);

    if (vicon->volume != NULL) {
        return xfdesktop_volume_icon_sort_key_for_volume(vicon->volume);
    } else if (vicon->mount != NULL) {
        return xfdesktop_volume_icon_sort_key_for_mount(vicon->mount);
    } else {
        g_assert_not_reached();
        return NULL;
    }
}

static void
xfdesktop_volume_icon_file_info_ready(GObject *source,
                                      GAsyncResult *result,
                                      gpointer user_data)
{
    GFile *file = G_FILE(source);
    GWeakRef *volume_icon_ref = user_data;
    gpointer volume_icon_p = g_weak_ref_get(volume_icon_ref);

    g_weak_ref_clear(volume_icon_ref);
    g_slice_free(GWeakRef, volume_icon_ref);

    if (G_UNLIKELY(volume_icon_p == NULL)) {
        GFileInfo *file_info = g_file_query_info_finish(file, result, NULL);
        if (file_info != NULL) {
            g_object_unref(file_info);
        }
    } else {
        XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(volume_icon_p);
        GFileInfo *file_info;
        GError *error = NULL;

        g_clear_object(&volume_icon->file_info);
        g_clear_object(&volume_icon->file_info_op_handle);

        file_info = g_file_query_info_finish(file, result, &error);
        if (file_info != NULL) {
            volume_icon->file_info = file_info;
            xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(volume_icon));
            xfdesktop_icon_label_changed(XFDESKTOP_ICON(volume_icon));
        } else {
            if (error != NULL) {
                g_printerr("Failed to query new volume icon file info (%d, %d): %s\n",
                           error->domain, error->code, error->message);
                g_error_free(error);
            }
        }
    }
}

static void
xfdesktop_volume_icon_filesystem_info_ready(GObject *source,
                                            GAsyncResult *result,
                                            gpointer user_data)
{
    GFile *file = G_FILE(source);
    GWeakRef *volume_icon_ref = user_data;
    gpointer volume_icon_p = g_weak_ref_get(volume_icon_ref);

    g_weak_ref_clear(volume_icon_ref);
    g_slice_free(GWeakRef, volume_icon_ref);

    if (G_UNLIKELY(volume_icon_p == NULL)) {
        GFileInfo *file_info = g_file_query_filesystem_info_finish(file, result, NULL);
        if (file_info != NULL) {
            g_object_unref(file_info);
        }
    } else {
        XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(volume_icon_p);
        GFileInfo *file_info;
        GError *error = NULL;

        g_clear_object(&volume_icon->filesystem_info);
        g_clear_object(&volume_icon->filesystem_info_op_handle);

        file_info = g_file_query_filesystem_info_finish(G_FILE(source), result, &error);
        if (file_info != NULL) {
            volume_icon->filesystem_info = file_info;
        } else {
            if (error != NULL) {
                g_printerr("Failed to query new volume icon filesystem info (%d, %d): %s\n",
                           error->domain, error->code, error->message);
                g_error_free(error);
            }
        }
    }
}

static XfdesktopVolumeIcon *
xfdesktop_volume_icon_new(GVolume *volume, GMount *mount, GdkScreen *screen) {
    XfdesktopVolumeIcon *volume_icon;

    g_return_val_if_fail(G_IS_VOLUME(volume) || G_IS_MOUNT(mount), NULL);

    volume_icon = g_object_new(XFDESKTOP_TYPE_VOLUME_ICON, NULL);
    volume_icon->gscreen = screen;

    if (volume != NULL) {
        volume_icon->volume = g_object_ref(volume);
    } else {
        volume_icon->volume = g_mount_get_volume(mount);
    }
    if (mount != NULL) {
        volume_icon->mount = g_object_ref(mount);
    } else {
        volume_icon->mount = g_volume_get_mount(volume);
    }

    if (volume_icon->mount != NULL) {
        volume_icon->file = g_mount_get_root(volume_icon->mount);
        xfdesktop_volume_icon_fetch_file_info(volume_icon, xfdesktop_volume_icon_file_info_ready);
        xfdesktop_volume_icon_fetch_filesystem_info(volume_icon);
    }

    return volume_icon;
}

XfdesktopVolumeIcon *
xfdesktop_volume_icon_new_for_volume(GVolume *volume, GdkScreen *screen) {
    return xfdesktop_volume_icon_new(volume, NULL, screen);
}

XfdesktopVolumeIcon *
xfdesktop_volume_icon_new_for_mount(GMount *mount, GdkScreen *screen) {
    return xfdesktop_volume_icon_new(NULL, mount, screen);
}


GVolume *
xfdesktop_volume_icon_peek_volume(XfdesktopVolumeIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon), NULL);
    return icon->volume;
}

GMount *
xfdesktop_volume_icon_peek_mount(XfdesktopVolumeIcon *icon) {
    g_return_val_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon), NULL);
    return icon->mount;
}

void
xfdesktop_volume_icon_mounted(XfdesktopVolumeIcon *icon, GMount *mount) {
    g_return_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon));
    g_return_if_fail(G_IS_MOUNT(mount));

    if (icon->mount != mount) {
        if (icon->mount != NULL) {
            DBG("Strange, we got a new mount but also had an old mount");
            xfdesktop_volume_icon_unmounted(icon);
        }

        icon->mount = g_object_ref(mount);
        icon->file = g_mount_get_root(icon->mount);

        xfdesktop_volume_icon_fetch_file_info(icon, xfdesktop_volume_icon_file_info_ready);
        xfdesktop_volume_icon_fetch_filesystem_info(icon);

        g_clear_pointer(&icon->tooltip, g_free);

        /* not really easy to check if this changed or not, so just invalidate it */
        xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(icon));
    }
}

void
xfdesktop_volume_icon_unmounted(XfdesktopVolumeIcon *icon) {
    g_return_if_fail(XFDESKTOP_IS_VOLUME_ICON(icon));

    if (icon->mount != NULL) {
        g_clear_object(&icon->mount);
        g_clear_object(&icon->file);
        g_clear_object(&icon->file_info);
        g_clear_object(&icon->filesystem_info);

        g_clear_pointer(&icon->tooltip, g_free);

        xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(icon));
    }
}

gchar *
xfdesktop_volume_icon_sort_key_for_volume(GVolume *volume)
{
    gchar *sort_key = NULL;

    if (G_UNLIKELY(g_volume_get_sort_key(G_VOLUME(volume)) != NULL)) {
        sort_key = g_strdup(g_volume_get_sort_key(volume));
    }

    if (G_UNLIKELY(sort_key == NULL)) {
        for (gsize i = 0; i < G_N_ELEMENTS(idents_for_sort_key); ++i) {
            sort_key = g_volume_get_identifier(volume, idents_for_sort_key[i]);
            if (sort_key != NULL) {
                break;
            }
        }
    }

    if (G_UNLIKELY(sort_key == NULL)) {
        sort_key = g_volume_get_name(volume);
    }

    return sort_key;
}

gchar *
xfdesktop_volume_icon_sort_key_for_mount(GMount *mount) {
    gchar *sort_key = NULL;

    if (G_UNLIKELY(g_mount_get_sort_key(mount) != NULL)) {
        sort_key = g_strdup(g_mount_get_sort_key(mount));
    }

    if (G_UNLIKELY(sort_key == NULL)) {
        sort_key = g_mount_get_uuid(mount);
    }

    if (G_UNLIKELY(sort_key == NULL)) {
        GFile *root = g_mount_get_root(mount);
        if (root != NULL) {
            sort_key = g_file_get_uri(root);
            g_object_unref(root);
        }
    }

    if (G_UNLIKELY(sort_key == NULL)) {
        sort_key = g_mount_get_name(mount);
    }

    return sort_key;
}
