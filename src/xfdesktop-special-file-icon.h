/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006 Brian Tarricone, <brian@tarricone.org>
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

#ifndef __XFDESKTOP_SPECIAL_FILE_ICON_H__
#define __XFDESKTOP_SPECIAL_FILE_ICON_H__

#include <glib-object.h>

#include "xfdesktop-file-icon.h"

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(XfdesktopSpecialFileIcon, xfdesktop_special_file_icon, XFDESKTOP, SPECIAL_FILE_ICON, XfdesktopFileIcon)
#define XFDESKTOP_TYPE_SPECIAL_FILE_ICON (xfdesktop_special_file_icon_get_type())

typedef enum
{
    XFDESKTOP_SPECIAL_FILE_ICON_HOME = 0,
    XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM,
    XFDESKTOP_SPECIAL_FILE_ICON_TRASH,
} XfdesktopSpecialFileIconType;

GFile *xfdesktop_special_file_icon_file_for_type(XfdesktopSpecialFileIconType type) G_GNUC_WARN_UNUSED_RESULT;

XfdesktopSpecialFileIcon *xfdesktop_special_file_icon_new(XfdesktopSpecialFileIconType type,
                                                          GdkScreen *screen);

XfdesktopSpecialFileIconType xfdesktop_special_file_icon_get_icon_type(XfdesktopSpecialFileIcon *icon);

G_END_DECLS

#endif /* __XFDESKTOP_SPECIAL_FILE_ICON_H__ */
