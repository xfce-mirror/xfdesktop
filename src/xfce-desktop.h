/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004-2007 Brian Tarricone, <brian@tarricone.org>
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

#ifndef _XFCE_DESKTOP_H_
#define _XFCE_DESKTOP_H_

#include <gtk/gtk.h>
#include <xfconf/xfconf.h>
#include <libxfce4windowing/libxfce4windowing.h>

#include "glib-object.h"
#include "xfdesktop-backdrop-manager.h"

G_BEGIN_DECLS

#define XFCE_TYPE_DESKTOP (xfce_desktop_get_type())
G_DECLARE_FINAL_TYPE(XfceDesktop, xfce_desktop, XFCE, DESKTOP, GtkWindow)

typedef void (*SessionLogoutFunc)();

GtkWidget *xfce_desktop_new(GdkScreen *gscreen,
                            XfwMonitor *monitor,
                            XfconfChannel *channel,
                            const gchar *property_prefix,
                            XfdesktopBackdropManager *backdrop_manager);

XfwMonitor *xfce_desktop_get_monitor(XfceDesktop *desktop);
void xfce_desktop_update_monitor(XfceDesktop *desktop,
                                 XfwMonitor *monitor);

void xfce_desktop_freeze_updates(XfceDesktop *desktop);
void xfce_desktop_thaw_updates(XfceDesktop *desktop);

void xfce_desktop_set_is_active(XfceDesktop *desktop,
                                gboolean active);
gboolean xfce_desktop_is_active(XfceDesktop *desktop);

void xfce_desktop_refresh(XfceDesktop *desktop);

void xfce_desktop_cycle_backdrop(XfceDesktop *desktop);

void xfce_desktop_arrange_icons(XfceDesktop *desktop);

G_END_DECLS

#endif
