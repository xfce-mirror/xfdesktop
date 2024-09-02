/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2024 Brian Tarricone, <brian@tarricone.org>
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

#ifndef __XFDESKTOP_ICON_POSITION_MIGRATION_H__
#define __XFDESKTOP_ICON_POSITION_MIGRATION_H__

#include <glib.h>
#include <libxfce4windowing/libxfce4windowing.h>
#include <xfconf/xfconf.h>

#include "xfdesktop-icon-position-configs.h"

G_BEGIN_DECLS

XfdesktopIconPositionConfig *xfdesktop_icon_positions_try_migrate(XfconfChannel *channel,
                                                                  XfwScreen *screen,
                                                                  XfwMonitor *monitor,
                                                                  XfdesktopIconPositionLevel level);

G_END_DECLS

#endif /* __XFDESKTOP_ICON_POSITION_MIGRATION_H__ */
