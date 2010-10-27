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

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include <gio/gio.h>

#include <libxfce4ui/libxfce4ui.h>

#include <dbus/dbus-glib.h>

#ifdef HAVE_THUNARX
#include <thunarx/thunarx.h>
#endif

#include "xfdesktop-common.h"
#include "xfdesktop-file-utils.h"
#include "xfdesktop-special-file-icon.h"
#include "xfdesktop-trash-proxy.h"

struct _XfdesktopSpecialFileIconPrivate
{
    XfdesktopSpecialFileIconType type;
    GdkPixbuf *pix;
    gchar *tooltip;
    gint cur_pix_size;
    ThunarVfsInfo *info;
    GFileInfo *file_info;
    GFileInfo *filesystem_info;
    GFile *file;
    GdkScreen *gscreen;
    
    /* only needed for trash */
    DBusGProxy *dbus_proxy;
    gboolean trash_full;
};

static void xfdesktop_special_file_icon_finalize(GObject *obj);

static GdkPixbuf *xfdesktop_special_file_icon_peek_pixbuf(XfdesktopIcon *icon,
                                                          gint size);
static G_CONST_RETURN gchar *xfdesktop_special_file_icon_peek_label(XfdesktopIcon *icon);
static G_CONST_RETURN gchar *xfdesktop_special_file_icon_peek_tooltip(XfdesktopIcon *icon);
static GdkDragAction xfdesktop_special_file_icon_get_allowed_drag_actions(XfdesktopIcon *icon);
static GdkDragAction xfdesktop_special_file_icon_get_allowed_drop_actions(XfdesktopIcon *icon);
static gboolean xfdesktop_special_file_icon_do_drop_dest(XfdesktopIcon *icon,
                                                         XfdesktopIcon *src_icon,
                                                         GdkDragAction action);
static gboolean xfdesktop_special_file_icon_populate_context_menu(XfdesktopIcon *icon,
                                                                  GtkWidget *menu);

static G_CONST_RETURN ThunarVfsInfo *xfdesktop_special_file_icon_peek_info(XfdesktopFileIcon *icon);
static GFileInfo *xfdesktop_special_file_icon_peek_file_info(XfdesktopFileIcon *icon);
static GFileInfo *xfdesktop_special_file_icon_peek_filesystem_info(XfdesktopFileIcon *icon);
static GFile *xfdesktop_special_file_icon_peek_file(XfdesktopFileIcon *icon);

#ifdef HAVE_THUNARX
static void xfdesktop_special_file_icon_tfi_init(ThunarxFileInfoIface *iface);
#endif


static inline void xfdesktop_special_file_icon_invalidate_pixbuf(XfdesktopSpecialFileIcon *icon);


#ifdef HAVE_THUNARX
G_DEFINE_TYPE_EXTENDED(XfdesktopSpecialFileIcon, xfdesktop_special_file_icon,
                       XFDESKTOP_TYPE_FILE_ICON, 0,
                       G_IMPLEMENT_INTERFACE(THUNARX_TYPE_FILE_INFO,
                                             xfdesktop_special_file_icon_tfi_init)
                       )
#else
G_DEFINE_TYPE(XfdesktopSpecialFileIcon, xfdesktop_special_file_icon,
              XFDESKTOP_TYPE_FILE_ICON)
#endif



static void
xfdesktop_special_file_icon_class_init(XfdesktopSpecialFileIconClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    XfdesktopIconClass *icon_class = (XfdesktopIconClass *)klass;
    XfdesktopFileIconClass *file_icon_class = (XfdesktopFileIconClass *)klass;
    
    g_type_class_add_private(klass, sizeof(XfdesktopSpecialFileIconPrivate));
    
    gobject_class->finalize = xfdesktop_special_file_icon_finalize;
    
    icon_class->peek_pixbuf = xfdesktop_special_file_icon_peek_pixbuf;
    icon_class->peek_label = xfdesktop_special_file_icon_peek_label;
    icon_class->peek_tooltip = xfdesktop_special_file_icon_peek_tooltip;
    icon_class->get_allowed_drag_actions = xfdesktop_special_file_icon_get_allowed_drag_actions;
    icon_class->get_allowed_drop_actions = xfdesktop_special_file_icon_get_allowed_drop_actions;
    icon_class->do_drop_dest = xfdesktop_special_file_icon_do_drop_dest;
    icon_class->populate_context_menu = xfdesktop_special_file_icon_populate_context_menu;
    
    file_icon_class->peek_info = xfdesktop_special_file_icon_peek_info;
    file_icon_class->peek_file_info = xfdesktop_special_file_icon_peek_file_info;
    file_icon_class->peek_filesystem_info = xfdesktop_special_file_icon_peek_filesystem_info;
    file_icon_class->peek_file = xfdesktop_special_file_icon_peek_file;
    file_icon_class->can_rename_file = (gboolean (*)(XfdesktopFileIcon *))gtk_false;
    file_icon_class->can_delete_file = (gboolean (*)(XfdesktopFileIcon *))gtk_false;
}

static void
xfdesktop_special_file_icon_init(XfdesktopSpecialFileIcon *icon)
{
    icon->priv = G_TYPE_INSTANCE_GET_PRIVATE(icon,
                                             XFDESKTOP_TYPE_SPECIAL_FILE_ICON,
                                             XfdesktopSpecialFileIconPrivate);
}

static void
xfdesktop_special_file_icon_finalize(GObject *obj)
{
    XfdesktopSpecialFileIcon *icon = XFDESKTOP_SPECIAL_FILE_ICON(obj);
    GtkIconTheme *itheme = gtk_icon_theme_get_for_screen(icon->priv->gscreen);
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(itheme),
                                         G_CALLBACK(xfdesktop_special_file_icon_invalidate_pixbuf),
                                         icon);
    
    if(icon->priv->pix)
        g_object_unref(G_OBJECT(icon->priv->pix));

    g_object_unref(icon->priv->file);

    if(icon->priv->file_info)
        g_object_unref(icon->priv->file_info);
    
    if(icon->priv->info)
        thunar_vfs_info_unref(icon->priv->info);
    
    if(icon->priv->tooltip)
        g_free(icon->priv->tooltip);
    
    G_OBJECT_CLASS(xfdesktop_special_file_icon_parent_class)->finalize(obj);
}

#ifdef HAVE_THUNARX
static gchar *
xfdesktop_special_file_icon_tfi_get_uri_scheme(ThunarxFileInfo *file_info)
{
    XfdesktopSpecialFileIcon *icon = XFDESKTOP_SPECIAL_FILE_ICON(file_info);
    
    if(XFDESKTOP_SPECIAL_FILE_ICON_TRASH == icon->priv->type)
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

static inline void
xfdesktop_special_file_icon_invalidate_pixbuf(XfdesktopSpecialFileIcon *icon)
{
    if(icon->priv->pix) {
        g_object_unref(G_OBJECT(icon->priv->pix));
        icon->priv->pix = NULL;
    }
}

static GdkPixbuf *
xfdesktop_special_file_icon_peek_pixbuf(XfdesktopIcon *icon,
                                        gint size)
{
    XfdesktopSpecialFileIcon *file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    
    if(size != file_icon->priv->cur_pix_size)
        xfdesktop_special_file_icon_invalidate_pixbuf(file_icon);
    
    if(!file_icon->priv->pix) {
        const gchar *custom_icon_name = NULL;

        /* use a custom icon name for the local filesystem root */
        GFile *parent = g_file_get_parent(file_icon->priv->file);
        if(!parent && g_file_has_uri_scheme(file_icon->priv->file, "file"))
            custom_icon_name = "drive-harddisk";
        if(parent)
            g_object_unref(parent);
        
        file_icon->priv->pix = xfdesktop_file_utils_get_file_icon(custom_icon_name,
                                                                  file_icon->priv->file_info,
                                                                  size, NULL, 100);
        
        file_icon->priv->cur_pix_size = size;
    }
    
    return file_icon->priv->pix;
}

static G_CONST_RETURN gchar *
xfdesktop_special_file_icon_peek_label(XfdesktopIcon *icon)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    
    if(XFDESKTOP_SPECIAL_FILE_ICON_HOME == special_file_icon->priv->type)
        return _("Home");
    else
        return special_file_icon->priv->info->display_name;
}

static GdkDragAction
xfdesktop_special_file_icon_get_allowed_drag_actions(XfdesktopIcon *icon)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    GdkDragAction actions = 0;
    
    switch(special_file_icon->priv->type) {
        case XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM:
            /* move is just impossible, and copy seems a bit retarded.  link
             * is possible */
            actions = GDK_ACTION_LINK;
            break;
        
        case XFDESKTOP_SPECIAL_FILE_ICON_HOME:
            /* user shouldn't be able to move their own homedir.  copy might
             * be a little silly, but allow it anyway.  link is fine. */
            actions = GDK_ACTION_COPY | GDK_ACTION_LINK;
            break;
            
        case XFDESKTOP_SPECIAL_FILE_ICON_TRASH:
            /* move is impossible, but we can copy and link the trash root
             * anywhere */
            actions = GDK_ACTION_COPY | GDK_ACTION_LINK;
            break;
    }
    
    return actions;
}

static GdkDragAction
xfdesktop_special_file_icon_get_allowed_drop_actions(XfdesktopIcon *icon)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    GFileInfo *info;
    GdkDragAction actions = 0;

    if(special_file_icon->priv->type != XFDESKTOP_SPECIAL_FILE_ICON_TRASH) {
        info = xfdesktop_file_icon_peek_file_info(XFDESKTOP_FILE_ICON(icon));
        if(info) {
            if(g_file_info_get_attribute_boolean(info,
                                                 G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
            {
                DBG("can move, copy and link");
                actions = GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK;
            }
        }
    } else {
        DBG("can move");
        actions = GDK_ACTION_MOVE; /* everything else is just silly */
    }
    
    return actions;
}

static gboolean
xfdesktop_special_file_icon_do_drop_dest(XfdesktopIcon *icon,
                                         XfdesktopIcon *src_icon,
                                         GdkDragAction action)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    XfdesktopFileIcon *src_file_icon = XFDESKTOP_FILE_ICON(src_icon);
    GFileInfo *src_info;
    GFile *src_file;
    GFile *dest_file = NULL;
    gboolean result = FALSE;
    
    DBG("entering");
    
    g_return_val_if_fail(special_file_icon && src_file_icon, FALSE);
    g_return_val_if_fail(xfdesktop_special_file_icon_get_allowed_drop_actions(icon),
                         FALSE);
    
    src_file = xfdesktop_file_icon_peek_file(src_file_icon);

    src_info = xfdesktop_file_icon_peek_file_info(src_file_icon);
    if(!src_info)
        return FALSE;
    
    if(special_file_icon->priv->type == XFDESKTOP_SPECIAL_FILE_ICON_TRASH) {
        GList files;

        DBG("doing trash");

        /* fake a file list */
        files.data = src_file;
        files.prev = files.next = NULL;

        /* let the trash service handle the trash operation */
        xfdesktop_file_utils_trash_files(&files, special_file_icon->priv->gscreen, NULL);
    } else {
        gchar *name = g_file_get_basename(src_file);
        if(!name)
            return FALSE;
    
        switch(action) {
            case GDK_ACTION_MOVE:
                DBG("doing move");
                dest_file = g_object_ref(special_file_icon->priv->file);
                break;
            case GDK_ACTION_COPY:
                DBG("doing copy");
                dest_file = g_file_get_child(special_file_icon->priv->file, name);
                break;
            case GDK_ACTION_LINK:
                DBG("doing link");
                dest_file = g_object_ref(special_file_icon->priv->file);
                break;
            default:
                g_warning("Unsupported drag action: %d", action);
        }

        /* let the file manager service move/copy/link the file */
        if(dest_file) {
            xfdesktop_file_utils_transfer_file(action, src_file, dest_file,
                                               special_file_icon->priv->gscreen);

            result = TRUE;
        }

        g_object_unref(dest_file);
        g_free(name);
    }

    return result;
}

static G_CONST_RETURN gchar *
xfdesktop_special_file_icon_peek_tooltip(XfdesktopIcon *icon)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    
    if(!special_file_icon->priv->tooltip) {
        GFileInfo *info = xfdesktop_file_icon_peek_file_info(XFDESKTOP_FILE_ICON(icon));

        if(!info)
            return NULL;

        if(XFDESKTOP_SPECIAL_FILE_ICON_TRASH == special_file_icon->priv->type) {
            guint item_count = g_file_info_get_attribute_uint32(info, 
                                                                G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT);

            if(item_count == 0) {
                special_file_icon->priv->tooltip = g_strdup(_("Trash is empty"));
            } else {
                special_file_icon->priv->tooltip = g_strdup_printf(g_dngettext(GETTEXT_PACKAGE,
                                                                               _("Trash contains one item"),
                                                                               _("Trash contains %d items"),
                                                                               item_count), 

                                                                   item_count);
            }
        } else {
            const gchar *description;
            gchar *size_string, *time_string;
            guint64 size, mtime;

            if(special_file_icon->priv->type == XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM)
                description = _("File System");
            else if(special_file_icon->priv->type == XFDESKTOP_SPECIAL_FILE_ICON_HOME)
                description = _("Home");
            else {
                description = g_file_info_get_attribute_string(info,
                                                               G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION);
            }

            size = g_file_info_get_attribute_uint64(info,
                                                    G_FILE_ATTRIBUTE_STANDARD_SIZE);
            size_string = g_format_size_for_display(size);

            mtime = g_file_info_get_attribute_uint64(info,
                                                     G_FILE_ATTRIBUTE_TIME_MODIFIED);
            time_string = xfdesktop_file_utils_format_time_for_display(mtime);

            special_file_icon->priv->tooltip = 
                g_strdup_printf(_("%s\nSize: %s\nLast modified: %s"),
                                description, size_string, time_string);

            g_free(size_string);
            g_free(time_string);
        }
    }
    
    return special_file_icon->priv->tooltip;
}

static void
xfdesktop_special_file_icon_trash_handle_error(XfdesktopSpecialFileIcon *icon,
                                               const gchar *method,
                                               const gchar *message)
{
    GtkWidget *icon_view = xfdesktop_icon_peek_icon_view(XFDESKTOP_ICON(icon));
    GtkWidget *toplevel = gtk_widget_get_toplevel(icon_view);
    GtkWidget *dlg = xfce_message_dialog_new(GTK_WINDOW(toplevel),
                                             _("Trash Error"),
                                             GTK_STOCK_DIALOG_WARNING,
                                             _("Unable to contact the Xfce Trash service."),
                                             _("Make sure you have a file manager installed that supports the Xfce Trash service, such as Thunar."),
                                             GTK_STOCK_CLOSE,
                                             GTK_RESPONSE_ACCEPT, NULL);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
    
    g_warning("org.xfce.Trash.%s failed: %s", method ? method : "??",
              message ? message : "??");
}

static void
xfdesktop_special_file_icon_trash_open_cb(DBusGProxy *proxy,
                                          GError *error,
                                          gpointer user_data)
{
    GtkWidget *icon_view, *toplevel;
    
    icon_view = xfdesktop_icon_peek_icon_view(XFDESKTOP_ICON(user_data));
    toplevel = gtk_widget_get_toplevel(icon_view);
    xfdesktop_file_utils_set_window_cursor(GTK_WINDOW(toplevel),
                                           GDK_LEFT_PTR);
    
    if(error) {
        xfdesktop_special_file_icon_trash_handle_error(XFDESKTOP_SPECIAL_FILE_ICON(user_data),
                                                       "DisplayTrash",
                                                        error->message);
    }
    
    g_object_unref(G_OBJECT(user_data));
}

static void
xfdesktop_special_file_icon_trash_empty_cb(DBusGProxy *proxy,
                                           GError *error,
                                           gpointer user_data)
{
    if(error) {
        xfdesktop_special_file_icon_trash_handle_error(XFDESKTOP_SPECIAL_FILE_ICON(user_data),
                                                       "EmptyTrash",
                                                        error->message);
    }
    
    g_object_unref(G_OBJECT(user_data));
}

static void
xfdesktop_special_file_icon_trash_open(GtkWidget *w,
                                       gpointer user_data)
{
    XfdesktopSpecialFileIcon *file_icon = XFDESKTOP_SPECIAL_FILE_ICON(user_data);
    
    if(G_LIKELY(file_icon->priv->dbus_proxy)) {
        gchar *display_name = gdk_screen_make_display_name(file_icon->priv->gscreen);
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        
        if(!xfdesktop_trash_proxy_display_trash_async(file_icon->priv->dbus_proxy,
                                                      display_name, startup_id,
                                                      xfdesktop_special_file_icon_trash_open_cb,
                                                      file_icon))
        {
            xfdesktop_special_file_icon_trash_handle_error(file_icon,
                                                           "DisplayTrash",
                                                            NULL);
        } else {
            GtkWidget *icon_view, *toplevel;
            
            g_object_ref(G_OBJECT(file_icon));
            
            icon_view = xfdesktop_icon_peek_icon_view(XFDESKTOP_ICON(file_icon));
            toplevel = gtk_widget_get_toplevel(icon_view);
            xfdesktop_file_utils_set_window_cursor(GTK_WINDOW(toplevel),
                                                   GDK_WATCH);
        }
            
        g_free(startup_id);
        g_free(display_name);
    }
}

static void
xfdesktop_special_file_icon_trash_empty(GtkWidget *w,
                                        gpointer user_data)
{
    XfdesktopSpecialFileIcon *file_icon = XFDESKTOP_SPECIAL_FILE_ICON(user_data);
    
    if(G_LIKELY(file_icon->priv->dbus_proxy)) {
        gchar *display_name = gdk_screen_make_display_name(file_icon->priv->gscreen);
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        
        if(!xfdesktop_trash_proxy_empty_trash_async(file_icon->priv->dbus_proxy,
                                                    display_name, startup_id,
                                                    xfdesktop_special_file_icon_trash_empty_cb,
                                                    file_icon))
        {
            xfdesktop_special_file_icon_trash_handle_error(file_icon,
                                                           "EmptyTrash",
                                                            NULL);
        } else
            g_object_ref(G_OBJECT(file_icon));

        g_free(startup_id);
        g_free(display_name);
    }
}

static gboolean
xfdesktop_special_file_icon_populate_context_menu(XfdesktopIcon *icon,
                                                  GtkWidget *menu)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    GtkWidget *mi, *img;
    GtkIconTheme *icon_theme;
    
    if(XFDESKTOP_SPECIAL_FILE_ICON_TRASH != special_file_icon->priv->type)
        return FALSE;
    
    icon_theme = gtk_icon_theme_get_default();
    
    img = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
    gtk_widget_show(img);
    mi = gtk_image_menu_item_new_with_mnemonic(_("_Open"));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_special_file_icon_trash_open), icon);
    
    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    
    if(gtk_icon_theme_has_icon(icon_theme, "user-trash"))
        img = gtk_image_new_from_icon_name("user-trash", GTK_ICON_SIZE_MENU);
    else if(gtk_icon_theme_has_icon(icon_theme, "gnome-fs-trash-empty"))
        img = gtk_image_new_from_icon_name("gnome-fs-trash-empty", GTK_ICON_SIZE_MENU);
    else
        img = NULL;
    
    mi = gtk_image_menu_item_new_with_mnemonic(_("_Empty Trash"));
    if(img)
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    if(special_file_icon->priv->trash_full) {
        g_signal_connect(G_OBJECT(mi), "activate",
                         G_CALLBACK(xfdesktop_special_file_icon_trash_empty),
                         icon);
    } else
        gtk_widget_set_sensitive(mi, FALSE);
    
    return TRUE;
}


static G_CONST_RETURN ThunarVfsInfo *
xfdesktop_special_file_icon_peek_info(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_SPECIAL_FILE_ICON(icon), NULL);
    return XFDESKTOP_SPECIAL_FILE_ICON(icon)->priv->info;
}

static GFileInfo *
xfdesktop_special_file_icon_peek_file_info(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_SPECIAL_FILE_ICON(icon), NULL);
    return XFDESKTOP_SPECIAL_FILE_ICON(icon)->priv->file_info;
}

static GFileInfo *
xfdesktop_special_file_icon_peek_filesystem_info(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_SPECIAL_FILE_ICON(icon), NULL);
    return XFDESKTOP_SPECIAL_FILE_ICON(icon)->priv->filesystem_info;
}

static GFile *
xfdesktop_special_file_icon_peek_file(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_SPECIAL_FILE_ICON(icon), NULL);
    return XFDESKTOP_SPECIAL_FILE_ICON(icon)->priv->file;
}

/* public API */

XfdesktopSpecialFileIcon *
xfdesktop_special_file_icon_new(XfdesktopSpecialFileIconType type,
                                GdkScreen *screen)
{
    XfdesktopSpecialFileIcon *special_file_icon;
    ThunarVfsPath *path = NULL;
    GFile *file = NULL;
    gchar *uri;
    
    switch(type) {
        case XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM:
            file = g_file_new_for_uri("file:///");
            break;
        
        case XFDESKTOP_SPECIAL_FILE_ICON_HOME:
            file = g_file_new_for_path(xfce_get_homedir());
            break;
        
        case XFDESKTOP_SPECIAL_FILE_ICON_TRASH:
            file = g_file_new_for_uri("trash:///");
            break;
        
        default:
            g_return_val_if_reached(NULL);
    }
    
    special_file_icon = g_object_new(XFDESKTOP_TYPE_SPECIAL_FILE_ICON, NULL);
    special_file_icon->priv->type = type;
    special_file_icon->priv->gscreen = screen;
    special_file_icon->priv->file = file;

    special_file_icon->priv->file_info = g_file_query_info(file, 
                                                           XFDESKTOP_FILE_INFO_NAMESPACE,
                                                           G_FILE_QUERY_INFO_NONE,
                                                           NULL, NULL);

    if(!special_file_icon->priv->file_info) {
        g_object_unref(special_file_icon);
        return NULL;
    }

    /* query file system information from GIO */
    special_file_icon->priv->filesystem_info = g_file_query_filesystem_info(special_file_icon->priv->file,
                                                                            XFDESKTOP_FILESYSTEM_INFO_NAMESPACE,
                                                                            NULL, NULL);

    /* query a ThunarVfsInfo for the icon URI */
    uri = g_file_get_uri(file);
    path = thunar_vfs_path_new(uri, NULL);
    special_file_icon->priv->info = thunar_vfs_info_new_for_path(path, NULL);
    thunar_vfs_path_unref(path);
    g_free(uri);
    
    g_signal_connect_swapped(G_OBJECT(gtk_icon_theme_get_for_screen(screen)),
                             "changed",
                             G_CALLBACK(xfdesktop_special_file_icon_invalidate_pixbuf),
                             special_file_icon);
    
    return special_file_icon;
}

XfdesktopSpecialFileIconType
xfdesktop_special_file_icon_get_icon_type(XfdesktopSpecialFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_SPECIAL_FILE_ICON(icon), -1);
    return icon->priv->type;
}
