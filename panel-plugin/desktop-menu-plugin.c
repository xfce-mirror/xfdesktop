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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4util/i18n.h>
#include <libxfcegui4/libxfcegui4.h>
#include <gmodule.h>

#include <panel/plugins.h>
#include <panel/xfce.h>

#include "desktop-menu-stub.h"

#define BORDER 8
#define DEFAULT_BUTTON_ICON  DATADIR "/pixmaps/xfce4_xicon1.png"

typedef struct _DMPlugin {
	GtkWidget *evtbox;
	GtkWidget *button;
	XfceDesktopMenu *desktop_menu;
	gchar *menu_file;
	gchar *icon_file;
	gboolean show_menu_icons;
	
	GtkWidget *file_entry;
	GtkWidget *file_fb;
	GtkWidget *icon_entry;
	GtkWidget *icon_fb;
	GtkWidget *icons_chk;
	GtkTooltips *tooltip;  /* needed? */
} DMPlugin;

static GModule *menu_gmod = NULL;

static void
dmp_set_size(Control *c, int size)
{
	DMPlugin *dmp = c->data;
	GdkPixbuf *pix;

	if(dmp->icon_file) {
		pix = xfce_themed_icon_load(dmp->icon_file,
				icon_size[settings.size] - 2*border_width);
		if(pix) {
			xfce_iconbutton_set_pixbuf(XFCE_ICONBUTTON(dmp->button), pix);
			g_object_unref(G_OBJECT(pix));
		}
	}
}

static void
dmp_attach_callback(Control *c, const char *signal, GCallback callback,
		gpointer data)
{
	DMPlugin *dmp = c->data;
	
	g_signal_connect(G_OBJECT(dmp->evtbox), signal, callback, data);
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
	
	g_free(dmp);
}

static gboolean
dmp_popup(GtkWidget *w, GdkEventButton *evt, gpointer user_data)
{
	GtkWidget *menu;
	DMPlugin *dmp = user_data;
	
	if(evt->button != 1)
		return FALSE;
	
	if(!menu_gmod)
		return FALSE;
	
	g_return_val_if_fail(dmp != NULL && dmp->desktop_menu != NULL, FALSE);
	
	if(xfce_desktop_menu_need_update(dmp->desktop_menu))
		xfce_desktop_menu_force_regen(dmp->desktop_menu);

	menu = xfce_desktop_menu_get_widget(dmp->desktop_menu);
	if(menu) {
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, evt->button,
				evt->time);
	}
	
	return TRUE;
}

static DMPlugin *
dmp_new()
{
	DMPlugin *dmp = g_new0(DMPlugin, 1);
	
	dmp->show_menu_icons = TRUE;  /* default */
	dmp->tooltip = gtk_tooltips_new();

	dmp->evtbox = gtk_event_box_new();
	gtk_container_set_border_width(GTK_CONTAINER(dmp->evtbox), 0);
	gtk_widget_show(dmp->evtbox);
	
	dmp->button = xfce_iconbutton_new();
	gtk_button_set_relief(GTK_BUTTON(dmp->button), GTK_RELIEF_NONE);
	gtk_container_set_border_width(GTK_CONTAINER(dmp->button), 0);
	gtk_widget_show(dmp->button);
	gtk_container_add(GTK_CONTAINER(dmp->evtbox), dmp->button);
	gtk_tooltips_set_tip(dmp->tooltip, dmp->button, _("Xfce Menu"), NULL);
	
	dmp->desktop_menu = xfce_desktop_menu_new(NULL, TRUE);
	xfce_desktop_menu_start_autoregen(dmp->desktop_menu, 10);
	g_signal_connect(G_OBJECT(dmp->button), "button-press-event",
			G_CALLBACK(dmp_popup), dmp);
	
	dmp->icon_file = g_strdup(DEFAULT_BUTTON_ICON);
	
	return dmp;
}

static gboolean
dmp_create(Control *c)
{
	DMPlugin *dmp;
	
	if(!menu_gmod)
		return FALSE;

	dmp = dmp_new();
	gtk_container_add(GTK_CONTAINER(c->base), dmp->evtbox);
	gtk_widget_set_size_request(c->base, -1, -1);
	
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
	
	value = xmlGetProp(node, (const xmlChar *)"menu_file");
	if(value) {
		if(dmp->desktop_menu)
			xfce_desktop_menu_destroy(dmp->desktop_menu);
		dmp->desktop_menu = xfce_desktop_menu_new(value, TRUE);
		if(dmp->menu_file)
			g_free(dmp->menu_file);
		dmp->menu_file = g_strdup(value);
	}		
	
	value = xmlGetProp(node, (const xmlChar *)"icon_file");
	if(value) {
		pix = xfce_themed_icon_load(value,
				icon_size[settings.size] - 2*border_width);
		if(pix) {
			if(dmp->icon_file)
				g_free(dmp->icon_file);
			dmp->icon_file = (gchar *)value;
			xfce_iconbutton_set_pixbuf(XFCE_ICONBUTTON(dmp->button), pix);
			g_object_unref(G_OBJECT(pix));
		} else
			xmlFree(value);
	} else {
		dmp->icon_file = g_strdup(DEFAULT_BUTTON_ICON);
		pix = xfce_themed_icon_load(dmp->icon_file,
				icon_size[settings.size] - 2*border_width);
		if(pix) {
			xfce_iconbutton_set_pixbuf(XFCE_ICONBUTTON(dmp->button), pix);
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
}

static void
dmp_write_config(Control *control, xmlNodePtr node)
{
	DMPlugin *dmp = control->data;
	
	xmlSetProp(node, (const xmlChar *)"menu_file", dmp->menu_file ? dmp->menu_file : "");
	xmlSetProp(node, (const xmlChar *)"icon_file", dmp->icon_file ? dmp->icon_file : "");
	xmlSetProp(node, (const xmlChar *)"show_menu_icons", dmp->show_menu_icons ? "1" : "0");
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
			xfce_iconbutton_set_pixbuf(XFCE_ICONBUTTON(dmp->button), pix);
			g_object_unref(G_OBJECT(pix));
		} else
			xfce_iconbutton_set_pixbuf(XFCE_ICONBUTTON(dmp->button), NULL);
	} else if(w == dmp->file_entry) {
		if(dmp->menu_file)
			g_free(dmp->menu_file);
		
		dmp->menu_file = gtk_editable_get_chars(GTK_EDITABLE(w), 0, -1);
		cur_file = xfce_desktop_menu_get_menu_file(dmp->desktop_menu);
		if(strcmp(dmp->menu_file, cur_file)) {
			xfce_desktop_menu_destroy(dmp->desktop_menu);
			dmp->desktop_menu = xfce_desktop_menu_new(dmp->menu_file, TRUE);
			if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dmp->icons_chk)))
				xfce_desktop_menu_set_show_icons(dmp->desktop_menu, FALSE);
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
dmp_create_options(Control *ctrl, GtkContainer *con, GtkWidget *done)
{
	DMPlugin *dmp = ctrl->data;
	GtkWidget *vbox, *hbox;
	GtkWidget *label, *image, *filebutton, *chk;
	GtkSizeGroup *sg;
	const gchar *menu_filename;
	
	vbox = gtk_vbox_new(FALSE, BORDER);
	gtk_widget_show(vbox);
	gtk_container_add(con, vbox);
	
	hbox = gtk_hbox_new(FALSE, BORDER);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	
	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	
	label = gtk_label_new_with_mnemonic(_("_Menu file:"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_size_group_add_widget(sg, label);
	
	dmp->file_entry = gtk_entry_new();
	menu_filename = xfce_desktop_menu_get_menu_file(dmp->desktop_menu);
	gtk_entry_set_text(GTK_ENTRY(dmp->file_entry), menu_filename);
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
	
	hbox = gtk_hbox_new(FALSE, BORDER);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	
	label = gtk_label_new_with_mnemonic(_("_Button icon:"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_size_group_add_widget(sg, label);
	
	dmp->icon_entry = gtk_entry_new();
	if(dmp->icon_file)
		gtk_entry_set_text(GTK_ENTRY(dmp->icon_entry), dmp->icon_file);
	gtk_widget_set_size_request(dmp->icon_entry, 250, -1);  /* FIXME */
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
	gtk_box_pack_start(GTK_BOX(vbox), chk, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(chk), "toggled", G_CALLBACK(icon_chk_cb), dmp);
}

G_MODULE_EXPORT void
xfce_control_class_init(ControlClass *cc)
{
	cc->name = "xfce-menu";
	cc->caption = _("Xfce Menu");
	
	cc->create_control = (CreateControlFunc)dmp_create;
	cc->read_config = dmp_read_config;
	cc->write_config = dmp_write_config;
	cc->create_options = dmp_create_options;
	cc->free = dmp_free;
	cc->attach_callback = dmp_attach_callback;
	cc->set_size = dmp_set_size;
	cc->set_orientation = NULL;
	
	menu_gmod = xfce_desktop_menu_stub_init();
}

G_MODULE_EXPORT void
g_module_unload(GModule *module)
{
	xfce_desktop_menu_stub_cleanup(menu_gmod);
}

XFCE_PLUGIN_CHECK_INIT
