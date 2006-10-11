/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006 Brian Tarricone, <bjt23@cornell.edu>
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

#include <gtk/gtk.h>

#include <libxfcegui4/libxfcegui4.h>

#include <exo/exo.h>

#include <thunar-vfs/thunar-vfs.h>

#include <dbus/dbus-glib-lowlevel.h>

#ifdef HAVE_THUNARX
#include <thunarx/thunarx.h>
#endif

#include "xfdesktop-file-icon.h"
#include "xfdesktop-file-utils.h"

ThunarVfsInteractiveJobResponse
xfdesktop_file_utils_interactive_job_ask(GtkWindow *parent,
                                         const gchar *message,
                                         ThunarVfsInteractiveJobResponse choices)
{
    GtkWidget *dlg, *btn;
    gint resp;
    
    dlg = xfce_message_dialog_new(parent, _("Question"),
                                  GTK_STOCK_DIALOG_QUESTION, NULL, message,
                                  NULL);
    
    if(choices & THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_CANCEL) {
        btn = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
        gtk_widget_show(btn);
        gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn,
                                     THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_CANCEL);
    }
    
    if(choices & THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_NO) {
        btn = gtk_button_new_from_stock(GTK_STOCK_NO);
        gtk_widget_show(btn);
        gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn,
                                     THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_NO);
    }
    
    if(choices & THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES_ALL) {
        btn = gtk_button_new_with_mnemonic(_("Yes to _all"));
        gtk_widget_show(btn);
        gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn,
                                     THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES_ALL);
    }
    
    if(choices & THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES) {
        btn = gtk_button_new_from_stock(GTK_STOCK_YES);
        gtk_widget_show(btn);
        gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn,
                                     THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES);
    }
    
    resp = gtk_dialog_run(GTK_DIALOG(dlg));
    
    gtk_widget_destroy(dlg);
    
    return (ThunarVfsInteractiveJobResponse)resp;
}

void
xfdesktop_file_utils_handle_fileop_error(GtkWindow *parent,
                                         const ThunarVfsInfo *src_info,
                                         const ThunarVfsInfo *dest_info,
                                         XfdesktopFileUtilsFileop fileop,
                                         GError *error)
{
    if(error) {
        gchar *primary_fmt, *primary;
        
        switch(fileop) {
            case XFDESKTOP_FILE_UTILS_FILEOP_MOVE:
                primary_fmt = _("There was an error moving \"%s\" to \"%s\":");
                break;
            case XFDESKTOP_FILE_UTILS_FILEOP_COPY:
                primary_fmt = _("There was an error copying \"%s\" to \"%s\":");
                break;
            case XFDESKTOP_FILE_UTILS_FILEOP_LINK:
                primary_fmt = _("There was an error linking \"%s\" to \"%s\":");
                break;
            default:
                return;
        }
        
        primary = g_strdup_printf(primary_fmt,
                                  src_info->display_name,
                                  dest_info->display_name);
        xfce_message_dialog(parent, _("File Error"), GTK_STOCK_DIALOG_ERROR,
                            primary, error->message,
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        g_free(primary);
    }
}

gchar *
xfdesktop_file_utils_get_file_kind(const ThunarVfsInfo *info,
                                   gboolean *is_link)
{
    gchar *str = NULL;

    if(!strcmp(thunar_vfs_mime_info_get_name(info->mime_info),
               "inode/symlink"))
    {
        str = g_strdup(_("broken link"));
        if(is_link)
            *is_link = TRUE;
    } else if(info->flags & THUNAR_VFS_FILE_FLAGS_SYMLINK) {
        str = g_strdup_printf(_("link to %s"),
                              thunar_vfs_mime_info_get_comment(info->mime_info));
        if(is_link)
            *is_link = TRUE;
    } else {
        str = g_strdup(thunar_vfs_mime_info_get_comment(info->mime_info));
        if(is_link)
            *is_link = FALSE;
    }
    
    return str;
}


GList *
xfdesktop_file_utils_file_icon_list_to_path_list(GList *icon_list)
{
    GList *path_list = NULL, *l;
    XfdesktopFileIcon *icon;
    const ThunarVfsInfo *info;
    
    for(l = icon_list; l; l = l->next) {
        icon = XFDESKTOP_FILE_ICON(l->data);
        info = xfdesktop_file_icon_peek_info(icon);
        if(info) {
            path_list = g_list_prepend(path_list,
                                       thunar_vfs_path_ref(info->path));
        }
    }
    
    return g_list_reverse(path_list);
}

static GdkPixbuf *xfdesktop_fallback_icon = NULL;
static gint xfdesktop_fallback_icon_size = -1;

GdkPixbuf *
xfdesktop_file_utils_get_fallback_icon(gint size)
{
    g_return_val_if_fail(size > 0, NULL);
    
    if(size != xfdesktop_fallback_icon_size && xfdesktop_fallback_icon) {
        g_object_unref(G_OBJECT(xfdesktop_fallback_icon));
        xfdesktop_fallback_icon = NULL;
    }
    
    if(!xfdesktop_fallback_icon) {
        xfdesktop_fallback_icon = gdk_pixbuf_new_from_file_at_size(DATADIR "/pixmaps/xfdesktop/xfdesktop-fallback-icon.png",
                                                                   size,
                                                                   size,
                                                                   NULL);
    }
    
    if(G_UNLIKELY(!xfdesktop_fallback_icon)) {
        GtkWidget *dummy = gtk_invisible_new();
        gtk_widget_realize(dummy);
        
        /* this is kinda crappy, but hopefully should never happen */
        xfdesktop_fallback_icon = gtk_widget_render_icon(dummy,
                                                         GTK_STOCK_MISSING_IMAGE,
                                                         (GtkIconSize)-1, NULL);
        if(gdk_pixbuf_get_width(xfdesktop_fallback_icon) != size
           || gdk_pixbuf_get_height(xfdesktop_fallback_icon) != size)
        {
            GdkPixbuf *tmp = gdk_pixbuf_scale_simple(xfdesktop_fallback_icon,
                                                     size, size,
                                                     GDK_INTERP_BILINEAR);
            g_object_unref(G_OBJECT(xfdesktop_fallback_icon));
            xfdesktop_fallback_icon = tmp;
        }
    }
    
    xfdesktop_fallback_icon_size = size;
    
    return g_object_ref(G_OBJECT(xfdesktop_fallback_icon));
}

GdkPixbuf *
xfdesktop_file_utils_get_file_icon(const gchar *custom_icon_name,
                                   ThunarVfsInfo *info,
                                   gint size,
                                   const GdkPixbuf *emblem,
                                   guint opacity)
{
    GdkPixbuf *pix_theme = NULL, *pix = NULL;
    const gchar *icon_name;
    
    if(custom_icon_name)
        pix_theme = xfce_themed_icon_load(custom_icon_name, size);
    
    if(!pix_theme && info) {
        icon_name = thunar_vfs_info_get_custom_icon(info);
        if(icon_name)
            pix_theme = xfce_themed_icon_load(icon_name, size);
    }
            
    if(G_UNLIKELY(!pix_theme) && info) {
        icon_name = thunar_vfs_mime_info_lookup_icon_name(info->mime_info,
                                                          gtk_icon_theme_get_default());
        if(icon_name)
            pix_theme = xfce_themed_icon_load(icon_name, size);
    }
    
    if(G_LIKELY(pix_theme)) {
        /* we can't edit thsese icons */
        pix = gdk_pixbuf_copy(pix_theme);
        g_object_unref(G_OBJECT(pix_theme));
        pix_theme = NULL;
    }
    
    /* fallback */
    if(G_UNLIKELY(!pix))
        pix = xfdesktop_file_utils_get_fallback_icon(size);
    
    /* sanity check */
    g_return_val_if_fail(G_LIKELY(pix), NULL);
    
    if(emblem) {
        gint emblem_pix_size = gdk_pixbuf_get_width(emblem);
        gint dest_size = size - emblem_pix_size;
        
        /* if we're using the fallback icon, we don't want to draw an emblem on
         * it, since other icons might use it without the emblem */
        if(G_UNLIKELY(pix == xfdesktop_fallback_icon)) {
            GdkPixbuf *tmp = gdk_pixbuf_copy(pix);
            g_object_unref(G_OBJECT(pix));
            pix = tmp;
        }
        
        if(dest_size < 0)
            g_critical("xfdesktop_file_utils_get_file_icon(): (dest_size > 0) failed");
        else {
            DBG("calling gdk_pixbuf_composite(%p, %p, %d, %d, %d, %d, %.1f, %.1f, %.1f, %.1f, %d, %d)",
                emblem, pix,
                dest_size, dest_size,
                emblem_pix_size, emblem_pix_size,
                (gdouble)dest_size, (gdouble)dest_size,
                1.0, 1.0, GDK_INTERP_BILINEAR, 255);
            
            gdk_pixbuf_composite(emblem, pix,
                                 dest_size, dest_size,
                                 emblem_pix_size, emblem_pix_size,
                                 dest_size, dest_size,
                                 1.0, 1.0, GDK_INTERP_BILINEAR, 255);
        }
    }
    
#ifdef HAVE_LIBEXO
    if(opacity != 100) {
        GdkPixbuf *tmp = exo_gdk_pixbuf_lucent(pix, opacity);
        g_object_unref(G_OBJECT(pix));
        pix = tmp;
    }
#endif
    
    return pix;
}

gboolean
xfdesktop_file_utils_launch_external(const ThunarVfsInfo *info,
                                     GdkScreen *screen)
{
    gboolean ret = FALSE;
    gchar *folder_name, *display_name, *commandline;
    gint status = 0;
    
    g_return_val_if_fail(info, FALSE);
    
    if(!screen)
        screen = gdk_display_get_default_screen(gdk_display_get_default());
    
    folder_name = thunar_vfs_path_dup_uri(info->path);
    display_name = gdk_screen_make_display_name(screen);
    
    /* try the org.xfce.FileManager D-BUS interface first */
    commandline = g_strdup_printf("dbus-send --print-reply --dest=org.xfce.FileManager "
                                  "/org/xfce/FileManager org.xfce.FileManager.Launch "
                                  "string:\"%s\" string:\"%s\"",
                                  folder_name, display_name);
    
    ret = (g_spawn_command_line_sync(commandline, NULL, NULL, &status, NULL)
           && status == 0);
    g_free(commandline);

    /* hardcoded fallback to a file manager if that didn't work */
    if(!ret) {
        gchar *file_manager_app = g_find_program_in_path(FILE_MANAGER_FALLBACK);
        
        if(file_manager_app) {
            commandline = g_strconcat("env DISPLAY=\"", display_name, "\" \"",
                                      file_manager_app, "\" \"", folder_name,
                                      "\"", NULL);
            
            DBG("executing:\n%s\n", commandline);
            
            ret = xfce_exec(commandline, FALSE, TRUE, NULL);
            g_free(commandline);
            g_free(file_manager_app);
        }
    }

    g_free(display_name);
    g_free(folder_name);
    
    return ret;
}


static gint dbus_ref_cnt = 0;
static DBusGProxy *dbus_trash_proxy = NULL;
static DBusGProxy *dbus_filemanager_proxy = NULL;

gboolean
xfdesktop_file_utils_dbus_init()
{
    DBusGConnection *dgconn;
    gboolean ret = TRUE;
    
    if(dbus_ref_cnt++)
        return TRUE;
    
    dgconn = dbus_g_bus_get(DBUS_BUS_SESSION, NULL);
    
    if(G_LIKELY(dgconn)) {
        /* dbus's default is brain-dead */
        DBusConnection *dconn = dbus_g_connection_get_connection(dgconn);
        dbus_connection_set_exit_on_disconnect(dconn, FALSE);
        
        dbus_trash_proxy = dbus_g_proxy_new_for_name(dgconn,
                                                     "org.xfce.FileManager",
                                                     "/org/xfce/FileManager",
                                                     "org.xfce.Trash");
        dbus_g_proxy_add_signal(dbus_trash_proxy, "TrashChanged",
                                G_TYPE_BOOLEAN, G_TYPE_INVALID);
        
        dbus_filemanager_proxy = dbus_g_proxy_new_for_name(dgconn,
                                                           "org.xfce.FileManager",
                                                           "/org/xfce/FileManager",
                                                           "org.xfce.FileManager");
        
        /* the DBusGProxy (actually, its associated DBusGProxyManager) holds
         * a reference to the DBusGConnection during its lifetime, so we
         * don't have to hold a ref. */
        dbus_g_connection_unref(dgconn);
    } else {
        ret = FALSE;
        dbus_ref_cnt = 0;
    }
    
    return ret;
}

DBusGProxy *
xfdesktop_file_utils_peek_trash_proxy()
{
    return dbus_trash_proxy;
}

DBusGProxy *
xfdesktop_file_utils_peek_filemanager_proxy()
{
    return dbus_filemanager_proxy;
}

void
xfdesktop_file_utils_dbus_cleanup()
{
    if(dbus_ref_cnt == 0 || --dbus_ref_cnt > 0)
        return;
    
    if(dbus_trash_proxy)
        g_object_unref(G_OBJECT(dbus_trash_proxy));
    if(dbus_filemanager_proxy)
        g_object_unref(G_OBJECT(dbus_filemanager_proxy));
}



#ifdef HAVE_THUNARX

/* thunar extension interface stuff: ThunarxFileInfo implementation */

gchar *
xfdesktop_thunarx_file_info_get_name(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    const ThunarVfsInfo *info = xfdesktop_file_icon_peek_info(icon);
    
    if(info)
        return g_strdup(thunar_vfs_path_get_name(info->path));
    else
        return NULL;
}

gchar *
xfdesktop_thunarx_file_info_get_uri(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    const ThunarVfsInfo *info = xfdesktop_file_icon_peek_info(icon);
    gchar buf[PATH_MAX];
    
    if(!info)
        return NULL;
        
    g_return_val_if_fail(thunar_vfs_path_to_uri(info->path, buf, PATH_MAX,
                                                NULL) > 0,
                         NULL);
    
    return g_strdup(buf);
}

gchar *
xfdesktop_thunarx_file_info_get_parent_uri(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    const ThunarVfsInfo *info = xfdesktop_file_icon_peek_info(icon);
    ThunarVfsPath *parent;
    gchar buf[PATH_MAX];
    
    if(!info)
        return NULL;
    
    parent = thunar_vfs_path_get_parent(info->path);
    
    if(G_UNLIKELY(!parent))
        return NULL;
    
    g_return_val_if_fail(thunar_vfs_path_to_uri(parent, buf, PATH_MAX,
                                                NULL) > 0,
                         NULL);
    
    return g_strdup(buf);
}

gchar *
xfdesktop_thunarx_file_info_get_uri_scheme_file(ThunarxFileInfo *file_info)
{
    return g_strdup("file");
}
    
gchar *
xfdesktop_thunarx_file_info_get_mime_type(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    const ThunarVfsInfo *info = xfdesktop_file_icon_peek_info(icon);
    
    if(!info || !info->mime_info)
        return NULL;
    
    return g_strdup(thunar_vfs_mime_info_get_name(info->mime_info));
}

gboolean
xfdesktop_thunarx_file_info_has_mime_type(ThunarxFileInfo *file_info,
                                      const gchar *mime_type)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    const ThunarVfsInfo *info = xfdesktop_file_icon_peek_info(icon);
    ThunarVfsMimeDatabase *mime_db;
    GList *mime_infos, *l;
    ThunarVfsMimeInfo *minfo;
    gboolean has_type = FALSE;
    
    if(!info || !info->mime_info)
        return FALSE;
    
    mime_db = thunar_vfs_mime_database_get_default();
    
    mime_infos = thunar_vfs_mime_database_get_infos_for_info(mime_db,
                                                             info->mime_info);
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

gboolean
xfdesktop_thunarx_file_info_is_directory(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    const ThunarVfsInfo *info = xfdesktop_file_icon_peek_info(icon);
    return (info && info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY);
}

ThunarVfsInfo *
xfdesktop_thunarx_file_info_get_vfs_info(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    const ThunarVfsInfo *info = xfdesktop_file_icon_peek_info(icon);
    return info ? thunar_vfs_info_copy(info) : NULL;
}

#endif  /* HAVE_THUNARX */
