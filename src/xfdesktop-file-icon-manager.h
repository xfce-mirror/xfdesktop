/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006 Brian Tarricone, <bjt23@cornell.edu>
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
 */

#ifndef __XFDESKTOP_FILE_ICON_MANAGER_H__
#define __XFDESKTOP_FILE_ICON_MANAGER_H__

#include <glib.h>

#include "xfdesktop-icon-view.h"

G_BEGIN_DECLS

void xfdesktop_file_icon_manager_init(XfdesktopIconView *icon_view);
void xfdesktop_file_icon_manager_fini(XfdesktopIconView *icon_view);

G_END_DECLS

#endif  /* __XFDESKTOP_WINDOW_ICON_MANAGER_H__ */
