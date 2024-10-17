/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006      Brian Tarricone, <brian@tarricone.org>
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

#include "xfdesktop-common.h"
#include "xfdesktop-file-utils.h"
#include "xfdesktop-special-file-icon.h"

struct _XfdesktopSpecialFileIcon
{
    XfdesktopFileIcon parent_instance;

    XfdesktopSpecialFileIconType type;
    gchar *tooltip;
    GFileMonitor *monitor;
    GFileInfo *file_info;
    GFileInfo *filesystem_info;
    GFile *file;
    GdkScreen *gscreen;

    /* only needed for trash */
    guint trash_item_count;
};

static void xfdesktop_special_file_icon_finalize(GObject *obj);

static const gchar *xfdesktop_special_file_icon_peek_label(XfdesktopIcon *icon);
static const gchar *xfdesktop_special_file_icon_peek_tooltip(XfdesktopIcon *icon);
static gboolean xfdesktop_special_file_icon_populate_context_menu(XfdesktopIcon *icon,
                                                                  GtkWidget *menu);

static GIcon * xfdesktop_special_file_icon_get_gicon(XfdesktopFileIcon *icon);
static GFileInfo *xfdesktop_special_file_icon_peek_file_info(XfdesktopFileIcon *icon);
static GFileInfo *xfdesktop_special_file_icon_peek_filesystem_info(XfdesktopFileIcon *icon);
static GFile *xfdesktop_special_file_icon_peek_file(XfdesktopFileIcon *icon);
static void xfdesktop_special_file_icon_changed(GFileMonitor *monitor,
                                                GFile *file,
                                                GFile *other_file,
                                                GFileMonitorEvent event,
                                                XfdesktopSpecialFileIcon *special_file_icon);
static void xfdesktop_special_file_icon_update_trash_count(XfdesktopSpecialFileIcon *special_file_icon);

#ifdef HAVE_THUNARX
static void xfdesktop_special_file_icon_tfi_init(ThunarxFileInfoIface *iface);

G_DEFINE_TYPE_EXTENDED(XfdesktopSpecialFileIcon, xfdesktop_special_file_icon,
                       XFDESKTOP_TYPE_FILE_ICON, 0,
                       G_IMPLEMENT_INTERFACE(THUNARX_TYPE_FILE_INFO,
                                             xfdesktop_special_file_icon_tfi_init))
#else
G_DEFINE_TYPE(XfdesktopSpecialFileIcon, xfdesktop_special_file_icon, XFDESKTOP_TYPE_FILE_ICON)
#endif



static void
xfdesktop_special_file_icon_class_init(XfdesktopSpecialFileIconClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    XfdesktopIconClass *icon_class = (XfdesktopIconClass *)klass;
    XfdesktopFileIconClass *file_icon_class = (XfdesktopFileIconClass *)klass;

    gobject_class->finalize = xfdesktop_special_file_icon_finalize;

    icon_class->peek_label = xfdesktop_special_file_icon_peek_label;
    icon_class->peek_tooltip = xfdesktop_special_file_icon_peek_tooltip;
    icon_class->populate_context_menu = xfdesktop_special_file_icon_populate_context_menu;

    file_icon_class->get_gicon = xfdesktop_special_file_icon_get_gicon;
    file_icon_class->peek_file_info = xfdesktop_special_file_icon_peek_file_info;
    file_icon_class->peek_filesystem_info = xfdesktop_special_file_icon_peek_filesystem_info;
    file_icon_class->peek_file = xfdesktop_special_file_icon_peek_file;
}

static void
xfdesktop_special_file_icon_init(XfdesktopSpecialFileIcon *icon) {}

static void
xfdesktop_special_file_icon_finalize(GObject *obj)
{
    XfdesktopSpecialFileIcon *icon = XFDESKTOP_SPECIAL_FILE_ICON(obj);

    if(icon->monitor) {
        g_signal_handlers_disconnect_by_func(icon->monitor,
                                             G_CALLBACK(xfdesktop_special_file_icon_changed),
                                             icon);
        g_object_unref(icon->monitor);
    }

    g_object_unref(icon->file);

    if(icon->file_info)
        g_object_unref(icon->file_info);

    if (icon->filesystem_info != NULL) {
        g_object_unref(icon->filesystem_info);
    }

    if(icon->tooltip)
        g_free(icon->tooltip);

    G_OBJECT_CLASS(xfdesktop_special_file_icon_parent_class)->finalize(obj);
}

#ifdef HAVE_THUNARX
static gchar *
xfdesktop_special_file_icon_tfi_get_uri_scheme(ThunarxFileInfo *file_info)
{
    XfdesktopSpecialFileIcon *icon = XFDESKTOP_SPECIAL_FILE_ICON(file_info);

    if(XFDESKTOP_SPECIAL_FILE_ICON_TRASH == icon->type)
        return g_strdup("trash");
    else
        return g_strdup("file");
}

static void
xfdesktop_special_file_icon_tfi_init(ThunarxFileInfoIface *iface)
{
    iface->get_name = xfdesktop_thunarx_file_info_get_name;
    iface->get_uri = xfdesktop_thunarx_file_info_get_uri;
    iface->get_parent_uri = xfdesktop_thunarx_file_info_get_parent_uri;
    iface->get_uri_scheme = xfdesktop_special_file_icon_tfi_get_uri_scheme;
    iface->get_mime_type = xfdesktop_thunarx_file_info_get_mime_type;
    iface->has_mime_type = xfdesktop_thunarx_file_info_has_mime_type;
    iface->is_directory = xfdesktop_thunarx_file_info_is_directory;
    iface->get_file_info = xfdesktop_thunarx_file_info_get_file_info;
    iface->get_filesystem_info = xfdesktop_thunarx_file_info_get_filesystem_info;
    iface->get_location = xfdesktop_thunarx_file_info_get_location;
}
#endif  /* HAVE_THUNARX */


static GIcon *
xfdesktop_special_file_icon_get_gicon(XfdesktopFileIcon *icon)
{
    XfdesktopSpecialFileIcon *special_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    gchar *icon_name = NULL;
    GFile *parent = NULL;
    GIcon *base_gicon = NULL;
    GIcon *gicon = NULL;

    TRACE("entering");

    /* use a custom icon name for the local filesystem root */
    parent = g_file_get_parent(special_icon->file);
    if(!parent && g_file_has_uri_scheme(special_icon->file, "file"))
        icon_name = g_strdup("drive-harddisk");

    if(parent)
        g_object_unref(parent);

    /* use a custom icon for the trash, based on it having files
     * the user can delete */
    if(special_icon->type == XFDESKTOP_SPECIAL_FILE_ICON_TRASH) {
        if(special_icon->trash_item_count == 0)
            icon_name = g_strdup("user-trash");
        else
            icon_name = g_strdup("user-trash-full");
    }

    /* Create the themed icon for it */
    if(icon_name) {
        base_gicon = g_themed_icon_new(icon_name);
        g_free(icon_name);
    }

    /* If we still don't have an icon, use the default */
    if(!G_IS_ICON(base_gicon)) {
        base_gicon = g_file_info_get_icon(special_icon->file_info);
        if(G_IS_ICON(base_gicon))
            g_object_ref(base_gicon);
    }

    /* Add any user set emblems */
    gicon = xfdesktop_file_icon_add_emblems(file_icon, base_gicon);
    g_object_unref(base_gicon);

    return gicon;
}

static const gchar *
xfdesktop_special_file_icon_peek_label(XfdesktopIcon *icon)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    GFileInfo *info = special_file_icon->file_info;

    if(XFDESKTOP_SPECIAL_FILE_ICON_HOME == special_file_icon->type)
        return _("Home");
    else if(XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM == special_file_icon->type)
        return _("File System");
    else if(XFDESKTOP_SPECIAL_FILE_ICON_TRASH == special_file_icon->type)
        return _("Trash");
    else
        return info ? g_file_info_get_display_name(info) : NULL;
}

static const gchar *
xfdesktop_special_file_icon_peek_tooltip(XfdesktopIcon *icon)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);

    if(!special_file_icon->tooltip) {
        GFileInfo *info = xfdesktop_file_icon_peek_file_info(XFDESKTOP_FILE_ICON(icon));

        if(!info)
            return NULL;

        if(XFDESKTOP_SPECIAL_FILE_ICON_TRASH == special_file_icon->type) {
            if(special_file_icon->trash_item_count == 0) {
                special_file_icon->tooltip = g_strdup(_("Trash is empty"));
            } else {
                special_file_icon->tooltip = g_strdup_printf(g_dngettext(GETTEXT_PACKAGE,
                                                                         _("Trash contains one item"),
                                                                         _("Trash contains %d items"),
                                                                         special_file_icon->trash_item_count),
                                                             special_file_icon->trash_item_count);
            }
        } else {
            const gchar *description;
            gchar *size_string, *time_string;
            guint64 size, mtime;

            if(special_file_icon->type == XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM)
                description = _("File System");
            else if(special_file_icon->type == XFDESKTOP_SPECIAL_FILE_ICON_HOME)
                description = _("Home");
            else {
                description = g_file_info_get_attribute_string(info,
                                                               G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION);
            }

            size = g_file_info_get_attribute_uint64(info,
                                                    G_FILE_ATTRIBUTE_STANDARD_SIZE);

            size_string = g_format_size(size);

            mtime = g_file_info_get_attribute_uint64(info,
                                                     G_FILE_ATTRIBUTE_TIME_MODIFIED);
            time_string = xfdesktop_file_utils_format_time_for_display(mtime);

            special_file_icon->tooltip =
                g_strdup_printf(_("%s\nSize: %s\nLast modified: %s"),
                                description, size_string, time_string);

            g_free(size_string);
            g_free(time_string);
        }
    }

    return special_file_icon->tooltip;
}

static void
xfdesktop_special_file_icon_trash_open(GtkWidget *w,
                                       gpointer user_data)
{
    XfdesktopSpecialFileIcon *file_icon = XFDESKTOP_SPECIAL_FILE_ICON(user_data);
    GtkWindow *toplevel = xfdesktop_find_toplevel(w);
    GList link = {
        .data = file_icon->file,
        .prev = NULL,
        .next = NULL,
    };

    xfdesktop_file_utils_open_folders(&link,
                                      file_icon->gscreen,
                                      toplevel);
}

static void
xfdesktop_special_file_icon_trash_empty(GtkWidget *w,
                                        gpointer user_data)
{
    XfdesktopSpecialFileIcon *file_icon = XFDESKTOP_SPECIAL_FILE_ICON(user_data);
    GtkWindow *toplevel = xfdesktop_find_toplevel(w);

    xfdesktop_file_utils_empty_trash(file_icon->gscreen, toplevel);
}

static gboolean
xfdesktop_special_file_icon_populate_context_menu(XfdesktopIcon *icon,
                                                  GtkWidget *menu)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    GtkWidget *mi, *img;

    if(XFDESKTOP_SPECIAL_FILE_ICON_TRASH != special_file_icon->type)
        return FALSE;

    img = gtk_image_new_from_icon_name("document-open", GTK_ICON_SIZE_MENU);
    mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("_Open"), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_special_file_icon_trash_open), icon);

    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

    if(special_file_icon->trash_item_count == 0) {
        img = gtk_image_new_from_icon_name("user-trash", GTK_ICON_SIZE_MENU);
    } else {
        img = gtk_image_new_from_icon_name("user-trash-full", GTK_ICON_SIZE_MENU);
    }

    mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("_Empty Trash"), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    if(special_file_icon->trash_item_count > 0) {
        g_signal_connect(G_OBJECT(mi), "activate",
                         G_CALLBACK(xfdesktop_special_file_icon_trash_empty),
                         icon);
    } else
        gtk_widget_set_sensitive(mi, FALSE);

    return TRUE;
}

static GFileInfo *
xfdesktop_special_file_icon_peek_file_info(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_SPECIAL_FILE_ICON(icon), NULL);
    return XFDESKTOP_SPECIAL_FILE_ICON(icon)->file_info;
}

static GFileInfo *
xfdesktop_special_file_icon_peek_filesystem_info(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_SPECIAL_FILE_ICON(icon), NULL);
    return XFDESKTOP_SPECIAL_FILE_ICON(icon)->filesystem_info;
}

static GFile *
xfdesktop_special_file_icon_peek_file(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_SPECIAL_FILE_ICON(icon), NULL);
    return XFDESKTOP_SPECIAL_FILE_ICON(icon)->file;
}

static void
xfdesktop_special_file_icon_changed(GFileMonitor *monitor,
                                    GFile *file,
                                    GFile *other_file,
                                    GFileMonitorEvent event,
                                    XfdesktopSpecialFileIcon *special_file_icon)
{
    g_return_if_fail(G_IS_FILE_MONITOR(monitor));
    g_return_if_fail(G_IS_FILE(file));
    g_return_if_fail(XFDESKTOP_IS_SPECIAL_FILE_ICON(special_file_icon));

    /* We don't care about change events only created/deleted */
    if(event == G_FILE_MONITOR_EVENT_CHANGED ||
       event == G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED ||
       event == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
        return;

    /* release the old file information */
    if(special_file_icon->file_info) {
        g_object_unref(special_file_icon->file_info);
        special_file_icon->file_info = NULL;
    }

    /* release the old file system information */
    if(special_file_icon->filesystem_info) {
        g_object_unref(special_file_icon->filesystem_info);
        special_file_icon->filesystem_info = NULL;
    }

    /* reload the file information */
    special_file_icon->file_info = g_file_query_info(special_file_icon->file,
                                                     XFDESKTOP_FILE_INFO_NAMESPACE,
                                                     G_FILE_QUERY_INFO_NONE,
                                                     NULL, NULL);

    /* reload the file system information */
    special_file_icon->filesystem_info = g_file_query_filesystem_info(special_file_icon->file,
                                                                      XFDESKTOP_FILESYSTEM_INFO_NAMESPACE,
                                                                      NULL, NULL);

    /* update the trash full state */
    if(special_file_icon->type == XFDESKTOP_SPECIAL_FILE_ICON_TRASH)
        xfdesktop_special_file_icon_update_trash_count(special_file_icon);

    /* invalidate the tooltip */
    g_free(special_file_icon->tooltip);
    special_file_icon->tooltip = NULL;

    /* update the icon */
    xfdesktop_file_icon_invalidate_icon(XFDESKTOP_FILE_ICON(special_file_icon));
    xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(special_file_icon));
}

static void
xfdesktop_special_file_icon_update_trash_count(XfdesktopSpecialFileIcon *special_file_icon)
{
    GFileEnumerator *enumerator;
    GFileInfo *f_info;
    gint n = 0;

    g_return_if_fail(XFDESKTOP_IS_SPECIAL_FILE_ICON(special_file_icon));

    if(special_file_icon->file_info == NULL
       || special_file_icon->type != XFDESKTOP_SPECIAL_FILE_ICON_TRASH)
    {
        return;
    }

    /* The trash count may return a number of files the user can't
     * currently delete, for example if the file is in a removable
     * drive that isn't mounted.
     */
    enumerator = g_file_enumerate_children(special_file_icon->file,
                                           G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE,
                                           G_FILE_QUERY_INFO_NONE,
                                           NULL,
                                           NULL);
    if(enumerator == NULL)
        return;

    for(f_info = g_file_enumerator_next_file(enumerator, NULL, NULL);
        f_info != NULL;
        f_info = g_file_enumerator_next_file(enumerator, NULL, NULL))
    {
          n++;
          g_object_unref(f_info);
    }

    g_file_enumerator_close(enumerator, NULL, NULL);
    g_object_unref(enumerator);

    special_file_icon->trash_item_count = n;
    TRACE("exiting, trash count %d", n);
}

/* public API */


GFile *
xfdesktop_special_file_icon_file_for_type(XfdesktopSpecialFileIconType type)
{
    switch (type) {
        case XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM:
            return g_file_new_for_uri("file:///");
        case XFDESKTOP_SPECIAL_FILE_ICON_HOME:
            return g_file_new_for_path(xfce_get_homedir());
        case XFDESKTOP_SPECIAL_FILE_ICON_TRASH:
            return g_file_new_for_uri("trash:///");
        default:
            g_assert_not_reached();
    }
}

XfdesktopSpecialFileIcon *
xfdesktop_special_file_icon_new(XfdesktopSpecialFileIconType type,
                                GdkScreen *screen)
{
    XfdesktopSpecialFileIcon *special_file_icon;
    GFile *file = xfdesktop_special_file_icon_file_for_type(type);

    special_file_icon = g_object_new(XFDESKTOP_TYPE_SPECIAL_FILE_ICON, NULL);
    special_file_icon->type = type;
    special_file_icon->gscreen = screen;
    special_file_icon->file = file;

    special_file_icon->file_info = g_file_query_info(file,
                                                     XFDESKTOP_FILE_INFO_NAMESPACE,
                                                     G_FILE_QUERY_INFO_NONE,
                                                     NULL, NULL);

    if(!special_file_icon->file_info) {
        g_object_unref(special_file_icon);
        return NULL;
    }

    /* query file system information from GIO */
    special_file_icon->filesystem_info = g_file_query_filesystem_info(special_file_icon->file,
                                                                      XFDESKTOP_FILESYSTEM_INFO_NAMESPACE,
                                                                      NULL, NULL);
    /* update the trash full state */
    if(type == XFDESKTOP_SPECIAL_FILE_ICON_TRASH)
        xfdesktop_special_file_icon_update_trash_count(special_file_icon);

    special_file_icon->monitor = g_file_monitor(special_file_icon->file,
                                                      G_FILE_MONITOR_NONE,
                                                      NULL, NULL);
    if(special_file_icon->monitor) {
        g_signal_connect(special_file_icon->monitor,
                         "changed",
                         G_CALLBACK(xfdesktop_special_file_icon_changed),
                         special_file_icon);
    }

    return special_file_icon;
}

XfdesktopSpecialFileIconType
xfdesktop_special_file_icon_get_icon_type(XfdesktopSpecialFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_SPECIAL_FILE_ICON(icon), -1);
    return icon->type;
}
