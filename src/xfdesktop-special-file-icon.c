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
    DBusGProxyCall *dbus_querytrash_call;
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
static void xfdesktop_special_file_icon_trash_changed_cb(DBusGProxy *proxy,
                                                         gboolean trash_full,
                                                         gpointer user_data);


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
    
    if(icon->priv->dbus_proxy) {
        if(icon->priv->dbus_querytrash_call) {
            dbus_g_proxy_cancel_call(icon->priv->dbus_proxy,
                                     icon->priv->dbus_querytrash_call);
        }
        dbus_g_proxy_disconnect_signal(icon->priv->dbus_proxy, "TrashChanged",
                                       G_CALLBACK(xfdesktop_special_file_icon_trash_changed_cb),
                                       icon);
        g_object_unref(G_OBJECT(icon->priv->dbus_proxy));
    }
    
    if(icon->priv->pix)
        g_object_unref(G_OBJECT(icon->priv->pix));
    
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

static GdkDragAction
xfdesktop_special_file_icon_get_allowed_drag_actions(XfdesktopIcon *icon)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    GdkDragAction actions = 0;
    
    switch(special_file_icon->priv->type) {
        case XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM:
            /* move is just impossible, and copy seems a bit retarded.  link
             * is possible, but thunar-vfs doesn't support it (deliberately). */
            actions = 0;
            break;
        
        case XFDESKTOP_SPECIAL_FILE_ICON_HOME:
            /* user shouldn't be able to move their own homedir.  copy might
             * be a little silly, but allow it anyway.  link is fine. */
            actions = GDK_ACTION_COPY | GDK_ACTION_LINK;
            break;
            
        case XFDESKTOP_SPECIAL_FILE_ICON_TRASH:
            /* i don't think we can even do a link here; thunar doesn't let
             * us, anyway. */
            actions = 0;
            break;
    }
    
    return actions;
}

static GdkDragAction
xfdesktop_special_file_icon_get_allowed_drop_actions(XfdesktopIcon *icon)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    const ThunarVfsInfo *info;
    GdkDragAction actions = 0;
    
    switch(special_file_icon->priv->type) {
        case XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM:
            /* we should hope the user isn't running as root, but we might as
             * well let them hang themselves if they are */
            info = xfdesktop_file_icon_peek_info(XFDESKTOP_FILE_ICON(icon));
            if(info && info->flags & THUNAR_VFS_FILE_FLAGS_WRITABLE)
                actions = GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK;
            else
                actions = 0;
            break;
        
        case XFDESKTOP_SPECIAL_FILE_ICON_HOME:
            /* assume the user can write to their own home directory */
            actions = GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK;
            break;
            
        case XFDESKTOP_SPECIAL_FILE_ICON_TRASH:
            actions = GDK_ACTION_MOVE;  /* anything else is just silly */
            break;
    }
    
    return actions;
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
    
    g_return_if_fail(special_file_icon);

    if(!src_file_icon)
        return;
    
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
    GtkWidget *icon_view = xfdesktop_icon_peek_icon_view(XFDESKTOP_ICON(user_data));
    GtkWidget *toplevel = gtk_widget_get_toplevel(icon_view);
    return xfdesktop_file_utils_interactive_job_ask(GTK_WINDOW(toplevel),
                                                    message, choices);
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

static gboolean
xfdesktop_special_file_icon_do_drop_dest(XfdesktopIcon *icon,
                                         XfdesktopIcon *src_icon,
                                         GdkDragAction action)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    XfdesktopFileIcon *src_file_icon = XFDESKTOP_FILE_ICON(src_icon);
    const ThunarVfsInfo *src_info;
    ThunarVfsJob *job = NULL;
    const gchar *name = NULL;
    ThunarVfsPath *dest_path;
    
    DBG("entering");
    
    g_return_val_if_fail(special_file_icon && src_file_icon, FALSE);
    g_return_val_if_fail(xfdesktop_special_file_icon_get_allowed_drop_actions(icon),
                         FALSE);
    
    src_info = xfdesktop_file_icon_peek_info(src_file_icon);
    if(!src_info)
        return FALSE;
    
    if(thunar_vfs_path_is_root(src_info->path))
        return FALSE;
    
    name = thunar_vfs_path_get_name(src_info->path);
    if(!name)
        return FALSE;
    
    dest_path = thunar_vfs_path_relative(special_file_icon->priv->info->path,
                                         name);
    if(!dest_path)
        return FALSE;
    
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
        
        return TRUE;
    } else
        return FALSE;
    
    return FALSE;
}

static G_CONST_RETURN gchar *
xfdesktop_special_file_icon_peek_tooltip(XfdesktopIcon *icon)
{
    XfdesktopSpecialFileIcon *special_file_icon = XFDESKTOP_SPECIAL_FILE_ICON(icon);
    
    if(!special_file_icon->priv->tooltip) {
        if(XFDESKTOP_SPECIAL_FILE_ICON_TRASH == special_file_icon->priv->type) {
            /* FIXME: also display # of items in trash */
            special_file_icon->priv->tooltip = g_strdup(_("Kind: Trash"));
        } else {
            gchar mod[64], *kind, sizebuf[64], *size;
            struct tm *tm = localtime(&special_file_icon->priv->info->mtime);

            strftime(mod, 64, "%Y-%m-%d %H:%M:%S", tm);
            kind = xfdesktop_file_utils_get_file_kind(special_file_icon->priv->info,
                                                      NULL);
            thunar_vfs_humanize_size(special_file_icon->priv->info->size,
                                     sizebuf, 64);
            size = g_strdup_printf(_("%s (%" G_GINT64_FORMAT " Bytes)"),
                                   sizebuf,
                                  (gint64)special_file_icon->priv->info->size);
            
            special_file_icon->priv->tooltip = g_strdup_printf(_("Kind: %s\nModified:%s\nSize: %s"),
                                                               kind, mod, size);
            
            g_free(kind);
            g_free(size);
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
        xfdesktop_special_file_icon_trash_handle_error(icon,
                                                       "QueryTrash",
                                                       error->message);
    } else {
        icon->priv->trash_full = trash_full;
        xfdesktop_special_file_icon_invalidate_pixbuf(icon);
        xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(icon));
    }
    
    icon->priv->dbus_querytrash_call = NULL;
}


/* public API */

XfdesktopSpecialFileIcon *
xfdesktop_special_file_icon_new(XfdesktopSpecialFileIconType type,
                                GdkScreen *screen)
{
    XfdesktopSpecialFileIcon *special_file_icon;
    ThunarVfsPath *path = NULL;
    gchar *pathname;
    
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

    /* convert the ThunarVfsPath into a GFile */
    pathname = thunar_vfs_path_dup_string(special_file_icon->priv->info->path);
    special_file_icon->priv->file = g_file_new_for_path(pathname);
    g_free(pathname);

    /* query file information from GIO */
    special_file_icon->priv->file_info = g_file_query_info(special_file_icon->priv->file,
                                                           XFDESKTOP_FILE_INFO_NAMESPACE,
                                                           G_FILE_QUERY_INFO_NONE,
                                                           NULL, NULL);

    /* query file system information from GIO */
    special_file_icon->priv->filesystem_info = g_file_query_filesystem_info(special_file_icon->priv->file,
                                                                            XFDESKTOP_FILESYSTEM_INFO_NAMESPACE,
                                                                            NULL, NULL);

    if(G_UNLIKELY(!special_file_icon->priv->info)) {
        g_object_unref(G_OBJECT(special_file_icon));
        return NULL;
    }
    
    if(XFDESKTOP_SPECIAL_FILE_ICON_TRASH == type) {
        DBusGProxy *trash_proxy = xfdesktop_file_utils_peek_trash_proxy();
            
        if(G_LIKELY(trash_proxy)) {
            DBusGProxyCall *call;
            
            special_file_icon->priv->dbus_proxy = g_object_ref(G_OBJECT(trash_proxy));
            dbus_g_proxy_connect_signal(special_file_icon->priv->dbus_proxy,
                                        "TrashChanged",
                                        G_CALLBACK(xfdesktop_special_file_icon_trash_changed_cb),
                                        special_file_icon, NULL);
            
            call = xfdesktop_trash_proxy_query_trash_async(special_file_icon->priv->dbus_proxy,
                                                           xfdesktop_special_file_icon_query_trash_cb,
                                                           special_file_icon);
            if(!call) {
                xfdesktop_special_file_icon_trash_handle_error(special_file_icon,
                                                               "QueryTrash",
                                                               NULL);
            }
            special_file_icon->priv->dbus_querytrash_call = call;
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
