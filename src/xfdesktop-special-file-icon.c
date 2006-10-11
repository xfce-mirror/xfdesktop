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

#include <dbus/dbus-glib.h>

#ifdef HAVE_THUNARX
#include <thunarx/thunarx.h>
#endif

#include "xfdesktop-file-utils.h"
#include "xfdesktop-icon.h"
#include "xfdesktop-file-icon.h"
#include "xfdesktop-dbus-bindings-trash.h"
#include "xfdesktop-special-file-icon.h"

struct _XfdesktopSpecialFileIconPrivate
{
    XfdesktopSpecialFileIconType type;
    GdkPixbuf *pix;
    gchar *tooltip;
    gint cur_pix_size;
    ThunarVfsInfo *info;
    GdkScreen *gscreen;
    
    /* only needed for trash */
    DBusGProxy *dbus_proxy;
    gboolean trash_full;
};

static void xfdesktop_special_file_icon_finalize(GObject *obj);

static void xfdesktop_special_file_icon_icon_init(XfdesktopIconIface *iface);
static GdkPixbuf *xfdesktop_special_file_icon_peek_pixbuf(XfdesktopIcon *icon,
                                                          gint size);
static G_CONST_RETURN gchar *xfdesktop_special_file_icon_peek_label(XfdesktopIcon *icon);
static G_CONST_RETURN gchar *xfdesktop_special_file_icon_peek_tooltip(XfdesktopIcon *icon);
static gboolean xfdesktop_special_file_icon_is_drop_dest(XfdesktopIcon *icon);
static XfdesktopIconDragResult xfdesktop_special_file_icon_do_drop_dest(XfdesktopIcon *icon,
                                                                        XfdesktopIcon *src_icon,
                                                                        GdkDragAction action);
static GtkWidget *xfdesktop_special_file_icon_get_popup_menu(XfdesktopIcon *icon);

static G_CONST_RETURN ThunarVfsInfo *xfdesktop_special_file_icon_peek_info(XfdesktopFileIcon *icon);

#ifdef HAVE_THUNARX
static void xfdesktop_special_file_icon_tfi_init(ThunarxFileInfoIface *iface);
#endif


static inline void xfdesktop_special_file_icon_invalidate_pixbuf(XfdesktopSpecialFileIcon *icon);
static void xfdesktop_special_file_icon_trash_changed_cb(DBusGProxy *proxy,
                                                         gboolean trash_full,
                                                         gpointer user_data);


#ifdef HAVE_THUNARX
G_DEFINE_TYPE_EXTENDED(XfdesktopSpecialFileIcon, xfdesktop_special_file_icon,
                       XFDESKTOP_TYPE_FILE_ICON, 0,
                       G_IMPLEMENT_INTERFACE(XFDESKTOP_TYPE_ICON,
                                             xfdesktop_special_file_icon_icon_init)
                       G_IMPLEMENT_INTERFACE(THUNARX_TYPE_FILE_INFO,
                                             xfdesktop_special_file_icon_tfi_init)
                       )
#else
G_DEFINE_TYPE_EXTENDED(XfdesktopSpecialFileIcon, xfdesktop_special_file_icon,
                       XFDESKTOP_TYPE_FILE_ICON, 0,
                       G_IMPLEMENT_INTERFACE(XFDESKTOP_TYPE_ICON,
                                             xfdesktop_special_file_icon_icon_init)
#endif



static void
xfdesktop_special_file_icon_class_init(XfdesktopSpecialFileIconClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    XfdesktopFileIconClass *file_icon_class = (XfdesktopFileIconClass *)klass;
    
    g_type_class_add_private(klass, sizeof(XfdesktopSpecialFileIconPrivate));
    
    gobject_class->finalize = xfdesktop_special_file_icon_finalize;
    
    file_icon_class->peek_info = xfdesktop_special_file_icon_peek_info;
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
    
    if(icon->priv->info)
        thunar_vfs_info_unref(icon->priv->info);
    
    if(icon->priv->tooltip)
        g_free(icon->priv->tooltip);
    
    if(icon->priv->dbus_proxy) {
        dbus_g_proxy_disconnect_signal(icon->priv->dbus_proxy, "TrashChanged",
                                       G_CALLBACK(xfdesktop_special_file_icon_trash_changed_cb),
                                       icon);
        g_object_unref(G_OBJECT(icon->priv->dbus_proxy));
    }
    
    G_OBJECT_CLASS(xfdesktop_special_file_icon_parent_class)->finalize(obj);
}

static void
xfdesktop_special_file_icon_icon_init(XfdesktopIconIface *iface)
{
    iface->peek_pixbuf = xfdesktop_special_file_icon_peek_pixbuf;
    iface->peek_label = xfdesktop_special_file_icon_peek_label;
    iface->peek_tooltip = xfdesktop_special_file_icon_peek_tooltip;
    iface->is_drop_dest = xfdesktop_special_file_icon_is_drop_dest;
    iface->do_drop_dest = xfdesktop_special_file_icon_do_drop_dest;
    iface->get_popup_menu = xfdesktop_special_file_icon_get_popup_menu;
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
    iface->get_vfs_info = xfdesktop_thunarx_file_info_get_vfs_info;
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
        GtkIconTheme *icon_theme = gtk_icon_theme_get_for_screen(file_icon->priv->gscreen);
        
        if(XFDESKTOP_SPECIAL_FILE_ICON_HOME == file_icon->priv->type) {
            if(gtk_icon_theme_has_icon(icon_theme, "user-home"))
                custom_icon_name = "user-home";
            else if(gtk_icon_theme_has_icon(icon_theme, "gnome-fs-desktop"))
                custom_icon_name = "gnome-fs-desktop";
        } else if(XFDESKTOP_SPECIAL_FILE_ICON_TRASH == file_icon->priv->type) {
            if(file_icon->priv->trash_full) {
                if(gtk_icon_theme_has_icon(icon_theme, "user-trash-full"))
                    custom_icon_name = "user-trash-full";
                else if(gtk_icon_theme_has_icon(icon_theme, "gnome-fs-trash-full"))
                    custom_icon_name = "gnome-fs-trash-full";
            } else {
                if(gtk_icon_theme_has_icon(icon_theme, "user-trash"))
                    custom_icon_name = "user-trash";
                else if(gtk_icon_theme_has_icon(icon_theme, "gnome-fs-trash-empty"))
                    custom_icon_name = "gnome-fs-trash-empty";
            }
        }
        
        file_icon->priv->pix = xfdesktop_file_utils_get_file_icon(custom_icon_name,
                                                                  file_icon->priv->info,
                                                                  size,
                                                                  NULL,
                                                                  100);
        
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

static gboolean
xfdesktop_special_file_icon_is_drop_dest(XfdesktopIcon *icon)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    /* FIXME: i suppose '/' could be writable if we're foolishly running as root */
    return (XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM != special_file_icon->priv->type);
}

static void
xfdesktop_special_file_icon_drag_job_error(ThunarVfsJob *job,
                                           GError *error,
                                           gpointer user_data)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(user_data);
    XfdesktopFileIcon *src_file_icon = g_object_get_data(G_OBJECT(job),
                                                         "--xfdesktop-src-file-icon");
    XfdesktopFileUtilsFileop fileop = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(job),
                                                                        "--xfdesktop-file-icon-action"));
    const ThunarVfsInfo *src_info = xfdesktop_file_icon_peek_info(src_file_icon);
    
    g_return_if_fail(special_file_icon && src_file_icon);
    
    xfdesktop_file_utils_handle_fileop_error(NULL, src_info,
                                             special_file_icon->priv->info,
                                             fileop, error);
}

static ThunarVfsInteractiveJobResponse
xfdesktop_special_file_icon_interactive_job_ask(ThunarVfsJob *job,
                                                const gchar *message,
                                                ThunarVfsInteractiveJobResponse choices,
                                                gpointer user_data)
{
    return xfdesktop_file_utils_interactive_job_ask(NULL, message, choices);
}

static void
xfdesktop_special_file_icon_drag_job_finished(ThunarVfsJob *job,
                                      gpointer user_data)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(user_data);
    XfdesktopFileIcon *src_file_icon = g_object_get_data(G_OBJECT(job),
                                                         "--xfdesktop-src-file-icon");
    
    if(!xfdesktop_file_icon_remove_active_job(XFDESKTOP_FILE_ICON(special_file_icon), job))
        g_critical("ThunarVfsJob 0x%p not found in active jobs list", job);
    
    if(!xfdesktop_file_icon_remove_active_job(src_file_icon, job))
        g_critical("ThunarVfsJob 0x%p not found in active jobs list", job);
    
    g_object_unref(G_OBJECT(job));
    
    g_object_unref(G_OBJECT(src_file_icon));
    g_object_unref(G_OBJECT(special_file_icon));
}

static XfdesktopIconDragResult
xfdesktop_special_file_icon_do_drop_dest(XfdesktopIcon *icon,
                                         XfdesktopIcon *src_icon,
                                         GdkDragAction action)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    XfdesktopFileIcon *src_file_icon = XFDESKTOP_FILE_ICON(src_icon);
    const ThunarVfsInfo *src_info;
    ThunarVfsJob *job = NULL;
    const gchar *name;
    ThunarVfsPath *dest_path;
    
    DBG("entering");
    
    g_return_val_if_fail(special_file_icon && src_file_icon,
                         XFDESKTOP_ICON_DRAG_FAILED);
    g_return_val_if_fail(xfdesktop_special_file_icon_is_drop_dest(icon),
                         XFDESKTOP_ICON_DRAG_FAILED);
    
    src_info = xfdesktop_file_icon_peek_info(src_file_icon);
    if(!src_info)
        return XFDESKTOP_ICON_DRAG_FAILED;
    
    name = thunar_vfs_path_get_name(src_info->path);
    g_return_val_if_fail(name, XFDESKTOP_ICON_DRAG_FAILED);
        
    dest_path = thunar_vfs_path_relative(special_file_icon->priv->info->path,
                                         name);
    g_return_val_if_fail(dest_path, XFDESKTOP_ICON_DRAG_FAILED);
    
    if(special_file_icon->priv->type == XFDESKTOP_SPECIAL_FILE_ICON_TRASH)  {
        /* any drop to the trash is a move */
        action = GDK_ACTION_MOVE;
    }
    
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
        g_object_ref(G_OBJECT(special_file_icon));
        
        g_object_set_data(G_OBJECT(job), "--xfdesktop-file-icon-callback",
                          G_CALLBACK(xfdesktop_special_file_icon_drag_job_finished));
        g_object_set_data(G_OBJECT(job), "--xfdesktop-file-icon-data", icon);
        xfdesktop_file_icon_add_active_job(XFDESKTOP_FILE_ICON(special_file_icon),
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
                         G_CALLBACK(xfdesktop_special_file_icon_drag_job_error),
                         special_file_icon);
        g_signal_connect(G_OBJECT(job), "ask",
                         G_CALLBACK(xfdesktop_special_file_icon_interactive_job_ask),
                         special_file_icon);
        g_signal_connect(G_OBJECT(job), "finished",
                         G_CALLBACK(xfdesktop_special_file_icon_drag_job_finished),
                         special_file_icon);
        
        return XFDESKTOP_ICON_DRAG_SUCCEEDED_NO_ACTION;
    } else
        return XFDESKTOP_ICON_DRAG_FAILED;
    
    return XFDESKTOP_ICON_DRAG_FAILED;
}

static G_CONST_RETURN gchar *
xfdesktop_special_file_icon_peek_tooltip(XfdesktopIcon *icon)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    
    /* FIXME: implement trash stuff */
    
    if(!special_file_icon->priv->tooltip) {
        gchar mod[64], *kind, sizebuf[64], *size;
        struct tm *tm = localtime(&special_file_icon->priv->info->mtime);

        strftime(mod, 64, "%Y-%m-%d %H:%M:%S", tm);
        kind = xfdesktop_file_utils_get_file_kind(special_file_icon->priv->info, NULL);
        thunar_vfs_humanize_size(special_file_icon->priv->info->size, sizebuf, 64);
        size = g_strdup_printf(_("%s (%" G_GINT64_FORMAT " Bytes)"), sizebuf,
                              (gint64)special_file_icon->priv->info->size);
        
        special_file_icon->priv->tooltip = g_strdup_printf(_("Kind: %s\nModified:%s\nSize: %s"),
                                                   kind, mod, size);
        
        g_free(kind);
        g_free(size);
    }
    
    return special_file_icon->priv->tooltip;
}

static void
xfdesktop_special_file_icon_trash_handle_error(GdkScreen *gscreen,
                                               const gchar *method,
                                               const gchar *message)
{
    GtkWidget *dlg = xfce_message_dialog_new(NULL, _("Trash Error"),
                                             GTK_STOCK_DIALOG_WARNING,
                                             _("Unable to contact the Xfce Trash service."),
                                             _("Make sure you have a file manager installed that supports the Xfce Trash service, such as Thunar."),
                                             GTK_STOCK_CLOSE,
                                             GTK_RESPONSE_ACCEPT, NULL);
    gtk_widget_unrealize(dlg);
    xfce_gtk_window_center_on_monitor(GTK_WINDOW(dlg), gscreen, 0);
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
    if(error) {
        xfdesktop_special_file_icon_trash_handle_error(GDK_SCREEN(user_data),
                                                       "DisplayTrash",
                                                        error->message);
    }
}

static void
xfdesktop_special_file_icon_trash_empty_cb(DBusGProxy *proxy,
                                           GError *error,
                                           gpointer user_data)
{
    if(error) {
        xfdesktop_special_file_icon_trash_handle_error(GDK_SCREEN(user_data),
                                                       "EmptyTrash",
                                                        error->message);
    }
}

static void
xfdesktop_special_file_icon_trash_open(GtkWidget *w,
                                       gpointer user_data)
{
    XfdesktopSpecialFileIcon *file_icon = XFDESKTOP_SPECIAL_FILE_ICON(user_data);
    
    if(G_LIKELY(file_icon->priv->dbus_proxy)) {
        gchar *display_name = gdk_screen_make_display_name(file_icon->priv->gscreen);
        
        if(!org_xfce_Trash_display_trash_async(file_icon->priv->dbus_proxy,
                                               display_name,
                                               xfdesktop_special_file_icon_trash_open_cb,
                                               file_icon->priv->gscreen))
        {
            xfdesktop_special_file_icon_trash_handle_error(file_icon->priv->gscreen,
                                                           "DisplayTrash",
                                                            NULL);
        }
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
        
        if(!org_xfce_Trash_empty_trash_async(file_icon->priv->dbus_proxy,
                                             display_name,
                                             xfdesktop_special_file_icon_trash_empty_cb,
                                             file_icon->priv->gscreen))
        {
            xfdesktop_special_file_icon_trash_handle_error(file_icon->priv->gscreen,
                                                           "EmptyTrash",
                                                            NULL);
        }
        g_free(display_name);
    }
}

static GtkWidget *
xfdesktop_special_file_icon_get_popup_menu(XfdesktopIcon *icon)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    GtkWidget *menu, *mi, *img;
    
    if(XFDESKTOP_SPECIAL_FILE_ICON_TRASH != special_file_icon->priv->type)
        return NULL;
    
    menu = gtk_menu_new();
    
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
    
    mi = gtk_image_menu_item_new_with_mnemonic(_("_Empty Trash"));
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    if(special_file_icon->priv->trash_full) {
        g_signal_connect(G_OBJECT(mi), "activate",
                         G_CALLBACK(xfdesktop_special_file_icon_trash_empty),
                         icon);
    } else
        gtk_widget_set_sensitive(mi, FALSE);
    
    return menu;
}


static G_CONST_RETURN ThunarVfsInfo *
xfdesktop_special_file_icon_peek_info(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_SPECIAL_FILE_ICON(icon), NULL);
    return XFDESKTOP_SPECIAL_FILE_ICON(icon)->priv->info;
}


static void
xfdesktop_special_file_icon_trash_changed_cb(DBusGProxy *proxy,
                                             gboolean trash_full,
                                             gpointer user_data)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(user_data);
    
    TRACE("entering (%p, %d, %p)", proxy, trash_full, user_data);
    
    if(trash_full == special_file_icon->priv->trash_full)
        return;
    
    special_file_icon->priv->trash_full = trash_full;
    
    xfdesktop_special_file_icon_invalidate_pixbuf(special_file_icon);
    xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(special_file_icon));
}

static void
xfdesktop_special_file_icon_query_trash_cb(DBusGProxy *proxy,
                                           gboolean trash_full,
                                           GError *error,
                                           gpointer user_data)
{
    XfdesktopSpecialFileIcon *icon = XFDESKTOP_SPECIAL_FILE_ICON(user_data);
    
    if(error) {
        xfdesktop_special_file_icon_trash_handle_error(icon->priv->gscreen,
                                                       "QueryTrash",
                                                       error->message);
    } else
        icon->priv->trash_full = trash_full;
}


/* public API */

XfdesktopSpecialFileIcon *
xfdesktop_special_file_icon_new(XfdesktopSpecialFileIconType type,
                                GdkScreen *screen)
{
    XfdesktopSpecialFileIcon *special_file_icon;
    ThunarVfsPath *path = NULL;
    
    switch(type) {
        case XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM:
            path = thunar_vfs_path_get_for_root();
            break;
        
        case XFDESKTOP_SPECIAL_FILE_ICON_HOME:
            path = thunar_vfs_path_get_for_home();
            break;
        
        case XFDESKTOP_SPECIAL_FILE_ICON_TRASH:
            path = thunar_vfs_path_get_for_trash();
            break;
        
        default:
            g_return_val_if_reached(NULL);
    }
    
    special_file_icon = g_object_new(XFDESKTOP_TYPE_SPECIAL_FILE_ICON, NULL);
    special_file_icon->priv->type = type;
    special_file_icon->priv->gscreen = screen;
    special_file_icon->priv->info = thunar_vfs_info_new_for_path(path, NULL);
    thunar_vfs_path_unref(path);
    if(G_UNLIKELY(!special_file_icon->priv->info)) {
        g_object_unref(G_OBJECT(special_file_icon));
        return NULL;
    }
    
    if(XFDESKTOP_SPECIAL_FILE_ICON_TRASH == type) {
        DBusGProxy *trash_proxy = xfdesktop_file_utils_peek_trash_proxy();
            
        if(G_LIKELY(trash_proxy)) {
            special_file_icon->priv->dbus_proxy = g_object_ref(G_OBJECT(trash_proxy));
            dbus_g_proxy_connect_signal(special_file_icon->priv->dbus_proxy,
                                        "TrashChanged",
                                        G_CALLBACK(xfdesktop_special_file_icon_trash_changed_cb),
                                        special_file_icon, NULL);
            
            if(!org_xfce_Trash_query_trash_async(special_file_icon->priv->dbus_proxy,
                                                 xfdesktop_special_file_icon_query_trash_cb,
                                                 special_file_icon))
            {
                xfdesktop_special_file_icon_trash_handle_error(special_file_icon->priv->gscreen,
                                                               "QueryTrash",
                                                               NULL);
            }
        } else {
            /* we might as well just bail here */
            g_object_unref(G_OBJECT(special_file_icon));
            return NULL;
        }
    }
    
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
