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
*/

#ifndef __XFDESKTOP_SETTINGS_COMMON_H__
#define __XFDESKTOP_SETTINGS_COMMON_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkbox.h>
#include <xfce-mcs-manager/manager-plugin.h>
#include "backdrop-common.h"

#define BORDER 5

typedef struct {
    McsPlugin *plugin;

    /* options dialog */
    GtkWidget *dialog;
	GtkWidget *top_notebook;
	GtkWidget *screens_notebook;
	
	/* menu options */
	GtkWidget *frame_wl1;
#ifdef USE_DESKTOP_MENU
	GtkWidget *frame_dm1;
#endif
} BackdropDialog;

typedef struct {
	/* which screen this panel is for */
	gint xscreen;
	
	/* the settings themselves */
	gboolean set_backdrop;
	gchar *image_path;
	XfceBackdropStyle style;
	gboolean color_only;
	McsColor color1;
	McsColor color2;
	XfceColorStyle color_style;
	
	/* the panel's GUI controls */
	GtkWidget *color_frame;
	GtkWidget *color_style_combo;
	GtkWidget *color1_box;
	GtkWidget *color2_hbox;
	GtkWidget *color2_box;
	GtkWidget *color_only_chk;
	
	GtkWidget *image_frame;
	GtkWidget *file_entry;
	GtkWidget *edit_list_button;
	GtkWidget *style_combo;
	
	GtkWidget *set_backdrop_chk;
	
	/* backreference */
	BackdropDialog *bd;
} BackdropPanel;

extern void create_channel (McsManager *, const char *, const char *);
extern gboolean save_channel (McsManager *, const char *, const char *);
extern void add_spacer (GtkBox *);

#endif /* !__XFDESKTOP_SETTINGS_COMMON_H__ */
