/*
 *  Copyright (C) 2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *  Copyright (C) 2003 Benedikt Meurer (benedikt.meurer@unix-ag.uni-siegen.de)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef __XFDESKTOP_BACKDROP_H__
#define __XFDESKTOP_BACKDROP_H__

#include <glib.h>

#define BACKDROP_CHANNEL    "BACKDROP"

#define LIST_TEXT           "# xfce backdrop list"

/* styles */
typedef enum
{
    TILED,
    CENTERED,
    SCALED,
    STRETCHED,
    AUTO,
    NONE
}
XfceBackgroundStyle;

extern gchar **get_list_from_file (const gchar *);

extern gboolean is_backdrop_list (const char *path);

#endif /* !__XFDESKTOP_BACKDROP_H__ */
