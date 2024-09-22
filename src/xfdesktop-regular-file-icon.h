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

#ifndef __XFDESKTOP_REGULAR_FILE_ICON_H__
#define __XFDESKTOP_REGULAR_FILE_ICON_H__

#include <gdk/gdk.h>
#include <xfconf/xfconf.h>

#include "xfdesktop-file-icon.h"

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(XfdesktopRegularFileIcon, xfdesktop_regular_file_icon, XFDESKTOP, REGULAR_FILE_ICON, XfdesktopFileIcon)
#define XFDESKTOP_TYPE_REGULAR_FILE_ICON (xfdesktop_regular_file_icon_get_type())

XfdesktopRegularFileIcon *xfdesktop_regular_file_icon_new(XfconfChannel *channel,
                                                          GdkScreen *gdkscreen,
                                                          GFile *file,
                                                          GFileInfo *file_info);

G_END_DECLS

#endif /* __XFDESKTOP_REGULAR_FILE_ICON_H__ */
