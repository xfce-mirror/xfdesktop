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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <X11/Xlib.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libxfce4mcs/mcs-client.h>
#include <libxfce4util/debug.h>
#include <libxfcegui4/netk-screen.h>

#include "main.h"
#include "workspaces.h"
#include "settings.h"
#include "settings/workspaces_settings.h"

static void set_workspace_count(int n)
{
    
    static Atom xa_NET_NUMBER_OF_DESKTOPS = 0;
    XClientMessageEvent sev;

    TRACE("");
    if(!xa_NET_NUMBER_OF_DESKTOPS)
    {
	xa_NET_NUMBER_OF_DESKTOPS = 
	    XInternAtom(GDK_DISPLAY(), "_NET_NUMBER_OF_DESKTOPS", False);
    }

    sev.type = ClientMessage;
    sev.display = GDK_DISPLAY();
    sev.format = 32;
    sev.window = GDK_ROOT_WINDOW();
    sev.message_type = xa_NET_NUMBER_OF_DESKTOPS;
    sev.data.l[0] = n;
    
/*    g_message ("setting nbr of desktops to %i", count);*/

    gdk_error_trap_push();
    XSendEvent(GDK_DISPLAY(), GDK_ROOT_WINDOW(), False,
               SubstructureNotifyMask | SubstructureRedirectMask,
               (XEvent *) & sev);
    gdk_flush();
    gdk_error_trap_pop();
}

static void set_workspace_names(const char *names)
{
    char *data;
    static Atom xa_NET_DESKTOP_NAMES = 0;
    int len;

    TRACE("");
    if (!xa_NET_DESKTOP_NAMES)
    {
	xa_NET_DESKTOP_NAMES = XInternAtom(GDK_DISPLAY(), 
					   "_NET_DESKTOP_NAMES", False);
    }

    data = g_strdup(names);
    len = strlen(data);
    
    data = g_strdelimit(data, SEP_S, '\0');
    
    gdk_error_trap_push();
    gdk_property_change(gdk_get_default_root_window(), 
	    		gdk_x11_xatom_to_atom(xa_NET_DESKTOP_NAMES),
			gdk_atom_intern("UTF8_STRING", FALSE),
			8, /* FIXME: is this correct ? */
			GDK_PROP_MODE_REPLACE,
			data, len);
    gdk_flush();
    gdk_error_trap_pop();

    g_free(data);
}

void workspaces_load_settings(McsClient *client)
{
    McsSetting *setting;

    TRACE("");
    if (MCS_SUCCESS == mcs_client_get_setting(client, "count", WORKSPACES_CHANNEL, &setting))
    {
	set_workspace_count(setting->data.v_int);
	mcs_setting_free(setting);
    }

    if (MCS_SUCCESS == mcs_client_get_setting(client, "names", WORKSPACES_CHANNEL, &setting))
    {
	set_workspace_names(setting->data.v_string);
	mcs_setting_free(setting);
    }
}

/* callback */
static void update_workspaces_channel(const char *name, McsAction action,
				      McsSetting *setting)
{
    TRACE("");
    switch (action)
    {
        case MCS_ACTION_NEW:
	    /* fall through unless we are in init state */
	    if (init_settings)
	    {
	        return;
	    }
        case MCS_ACTION_CHANGED:
	    if (strcmp(name, "count") == 0)
		set_workspace_count(setting->data.v_int);
	    else if (strcmp(name, "names") == 0)
		set_workspace_names(setting->data.v_string);
            break;
        case MCS_ACTION_DELETED:
	    /* We don't use this now. Perhaps revert to default? */
            break;
    }
}

void add_workspaces_callback(GHashTable *ht)
{
    TRACE("");
    g_hash_table_insert(ht, WORKSPACES_CHANNEL, update_workspaces_channel);
}

