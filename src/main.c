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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#include <X11/Xlib.h>
#include <gtk/gtk.h>

#include <libxfce4util/i18n.h>
#include <libxfce4mcs/mcs-client.h>
#include <libxfcegui4/libxfcegui4.h>

#include "main.h"
#include "xfdesktop-common.h"
#include "xfce-backdrop.h"
#include "xfce-desktop.h"
#include "menu.h"
#include "windowlist.h"
#include "settings.h"

#define RELOAD_MESSAGE     "reload"
#define MENU_MESSAGE       "menu"
#define WINDOWLIST_MESSAGE "windowlist"

SessionClient *client_session = NULL;
gboolean is_session_managed = FALSE;

void
quit(gboolean force)
{
	if(is_session_managed)
		logout_session(client_session);
	else
		gtk_main_quit();
}

static void
session_die(gpointer user_data)
{
	gtk_main_quit();
}

static gboolean
scroll_cb(GtkWidget *w, GdkEventScroll *evt, gpointer user_data)
{
	GdkScrollDirection dir = evt->direction;
	GdkScreen *gscreen;
	NetkScreen *netk_screen;
	NetkWorkspace *ws = NULL;
	gint n, active;
	
	gscreen = gtk_widget_get_screen(w);
	netk_screen = netk_screen_get(gdk_screen_get_number(gscreen));
	n = netk_screen_get_workspace_count(netk_screen);
	if(n <= 1)
		return FALSE;
	
	ws = netk_screen_get_active_workspace(netk_screen);
	active = netk_workspace_get_number(ws);
	
	switch(dir) {
		case GDK_SCROLL_UP:
		case GDK_SCROLL_LEFT:
			if(active == 0)
				active = n - 1;
			else
				active--;
			break;
		
		case GDK_SCROLL_DOWN:
		case GDK_SCROLL_RIGHT:
			if(active == n - 1)
				active = 0;
			else
				active++;
	}
	
	ws = netk_screen_get_workspace(netk_screen, active);
	netk_workspace_activate(ws);
	
	return FALSE;
}

static gboolean
button_cb(GtkWidget *w, GdkEventButton *evt, gpointer user_data)
{
	GdkScreen *gscreen = gtk_widget_get_screen(w);
	gint button = evt->button;
	gint state = evt->state;
	
	if(button == 2 || (button == 1 && (state & GDK_SHIFT_MASK)
			&& (state & GDK_CONTROL_MASK)))
	{
		popup_windowlist(gscreen, button, evt->time);
		return TRUE;
	} else if(button == 3 || (button == 1 && (state & GDK_SHIFT_MASK))) {
		popup_desktop_menu(gscreen, button, evt->time);
		return TRUE;
	}
	
	return FALSE;
}

static void
send_client_message(Window xid, const gchar *msg)
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
	strcpy(gev.data.b, msg);
	
	gdk_event_send_client_message((GdkEvent *)&gev, (GdkNativeWindow)xid);
	gdk_flush();
	
	gtk_widget_destroy(win);
}

gboolean
client_message_received(GtkWidget *w, GdkEventClient *evt, gpointer user_data)
{
	if(evt->data_format == 8) {
		if(!strcmp(RELOAD_MESSAGE, evt->data.b)) {
			settings_reload_all();
			return TRUE;
		} else if(!strcmp(MENU_MESSAGE, evt->data.b)) {
			popup_desktop_menu(gtk_widget_get_screen(w), 0, GDK_CURRENT_TIME);
			return TRUE;
		} else if(!strcmp(WINDOWLIST_MESSAGE, evt->data.b)) {
			popup_windowlist(gtk_widget_get_screen(w), 0, GDK_CURRENT_TIME);
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean
check_is_running(Window *xid)
{
	const gchar *display = g_getenv("DISPLAY");
	gchar *p;
	gint xscreen = -1;
	gchar selection_name[100];
	Atom selection_atom;
	
	if(display) {
		if((p=g_strrstr(display, ".")))
			xscreen = atoi(p);
	}
	if(xscreen == -1)
		xscreen = 0;

	g_snprintf(selection_name, 100, XFDESKTOP_SELECTION_FMT, xscreen);
	selection_atom = XInternAtom(GDK_DISPLAY(), selection_name, False);

	if((*xid = XGetSelectionOwner(GDK_DISPLAY(), selection_atom)))
		return TRUE;

	return FALSE;
}

void
sighandler_cb(int sig)
{
	switch(sig) {
		case SIGUSR1:
			settings_reload_all();
			break;
		
		default:
			gtk_main_quit();
			break;
	}
}

int
main(int argc, char **argv)
{
	GdkDisplay *gdpy;
	GtkWidget **desktops;
	gint i, nscreens;
	Window xid;
	McsClient *mcs_client;
	
	if(argc > 1 && (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-V"))) {
		g_print("\tThis is %s version %s for Xfce %s\n", PACKAGE, VERSION,
				xfce_version_string());
		g_print("\tbuilt with GTK+-%d.%d.%d, ", GTK_MAJOR_VERSION,
				GTK_MINOR_VERSION, GTK_MICRO_VERSION);
		g_print("linked with GTK+-%d.%d.%d.\n", gtk_major_version,
				gtk_minor_version, gtk_micro_version);
		exit(0);
	}

	/* bind gettext textdomain */
	xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

	gtk_init(&argc, &argv);

	if(check_is_running(&xid)) {
		DBG("xfdesktop is running");
		
		if(argc <= 1 || strcmp("-reload", argv[1]) == 0)
			send_client_message(xid, RELOAD_MESSAGE);
		else if(strcmp("-menu", argv[1]) == 0)
			send_client_message(xid, MENU_MESSAGE);
		else if(strcmp("-windowlist", argv[1]) == 0)
			send_client_message(xid, WINDOWLIST_MESSAGE);
		else {
			g_printerr(_("%s: Unknown option: %s\n"), PACKAGE, argv[1]);
			g_printerr(_("Options are:\n"));
			g_printerr(_("    -reload      Reload all settings, refresh image list\n"));
			g_printerr(_("    -menu        Pop up the menu (at the current mouse position)\n"));
			g_printerr(_("    -windowlist  Pop up the window list (at the current mouse position)\n"));
			
			return 1;
		}
		
		return 0;
	}
	
	mcs_client = settings_init();
	
	gdpy = gdk_display_get_default();
	nscreens = gdk_display_get_n_screens(gdpy);
	desktops = g_new(GtkWidget *, nscreens);
	for(i = 0; i < nscreens; i++) {
		desktops[i] = xfce_desktop_new(gdk_display_get_screen(gdpy, i), mcs_client);
		gtk_widget_add_events(desktops[i],
			GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_SCROLL_MASK);
		g_signal_connect(G_OBJECT(desktops[i]), "scroll-event",
				G_CALLBACK(scroll_cb), NULL);
		g_signal_connect(G_OBJECT(desktops[i]), "button-press-event",
			G_CALLBACK(button_cb), NULL);
		if(mcs_client)
			settings_register_callback(xfce_desktop_settings_changed, desktops[i]);
		gtk_widget_show(desktops[i]);
		gdk_window_lower(desktops[i]->window);
	}
	
	client_session = client_session_new(argc, argv, NULL,
			SESSION_RESTART_IF_RUNNING, 35);
	client_session->die = session_die;
	is_session_managed = session_init(client_session);
	
	menu_init(mcs_client);
	windowlist_init(mcs_client);
	
	if(mcs_client) {
		settings_register_callback(menu_settings_changed, NULL);
		settings_register_callback(windowlist_settings_changed, NULL);
	}
	
	signal(SIGHUP, sighandler_cb);
	signal(SIGINT, sighandler_cb);
	signal(SIGTERM, sighandler_cb);
	signal(SIGUSR1, sighandler_cb);
	
	gtk_main();
	
	menu_cleanup();
	windowlist_cleanup();
	settings_cleanup();
	
	return 0;
}
