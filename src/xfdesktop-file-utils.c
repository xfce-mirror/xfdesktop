/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006 Brian Tarricone, <bjt23@cornell.edu>
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

#include <gtk/gtk.h>

#include <libxfce4ui/libxfce4ui.h>

#include <exo/exo.h>

#include <thunar-vfs/thunar-vfs.h>

#include <dbus/dbus-glib-lowlevel.h>

#ifdef HAVE_THUNARX
#include <thunarx/thunarx.h>
#endif

#include "xfdesktop-common.h"
#include "xfdesktop-file-icon.h"
#include "xfdesktop-file-manager-proxy.h"
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
                                  NULL, NULL);
    
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
xfdesktop_file_utils_file_icon_list_to_file_list(GList *icon_list)
{
    GList *file_list = NULL, *l;
    XfdesktopFileIcon *icon;
    GFile *file;
    
    for(l = icon_list; l; l = l->next) {
        icon = XFDESKTOP_FILE_ICON(l->data);
        file = xfdesktop_file_icon_peek_file(icon);
        file_list = g_list_prepend(file_list, g_object_ref(file));
    }
    
    return g_list_reverse(file_list);
}

gchar *
xfdesktop_file_utils_file_list_to_string(GList *list)
{
  GString *string;
  GList *lp;
  gchar *uri;

  /* allocate initial string */
  string = g_string_new(NULL);

  for (lp = list; lp != NULL; lp = lp->next)
    {
      uri = g_file_get_uri(lp->data);
      string = g_string_append(string, uri);
      g_free(uri);

      string = g_string_append(string, "\r\n");
    }

  return g_string_free(string, FALSE);
}

void
xfdesktop_file_utils_file_list_free(GList *file_list)
{
  g_list_foreach(file_list, (GFunc) g_object_unref, NULL);
  g_list_free(file_list);
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
    GtkIconTheme *itheme = gtk_icon_theme_get_default();
    GdkPixbuf *pix_theme = NULL, *pix = NULL;
    const gchar *icon_name;
    
    if(custom_icon_name) {
        pix_theme = gtk_icon_theme_load_icon(itheme, custom_icon_name, size,
                                             ITHEME_FLAGS, NULL);
    }
    
    if(!pix_theme && info) {
        icon_name = thunar_vfs_info_get_custom_icon(info);
        if(icon_name) {
            pix_theme = gtk_icon_theme_load_icon(itheme, icon_name, size,
                                                 ITHEME_FLAGS, NULL);
        }
    }

    if(!pix_theme && info && info->mime_info) {
        icon_name = thunar_vfs_mime_info_lookup_icon_name(info->mime_info,
                                                          gtk_icon_theme_get_default());
        DBG("got mime info icon name: %s", icon_name);
        if(icon_name) {
            pix_theme = gtk_icon_theme_load_icon(itheme, icon_name, size,
                                                 ITHEME_FLAGS, NULL);
        }
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
    if(G_UNLIKELY(!pix)) {
        g_warning("Unable to find fallback icon");
        return NULL;
    }
    
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

void
xfdesktop_file_utils_set_window_cursor(GtkWindow *window,
                                       GdkCursorType cursor_type)
{
    GdkCursor *cursor;
    
    if(!window || !GTK_WIDGET(window)->window)
        return;
    
    cursor = gdk_cursor_new(cursor_type);
    if(G_LIKELY(cursor)) {
        gdk_window_set_cursor(GTK_WIDGET(window)->window, cursor);
        gdk_cursor_unref(cursor);
    }
}

gboolean
xfdesktop_file_utils_launch_fallback(const ThunarVfsInfo *info,
                                     GdkScreen *screen,
                                     GtkWindow *parent)
{
    gboolean ret = FALSE;
    gchar *file_manager_app;
    
    g_return_val_if_fail(info, FALSE);
    
    file_manager_app = g_find_program_in_path(FILE_MANAGER_FALLBACK);
    if(file_manager_app) {
        gchar *commandline, *uri, *display_name;
        
        if(!screen && parent)
            screen = gtk_widget_get_screen(GTK_WIDGET(parent));
        else if(!screen)
            screen = gdk_display_get_default_screen(gdk_display_get_default());
        
        display_name = gdk_screen_make_display_name(screen);
        uri = thunar_vfs_path_dup_uri(info->path);
        
        commandline = g_strconcat("\"", file_manager_app, "\" \"",
                                  uri, "\"", NULL);
        
        DBG("executing:\n%s\n", commandline);
        
        ret = xfce_spawn_command_line_on_screen(screen, commandline, FALSE, TRUE, NULL);
        
        g_free(commandline);
        g_free(file_manager_app);
        g_free(uri);
        g_free(display_name);
    }
    
    if(!ret) {
        gchar *primary = g_markup_printf_escaped(_("Unable to launch \"%s\":"),
                                                 info->display_name);
        xfce_message_dialog(GTK_WINDOW(parent),
                            _("Launch Error"), GTK_STOCK_DIALOG_ERROR,
                            primary,
                            _("This feature requires a file manager service present (such as that supplied by Thunar)."),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        g_free(primary);
    }
    
    return ret;
}

static void
xfdesktop_file_utils_display_folder_cb(DBusGProxy *proxy,
                                       GError *error,
                                       gpointer user_data)
{
    GtkWindow *parent = user_data;

    g_return_if_fail(GTK_IS_WINDOW(parent));
    
    xfdesktop_file_utils_set_window_cursor(parent, GDK_LEFT_PTR);
    
    if(error) {
        xfce_message_dialog(parent,
                            _("Launch Error"), GTK_STOCK_DIALOG_ERROR,
                            _("The folder could not be opened"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
    }
}

void
xfdesktop_file_utils_open_folder(GFile *file,
                                 GdkScreen *screen,
                                 GtkWindow *parent)
{
    DBusGProxy *fileman_proxy;
    
    g_return_if_fail(G_IS_FILE(file));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));
    
    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));
    
    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        gchar *uri = g_file_get_uri(file);
        gchar *display_name = gdk_screen_make_display_name(screen);
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        
        if(!xfdesktop_file_manager_proxy_display_folder_async(fileman_proxy,
                                                              uri, display_name, startup_id,
                                                              parent ? xfdesktop_file_utils_display_folder_cb : NULL,
                                                              parent))
        {
            xfce_message_dialog(parent,
                                _("Launch Error"), GTK_STOCK_DIALOG_ERROR,
                                _("The folder could not be opened"),
                                _("This feature requires a file manager service to "
                                  "be present (such as the one supplied by Thunar)."),
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        } else {
            if(parent)
                xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);
        }
        
        g_free(startup_id);
        g_free(uri);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Launch Error"), GTK_STOCK_DIALOG_ERROR,
                            _("The folder could not be opened"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
    }
}

static void
xfdesktop_file_utils_rename_file_cb(DBusGProxy *proxy,
                                    GError *error,
                                    gpointer user_data)
{
    GtkWindow *parent = user_data;

    if(error) {
        xfce_message_dialog(parent,
                            _("Rename Error"), GTK_STOCK_DIALOG_ERROR,
                            _("The file could not be renamed"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
    }
}

void
xfdesktop_file_utils_rename_file(GFile *file,
                                 GdkScreen *screen,
                                 GtkWindow *parent)
{
    DBusGProxy *fileman_proxy;
    
    g_return_if_fail(G_IS_FILE(file));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));
    
    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));
    
    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        gchar *uri = g_file_get_uri(file);
        gchar *display_name = gdk_screen_make_display_name(screen);
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        
        if(!xfdesktop_file_manager_proxy_rename_file_async(fileman_proxy,
                                                           uri, display_name, startup_id,
                                                           xfdesktop_file_utils_rename_file_cb,
                                                           parent))
        {
            xfce_message_dialog(parent,
                                _("Rename Error"), GTK_STOCK_DIALOG_ERROR,
                                _("The file could not be renamed"),
                                _("This feature requires a file manager service to "
                                  "be present (such as the one supplied by Thunar)."),
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        }
        
        g_free(startup_id);
        g_free(uri);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Rename Error"), GTK_STOCK_DIALOG_ERROR,
                            _("The file could not be renamed"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
    }
}

static void
xfdesktop_file_utils_create_file_cb(DBusGProxy *proxy,
                                    GError *error,
                                    gpointer user_data)
{
    GtkWindow *parent = user_data;

    if(error) {
        xfce_message_dialog(parent,
                            _("Create File Error"), GTK_STOCK_DIALOG_ERROR,
                            _("Could not create a new file"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
    }
}

void
xfdesktop_file_utils_create_file(GFile *parent_folder,
                                 const gchar *content_type,
                                 GdkScreen *screen,
                                 GtkWindow *parent)
{
    DBusGProxy *fileman_proxy;
    
    g_return_if_fail(G_IS_FILE(parent_folder));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));
    
    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));
    
    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        gchar *parent_directory = g_file_get_uri(parent_folder);
        gchar *display_name = gdk_screen_make_display_name(screen);
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        
        if(!xfdesktop_file_manager_proxy_create_file_async(fileman_proxy, 
                                                           parent_directory, 
                                                           content_type, display_name,
                                                           startup_id,
                                                           xfdesktop_file_utils_create_file_cb,
                                                           parent))
        {
            xfce_message_dialog(parent,
                                _("Create File Error"), GTK_STOCK_DIALOG_ERROR,
                                _("Could not create a new file"),
                                _("This feature requires a file manager service to "
                                  "be present (such as the one supplied by Thunar)."),
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        }
        
        g_free(startup_id);
        g_free(parent_directory);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Create File Error"), GTK_STOCK_DIALOG_ERROR,
                            _("Could not create a new file"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
    }
}

static void
xfdesktop_file_utils_show_properties_dialog_cb(DBusGProxy *proxy,
                                               GError *error,
                                               gpointer user_data)
{
    GtkWindow *parent = user_data;

    if(error) {
        xfce_message_dialog(parent,
                            _("File Properties Error"), GTK_STOCK_DIALOG_ERROR,
                            _("The file properties dialog could not be opened"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
    }
}

void
xfdesktop_file_utils_show_properties_dialog(GFile *file,
                                            GdkScreen *screen,
                                            GtkWindow *parent)
{
    DBusGProxy *fileman_proxy;
    
    g_return_if_fail(G_IS_FILE(file));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));
    
    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));
    
    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        gchar *uri = g_file_get_uri(file);
        gchar *display_name = gdk_screen_make_display_name(screen);
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        
        if(!xfdesktop_file_manager_proxy_display_file_properties_async(fileman_proxy,
                                                                       uri, display_name, startup_id,
                                                                       xfdesktop_file_utils_show_properties_dialog_cb,
                                                                       parent))
        {
            xfce_message_dialog(parent,
                                _("File Properties Error"), GTK_STOCK_DIALOG_ERROR,
                                _("The file properties dialog could not be opened"),
                                _("This feature requires a file manager service to "
                                  "be present (such as the one supplied by Thunar)."),
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        }
        
        g_free(startup_id);
        g_free(uri);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("File Properties Error"), GTK_STOCK_DIALOG_ERROR,
                            _("The file properties dialog could not be opened"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
    }
}

static gint dbus_ref_cnt = 0;
static DBusGConnection *dbus_gconn = NULL;
static DBusGProxy *dbus_trash_proxy = NULL;
static DBusGProxy *dbus_filemanager_proxy = NULL;

gboolean
xfdesktop_file_utils_dbus_init(void)
{
    gboolean ret = TRUE;
    
    if(dbus_ref_cnt++)
        return TRUE;
    
    if(!dbus_gconn) {
        dbus_gconn = dbus_g_bus_get(DBUS_BUS_SESSION, NULL);
        if(G_LIKELY(dbus_gconn)) {
            /* dbus's default is brain-dead */
            DBusConnection *dconn = dbus_g_connection_get_connection(dbus_gconn);
            dbus_connection_set_exit_on_disconnect(dconn, FALSE);
        }
    }
    
    if(G_LIKELY(dbus_gconn)) {
        dbus_trash_proxy = dbus_g_proxy_new_for_name(dbus_gconn,
                                                     "org.xfce.FileManager",
                                                     "/org/xfce/FileManager",
                                                     "org.xfce.Trash");
        dbus_g_proxy_add_signal(dbus_trash_proxy, "TrashChanged",
                                G_TYPE_BOOLEAN, G_TYPE_INVALID);
        
        dbus_filemanager_proxy = dbus_g_proxy_new_for_name(dbus_gconn,
                                                           "org.xfce.FileManager",
                                                           "/org/xfce/FileManager",
                                                           "org.xfce.FileManager");
    } else {
        ret = FALSE;
        dbus_ref_cnt = 0;
    }
    
    return ret;
}

DBusGProxy *
xfdesktop_file_utils_peek_trash_proxy(void)
{
    return dbus_trash_proxy;
}

DBusGProxy *
xfdesktop_file_utils_peek_filemanager_proxy(void)
{
    return dbus_filemanager_proxy;
}

void
xfdesktop_file_utils_dbus_cleanup(void)
{
    if(dbus_ref_cnt == 0 || --dbus_ref_cnt > 0)
        return;
    
    if(dbus_trash_proxy)
        g_object_unref(G_OBJECT(dbus_trash_proxy));
    if(dbus_filemanager_proxy)
        g_object_unref(G_OBJECT(dbus_filemanager_proxy));
    
    /* we aren't going to unref dbus_gconn because dbus appears to have a
     * memleak in dbus_connection_setup_with_g_main().  really; the comments
     * in dbus-gmain.c admit this. */
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
        
    if(thunar_vfs_path_to_uri(info->path, buf, PATH_MAX, NULL) <= 0)
        return NULL;
    
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
    
    if(thunar_vfs_path_to_uri(parent, buf, PATH_MAX, NULL) <= 0)
        return NULL;
    
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

GFileInfo *
xfdesktop_thunarx_file_info_get_file_info(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    GFileInfo *info = xfdesktop_file_icon_peek_file_info(icon);
    return info ? g_object_ref (info) : NULL;
}

GFileInfo *
xfdesktop_thunarx_file_info_get_filesystem_info(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    GFileInfo *info = xfdesktop_file_icon_peek_filesystem_info(icon);
    return info ? g_object_ref (info) : NULL;
}

GFile *
xfdesktop_thunarx_file_info_get_location(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    GFile *file = xfdesktop_file_icon_peek_file(icon);
    return g_object_ref (file);
}

#endif  /* HAVE_THUNARX */
