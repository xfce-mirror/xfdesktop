/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006,2024 Brian Tarricone, <brian@tarricone.org>
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

#include "libxfce4windowing/libxfce4windowing.h"
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
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixmounts.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <libxfce4ui/libxfce4ui.h>

#include <exo/exo.h>

#ifdef HAVE_THUNARX
#include <thunarx/thunarx.h>
#endif

#include "xfdesktop-common.h"
#include "xfdesktop-file-icon.h"
#include "xfdesktop-file-manager-fdo-proxy.h"
#include "xfdesktop-file-manager-proxy.h"
#include "xfdesktop-file-utils.h"
#include "xfdesktop-trash-proxy.h"
#include "xfdesktop-thunar-proxy.h"

typedef struct {
    GtkWindow *parent;
    GFile *file;
} ExecuteData;

typedef struct {
    GFile *dest_file;
    GtkWindow *parent;
} TemplateCreateData;

typedef struct {
    GCancellable *cancellable;

    GString *output_string;
    gchar buffer[256];

    CreateDesktopFileCallback callback;
    gpointer callback_data;
} CreateDesktopFileData;

static XfdesktopTrash       *xfdesktop_file_utils_peek_trash_proxy(void);
static XfdesktopFileManager *xfdesktop_file_utils_peek_filemanager_proxy(void);
static XfdesktopFileManager1 *xfdesktop_file_utils_peek_filemanager_fdo_proxy(void);

static void xfdesktop_file_utils_trash_proxy_new_cb (GObject *source_object,
                                                     GAsyncResult *res,
                                                     gpointer user_data);

static void xfdesktop_file_utils_file_manager_proxy_new_cb (GObject *source_object,
                                                            GAsyncResult *res,
                                                            gpointer user_data);

static void xfdesktop_file_utils_file_manager_fdo_proxy_new_cb(GObject *source_object,
                                                               GAsyncResult *res,
                                                               gpointer user_data);

#ifdef HAVE_THUNARX
static void xfdesktop_file_utils_thunar_proxy_new_cb (GObject *source_object,
                                                      GAsyncResult *res,
                                                      gpointer user_data);

static XfdesktopThunar *xfdesktop_file_utils_peek_thunar_proxy(void);
#else
static gpointer xfdesktop_file_utils_peek_thunar_proxy(void);
#endif

gboolean
xfdesktop_file_utils_is_desktop_file(GFileInfo *info)
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

gchar *
xfdesktop_file_utils_format_time_for_display(guint64 file_time)
{
    const gchar *date_format;
    struct tm *tfile;
    time_t ftime;
    GDate dfile;
    GDate dnow;
    gchar buffer[128];
    gint diff;

    /* check if the file_time is valid */
    if(file_time != 0) {
        ftime = (time_t) file_time;

        /* determine the local file time */
        tfile = localtime(&ftime);

        /* setup the dates for the time values */
        g_date_set_time_t(&dfile, (time_t) ftime);
        g_date_set_time_t(&dnow, time(NULL));

        /* determine the difference in days */
        diff = g_date_get_julian(&dnow) - g_date_get_julian(&dfile);
        if(diff == 0) {
            /* TRANSLATORS: file was modified less than one day ago */
            strftime(buffer, 128, _("Today at %X"), tfile);
            return g_strdup(buffer);
        } else if(diff == 1) {
            /* TRANSLATORS: file was modified less than two days ago */
            strftime(buffer, 128, _("Yesterday at %X"), tfile);
            return g_strdup(buffer);
        } else {
            if (diff > 1 && diff < 7) {
                /* Days from last week */
                date_format = _("%A at %X");
            } else {
                /* Any other date */
                date_format = _("%x at %X");
            }

            /* format the date string accordingly */
            strftime(buffer, 128, date_format, tfile);
            return g_strdup(buffer);
        }
    }

    /* the file_time is invalid */
    return g_strdup(_("Unknown"));
}

GKeyFile *
xfdesktop_file_utils_query_key_file(GFile *file,
                                    GCancellable *cancellable,
                                    GError **error)
{
    GKeyFile *key_file;
    gchar *contents = NULL;
    gsize length;

    g_return_val_if_fail(G_IS_FILE(file), NULL);
    g_return_val_if_fail(cancellable == NULL || G_IS_CANCELLABLE(cancellable), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    /* try to load the entire file into memory */
    if (!g_file_load_contents(file, cancellable, &contents, &length, NULL, error))
        return NULL;

    /* allocate a new key file */
    key_file = g_key_file_new();

    /* try to parse the key file from the contents of the file */
    if (length == 0
        || g_key_file_load_from_data(key_file, contents, length,
                                     G_KEY_FILE_KEEP_COMMENTS
                                     | G_KEY_FILE_KEEP_TRANSLATIONS,
                                     error))
    {
        g_free(contents);
        return key_file;
    }
    else
    {
        g_free(contents);
        g_key_file_free(key_file);
        return NULL;
    }
}

gchar *
xfdesktop_file_utils_get_display_name(GFile *file,
                                      GFileInfo *info)
{
    GKeyFile *key_file;
    gchar *display_name = NULL;

    g_return_val_if_fail(G_IS_FILE_INFO(info), NULL);

    /* check if we have a desktop entry */
    if(xfdesktop_file_utils_is_desktop_file(info)) {
        /* try to load its data into a GKeyFile */
        key_file = xfdesktop_file_utils_query_key_file(file, NULL, NULL);
        if(key_file) {
            /* try to parse the display name */
            display_name = g_key_file_get_locale_string(key_file,
                                                        G_KEY_FILE_DESKTOP_GROUP,
                                                        G_KEY_FILE_DESKTOP_KEY_NAME,
                                                        NULL,
                                                        NULL);

            /* free the key file */
            g_key_file_free (key_file);
        }
    }

    /* use the default display name as a fallback */
    if(!display_name
       || *display_name == '\0'
       || !g_utf8_validate(display_name, -1, NULL))
    {
        display_name = g_strdup(g_file_info_get_display_name(info));
    }

    return display_name;
}

/**
 * xfdesktop_file_utils_next_new_file_name:
 * @file: the filename which will be used as the basis/default
 *
 * Returns a filename that is like @filename with the possible addition of
 * a number to differentiate it from other similarly named files. In other words
 * it searches @folder for incrementally named files starting from @file_name
 * and returns the first available increment.
 *
 * e.g. in a folder with the following files:
 * - file
 * - empty
 * - file_copy
 *
 * Calling this functions with the above folder and @filename equal to 'file' the returned
 * filename will be 'file (copy 1)'.
 *
 * The caller is responsible to free the returned string using g_free() when no longer needed.
 *
 * Code extracted and adapted from on thunar_util_next_new_file_name.
 *
 * Return value: pointer to the new filename.
 **/
GFile *
xfdesktop_file_utils_next_new_file_name(GFile *file) {
  GFile *folder = g_file_get_parent(file);
  gchar *filename = g_file_get_basename(file);
  unsigned long   file_name_size  = strlen(filename);
  unsigned        count           = 0;
  gchar          *extension       = NULL;
  gchar          *new_name        = g_strdup(filename);

  extension = strrchr(filename, '.');
  if (!extension || extension == filename)
    extension = "";
  else
    file_name_size -= strlen(extension);

  /* loop until new_name is unique */
  while(TRUE)
    {
      GFile *new_file = g_file_get_child(folder, new_name);
      if (!g_file_query_exists(new_file, NULL)) {
          g_free(new_name);
          g_free(filename);
          g_object_unref(folder);
          return new_file;
      }
      g_object_unref(new_file);

      g_free(new_name);
      new_name = g_strdup_printf(_("%.*s %u%s"), (int) file_name_size, filename, ++count, extension);
    }
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
        if(file)
            file_list = g_list_prepend(file_list, g_object_ref(file));
    }

    return g_list_reverse(file_list);
}

GList *
xfdesktop_file_utils_file_list_from_string(const gchar *string)
{
    GList *list = NULL;
    gchar **uris;
    gsize n;

    uris = g_uri_list_extract_uris(string);

    for (n = 0; uris != NULL && uris[n] != NULL; ++n)
      list = g_list_append(list, g_file_new_for_uri(uris[n]));

    g_strfreev (uris);

    return list;
}

gchar *
xfdesktop_file_utils_file_list_to_string(GList *list)
{
    GString *string;
    GList *lp;
    gchar *uri;

    /* allocate initial string */
    string = g_string_new(NULL);

    for (lp = list; lp != NULL; lp = lp->next) {
        uri = g_file_get_uri(lp->data);
        string = g_string_append(string, uri);
        g_free(uri);

        string = g_string_append(string, "\r\n");
      }

    return g_string_free(string, FALSE);
}

gchar **
xfdesktop_file_utils_file_list_to_uri_array(GList *file_list)
{
    GList *lp;
    gchar **uris = NULL;
    guint list_length, n;

    list_length = g_list_length(file_list);

    uris = g_new0(gchar *, list_length + 1);
    for (n = 0, lp = file_list; lp != NULL; ++n, lp = lp->next)
        uris[n] = g_file_get_uri(lp->data);
    uris[n] = NULL;

    return uris;
}

void
xfdesktop_file_utils_file_list_free(GList *file_list)
{
    g_list_free_full(file_list, g_object_unref);
}

static GdkPixbuf *xfdesktop_fallback_icon = NULL;
static gint xfdesktop_fallback_icon_size = -1;
static gint xfdesktop_fallback_icon_scale = -1;

GdkPixbuf *
xfdesktop_file_utils_get_fallback_icon(gint size,
                                       gint scale)
{
    g_return_val_if_fail(size > 0, NULL);

    if((size != xfdesktop_fallback_icon_size || scale != xfdesktop_fallback_icon_scale) && xfdesktop_fallback_icon) {
        g_object_unref(G_OBJECT(xfdesktop_fallback_icon));
        xfdesktop_fallback_icon = NULL;
    }

    if(!xfdesktop_fallback_icon) {
        xfdesktop_fallback_icon = gdk_pixbuf_new_from_file_at_size(DATADIR "/pixmaps/xfdesktop/xfdesktop-fallback-icon.png",
                                                                   size * scale,
                                                                   size * scale,
                                                                   NULL);
    }

    if(G_UNLIKELY(!xfdesktop_fallback_icon)) {
        /* this is kinda crappy, but hopefully should never happen */
        xfdesktop_fallback_icon = gtk_icon_theme_load_icon_for_scale(gtk_icon_theme_get_default(),
                                                                     "image-missing",
                                                                     size,
                                                                     scale,
                                                                     GTK_ICON_LOOKUP_USE_BUILTIN,
                                                                     NULL);
        if(gdk_pixbuf_get_width(xfdesktop_fallback_icon) != size * scale
           || gdk_pixbuf_get_height(xfdesktop_fallback_icon) != size * scale)
        {
            GdkPixbuf *tmp = gdk_pixbuf_scale_simple(xfdesktop_fallback_icon,
                                                     size * scale, size,
                                                     GDK_INTERP_BILINEAR * scale);
            g_object_unref(G_OBJECT(xfdesktop_fallback_icon));
            xfdesktop_fallback_icon = tmp;
        }
    }

    xfdesktop_fallback_icon_size = size;
    xfdesktop_fallback_icon_scale = scale;

    return GDK_PIXBUF(g_object_ref(G_OBJECT(xfdesktop_fallback_icon)));
}

void
xfdesktop_file_utils_set_window_cursor(GtkWindow *window,
                                       GdkCursorType cursor_type)
{
    GdkCursor *cursor;

    if(!window || !gtk_widget_get_window(GTK_WIDGET(window)))
        return;

    cursor = gdk_cursor_new_for_display(gtk_widget_get_display(GTK_WIDGET(window)), cursor_type);
    if(G_LIKELY(cursor)) {
        gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(window)), cursor);
        g_object_unref(cursor);
    }
}

static void
xfdesktop_file_utils_unset_window_cursor(GtkWindow *window)
{
    if (window != NULL) {
        GdkWindow *gdk_window = gtk_widget_get_window(GTK_WIDGET(window));
        if (gdk_window != NULL) {
            gdk_window_set_cursor(gdk_window, NULL);
        }
    }
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

static void
report_open_folders_error(GtkWindow *parent,
                          GError *error)
{
    xfce_message_dialog(parent,
                        _("Launch Error"), "dialog-error",
                        _("The folder could not be opened"),
                        error->message,
                        XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                        NULL);
}

static gboolean
xfdesktop_file_utils_open_folders_fallback(const gchar *const *uris,
                                           GdkScreen *screen,
                                           GtkWindow *parent,
                                           gboolean report_errors)
{
    gboolean succeeded = TRUE;

    for (gint i = 0; uris[i] != NULL; ++i) {
        GError *error = NULL;

        if (!exo_execute_preferred_application_on_screen("FileManager",
                                                         uris[i],
                                                         NULL,
                                                         NULL,
                                                         screen,
                                                         &error))
        {
            if (report_errors) {
                report_open_folders_error(parent, error);
            }

            g_clear_error(&error);
            succeeded = FALSE;
        }
    }

    return succeeded;
}

typedef struct {
    gchar **uris;
    GdkScreen *screen;
    GtkWindow *parent;
} ShowFoldersFallbackData;

static void
show_folders_finished(GObject *source_object,
                      GAsyncResult *res,
                      gpointer user_data)
{
    ShowFoldersFallbackData *data = user_data;
    GError *error = NULL;

    if (!xfdesktop_file_manager1_call_show_folders_finish(XFDESKTOP_FILE_MANAGER1(source_object), res, &error)) {
        if (!xfdesktop_file_utils_open_folders_fallback((const gchar *const *)data->uris,
                                                        data->screen,
                                                        data->parent,
                                                        FALSE))
        {
            report_open_folders_error(data->parent, error);
        }

        g_clear_error(&error);
    }

    g_strfreev(data->uris);
    if (data->parent != NULL) {
        g_object_unref(data->parent);
    }
    g_slice_free(ShowFoldersFallbackData, data);
}

void
xfdesktop_file_utils_open_folders(GList *files,
                                  GdkScreen *screen,
                                  GtkWindow *parent)
{
    XfdesktopFileManager1 *fileman_fdo_proxy;
    gchar **uris;

    g_return_if_fail(files != NULL);
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    uris = xfdesktop_file_utils_file_list_to_uri_array(files);
    g_return_if_fail(uris != NULL && uris[0] != NULL);

    fileman_fdo_proxy = xfdesktop_file_utils_peek_filemanager_fdo_proxy();
    if (fileman_fdo_proxy != NULL) {
        ShowFoldersFallbackData *data = g_slice_new0(ShowFoldersFallbackData);
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());

        data->uris = uris;
        data->screen = screen;
        data->parent = parent != NULL ? g_object_ref(parent) : NULL;

        xfdesktop_file_manager1_call_show_folders(fileman_fdo_proxy,
                                                  (const gchar *const *)uris,
                                                  startup_id,
                                                  NULL,
                                                  show_folders_finished,
                                                  data);

        g_free(startup_id);
    } else {
        xfdesktop_file_utils_open_folders_fallback((const gchar *const *)uris,screen, parent, TRUE);
        g_strfreev(uris);
    }
}

static void
xfdesktop_file_utils_async_handle_error(GError *error, gpointer userdata)
{
    GtkWindow *parent = GTK_WINDOW(userdata);

    if(error != NULL) {
        if(error->domain != G_IO_ERROR || error->code != G_IO_ERROR_TIMED_OUT) {
            xfce_message_dialog(parent,
                                _("Error"), "dialog-error",
                                _("The requested operation could not be completed"),
                                error->message,
                                XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                                NULL);
        }

        g_clear_error(&error);
    }
}

static void
rename_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    if (!xfdesktop_file_manager_call_rename_file_finish(XFDESKTOP_FILE_MANAGER(source_object), res, &error))
        xfdesktop_file_utils_async_handle_error(error, user_data);
}

void
xfdesktop_file_utils_rename_file(GFile *file,
                                 GdkScreen *screen,
                                 GtkWindow *parent)
{
    XfdesktopFileManager *fileman_proxy;

    g_return_if_fail(G_IS_FILE(file));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        gchar *uri = g_file_get_uri(file);
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);


        xfdesktop_file_manager_call_rename_file(fileman_proxy,
                                                uri, display_name, startup_id,
                                                NULL,
                                                rename_cb,
                                                parent);

        xfdesktop_file_utils_unset_window_cursor(parent);

        g_free(startup_id);
        g_free(uri);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Rename Error"), "dialog-error",
                            _("The file could not be renamed"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }
}

static void
bulk_rename_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    if (!xfdesktop_thunar_call_bulk_rename_finish(XFDESKTOP_THUNAR(source_object), res, &error))
        xfdesktop_file_utils_async_handle_error(error, user_data);
}

void
xfdesktop_file_utils_bulk_rename(GFile *working_directory,
                                 GList *files,
                                 GdkScreen *screen,
                                 GtkWindow *parent)
{
    XfdesktopThunar *thunar_proxy;

    g_return_if_fail(G_IS_FILE(working_directory));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    thunar_proxy = xfdesktop_file_utils_peek_thunar_proxy();
    if(thunar_proxy) {
        gchar *directory = g_file_get_path(working_directory);
        guint nfiles = g_list_length(files);
        gchar **filenames = g_new0(gchar *, nfiles+1);
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        GList *lp;
        gint n;

        /* convert GFile list into an array of filenames */
        for(n = 0, lp = files; lp != NULL; ++n, lp = lp->next)
            filenames[n] = g_file_get_basename(lp->data);
        filenames[n] = NULL;

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);


        xfdesktop_thunar_call_bulk_rename(thunar_proxy,
                                          directory, (const gchar **)filenames,
                                          FALSE, display_name, startup_id,
                                          NULL,
                                          bulk_rename_cb,
                                          parent);

        xfdesktop_file_utils_unset_window_cursor(parent);

        g_free(directory);
        g_free(startup_id);
        g_strfreev(filenames);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Rename Error"), "dialog-error",
                            _("The files could not be renamed"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }
}

static void
unlink_files_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    if (!xfdesktop_file_manager_call_unlink_files_finish(XFDESKTOP_FILE_MANAGER(source_object), res, &error))
        xfdesktop_file_utils_async_handle_error(error, user_data);
}

void
xfdesktop_file_utils_unlink_files(GList *files,
                                  GdkScreen *screen,
                                  GtkWindow *parent)
{
    XfdesktopFileManager *fileman_proxy;

    g_return_if_fail(files != NULL && G_IS_FILE(files->data));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        guint nfiles = g_list_length(files);
        gchar **uris = g_new0(gchar *, nfiles+1);
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        GList *lp;
        gint n;

        /* convert GFile list into an array of URIs */
        for(n = 0, lp = files; lp != NULL; ++n, lp = lp->next)
            uris[n] = g_file_get_uri(lp->data);
        uris[n] = NULL;

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);


        xfdesktop_file_manager_call_unlink_files(fileman_proxy,
                                                 "", (const gchar **)uris,
                                                 display_name, startup_id,
                                                 NULL,
                                                 unlink_files_cb,
                                                 parent);

        xfdesktop_file_utils_unset_window_cursor(parent);

        g_free(startup_id);
        g_strfreev(uris);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Delete Error"), "dialog-error",
                            _("The selected files could not be deleted"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }
}

static void
trash_files_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    if (!xfdesktop_trash_call_move_to_trash_finish(XFDESKTOP_TRASH(source_object), res, &error))
        xfdesktop_file_utils_async_handle_error(error, user_data);
}

void
xfdesktop_file_utils_trash_files(GList *files,
                                 GdkScreen *screen,
                                 GtkWindow *parent)
{
    XfdesktopTrash *trash_proxy;

    g_return_if_fail(files != NULL && G_IS_FILE(files->data));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    trash_proxy = xfdesktop_file_utils_peek_trash_proxy();
    if(trash_proxy) {
        guint nfiles = g_list_length(files);
        gchar **uris = g_new0(gchar *, nfiles+1);
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        GList *lp;
        gint n;

        /* convert GFile list into an array of URIs */
        for(n = 0, lp = files; lp != NULL; ++n, lp = lp->next)
            uris[n] = g_file_get_uri(lp->data);
        uris[n] = NULL;

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);


        xfdesktop_trash_call_move_to_trash(trash_proxy,
                                           (const gchar **)uris,
                                           display_name, startup_id,
                                           NULL,
                                           trash_files_cb,
                                           parent);

        xfdesktop_file_utils_unset_window_cursor(parent);

        g_free(startup_id);
        g_strfreev(uris);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Trash Error"), "dialog-error",
                            _("The selected files could not be moved to the trash"),
                            _("This feature requires a trash service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }
}

static void
empty_trash_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    if (!xfdesktop_trash_call_empty_trash_finish(XFDESKTOP_TRASH(source_object), res, &error))
        xfdesktop_file_utils_async_handle_error(error, user_data);
}

void
xfdesktop_file_utils_empty_trash(GdkScreen *screen,
                                 GtkWindow *parent)
{
    XfdesktopTrash *trash_proxy;

    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    trash_proxy = xfdesktop_file_utils_peek_trash_proxy();
    if(trash_proxy) {
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);


        xfdesktop_trash_call_empty_trash(trash_proxy,
                                         display_name, startup_id,
                                         NULL,
                                         empty_trash_cb,
                                         parent);

        xfdesktop_file_utils_unset_window_cursor(parent);

        g_free(startup_id);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Trash Error"), "dialog-error",
                            _("Could not empty the trash"),
                            _("This feature requires a trash service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }
}

static void
show_template_creation_error(GtkWindow *parent, GFile *dest_file, const gchar *template_name, GError *error) {
    gchar *secondary = g_strdup_printf(_("Unable to create new file \"%1$s\" from template file \"%2$s\": %3$s"),
                                       g_file_peek_path(dest_file),
                                       template_name,
                                       error->message);

    xfce_message_dialog(parent,
                        _("Create File Error"),
                        "dialog-error",
                        _("Could not create a new file"),
                        secondary,
                        XFCE_BUTTON_TYPE_MIXED,
                        "window-close",
                        _("_Close"),
                        GTK_RESPONSE_ACCEPT,
                        NULL);

    g_free(secondary);
}

static GFile *
new_empty_file_name(GFile *folder) {
    GFile *file = g_file_get_child(folder, _("New Empty File"));
    GFile *next_file = xfdesktop_file_utils_next_new_file_name(file);
    g_object_unref(file);
    return next_file;
}

static void
template_create_done(GObject *source, GAsyncResult *res, gpointer data) {
    TemplateCreateData *tcdata = data;

    GError *error = NULL;
    if (!g_file_copy_finish(G_FILE(source), res, &error)) {
        gchar *template_name = g_file_get_basename(G_FILE(source));
        show_template_creation_error(tcdata->parent, tcdata->dest_file, template_name, error);
        g_free(template_name);
        g_error_free(error);
    }

    g_object_unref(tcdata->dest_file);
    g_free(tcdata);
}

static void
empty_file_close_done(GObject *source, GAsyncResult *res, gpointer data) {
    TemplateCreateData *tcdata = data;
    GOutputStream *stream = G_OUTPUT_STREAM(source);

    GError *error = NULL;
    if (!g_output_stream_close_finish(G_OUTPUT_STREAM(source), res, &error)) {
        gchar *name = g_file_get_basename(G_FILE(source));
        show_template_creation_error(tcdata->parent, tcdata->dest_file, name, error);
        g_free(name);
        g_error_free(error);
    }

    g_object_unref(stream);
    g_object_unref(tcdata->dest_file);
    g_free(tcdata);
}

static void
empty_file_create_done(GObject *source, GAsyncResult *res, gpointer data) {
    TemplateCreateData *tcdata = data;

    GError *error = NULL;
    GFileOutputStream *stream = g_file_create_finish(G_FILE(source), res, &error);
    if (stream != NULL) {
        g_output_stream_close_async(G_OUTPUT_STREAM(stream), G_PRIORITY_DEFAULT, NULL, empty_file_close_done, tcdata);
    } else {
        gchar *name = g_file_get_basename(G_FILE(source));
        show_template_creation_error(tcdata->parent, G_FILE(source), name, error);
        g_free(name);
        g_error_free(error);

        g_object_unref(tcdata->dest_file);
        g_free(tcdata);
    }
}

void
xfdesktop_file_utils_create_file_from_template(GFile *template_file, GFile *dest_file, GtkWindow *parent) {
    g_return_if_fail(template_file == NULL || G_IS_FILE(template_file));
    g_return_if_fail(G_IS_FILE(dest_file));
    g_return_if_fail(parent == NULL || GTK_IS_WINDOW(parent));

    TemplateCreateData *tcdata = g_new0(TemplateCreateData, 1);
    tcdata->parent = parent;
    tcdata->dest_file = g_object_ref(dest_file);

    if (template_file != NULL) {
        g_file_copy_async(template_file,
                          tcdata->dest_file,
                          G_FILE_COPY_NONE,
                          G_PRIORITY_DEFAULT,
                          NULL,
                          NULL,
                          NULL,
                          template_create_done,
                          tcdata);
    } else {
        g_file_create_async(dest_file,
                            G_FILE_CREATE_NONE,
                            G_PRIORITY_DEFAULT,
                            NULL,
                            empty_file_create_done,
                            tcdata);
    }
}

static void
folder_create_done(GObject *source, GAsyncResult *res, gpointer data) {
    GError *error = NULL;
    if (!g_file_make_directory_finish(G_FILE(source), res, &error)) {
        GtkWindow *parent = data;
        gchar *folder_name = g_file_get_basename(G_FILE(source));
        gchar *secondary = g_strdup_printf(_("Unable to create new folder \"%1$s\": %2$s"),
                                           folder_name,
                                           error->message);
        xfce_message_dialog(parent,
                            _("Create File Error"),
                            "dialog-error",
                            _("Could not create a new folder"),
                            secondary,
                            XFCE_BUTTON_TYPE_MIXED,
                            "window-close",
                            _("_Close"),
                            GTK_RESPONSE_ACCEPT,
                            NULL);

        g_free(folder_name);
        g_free(secondary);
        g_error_free(error);
    }

    g_object_unref(source);
}

void
xfdesktop_file_utils_create_folder(GFile *folder, GtkWindow *parent) {
    g_file_make_directory_async(g_object_ref(folder), G_PRIORITY_DEFAULT, NULL, folder_create_done, parent);
}

static gchar *
show_editable_file_create_dialog(const gchar *title,
                                 GIcon *icon,
                                 const gchar *prompt,
                                 const gchar *prefill,
                                 GtkWindow *parent,
                                 const gchar *error_primary_text)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons(title,
                                                    parent,
                                                    GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                                    _("_Cancel"),
                                                    GTK_RESPONSE_CANCEL,
                                                    _("C_reate"),
                                                    GTK_RESPONSE_ACCEPT,
                                                    NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT, FALSE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 300, -1);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), hbox);

    GtkWidget *image = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_DIALOG);
    gtk_widget_set_margin_start(image, 6);
    gtk_widget_set_margin_end(image, 6);
    gtk_widget_set_margin_top(image, 6);
    gtk_widget_set_margin_bottom(image, 6);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, TRUE, 0);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    GtkWidget *label = gtk_label_new(prompt);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    GtkWidget *filename_input = g_object_new(XFCE_TYPE_FILENAME_INPUT,
                                    "original-filename", prefill,
                                    NULL);
    gtk_box_pack_start(GTK_BOX(vbox), filename_input, FALSE, FALSE, 0);
    g_signal_connect_swapped(filename_input, "text-invalid",
                             G_CALLBACK(xfce_filename_input_desensitise_widget),
                             gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT));
    g_signal_connect_swapped(filename_input, "text-valid",
                             G_CALLBACK(xfce_filename_input_sensitise_widget),
                             gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT));

    xfce_filename_input_check(XFCE_FILENAME_INPUT(filename_input));

    GtkEntry *filename_entry = xfce_filename_input_get_entry(XFCE_FILENAME_INPUT(filename_input));

    gtk_widget_show_all(hbox);
    gtk_widget_grab_focus(GTK_WIDGET(filename_entry));

    const gchar *ext = strrchr(prefill, '.');
    if (ext != NULL) {
        glong offset = g_utf8_pointer_to_offset(prefill, ext);
        if (offset >= 1) {
            gtk_editable_select_region(GTK_EDITABLE(filename_entry), 0, offset);
        }
    }

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response == GTK_RESPONSE_ACCEPT) {
        const gchar *text = xfce_filename_input_get_text(XFCE_FILENAME_INPUT(filename_input));
        gchar *filename = g_filename_from_utf8(text, -1, NULL, NULL, NULL);
        gtk_widget_destroy(dialog);

        if (filename == NULL) {
            gchar *secondary = g_strdup_printf(_("Cannot convert filename \"%s\" to the local encoding"), text);
            xfce_message_dialog(parent,
                                _("Create File Error"),
                                "dialog-error",
                                error_primary_text,
                                secondary,
                                XFCE_BUTTON_TYPE_MIXED,
                                "window-close",
                                _("_Close"),
                                GTK_RESPONSE_ACCEPT,
                                NULL);
            g_free(secondary);
        }

        return filename;
    } else {
        gtk_widget_destroy(dialog);
        return NULL;
    }
}

GFile *
xfdesktop_file_utils_prompt_for_template_file_name(GFile *parent_folder, GFile *template_file, GtkWindow *parent) {
    g_return_val_if_fail(G_IS_FILE(parent_folder), NULL);
    g_return_val_if_fail(template_file == NULL || G_IS_FILE(template_file), NULL);
    g_return_val_if_fail(parent == NULL || GTK_IS_WINDOW(parent), NULL);

    GIcon *icon = NULL;
    if (template_file != NULL) {
        GFileInfo *info = g_file_query_info(template_file,
                                            G_FILE_ATTRIBUTE_STANDARD_ICON,
                                            G_FILE_QUERY_INFO_NONE,
                                            NULL, NULL);
        if (info != NULL) {
            if (g_file_info_get_icon(info) != NULL) {
                icon = g_object_ref(g_file_info_get_icon(info));
            }
            g_object_unref(info);
        }
    }

    if (icon == NULL) {
        icon = g_content_type_get_icon("text/plain");
    }

    gchar *name;
    if (template_file != NULL) {
        name = g_file_get_basename(template_file);
        GFile *new_file = g_file_get_child(parent_folder, name);
        g_free(name);

        GFile *next_new_file = xfdesktop_file_utils_next_new_file_name(new_file);
        name = g_file_get_basename(next_new_file);

        g_object_unref(new_file);
        g_object_unref(next_new_file);
    } else {
        GFile *new_file = new_empty_file_name(parent_folder);
        name = g_file_get_basename(new_file);
        g_object_unref(new_file);
    }

    gchar *title = g_strdup_printf(_("Create Document from template \"%s\""), name);
    gchar *filename = show_editable_file_create_dialog(title,
                                                       icon,
                                                       _("Enter the name:"),
                                                       name,
                                                       parent,
                                                       _("Could not create a new file"));

    g_free(name);
    g_free(title);
    g_object_unref(icon);

    if (filename != NULL) {
        GFile *target_file = g_file_get_child(parent_folder, filename);
        g_free(filename);
        return target_file;
    } else {
        return NULL;
    }
}

GFile *
xfdesktop_file_utils_prompt_for_new_folder_name(GFile *parent_folder, GtkWindow *parent) {
    g_return_val_if_fail(G_IS_FILE(parent_folder), NULL);
    g_return_val_if_fail(parent == NULL || GTK_IS_WINDOW(parent), NULL);

    GIcon *icon = g_content_type_get_icon("inode/directory");

    GFile *new_folder = g_file_get_child(parent_folder, _("New Folder"));
    GFile *next_new_folder = xfdesktop_file_utils_next_new_file_name(new_folder);
    gchar *prefill = g_file_get_basename(next_new_folder);
    g_object_unref(new_folder);
    g_object_unref(next_new_folder);

    gchar *folder_name = show_editable_file_create_dialog(_("Create New Folder"),
                                                          icon,
                                                          _("Enter the name:"),
                                                          prefill,
                                                          parent,
                                                          _("Could not create a new folder"));
    g_object_unref(icon);
    g_free(prefill);

    if (folder_name != NULL) {
        GFile *folder = g_file_get_child(parent_folder, folder_name);
        g_free(folder_name);
        return folder;
    } else {
        return NULL;
    }
}

static void
show_properties_fdo_cb(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    if (!xfdesktop_file_manager1_call_show_item_properties_finish(XFDESKTOP_FILE_MANAGER1(source_object), res, &error)) {
        xfdesktop_file_utils_async_handle_error(error, user_data);
    }
}


static void
show_properties_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    if (!xfdesktop_file_manager_call_display_file_properties_finish(XFDESKTOP_FILE_MANAGER(source_object), res, &error))
        xfdesktop_file_utils_async_handle_error(error, user_data);
}

void
xfdesktop_file_utils_show_properties_dialog(GList *files,
                                            GdkScreen *screen,
                                            GtkWindow *parent)
{
    XfdesktopFileManager *fileman_proxy;
    XfdesktopFileManager1 *fileman_fdo_proxy;

    g_return_if_fail(files != NULL);
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    fileman_fdo_proxy = xfdesktop_file_utils_peek_filemanager_fdo_proxy();

    if ((files->next != NULL || fileman_proxy == NULL) && fileman_fdo_proxy != NULL) {  // multiple files or no xfce fileman proxy
        gchar **uris = g_new0(gchar *, g_list_length(files) + 1);
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        gint i = 0;

        for (GList *l = files; l != NULL; l = l->next, ++i) {
            uris[i] = g_file_get_uri(G_FILE(l->data));
        }

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);

        xfdesktop_file_manager1_call_show_item_properties(fileman_fdo_proxy,
                                                          (const gchar *const *)uris,
                                                          startup_id,
                                                          NULL,
                                                          show_properties_fdo_cb,
                                                          parent);

        xfdesktop_file_utils_unset_window_cursor(parent);

        g_strfreev(uris);
        g_free(startup_id);
    } else if (files->next == NULL && fileman_proxy) {
        GFile *file = files->data;
        gchar *uri = g_file_get_uri(file);
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);

        xfdesktop_file_manager_call_display_file_properties(fileman_proxy,
                                                            uri, display_name, startup_id,
                                                            NULL,
                                                            show_properties_cb,
                                                            parent);

        xfdesktop_file_utils_unset_window_cursor(parent);

        g_free(startup_id);
        g_free(uri);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("File Properties Error"), "dialog-error",
                            _("The file properties dialog could not be opened"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }
}

static void
launch_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    if (!xfdesktop_file_manager_call_launch_files_finish(XFDESKTOP_FILE_MANAGER(source_object), res, &error))
        xfdesktop_file_utils_async_handle_error(error, user_data);
}

void
xfdesktop_file_utils_launch(GFile *file,
                            GdkScreen *screen,
                            GtkWindow *parent)
{
    XfdesktopFileManager *fileman_proxy;

    g_return_if_fail(G_IS_FILE(file));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        gchar **uris;
        GFile  *parent_file = g_file_get_parent(file);
        gchar  *parent_path = g_file_get_path(parent_file);
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar  *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);

        uris = g_new0(gchar *, 2);
        uris[0] = g_file_get_uri(file);
        uris[1] = NULL;

        xfdesktop_file_manager_call_launch_files(fileman_proxy, parent_path,
                                                 (const gchar * const*)uris,
                                                 display_name, startup_id,
                                                 NULL,
                                                 launch_cb,
                                                 parent);

        xfdesktop_file_utils_unset_window_cursor(parent);

        g_free(startup_id);
        g_free(uris[0]);
        g_free(uris);
        g_free(parent_path);
        g_object_unref(parent_file);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Launch Error"), "dialog-error",
                            _("The file could not be opened"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }
}

static void
execute_finished_cb(GObject *source,
                    GAsyncResult *res,
                    gpointer user_data)
{
    ExecuteData *edata = (ExecuteData *)user_data;
    gboolean ret;
    GError *error = NULL;

    ret = xfdesktop_file_manager_call_execute_finish(XFDESKTOP_FILE_MANAGER(source), res, &error);
    if (!ret) {
        gchar *filename = g_file_get_uri(edata->file);
        gchar *name = g_filename_display_basename(filename);
        gchar *primary = g_markup_printf_escaped(_("Failed to run \"%s\""), name);

        xfce_message_dialog(edata->parent,
                            _("Launch Error"), "dialog-error",
                            primary, error->message,
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);

        g_free(primary);
        g_free(name);
        g_free(filename);
        g_error_free(error);
    }

    if (edata->parent != NULL) {
        g_object_unref(edata->parent);
    }
    g_object_unref(edata->file);
    g_free(edata);
}

gboolean
xfdesktop_file_utils_execute(GFile *working_directory,
                             GFile *file,
                             GList *files,
                             GdkScreen *screen,
                             GtkWindow *parent)
{
    XfdesktopFileManager *fileman_proxy;
    gboolean success = TRUE;

    g_return_val_if_fail(working_directory == NULL || G_IS_FILE(working_directory), FALSE);
    g_return_val_if_fail(G_IS_FILE(file), FALSE);
    g_return_val_if_fail(screen == NULL || GDK_IS_SCREEN(screen), FALSE);
    g_return_val_if_fail(parent == NULL || GTK_IS_WINDOW(parent), FALSE);

    if(!screen)
        screen = gdk_display_get_default_screen(gdk_display_get_default());

    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        ExecuteData *edata;
        gchar *working_dir = working_directory != NULL ? g_file_get_uri(working_directory) : NULL;
        const gchar *path_prop;
        gchar *uri = g_file_get_uri(file);
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
        GList *lp;
        guint n = g_list_length (files);
        gchar **uris = g_new0 (gchar *, n + 1);

        for (n = 0, lp = files; lp != NULL; ++n, lp = lp->next)
            uris[n] = g_file_get_uri(lp->data);
        uris[n] = NULL;

        /* If the working_dir wasn't set check if this is a .desktop file
         * we can parse a working dir from */
        if(working_dir == NULL) {
            GFileInfo *info = g_file_query_info(file,
                                                XFDESKTOP_FILE_INFO_NAMESPACE,
                                                G_FILE_QUERY_INFO_NONE,
                                                NULL, NULL);

            if(xfdesktop_file_utils_is_desktop_file(info)) {
                XfceRc *rc;
                gchar *path = g_file_get_path(file);
                if(path != NULL) {
                    rc = xfce_rc_simple_open(path, TRUE);
                    if(rc != NULL) {
                        path_prop = xfce_rc_read_entry(rc, "Path", NULL);
                        if(xfce_str_is_empty(path_prop))
                            working_dir = g_strdup(g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP));
                        else
                            working_dir = g_strdup(path_prop);
                        xfce_rc_close(rc);
                    }
                    g_free(path);
                }
            }

            if(info)
                g_object_unref(info);
        }

        edata = g_new0(ExecuteData, 1);
        if (parent != NULL) {
            edata->parent = g_object_ref(parent);
        }
        edata->file = g_object_ref(file);
        xfdesktop_file_manager_call_execute(fileman_proxy,
                                            working_dir,
                                            uri,
                                            (const gchar **)uris,
                                            display_name,
                                            startup_id,
                                            NULL,
                                            execute_finished_cb,
                                            edata);

        g_free(startup_id);
        g_strfreev(uris);
        g_free(uri);
        g_free(working_dir);
        g_free(display_name);
    } else {
        gchar *filename = g_file_get_uri(file);
        gchar *name = g_filename_display_basename(filename);
        gchar *primary = g_markup_printf_escaped(_("Failed to run \"%s\""), name);

        xfce_message_dialog(parent,
                            _("Launch Error"), "dialog-error",
                            primary,
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);

        g_free(primary);
        g_free(name);
        g_free(filename);

        success = FALSE;
    }

    return success;
}

static void
display_chooser_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    if (!xfdesktop_file_manager_call_display_application_chooser_dialog_finish(XFDESKTOP_FILE_MANAGER(source_object), res, &error))
        xfdesktop_file_utils_async_handle_error(error, user_data);
}

void
xfdesktop_file_utils_display_app_chooser_dialog(GFile *file,
                                                gboolean open,
                                                gboolean preselect_default_checkbox,
                                                GdkScreen *screen,
                                                GtkWindow *parent)
{
    XfdesktopFileManager *fileman_proxy;

    g_return_if_fail(G_IS_FILE(file));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        gchar *uri = g_file_get_uri(file);
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);

        xfdesktop_file_manager_call_display_application_chooser_dialog(fileman_proxy,
                                                                       uri, open,
                                                                       preselect_default_checkbox,
                                                                       display_name,
                                                                       startup_id,
                                                                       NULL,
                                                                       display_chooser_cb,
                                                                       parent);

        xfdesktop_file_utils_unset_window_cursor(parent);

        g_free(startup_id);
        g_free(uri);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Launch Error"), "dialog-error",
                            _("The application chooser could not be opened"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }
}

/**
 * 'out_source_files' will hold a list owned by caller with contents owned by callee
 * 'dest_source_files' will hold a list and contents owned by caller
 */
void
xfdesktop_file_utils_build_transfer_file_lists(GdkDragAction action,
                                               GList *source_icons,
                                               XfdesktopFileIcon *dest_icon,
                                               GList **out_source_files,
                                               GList **out_dest_files)
{
    g_return_if_fail(source_icons != NULL);
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON(dest_icon));
    g_return_if_fail(out_source_files != NULL && out_dest_files != NULL);

    switch (action) {
        case GDK_ACTION_MOVE:
        case GDK_ACTION_LINK:
            *out_dest_files = g_list_append(NULL, g_object_ref(xfdesktop_file_icon_peek_file(dest_icon)));
            for (GList *l = source_icons; l != NULL; l = l->next) {
                GFile *source_file = xfdesktop_file_icon_peek_file(XFDESKTOP_FILE_ICON(l->data));
                if (source_file != NULL) {
                    *out_source_files = g_list_prepend(*out_source_files, source_file);
                }
            }
            *out_source_files = g_list_reverse(*out_source_files);
            break;

        case GDK_ACTION_COPY:
            for (GList *l = source_icons; l != NULL; l = l->next) {
                GFile *source_file = xfdesktop_file_icon_peek_file(XFDESKTOP_FILE_ICON(l->data));
                if (source_file != NULL) {
                    gchar *name = g_file_get_basename(source_file);
                    if (name != NULL) {
                        GFile *dest_file = g_file_get_child(xfdesktop_file_icon_peek_file(dest_icon), name);
                        *out_dest_files = g_list_prepend(*out_dest_files, dest_file);
                        *out_source_files = g_list_prepend(*out_source_files, source_file);
                        g_free(name);
                    }
                }
            }
            *out_source_files = g_list_reverse(*out_source_files);
            *out_dest_files = g_list_reverse(*out_dest_files);
            break;

        default:
            g_warning("Unsupported drag action: %d", action);
    }
}

static void
transfer_files_cb(GObject *source_object,
                  GAsyncResult *result,
                  gpointer user_data)
{
    gboolean (*finish_func)(XfdesktopFileManager *, GAsyncResult *, GError **) = user_data;
    GError *error = NULL;
    gboolean success;

    success = finish_func(XFDESKTOP_FILE_MANAGER(source_object), result, &error);
    if (!success) {
        gchar *message = error != NULL ? error->message : _("Unknown");
        xfce_message_dialog(NULL,
                            _("Transfer Error"), "dialog-error",
                            _("The file transfer could not be performed"),
                            message,
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);

        g_clear_error(&error);
    }
}

void
xfdesktop_file_utils_transfer_files(GdkDragAction action,
                                    GList *source_files,
                                    GList *target_files,
                                    GdkScreen *screen)
{
    XfdesktopFileManager *fileman_proxy;

    g_return_if_fail(action == GDK_ACTION_MOVE || action == GDK_ACTION_COPY || action == GDK_ACTION_LINK);
    g_return_if_fail(source_files != NULL && G_IS_FILE(source_files->data));
    g_return_if_fail(target_files != NULL && G_IS_FILE(target_files->data));
    g_return_if_fail(screen == NULL || GDK_IS_SCREEN(screen));

    if(!screen)
        screen = gdk_display_get_default_screen(gdk_display_get_default());

    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        gchar **source_uris = xfdesktop_file_utils_file_list_to_uri_array(source_files);
        gchar **target_uris = xfdesktop_file_utils_file_list_to_uri_array(target_files);
        gchar *display_name = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());

        switch(action) {
            case GDK_ACTION_MOVE:
                xfdesktop_file_manager_call_move_into(fileman_proxy, "",
                                                      (const gchar **)source_uris,
                                                      (const gchar *)target_uris[0],
                                                      display_name, startup_id,
                                                      NULL,
                                                      transfer_files_cb, xfdesktop_file_manager_call_move_into_finish);
                break;
            case GDK_ACTION_COPY:
                xfdesktop_file_manager_call_copy_to(fileman_proxy, "",
                                                    (const gchar **)source_uris,
                                                    (const gchar **)target_uris,
                                                    display_name, startup_id,
                                                    NULL,
                                                    transfer_files_cb, xfdesktop_file_manager_call_copy_to_finish);
                break;
            case GDK_ACTION_LINK:
                xfdesktop_file_manager_call_link_into(fileman_proxy, "",
                                                      (const gchar **)source_uris,
                                                      (const gchar *)target_uris[0],
                                                      display_name, startup_id,
                                                      NULL,
                                                      transfer_files_cb, xfdesktop_file_manager_call_link_into_finish);
                break;
            default:
                g_assert_not_reached();
                break;
        }

        g_free(startup_id);
        g_free(display_name);
        g_strfreev(target_uris);
        g_strfreev(source_uris);
    } else {
        xfce_message_dialog(NULL,
                            _("Transfer Error"), "dialog-error",
                            _("The file transfer could not be performed"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                            NULL);
    }
}

static gboolean
exo_desktop_item_edit_has_print_saved_uri_flag(void) {
    const gchar *test_argv[] = {
        "exo-desktop-item-edit",
        "--help",
        NULL,
    };

    gboolean has_print_saved_filename = FALSE;
    gchar *cmd_stdout = NULL;
    if (g_spawn_sync(NULL,
                     (gchar **)test_argv,
                     NULL,
                     G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
                     NULL,
                     NULL,
                     &cmd_stdout,
                     NULL,
                     NULL,
                     NULL)
        && cmd_stdout != NULL)
    {
        has_print_saved_filename = strstr(cmd_stdout, "--print-saved-uri") != NULL;
    }
    g_free(cmd_stdout);

    return has_print_saved_filename;
}

static void
create_desktop_file_data_free(CreateDesktopFileData *cdfdata) {
    if (cdfdata->cancellable != NULL) {
        g_object_unref(cdfdata->cancellable);
    }
    g_string_free(cdfdata->output_string, TRUE);
    g_free(cdfdata);
}

static void
create_desktop_file_stdout_data_ready(GObject *source, GAsyncResult *res, gpointer data) {
    GInputStream *stdout_stream = G_INPUT_STREAM(source);
    CreateDesktopFileData *cdfdata = data;

    GError *error = NULL;
    gssize bytes_read = g_input_stream_read_finish(stdout_stream, res, &error);
    if (bytes_read < 0) {
        g_input_stream_close(stdout_stream, NULL, NULL);
        g_object_unref(stdout_stream);

        cdfdata->callback(NULL, error, cdfdata->callback_data);
        g_error_free(error);
        create_desktop_file_data_free(cdfdata);
    } else if (bytes_read == 0) {
        g_input_stream_close(stdout_stream, NULL, NULL);
        g_object_unref(stdout_stream);

        while (g_str_has_suffix(cdfdata->output_string->str, "\n")) {
            g_string_truncate(cdfdata->output_string, cdfdata->output_string->len - 1);
        }

        if (cdfdata->output_string->len == 0) {
            error = g_error_new_literal(G_IO_ERROR, G_IO_ERROR_CANCELLED, "User cancelled dialog");
            cdfdata->callback(NULL, error, cdfdata->callback_data);
            g_error_free(error);
        } else {
            GFile *file = g_file_new_for_uri(cdfdata->output_string->str);
            cdfdata->callback(file, NULL, cdfdata->callback_data);
            g_object_unref(file);
        }

        create_desktop_file_data_free(cdfdata);
    } else {
        g_string_append_len(cdfdata->output_string, cdfdata->buffer, bytes_read);
        g_input_stream_read_async(stdout_stream,
                                  cdfdata->buffer,
                                  sizeof(cdfdata->buffer),
                                  G_PRIORITY_DEFAULT,
                                  cdfdata->cancellable,
                                  create_desktop_file_stdout_data_ready,
                                  cdfdata);
    }
}

void
xfdesktop_file_utils_create_desktop_file(GdkScreen *screen,
                                         GFile *folder,
                                         const gchar *launcher_type,
                                         const gchar *suggested_name,
                                         const gchar *suggested_command_or_url,
                                         GCancellable *cancellable,
                                         CreateDesktopFileCallback callback,
                                         gpointer callback_data)
{
    g_return_if_fail(screen == NULL || GDK_IS_SCREEN(screen));
    g_return_if_fail(G_IS_FILE(folder));
    g_return_if_fail(g_strcmp0(launcher_type, "Application") == 0 || g_strcmp0(launcher_type, "Link") == 0);

    GStrvBuilder *argv_builder = g_strv_builder_new();
    g_strv_builder_add(argv_builder, "exo-desktop-item-edit");

    if (screen != NULL && xfw_windowing_get() == XFW_WINDOWING_X11) {
        const gchar *display_name = gdk_display_get_name(gdk_screen_get_display(screen));
        g_strv_builder_add(argv_builder, "--display");
        g_strv_builder_add(argv_builder, display_name);
    }

    g_strv_builder_add(argv_builder, "--create-new");
    g_strv_builder_add(argv_builder, "--type");
    g_strv_builder_add(argv_builder, launcher_type);

    if (suggested_name != NULL) {
        g_strv_builder_add(argv_builder, "--name");
        g_strv_builder_add(argv_builder, suggested_name);
    }

    if (suggested_command_or_url != NULL) {
        if (g_strcmp0(launcher_type, "Application") == 0) {
            g_strv_builder_add(argv_builder, "--command");
        } else {
            g_strv_builder_add(argv_builder, "--url");
        }
        g_strv_builder_add(argv_builder, suggested_command_or_url);
    }

    gboolean used_print_saved_uri = callback != NULL && exo_desktop_item_edit_has_print_saved_uri_flag();
    if (used_print_saved_uri) {
        g_strv_builder_add(argv_builder, "--print-saved-uri");
    }

    gchar *uri = g_file_get_uri(folder);
    g_strv_builder_add(argv_builder, uri);
    g_free(uri);

    gchar **argv = g_strv_builder_end(argv_builder);

    GError *error = NULL;
    gint stdout_fd = -1;
    if (g_spawn_async_with_pipes(NULL,
                                 argv,
                                 NULL,
                                 G_SPAWN_SEARCH_PATH,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 used_print_saved_uri ? &stdout_fd : NULL,
                                 NULL,
                                 &error))
    {
        if (used_print_saved_uri && stdout_fd >= 0) {
            CreateDesktopFileData *cdfdata = g_new0(CreateDesktopFileData, 1);
            cdfdata->cancellable = cancellable != NULL ? g_object_ref(cancellable) : NULL;
            cdfdata->output_string = g_string_sized_new(sizeof(cdfdata->buffer));
            cdfdata->callback = callback;
            cdfdata->callback_data = callback_data;

            GInputStream *stdout_stream = g_unix_input_stream_new(stdout_fd, TRUE);
            g_input_stream_read_async(stdout_stream,
                                      cdfdata->buffer,
                                      sizeof(cdfdata->buffer),
                                      G_PRIORITY_DEFAULT,
                                      cdfdata->cancellable,
                                      create_desktop_file_stdout_data_ready,
                                      cdfdata);
        } else if (callback != NULL) {
            error = g_error_new_literal(G_IO_ERROR,
                                        G_IO_ERROR_NOT_SUPPORTED,
                                        "exo-desktop-item-edit doesn't support the --print-saved-uri option");
            callback(NULL, error, callback_data);
            g_error_free(error);
        }
    } else {
        if (callback != NULL) {
            callback(NULL, error, callback_data);
        }
        g_error_free(error);
    }

    g_strfreev(argv);
    g_strv_builder_unref(argv_builder);
}

static gint dbus_ref_cnt = 0;
static GDBusConnection *dbus_gconn = NULL;
static XfdesktopTrash *dbus_trash_proxy = NULL;
static XfdesktopFileManager *dbus_filemanager_proxy = NULL;
static XfdesktopFileManager1 *dbus_filemanager_fdo_proxy = NULL;
#ifdef HAVE_THUNARX
static XfdesktopThunar *dbus_thunar_proxy = NULL;
#else
static GDBusProxy *dbus_thunar_proxy = NULL;
#endif
gboolean
xfdesktop_file_utils_dbus_init(void)
{
    gboolean ret = TRUE;

    if(dbus_ref_cnt++)
        return TRUE;

    if(!dbus_gconn) {
        dbus_gconn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    }

    if(dbus_gconn) {
        xfdesktop_trash_proxy_new(dbus_gconn,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  "org.xfce.FileManager",
                                  "/org/xfce/FileManager",
                                  NULL,
                                  xfdesktop_file_utils_trash_proxy_new_cb,
                                  NULL);

        xfdesktop_file_manager_proxy_new(dbus_gconn,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         "org.xfce.FileManager",
                                         "/org/xfce/FileManager",
                                         NULL,
                                         xfdesktop_file_utils_file_manager_proxy_new_cb,
                                         NULL);

        xfdesktop_file_manager1_proxy_new(dbus_gconn,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         "org.freedesktop.FileManager1",
                                         "/org/freedesktop/FileManager1",
                                         NULL,
                                         xfdesktop_file_utils_file_manager_fdo_proxy_new_cb,
                                         NULL);

#ifdef HAVE_THUNARX
        xfdesktop_thunar_proxy_new(dbus_gconn,
                                   G_DBUS_PROXY_FLAGS_NONE,
                                   "org.xfce.FileManager",
                                   "/org/xfce/FileManager",
                                   NULL,
                                   xfdesktop_file_utils_thunar_proxy_new_cb,
                                   NULL);
#else
        dbus_thunar_proxy = NULL;
#endif

    } else {
        ret = FALSE;
        dbus_ref_cnt = 0;
    }

    return ret;
}

static XfdesktopTrash *
xfdesktop_file_utils_peek_trash_proxy(void)
{
    return dbus_trash_proxy;
}

static XfdesktopFileManager *
xfdesktop_file_utils_peek_filemanager_proxy(void)
{
    return dbus_filemanager_proxy;
}

static XfdesktopFileManager1 *
xfdesktop_file_utils_peek_filemanager_fdo_proxy(void)
{
    return dbus_filemanager_fdo_proxy;
}

#ifdef HAVE_THUNARX
static XfdesktopThunar *
xfdesktop_file_utils_peek_thunar_proxy(void)
{
    return dbus_thunar_proxy;
}
#else
static gpointer
xfdesktop_file_utils_peek_thunar_proxy(void)
{
    return NULL;
}
#endif

static void
xfdesktop_file_utils_trash_proxy_new_cb (GObject *source_object,
                                         GAsyncResult *res,
                                         gpointer user_data) {
    dbus_trash_proxy = xfdesktop_trash_proxy_new_finish (res, NULL);
}

static void
xfdesktop_file_utils_file_manager_proxy_new_cb (GObject *source_object,
                                                GAsyncResult *res,
                                                gpointer user_data) {
    dbus_filemanager_proxy = xfdesktop_file_manager_proxy_new_finish (res, NULL);
}

static void
xfdesktop_file_utils_file_manager_fdo_proxy_new_cb(GObject *source_object,
                                                   GAsyncResult *res,
                                                   gpointer user_data)
{
    dbus_filemanager_fdo_proxy = xfdesktop_file_manager1_proxy_new_finish(res, NULL);
}

#ifdef HAVE_THUNARX
static void
xfdesktop_file_utils_thunar_proxy_new_cb (GObject *source_object,
                                          GAsyncResult *res,
                                          gpointer user_data) {

    dbus_thunar_proxy = xfdesktop_thunar_proxy_new_finish (res, NULL);
}
#endif

void
xfdesktop_file_utils_dbus_cleanup(void)
{
    if(dbus_ref_cnt == 0 || --dbus_ref_cnt > 0)
        return;

    if(dbus_trash_proxy)
        g_object_unref(G_OBJECT(dbus_trash_proxy));
    if(dbus_filemanager_proxy)
        g_object_unref(G_OBJECT(dbus_filemanager_proxy));
    if (dbus_filemanager_fdo_proxy) {
        g_object_unref(G_OBJECT(dbus_filemanager_fdo_proxy));
    }
    if(dbus_thunar_proxy)
        g_object_unref(G_OBJECT(dbus_thunar_proxy));
    if(dbus_gconn)
        g_object_unref(G_OBJECT(dbus_gconn));
}



#ifdef HAVE_THUNARX

/* thunar extension interface stuff: ThunarxFileInfo implementation */

gchar *
xfdesktop_thunarx_file_info_get_name(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    GFile *file = xfdesktop_file_icon_peek_file(icon);

    return file ? g_file_get_basename(file) : NULL;
}

gchar *
xfdesktop_thunarx_file_info_get_uri(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    GFile *file = xfdesktop_file_icon_peek_file(icon);

    return file ? g_file_get_uri(file) : NULL;
}

gchar *
xfdesktop_thunarx_file_info_get_parent_uri(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    GFile *file = xfdesktop_file_icon_peek_file(icon);
    gchar *uri = NULL;

    if(file) {
        GFile *parent = g_file_get_parent(file);
        if(parent) {
            uri = g_file_get_uri(parent);
            g_object_unref(parent);
        }
    }

    return uri;
}

gchar *
xfdesktop_thunarx_file_info_get_uri_scheme_file(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    GFile *file = xfdesktop_file_icon_peek_file(icon);

    return file ? g_file_get_uri_scheme(file) : NULL;
}

gchar *
xfdesktop_thunarx_file_info_get_mime_type(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    GFileInfo *info = xfdesktop_file_icon_peek_file_info(icon);

    return info ? g_strdup(g_file_info_get_content_type(info)) : NULL;
}

gboolean
xfdesktop_thunarx_file_info_has_mime_type(ThunarxFileInfo *file_info,
                                          const gchar *mime_type)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    GFileInfo *info = xfdesktop_file_icon_peek_file_info(icon);
    const gchar *content_type;

    if(!info)
        return FALSE;

    content_type = g_file_info_get_content_type(info);
    return g_content_type_is_a(content_type, mime_type);
}

gboolean
xfdesktop_thunarx_file_info_is_directory(ThunarxFileInfo *file_info)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(file_info);
    GFileInfo *info = xfdesktop_file_icon_peek_file_info(icon);

    return (info && g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY);
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
