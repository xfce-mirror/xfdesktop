/*  xfce4
 *  
 *  Copyright (C) 2002 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *  Copyright (C) 2004 Brian Tarricone <bjt23@cornell.edu>
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
 *
 *  xfdesktop_check_image_file() is based on gdk_pixbuf_get_file_info(),
 *      which is copyright (c) 1999 The Free Software Foundation.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

#include <libxfce4mcs/mcs-manager.h>
#include <libxfce4util/libxfce4util.h>

#include "settings_common.h"

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
