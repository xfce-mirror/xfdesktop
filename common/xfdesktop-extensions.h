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

#ifndef __XFDESTKOP_EXTENSIONS_H__
#define __XFDESTKOP_EXTENSIONS_H__

#include <glib.h>

G_BEGIN_DECLS

GList *xfdesktop_g_list_last(GList *list,
                             guint *length);
GList *xfdesktop_g_list_append(GList *list,
                               gpointer data,
                               GList **new_link,
                               guint *new_length);

G_END_DECLS

#endif  /* __XFDESTKOP_EXTENSIONS_H__ */
