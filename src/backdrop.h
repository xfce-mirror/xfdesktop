/*  xfce4
 *  
 *  Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
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

#ifndef __XFDESKTOP_BACKDROP_H__
#define __XFDESKTOP_BACKDROP_H__

#include <gtk/gtkwidget.h>
#include <libxfce4mcs/mcs-client.h>

#include "backdrop-common.h"

typedef struct {
    Window root;
	int xscreen;
	GdkScreen *gscreen;
    Atom atom;
    Atom e_atom;

    GtkWidget *win;

    guint set_backdrop:1;
    guint color_only:1;

    GdkColor color1;
	GdkColor color2;
    char *path;
    XfceBackdropStyle style;
	
	McsClient *client;  /* FIXME */
} XfceBackdrop;

void backdrop_settings_init();
XfceBackdrop *backdrop_new(gint screen, GtkWidget *fullscreen, McsClient *client);
void backdrop_load_settings(XfceBackdrop *xfbackdrop);
void backdrop_cleanup(XfceBackdrop *xfbackdrop);

#endif /* __XFDESKTOP_BACKDROP_H__ */
