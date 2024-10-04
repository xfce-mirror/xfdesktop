/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006 Brian Tarricone, <brian@tarricone.org>
 *  Copyright(c) 2006 Benedikt Meurer, <benny@xfce.org>
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

#include <exo/exo.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include <gio/gio.h>

#include <libxfce4ui/libxfce4ui.h>

#ifdef HAVE_THUNARX
#include <thunarx/thunarx.h>
#endif

#include "xfdesktop-common.h"
#include "xfdesktop-file-icon.h"
#include "xfdesktop-file-utils.h"
#include "xfdesktop-regular-file-icon.h"

#define EMBLEM_UNREADABLE "emblem-unreadable"
#define EMBLEM_READONLY   "emblem-readonly"
#define EMBLEM_SYMLINK    "emblem-symbolic-link"

struct _XfdesktopRegularFileIcon
{
    XfdesktopFileIcon parent_instance;

    XfconfChannel *channel;
    GdkScreen *gscreen;
    GFile *file;
    GFileInfo *file_info;

    gchar *display_name;
    gchar *tooltip;
    GFileInfo *filesystem_info;
    GFile *thumbnail_file;
    GFileMonitor *monitor;
    gboolean show_thumbnails;
    gboolean is_hidden;
};

enum {
    PROP0,
    PROP_CHANNEL,
    PROP_GDK_SCREEN,
    PROP_FILE,
    PROP_FILE_INFO,
    PROP_SHOW_THUMBNAILS,
};

static void xfdesktop_regular_file_icon_constructed(GObject *obj);
static void xfdesktop_regular_file_icon_set_property(GObject *obj,
                                                     guint property_id,
                                                     const GValue *value,
                                                     GParamSpec *pspec);
static void xfdesktop_regular_file_icon_get_property(GObject *obj,
                                                     guint property_id,
                                                     GValue *value,
                                                     GParamSpec *pspec);
static void xfdesktop_regular_file_icon_finalize(GObject *obj);

static void xfdesktop_regular_file_icon_set_thumbnail_file(XfdesktopIcon *icon, GFile *file);
static void xfdesktop_regular_file_icon_delete_thumbnail_file(XfdesktopIcon *icon);

static const gchar *xfdesktop_regular_file_icon_peek_label(XfdesktopIcon *icon);
static const gchar *xfdesktop_regular_file_icon_peek_tooltip(XfdesktopIcon *icon);

static GIcon *xfdesktop_regular_file_icon_get_gicon(XfdesktopFileIcon *icon);
static GFileInfo *xfdesktop_regular_file_icon_peek_file_info(XfdesktopFileIcon *icon);
static GFileInfo *xfdesktop_regular_file_icon_peek_filesystem_info(XfdesktopFileIcon *icon);
static GFile *xfdesktop_regular_file_icon_peek_file(XfdesktopFileIcon *icon);
static void xfdesktop_regular_file_icon_update_file_info(XfdesktopFileIcon *icon,
                                                         GFileInfo *info);
static gboolean xfdesktop_regular_file_can_write_parent(XfdesktopFileIcon *icon);
static gboolean xfdesktop_regular_file_icon_is_hidden_file(XfdesktopFileIcon *icon);

static void cb_folder_contents_changed(GFileMonitor *monitor,
                                       GFile *file,
                                       GFile *other_file,
                                       GFileMonitorEvent event,
                                       gpointer user_data);
static gboolean is_file_hidden(GFile *file, GFileInfo *info);
static gboolean is_folder_icon(GFile *file);

#ifdef HAVE_THUNARX
static void xfdesktop_regular_file_icon_tfi_init(ThunarxFileInfoIface *iface);

G_DEFINE_TYPE_EXTENDED(XfdesktopRegularFileIcon, xfdesktop_regular_file_icon,
                       XFDESKTOP_TYPE_FILE_ICON, 0,
                       G_IMPLEMENT_INTERFACE(THUNARX_TYPE_FILE_INFO,
                                             xfdesktop_regular_file_icon_tfi_init))
#else
G_DEFINE_TYPE(XfdesktopRegularFileIcon, xfdesktop_regular_file_icon, XFDESKTOP_TYPE_FILE_ICON)
#endif


/* So much for standards */
static const gchar *folder_icon_names[] = {
    "Folder.jpg",
    "folder.jpg",
    "Folder.JPG",
    "folder.JPG",
    "folder.jpeg",
    "folder.JPEG",
    "Folder.JPEG",
    "Folder.jpeg",
    "Cover.jpg",
    "cover.jpg",
    "Cover.jpeg",
    "cover.jpeg",
    "albumart.jpg",
    "albumart.jpeg",
    "fanart.jpg",
    "Fanart.jpg",
    "fanart.JPG",
    "Fanart.JPG",
    "FANART.JPG",
    "FANART.jpg",
};

static void
xfdesktop_regular_file_icon_class_init(XfdesktopRegularFileIconClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    XfdesktopIconClass *icon_class = (XfdesktopIconClass *)klass;
    XfdesktopFileIconClass *file_icon_class = (XfdesktopFileIconClass *)klass;

    gobject_class->constructed = xfdesktop_regular_file_icon_constructed;
    gobject_class->set_property = xfdesktop_regular_file_icon_set_property;
    gobject_class->get_property = xfdesktop_regular_file_icon_get_property;
    gobject_class->finalize = xfdesktop_regular_file_icon_finalize;

    icon_class->peek_label = xfdesktop_regular_file_icon_peek_label;
    icon_class->peek_tooltip = xfdesktop_regular_file_icon_peek_tooltip;
    icon_class->set_thumbnail_file = xfdesktop_regular_file_icon_set_thumbnail_file;
    icon_class->delete_thumbnail_file = xfdesktop_regular_file_icon_delete_thumbnail_file;

    file_icon_class->get_gicon = xfdesktop_regular_file_icon_get_gicon;
    file_icon_class->peek_file_info = xfdesktop_regular_file_icon_peek_file_info;
    file_icon_class->peek_filesystem_info = xfdesktop_regular_file_icon_peek_filesystem_info;
    file_icon_class->peek_file = xfdesktop_regular_file_icon_peek_file;
    file_icon_class->update_file_info = xfdesktop_regular_file_icon_update_file_info;
    file_icon_class->can_rename_file = xfdesktop_regular_file_can_write_parent;
    file_icon_class->can_delete_file = xfdesktop_regular_file_can_write_parent;
    file_icon_class->is_hidden_file = xfdesktop_regular_file_icon_is_hidden_file;

    g_object_class_install_property(gobject_class,
                                    PROP_CHANNEL,
                                    g_param_spec_object("channel",
                                                        "channel",
                                                        "xfconf channel",
                                                        XFCONF_TYPE_CHANNEL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class,
                                    PROP_GDK_SCREEN,
                                    g_param_spec_object("gdk-screen",
                                                        "gdk-screen",
                                                        "GDK screen",
                                                        GDK_TYPE_SCREEN,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class,
                                    PROP_FILE,
                                    g_param_spec_object("file",
                                                        "file",
                                                        "GFile",
                                                        G_TYPE_FILE,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class,
                                    PROP_FILE_INFO,
                                    g_param_spec_object("file-info",
                                                        "file-info",
                                                        "GFileInfo",
                                                        G_TYPE_FILE_INFO,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class,
                                    PROP_SHOW_THUMBNAILS,
                                    g_param_spec_boolean("show-thumbnails",
                                                         "show-thumbnails",
                                                         "show-thumbnails",
                                                         TRUE,
                                                         G_PARAM_READWRITE));
}

static void
xfdesktop_regular_file_icon_init(XfdesktopRegularFileIcon *icon)
{
    icon = xfdesktop_regular_file_icon_get_instance_private(icon);
    icon->display_name = NULL;
    icon->show_thumbnails = TRUE;
}

static void
xfdesktop_regular_file_icon_constructed(GObject *obj) {
    G_OBJECT_CLASS(xfdesktop_regular_file_icon_parent_class)->constructed(obj);

    XfdesktopRegularFileIcon *regular_file_icon = XFDESKTOP_REGULAR_FILE_ICON(obj);

    regular_file_icon->display_name = xfdesktop_file_utils_get_display_name(regular_file_icon->file,
                                                                                  regular_file_icon->file_info);

    regular_file_icon->filesystem_info = g_file_query_filesystem_info(regular_file_icon->file,
                                                                            XFDESKTOP_FILESYSTEM_INFO_NAMESPACE,
                                                                            NULL, NULL);

    regular_file_icon->is_hidden = is_file_hidden(regular_file_icon->file, regular_file_icon->file_info);

    if (g_file_info_get_file_type(regular_file_icon->file_info) == G_FILE_TYPE_DIRECTORY) {
        regular_file_icon->monitor = g_file_monitor(regular_file_icon->file,
                                                          G_FILE_MONITOR_WATCH_MOVES,
                                                          NULL,
                                                          NULL);

        g_signal_connect(regular_file_icon->monitor, "changed",
                         G_CALLBACK(cb_folder_contents_changed),
                         regular_file_icon);

    }

    xfconf_g_property_bind(regular_file_icon->channel,
                           DESKTOP_ICONS_SHOW_THUMBNAILS,
                           G_TYPE_BOOLEAN,
                           regular_file_icon,
                           "show-thumbnails");
}

static void
xfdesktop_regular_file_icon_set_property(GObject *obj, guint property_id, const GValue *value, GParamSpec *pspec) {
    XfdesktopRegularFileIcon *icon = XFDESKTOP_REGULAR_FILE_ICON(obj);

    switch (property_id) {
        case PROP_CHANNEL:
            icon->channel = g_value_dup_object(value);
            break;

        case PROP_GDK_SCREEN:
            icon->gscreen = g_value_get_object(value);
            break;

        case PROP_FILE:
            icon->file = g_value_dup_object(value);
            break;

        case PROP_FILE_INFO:
            icon->file_info = g_value_dup_object(value);
            break;

        case PROP_SHOW_THUMBNAILS:
            if (icon->show_thumbnails != g_value_get_boolean(value)) {
                icon->show_thumbnails = g_value_get_boolean(value);

                XF_DEBUG("show-thumbnails changed! now: %s", icon->show_thumbnails ? "TRUE" : "FALSE");
                xfdesktop_file_icon_invalidate_icon(XFDESKTOP_FILE_ICON(icon));
                xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(icon));
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
            break;
    }
}

static void
xfdesktop_regular_file_icon_get_property(GObject *obj, guint property_id, GValue *value, GParamSpec *pspec) {
    XfdesktopRegularFileIcon *icon = XFDESKTOP_REGULAR_FILE_ICON(obj);

    switch (property_id) {
        case PROP_CHANNEL:
            g_value_set_object(value, icon->channel);
            break;

        case PROP_GDK_SCREEN:
            g_value_set_object(value, icon->gscreen);
            break;

        case PROP_FILE:
            g_value_set_object(value, icon->file);
            break;

        case PROP_FILE_INFO:
            g_value_set_object(value, icon->file_info);
            break;

        case PROP_SHOW_THUMBNAILS:
            g_value_set_boolean(value, icon->show_thumbnails);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
            break;
    }
}

static void
xfdesktop_regular_file_icon_finalize(GObject *obj)
{
    XfdesktopRegularFileIcon *icon = XFDESKTOP_REGULAR_FILE_ICON(obj);

    if(icon->file_info)
        g_object_unref(icon->file_info);

    if(icon->filesystem_info)
        g_object_unref(icon->filesystem_info);

    g_object_unref(icon->file);

    g_free(icon->display_name);

    if(icon->tooltip)
        g_free(icon->tooltip);

    if(icon->thumbnail_file)
        g_object_unref(icon->thumbnail_file);

    if(icon->monitor)
        g_object_unref(icon->monitor);

    g_object_unref(icon->channel);

    G_OBJECT_CLASS(xfdesktop_regular_file_icon_parent_class)->finalize(obj);
}

#ifdef HAVE_THUNARX
static void
xfdesktop_regular_file_icon_tfi_init(ThunarxFileInfoIface *iface)
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
#endif

static void
xfdesktop_regular_file_icon_delete_thumbnail_file(XfdesktopIcon *icon)
{
    XfdesktopRegularFileIcon *file_icon;

    if(!XFDESKTOP_IS_REGULAR_FILE_ICON(icon))
        return;

    file_icon = XFDESKTOP_REGULAR_FILE_ICON(icon);

    g_clear_object(&file_icon->thumbnail_file);

    xfdesktop_file_icon_invalidate_icon(XFDESKTOP_FILE_ICON(icon));

    xfdesktop_icon_pixbuf_changed(icon);
}

static void
xfdesktop_regular_file_icon_set_thumbnail_file(XfdesktopIcon *icon, GFile *file)
{
    XfdesktopRegularFileIcon *file_icon;

    if(!XFDESKTOP_IS_REGULAR_FILE_ICON(icon))
        return;

    file_icon = XFDESKTOP_REGULAR_FILE_ICON(icon);

    if(file_icon->thumbnail_file)
        g_object_unref(file_icon->thumbnail_file);

    file_icon->thumbnail_file = file;

    xfdesktop_file_icon_invalidate_icon(XFDESKTOP_FILE_ICON(icon));

    xfdesktop_icon_pixbuf_changed(icon);
}

/* builds a folder/file path and then tests if that file is a valid image.
 * returns the file location if it does, NULL if it doesn't */
static gchar *
xfdesktop_check_file_is_valid(const gchar *folder, const gchar *file)
{
    gchar *path = g_strconcat(folder, "/", file, NULL);

    if(gdk_pixbuf_get_file_info(path, NULL, NULL) == NULL) {
        g_free(path);
        path = NULL;
    }

    return path;
}

static gboolean
is_folder_icon(GFile *file) {
    if (file != NULL) {
        gboolean match = FALSE;

        gchar *filename = g_file_get_basename(file);
        for (gsize i = 0; i < G_N_ELEMENTS(folder_icon_names); ++i) {
            if (g_strcmp0(filename, folder_icon_names[i]) == 0) {
                match = TRUE;
                break;
            }
        }

        g_free(filename);
        return match;
    } else {
        return FALSE;
    }
}

static gchar *
xfdesktop_load_icon_location_from_folder(XfdesktopFileIcon *icon)
{
    gchar *icon_file = g_file_get_path(xfdesktop_file_icon_peek_file(icon));
    gchar *path = NULL;

    g_return_val_if_fail(icon_file, NULL);

    for (gsize i = 0; i < G_N_ELEMENTS(folder_icon_names); ++i) {
        path = xfdesktop_check_file_is_valid(icon_file, folder_icon_names[i]);
        if (path != NULL) {
            break;
        }
    }

    g_free(icon_file);

    /* the file *should* already be a thumbnail */
    return path;
}

static GIcon *
xfdesktop_load_icon_from_desktop_file(XfdesktopRegularFileIcon *regular_icon)
{
    GKeyFile *key_file;
    gchar *contents, *icon_name;
    gsize length;
    GIcon *gicon = NULL;
    GtkIconTheme *itheme = gtk_icon_theme_get_default();
    gboolean is_pixmaps = FALSE;

    /* try to load the file into memory */
    if(!g_file_load_contents(regular_icon->file, NULL, &contents, &length, NULL, NULL))
        return NULL;

    /* allocate a new key file */
    key_file = g_key_file_new();

    /* try to parse the key file from the contents of the file */
    if(!g_key_file_load_from_data(key_file, contents, length, 0, NULL)) {
        g_free(contents);
        return NULL;
    }

    /* try to determine the custom icon name */
    icon_name = g_key_file_get_string(key_file,
                                      G_KEY_FILE_DESKTOP_GROUP,
                                      G_KEY_FILE_DESKTOP_KEY_ICON,
                                      NULL);

    /* No icon name in the desktop file */
    if(icon_name == NULL) {
        /* free key file and in-memory data */
        g_key_file_free(key_file);
        g_free(contents);
        return NULL;
    }

    /* icon_name is an absolute path, create it as a file icon */
    if(g_file_test(icon_name, G_FILE_TEST_IS_REGULAR)) {
        gicon = g_file_icon_new(g_file_new_for_path(icon_name));
    }

    /* check if the icon theme includes the icon name as-is */
    if(gicon == NULL) {
        if(gtk_icon_theme_has_icon(itheme, icon_name)) {
            /* load it */
            gicon = g_themed_icon_new(icon_name);
        }
    }

    /* drop any suffix (e.g. '.png') from the icon name and try to load that */
    if(gicon == NULL) {
        gchar *tmp_name = NULL;
        gchar *p = strrchr(icon_name, '.');

        if(!(g_strcmp0(p, ".png") == 0 || g_strcmp0(p, ".xpm") == 0 || g_strcmp0(p, ".svg") == 0))
            p = NULL;

        if(p != NULL)
            tmp_name = g_strndup(icon_name, p - icon_name);
        else
            tmp_name = g_strdup(icon_name);

        if(tmp_name)
            gicon = g_themed_icon_new(tmp_name);

        /* check if icon file exists */
        if(gicon != NULL) {
            GtkIconInfo *icon_info = gtk_icon_theme_lookup_by_gicon(itheme,
                                                                    gicon,
                                                                    -1,
                                                                    ITHEME_FLAGS);
            if(icon_info) {
                /* check if icon is located in pixmaps folder */
                const gchar *filename = gtk_icon_info_get_filename(icon_info);
                is_pixmaps = g_strrstr(filename, "pixmaps") ? TRUE : FALSE;

                g_object_unref(icon_info);
            } else {
                /* icon not found*/
                g_object_unref(gicon);
                gicon = NULL;
            }
        }

        g_free(tmp_name);
    }

    /* maybe it points to a file in the pixmaps folder */
    if(gicon == NULL || is_pixmaps) {
        gchar *filename = g_build_filename("pixmaps", icon_name, NULL);
        gchar *tmp_name = xfce_resource_lookup(XFCE_RESOURCE_DATA, filename);


        if(tmp_name) {
            if(gicon != NULL)
                g_object_unref(gicon);
            gicon = g_file_icon_new(g_file_new_for_path(tmp_name));
        }

        g_free(filename);
        g_free(tmp_name);
    }

    /* free key file and in-memory data */
    g_key_file_free(key_file);
    g_free(contents);
    g_free(icon_name);

    return gicon;
}

static GIcon *
xfdesktop_regular_file_icon_get_gicon(XfdesktopFileIcon *icon)
{
    XfdesktopRegularFileIcon *regular_icon = XFDESKTOP_REGULAR_FILE_ICON(icon);
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    GIcon *base_gicon = NULL;
    GIcon *gicon = NULL;

    TRACE("entering");

    /* Try to load the icon referenced in the .desktop file */
    if(xfdesktop_file_utils_is_desktop_file(regular_icon->file_info)) {
        base_gicon = xfdesktop_load_icon_from_desktop_file(regular_icon);

    } else if(g_file_info_get_file_type(regular_icon->file_info) == G_FILE_TYPE_DIRECTORY) {
        /* Try to load a thumbnail from the standard folder image locations */
        gchar *thumbnail_file = NULL;

        if(regular_icon->show_thumbnails)
            thumbnail_file = xfdesktop_load_icon_location_from_folder(file_icon);

        if(thumbnail_file) {
            /* If there's a folder thumbnail, use it */
            regular_icon->thumbnail_file = g_file_new_for_path(thumbnail_file);
            base_gicon = g_file_icon_new(regular_icon->thumbnail_file);
            g_free(thumbnail_file);
        }

    } else {
        /* If we have a thumbnail then they are enabled, use it. */
        if(regular_icon->thumbnail_file) {
            gchar *mimetype = xfdesktop_get_file_mimetype(regular_icon->file);

            /* Don't use thumbnails for svg, use the file itself */
            if(g_strcmp0(mimetype, "image/svg+xml") == 0)
                base_gicon = g_file_icon_new(regular_icon->file);
            else
                base_gicon = g_file_icon_new(regular_icon->thumbnail_file);

            g_free(mimetype);
        }
    }

    /* If we still don't have an icon, use the default */
    if(!G_IS_ICON(base_gicon)) {
        base_gicon = g_file_info_get_icon(regular_icon->file_info);
        if(G_IS_ICON(base_gicon))
            g_object_ref(base_gicon);
    }

    /* Add any user set emblems */
    gicon = xfdesktop_file_icon_add_emblems(file_icon, base_gicon);
    g_object_unref(base_gicon);

    /* load the unreadable emblem if necessary */
    if(!g_file_info_get_attribute_boolean(regular_icon->file_info,
                                          G_FILE_ATTRIBUTE_ACCESS_CAN_READ))
    {
        GIcon *themed_icon = g_themed_icon_new(EMBLEM_UNREADABLE);
        GEmblem *emblem = g_emblem_new(themed_icon);

        g_emblemed_icon_add_emblem(G_EMBLEMED_ICON(gicon), emblem);

        g_object_unref(emblem);
        g_object_unref(themed_icon);
    }
    /* load the read only emblem if necessary */
    else if(!g_file_info_get_attribute_boolean(regular_icon->file_info,
                                               G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
    {
        GIcon *themed_icon = g_themed_icon_new(EMBLEM_READONLY);
        GEmblem *emblem = g_emblem_new(themed_icon);

        g_emblemed_icon_add_emblem(G_EMBLEMED_ICON(gicon), emblem);

        g_object_unref(emblem);
        g_object_unref(themed_icon);
    }

    /* load the symlink emblem if necessary */
    if(g_file_info_get_attribute_boolean(regular_icon->file_info,
                                         G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK))
    {
        GIcon *themed_icon = g_themed_icon_new(EMBLEM_SYMLINK);
        GEmblem *emblem = g_emblem_new(themed_icon);

        g_emblemed_icon_add_emblem(G_EMBLEMED_ICON(gicon), emblem);

        g_object_unref(emblem);
        g_object_unref(themed_icon);
    }

    return gicon;
}

static const gchar *
xfdesktop_regular_file_icon_peek_label(XfdesktopIcon *icon)
{
    XfdesktopRegularFileIcon *regular_file_icon = XFDESKTOP_REGULAR_FILE_ICON(icon);

    g_return_val_if_fail(XFDESKTOP_IS_REGULAR_FILE_ICON(icon), NULL);

    return regular_file_icon->display_name;
}

static const gchar *
xfdesktop_regular_file_icon_peek_tooltip(XfdesktopIcon *icon)
{
    XfdesktopRegularFileIcon *regular_file_icon = XFDESKTOP_REGULAR_FILE_ICON(icon);

    if(!regular_file_icon->tooltip) {
        GFileInfo *info = xfdesktop_file_icon_peek_file_info(XFDESKTOP_FILE_ICON(icon));
        const gchar *content_type, *comment = NULL;
        gchar *description, *size_string, *time_string;
        guint64 size, mtime;
        gboolean is_desktop_file = FALSE;

        if(!info)
            return NULL;

        if(g_content_type_equals(g_file_info_get_content_type(info),
                                 "application/x-desktop"))
        {
            is_desktop_file = TRUE;
        }
        else
        {
          gchar *uri = g_file_get_uri(regular_file_icon->file);
          if(g_str_has_suffix(uri, ".desktop"))
              is_desktop_file = TRUE;
          g_free(uri);
        }

        content_type = g_file_info_get_content_type(info);
        description = g_content_type_get_description(content_type);

        size = g_file_info_get_attribute_uint64(info,
                                                G_FILE_ATTRIBUTE_STANDARD_SIZE);

        size_string = g_format_size(size);

        mtime = g_file_info_get_attribute_uint64(info,
                                                 G_FILE_ATTRIBUTE_TIME_MODIFIED);
        time_string = xfdesktop_file_utils_format_time_for_display(mtime);

        regular_file_icon->tooltip =
            g_strdup_printf(_("Name: %s\nType: %s\nSize: %s\nLast modified: %s"),
                            regular_file_icon->display_name,
                            description, size_string, time_string);

        /* Extract the Comment entry from the .desktop file */
        if(is_desktop_file)
        {
            gchar *path = g_file_get_path(regular_file_icon->file);
            XfceRc *rcfile = xfce_rc_simple_open(path, TRUE);
            g_free(path);

            if(rcfile) {
                xfce_rc_set_group(rcfile, "Desktop Entry");
                comment = xfce_rc_read_entry(rcfile, "Comment", NULL);
            }
            /* Prepend the comment to the tooltip */
            if(comment != NULL && *comment != '\0') {
                gchar *tooltip = regular_file_icon->tooltip;
                regular_file_icon->tooltip = g_strdup_printf("%s\n%s", comment, tooltip);
                g_free(tooltip);
            }

            if (rcfile != NULL) {
                xfce_rc_close(rcfile);
            }
        }

        g_free(time_string);
        g_free(size_string);
        g_free(description);
    }

    return regular_file_icon->tooltip;
}

static gboolean
xfdesktop_regular_file_can_write_parent(XfdesktopFileIcon *icon)
{
    XfdesktopRegularFileIcon *file_icon = XFDESKTOP_REGULAR_FILE_ICON(icon);
    GFile *parent_file;
    GFileInfo *parent_info;
    gboolean writable;

    g_return_val_if_fail(file_icon, FALSE);

    parent_file = g_file_get_parent(file_icon->file);
    if(!parent_file)
        return FALSE;

    parent_info = g_file_query_info(parent_file,
                                    XFDESKTOP_FILE_INFO_NAMESPACE,
                                    G_FILE_QUERY_INFO_NONE,
                                    NULL, NULL);
    if(!parent_info) {
        g_object_unref(parent_file);
        return FALSE;
    }

    writable = g_file_info_get_attribute_boolean(parent_info,
                                                 G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
    g_object_unref(parent_info);
    g_object_unref(parent_file);

    return writable;

}

static gboolean
xfdesktop_regular_file_icon_is_hidden_file(XfdesktopFileIcon *icon) {
    return XFDESKTOP_REGULAR_FILE_ICON(icon)->is_hidden;
}

static GFileInfo *
xfdesktop_regular_file_icon_peek_file_info(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_REGULAR_FILE_ICON(icon), NULL);
    return XFDESKTOP_REGULAR_FILE_ICON(icon)->file_info;
}

static GFileInfo *
xfdesktop_regular_file_icon_peek_filesystem_info(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_REGULAR_FILE_ICON(icon), NULL);
    return XFDESKTOP_REGULAR_FILE_ICON(icon)->filesystem_info;
}

static GFile *
xfdesktop_regular_file_icon_peek_file(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_REGULAR_FILE_ICON(icon), NULL);
    return XFDESKTOP_REGULAR_FILE_ICON(icon)->file;
}

static void
xfdesktop_regular_file_icon_update_file_info(XfdesktopFileIcon *icon,
                                             GFileInfo *info)
{
    XfdesktopRegularFileIcon *regular_file_icon = XFDESKTOP_REGULAR_FILE_ICON(icon);
    const gchar *old_display_name;
    gchar *new_display_name;

    g_return_if_fail(XFDESKTOP_IS_REGULAR_FILE_ICON(icon));
    g_return_if_fail(G_IS_FILE_INFO(info));

    /* release the old file info */
    if(regular_file_icon->file_info) {
        g_object_unref(regular_file_icon->file_info);
        regular_file_icon->file_info = NULL;
    }

    regular_file_icon->file_info = g_object_ref(info);

    if(regular_file_icon->filesystem_info)
        g_object_unref(regular_file_icon->filesystem_info);

    regular_file_icon->filesystem_info = g_file_query_filesystem_info(regular_file_icon->file,
                                                                      XFDESKTOP_FILESYSTEM_INFO_NAMESPACE,
                                                                      NULL, NULL);

    /* get both, old and new display name */
    old_display_name = regular_file_icon->display_name;
    new_display_name = xfdesktop_file_utils_get_display_name(regular_file_icon->file,
                                                             regular_file_icon->file_info);

    /* check whether the display name has changed with the info update */
    if(g_strcmp0 (old_display_name, new_display_name) != 0) {
        /* replace the display name */
        g_free (regular_file_icon->display_name);
        regular_file_icon->display_name = new_display_name;

        /* notify listeners of the label change */
        xfdesktop_icon_label_changed(XFDESKTOP_ICON(icon));
    } else {
        /* no change, release the new display name */
        g_free (new_display_name);
    }

    /* invalidate the tooltip */
    g_free(regular_file_icon->tooltip);
    regular_file_icon->tooltip = NULL;

    /* not really easy to check if this changed or not, so just invalidate it */
    xfdesktop_file_icon_invalidate_icon(XFDESKTOP_FILE_ICON(icon));
    xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(icon));
}

static void
cb_folder_contents_changed(GFileMonitor     *monitor,
                           GFile            *file,
                           GFile            *other_file,
                           GFileMonitorEvent event,
                           gpointer          user_data)
{
    XfdesktopRegularFileIcon *regular_file_icon;

    if(!user_data || !XFDESKTOP_IS_REGULAR_FILE_ICON(user_data))
        return;

    regular_file_icon = XFDESKTOP_REGULAR_FILE_ICON(user_data);

    /* not showing thumbnails */
    if(!regular_file_icon->show_thumbnails)
        return;

    gboolean reload_icon;
    switch(event) {
        case G_FILE_MONITOR_EVENT_CREATED:
        case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
        case G_FILE_MONITOR_EVENT_DELETED:
            reload_icon = is_folder_icon(file);
            break;

        case G_FILE_MONITOR_EVENT_MOVED:
        case G_FILE_MONITOR_EVENT_MOVED_IN:
        case G_FILE_MONITOR_EVENT_MOVED_OUT:
            reload_icon = is_folder_icon(file) || is_folder_icon(other_file);
            break;

        default:
            reload_icon = FALSE;
            break;
    }

    if (reload_icon) {
        gchar *thumbnail_file = xfdesktop_load_icon_location_from_folder(XFDESKTOP_FILE_ICON(regular_file_icon));
        if (thumbnail_file) {
            GFile *thumbnail = g_file_new_for_path(thumbnail_file);
            /* found a thumbnail file, apply it */
            xfdesktop_regular_file_icon_set_thumbnail_file(XFDESKTOP_ICON(regular_file_icon), thumbnail);
            g_free(thumbnail_file);
        } else {
            xfdesktop_regular_file_icon_set_thumbnail_file(XFDESKTOP_ICON(regular_file_icon), NULL);
        }
    }
}

/* if it's a .desktop file, and it has Hidden=true, or an
 * OnlyShowIn Or NotShowIn that would hide it from Xfce, don't
 * show it on the desktop (bug #4022) */
static gboolean
is_desktop_file_hidden(GFile *file) {
    gboolean is_hidden = FALSE;

    gchar *path = g_file_get_path(file);
    XfceRc *rcfile = xfce_rc_simple_open(path, TRUE);
    g_free(path);

    if (rcfile != NULL) {
        xfce_rc_set_group(rcfile, "Desktop Entry");
        if (xfce_rc_read_bool_entry(rcfile, "Hidden", FALSE)) {
            XF_DEBUG("Hidden Desktop Entry set (%s)", g_file_peek_path(file));
            is_hidden = TRUE;
        } else {
            const gchar *value = xfce_rc_read_entry(rcfile, "OnlyShowIn", NULL);
            if (value != NULL && !g_str_has_prefix(value, "XFCE;") && strstr(value, ";XFCE;") == NULL) {
                XF_DEBUG("OnlyShowIn Desktop Entry set (%s)", g_file_peek_path(file));
                is_hidden = TRUE;
            } else if ((value = xfce_rc_read_entry(rcfile, "NotShowIn", NULL)) != NULL
                       && (g_str_has_prefix(value, "XFCE;") || strstr(value, ";XFCE;") != NULL))
            {
                XF_DEBUG("NotShowIn Desktop Entry set (%s)", g_file_peek_path(file));
                is_hidden = TRUE;
            }
        }

        xfce_rc_close(rcfile);
    }

    return is_hidden;
}

static gboolean
is_file_hidden(GFile *file, GFileInfo *info) {
    if (g_file_info_get_is_hidden(info) || g_file_info_get_is_backup(info)) {
        XF_DEBUG("hidden or backup file (%s)", g_file_peek_path(file));
        return TRUE;
    } else {
        gboolean is_desktop_file = FALSE;
        if (g_content_type_equals(g_file_info_get_content_type(info), "application/x-desktop")) {
            is_desktop_file = TRUE;
        } else {
            gchar *uri = g_file_get_uri(file);
            if (g_str_has_suffix(uri, ".desktop")) {
                is_desktop_file = TRUE;
            }
            g_free(uri);
        }

        return is_desktop_file && is_desktop_file_hidden(file);
    }
}


/* public API */

XfdesktopRegularFileIcon *
xfdesktop_regular_file_icon_new(XfconfChannel *channel, GdkScreen *gdkscreen, GFile *file, GFileInfo *file_info) {
    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel), NULL);
    g_return_val_if_fail(GDK_IS_SCREEN(gdkscreen), NULL);
    g_return_val_if_fail(G_IS_FILE(file), NULL);
    g_return_val_if_fail(G_IS_FILE_INFO(file_info), NULL);

    return g_object_new(XFDESKTOP_TYPE_REGULAR_FILE_ICON,
                        "channel", channel,
                        "gdk-screen", gdkscreen,
                        "file", file,
                        "file-info", file_info,
                        NULL);
}
