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

#ifndef _XFDESKTOP__WINDOWLIST_H_
#define _XFDESKTOP__WINDOWLIST_H_

#include <gdk/gdkscreen.h>

#include <libxfce4mcs/mcs-client.h>

G_BEGIN_DECLS

void windowlist_init(McsClient *mcs_client);
void popup_windowlist(GdkScreen *gscreen, gint button, guint32 time);
gboolean windowlist_settings_changed(McsClient *client, McsAction action, McsSetting *setting, gpointer user_data);
void windowlist_cleanup();

G_END_DECLS

#endif
