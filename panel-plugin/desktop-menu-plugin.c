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

#include <time.h>

#include <libxfce4util/i18n.h>
#include <gmodule.h>

#include <panel/plugins.h>
#include <panel/xfce.h>

#include "desktop-menu-stub.h"

typedef struct _DMPlugin {
	/* panel widget */
	GtkWidget *evtbox;
	GtkWidget *button;
	GdkPixbuf *icon;
	GtkWidget *image;
	GtkWidget *label;
	XfceDesktopMenu *desktop_menu;

	/* prefs pane */
	gboolean show_icon;
	gboolean show_text;
} DMPlugin;

static GModule *menu_gmod = NULL;

static void
dmp_set_size(Control *c, int size)
{
	DMPlugin *dmp = c->data;
	gint w;
	GdkPixbuf *tmp;

	/* if we have one, see if the size is ok */
	if(dmp->icon) {
		w = gdk_pixbuf_get_width(dmp->icon);
		if(w == settings.size)
			return;
	}
	
	/* either the size wasn't ok, or we didn't have an icon */
	tmp = gdk_pixbuf_new_from_file(DATADIR "/pixmaps/xfce4_xicon.png", NULL);
	if(tmp) {
		w = gdk_pixbuf_get_width(tmp);
		if(w != icon_size[settings.size]) {
			dmp->icon = gdk_pixbuf_scale_simple(tmp,
					icon_size[settings.size], icon_size[settings.size],
					GDK_INTERP_BILINEAR);
			g_object_unref(G_OBJECT(tmp));
		} else
			dmp->icon = tmp;
		gtk_image_set_from_pixbuf(GTK_IMAGE(dmp->image), dmp->icon);
		g_object_unref(G_OBJECT(dmp->icon));
	}
	
	if(!dmp->icon) {
		/* if we still don't have one, do the text thing */
		dmp->show_icon = FALSE;
		dmp->show_text = TRUE;
		gtk_widget_hide(dmp->image);
		gtk_widget_show(dmp->label);
	}
	
	gtk_widget_set_size_request(dmp->button, -1, -1);
}

static void
dmp_attach_callback(Control *c, const char *signal, GCallback callback,
		gpointer data)
{
	DMPlugin *dmp = c->data;
	
	g_signal_connect(G_OBJECT(dmp->evtbox), signal, callback, data);
	g_signal_connect(G_OBJECT(dmp->button), signal, callback, data);
	g_signal_connect(G_OBJECT(dmp->label), signal, callback, data);
}

static void
dmp_free(Control *c)
{
	DMPlugin *dmp = c->data;
	
	if(dmp->desktop_menu)
		xfce_desktop_menu_destroy(dmp->desktop_menu);
	
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
		return;
	
	g_return_if_fail(dmp != NULL && dmp->desktop_menu != NULL);
	
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
	GtkWidget *hbox;
	DMPlugin *dmp = g_new0(DMPlugin, 1);

	dmp->evtbox = gtk_event_box_new();
	gtk_container_set_border_width(GTK_CONTAINER(dmp->evtbox), 3);
	gtk_widget_show(dmp->evtbox);
	
	dmp->button = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(dmp->button), GTK_RELIEF_NONE);
	gtk_widget_show(dmp->button);
	gtk_container_add(GTK_CONTAINER(dmp->evtbox), dmp->button);
	
	hbox = gtk_hbox_new(FALSE, 3);
	gtk_widget_show(hbox);
	gtk_container_add(GTK_CONTAINER(dmp->button), hbox);
	
	dmp->image = gtk_image_new();
	gtk_box_pack_start(GTK_BOX(hbox), dmp->image, TRUE, TRUE, 0);
	
	dmp->label = gtk_label_new("Xfce4");
	gtk_box_pack_start(GTK_BOX(hbox), dmp->label, TRUE, TRUE, 0);
	
	dmp->desktop_menu = xfce_desktop_menu_new(NULL, TRUE);
	xfce_desktop_menu_start_autoregen(dmp->desktop_menu, 10);
	g_signal_connect(G_OBJECT(dmp->button), "button-press-event",
			G_CALLBACK(dmp_popup), dmp);

	return dmp;
}

gboolean
dmp_create(Control *c)
{
	DMPlugin *dmp;
	
	if(!menu_gmod) {
		menu_gmod = xfce_desktop_menu_stub_init();
		if(!menu_gmod)
			return FALSE;
	}

	dmp = dmp_new();
	gtk_container_add(GTK_CONTAINER(c->base), dmp->evtbox);
	gtk_widget_set_size_request(c->base, -1, -1);
	
	c->data = (gpointer)dmp;
	c->with_popup = FALSE;
	
	return TRUE;
}

/* settings */
static void
read_config(Control *control, xmlNodePtr node)
{
	xmlChar *value;
	DMPlugin *dmp = control->data;
	
	value = xmlGetProp(node, (const xmlChar *)"show_icon");
	
	if(value) {
		if(!xmlStrcmp(value, (xmlChar*)"true")){
			dmp->show_icon = TRUE;
			gtk_widget_show(dmp->image);
		} else {
			dmp->show_icon = FALSE;
			gtk_widget_hide(dmp->image);
		}
		xmlFree(value);
	}
	
	value = xmlGetProp(node, (const xmlChar *)"show_text");
	
	if(value) {
		if(!xmlStrcmp(value, (xmlChar*)"true")){
			dmp->show_text = TRUE;
			gtk_widget_show(dmp->label);
		} else { 
			dmp->show_text = FALSE;
			gtk_widget_hide(dmp->label);
		}
		xmlFree(value);
	}
	
	if(!dmp->show_text && !dmp->show_icon) {
		/* default to icon, unless we don't have one */
		if(dmp->icon) {
			dmp->show_icon = TRUE;
			gtk_widget_show(dmp->image);
		} else {
			dmp->show_text = TRUE;
			gtk_widget_show(dmp->label);
		}
	}
}

static void
write_config(Control *control, xmlNodePtr node)
{
	DMPlugin *dmp = control->data;
	
	xmlSetProp(node, (const xmlChar *)"show_icon", dmp->show_icon ? "true" : "false");
	xmlSetProp(node, (const xmlChar *)"show_text", dmp->show_text ? "true" : "false");
}

/* options dialog */
static void
checkbutton_icon_changed(GtkWidget *w, DMPlugin *dmp)
{
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
		dmp->show_icon = TRUE;
		gtk_widget_show(dmp->image);
	} else {
		dmp->show_icon = FALSE;
		gtk_widget_hide(dmp->image);
	}
}

static void
checkbutton_text_changed(GtkWidget *w, DMPlugin *dmp)
{
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
		dmp->show_text = TRUE;
		gtk_widget_show(dmp->label);
	} else {
		dmp->show_text = FALSE;
		gtk_widget_hide(dmp->label);
	}
}

static void
create_options(Control *ctrl, GtkContainer *con, GtkWidget *done)
{
	DMPlugin *dmp = ctrl->data;
	GtkWidget *vbox;
	GtkWidget *checkbutton_icon, *checkbutton_text;
	
	vbox  = gtk_vbox_new(FALSE, 8);
	
	checkbutton_icon = gtk_check_button_new_with_mnemonic(_("Show _icon"));
	checkbutton_text = gtk_check_button_new_with_mnemonic(_("Show _text"));
	
	gtk_box_pack_start(GTK_BOX(vbox), checkbutton_icon, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), checkbutton_text, FALSE, FALSE, 0);
	
	/* Connect signals */
	g_signal_connect(checkbutton_icon, "toggled",
			G_CALLBACK(checkbutton_icon_changed), dmp);
	g_signal_connect(checkbutton_text, "toggled",
			G_CALLBACK(checkbutton_text_changed), dmp);
	
	/* Show in container */
	gtk_widget_show_all(vbox);
	gtk_container_add(con, vbox);
	
	/* Set active or not */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton_icon),
			dmp->show_icon);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton_text),
			dmp->show_text);

	if(!dmp->show_icon && !dmp->show_text) {
		/* default to icon, unless we don't have one */
		if(dmp->icon) {
			dmp->show_icon = TRUE;
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton_icon), TRUE);
			gtk_widget_show(dmp->image);
		} else {
			dmp->show_text = TRUE;
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton_text), TRUE);
			gtk_widget_show(dmp->label);
		}
	}
}

G_MODULE_EXPORT void
xfce_control_class_init(ControlClass *cc)
{
	cc->name = "desktop-menu";
	cc->caption = _("Desktop Menu");
	
	cc->create_control = (CreateControlFunc)dmp_create;
	cc->read_config = read_config;
	cc->write_config = write_config;
	cc->create_options = create_options;
	cc->free = dmp_free;
	cc->attach_callback = dmp_attach_callback;
	cc->set_size = dmp_set_size;
	cc->set_orientation = NULL;
}

G_MODULE_EXPORT void
g_module_unload(GModule *module)
{
	xfce_desktop_menu_stub_cleanup_all(menu_gmod);
}

XFCE_PLUGIN_CHECK_INIT
