/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006      Brian Tarricone, <bjt23@cornell.edu>
 *  Copyright (c) 2010-2011 Jannis Pohlmann, <jannis@xfce.org>
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

#include <gio/gio.h>

#include <libxfce4ui/libxfce4ui.h>

#include "xfdesktop-file-utils.h"
#include "xfdesktop-file-icon.h"

static void xfdesktop_file_icon_finalize(GObject *obj);

static gboolean xfdesktop_file_icon_activated(XfdesktopIcon *icon);


G_DEFINE_ABSTRACT_TYPE(XfdesktopFileIcon, xfdesktop_file_icon,
                       XFDESKTOP_TYPE_ICON)


static void
xfdesktop_file_icon_class_init(XfdesktopFileIconClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    XfdesktopIconClass *icon_class = (XfdesktopIconClass *)klass;
    
    gobject_class->finalize = xfdesktop_file_icon_finalize;
    
    icon_class->activated = xfdesktop_file_icon_activated;
}

static void
xfdesktop_file_icon_init(XfdesktopFileIcon *icon)
{
}

static void
xfdesktop_file_icon_finalize(GObject *obj)
{
    G_OBJECT_CLASS(xfdesktop_file_icon_parent_class)->finalize(obj);
}

static gboolean
xfdesktop_file_icon_activated(XfdesktopIcon *icon)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    GFileInfo *info = xfdesktop_file_icon_peek_file_info(file_icon);
    GFile *file = xfdesktop_file_icon_peek_file(file_icon);
    GtkWidget *icon_view, *toplevel;
    GdkScreen *gscreen;
    
    TRACE("entering");

    if(!info)
        return FALSE;
    
    icon_view = xfdesktop_icon_peek_icon_view(icon);
    toplevel = gtk_widget_get_toplevel(icon_view);
    gscreen = gtk_widget_get_screen(icon_view);

    if(g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY)
        xfdesktop_file_utils_open_folder(file, gscreen, GTK_WINDOW(toplevel));
    else if(xfdesktop_file_utils_file_is_executable(info))
        xfdesktop_file_utils_execute(NULL, file, NULL, gscreen, GTK_WINDOW(toplevel));
    else
        xfdesktop_file_utils_launch(file, gscreen, GTK_WINDOW(toplevel));
    
    return TRUE;
}


GFileInfo *
xfdesktop_file_icon_peek_file_info(XfdesktopFileIcon *icon)
{
    XfdesktopFileIconClass *klass;
    
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), NULL);
    
    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    
    if(klass->peek_file_info)
       return klass->peek_file_info(icon);
    else
        return NULL;
}

GFileInfo *
xfdesktop_file_icon_peek_filesystem_info(XfdesktopFileIcon *icon)
{
    XfdesktopFileIconClass *klass;
    
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), NULL);
    
    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    
    if(klass->peek_filesystem_info)
       return klass->peek_filesystem_info(icon);
    else
        return NULL;
}

GFile *
xfdesktop_file_icon_peek_file(XfdesktopFileIcon *icon)
{
    XfdesktopFileIconClass *klass;
    
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), NULL);
    
    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    
    if(klass->peek_file)
       return klass->peek_file(icon);
    else
        return NULL;
}

void
xfdesktop_file_icon_update_file_info(XfdesktopFileIcon *icon,
                                     GFileInfo *info)
{
    XfdesktopFileIconClass *klass;
    
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON(icon));
    
    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    
    if(klass->update_file_info)
       klass->update_file_info(icon, info);
}

gboolean
xfdesktop_file_icon_can_rename_file(XfdesktopFileIcon *icon)
{
    XfdesktopFileIconClass *klass;
    
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), FALSE);
    
    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    
    if(klass->can_rename_file)
       return klass->can_rename_file(icon);
    else
        return FALSE;
}

gboolean
xfdesktop_file_icon_can_delete_file(XfdesktopFileIcon *icon)
{
    XfdesktopFileIconClass *klass;
    
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), FALSE);
    
    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    
    if(klass->can_delete_file)
       return klass->can_delete_file(icon);
    else
        return FALSE;
}
