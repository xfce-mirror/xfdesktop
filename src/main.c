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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef GDK_MULTIHEAD_SAFE
#undef GDK_MULTIHEAD_SAFE
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <libxfce4mcs/mcs-client.h>
#include <libxfce4util/debug.h>
#include <libxfce4util/i18n.h>
#include <libxfcegui4/libxfcegui4.h>

#include "main.h"
#include "settings.h"

#define XFDESKTOP_SELECTION "XFDESKTOP_SELECTION"

static GtkWidget *fullscreen_window;
static NetkScreen *netk_screen;

static SessionClient *client_session = NULL;
static gboolean session_managed = FALSE;

static Window selection_window;

void
quit(void)
{
    TRACE("dummy");

    if (session_managed)
	    logout_session(client_session);
    else
	    gtk_main_quit();
}

/* client message */
#define RELOAD_MESSAGE "reload"

static void
send_client_message(Window xid)
{
    GdkEventClient gev;
    GtkWidget *win;

    win = gtk_invisible_new();
    gtk_widget_realize(win);
    
    gev.type = GDK_CLIENT_EVENT;
    gev.window = win->window;
    gev.send_event = TRUE;
    gev.message_type = gdk_atom_intern("STRING", FALSE);
    gev.data_format = 8;
    strcpy(gev.data.b, RELOAD_MESSAGE);
    
    gdk_event_send_client_message((GdkEvent *)&gev, (GdkNativeWindow)xid);
    gdk_flush();
}

static gboolean
client_message_received(GtkWidget *widget, GdkEventClient *event,
        gpointer user_data)
{
    TRACE("dummy");

    if (event->data_format == 8 && strcmp(event->data.b, RELOAD_MESSAGE) == 0) {
	    load_settings();
	    return(TRUE);
    }
    
    return(FALSE);
}

/* selection to ensure uniqueness 
 * copied from settings manager sample code */
static Atom selection_atom = 0;
static Atom manager_atom = 0;

gboolean
check_is_running(Window *xid)
{
    TRACE("dummy");

    if (!selection_atom)
	    selection_atom = XInternAtom(gdk_display, XFDESKTOP_SELECTION, False);

    if ((*xid = XGetSelectionOwner (gdk_display, selection_atom)))
	    return(TRUE);

	return(FALSE);
}

static void
xfdesktop_set_selection(void)
{
    Display *display = GDK_DISPLAY();
    int screen = DefaultScreen(display);
    GtkWidget *invisible;
    
    TRACE("dummy");
    invisible = gtk_invisible_new();
    gtk_widget_realize(invisible);
    
    if (!selection_atom)
	    selection_atom = XInternAtom(gdk_display, XFDESKTOP_SELECTION, False);

    if (!manager_atom)
	    manager_atom = XInternAtom(gdk_display, "MANAGER", False);

    selection_window = GDK_WINDOW_XWINDOW(invisible->window);
	
/*	XCreateSimpleWindow (display, GDK_ROOT_WINDOW(),
				       	    0, 0, 10, 10, 0, 
					    WhitePixel (display, screen), 
					    WhitePixel (display, screen));
*/
    XSelectInput(display, selection_window, PropertyChangeMask);
    XSetSelectionOwner(display, selection_atom,
            selection_window, GDK_CURRENT_TIME);

    /* listen for client messages */
    g_signal_connect(invisible, "client-event",
            G_CALLBACK(client_message_received), NULL);

    /* Check to see if we managed to claim the selection. If not,
     * we treat it as if we got it then immediately lost it
     */
    if (XGetSelectionOwner(display, selection_atom) == selection_window) {
        XClientMessageEvent xev;

	    xev.type = ClientMessage;
	    xev.window = GDK_ROOT_WINDOW();
	    xev.message_type = manager_atom;
	    xev.format = 32;
	    xev.data.l[0] = GDK_CURRENT_TIME;
	    xev.data.l[1] = selection_atom;
	    xev.data.l[2] = selection_window;
	    xev.data.l[3] = 0;	/* manager specific data */
	    xev.data.l[4] = 0;	/* manager specific data */

	    XSendEvent(display, RootWindow (display, screen), False,
                StructureNotifyMask, (XEvent *)&xev);
    }
    else {
       g_warning("xfdesktop could not set selection ownership");
       exit (1);
    }
}

/* session management */
static void
die(gpointer client_data)
{
    TRACE("dummy");
    gtk_main_quit();
}

/* init and cleanup */

/* copied from ROX Filer pinboard feature 
 * (c) Thomas Leonard. See http://rox.sf.net. */
static GtkWidget *
create_fullscreen_window(void)
{
	GdkAtom desktop_type;
    GtkWidget *win;
    GtkStyle *style;
    
    TRACE("dummy");
    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    netk_gtk_window_avoid_input(GTK_WINDOW(win));
    gtk_widget_set_size_request(win, gdk_screen_width(), gdk_screen_height());
    gtk_widget_realize(win);
    gtk_window_move(GTK_WINDOW(win), 0, 0);

    gtk_window_set_title(GTK_WINDOW(win), _("Desktop"));
    
    style = gtk_widget_get_style(win);

    gtk_widget_modify_bg(win, GTK_STATE_NORMAL, &(style->black));
    gtk_widget_modify_base(win, GTK_STATE_NORMAL, &(style->black));
    /* Remove double buffering in desktop window otherwise
       gtk allocates twice the size of the screen in video memory
       which can show to be unusable even on a GeForce II MX with 
       32Mb using 3D because the video RAM alloted to pixmaps
       cannot handle the total amount of pixmaps.
     */
    if (GTK_WIDGET_DOUBLE_BUFFERED(win))
	    gtk_widget_set_double_buffered(win, FALSE);

	desktop_type = gdk_atom_intern("_NET_WM_WINDOW_TYPE_DESKTOP", FALSE);

	gdk_property_change(win->window,
		gdk_atom_intern("_NET_WM_WINDOW_TYPE", FALSE),
		gdk_atom_intern("ATOM", FALSE), 32,
		GDK_PROP_MODE_REPLACE, (guchar *) &desktop_type, 1);

    gtk_widget_add_events(win, 
	    GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_SCROLL_MASK);

    return(win);
}

static void
xfdesktop_init(void)
{
    TRACE("dummy");
    fullscreen_window = create_fullscreen_window();

    netk_screen = netk_screen_get_default();
    netk_screen_force_update(netk_screen); 
    
    watch_settings(fullscreen_window, netk_screen);
    
    gtk_widget_show(fullscreen_window);
    gdk_window_lower(fullscreen_window->window);
}

static void
xfdesktop_cleanup(void)
{
    TRACE("dummy");
    stop_watch();
    
    gtk_widget_destroy(fullscreen_window);
}

static void
sighandler(int sig)
{
    TRACE("dummy");

    switch(sig) {
	case SIGUSR1:
	    load_settings();
	    break;

	default:
	    gtk_main_quit();
	    break;
    }
}

int
main(int argc, char **argv)
{
#ifdef HAVE_SIGACTION
    struct sigaction act;
#endif
    Window xid;

    /* bind gettext textdomain */
    xfce_textdomain(GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    gtk_init(&argc, &argv);
    
    if (check_is_running(&xid)) {
	    send_client_message(xid);
	    return(0);
    }

#ifdef HAVE_SIGACTION
    act.sa_handler = sighandler;
    sigemptyset(&act.sa_mask);
#ifdef SA_RESTART
    act.sa_flags = SA_RESTART;
#else
    act.sa_flags = 0;
#endif
    (void)sigaction(SIGUSR1, &act, NULL);
    (void)sigaction(SIGINT, &act, NULL);
    (void)sigaction(SIGTERM, &act, NULL);
#else
    (void)signal(SIGUSR1, sighandler);
    (void)signal(SIGINT, sighandler);
    (void)signal(SIGTERM, sighandler);
#endif

    client_session = client_session_new(argc, argv, NULL /* data */ , 
	    				SESSION_RESTART_IF_RUNNING, 35);

    client_session->die = die;

    if(!(session_managed = session_init(client_session)))
        g_message("xfdesktop: running without session manager");
    
    xfdesktop_init();
    
    xfdesktop_set_selection();

    gtk_main();
    
    xfdesktop_cleanup();
    
    return 0;
}

