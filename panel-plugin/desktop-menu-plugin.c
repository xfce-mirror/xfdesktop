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
	GtkWidget *evtbox;
	GtkWidget *button;
	GdkPixbuf *icon;
	XfceDesktopMenu *desktop_menu;
} DMPlugin;

static GModule *menu_gmod = NULL;

static void
dmp_set_size(Control *c, int size)
{
	DMPlugin *dmp = c->data;
	gint w;
	GdkPixbuf *tmp;
	GtkWidget *img;
	
	/* if we have one, see if the size is ok */
	if(dmp->icon) {
		w = gdk_pixbuf_get_width(dmp->icon);
		if(w != icon_size[settings.size]) {
			gtk_container_remove(GTK_CONTAINER(dmp->button),
					gtk_bin_get_child(GTK_BIN(dmp->button)));
			g_object_unref(G_OBJECT(dmp->icon));
			dmp->icon = NULL;
		}
	}
	
	if(!dmp->icon) {
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
			img = gtk_image_new_from_pixbuf(dmp->icon);
			gtk_widget_show(img);
			gtk_container_add(GTK_CONTAINER(dmp->button), img);
		}
	}
	
	if(!dmp->icon) {
		/* if we still don't have one, just set some text */
		gtk_button_set_label(GTK_BUTTON(dmp->button), "Xfce4");
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
	
	if(!menu_gmod)
		return;
	
	g_return_if_fail(dmp != NULL && dmp->desktop_menu != NULL);
	
	if(xfce_desktop_menu_need_update(dmp->desktop_menu))
		xfce_desktop_menu_force_regen(dmp->desktop_menu);

	menu = xfce_desktop_menu_get_widget(dmp->desktop_menu);
	if(menu) {
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0,
				gtk_get_current_event_time());
	}
	
	return TRUE;
}

static DMPlugin *
dmp_new()
{
	DMPlugin *dmp = g_new0(DMPlugin, 1);

	dmp->evtbox = gtk_event_box_new();
	gtk_container_set_border_width(GTK_CONTAINER(dmp->evtbox), 3);
	gtk_widget_show(dmp->evtbox);
	
	dmp->button = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(dmp->button), GTK_RELIEF_NONE);
	gtk_widget_show(dmp->button);
	gtk_container_add(GTK_CONTAINER(dmp->evtbox), dmp->button);
	
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

G_MODULE_EXPORT void
xfce_control_class_init(ControlClass *cc)
{
	cc->name = "desktop-menu";
	cc->caption = _("Desktop Menu");
	
	cc->create_control = (CreateControlFunc)dmp_create;
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
