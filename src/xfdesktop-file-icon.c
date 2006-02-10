/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006 Brian Tarricone, <bjt23@cornell.edu>
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

#include <libxfcegui4/libxfcegui4.h>

#include "xfdesktop-icon.h"
#include "xfdesktop-file-icon.h"

struct _XfdesktopFileIconPrivate
{
    gint16 row;
    gint16 col;
    GdkPixbuf *pix;
    gint cur_pix_size;
    gchar *label;
    GdkRectangle extents;
    ThunarVfsPath *path;
    GdkScreen *gscreen;
};

static void xfdesktop_file_icon_icon_init(XfdesktopIconIface *iface);
static void xfdesktop_file_icon_finalize(GObject *obj);

static GdkPixbuf *xfdesktop_file_icon_peek_pixbuf(XfdesktopIcon *icon,
                                                  gint size);
static G_CONST_RETURN gchar *xfdesktop_file_icon_peek_label(XfdesktopIcon *icon);

static void xfdesktop_file_icon_set_position(XfdesktopIcon *icon,
                                             gint16 row,
                                             gint16 col);
static gboolean xfdesktop_file_icon_get_position(XfdesktopIcon *icon,
                                                 gint16 *row,
                                                 gint16 *col);

static void xfdesktop_file_icon_set_extents(XfdesktopIcon *icon,
                                            const GdkRectangle *extents);
static gboolean xfdesktop_file_icon_get_extents(XfdesktopIcon *icon,
                                                GdkRectangle *extents);

static void xfdesktop_file_icon_selected(XfdesktopIcon *icon);
static void xfdesktop_file_icon_activated(XfdesktopIcon *icon);
static void xfdesktop_file_icon_menu_popup(XfdesktopIcon *icon);


static ThunarVfsMimeInfo *xfdesktop_file_icon_get_mime_info(XfdesktopFileIcon *icon);
static GdkPixbuf *xfdesktop_fallback_icon = NULL;


G_DEFINE_TYPE_EXTENDED(XfdesktopFileIcon, xfdesktop_file_icon,
                       G_TYPE_OBJECT, 0,
                       G_IMPLEMENT_INTERFACE(XFDESKTOP_TYPE_ICON,
                                             xfdesktop_file_icon_icon_init))


/* FIXME: memleak */
static ThunarVfsMimeDatabase *thunar_mime_database = NULL;


static void
xfdesktop_file_icon_class_init(XfdesktopFileIconClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    
    gobject_class->finalize = xfdesktop_file_icon_finalize;
}

static void
xfdesktop_file_icon_init(XfdesktopFileIcon *icon)
{
    icon->priv = g_new0(XfdesktopFileIconPrivate, 1);
}

static void
xfdesktop_file_icon_finalize(GObject *obj)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(obj);
    
    if(icon->priv->pix)
        g_object_unref(G_OBJECT(icon->priv->pix));
    
    g_free(icon->priv->label);
    
    if(icon->priv->path)
        thunar_vfs_path_unref(icon->priv->path);
    
    g_free(icon->priv);
    
    G_OBJECT_CLASS(xfdesktop_file_icon_parent_class)->finalize(obj);
}

static void
xfdesktop_file_icon_icon_init(XfdesktopIconIface *iface)
{
    iface->peek_pixbuf = xfdesktop_file_icon_peek_pixbuf;
    iface->peek_label = xfdesktop_file_icon_peek_label;
    iface->set_position = xfdesktop_file_icon_set_position;
    iface->get_position = xfdesktop_file_icon_get_position;
    iface->set_extents = xfdesktop_file_icon_set_extents;
    iface->get_extents = xfdesktop_file_icon_get_extents;
    iface->selected = xfdesktop_file_icon_selected;
    iface->activated = xfdesktop_file_icon_activated;
    iface->menu_popup = xfdesktop_file_icon_menu_popup;
}


XfdesktopFileIcon *
xfdesktop_file_icon_new(ThunarVfsPath *path,
                        GdkScreen *screen)
{
    XfdesktopFileIcon *file_icon = g_object_new(XFDESKTOP_TYPE_FILE_ICON, NULL);
    file_icon->priv->path = thunar_vfs_path_ref(path);
    file_icon->priv->gscreen = screen;
    
    return file_icon;
}


static GdkPixbuf *
xfdesktop_file_icon_peek_pixbuf(XfdesktopIcon *icon,
                                gint size)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    
    if(!file_icon->priv->pix || size != file_icon->priv->cur_pix_size) {
        ThunarVfsInfo *info;
        
        if(file_icon->priv->pix) {
            g_object_unref(G_OBJECT(file_icon->priv->pix));
            file_icon->priv->pix = NULL;
        }
        
        info = thunar_vfs_info_new_for_path(file_icon->priv->path, NULL);
        
        if(info) {
            if(info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY) {
                file_icon->priv->pix = xfce_themed_icon_load("stock_folder", size);
                if(file_icon->priv->pix)
                    file_icon->priv->cur_pix_size = size;
            } else {
                const gchar *custom_icon = thunar_vfs_info_get_custom_icon(info);
                
                if(custom_icon) {
                    file_icon->priv->pix = xfce_themed_icon_load(custom_icon, size);
                    if(file_icon->priv->pix)
                        file_icon->priv->cur_pix_size = size;
                }
                
                if(!file_icon->priv->pix) {
                    ThunarVfsMimeInfo *mime_info;
                    
                    mime_info = xfdesktop_file_icon_get_mime_info(file_icon);
                    if(mime_info) {
                        /* FIXME: GtkIconTheme/XfceIconTheme */
                        const gchar *icon_name = thunar_vfs_mime_info_lookup_icon_name(mime_info,
                                                                                       gtk_icon_theme_get_default());
                        
                        if(icon_name) {
                            file_icon->priv->pix = xfce_themed_icon_load(icon_name, size);
                            if(file_icon->priv->pix)
                                file_icon->priv->cur_pix_size = size;
                        }
                        
                        thunar_vfs_mime_info_unref(mime_info);
                    }
                }
            }
            
            thunar_vfs_info_unref(info);
        }
    }
    
    /* fallback */
    if(!file_icon->priv->pix) {
        if(xfdesktop_fallback_icon) {
            if(gdk_pixbuf_get_width(xfdesktop_fallback_icon) != size) {
                g_object_unref(G_OBJECT(xfdesktop_fallback_icon));
                xfdesktop_fallback_icon = NULL;
            }
        }
        if(!xfdesktop_fallback_icon) {
            xfdesktop_fallback_icon = xfce_pixbuf_new_from_file_at_size(DATADIR "/pixmaps/xfdesktop/xfdesktop-fallback-icon.png",
                                                                        size,
                                                                        size,
                                                                        NULL);
        }
        
        file_icon->priv->pix = g_object_ref(G_OBJECT(xfdesktop_fallback_icon));
        file_icon->priv->cur_pix_size = size;
    }
    
    return file_icon->priv->pix;
}

static G_CONST_RETURN gchar *
xfdesktop_file_icon_peek_label(XfdesktopIcon *icon)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    
    if(!file_icon->priv->label) {
        const char *name = thunar_vfs_path_get_name(file_icon->priv->path);
        if(name)
            file_icon->priv->label = g_filename_to_utf8(name, -1, NULL,
                                                        NULL, NULL);
    }
    
    return file_icon->priv->label;
}

static void
xfdesktop_file_icon_set_position(XfdesktopIcon *icon,
                                 gint16 row,
                                 gint16 col)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    
    file_icon->priv->row = row;
    file_icon->priv->col = col;
}

static gboolean
xfdesktop_file_icon_get_position(XfdesktopIcon *icon,
                                 gint16 *row,
                                 gint16 *col)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    
    *row = file_icon->priv->row;
    *col = file_icon->priv->col;
    
    return TRUE;
}

static void
xfdesktop_file_icon_set_extents(XfdesktopIcon *icon,
                                const GdkRectangle *extents)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    
    memcpy(&file_icon->priv->extents, extents, sizeof(GdkRectangle));
}

static gboolean
xfdesktop_file_icon_get_extents(XfdesktopIcon *icon,
                                GdkRectangle *extents)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    
    if(file_icon->priv->extents.width > 0
       && file_icon->priv->extents.height > 0)
    {
        memcpy(extents, &file_icon->priv->extents, sizeof(GdkRectangle));
        return TRUE;
    }
    
    return FALSE;
}

static void
xfdesktop_file_icon_selected(XfdesktopIcon *icon)
{
    
}

static void
xfdesktop_file_icon_activated(XfdesktopIcon *icon)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    ThunarVfsMimeInfo *mime_info;
    ThunarVfsMimeApplication *mime_app;
    gboolean succeeded = FALSE;
    GList *path_list = g_list_prepend(NULL, file_icon->priv->path);
    
    TRACE("entering");
    
    mime_info = xfdesktop_file_icon_get_mime_info(file_icon);
    if(mime_info) {
        mime_app = thunar_vfs_mime_database_get_default_application(thunar_mime_database,
                                                                    mime_info);
        if(mime_app) {
            DBG("executing");
            
            succeeded = thunar_vfs_mime_handler_exec(THUNAR_VFS_MIME_HANDLER(mime_app),
                                                     file_icon->priv->gscreen,
                                                     path_list,
                                                     NULL);            
            g_object_unref(G_OBJECT(mime_app));
        }
    
        thunar_vfs_mime_info_unref(mime_info);
    }
    
    if(!succeeded) {
        ThunarVfsInfo *info = thunar_vfs_info_new_for_path(file_icon->priv->path,
                                                           NULL);
        
        if(info) {
            if(info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY) {
                gchar *thunar_app = g_find_program_in_path("Thunar");
                
                if(thunar_app) {
                    gchar *folder_name = thunar_vfs_path_dup_string(file_icon->priv->path);
                    gchar *commandline = g_strconcat(thunar_app, " \"", folder_name,
                                                     "\"", NULL);
                    
                    DBG("executing:\n%s\n", commandline);
                    
                    succeeded = xfce_exec(commandline, FALSE, TRUE, NULL);
                    g_free(folder_name);
                    g_free(commandline);
                }
                g_free(thunar_app);
            } else {
                succeeded = thunar_vfs_info_execute(info,
                                                    file_icon->priv->gscreen,
                                                    path_list,
                                                    NULL);
            }
            
            thunar_vfs_info_unref(info);
        }
    }
    
    g_list_free(path_list);
}

static void
xfdesktop_file_icon_menu_popup(XfdesktopIcon *icon)
{
    
}



static ThunarVfsMimeInfo *
xfdesktop_file_icon_get_mime_info(XfdesktopFileIcon *icon)
{
    ThunarVfsMimeInfo *mime_info = NULL;
    gchar *file_path;
    
    if(!thunar_mime_database)
        thunar_mime_database = thunar_vfs_mime_database_get_default();
    
    if(!icon->priv->label)
        xfdesktop_file_icon_peek_label(XFDESKTOP_ICON(icon));
    
    file_path = thunar_vfs_path_dup_string(icon->priv->path);
    mime_info = thunar_vfs_mime_database_get_info_for_file(thunar_mime_database,
                                                           file_path,
                                                           icon->priv->label);
    g_free(file_path);
    
    return mime_info;
}
