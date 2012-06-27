/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006 Brian Tarricone, <bjt23@cornell.edu>
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

#include <exo/exo.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include <gio/gio.h>

#include <libxfce4ui/libxfce4ui.h>

#ifdef HAVE_THUNARX
#include <thunarx/thunarx.h>
#endif

#include "xfdesktop-file-utils.h"
#include "xfdesktop-common.h"
#include "xfdesktop-regular-file-icon.h"

#define EMBLEM_SYMLINK  "emblem-symbolic-link"

struct _XfdesktopRegularFileIconPrivate
{
    GdkPixbuf *pix;
    gchar *display_name;
    gchar *tooltip;
    guint pix_opacity;
    gint cur_pix_size;
    GFileInfo *file_info;
    GFileInfo *filesystem_info;
    GFile *file;
    GFile *thumbnail_file;
    GdkScreen *gscreen;
};

static void xfdesktop_regular_file_icon_finalize(GObject *obj);

static void xfdesktop_regular_file_icon_set_thumbnail_file(XfdesktopIcon *icon, GFile *file);
static void xfdesktop_regular_file_icon_delete_thumbnail_file(XfdesktopIcon *icon);

static GdkPixbuf *xfdesktop_regular_file_icon_peek_pixbuf(XfdesktopIcon *icon,
                                                          gint size);
static G_CONST_RETURN gchar *xfdesktop_regular_file_icon_peek_label(XfdesktopIcon *icon);
static G_CONST_RETURN gchar *xfdesktop_regular_file_icon_peek_tooltip(XfdesktopIcon *icon);
static GdkDragAction xfdesktop_regular_file_icon_get_allowed_drag_actions(XfdesktopIcon *icon);
static GdkDragAction xfdesktop_regular_file_icon_get_allowed_drop_actions(XfdesktopIcon *icon);
static gboolean xfdesktop_regular_file_icon_do_drop_dest(XfdesktopIcon *icon,
                                                         XfdesktopIcon *src_icon,
                                                         GdkDragAction action);

static GFileInfo *xfdesktop_regular_file_icon_peek_file_info(XfdesktopFileIcon *icon);
static GFileInfo *xfdesktop_regular_file_icon_peek_filesystem_info(XfdesktopFileIcon *icon);
static GFile *xfdesktop_regular_file_icon_peek_file(XfdesktopFileIcon *icon);
static void xfdesktop_regular_file_icon_update_file_info(XfdesktopFileIcon *icon,
                                                         GFileInfo *info);
static gboolean xfdesktop_regular_file_can_write_parent(XfdesktopFileIcon *icon);

#ifdef HAVE_THUNARX
static void xfdesktop_regular_file_icon_tfi_init(ThunarxFileInfoIface *iface);
#endif

static inline void xfdesktop_regular_file_icon_invalidate_pixbuf(XfdesktopRegularFileIcon *icon);


#ifdef HAVE_THUNARX
G_DEFINE_TYPE_EXTENDED(XfdesktopRegularFileIcon, xfdesktop_regular_file_icon,
                       XFDESKTOP_TYPE_FILE_ICON, 0,
                       G_IMPLEMENT_INTERFACE(THUNARX_TYPE_FILE_INFO,
                                             xfdesktop_regular_file_icon_tfi_init)
                       )
#else
G_DEFINE_TYPE(XfdesktopRegularFileIcon, xfdesktop_regular_file_icon,
              XFDESKTOP_TYPE_FILE_ICON)
#endif



static void
xfdesktop_regular_file_icon_class_init(XfdesktopRegularFileIconClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    XfdesktopIconClass *icon_class = (XfdesktopIconClass *)klass;
    XfdesktopFileIconClass *file_icon_class = (XfdesktopFileIconClass *)klass;
    
    g_type_class_add_private(klass, sizeof(XfdesktopRegularFileIconPrivate));
    
    gobject_class->finalize = xfdesktop_regular_file_icon_finalize;
    
    icon_class->peek_pixbuf = xfdesktop_regular_file_icon_peek_pixbuf;
    icon_class->peek_label = xfdesktop_regular_file_icon_peek_label;
    icon_class->peek_tooltip = xfdesktop_regular_file_icon_peek_tooltip;
    icon_class->get_allowed_drag_actions = xfdesktop_regular_file_icon_get_allowed_drag_actions;
    icon_class->get_allowed_drop_actions = xfdesktop_regular_file_icon_get_allowed_drop_actions;
    icon_class->do_drop_dest = xfdesktop_regular_file_icon_do_drop_dest;
    icon_class->set_thumbnail_file = xfdesktop_regular_file_icon_set_thumbnail_file;
    icon_class->delete_thumbnail_file = xfdesktop_regular_file_icon_delete_thumbnail_file;
    
    file_icon_class->peek_file_info = xfdesktop_regular_file_icon_peek_file_info;
    file_icon_class->peek_filesystem_info = xfdesktop_regular_file_icon_peek_filesystem_info;
    file_icon_class->peek_file = xfdesktop_regular_file_icon_peek_file;
    file_icon_class->update_file_info = xfdesktop_regular_file_icon_update_file_info;
    file_icon_class->can_rename_file = xfdesktop_regular_file_can_write_parent;
    file_icon_class->can_delete_file = xfdesktop_regular_file_can_write_parent;
}

static void
xfdesktop_regular_file_icon_init(XfdesktopRegularFileIcon *icon)
{
    icon->priv = G_TYPE_INSTANCE_GET_PRIVATE(icon,
                                             XFDESKTOP_TYPE_REGULAR_FILE_ICON,
                                             XfdesktopRegularFileIconPrivate);
    icon->priv->pix_opacity = 100;
    icon->priv->display_name = NULL;
}

static void
xfdesktop_regular_file_icon_finalize(GObject *obj)
{
    XfdesktopRegularFileIcon *icon = XFDESKTOP_REGULAR_FILE_ICON(obj);
    GtkIconTheme *itheme = gtk_icon_theme_get_for_screen(icon->priv->gscreen);
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(itheme),
                                         G_CALLBACK(xfdesktop_regular_file_icon_invalidate_pixbuf),
                                         icon);

    if(icon->priv->pix)
        g_object_unref(G_OBJECT(icon->priv->pix));
    
    if(icon->priv->file_info)
        g_object_unref(icon->priv->file_info);

    if(icon->priv->filesystem_info)
        g_object_unref(icon->priv->filesystem_info);

    g_object_unref(icon->priv->file);
    
    g_free(icon->priv->display_name);
    
    if(icon->priv->tooltip)
        g_free(icon->priv->tooltip);

    if(icon->priv->thumbnail_file)
        g_object_unref(icon->priv->thumbnail_file);
    
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

static inline void
xfdesktop_regular_file_icon_invalidate_pixbuf(XfdesktopRegularFileIcon *icon)
{
    if(icon->priv->pix) {
        g_object_unref(G_OBJECT(icon->priv->pix));
        icon->priv->pix = NULL;
    }
}

static void
xfdesktop_regular_file_icon_delete_thumbnail_file(XfdesktopIcon *icon)
{
    XfdesktopRegularFileIcon *file_icon;

    if(!XFDESKTOP_IS_REGULAR_FILE_ICON(icon))
        return;

    file_icon = XFDESKTOP_REGULAR_FILE_ICON(icon);

    if(file_icon->priv->thumbnail_file) {
        g_object_unref(file_icon->priv->thumbnail_file);
        file_icon->priv->thumbnail_file = NULL;
    }

    xfdesktop_regular_file_icon_invalidate_pixbuf(file_icon);
    xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(icon));
}

static void
xfdesktop_regular_file_icon_set_thumbnail_file(XfdesktopIcon *icon, GFile *file)
{
    XfdesktopRegularFileIcon *file_icon;

    if(!XFDESKTOP_IS_REGULAR_FILE_ICON(icon))
        return;

    file_icon = XFDESKTOP_REGULAR_FILE_ICON(icon);

    if(file_icon->priv->thumbnail_file)
        g_object_unref(file_icon->priv->thumbnail_file);

    file_icon->priv->thumbnail_file = file;

    xfdesktop_regular_file_icon_invalidate_pixbuf(file_icon);
    xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(icon));
}

static GdkPixbuf *
xfdesktop_regular_file_icon_peek_pixbuf(XfdesktopIcon *icon,
                                        gint size)
{
    XfdesktopRegularFileIcon *file_icon = XFDESKTOP_REGULAR_FILE_ICON(icon);
    gchar *icon_name = NULL;
    GdkPixbuf *emblem_pix = NULL;

    if(size != file_icon->priv->cur_pix_size)
        xfdesktop_regular_file_icon_invalidate_pixbuf(file_icon);

    if(!file_icon->priv->pix) {
        GIcon *gicon = NULL;

        /* create a GFile for the $HOME/.thumbnails/ directory */
        gchar *thumbnail_dir_path = g_build_filename(xfce_get_homedir(), 
                                                     ".thumbnails", NULL);
        GFile *thumbnail_dir = g_file_new_for_path(thumbnail_dir_path);

        if(g_file_has_prefix(file_icon->priv->file, thumbnail_dir)) {
            /* use the filename as custom icon name for thumbnails */
            icon_name = g_file_get_path(file_icon->priv->file);

            /* release thumbnail path */
            g_object_unref(thumbnail_dir);
            g_free(thumbnail_dir_path);
        } else if(xfdesktop_file_utils_is_desktop_file(file_icon->priv->file_info)) {
            gchar *contents;
            gsize length;

            /* try to load the file into memory */
            if(g_file_load_contents(file_icon->priv->file, NULL, &contents, &length, 
                                    NULL, NULL)) 
            {
                /* allocate a new key file */
                GKeyFile *key_file = g_key_file_new();

                /* try to parse the key file from the contents of the file */
                if (g_key_file_load_from_data(key_file, contents, length, 0, NULL)) {
                    /* try to determine the custom icon name */
                    icon_name = g_key_file_get_string(key_file,
                                                      G_KEY_FILE_DESKTOP_GROUP,
                                                      G_KEY_FILE_DESKTOP_KEY_ICON,
                                                      NULL);
                }

                /* free key file and in-memory data */
                g_key_file_free(key_file);
                g_free(contents);
            }
        } else {
            /* If we have a thumbnail then they are enabled, use it. */
            if(file_icon->priv->thumbnail_file)
            {
                file_icon->priv->pix = gdk_pixbuf_new_from_file_at_size(g_file_get_path(file_icon->priv->thumbnail_file),
                                                                        size, size,
                                                                        NULL);
            }
        }

        /* load the symlink emblem if necessary */
        if(g_file_info_get_attribute_boolean(file_icon->priv->file_info, 
                                             G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK)) 
        {
            GtkIconTheme *itheme = gtk_icon_theme_get_default();
            gint sym_pix_size = size * 2 / 3;
            
            emblem_pix = gtk_icon_theme_load_icon(itheme, EMBLEM_SYMLINK,
                                                  sym_pix_size, ITHEME_FLAGS,
                                                  NULL);
            if(emblem_pix) {
                if(gdk_pixbuf_get_width(emblem_pix) != sym_pix_size
                   || gdk_pixbuf_get_height(emblem_pix) != sym_pix_size)
                {
                    GdkPixbuf *tmp = gdk_pixbuf_scale_simple(emblem_pix,
                                                             sym_pix_size,
                                                             sym_pix_size,
                                                             GDK_INTERP_BILINEAR);
                    g_object_unref(emblem_pix);
                    emblem_pix = tmp;
                }
            }
        }

        if(file_icon->priv->file_info)
            gicon = g_file_info_get_icon(file_icon->priv->file_info);

        if(file_icon->priv->pix) {
            if(emblem_pix) {
                gint emblem_pix_size = gdk_pixbuf_get_width(emblem_pix);
                gint dest_size = size - emblem_pix_size;

                /* We have to add the emblem */
                gdk_pixbuf_composite(emblem_pix, file_icon->priv->pix,
                                     dest_size, dest_size,
                                     emblem_pix_size, emblem_pix_size,
                                     dest_size, dest_size,
                                     1.0, 1.0, GDK_INTERP_BILINEAR, 255);
            }
        } else {
            file_icon->priv->pix = xfdesktop_file_utils_get_icon(icon_name, gicon,
                                                                 size, emblem_pix,
                                                                 file_icon->priv->pix_opacity);
        }
        
        file_icon->priv->cur_pix_size = size;

        if(emblem_pix)
             g_object_unref(emblem_pix);
        
        g_free(icon_name);
    }
    
    return file_icon->priv->pix;
}

static G_CONST_RETURN gchar *
xfdesktop_regular_file_icon_peek_label(XfdesktopIcon *icon)
{
    XfdesktopRegularFileIcon *regular_file_icon = XFDESKTOP_REGULAR_FILE_ICON(icon);

    g_return_val_if_fail(XFDESKTOP_IS_REGULAR_FILE_ICON(icon), NULL);

    return regular_file_icon->priv->display_name;
}

static GdkDragAction
xfdesktop_regular_file_icon_get_allowed_drag_actions(XfdesktopIcon *icon)
{
    GFileInfo *info = xfdesktop_file_icon_peek_file_info(XFDESKTOP_FILE_ICON(icon));
    GFile *file = xfdesktop_file_icon_peek_file(XFDESKTOP_FILE_ICON(icon));
    GdkDragAction actions = GDK_ACTION_LINK;  /* we can always link */

    if(!info)
        return 0;

    if(g_file_info_get_attribute_boolean(info,
                                         G_FILE_ATTRIBUTE_ACCESS_CAN_READ))
    {
        GFileInfo *parent_info;
        GFile *parent_file;
        
        actions |= GDK_ACTION_COPY;
        
        /* we can only move if the parent is writable */
        parent_file = g_file_get_parent(file);
        parent_info = g_file_query_info(parent_file, 
                                        XFDESKTOP_FILE_INFO_NAMESPACE,
                                        G_FILE_QUERY_INFO_NONE, 
                                        NULL, NULL);
        if(parent_info) {
            if(g_file_info_get_attribute_boolean(parent_info,
                                                 G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
            {
                actions |= GDK_ACTION_MOVE;
            }
            g_object_unref(parent_info);
        }
        g_object_unref(parent_file);
    }
    
    return actions;
}

static GdkDragAction
xfdesktop_regular_file_icon_get_allowed_drop_actions(XfdesktopIcon *icon)
{
    GFileInfo *info = xfdesktop_file_icon_peek_file_info(XFDESKTOP_FILE_ICON(icon));
    
    if(!info)
        return 0;
    
    /* if it's executable we can 'copy'.  if it's a folder we can do anything
     * if it's writable. */
    if(g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
        if(g_file_info_get_attribute_boolean(info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
            return GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK;
    } else {
        if(xfdesktop_file_utils_file_is_executable(info)) {
            return GDK_ACTION_COPY;
        }
    }

    return 0;
}

gboolean
xfdesktop_regular_file_icon_do_drop_dest(XfdesktopIcon *icon,
                                         XfdesktopIcon *src_icon,
                                         GdkDragAction action)
{
    XfdesktopRegularFileIcon *regular_file_icon = XFDESKTOP_REGULAR_FILE_ICON(icon);
    XfdesktopFileIcon *src_file_icon = XFDESKTOP_FILE_ICON(src_icon);
    GFileInfo *src_info;
    GFile *src_file;
    gboolean result = FALSE;
    
    DBG("entering");
    
    g_return_val_if_fail(regular_file_icon && src_file_icon, FALSE);
    g_return_val_if_fail(xfdesktop_regular_file_icon_get_allowed_drop_actions(icon) != 0,
                         FALSE);
    
    src_file = xfdesktop_file_icon_peek_file(src_file_icon);

    src_info = xfdesktop_file_icon_peek_file_info(src_file_icon);
    if(!src_info)
        return FALSE;
    
    if(g_file_info_get_file_type(regular_file_icon->priv->file_info) != G_FILE_TYPE_DIRECTORY
       && xfdesktop_file_utils_file_is_executable(regular_file_icon->priv->file_info))
    {
        GList files;

        files.data = src_file;
        files.prev = files.next = NULL;

        xfdesktop_file_utils_execute(NULL, regular_file_icon->priv->file, &files,
                                     regular_file_icon->priv->gscreen, NULL);

        result = TRUE;
    } else {
        GFile *parent, *dest_file = NULL;
        gchar *name;
        
        parent = g_file_get_parent(src_file);
        if(!parent)
            return FALSE;
        g_object_unref(parent);
        
        name = g_file_get_basename(src_file);
        if(!name)
            return FALSE;
        
        switch(action) {
            case GDK_ACTION_MOVE:
                dest_file = g_object_ref(regular_file_icon->priv->file);
                break;
            case GDK_ACTION_COPY:
                dest_file = g_file_get_child(regular_file_icon->priv->file, name);
                break;
            case GDK_ACTION_LINK:
                dest_file = g_object_ref(regular_file_icon->priv->file);
                break;
            default:
                g_warning("Unsupported drag action: %d", action);
        }

        if(dest_file) {
            xfdesktop_file_utils_transfer_file(action, src_file, dest_file,
                                               regular_file_icon->priv->gscreen);

            g_object_unref(dest_file);

            result = TRUE;
        }

        g_free(name);
    }
    
    return result;
}

static G_CONST_RETURN gchar *
xfdesktop_regular_file_icon_peek_tooltip(XfdesktopIcon *icon)
{
    XfdesktopRegularFileIcon *regular_file_icon = XFDESKTOP_REGULAR_FILE_ICON(icon);
    
    if(!regular_file_icon->priv->tooltip) {
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
          gchar *uri = g_file_get_uri(regular_file_icon->priv->file);
          if(g_str_has_suffix(uri, ".desktop"))
              is_desktop_file = TRUE;
          g_free(uri);
        }

        content_type = g_file_info_get_content_type(info);
        description = g_content_type_get_description(content_type);

        size = g_file_info_get_attribute_uint64(info,
                                                G_FILE_ATTRIBUTE_STANDARD_SIZE);
#if GLIB_CHECK_VERSION (2, 30, 0)
        size_string = g_format_size(size);
#else
        size_string = g_format_size_for_display(size);
#endif

        mtime = g_file_info_get_attribute_uint64(info,
                                                 G_FILE_ATTRIBUTE_TIME_MODIFIED);
        time_string = xfdesktop_file_utils_format_time_for_display(mtime);

        regular_file_icon->priv->tooltip =
            g_strdup_printf(_("Type: %s\nSize: %s\nLast modified: %s"),
                            description, size_string, time_string);

        /* Extract the Comment entry from the .desktop file */
        if(is_desktop_file)
        {
            gchar *path = g_file_get_path(regular_file_icon->priv->file);
            XfceRc *rcfile = xfce_rc_simple_open(path, TRUE);
            g_free(path);

            if(rcfile) {
                xfce_rc_set_group(rcfile, "Desktop Entry");
                comment = xfce_rc_read_entry(rcfile, "Comment", NULL);
            }
            /* Prepend the comment to the tooltip */
            if(comment != NULL) {
                gchar *tooltip = regular_file_icon->priv->tooltip;
                regular_file_icon->priv->tooltip = g_strdup_printf("%s\n%s",
                                                                   comment,
                                                                   tooltip);
                g_free(tooltip);
            }

            xfce_rc_close(rcfile);
        }

        g_free(time_string);
        g_free(size_string);
        g_free(description);
    }
    
    return regular_file_icon->priv->tooltip;
}

static gboolean
xfdesktop_regular_file_can_write_parent(XfdesktopFileIcon *icon)
{
    XfdesktopRegularFileIcon *file_icon = XFDESKTOP_REGULAR_FILE_ICON(icon);
    GFile *parent_file;
    GFileInfo *parent_info;
    gboolean writable;

    g_return_val_if_fail(file_icon, FALSE);

    parent_file = g_file_get_parent(file_icon->priv->file);
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

static GFileInfo *
xfdesktop_regular_file_icon_peek_file_info(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_REGULAR_FILE_ICON(icon), NULL);
    return XFDESKTOP_REGULAR_FILE_ICON(icon)->priv->file_info;
}

static GFileInfo *
xfdesktop_regular_file_icon_peek_filesystem_info(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_REGULAR_FILE_ICON(icon), NULL);
    return XFDESKTOP_REGULAR_FILE_ICON(icon)->priv->filesystem_info;
}

static GFile *
xfdesktop_regular_file_icon_peek_file(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_REGULAR_FILE_ICON(icon), NULL);
    return XFDESKTOP_REGULAR_FILE_ICON(icon)->priv->file;
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
    if(regular_file_icon->priv->file_info) { 
        g_object_unref(regular_file_icon->priv->file_info);
        regular_file_icon->priv->file_info = NULL;
    }

    regular_file_icon->priv->file_info = g_object_ref(info);

    if(regular_file_icon->priv->filesystem_info)
        g_object_unref(regular_file_icon->priv->filesystem_info);

    regular_file_icon->priv->filesystem_info = g_file_query_filesystem_info(regular_file_icon->priv->file,
                                                                            XFDESKTOP_FILESYSTEM_INFO_NAMESPACE,
                                                                            NULL, NULL);

    /* get both, old and new display name */
    old_display_name = regular_file_icon->priv->display_name;
    new_display_name = xfdesktop_file_utils_get_display_name(regular_file_icon->priv->file,
                                                             regular_file_icon->priv->file_info);

    /* check whether the display name has changed with the info update */
    if(g_strcmp0 (old_display_name, new_display_name) != 0) {
        /* replace the display name */
        g_free (regular_file_icon->priv->display_name);
        regular_file_icon->priv->display_name = new_display_name;

        /* notify listeners of the label change */
        xfdesktop_icon_label_changed(XFDESKTOP_ICON(icon));
    } else {
        /* no change, release the new display name */
        g_free (new_display_name);
    }

    /* invalidate the tooltip */
    g_free(regular_file_icon->priv->tooltip);
    regular_file_icon->priv->tooltip = NULL;
    
    /* not really easy to check if this changed or not, so just invalidate it */
    xfdesktop_regular_file_icon_invalidate_pixbuf(regular_file_icon);
    xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(icon));
}


/* public API */

XfdesktopRegularFileIcon *
xfdesktop_regular_file_icon_new(GFile *file,
                                GFileInfo *file_info,
                                GdkScreen *screen)
{
    XfdesktopRegularFileIcon *regular_file_icon;

    g_return_val_if_fail(G_IS_FILE(file), NULL);
    g_return_val_if_fail(G_IS_FILE_INFO(file_info), NULL);
    g_return_val_if_fail(GDK_IS_SCREEN(screen), NULL);

    regular_file_icon = g_object_new(XFDESKTOP_TYPE_REGULAR_FILE_ICON, NULL);

    regular_file_icon->priv->file = g_object_ref(file);
    regular_file_icon->priv->file_info = g_object_ref(file_info);

    /* set the display name */
    regular_file_icon->priv->display_name = xfdesktop_file_utils_get_display_name(file, 
                                                                                  file_info);

    /* query file system information from GIO */
    regular_file_icon->priv->filesystem_info = g_file_query_filesystem_info(regular_file_icon->priv->file,
                                                                            XFDESKTOP_FILESYSTEM_INFO_NAMESPACE,
                                                                            NULL, NULL);

    /* query file information from GIO */
    regular_file_icon->priv->file_info = g_file_query_info(regular_file_icon->priv->file,
                                                           XFDESKTOP_FILE_INFO_NAMESPACE,
                                                           G_FILE_QUERY_INFO_NONE,
                                                           NULL, NULL);

    regular_file_icon->priv->gscreen = screen;

    g_signal_connect_swapped(G_OBJECT(gtk_icon_theme_get_for_screen(screen)),
                             "changed",
                             G_CALLBACK(xfdesktop_regular_file_icon_invalidate_pixbuf),
                             regular_file_icon);
    
    return regular_file_icon;
}

void
xfdesktop_regular_file_icon_set_pixbuf_opacity(XfdesktopRegularFileIcon *icon,
                                       guint opacity)
{
    g_return_if_fail(XFDESKTOP_IS_REGULAR_FILE_ICON(icon) && opacity <= 100);
    
    if(opacity == icon->priv->pix_opacity)
        return;
    
    icon->priv->pix_opacity = opacity;
    
    xfdesktop_regular_file_icon_invalidate_pixbuf(icon);
    xfdesktop_icon_pixbuf_changed(XFDESKTOP_ICON(icon));
}
