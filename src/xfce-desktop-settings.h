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
 */

#ifndef __XFCE_DESKTOP_SETTINGS_H__
#define __XFCE_DESKTOP_SETTINGS_H__

#include <libxfce4mcs/mcs-client.h>

#include "xfce-desktop.h"

G_BEGIN_DECLS

void xfce_desktop_settings_load_initial(XfceDesktop *desktop,
                                        McsClient *mcs_client);
gboolean xfce_desktop_settings_changed(McsClient *client,
                                       McsAction action,
                                       McsSetting *setting,
                                       gpointer user_data);

G_END_DECLS

#endif  /* __XFCE_DESKTOP_SETTINGS_H__ */
