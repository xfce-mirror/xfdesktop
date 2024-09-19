/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004-2007,2022-2024 Brian Tarricone, <brian@tarricone.org>
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

#ifndef __XFDESKTOP_BACKDROP_MANAGER_H__
#define __XFDESKTOP_BACKDROP_MANAGER_H__

#include <gio/gio.h>
#include <gtk/gtk.h>

#include <xfconf/xfconf.h>
#include <libxfce4windowing/libxfce4windowing.h>

G_BEGIN_DECLS

/**
 * GenerateCompleteCallback:
 * @surface: (transfer full): surface, or %NULL on error
 * @surface_region: the region of the surface that applies to the selected
 *                  desktop widget, in device pixels, or %NULL on error
 * @image_file: file of the backdrop image, or %NULL on error or if there is no
 *              backdrop image
 * @error: error, or %NULL on success
 * @user_data: user data passed to function
 *
 * Only one of @surface+@surface_region+@image_filename or @error will be
 * non-%NULL.
 **/
typedef void (*GetImageSurfaceCallback)(cairo_surface_t *surface,
                                        GdkRectangle *surface_region,
                                        GFile *image_file,
                                        GError *error,
                                        gpointer user_data);

G_DECLARE_FINAL_TYPE(XfdesktopBackdropManager, xfdesktop_backdrop_manager, XFDESKTOP, BACKDROP_MANAGER, GObject)
#define XFDESKTOP_TYPE_BACKDROP_MANAGER (xfdesktop_backdrop_manager_get_type())

XfdesktopBackdropManager *xfdesktop_backdrop_manager_new(XfwScreen *screen,
                                                         XfconfChannel *channel);

void xfdesktop_backdrop_manager_get_image_surface(XfdesktopBackdropManager *manager,
                                                  GCancellable *cancellable,
                                                  XfwMonitor *monitor,
                                                  XfwWorkspace *workspace,
                                                  GetImageSurfaceCallback callback,
                                                  gpointer callback_user_data);

gboolean xfdesktop_backdrop_manager_can_cycle_backdrop(XfdesktopBackdropManager *manager,
                                                       XfwMonitor *monitor,
                                                       XfwWorkspace *workspace);

void xfdesktop_backdrop_manager_cycle_backdrop(XfdesktopBackdropManager *manager,
                                               XfwMonitor *monitor,
                                               XfwWorkspace *workspace);

void xfdesktop_backdrop_manager_invalidate(XfdesktopBackdropManager *manager,
                                           XfwMonitor *xfwmonitor,
                                           XfwWorkspace *workspace);

G_END_DECLS

#endif
