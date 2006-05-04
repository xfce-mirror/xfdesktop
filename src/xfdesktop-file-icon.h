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

#ifndef __XFDESKTOP_FILE_ICON_H__
#define __XFDESKTOP_FILE_ICON_H__

#include <glib-object.h>

#define EXO_API_SUBJECT_TO_CHANGE
#include <thunar-vfs/thunar-vfs.h>

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_FILE_ICON     (xfdesktop_file_icon_get_type())
#define XFDESKTOP_FILE_ICON(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), XFDESKTOP_TYPE_FILE_ICON, XfdesktopFileIcon))
#define XFDESKTOP_IS_FILE_ICON(obj)  (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDESKTOP_TYPE_FILE_ICON))

typedef struct _XfdesktopFileIcon         XfdesktopFileIcon;
typedef struct _XfdesktopFileIconClass    XfdesktopFileIconClass;
typedef struct _XfdesktopFileIconPrivate  XfdesktopFileIconPrivate;

struct _XfdesktopFileIcon
{
    GObject parent;
    
    /*< private >*/
    XfdesktopFileIconPrivate *priv;
};

struct _XfdesktopFileIconClass
{
    GObjectClass parent;
    
    /*< signals >*/
    void (*position_changed)(XfdesktopFileIcon *icon);
};

GType xfdesktop_file_icon_get_type() G_GNUC_CONST;

XfdesktopFileIcon *xfdesktop_file_icon_new(ThunarVfsInfo *info,
                                           GdkScreen *screen);
XfdesktopFileIcon *xfdesktop_file_icon_new_for_volume(ThunarVfsVolume *volume,
                                                      GdkScreen *screen);

void xfdesktop_file_icon_set_pixbuf_opacity(XfdesktopFileIcon *icon,
                                            guint opacity);

G_CONST_RETURN ThunarVfsInfo *xfdesktop_file_icon_peek_info(XfdesktopFileIcon *icon);
void xfdesktop_file_icon_update_info(XfdesktopFileIcon *icon,
                                     ThunarVfsInfo *info);

void xfdesktop_file_icon_delete_file(XfdesktopFileIcon *icon);
gboolean xfdesktop_file_icon_rename_file(XfdesktopFileIcon *icon, 
                                         const gchar *new_name);

ThunarVfsVolume *xfdesktop_file_icon_peek_volume(XfdesktopFileIcon *icon);

/* utility function for the clipboard manager */
GList *xfdesktop_file_icon_list_to_path_list(GList *icon_list);

G_END_DECLS

#endif /* __XFDESKTOP_FILE_ICON_H__ */
