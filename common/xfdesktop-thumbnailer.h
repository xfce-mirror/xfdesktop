/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006 Brian Tarricone, <brian@tarricone.org>
 *  Copyright(c) 2006 Benedikt Meurer, <benny@xfce.org>
 *  Copyright(c) 2010-2011 Jannis Pohlmann, <jannis@xfce.org>
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
 *  xfdesktop-thumbnailer is based on thumbnailer code from Ristretto
 *  Copyright (c) Stephan Arts 2009-2011 <stephan@xfce.org>
 */

#ifndef __XFDESKTOP_THUMBNAILER_H__
#define __XFDESKTOP_THUMBNAILER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_THUMBNAILER (xfdesktop_thumbnailer_get_type())
G_DECLARE_FINAL_TYPE(XfdesktopThumbnailer, xfdesktop_thumbnailer, XFDESKTOP, THUMBNAILER, GObject)

XfdesktopThumbnailer *xfdesktop_thumbnailer_new(void);

gboolean xfdesktop_thumbnailer_service_available(XfdesktopThumbnailer *thumbnailer);

gboolean xfdesktop_thumbnailer_is_supported(XfdesktopThumbnailer *thumbnailer,
                                            gchar *file);

gboolean xfdesktop_thumbnailer_queue_thumbnail(XfdesktopThumbnailer *thumbnailer,
                                               gchar *file);
void xfdesktop_thumbnailer_dequeue_thumbnail(XfdesktopThumbnailer *thumbnailer,
                                             gchar *file);
void xfdesktop_thumbnailer_dequeue_all_thumbnails(XfdesktopThumbnailer *thumbnailer);

void xfdesktop_thumbnailer_delete_thumbnail(XfdesktopThumbnailer *thumbnailer,
                                            gchar *src_file);

G_END_DECLS

#endif /* __XFDESKTOP_THUMBNAILER_H__ */
