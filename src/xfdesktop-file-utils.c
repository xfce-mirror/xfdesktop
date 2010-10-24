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

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_PWD_H
#include <pwd.h>
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

static
gboolean xfdesktop_file_utils_is_desktop_file(GFileInfo *info)
{
    const gchar *content_type;
    gboolean is_desktop_file = FALSE;

    content_type = g_file_info_get_content_type(info);
    if(content_type)
        is_desktop_file = g_content_type_equals(content_type, "application/x-desktop");

    return is_desktop_file 
        && !g_str_has_suffix(g_file_info_get_name(info), ".directory");
}

gboolean
xfdesktop_file_utils_file_is_executable(GFileInfo *info)
{
    const gchar *content_type;
    gboolean can_execute = FALSE;

    g_return_val_if_fail(G_IS_FILE_INFO(info), FALSE);

    if(g_file_info_get_attribute_boolean(info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE)) {
        /* get the content type of the file */
        content_type = g_file_info_get_content_type(info);
        if(content_type != NULL) {
#ifdef G_OS_WIN32
            /* check for .exe, .bar or .com */
            can_execute = g_content_type_can_be_executable(content_type);
#else
            /* check if the content type is save to execute, we don't use
             * g_content_type_can_be_executable() for unix because it also returns
             * true for "text/plain" and we don't want that */
            if(g_content_type_is_a(content_type, "application/x-executable")
               || g_content_type_is_a(content_type, "application/x-shellscript"))
            {
                can_execute = TRUE;
            }
#endif
        }
    }

    return can_execute || xfdesktop_file_utils_is_desktop_file(info);
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

static gchar *
xfdesktop_file_utils_change_working_directory (const gchar *new_directory)
{
  gchar *old_directory;

  g_return_val_if_fail(new_directory && *new_directory != '\0', NULL);

  /* allocate a path buffer for the old working directory */
  old_directory = g_malloc0(sizeof(gchar) * MAXPATHLEN);

  /* try to determine the current working directory */
#ifdef G_PLATFORM_WIN32
  if(!_getcwd(old_directory, MAXPATHLEN))
#else
  if(!getcwd (old_directory, MAXPATHLEN))
#endif
  {
      /* working directory couldn't be determined, reset the buffer */
      g_free(old_directory);
      old_directory = NULL;
  }

  /* try switching to the new working directory */
#ifdef G_PLATFORM_WIN32
  if(_chdir (new_directory))
#else
  if(chdir (new_directory))
#endif
  {
      /* switching failed, we don't need to return the old directory */
      g_free(old_directory);
      old_directory = NULL;
  }

  return old_directory;
}

gboolean
xfdesktop_file_utils_app_info_launch(GAppInfo *app_info,
                                     GFile *working_directory,
                                     GList *files,
                                     GAppLaunchContext *context,
                                     GError **error)
{
    gboolean result = FALSE;
    gchar *new_path = NULL;
    gchar *old_path = NULL;

    g_return_val_if_fail(G_IS_APP_INFO(app_info), FALSE);
    g_return_val_if_fail(working_directory == NULL || G_IS_FILE(working_directory), FALSE);
    g_return_val_if_fail(files != NULL && files->data != NULL, FALSE);
    g_return_val_if_fail(G_IS_APP_LAUNCH_CONTEXT(context), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    /* check if we want to set the working directory of the spawned app */
    if(working_directory) {
        /* determine the working directory path */
        new_path = g_file_get_path(working_directory);
        if(new_path) {
            /* switch to the desired working directory, remember that 
             * of xfdesktop itself */
            old_path = xfdesktop_file_utils_change_working_directory(new_path);

            /* forget about the new working directory path */
            g_free(new_path);
        }
    }

    /* launch the paths with the specified app info */
    result = g_app_info_launch(app_info, files, context, error);

    /* check if we need to reset the working directory to the one xfdesktop was
     * opened from */
    if(old_path) {
        /* switch to xfdesktop's original working directory */
        new_path = xfdesktop_file_utils_change_working_directory(old_path);

        /* clean up */
        g_free (new_path);
        g_free (old_path);
    }

    return result;
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
        GError *error = NULL;
        gchar *uri = g_file_get_uri(file);
        gchar *display_name = gdk_screen_make_display_name(screen);
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        
        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);

        if(!xfdesktop_file_manager_proxy_display_folder(fileman_proxy,
                                                        uri, display_name, startup_id,
                                                        &error))
        {
            xfce_message_dialog(parent,
                                _("Launch Error"), GTK_STOCK_DIALOG_ERROR,
                                _("The folder could not be opened"),
                                error->message, GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, 
                                NULL);

            g_error_free(error);
        }
        
        xfdesktop_file_utils_set_window_cursor(parent, GDK_LEFT_PTR);
        
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
        GError *error = NULL;
        gchar *uri = g_file_get_uri(file);
        gchar *display_name = gdk_screen_make_display_name(screen);
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);
        
        if(!xfdesktop_file_manager_proxy_rename_file(fileman_proxy,
                                                     uri, display_name, startup_id,
                                                     &error))
        {
            xfce_message_dialog(parent,
                                _("Rename Error"), GTK_STOCK_DIALOG_ERROR,
                                _("The file could not be renamed"),
                                error->message, GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, 
                                NULL);

            g_error_free(error);
        }
              
        xfdesktop_file_utils_set_window_cursor(parent, GDK_LEFT_PTR);
        
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

void
xfdesktop_file_utils_unlink_files(GList *files,
                                  GdkScreen *screen,
                                  GtkWindow *parent)
{
    DBusGProxy *fileman_proxy;
    
    g_return_if_fail(files != NULL && G_IS_FILE(files->data));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));
    
    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));
    
    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        GError *error = NULL;
        guint nfiles = g_list_length(files);
        gchar **uris = g_new0(gchar *, nfiles+1);
        gchar *display_name = gdk_screen_make_display_name(screen);
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        GList *lp;
        gint n;

        /* convert GFile list into an array of URIs */
        for(n = 0, lp = files; lp != NULL; ++n, lp = lp->next)
            uris[n] = g_file_get_uri(lp->data);
        uris[n] = NULL;

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);
        
        if(!xfdesktop_file_manager_proxy_unlink_files(fileman_proxy,
                                                      NULL, (const gchar **)uris, 
                                                      display_name, startup_id,
                                                      &error))
        {
            xfce_message_dialog(parent,
                                _("Delete Error"), GTK_STOCK_DIALOG_ERROR,
                                _("The selected files could not be deleted"),
                                error->message, GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, 
                                NULL);

            g_error_free(error);
        }

        xfdesktop_file_utils_set_window_cursor(parent, GDK_LEFT_PTR);
        
        g_free(startup_id);
        g_strfreev(uris);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Delete Error"), GTK_STOCK_DIALOG_ERROR,
                            _("The selected files could not be deleted"),
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
        GError *error = NULL;
        gchar *parent_directory = g_file_get_uri(parent_folder);
        gchar *display_name = gdk_screen_make_display_name(screen);
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        
        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);

        if(!xfdesktop_file_manager_proxy_create_file(fileman_proxy, 
                                                     parent_directory, 
                                                     content_type, display_name,
                                                     startup_id,
                                                     &error))
        {
            xfce_message_dialog(parent,
                                _("Create File Error"), GTK_STOCK_DIALOG_ERROR,
                                _("Could not create a new file"),
                                error->message, GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, 
                                NULL);

            g_error_free(error);
        }
        
        xfdesktop_file_utils_set_window_cursor(parent, GDK_LEFT_PTR);
        
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

void
xfdesktop_file_utils_create_file_from_template(GFile *parent_folder,
                                               GFile *template_file,
                                               GdkScreen *screen,
                                               GtkWindow *parent)
{
    DBusGProxy *fileman_proxy;
    
    g_return_if_fail(G_IS_FILE(parent_folder));
    g_return_if_fail(G_IS_FILE(template_file));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));
    
    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));
    
    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        GError *error = NULL;
        gchar *parent_directory = g_file_get_uri(parent_folder);
        gchar *template_uri = g_file_get_uri(template_file);
        gchar *display_name = gdk_screen_make_display_name(screen);
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        
        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);

        if(!xfdesktop_file_manager_proxy_create_file_from_template(fileman_proxy, 
                                                                   parent_directory, 
                                                                   template_uri,
                                                                   display_name,
                                                                   startup_id,
                                                                   &error))
        {
            xfce_message_dialog(parent,
                                _("Create Document Error"), GTK_STOCK_DIALOG_ERROR,
                                _("Could not create a new document from the template"),
                                error->message, GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, 
                                NULL);

            g_error_free(error);
        }
        
        xfdesktop_file_utils_set_window_cursor(parent, GDK_LEFT_PTR);
        
        g_free(startup_id);
        g_free(parent_directory);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Create Document Error"), GTK_STOCK_DIALOG_ERROR,
                            _("Could not create a new document from the template"),
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
        GError *error = NULL;
        gchar *uri = g_file_get_uri(file);
        gchar *display_name = gdk_screen_make_display_name(screen);
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        
        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);

        if(!xfdesktop_file_manager_proxy_display_file_properties(fileman_proxy,
                                                                 uri, display_name, startup_id,
                                                                 &error))
        {
            xfce_message_dialog(parent,
                                _("File Properties Error"), GTK_STOCK_DIALOG_ERROR,
                                _("The file properties dialog could not be opened"),
                                error->message, GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, 
                                NULL);

            g_error_free(error);
        }
        
        xfdesktop_file_utils_set_window_cursor(parent, GDK_LEFT_PTR);
        
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

void
xfdesktop_file_utils_launch(GFile *file,
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
        GError *error = NULL;
        gchar *uri = g_file_get_uri(file);
        gchar *display_name = gdk_screen_make_display_name(screen);
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);
        
        if(!xfdesktop_file_manager_proxy_launch(fileman_proxy,
                                                uri, display_name, startup_id,
                                                &error))
        {
            xfce_message_dialog(parent,
                                _("Launch Error"), GTK_STOCK_DIALOG_ERROR,
                                _("The file could not be opened"),
                                error->message, GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                                NULL);

            g_error_free(error);
        }

        xfdesktop_file_utils_set_window_cursor(parent, GDK_LEFT_PTR);
        
        g_free(startup_id);
        g_free(uri);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Launch Error"), GTK_STOCK_DIALOG_ERROR,
                            _("The file could not be opened"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
    }
}

void
xfdesktop_file_utils_execute(GFile *working_directory,
                             GFile *file,
                             GList *files,
                             GdkScreen *screen)
{
    DBusGProxy *fileman_proxy;
    
    g_return_if_fail(working_directory == NULL || G_IS_FILE(working_directory));
    g_return_if_fail(G_IS_FILE(file));
    g_return_if_fail(screen == NULL || GDK_IS_SCREEN(screen));
    
    if(!screen)
        screen = gdk_display_get_default_screen(gdk_display_get_default());
    
    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        GError *error = NULL;
        gchar *working_dir = working_directory != NULL ? g_file_get_uri(working_directory) : NULL;
        gchar *uri = g_file_get_uri(file);
        gchar *display_name = gdk_screen_make_display_name(screen);
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        GList *lp;
        guint n = g_list_length (files);
        gchar **uris = g_new0 (gchar *, n + 1);

        for (n = 0, lp = files; lp != NULL; ++n, lp = lp->next)
            uris[n] = g_file_get_uri(lp->data);
        uris[n] = NULL;

        if(!xfdesktop_file_manager_proxy_execute(fileman_proxy,
                                                 working_dir, uri, 
                                                 (const gchar **)uris,
                                                 display_name, startup_id,
                                                 &error))
        {
            gchar *filename = g_file_get_uri(file);
            gchar *name = g_filename_display_basename(filename);
            gchar *primary = g_markup_printf_escaped(_("Failed to run \"%s\""), name);

            xfce_message_dialog(NULL,
                                _("Run Error"), GTK_STOCK_DIALOG_ERROR,
                                primary, error->message, 
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                                NULL);

            g_free(primary);
            g_free(name);
            g_free(filename);

            g_error_free(error);
        }
        
        g_free(startup_id);
        g_free(display_name);
        g_strfreev(uris);
        g_free(uri);
        g_free(working_dir);
    } else {
        gchar *filename = g_file_get_uri(file);
        gchar *name = g_filename_display_basename(filename);
        gchar *primary = g_markup_printf_escaped(_("Failed to run \"%s\""), name);

        xfce_message_dialog(NULL,
                            _("Launch Error"), GTK_STOCK_DIALOG_ERROR,
                            primary,
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);

        g_free(primary);
        g_free(name);
        g_free(filename);
    }
}

void
xfdesktop_file_utils_display_chooser_dialog(GFile *file,
                                            gboolean open,
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
        GError *error = NULL;
        gchar *uri = g_file_get_uri(file);
        gchar *display_name = gdk_screen_make_display_name(screen);
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);
        
        if(!xfdesktop_file_manager_proxy_display_chooser_dialog(fileman_proxy,
                                                                uri, open,
                                                                display_name, 
                                                                startup_id,
                                                                &error))
        {
            xfce_message_dialog(parent,
                                _("Launch Error"), GTK_STOCK_DIALOG_ERROR,
                                _("The application chooser could not be opened"),
                                error->message, GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, 
                                NULL);

            g_error_free(error);
        }
        
        xfdesktop_file_utils_set_window_cursor(parent, GDK_LEFT_PTR);
        
        g_free(startup_id);
        g_free(uri);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Launch Error"), GTK_STOCK_DIALOG_ERROR,
                            _("The application chooser could not be opened"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
    }
}

void
xfdesktop_file_utils_transfer_file(GdkDragAction action,
                                   GFile *source_file,
                                   GFile *target_file,
                                   GdkScreen *screen)
{
    DBusGProxy *fileman_proxy;
    
    g_return_if_fail(G_IS_FILE(source_file));
    g_return_if_fail(G_IS_FILE(target_file));
    g_return_if_fail(screen == NULL || GDK_IS_SCREEN(screen));
    
    if(!screen)
        screen = gdk_display_get_default_screen(gdk_display_get_default());
    
    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        GError *error = NULL;
        gchar *source_uris[2] = { g_file_get_uri(source_file), NULL };
        gchar *target_uris[2] = { g_file_get_uri(target_file), NULL };
        gchar *display_name = gdk_screen_make_display_name(screen);
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());

        switch(action) {
            case GDK_ACTION_MOVE:
                xfdesktop_file_manager_proxy_move_into(fileman_proxy, NULL,
                                                       (const gchar **)source_uris, 
                                                       (const gchar *)target_uris[0],
                                                       display_name, startup_id,
                                                       &error);
                break;
            case GDK_ACTION_COPY:
                xfdesktop_file_manager_proxy_copy_to(fileman_proxy, NULL,
                                                     (const gchar **)source_uris, 
                                                     (const gchar **)target_uris,
                                                     display_name, startup_id,
                                                     &error);
                break;
            case GDK_ACTION_LINK:
                xfdesktop_file_manager_proxy_link_into(fileman_proxy, NULL,
                                                       (const gchar **)source_uris, 
                                                       (const gchar *)target_uris[0],
                                                       display_name, startup_id,
                                                       &error);
                break;
            default:
                g_warning("Unsupported transfer action");
        }

        if(error) {
            xfce_message_dialog(NULL,
                                _("Transfer Error"), GTK_STOCK_DIALOG_ERROR,
                                _("The file transfer could not be performed"),
                                error->message, GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, 
                                NULL);

            g_error_free(error);
        }
        
        g_free(startup_id);
        g_free(display_name);
        g_free(target_uris[0]);
        g_free(source_uris[0]);
    } else {
        xfce_message_dialog(NULL,
                            _("Transfer Error"), GTK_STOCK_DIALOG_ERROR,
                            _("The file transfer could not be performed"),
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
