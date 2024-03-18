/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004-2023 The Xfce Development Team
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

#ifndef __XFDESKTOP_X11_H__
#define __XFDESKTOP_X11_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _WaitForWM WaitForWM;
typedef void (*WMFoundCallback)(GApplication *app);

WaitForWM *xfdesktop_x11_wait_for_wm(GApplication *app,
                                     GCancellable *cancel,
                                     WMFoundCallback found_callback);
void xfdesktop_x11_wait_for_wm_destroy(WaitForWM *wfwm);

void xfdesktop_x11_set_compat_properties(GtkWidget *desktop);

GdkWindow *xfdesktop_x11_set_desktop_manager_selection(GdkScreen *gscreen,
                                                       GError **error);

void xfdesktop_x11_set_root_image_file_property(GdkScreen *gscreen,
                                                guint monitor_num,
                                                const gchar *filename);

void xfdesktop_x11_set_root_image_surface(GdkScreen *gscreen,
                                          cairo_surface_t *surface);

gboolean xfdesktop_x11_desktop_scrolled(GtkWidget *widget,
                                        GdkEventScroll *event);

gboolean xfdesktop_x11_get_full_workarea(GdkScreen *gscreen,
                                         GdkRectangle *workarea);

G_END_DECLS

#endif  /* __XFDESKTOP_X11_H__ */
