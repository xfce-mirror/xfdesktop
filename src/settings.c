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
    gpointer user_data;
    GList *callbacks;
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
    GList *l, *m;
    
    g_return_if_fail(!strcmp(channel_name, BACKDROP_CHANNEL));
    
    for(l = callbacks; l; l = l->next) {
        SettingsCBData *cbdata = l->data;
        
        for(m = cbdata->callbacks; m; m = m->next) {
            SettingsCallback cb = m->data;
            if(cb(mcs_client, action, setting, cbdata->user_data))
                break;
        }
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

static gint
settings_compare_cbdata(gconstpointer a,
                        gconstpointer b)
{
    const SettingsCBData *ca = a, *cb = b;
    return ca->user_data - cb->user_data;
}

/* callbacks are keyed to the user_data parameter, which can't be NULL.
 * for each user_data parameter registered, each callback registered (there
 * can be more than one) for that user_data pointer will be called until
 * one of them returns TRUE. */
void
settings_register_callback(SettingsCallback cb, gpointer user_data)
{
    GList *l;
    SettingsCBData *cbdata;
    
    g_return_if_fail(cb && user_data && mcs_client);
    
    l = g_list_find_custom(callbacks, user_data, settings_compare_cbdata);
    if(l)
        cbdata = l->data;
    else {
        cbdata = g_new0(SettingsCBData, 1);
        cbdata->user_data = user_data;
        callbacks = g_list_append(callbacks, cbdata);
    }
    
    cbdata->callbacks = g_list_append(cbdata->callbacks, cb);
}

void
settings_reload_all()
{
    g_return_if_fail(mcs_client);
    
    mcs_client_delete_channel(mcs_client, BACKDROP_CHANNEL);
    mcs_client_add_channel(mcs_client, BACKDROP_CHANNEL);
}

void
settings_cleanup()
{
    GList *l;
    
    if(mcs_client) {
        mcs_client_destroy(mcs_client);
        mcs_client = NULL;
    }
    
    for(l = callbacks; l; l = l->next) {
        g_list_free(((SettingsCBData *)l->data)->callbacks);
        g_free(l->data);
    }
    g_list_free(callbacks);
    callbacks = NULL;
}
