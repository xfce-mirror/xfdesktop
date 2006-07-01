/*      $Id$

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2, or (at your option)
        any later version.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

        (c) 2002-2006 Olivier Fourdan
 
 */

#include <gtk/gtk.h>
#include <glib.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "xfce4-popup-menu.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

/*
 * gcc -Wall -g xfce4-popup-menu.c -o xfce4-popup-menu `pkg-config --cflags gtk+-2.0` `pkg-config --libs gtk+-2.0`
 */

gboolean
xfce4_check_is_running(GtkWidget *widget, Window *xid)
{
    GdkScreen *gscreen;
    gchar selection_name[32];
    Atom selection_atom;

    gscreen = gtk_widget_get_screen (widget);
    g_snprintf(selection_name, 
               sizeof(selection_name), 
               XFCE_MENU_SELECTION"%d", 
               gdk_screen_get_number (gscreen));
    selection_atom = XInternAtom(GDK_DISPLAY(), selection_name, False);

    if((*xid = XGetSelectionOwner(GDK_DISPLAY(), selection_atom)))
        return TRUE;

    return FALSE;
}

int main( int   argc,
          char *argv[] )
{
    GdkEventClient gev;
    GtkWidget *win;
    Window id;
    
    gtk_init (&argc, &argv);
    
    win = gtk_invisible_new();
    gtk_widget_realize(win);
    
    gev.type = GDK_CLIENT_EVENT;
    gev.window = win->window;
    gev.send_event = TRUE;
    gev.message_type = gdk_atom_intern("STRING", FALSE);
    gev.data_format = 8;
    strcpy(gev.data.b, XFCE_MENU_MESSAGE);
    
    if (xfce4_check_is_running(win, &id))
        gdk_event_send_client_message((GdkEvent *)&gev, 
                                      (GdkNativeWindow)id);
    else
        g_warning ("Can't find the xfce4-panel menu to popup.\n");
    gdk_flush();
    
    gtk_widget_destroy(win);
    
    return 0;
}
