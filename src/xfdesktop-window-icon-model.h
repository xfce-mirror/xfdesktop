/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2022 Brian Tarricone, <brian@tarricone.org>
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

#ifndef __XFDESKTOP_WINDOW_ICON_MODEL_H__
#define __XFDESKTOP_WINDOW_ICON_MODEL_H__

#include <gtk/gtk.h>
#include <libxfce4windowing/libxfce4windowing.h>

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_WINDOW_ICON_MODEL    (xfdesktop_window_icon_model_get_type())
#define XFDESKTOP_WINDOW_ICON_MODEL(obj)    (G_TYPE_CHECK_INSTANCE_CAST((obj), XFDESKTOP_TYPE_WINDOW_ICON_MODEL, XfdesktopWindowIconModel))
#define XFDESKTOP_IS_WINDOW_ICON_MODEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDESKTOP_TYPE_WINDOW_ICON_MODEL))

typedef struct _XfdesktopWindowIconModel XfdesktopWindowIconModel;
typedef struct _XfdesktopWindowIconModelClass XfdesktopWindowIconModelClass;

GType xfdesktop_window_icon_model_get_type(void) G_GNUC_CONST;

XfdesktopWindowIconModel *xfdesktop_window_icon_model_new(XfwScreen *screen);

void xfdesktop_window_icon_model_changed(XfdesktopWindowIconModel *wmodel,
                                         XfwWindow *window);

void xfdesktop_window_icon_model_set_position(XfdesktopWindowIconModel *wmodel,
                                              GtkTreeIter *iter,
                                              gint row,
                                              gint col);

XfwWindow *xfdesktop_window_icon_model_get_window(XfdesktopWindowIconModel *wmodel,
                                                  GtkTreeIter *iter);
gboolean xfdesktop_window_icon_model_get_window_iter(XfdesktopWindowIconModel *wmodel,
                                                     XfwWindow *window,
                                                     GtkTreeIter *iter);

G_END_DECLS

#endif  /* __XFDESKTOP_WINDOW_ICON_MODEL_H__ */
