/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004 Brian Tarricone, <bjt23@cornell.edu>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  Random portions taken from or inspired by the original xfdesktop for xfce4:
 *     Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *     Copyright (C) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
 */

#ifndef _XFDESKTOP_COMMON_H_
#define _XFDESKTOP_COMMON_H_

#include <glib.h>

#define BACKDROP_CHANNEL         "BACKDROP"
#define DEFAULT_BACKDROP         DATADIR "/xfce4/backdrops/xfce-stripes.png"
#define LIST_TEXT                "# xfce backdrop list"
#define XFDESKTOP_SELECTION_FMT  "XFDESKTOP_SELECTION_%d"
#define XFDESKTOP_IMAGE_FILE_FMT "XFDESKTOP_IMAGE_FILE_%d"

G_BEGIN_DECLS

typedef enum
{
	XFCE_BACKDROP_IMAGE_AUTO = 0,
	XFCE_BACKDROP_IMAGE_CENTERED,
	XFCE_BACKDROP_IMAGE_TILED,
	XFCE_BACKDROP_IMAGE_STRETCHED,
	XFCE_BACKDROP_IMAGE_SCALED
} XfceBackdropImageStyle;

typedef enum
{
	XFCE_BACKDROP_COLOR_SOLID = 0,
	XFCE_BACKDROP_COLOR_HORIZ_GRADIENT,
	XFCE_BACKDROP_COLOR_VERT_GRADIENT
} XfceBackdropColorStyle;

gchar **get_list_from_file(const gchar *);
gboolean is_backdrop_list(const gchar *path);
gboolean xfdesktop_check_image_file(const gchar *filename);
gchar *desktop_menu_file_get_menufile();

G_END_DECLS

#endif
