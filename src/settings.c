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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <libxfce4mcs/mcs-client.h>

#include "settings.h"
#include "xfdesktop-common.h"

typedef struct _SettingsCBData
{
    SettingsCallback cb;
    gpointer user_data;
} SettingsCBData;

static McsClient *mcs_client = NULL;
static GList *callbacks = NULL;

static GdkFilterReturn
client_event_filter(GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
    g_return_val_if_fail(mcs_client != NULL, GDK_FILTER_REMOVE);
    
    if(mcs_client_process_event(mcs_client, (XEvent *)xevent))
        return GDK_FILTER_REMOVE;
    else
        return GDK_FILTER_CONTINUE;
}

static void
watch_cb(Window window, Bool is_start, long mask, void *cb_data)
{
    GdkWindow *gdkwin = gdk_window_lookup(window);
    
    if(is_start)
        gdk_window_add_filter(gdkwin, client_event_filter, NULL);
    else
        gdk_window_remove_filter(gdkwin, client_event_filter, NULL);
}

static void
notify_cb(const char *name, const char *channel_name, McsAction action,
        McsSetting *setting, void *data)
{
    GList *l;
    
    g_return_if_fail(!strcmp(channel_name, BACKDROP_CHANNEL));
    
    for(l = callbacks; l; l = l->next) {
        SettingsCBData *cbd = l->data;
        
        if((*cbd->cb)(mcs_client, action, setting, cbd->user_data))
            break;
    }
}

McsClient *
settings_init()
{
    Display *xdpy;
    gint xscreen;
    
    g_return_val_if_fail(mcs_client == NULL, mcs_client);
    
    xdpy = GDK_DISPLAY();
    xscreen = gdk_screen_get_number(gdk_display_get_default_screen(gdk_display_get_default()));
    
    if(!mcs_client_check_manager(xdpy, xscreen, "xfce-mcs-manager")) {
        g_critical("%s: Unable to start settings manager", PACKAGE);
        return NULL;
    }
    
    mcs_client = mcs_client_new(xdpy, xscreen, notify_cb, watch_cb, NULL);
    if(!mcs_client) {
        g_critical("%s: Unable to connect to settings manager\n", PACKAGE);
        return NULL;
    }
    mcs_client_add_channel(mcs_client, BACKDROP_CHANNEL);
    
    return mcs_client;
}

void
settings_register_callback(SettingsCallback cb, gpointer user_data)
{
    SettingsCBData *cbdata;
    
    g_return_if_fail(cb != NULL && mcs_client != NULL);
    
    cbdata = g_new0(SettingsCBData, 1);
    cbdata->cb = cb;
    cbdata->user_data = user_data;
    
    callbacks = g_list_append(callbacks, cbdata);
}

void
settings_reload_all()
{
    if(mcs_client) {
        mcs_client_destroy(mcs_client);
        mcs_client = NULL;
    }
    
    settings_init();
}

void
settings_cleanup()
{
    GList *l;
    
    if(mcs_client) {
        mcs_client_destroy(mcs_client);
        mcs_client = NULL;
    }
    
    for(l = callbacks; l; l = l->next)
        g_free(l->data);
    if(callbacks) {
        g_list_free(callbacks);
        callbacks = NULL;
    }
}
