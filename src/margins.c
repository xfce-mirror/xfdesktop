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

#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>

#include "main.h"
#include "margins.h"
#include "settings.h"
#include "settings/margins_settings.h"


static Window xwin;
static DesktopMargins margins;

void margins_init(GtkWidget *window)
{
    xwin = GDK_WINDOW_XWINDOW(window->window);

    margins.left = 0;
    margins.right = 0;
    margins.top = 0;
    margins.bottom = 0;
}

void margins_load_settings(McsClient *client)
{
    McsSetting *setting;

    if (MCS_SUCCESS == mcs_client_get_setting(client, "left", MARGINS_CHANNEL, &setting))
    {
	margins.left = setting->data.v_int;
	mcs_setting_free(setting);
    }

    if (MCS_SUCCESS == mcs_client_get_setting(client, "right", MARGINS_CHANNEL, &setting))
    {
	margins.right = setting->data.v_int;
	mcs_setting_free(setting);
    }

    if (MCS_SUCCESS == mcs_client_get_setting(client, "top", MARGINS_CHANNEL, &setting))
    {
	margins.top = setting->data.v_int;
	mcs_setting_free(setting);
    }

    if (MCS_SUCCESS == mcs_client_get_setting(client, "bottom", MARGINS_CHANNEL, &setting))
    {
	margins.bottom = setting->data.v_int;
	mcs_setting_free(setting);
    }
    
    if (margins.left < 0)
	margins.left = 0;

    if (margins.right < 0)
	margins.right = 0;

    if (margins.top < 0)
	margins.top = 0;

    if (margins.bottom < 0)
	margins.bottom = 0;

    netk_set_desktop_margins(xwin, &margins);
}

/* callback */
static void update_margins_channel(const char *name, McsAction action,
				    McsSetting *setting)
{
    int margin;
    
    switch (action)
    {
        case MCS_ACTION_NEW:
	    /* fall through unless we are in init state */
	    if (init_settings)
	    {
	        return;
	    }
        case MCS_ACTION_CHANGED:
	    margin = setting->data.v_int;

	    if (strcmp(name, "left") == 0)
		margins.left = margin;
	    else if (strcmp(name, "right") == 0)
		margins.right = margin;
	    else if (strcmp(name, "top") == 0)
		margins.top = margin;
	    else if (strcmp(name, "bottom") == 0)
		margins.bottom = margin;
	    
	    netk_set_desktop_margins(xwin, &margins);
            break;
        case MCS_ACTION_DELETED:
	    /* We don't use this now. Perhaps revert to default? */
            break;
    }
}

void add_margins_callback(GHashTable *ht)
{
    g_hash_table_insert(ht, MARGINS_CHANNEL, update_margins_channel);
}

