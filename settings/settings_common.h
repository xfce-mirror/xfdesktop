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

#ifndef __XFDESKTOP_SETTINGS_COMMON_H
#define __XFDESKTOP_SETTINGS_COMMON_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4mcs/mcs-common.h>
#include <libxfce4mcs/mcs-manager.h>
#include <xfce-mcs-manager/manager-plugin.h>

/* from xfce4-panel */
#include <debug.h>

#define _(x) x
#define N_(x) x

#define BORDER 5

void create_channel(McsManager *manager, const char *channel, 
		    const char *rcfile);

gboolean save_channel(McsManager *manager, const char *channel, 
		      const char *rcfile);

void add_spacer(GtkBox * box);

#endif

