/*  xfce4
 *  
 *  Copyright (C) 2002 Jasper Huijsmans (huysmans@users.sourceforge.net)
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

#ifndef __XFDESKTOP_SETTING__
#define __XFDESKTOP_SETTING__

#include <libxfce4mcs/mcs-client.h>
#include "main.h"

typedef void (*ChannelCallback)(const char *channel_name, McsClient *client,
		McsAction action, McsSetting *setting);

McsClient *settings_init_global();
void settings_cleanup_global();

void settings_init(XfceDesktop *xfdesktop);

void register_channel(const char *channel_name, ChannelCallback callback);

extern McsClient *mcs_client;

#endif /* !__XFDESKTOP_SETTING__ */
