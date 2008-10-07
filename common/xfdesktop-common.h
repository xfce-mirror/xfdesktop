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
#include <gdk/gdk.h>

#include <X11/Xlib.h>

#define XFDESKTOP_CHANNEL        "xfce4-desktop"
#define DEFAULT_BACKDROP         DATADIR "/xfce4/backdrops/xfce-stripes.png"
#define DEFAULT_BACKDROP_LIST    "xfce4/desktop/backdrop.list"
#define DEFAULT_ICON_FONT_SIZE   12
#define DEFAULT_ICON_SIZE        32

#define LIST_TEXT                "# xfce backdrop list"
#define XFDESKTOP_SELECTION_FMT  "XFDESKTOP_SELECTION_%d"
#define XFDESKTOP_IMAGE_FILE_FMT "XFDESKTOP_IMAGE_FILE_%d"

#define RELOAD_MESSAGE     "reload"
#define MENU_MESSAGE       "menu"
#define WINDOWLIST_MESSAGE "windowlist"
#define QUIT_MESSAGE       "quit"

G_BEGIN_DECLS

gchar **xfdesktop_backdrop_list_load(const gchar *filename,
                                     gint *n_items,
                                     GError **error);
gboolean xfdesktop_backdrop_list_save(const gchar *filename,
                                      gchar * const *files,
                                      GError **error);
gchar *xfdesktop_backdrop_list_choose_random(const gchar *filename,
                                             GError **error);
gboolean xfdesktop_backdrop_list_is_valid(const gchar *filename);

gboolean xfdesktop_image_file_is_valid(const gchar *filename);

gboolean xfdesktop_check_is_running(Window *xid);
void xfdesktop_send_client_message(Window xid, const gchar *msg);

gboolean xfdesktop_popup_grab_available(GdkWindow *win, guint32 timestamp);

G_END_DECLS

#endif
