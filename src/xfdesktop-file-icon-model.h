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

#ifndef __XFDESKTOP_FILE_ICON_MODEL_H__
#define __XFDESKTOP_FILE_ICON_MODEL_H__

#include <gtk/gtk.h>

#include "xfdesktop-file-icon.h"

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_FILE_ICON_MODEL    (xfdesktop_file_icon_model_get_type())
#define XFDESKTOP_FILE_ICON_MODEL(obj)    (G_TYPE_CHECK_INSTANCE_CAST((obj), XFDESKTOP_TYPE_FILE_ICON_MODEL, XfdesktopFileIconModel))
#define XFDESKTOP_IS_FILE_ICON_MODEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDESKTOP_TYPE_FILE_ICON_MODEL))

typedef enum
{
    XFDESKTOP_FILE_ICON_MODEL_COLUMN_SURFACE,
    XFDESKTOP_FILE_ICON_MODEL_COLUMN_LABEL,
    XFDESKTOP_FILE_ICON_MODEL_COLUMN_ROW,
    XFDESKTOP_FILE_ICON_MODEL_COLUMN_COL,
    XFDESKTOP_FILE_ICON_MODEL_COLUMN_SORT_PRIORITY,
    XFDESKTOP_FILE_ICON_MODEL_COLUMN_TOOLTIP_SURFACE,
    XFDESKTOP_FILE_ICON_MODEL_COLUMN_TOOLTIP_TEXT,

    XFDESKTOP_FILE_ICON_MODEL_COLUMN_N_COLUMNS,
} XfdesktopFileIconModelColumn;

typedef struct _XfdesktopFileIconModel XfdesktopFileIconModel;
typedef struct _XfdesktopFileIconModelClass XfdesktopFileIconModelClass;

GType xfdesktop_file_icon_model_get_type(void) G_GNUC_CONST;

XfdesktopFileIconModel *xfdesktop_file_icon_model_new(void);

void xfdesktop_file_icon_model_set_icon_width(XfdesktopFileIconModel *fmodel,
                                              gint width);
void xfdesktop_file_icon_model_set_icon_height(XfdesktopFileIconModel *fmodel,
                                               gint height);
void xfdesktop_file_icon_model_set_tooltip_icon_size(XfdesktopFileIconModel *fmodel,
                                                     gint size);
void xfdesktop_file_icon_model_set_scale_factor(XfdesktopFileIconModel *fmodel,
                                                gint scale_factor);

void xfdesktop_file_icon_model_append(XfdesktopFileIconModel *fmodel,
                                      XfdesktopFileIcon *icon,
                                      GtkTreeIter *iter);
void xfdesktop_file_icon_model_remove(XfdesktopFileIconModel *fmodel,
                                      XfdesktopFileIcon *icon);
void xfdesktop_file_icon_model_changed(XfdesktopFileIconModel *fmodel,
                                       XfdesktopFileIcon *icon);

XfdesktopFileIcon *xfdesktop_file_icon_model_get_icon(XfdesktopFileIconModel *fmodel,
                                                      GtkTreeIter *iter);

void xfdesktop_file_icon_model_clear(XfdesktopFileIconModel *fmodel);

G_END_DECLS

#endif  /* __XFDESKTOP_FILE_ICON_MODEL_H__ */
