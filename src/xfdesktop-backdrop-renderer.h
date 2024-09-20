/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004,2024 Brian Tarricone, <brian@tarricone.org>
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
 *  Random portions taken from or inspired by the original xfdesktop for xfce4:
 *     Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *     Copyright (C) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
 */

#ifndef __XFDESKTOP_BACKDROP_RENDERER_H__
#define __XFDESKTOP_BACKDROP_RENDERER_H__

#include <gdk/gdk.h>

#include "xfdesktop-common.h"

G_BEGIN_DECLS

/**
 * RenderCompleteCallback:
 * @surface: (transfer full): surface, or %NULL
 * @width: width of surface, or -1
 * @height: height of surface, or -1
 * @error: error, or %NULL
 * @user_data: user data passed to function
 *
 * Both @surface and @error can be non-%NULL; in this case there was a failure
 * loading an image from disk, but @surface still contains a color/gradient to
 * use as a fallback.
 *
 * If @error is %G_IO_ERROR_CANCELLED, @surface will always be %NULL.
 **/
typedef void (*RenderCompleteCallback)(cairo_surface_t *surface,
                                       gint width,
                                       gint height,
                                       GError *error,
                                       gpointer user_data);

void xfdesktop_backdrop_render(GCancellable *cancellable,
                               XfceBackdropColorStyle color_style,
                               GdkRGBA *color1,
                               GdkRGBA *color2,
                               XfceBackdropImageStyle image_style,
                               GFile *image_file,
                               gint width,
                               gint height,
                               RenderCompleteCallback callback,
                               gpointer callback_user_data);

G_END_DECLS

#endif  /* __XFDESKTOP_BACKDROP_RENDERER_H__ */
