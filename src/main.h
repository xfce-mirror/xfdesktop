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

#ifndef __XFDESKTOP_MAIN_H__
#define __XFDESKTOP_MAIN_H__

#include <X11/Xlib.h>
#include <gtk/gtkwidget.h>
#include <libxfcegui4/netk-screen.h>

#include "backdrop.h"

typedef struct {
    int xscreen;
	GdkScreen *gscreen;
    Window root;
	
	Atom selection_atom;
	Atom manager_atom;

    NetkScreen *netk_screen;
    GtkWidget *fullscreen;
	XfceBackdrop *backdrop;
	
	McsClient *client;  /* this will be a pointer to the global McsClient */
} XfceDesktop;

extern GList *desktops;

extern void quit (void);

#endif /* __XFDESKTOP_MAIN_H__ */
