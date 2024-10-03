/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2024 Brian Tarricone, <brian@tarricone.org>
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

#ifndef __XFDESKTOP_ICON_VIEW_HOLDER_H__
#define __XFDESKTOP_ICON_VIEW_HOLDER_H__

#include <libxfce4windowing/libxfce4windowing.h>

#include "xfce-desktop.h"
#include "xfdesktop-icon-view.h"

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(XfdesktopIconViewHolder, xfdesktop_icon_view_holder, XFDESKTOP, ICON_VIEW_HOLDER, GObject)
#define XFDESKTOP_TYPE_ICON_VIEW_HOLDER (xfdesktop_icon_view_holder_get_type())

XfdesktopIconViewHolder *xfdesktop_icon_view_holder_new(XfwScreen *screen,
                                                        XfceDesktop *desktop,
                                                        XfdesktopIconView *icon_view,
                                                        GtkAccelGroup *accel_group);

XfceDesktop *xfdesktop_icon_view_holder_get_desktop(XfdesktopIconViewHolder *holder);
XfdesktopIconView *xfdesktop_icon_view_holder_get_icon_view(XfdesktopIconViewHolder *holder);

G_END_DECLS

#endif  /* __XFDESKTOP_ICON_VIEW_HOLDER_H__ */
