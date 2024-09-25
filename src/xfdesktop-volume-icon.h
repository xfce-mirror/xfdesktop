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

#ifndef __XFDESKTOP_VOLUME_ICON_H__
#define __XFDESKTOP_VOLUME_ICON_H__

#include <gio/gio.h>

#include "xfdesktop-file-icon.h"

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(XfdesktopVolumeIcon, xfdesktop_volume_icon, XFDESKTOP, VOLUME_ICON, XfdesktopFileIcon)
#define XFDESKTOP_TYPE_VOLUME_ICON (xfdesktop_volume_icon_get_type())

XfdesktopVolumeIcon *xfdesktop_volume_icon_new_for_volume(GVolume *volume,
                                                          GdkScreen *screen);
XfdesktopVolumeIcon *xfdesktop_volume_icon_new_for_mount(GMount *mount,
                                                         GdkScreen *screen);

GVolume *xfdesktop_volume_icon_peek_volume(XfdesktopVolumeIcon *icon);
GMount *xfdesktop_volume_icon_peek_mount(XfdesktopVolumeIcon *icon);

void xfdesktop_volume_icon_mounted(XfdesktopVolumeIcon *icon,
                                   GMount *mount);
void xfdesktop_volume_icon_unmounted(XfdesktopVolumeIcon *icon);

gchar *xfdesktop_volume_icon_sort_key_for_volume(GVolume *volume);
gchar *xfdesktop_volume_icon_sort_key_for_mount(GMount *mount);

G_END_DECLS

#endif /* __XFDESKTOP_VOLUME_ICON_H__ */
