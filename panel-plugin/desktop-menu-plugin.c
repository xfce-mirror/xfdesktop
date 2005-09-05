/*
 *  desktop-menu-plugin.c - xfce4-panel plugin that displays the desktop menu
 *
 *  Copyright (C) 2004 Brian Tarricone, <bjt23@cornell.edu>
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
 *  Contributors:
 *    Jean-Francois Wauthy (option panel for choice between icon/text)
 *    Jasper Huijsmans (menu placement function, toggle button, scaled image
 *                      fixes)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <libxfcegui4/xfce_scaled_image.h>

#include <panel/plugins.h>
#include <panel/xfce.h>

#include "desktop-menu-stub.h"

#define BORDER 8
#define DEFAULT_BUTTON_ICON  DATADIR "/pixmaps/xfce4_xicon1.png"

typedef struct _DMPlugin {
	GtkWidget *button;
	GtkWidget *image;
	XfceDesktopMenu *desktop_menu;
	gboolean use_default_menu;
	gchar *menu_file;
	gchar *icon_file;
	gboolean show_menu_icons;
	gchar *button_title;
	
	GtkWidget *file_entry;
	GtkWidget *file_fb;
	GtkWidget *icon_entry;
	GtkWidget *icon_fb;
	GtkWidget *icons_chk;
	GtkTooltips *tooltip;  /* needed? */
} DMPlugin;



#if GTK_CHECK_VERSION(2, 6, 0)
/* util */
GtkWidget *
xfutil_custom_button_new(const gchar *text, const gchar *icon)
{
	GtkWidget *btn, *hbox, *img, *lbl;
	
	hbox = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(hbox);
	
	img = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_BUTTON);
	if(img) {
		if(gtk_image_get_storage_type(GTK_IMAGE(img)) != GTK_IMAGE_EMPTY) {
			gtk_widget_show(img);
			gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
		} else
			gtk_widget_destroy(img);
	}
	
	lbl = gtk_label_new_with_mnemonic(text);
	gtk_widget_show(lbl);
	gtk_box_pack_start(GTK_BOX(hbox), lbl, FALSE, FALSE, 0);
	
	btn = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(btn), hbox);
	gtk_label_set_mnemonic_widget(GTK_LABEL(lbl), btn);
	
	return btn;
}
#endif

static gchar *
dmp_get_real_path(const gchar *raw_path)
{
	gchar *path;
	
	if(!raw_path)
		return NULL;
	
	if(strstr(raw_path, "$XDG_CONFIG_DIRS/") == raw_path)
		return xfce_resource_lookup(XFCE_RESOURCE_CONFIG, raw_path+17);
	else if(strstr(raw_path, "$XDG_CONFIG_HOME/") == raw_path)
		return xfce_resource_save_location(XFCE_RESOURCE_CONFIG, raw_path+17, FALSE);
	else if(strstr(raw_path, "$XDG_DATA_DIRS/") == raw_path)
		return xfce_resource_lookup(XFCE_RESOURCE_DATA, raw_path+15);
	else if(strstr(raw_path, "$XDG_DATA_HOME/") == raw_path)
		return xfce_resource_save_location(XFCE_RESOURCE_DATA, raw_path+15, FALSE);
	else if(strstr(raw_path, "$XDG_CACHE_HOME/") == raw_path)
		return xfce_resource_save_location(XFCE_RESOURCE_CACHE, raw_path+16, FALSE);
	
	return xfce_expand_variables(raw_path, NULL);
}

static void
dmp_set_size(Control *c, int size)
{
	DMPlugin *dmp = c->data;
	GdkPixbuf *pix;
	int s = icon_size[size] + border_width;

	if(dmp->icon_file) {
		pix = xfce_themed_icon_load(dmp->icon_file, s - border_width);
		if(pix) {
			xfce_scaled_image_set_from_pixbuf(XFCE_SCALED_IMAGE(dmp->image), pix);
			g_object_unref(G_OBJECT(pix));
		}
	}
	
	gtk_widget_set_size_request (c->base, s, s);
}

static void
dmp_attach_callback(Control *c, const char *signal, GCallback callback,
		gpointer data)
{
	DMPlugin *dmp = c->data;
	
	g_signal_connect(G_OBJECT(dmp->button), signal, callback, data);
}

static void
dmp_free(Control *c)
{
	DMPlugin *dmp = c->data;
	
	if(dmp->desktop_menu)
		xfce_desktop_menu_destroy(dmp->desktop_menu);
	if(dmp->tooltip)
		gtk_object_sink(GTK_OBJECT(dmp->tooltip));
	
	if(dmp->menu_file)
		g_free(dmp->menu_file);
	if(dmp->icon_file)
		g_free(dmp->icon_file);
	if(dmp->button_title)
		g_free(dmp->button_title);
	
	g_free(dmp);
}

static void
dmp_position_menu (GtkMenu *menu, int *x, int *y, gboolean *push_in, 
				   GtkWidget *button)
{
	GdkWindow *p;
	int xbutton, ybutton, xparent, yparent;
	int side;
	GtkRequisition req;

	gtk_widget_size_request (GTK_WIDGET (menu), &req);

	xbutton = button->allocation.x;
	ybutton = button->allocation.y;
	
	p = gtk_widget_get_parent_window (button);
	gdk_window_get_root_origin (p, &xparent, &yparent);

	side = panel_get_side ();

	/* set x and y to topleft corner of the button */
	*x = xbutton + xparent;
	*y = ybutton + yparent;

	switch (side)
	{
		case LEFT:
			*x += button->allocation.width;
			*y += button->allocation.height - req.height;
			break;
		case RIGHT:
			*x -= req.width;
			*y += button->allocation.height - req.height;
			break;
		case TOP:
			*y += button->allocation.height;
			break;
		default:
			*y -= req.height;
	}

	if (*x < 0)
		*x = 0;

	if (*y < 0)
		*y = 0;

	/* TODO: wtf is this ? */
	*push_in = FALSE;
}

static void
menu_deactivated(GtkWidget *menu, gpointer user_data)
{
	int id;
	DMPlugin *dmp = user_data;

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dmp->button), FALSE);

	id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(menu), "sig_id"));

	g_signal_handler_disconnect(menu, id);
}

static void
dmp_popup(GtkWidget *w, gpointer user_data)
{
	GtkWidget *menu;
	DMPlugin *dmp = user_data;
	
	if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)))
		return;
	
	if(!dmp->desktop_menu) {
		g_critical("dmp->desktop_menu is NULL - module load failed?");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), FALSE);
		return;
	}
	
	if(xfce_desktop_menu_need_update(dmp->desktop_menu))
		xfce_desktop_menu_force_regen(dmp->desktop_menu);

	menu = xfce_desktop_menu_get_widget(dmp->desktop_menu);
	if(menu) {
		guint id;
		
		panel_register_open_menu(menu);
		id = g_signal_connect(menu, "deactivate", 
				G_CALLBACK(menu_deactivated), dmp);
		g_object_set_data(G_OBJECT(menu), "sig_id", GUINT_TO_POINTER(id));
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, 
				(GtkMenuPositionFunc)dmp_position_menu, dmp->button->parent, 
				1, gtk_get_current_event_time());
	} else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), FALSE);
}

static DMPlugin *
dmp_new()
{
	DMPlugin *dmp = g_new0(DMPlugin, 1);
	dmp->use_default_menu = TRUE;
	
	dmp->show_menu_icons = TRUE;  /* default */
	dmp->tooltip = gtk_tooltips_new();

	dmp->button = gtk_toggle_button_new();
	gtk_button_set_relief(GTK_BUTTON(dmp->button), GTK_RELIEF_NONE);
	gtk_widget_show(dmp->button);
	if(!dmp->button_title)
		dmp->button_title = g_strdup(_("Xfce Menu"));
	gtk_tooltips_set_tip(dmp->tooltip, dmp->button, dmp->button_title, NULL);
	
	dmp->image = xfce_scaled_image_new();
	gtk_widget_show(dmp->image);
	gtk_container_add(GTK_CONTAINER(dmp->button), dmp->image);
	
	dmp->desktop_menu = xfce_desktop_menu_new(NULL, TRUE);
	if(dmp->desktop_menu)
		xfce_desktop_menu_start_autoregen(dmp->desktop_menu, 10);
	g_signal_connect(G_OBJECT(dmp->button), "toggled",
			G_CALLBACK(dmp_popup), dmp);
	
	dmp->icon_file = g_strdup(DEFAULT_BUTTON_ICON);
	
	return dmp;
}

static gboolean
dmp_create(Control *c)
{
	DMPlugin *dmp;

	dmp = dmp_new();
	gtk_container_add(GTK_CONTAINER(c->base), dmp->button);
	
	c->data = (gpointer)dmp;
	c->with_popup = FALSE;
	
	return TRUE;
}

static void
dmp_read_config(Control *control, xmlNodePtr node)
{
	xmlChar *value;
	DMPlugin *dmp = control->data;
	GdkPixbuf *pix;
	gboolean redo_desktop_menu = FALSE, migration__got_menu_bool = FALSE;
	
	value = xmlGetProp(node, (const xmlChar *)"use_default_menu");
	if(value) {
		migration__got_menu_bool = TRUE;
		if(*value == '1') {
			if(!dmp->use_default_menu)
				redo_desktop_menu = TRUE;
			dmp->use_default_menu = TRUE;
		} else {
			if(dmp->use_default_menu)
				redo_desktop_menu = TRUE;
			dmp->use_default_menu = FALSE;
		}
		xmlFree(value);
	}
	
	value = xmlGetProp(node, (const xmlChar *)"menu_file");
	if(value) {
		if(!migration__got_menu_bool)
			dmp->use_default_menu = FALSE;
		
		if(!dmp->use_default_menu)
			redo_desktop_menu = TRUE;
		
		if(dmp->menu_file)
			g_free(dmp->menu_file);
		dmp->menu_file = value;
	} else
		dmp->use_default_menu = TRUE;
	
	if(redo_desktop_menu) {
		if(dmp->desktop_menu)
			xfce_desktop_menu_destroy(dmp->desktop_menu);
		
		if(dmp->use_default_menu)
			dmp->desktop_menu = xfce_desktop_menu_new(NULL, TRUE);
		else {
			gchar *path;
			path = dmp_get_real_path(dmp->menu_file);
			dmp->desktop_menu = xfce_desktop_menu_new(path, TRUE);
			g_free(path);
		}
	}
	
	value = xmlGetProp(node, (const xmlChar *)"icon_file");
	if(value) {
		pix = xfce_themed_icon_load(value,
				icon_size[settings.size] - 2*border_width);
		if(pix) {
			if(dmp->icon_file)
				g_free(dmp->icon_file);
			dmp->icon_file = (gchar *)value;
			xfce_scaled_image_set_from_pixbuf(XFCE_SCALED_IMAGE(dmp->image), pix);
			g_object_unref(G_OBJECT(pix));
		} else
			xmlFree(value);
	} else {
		dmp->icon_file = g_strdup(DEFAULT_BUTTON_ICON);
		pix = xfce_themed_icon_load(dmp->icon_file,
				icon_size[settings.size] - 2*border_width);
		if(pix) {
			xfce_scaled_image_set_from_pixbuf(XFCE_SCALED_IMAGE(dmp->image), pix);
			g_object_unref(G_OBJECT(pix));
		}
	}
	
	value = xmlGetProp(node, (const xmlChar *)"show_menu_icons");
	if(value) {
		if(*value == '0')
			dmp->show_menu_icons = FALSE;
		else
			dmp->show_menu_icons = TRUE;
		if(dmp->desktop_menu)
			xfce_desktop_menu_set_show_icons(dmp->desktop_menu, dmp->show_menu_icons);
		xmlFree(value);
	}
	
	value = xmlGetProp(node, (const xmlChar *)"button_title");
	if(value) {
		if(dmp->button_title)
			g_free(dmp->button_title);
		dmp->button_title = value;
		if(dmp->tooltip && dmp->button)
			gtk_tooltips_set_tip(dmp->tooltip, dmp->button, dmp->button_title, NULL);
	}
}

static void
dmp_write_config(Control *control, xmlNodePtr node)
{
	DMPlugin *dmp = control->data;
	
	xmlSetProp(node, (const xmlChar *)"use_default_menu", dmp->use_default_menu ? "1" : "0");
	xmlSetProp(node, (const xmlChar *)"menu_file", dmp->menu_file ? dmp->menu_file : "");
	xmlSetProp(node, (const xmlChar *)"icon_file", dmp->icon_file ? dmp->icon_file : "");
	xmlSetProp(node, (const xmlChar *)"show_menu_icons", dmp->show_menu_icons ? "1" : "0");
	xmlSetProp(node, (const xmlChar *)"button_title", dmp->button_title ? dmp->button_title : "");
}

static gboolean
entry_focus_out_cb(GtkWidget *w, GdkEventFocus *evt, gpointer user_data)
{
	DMPlugin *dmp = user_data;
	GdkPixbuf *pix;
	const gchar *cur_file;
	
	if(w == dmp->icon_entry) {
		if(dmp->icon_file)
			g_free(dmp->icon_file);
	
		dmp->icon_file = gtk_editable_get_chars(GTK_EDITABLE(w), 0, -1);
		pix = xfce_themed_icon_load(dmp->icon_file,
				icon_size[settings.size] - 2*border_width);
		if(pix) {
			xfce_scaled_image_set_from_pixbuf(XFCE_SCALED_IMAGE(dmp->image), pix);
			g_object_unref(G_OBJECT(pix));
		} else
			xfce_scaled_image_set_from_pixbuf(XFCE_SCALED_IMAGE(dmp->image), NULL);
	} else if(w == dmp->file_entry) {
		if(dmp->menu_file)
			g_free(dmp->menu_file);
		
		dmp->menu_file = gtk_editable_get_chars(GTK_EDITABLE(w), 0, -1);
		if(dmp->desktop_menu) {
			cur_file = xfce_desktop_menu_get_menu_file(dmp->desktop_menu);
			if(strcmp(dmp->menu_file, cur_file)) {
				gchar *path;
				xfce_desktop_menu_destroy(dmp->desktop_menu);
				path = dmp_get_real_path(dmp->menu_file);
				dmp->desktop_menu = xfce_desktop_menu_new(path, TRUE);
				g_free(path);
				if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dmp->icons_chk)))
					xfce_desktop_menu_set_show_icons(dmp->desktop_menu, FALSE);
			}
		}
	}
	
	return FALSE;
}

static void
filebutton_update_preview_cb(XfceFileChooser *chooser, gpointer user_data)
{
	GtkImage *preview;
	gchar *filename;
	GdkPixbuf *pix = NULL;
	
	preview = GTK_IMAGE(user_data);
	filename = xfce_file_chooser_get_filename(chooser);
	
	if(g_file_test(filename, G_FILE_TEST_IS_REGULAR))
		pix = xfce_pixbuf_new_from_file_at_size(filename, 250, 250, NULL);
	g_free(filename);
	
	if(pix) {
		gtk_image_set_from_pixbuf(preview, pix);
		g_object_unref(G_OBJECT(pix));
	}
	xfce_file_chooser_set_preview_widget_active(chooser, (pix != NULL));
}

static void
filebutton_click_cb(GtkWidget *w, gpointer user_data)
{
	DMPlugin *dmp = user_data;
	GtkWidget *chooser, *image;
	gchar *filename;
	XfceFileFilter *filter;
	const gchar *title;
	gboolean is_icon = FALSE;
	
	if(w == dmp->icon_fb)
		is_icon = TRUE;
	
	if(is_icon)
		title = _("Select Icon");
	else
		title = _("Select Menu File");
	
	chooser = xfce_file_chooser_new(title,
			GTK_WINDOW(gtk_widget_get_toplevel(w)),
			XFCE_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	if(is_icon)
		xfce_file_chooser_add_shortcut_folder(XFCE_FILE_CHOOSER(chooser),
				DATADIR "/pixmaps", NULL);
	else
		xfce_file_chooser_add_shortcut_folder(XFCE_FILE_CHOOSER(chooser),
				xfce_get_userdir(), NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(chooser), GTK_RESPONSE_ACCEPT);
	
	filter = xfce_file_filter_new();
	xfce_file_filter_set_name(filter, _("All Files"));
	xfce_file_filter_add_pattern(filter, "*");
	xfce_file_chooser_add_filter(XFCE_FILE_CHOOSER(chooser), filter);
	xfce_file_chooser_set_filter(XFCE_FILE_CHOOSER(chooser), filter);
	filter = xfce_file_filter_new();
	if(is_icon) {
		xfce_file_filter_set_name(filter, _("Image Files"));
		xfce_file_filter_add_pattern(filter, "*.png");
		xfce_file_filter_add_pattern(filter, "*.jpg");
		xfce_file_filter_add_pattern(filter, "*.bmp");
		xfce_file_filter_add_pattern(filter, "*.svg");
		xfce_file_filter_add_pattern(filter, "*.xpm");
		xfce_file_filter_add_pattern(filter, "*.gif");
	} else {
		xfce_file_filter_set_name(filter, _("Menu Files"));
		xfce_file_filter_add_pattern(filter, "*.xml");
	}
	xfce_file_chooser_add_filter(XFCE_FILE_CHOOSER(chooser), filter);
	
	if(is_icon) {
		image = gtk_image_new();
		gtk_widget_show(image);
		xfce_file_chooser_set_preview_widget(XFCE_FILE_CHOOSER(chooser), image);
		xfce_file_chooser_set_preview_callback(XFCE_FILE_CHOOSER(chooser),
				filebutton_update_preview_cb, image);
		xfce_file_chooser_set_preview_widget_active(XFCE_FILE_CHOOSER(chooser), FALSE);
	}

	gtk_widget_show(chooser);
	if(gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
		filename = xfce_file_chooser_get_filename(XFCE_FILE_CHOOSER(chooser));
		if(filename) {
			if(is_icon) {
				gtk_entry_set_text(GTK_ENTRY(dmp->icon_entry), filename);
				entry_focus_out_cb(dmp->icon_entry, NULL, dmp);
			} else {
				gtk_entry_set_text(GTK_ENTRY(dmp->file_entry), filename);
				entry_focus_out_cb(dmp->file_entry, NULL, dmp);
			}
			g_free(filename);
		}
	}
	gtk_widget_destroy(chooser);
}

static void
icon_chk_cb(GtkToggleButton *w, gpointer user_data)
{
	DMPlugin *dmp = user_data;
	
	dmp->show_menu_icons = gtk_toggle_button_get_active(w);
	if(dmp->desktop_menu)
		xfce_desktop_menu_set_show_icons(dmp->desktop_menu, dmp->show_menu_icons);
}

static void
dmp_use_desktop_menu_toggled_cb(GtkToggleButton *tb, gpointer user_data)
{
	DMPlugin *dmp = user_data;
	
	if(gtk_toggle_button_get_active(tb)) {
		GtkWidget *hbox;
		
		dmp->use_default_menu = TRUE;
		
		hbox = g_object_get_data(G_OBJECT(tb), "dmp-child-hbox");
		gtk_widget_set_sensitive(hbox, FALSE);
		
		if(dmp->desktop_menu)
			xfce_desktop_menu_destroy(dmp->desktop_menu);
		dmp->desktop_menu = xfce_desktop_menu_new(NULL, TRUE);
	}
}

static void
dmp_use_custom_menu_toggled_cb(GtkToggleButton *tb, gpointer user_data)
{
	DMPlugin *dmp = user_data;
	
	if(gtk_toggle_button_get_active(tb)) {
		GtkWidget *hbox;
		
		dmp->use_default_menu = FALSE;
		
		hbox = g_object_get_data(G_OBJECT(tb), "dmp-child-hbox");
		gtk_widget_set_sensitive(hbox, TRUE);
		
		if(dmp->menu_file) {
			if(dmp->desktop_menu)
				xfce_desktop_menu_destroy(dmp->desktop_menu);
			dmp->desktop_menu = xfce_desktop_menu_new(dmp->menu_file, TRUE);
		}
	}
}

static gboolean
dmp_button_title_focus_out_cb(GtkWidget *w, GdkEventFocus *evt,
		gpointer user_data)
{
	DMPlugin *dmp = user_data;
	
	if(dmp->button_title)
		g_free(dmp->button_title);
	dmp->button_title = gtk_editable_get_chars(GTK_EDITABLE(w), 0, -1);
	
	gtk_tooltips_set_tip(dmp->tooltip, dmp->button, dmp->button_title, NULL);
	
	return FALSE;
}

static void
dmp_edit_menu_clicked_cb(GtkWidget *w, gpointer user_data)
{
	DMPlugin *dmp = user_data;
	GError *err = NULL;
	const gchar *menu_file;
	gchar cmd[PATH_MAX];
	
	g_return_if_fail(dmp && dmp->desktop_menu);
	
	menu_file = xfce_desktop_menu_get_menu_file(dmp->desktop_menu);
	if(!menu_file)
		return;
	
	g_snprintf(cmd, PATH_MAX, "%s/xfce4-menueditor \"%s\"", BINDIR, menu_file);
	if(xfce_exec(cmd, FALSE, FALSE, NULL))
		return;
	
	g_snprintf(cmd, PATH_MAX, "xfce4-menueditor \"%s\"", menu_file);
	if(!xfce_exec(cmd, FALSE, FALSE, &err)) {
		xfce_warn(_("Unable to launch xfce4-menueditor: %s"), err->message);
		g_error_free(err);
	}
}

static void
dmp_create_options(Control *ctrl, GtkContainer *con, GtkWidget *done)
{
	DMPlugin *dmp = ctrl->data;
	GtkWidget *topvbox, *vbox, *hbox, *frame, *spacer;
	GtkWidget *label, *image, *filebutton, *chk, *radio, *entry, *btn;
	
	xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

	topvbox = gtk_vbox_new(FALSE, BORDER/2);
	gtk_widget_show(topvbox);
	gtk_container_add(con, topvbox);
	
	hbox = gtk_hbox_new(FALSE, BORDER/2);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), BORDER/2);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(topvbox), hbox, FALSE, FALSE, 0);
	
	label = gtk_label_new_with_mnemonic(_("Button _title:"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	
	entry = gtk_entry_new();
	if(dmp->button_title)
		gtk_entry_set_text(GTK_ENTRY(entry), dmp->button_title);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_widget_show(entry);
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(entry), "focus-out-event",
			G_CALLBACK(dmp_button_title_focus_out_cb), dmp);	
	
	frame = xfce_framebox_new(_("Menu File"), TRUE);
	gtk_widget_show(frame);
	gtk_box_pack_start(GTK_BOX(topvbox), frame, FALSE, FALSE, 0);
	
	vbox = gtk_vbox_new(FALSE, BORDER/2);
	gtk_widget_show(vbox);
	xfce_framebox_add(XFCE_FRAMEBOX(frame), vbox);
	
	/* 2nd radio button's child hbox */
	hbox = gtk_hbox_new(FALSE, BORDER/2);
	gtk_widget_show(hbox);
	
	radio = gtk_radio_button_new_with_mnemonic(NULL, _("Use default _desktop menu file"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), dmp->use_default_menu);
	gtk_widget_show(radio);
	gtk_box_pack_start(GTK_BOX(vbox), radio, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(radio), "toggled",
			G_CALLBACK(dmp_use_desktop_menu_toggled_cb), dmp);
	g_object_set_data(G_OBJECT(radio), "dmp-child-hbox", hbox);
	
	radio = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(radio),
			_("Use _custom menu file:"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), !dmp->use_default_menu);
	gtk_widget_show(radio);
	gtk_box_pack_start(GTK_BOX(vbox), radio, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(radio), "toggled",
			G_CALLBACK(dmp_use_custom_menu_toggled_cb), dmp);
	g_object_set_data(G_OBJECT(radio), "dmp-child-hbox", hbox);
	
	/* now pack in the child hbox */
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	
	spacer = gtk_alignment_new(0.5, 0.5, 1, 1);
	gtk_widget_show(spacer);
	gtk_box_pack_start(GTK_BOX(hbox), spacer, FALSE, FALSE, 0);
	gtk_widget_set_size_request(spacer, 16, -1);
	
	dmp->file_entry = gtk_entry_new();
	if(dmp->menu_file)
		gtk_entry_set_text(GTK_ENTRY(dmp->file_entry), dmp->menu_file);
	else if(dmp->desktop_menu) {
		dmp->menu_file = g_strdup(xfce_desktop_menu_get_menu_file(dmp->desktop_menu));
		gtk_entry_set_text(GTK_ENTRY(dmp->file_entry), dmp->menu_file);
	}
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), dmp->file_entry);
	gtk_widget_set_size_request(dmp->file_entry, 325, -1);  /* FIXME */
	gtk_widget_show(dmp->file_entry);
	gtk_box_pack_start(GTK_BOX(hbox), dmp->file_entry, TRUE, TRUE, 3);
	g_signal_connect(G_OBJECT(dmp->file_entry), "focus-out-event",
			G_CALLBACK(entry_focus_out_cb), dmp);
	
	image = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
	gtk_widget_show(image);
	
	dmp->file_fb = filebutton = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(filebutton), image);
	gtk_widget_show(filebutton);
	gtk_box_pack_end(GTK_BOX(hbox), filebutton, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(filebutton), "clicked",
			G_CALLBACK(filebutton_click_cb), dmp);
	
	gtk_widget_set_sensitive(hbox, !dmp->use_default_menu);
	
	spacer = gtk_alignment_new(0.5, 0.5, 1, 1);
	gtk_widget_show(spacer);
	gtk_box_pack_start(GTK_BOX(vbox), spacer, FALSE, FALSE, 0);
	gtk_widget_set_size_request(spacer, -1, 4);
	
	hbox = gtk_hbox_new(FALSE, BORDER/2);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	
#if GTK_CHECK_VERSION(2, 6, 0)
	btn = xfutil_custom_button_new(_("_Edit Menu"), GTK_STOCK_EDIT);
#else
	btn = gtk_button_new_with_mnemonic(_("_Edit Menu"));
#endif
	gtk_widget_show(btn);
	gtk_box_pack_end(GTK_BOX(hbox), btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(btn), "clicked",
			G_CALLBACK(dmp_edit_menu_clicked_cb), dmp);
	
	frame = xfce_framebox_new(_("Icons"), TRUE);
	gtk_widget_show(frame);
	gtk_box_pack_start(GTK_BOX(topvbox), frame, FALSE, FALSE, 0);
	
	vbox = gtk_vbox_new(FALSE, BORDER/2);
	gtk_widget_show(vbox);
	xfce_framebox_add(XFCE_FRAMEBOX(frame), vbox);
	
	hbox = gtk_hbox_new(FALSE, BORDER/2);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	
	label = gtk_label_new_with_mnemonic(_("_Button icon:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	
	dmp->icon_entry = gtk_entry_new();
	if(dmp->icon_file)
		gtk_entry_set_text(GTK_ENTRY(dmp->icon_entry), dmp->icon_file);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), dmp->icon_entry);
	gtk_widget_show(dmp->icon_entry);
	gtk_box_pack_start(GTK_BOX(hbox), dmp->icon_entry, TRUE, TRUE, 3);
	g_signal_connect(G_OBJECT(dmp->icon_entry), "focus-out-event",
			G_CALLBACK(entry_focus_out_cb), dmp);
	
	image = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
	gtk_widget_show(image);
	
	dmp->icon_fb = filebutton = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(filebutton), image);
	gtk_widget_show(filebutton);
	gtk_box_pack_end(GTK_BOX(hbox), filebutton, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(filebutton), "clicked",
			G_CALLBACK(filebutton_click_cb), dmp);
	
	dmp->icons_chk = chk = gtk_check_button_new_with_mnemonic(_("Show _icons in menu"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk), dmp->show_menu_icons);
	gtk_widget_show(chk);
	gtk_box_pack_start(GTK_BOX(vbox), chk, FALSE, FALSE, BORDER/2);
	g_signal_connect(G_OBJECT(chk), "toggled", G_CALLBACK(icon_chk_cb), dmp);
}

G_MODULE_EXPORT void
xfce_control_class_init(ControlClass *cc)
{
	cc->name = "xfce-menu";
	
	xfce_textdomain(GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");
	cc->caption = _("Xfce Menu");
	
	cc->create_control = (CreateControlFunc)dmp_create;
	cc->read_config = dmp_read_config;
	cc->write_config = dmp_write_config;
	cc->create_options = dmp_create_options;
	cc->free = dmp_free;
	cc->attach_callback = dmp_attach_callback;
	cc->set_size = dmp_set_size;
	cc->set_orientation = NULL;
}

XFCE_PLUGIN_CHECK_INIT
