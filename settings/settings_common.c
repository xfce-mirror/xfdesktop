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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <gtk/gtk.h>

#include <libxfce4mcs/mcs-manager.h>
#include <libxfce4util/libxfce4util.h>

#include "settings_common.h"

void
create_channel (McsManager * manager, const gchar * channel,
		const gchar * rcfile)
{
    gchar *homefile;
    gchar *sysfile;

    homefile = xfce_get_userfile ("settings", rcfile, NULL);
    sysfile = g_build_filename (DATADIR, "xfce4", "settings", rcfile, NULL);

    if (g_file_test (homefile, G_FILE_TEST_EXISTS))
	mcs_manager_add_channel_from_file (manager, channel, homefile);
    else if (g_file_test (sysfile, G_FILE_TEST_EXISTS))
	mcs_manager_add_channel_from_file (manager, channel, sysfile);
    else
	mcs_manager_add_channel (manager, channel);

    g_free (homefile);
    g_free (sysfile);
}

gboolean
save_channel (McsManager * manager, const gchar * channel,
	      const gchar * rcfile)
{
    gboolean result;
    gchar *homefile;

    homefile = xfce_get_userfile ("settings", rcfile, NULL);
    result = mcs_manager_save_channel_to_file (manager, channel, homefile);
    g_free (homefile);

    return (result);
}

/* useful widget */
#define SKIP BORDER

void
add_spacer (GtkBox * box)
{
    GtkWidget *align = gtk_alignment_new (0, 0, 0, 0);

    gtk_widget_set_size_request (align, SKIP, SKIP);
    gtk_widget_show (align);
    gtk_box_pack_start (box, align, FALSE, TRUE, 0);
}
