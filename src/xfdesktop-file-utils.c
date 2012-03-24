/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006      Brian Tarricone, <bjt23@cornell.edu>
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
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gio/gio.h>
#ifdef HAVE_GIO_UNIX
#include <gio/gunixmounts.h>
#endif

#include <gtk/gtk.h>

#include <libxfce4ui/libxfce4ui.h>

#include <exo/exo.h>

#include <dbus/dbus-glib-lowlevel.h>

#ifdef HAVE_THUNARX
#include <thunarx/thunarx.h>
#endif

#include "xfdesktop-common.h"
#include "xfdesktop-file-icon.h"
#include "xfdesktop-file-manager-proxy.h"
#include "xfdesktop-file-utils.h"
#include "xfdesktop-trash-proxy.h"

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

gboolean
xfdesktop_file_utils_volume_is_present(GVolume *volume)
{
    gboolean has_media = FALSE;
    gboolean is_shadowed = FALSE;
    GDrive *drive;
#if GLIB_CHECK_VERSION (2, 20, 0)
    GMount *mount;
#endif

    g_return_val_if_fail(G_IS_VOLUME(volume), FALSE);

    drive = g_volume_get_drive (volume);
    if(drive) {
        has_media = g_drive_has_media(drive);
        g_object_unref(drive);
    }

#if GLIB_CHECK_VERSION (2, 20, 0)
    mount = g_volume_get_mount(volume);
    if(mount) {
        is_shadowed = g_mount_is_shadowed(mount);
        g_object_unref(mount);
    }
#endif

    return has_media && !is_shadowed;
}

#ifdef HAVE_GIO_UNIX
static gboolean
xfdesktop_file_utils_mount_is_internal (GMount *mount)
{
    const gchar *point_mount_path;
    gboolean is_internal = FALSE;
    GFile *root;
    GList *lp;
    GList *mount_points;
    gchar *mount_path;

    g_return_val_if_fail(G_IS_MOUNT(mount), FALSE);

    /* determine the mount path */
    root = g_mount_get_root(mount);
    mount_path = g_file_get_path(root);
    g_object_unref(root);

    /* assume non-internal if we cannot determine the path */
    if (!mount_path)
        return FALSE;

    if (g_unix_is_mount_path_system_internal(mount_path)) {
        /* mark as internal */
        is_internal = TRUE;
    } else {
        /* get a list of all mount points */
        mount_points = g_unix_mount_points_get(NULL);

        /* search for the mount point associated with the mount entry */
        for (lp = mount_points; !is_internal && lp != NULL; lp = lp->next) {
            point_mount_path = g_unix_mount_point_get_mount_path(lp->data);

            /* check if this is the mount point we are looking for */
            if (g_strcmp0(mount_path, point_mount_path) == 0) {
                /* mark as internal if the user cannot mount this device */
                if (!g_unix_mount_point_is_user_mountable(lp->data))
                    is_internal = TRUE;
            }

            /* free the mount point, we no longer need it */
            g_unix_mount_point_free(lp->data);
        }

        /* free the mount point list */
        g_list_free(mount_points);
    }

    g_free(mount_path);

    return is_internal;
}
#endif



gboolean
xfdesktop_file_utils_volume_is_removable(GVolume *volume)
{
  gboolean can_eject = FALSE;
  gboolean can_mount = FALSE;
  gboolean can_unmount = FALSE;
  gboolean is_removable = FALSE;
  gboolean is_internal = FALSE;
  GDrive *drive;
  GMount *mount;

  g_return_val_if_fail(G_IS_VOLUME(volume), FALSE);

  /* check if the volume can be ejected */
  can_eject = g_volume_can_eject(volume);

  /* determine the drive for the volume */
  drive = g_volume_get_drive(volume);
  if(drive) {
      /*check if the drive media can be removed */
      is_removable = g_drive_is_media_removable(drive);

      /* release the drive */
      g_object_unref(drive);
  }

  /* determine the mount for the volume (if it is mounted at all) */
  mount = g_volume_get_mount(volume);
  if(mount) {
#ifdef HAVE_GIO_UNIX
      is_internal = xfdesktop_file_utils_mount_is_internal (mount);
#endif

      /* check if the volume can be unmounted */
      can_unmount = g_mount_can_unmount(mount);

      /* release the mount */
      g_object_unref(mount);
  }

  /* determine whether the device can be mounted */
  can_mount = g_volume_can_mount(volume);

  return (!is_internal) && (can_eject || can_unmount || is_removable || can_mount);
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
xfdesktop_file_utils_get_icon(const gchar *custom_icon_name,
                              GIcon *icon,
                              gint size,
                              const GdkPixbuf *emblem,
                              guint opacity)
{
    GtkIconTheme *itheme = gtk_icon_theme_get_default();
    GdkPixbuf *pix_theme = NULL, *pix = NULL;

    if(custom_icon_name) {
        pix_theme = gtk_icon_theme_load_icon(itheme, custom_icon_name, size,
                                             ITHEME_FLAGS, NULL);
        if(!pix_theme && *custom_icon_name == '/' && g_file_test(custom_icon_name, G_FILE_TEST_IS_REGULAR))
            pix_theme = gdk_pixbuf_new_from_file_at_size(custom_icon_name, size, size, NULL);
    }

    if(!pix_theme && icon) {
        if(G_IS_THEMED_ICON(icon)) {
          GtkIconInfo *icon_info = gtk_icon_theme_lookup_by_gicon(itheme,
                                                                  icon, size,
                                                                  ITHEME_FLAGS);
          if(icon_info) {
              pix_theme = gtk_icon_info_load_icon(icon_info, NULL);
              gtk_icon_info_free(icon_info);
          }
        } else if(G_IS_LOADABLE_ICON(icon)) {
            GInputStream *stream = g_loadable_icon_load(G_LOADABLE_ICON(icon),
                                                        size, NULL, NULL, NULL);
            if(stream) {
                pix = gdk_pixbuf_new_from_stream(stream, NULL, NULL);
                g_object_unref(stream);
            }
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

    if(opacity != 100) {
        GdkPixbuf *tmp = exo_gdk_pixbuf_lucent(pix, opacity);
        g_object_unref(G_OBJECT(pix));
        pix = tmp;
    }

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
    gchar *uri = NULL;
    GError *error = NULL;

    g_return_if_fail(G_IS_FILE(file));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    uri = g_file_get_uri(file);

    if(!exo_execute_preferred_application_on_screen("FileManager",
                                                    uri,
                                                    NULL,
                                                    NULL,
                                                    screen,
                                                    &error))
    {
        xfce_message_dialog(parent,
                            _("Launch Error"), GTK_STOCK_DIALOG_ERROR,
                            _("The folder could not be opened"),
                            error->message, GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                            NULL);

        g_error_free(error);
    }

    g_free(uri);
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
xfdesktop_file_utils_trash_files(GList *files,
                                 GdkScreen *screen,
                                 GtkWindow *parent)
{
    DBusGProxy *trash_proxy;

    g_return_if_fail(files != NULL && G_IS_FILE(files->data));
    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    trash_proxy = xfdesktop_file_utils_peek_trash_proxy();
    if(trash_proxy) {
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

        if(!xfdesktop_trash_proxy_move_to_trash(trash_proxy,
                                                (const gchar **)uris,
                                                display_name, startup_id,
                                                &error))
        {
            xfce_message_dialog(parent,
                                _("Trash Error"), GTK_STOCK_DIALOG_ERROR,
                                _("The selected files could not be moved to the trash"),
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
                            _("Trash Error"), GTK_STOCK_DIALOG_ERROR,
                            _("The selected files could not be moved to the trash"),
                            _("This feature requires a trash service to "
                              "be present (such as the one supplied by Thunar)."),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
    }
}

void
xfdesktop_file_utils_empty_trash(GdkScreen *screen,
                                 GtkWindow *parent)
{
    DBusGProxy *trash_proxy;

    g_return_if_fail(GDK_IS_SCREEN(screen) || GTK_IS_WINDOW(parent));

    if(!screen)
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));

    trash_proxy = xfdesktop_file_utils_peek_trash_proxy();
    if(trash_proxy) {
        GError *error = NULL;
        gchar *display_name = gdk_screen_make_display_name(screen);
        gchar *startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());

        xfdesktop_file_utils_set_window_cursor(parent, GDK_WATCH);

        if(!xfdesktop_trash_proxy_empty_trash(trash_proxy,
                                              display_name, startup_id,
                                              &error))
        {
            xfce_message_dialog(parent,
                                _("Trash Error"), GTK_STOCK_DIALOG_ERROR,
                                _("Could not empty the trash"),
                                error->message, GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                                NULL);

            g_error_free(error);
        }

        xfdesktop_file_utils_set_window_cursor(parent, GDK_LEFT_PTR);

        g_free(startup_id);
        g_free(display_name);
    } else {
        xfce_message_dialog(parent,
                            _("Trash Error"), GTK_STOCK_DIALOG_ERROR,
                            _("Could not empty the trash"),
                            _("This feature requires a trash service to "
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
                                error->message,
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);

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

gboolean
xfdesktop_file_utils_execute(GFile *working_directory,
                             GFile *file,
                             GList *files,
                             GdkScreen *screen,
                             GtkWindow *parent)
{
    DBusGProxy *fileman_proxy;
    gboolean success = TRUE;

    g_return_val_if_fail(working_directory == NULL || G_IS_FILE(working_directory), FALSE);
    g_return_val_if_fail(G_IS_FILE(file), FALSE);
    g_return_val_if_fail(screen == NULL || GDK_IS_SCREEN(screen), FALSE);
    g_return_val_if_fail(parent == NULL || GTK_IS_WINDOW(parent), FALSE);

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

            xfce_message_dialog(parent,
                                _("Launch Error"), GTK_STOCK_DIALOG_ERROR,
                                primary, error->message,
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                                NULL);

            g_free(primary);
            g_free(name);
            g_free(filename);

            g_error_free(error);

            success = FALSE;
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

        xfce_message_dialog(parent,
                            _("Launch Error"), GTK_STOCK_DIALOG_ERROR,
                            primary,
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);

        g_free(primary);
        g_free(name);
        g_free(filename);

        success = FALSE;
    }

    return success;
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

gboolean
xfdesktop_file_utils_transfer_files(GdkDragAction action,
                                    GList *source_files,
                                    GList *target_files,
                                    GdkScreen *screen)
{
    DBusGProxy *fileman_proxy;
    gboolean success = TRUE;

    g_return_val_if_fail(source_files != NULL && G_IS_FILE(source_files->data), FALSE);
    g_return_val_if_fail(target_files != NULL && G_IS_FILE(target_files->data), FALSE);
    g_return_val_if_fail(screen == NULL || GDK_IS_SCREEN(screen), FALSE);

    if(!screen)
        screen = gdk_display_get_default_screen(gdk_display_get_default());

    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        GError *error = NULL;
        gchar **source_uris = xfdesktop_file_utils_file_list_to_uri_array(source_files);
        gchar **target_uris = xfdesktop_file_utils_file_list_to_uri_array(target_files);
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
                success = FALSE;
                break;
        }

        if(error) {
            xfce_message_dialog(NULL,
                                _("Transfer Error"), GTK_STOCK_DIALOG_ERROR,
                                _("The file transfer could not be performed"),
                                error->message, GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                                NULL);

            g_error_free(error);

            success = FALSE;
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

        success = FALSE;
    }

    return success;
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
    return g_content_type_is_a(mime_type, content_type);
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
