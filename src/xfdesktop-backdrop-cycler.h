/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004 Brian Tarricone, <brian@tarricone.org>
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
 *
 *  Random portions taken from or inspired by the original xfdesktop for xfce4:
 *     Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *     Copyright (C) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
 */

#ifndef __XFDESKTOP_BACKDROP_CYCLER_H__
#define __XFDESKTOP_BACKDROP_CYCLER_H__

#include <gio/gio.h>

#include <xfconf/xfconf.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(XfdesktopBackdropCycler, xfdesktop_backdrop_cycler, XFDESKTOP, BACKDROP_CYCLER, GObject)
#define XFDESKTOP_TYPE_BACKDROP_CYCLER (xfdesktop_backdrop_cycler_get_type())

XfdesktopBackdropCycler *xfdesktop_backdrop_cycler_new(XfconfChannel *channel,
                                                       const gchar *property_prefix,
                                                       GFile *image_file);

const gchar *xfdesktop_backdrop_cycler_get_property_prefix(XfdesktopBackdropCycler *cycler);

gboolean xfdesktop_backdrop_cycler_is_enabled(XfdesktopBackdropCycler *cycler);

void xfdesktop_backdrop_cycler_cycle_backdrop(XfdesktopBackdropCycler *cycler);

G_END_DECLS

#endif  /* __XFDESKTOP_BACKDROP_CYCLER_H__ */
