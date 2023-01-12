/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2022 Brian Tarricone, <brian@tarricone.org>
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

#ifndef __XFDESKTOP_CELL_RENDERER_ICON_LABEL__
#define __XFDESKTOP_CELL_RENDERER_ICON_LABEL__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_CELL_RENDERER_ICON_LABEL      (xfdesktop_cell_renderer_icon_label_get_type())
#define XFDESKTOP_CELL_RENDERER_ICON_LABEL(obj)      (G_TYPE_CHECK_INSTANCE_CAST((obj), XFDESKTOP_TYPE_CELL_RENDERER_ICON_LABEL, XfdesktopCellRendererIconLabel))
#define XFDESKTOP_IS_CELL_RENDERER_ICON_LABEL(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDESKTOP_TYPE_CELL_RENDERER_ICON_LABEL))

typedef struct _XfdesktopCellRendererIconLabel XfdesktopCellRendererIconLabel;
typedef struct _XfdesktopCellRendererIconLabelPrivate XfdesktopCellRendererIconLabelPrivate;
typedef struct _XfdesktopCellRendererIconLabelClass XfdesktopCellRendererIconLabelClass;

GType xfdesktop_cell_renderer_icon_label_get_type(void) G_GNUC_CONST;

GtkCellRenderer *xfdesktop_cell_renderer_icon_label_new(void);

G_END_DECLS

#endif  /* __XFDESKTOP_CELL_RENDERER_ICON_LABEL__ */
