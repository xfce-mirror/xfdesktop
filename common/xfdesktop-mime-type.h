/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2025 The Xfce Development Team
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

#ifndef __XFDESKTOP_MIME_TYPE_H__
#define __XFDESKTOP_MIME_TYPE_H__

#include <gtk/gtk.h>

gchar *xfdesktop_get_file_mime_type(GFile *file);

void xfdesktop_media_mime_type_to_filter(GtkFileFilter *filter);

gboolean xfdesktop_file_has_media_mime_type(GFile *file);

#ifdef ENABLE_VIDEO_BACKDROP
gboolean xfdesktop_file_has_video_mime_type(GFile *file);
#endif /* ENABLE_VIDEO_BACKDROP */

#endif /* __XFDESKTOP_MIME_TYPE_H__ */
