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

#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gtk/gtk.h>

#include <libxfce4mcs/mcs-client.h>
#include <libxfcegui4/libnetk.h>

#include "main.h"
#include "backdrop.h"
#include "menu.h"
#include "margins.h"
#include "settings.h"

static McsClient *client = NULL;
static GHashTable *settings_hash = NULL;

static Display *dpy;
static int xscreen;

/*  mcs client 
 *  ----------
*/
/* hash table: channel_name -> channel_callback */
static void init_settings_hash(void)
{
    settings_hash = g_hash_table_new(g_str_hash, g_str_equal);
    
    add_backdrop_callback(settings_hash);
    add_menu_callback(settings_hash);
    add_margins_callback(settings_hash);
    add_workspaces_callback(settings_hash);
}

/* load settings */
void load_settings(void)
{
    backdrop_load_settings(client);
    menu_load_settings(client);
    margins_load_settings(client);
    workspaces_load_settings(client);
}

/* settings */
static void notify_cb(const char *name, const char *channel_name, McsAction action, McsSetting * setting, void *data)
{
    void (*update_channel)(const char *name, McsAction action, 
	    		   McsSetting * setting);

    update_channel = g_hash_table_lookup(settings_hash, channel_name);

    if (update_channel)
	update_channel(name, action, setting);
    else
	g_printerr("Unknown channel: %s\n", channel_name);
}

GdkFilterReturn client_event_filter(GdkXEvent * xevent, GdkEvent * event, gpointer data)
{
    if(mcs_client_process_event(client, (XEvent *) xevent))
        return GDK_FILTER_REMOVE;
    else
        return GDK_FILTER_CONTINUE;
}

static void watch_cb(Window window, Bool is_start, long mask, void *cb_data)
{
    GdkWindow *gdkwin;

    gdkwin = gdk_window_lookup(window);

    if(is_start)
        gdk_window_add_filter(gdkwin, client_event_filter, NULL);
    else
        gdk_window_remove_filter(gdkwin, client_event_filter, NULL);
}

/* connecting and disconnecting */
static gboolean manager_is_running(Display *dpy, int screen)
{
    int result;
    
    /* we need a multi channel settings manager */
    result = mcs_manager_check_running(dpy, screen);

    return (MCS_MANAGER_STD < result);
}

static void start_mcs_manager(void)
{
    GError *error = NULL;
    
    g_message("xfdesktop: starting the settings manager\n");

    if (!g_spawn_command_line_sync("xfce-mcs-manager", 
				   NULL, NULL, NULL, &error))
    {
	g_critical("xfdesktop: could not start settings manager: %s\n",
		   error->message);
    }
    else
    {
        g_message("xfdesktop: settings manager successfully started\n");
    }
}

static void add_channel(char *channel, gpointer value, McsClient *client)
{
    mcs_client_add_channel(client, channel);
}

void watch_settings(GtkWidget *window, NetkScreen *screen)
{
    backdrop_init(window);
    menu_init(window, screen);
    margins_init(window);
    /* no workspaces init needed */

    if (!settings_hash)
    {
	init_settings_hash();

	dpy = GDK_DISPLAY();
	xscreen = DefaultScreen(dpy);
    }
    
    if (!manager_is_running(dpy, xscreen))
	start_mcs_manager();
    
    client = mcs_client_new(dpy, xscreen, notify_cb, watch_cb, NULL);
       
    if(!client || !manager_is_running(dpy, xscreen))
    {
        g_critical(_("xfdesktop: could not connect to settings manager!" 
		     "Please check your installation."));

/*	report_error(_("XFDesktop could not connect to the settings \n"
		       "manager.\n"
		       "Please make sure it is installed on your system."));
*/
	return;
    }
 
    g_hash_table_foreach(settings_hash, (GHFunc)add_channel, client);

    load_settings();
}

void stop_watch(void)
{
    if (client)
	mcs_client_destroy(client);

    client = NULL;
}

