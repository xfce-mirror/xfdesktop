/*  xfce4
 *  
 *  Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *  Copyright (C) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
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
#include "backdrop.h"
#include "menu.h"

/* uniqueness manager selection */
#define XFDESKTOP_SELECTION_FMT "XFDESKTOP_SELECTION_%d"

/* client messages */
#define RELOAD_MESSAGE "reload"
#define MENU_MESSAGE "menu"
#define WINDOWLIST_MESSAGE "windowlist"

/* globals */

static XfceDesktop xfdesktop;

static SessionClient *client_session = NULL;
static gboolean session_managed = FALSE;

static Atom selection_atom = 0;
static Atom manager_atom = 0;


void
quit (void)
{
    TRACE ("dummy");

    if (session_managed)
	logout_session (client_session);
    else
	gtk_main_quit ();
}

static void
load_settings ()
{
    DBG ("load settings");
    background_load_settings (&xfdesktop);
    menu_load_settings (&xfdesktop);
}

/* client messages */

static void
send_client_message (Window xid, const char *msg)
{
    GdkEventClient gev;
    GtkWidget *win;

    win = gtk_invisible_new ();
    gtk_widget_realize (win);

    gev.type = GDK_CLIENT_EVENT;
    gev.window = win->window;
    gev.send_event = TRUE;
    gev.message_type = gdk_atom_intern ("STRING", FALSE);
    gev.data_format = 8;
    strcpy (gev.data.b, msg);

    gdk_event_send_client_message ((GdkEvent *) & gev, (GdkNativeWindow) xid);
    gdk_flush ();

    gtk_widget_destroy (win);
}

static gboolean
client_message_received (GtkWidget * widget, GdkEventClient * event,
			 gpointer user_data)
{
    TRACE ("client message received");

    if (event->data_format == 8)
    {
	if (strcmp (RELOAD_MESSAGE, event->data.b) == 0)
	{
	    load_settings ();
	    return TRUE;
	}
	else if (strcmp (MENU_MESSAGE, event->data.b) == 0)
	{
	    popup_menu (0, GDK_CURRENT_TIME);
	    return TRUE;
	}
	else if (strcmp (WINDOWLIST_MESSAGE, event->data.b) == 0)
	{
	    popup_windowlist (0, GDK_CURRENT_TIME);
	    return TRUE;
	}
    }

    return FALSE;
}

/* uniqueness manager selection */

static gboolean
check_is_running (Window * xid)
{
    TRACE ("check for running instance on screen %d", xfdesktop.xscreen);

    if (!selection_atom)
    {
	char selection_name[100];

	sprintf (selection_name, XFDESKTOP_SELECTION_FMT, xfdesktop.xscreen);
	selection_atom = XInternAtom (GDK_DISPLAY (), selection_name, False);
    }

    if ((*xid = XGetSelectionOwner (GDK_DISPLAY (), selection_atom)))
	return TRUE;

    return FALSE;
}

static void
xfdesktop_set_selection (void)
{
    Display *dpy = xfdesktop.dpy;
    int scr = xfdesktop.xscreen;
    GtkWidget *gtkwin = xfdesktop.fullscreen;
    Window win;

    TRACE ("claiming xfdesktop manager selection for screen %d", scr);

    if (!selection_atom)
    {
	char selection_name[100];

	sprintf (selection_name, XFDESKTOP_SELECTION_FMT, scr);
	selection_atom = XInternAtom (dpy, selection_name, False);
    }

    if (!manager_atom)
	manager_atom = XInternAtom (gdk_display, "MANAGER", False);

    win = GDK_WINDOW_XID (gtkwin->window);

    XSelectInput (dpy, win, PropertyChangeMask | ButtonPressMask);
    XSetSelectionOwner (dpy, selection_atom, win, GDK_CURRENT_TIME);

    /* listen for client messages */
    g_signal_connect (gtkwin, "client-event",
		      G_CALLBACK (client_message_received), NULL);

    /* Check to see if we managed to claim the selection. If not,
     * we treat it as if we got it then immediately lost it
     */
    if (XGetSelectionOwner (dpy, selection_atom) == win)
    {
	XClientMessageEvent xev;
	Window root = xfdesktop.root;

	xev.type = ClientMessage;
	xev.window = root;
	xev.message_type = manager_atom;
	xev.format = 32;
	xev.data.l[0] = GDK_CURRENT_TIME;
	xev.data.l[1] = selection_atom;
	xev.data.l[2] = win;
	xev.data.l[3] = 0;	/* manager specific data */
	xev.data.l[4] = 0;	/* manager specific data */

	XSendEvent (dpy, root, False, StructureNotifyMask, (XEvent *) & xev);
    }
    else
    {
	g_error ("%s: could not set selection ownership", PACKAGE);
	exit (1);
    }
}

/* desktop window */

/* initially copied from ROX Filer pinboard feature 
 * (c) Thomas Leonard. See http://rox.sf.net. */
static GtkWidget *
create_fullscreen_window (void)
{
    GdkAtom atom;
    GtkWidget *win;
    Window xid;

    TRACE ("create fullscreen window");
    win = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    gtk_widget_add_events (win,
			   GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			   GDK_SCROLL_MASK);

    netk_gtk_window_avoid_input (GTK_WINDOW (win));

    gtk_widget_set_size_request (win, gdk_screen_width (),
				 gdk_screen_height ());

    gtk_window_move (GTK_WINDOW (win), 0, 0);

    gtk_widget_realize (win);

    gtk_window_set_title (GTK_WINDOW (win), _("Desktop"));

    /* Remove double buffering in desktop window otherwise
       gtk allocates twice the size of the screen in video memory
       which can show to be unusable even on a GeForce II MX with 
       32Mb using 3D because the video RAM alloted to pixmaps
       cannot handle the total amount of pixmaps.
     */
    if (GTK_WIDGET_DOUBLE_BUFFERED (win))
	gtk_widget_set_double_buffered (win, FALSE);

    gtk_window_set_type_hint (GTK_WINDOW (win), GDK_WINDOW_TYPE_HINT_DESKTOP);

    atom = gdk_atom_intern ("_NET_WM_WINDOW_TYPE_DESKTOP", FALSE);

    gdk_property_change (win->window,
			 gdk_atom_intern ("_NET_WM_WINDOW_TYPE", FALSE),
			 gdk_atom_intern ("ATOM", FALSE), 32,
			 GDK_PROP_MODE_REPLACE, (guchar *) & atom, 1);

    /* desktop window id -- should be a freedesktop standard IMO */
    atom = gdk_atom_intern ("XFCE_DESKTOP_WINDOW", FALSE);
    xid = GDK_WINDOW_XID (win->window);

    gdk_property_change (gdk_get_default_root_window (),
			 atom,
			 gdk_atom_intern ("WINDOW", FALSE), 32,
			 GDK_PROP_MODE_REPLACE, (guchar *) & xid, 1);

    return (win);
}

#if GTK_CHECK_VERSION(2,2,0)
/*
 * This is Gtk+ >= 2.2 only for now, should be no problem after all.
 */
static void
xfdesktop_size_changed (GdkScreen * screen)
{
    gdk_window_move_resize (xfdesktop.fullscreen->window, 0, 0,
			    gdk_screen_width (), gdk_screen_height ());
    load_settings ();
}
#endif

static void
xfdesktop_init (void)
{
    TRACE ("initialization");

    xfdesktop.dpy = GDK_DISPLAY ();
    xfdesktop.xscreen = DefaultScreen (xfdesktop.dpy);
    xfdesktop.root = GDK_ROOT_WINDOW ();

    xfdesktop.netk_screen = netk_screen_get_default ();
    netk_screen_force_update (xfdesktop.netk_screen);

    xfdesktop.fullscreen = create_fullscreen_window ();

    xfdesktop_set_selection ();

    settings_init (&xfdesktop);
    background_init (&xfdesktop);
    menu_init (&xfdesktop);

#if GTK_CHECK_VERSION(2,2,0)
    g_signal_connect (G_OBJECT (gdk_screen_get_default ()), "size-changed",
		      G_CALLBACK (xfdesktop_size_changed), NULL);
#endif

    load_settings ();
    gtk_widget_show (xfdesktop.fullscreen);
    gdk_window_lower (xfdesktop.fullscreen->window);
    load_settings ();
}

static void
xfdesktop_cleanup (void)
{
    TRACE ("cleaning up");
    settings_cleanup (&xfdesktop);
    background_cleanup (&xfdesktop);
}

static void
die (gpointer client_data)
{
    TRACE ("dummy");
    gtk_main_quit ();
}

static void
sighandler (int sig)
{
    TRACE ("signal %d caught", sig);

    switch (sig)
    {
	case SIGUSR1:
	    load_settings ();
	    break;

	default:
	    gtk_main_quit ();
	    break;
    }
}

int
main (int argc, char **argv)
{
#ifdef HAVE_SIGACTION
    struct sigaction act;
#endif
    Window xid;

    /* bind gettext textdomain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    gtk_init (&argc, &argv);

    if (check_is_running (&xid))
    {
	DBG ("xfdesktop is running");

	if (argc <= 1 || strcmp ("-reload", argv[1]) == 0)
	{
	    send_client_message (xid, RELOAD_MESSAGE);
	}
	else if (strcmp ("-menu", argv[1]) == 0)
	{
	    send_client_message (xid, MENU_MESSAGE);
	}
	else if (strcmp ("-windowlist", argv[1]) == 0)
	{
	    send_client_message (xid, WINDOWLIST_MESSAGE);
	}
	else
	{
	    g_message ("%s: unknown option: %s", PACKAGE, argv[1]);
	}

	return 0;
    }

#ifdef HAVE_SIGACTION
    act.sa_handler = sighandler;
    sigemptyset (&act.sa_mask);
#ifdef SA_RESTART
    act.sa_flags = SA_RESTART;
#else
    act.sa_flags = 0;
#endif
    (void) sigaction (SIGUSR1, &act, NULL);
    (void) sigaction (SIGINT, &act, NULL);
    (void) sigaction (SIGTERM, &act, NULL);
#else
    (void) signal (SIGUSR1, sighandler);
    (void) signal (SIGINT, sighandler);
    (void) signal (SIGTERM, sighandler);
#endif

    client_session = client_session_new (argc, argv, NULL /* data */ ,
					 SESSION_RESTART_IF_RUNNING, 35);
    client_session->die = die;
    session_managed = session_init (client_session);

    xfdesktop_init ();

    gtk_main ();

    xfdesktop_cleanup ();

    return 0;
}
