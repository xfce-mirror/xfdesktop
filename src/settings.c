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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <gtk/gtk.h>

#include <libxfce4mcs/mcs-client.h>
#include <libxfce4util/debug.h>
#include <libxfce4util/i18n.h>
#include <libxfcegui4/libnetk.h>

#include "settings.h"

/* hash table: channel_name -> channel_callback */
static GHashTable *settings_hash = NULL;

void
register_channel_callback (const char *name, ChannelCallback callback)
{
    g_hash_table_insert (settings_hash, (gpointer)name, (gpointer)callback);
}

static void init_settings_hash(void)
{
    TRACE("dummy");
    settings_hash = g_hash_table_new(g_str_hash, g_str_equal);
}

static void 
notify_cb(const char *name, const char *channel_name, McsAction action, 
	  McsSetting * setting, XfceDesktop *xfdesktop)
{
    ChannelCallback update_channel;

    TRACE("dummy");
    update_channel = g_hash_table_lookup(settings_hash, channel_name);

    if (update_channel)
	update_channel(name, action, setting, xfdesktop);
    else
	g_printerr("%s: Unknown settings channel: %s\n", PACKAGE, 
		   channel_name);
}

GdkFilterReturn 
client_event_filter(GdkXEvent * xevent, GdkEvent * event, 
	            XfceDesktop *xfdesktop)
{
    TRACE("dummy");
    if(mcs_client_process_event(xfdesktop->client, (XEvent *) xevent))
        return GDK_FILTER_REMOVE;
    else
        return GDK_FILTER_CONTINUE;
}

static void 
watch_cb(Window window, Bool is_start, long mask, void *cb_data)
{
    GdkWindow *gdkwin;

    TRACE("dummy");
    gdkwin = gdk_window_lookup(window);

    if(is_start)
        gdk_window_add_filter(gdkwin, (GdkFilterFunc) client_event_filter, 
			      cb_data);
    else
        gdk_window_remove_filter(gdkwin, (GdkFilterFunc) client_event_filter, 
				 cb_data);
}

static gboolean 
manager_is_running(Display *dpy, int screen)
{
    int result;
    
    TRACE("dummy");
    
    /* we need a multi channel settings manager */
    result = mcs_manager_check_running(dpy, screen);

    return (MCS_MANAGER_STD < result);
}

static void 
start_mcs_manager(void)
{
    GError *error = NULL;
    
    TRACE("dummy");
    
    g_message("%s: starting the settings manager\n", PACKAGE);

    if (!g_spawn_command_line_sync("xfce-mcs-manager", 
				   NULL, NULL, NULL, &error))
    {
	g_critical("%s: could not start settings manager:\n%s\n",
		   PACKAGE, error->message);
	g_error_free (error);
    }
}

void 
settings_init(XfceDesktop *xfdesktop)
{
    TRACE("dummy");
    
    if (!settings_hash)
	init_settings_hash();
    
    if (!manager_is_running(xfdesktop->dpy, xfdesktop->xscreen))
	start_mcs_manager();
    
    xfdesktop->client = mcs_client_new(xfdesktop->dpy, xfdesktop->xscreen, 
	    			       (McsNotifyFunc) notify_cb, 
				       (McsWatchFunc) watch_cb, 
				       (gpointer) xfdesktop);
       
    if(!xfdesktop->client || 
       !manager_is_running(xfdesktop->dpy, xfdesktop->xscreen))
    {
        g_critical("%s: could not connect to settings manager!", PACKAGE);
    }
}

void settings_cleanup(XfceDesktop *xfdesktop)
{
    TRACE("dummy");
    
    if (xfdesktop->client)
	mcs_client_destroy(xfdesktop->client);

    xfdesktop->client = NULL;
}

