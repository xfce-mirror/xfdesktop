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
#include "menu.h"

static NetkScreen *netk_screen = NULL;

static void do_help(void)
{
}

static void do_run(void)
{
    g_spawn_command_line_async("xfrun4", NULL);
}

static void do_xterm(void)
{
    g_spawn_command_line_async("xterm", NULL);
}

static void settings_mgr(void)
{
    g_spawn_command_line_async("xfce-setting-show", NULL);
}

static GtkItemFactoryEntry main_items[] = {
  { N_("/Desktop menu"), NULL, NULL,        0, "<Title>" },
  { "/sep",              NULL, NULL,        0, "<Separator>" },
  { N_("/X _Terminal"), NULL, do_xterm,  0, "<Item>" },
  { N_("/_Run program ..."), NULL, do_run,  0, "<Item>" },
  { N_("/_Settings manager"), NULL, settings_mgr,  0, "<Item>" },
  { "/sep",              NULL, NULL,        0, "<Separator>" },
  { N_("/_Help"),        NULL, do_help,     0, "<Item>" },
#if 0
  { "/sep",              NULL, NULL,        0, "<Separator>" },
  { N_("/E_xit"),        NULL, quit,        0, "<Item>" },
#endif
};

static gboolean popup_menu(GdkEventButton *ev);

static gboolean button_press_event(GtkWidget *win, GdkEventButton *ev, 
				gpointer data)
{
    return popup_menu(ev);
}

void menu_init(GtkWidget *window, NetkScreen *screen)
{
    netk_screen = screen;

    g_signal_connect(window, "button-press-event",
		     G_CALLBACK(button_press_event), NULL);
}

void menu_load_settings(McsClient *client)
{
}

void add_menu_callback(GHashTable *ht)
{
}

static void activate_window(GtkWidget *item, NetkWindow *win)
{
    netk_window_activate(win);
}

static void set_num_screens(gpointer num)
{
    static Atom xa_NET_NUMBER_OF_DESKTOPS = 0;
    XClientMessageEvent sev;
    int n;

    if(!xa_NET_NUMBER_OF_DESKTOPS)
    {
	xa_NET_NUMBER_OF_DESKTOPS = 
	    XInternAtom(GDK_DISPLAY(), "_NET_NUMBER_OF_DESKTOPS", False);
    }

    n = GPOINTER_TO_INT(num);
    
    sev.type = ClientMessage;
    sev.display = GDK_DISPLAY();
    sev.format = 32;
    sev.window = GDK_ROOT_WINDOW();
    sev.message_type = xa_NET_NUMBER_OF_DESKTOPS;
    sev.data.l[0] = n;

    gdk_error_trap_push();

    XSendEvent(GDK_DISPLAY(), GDK_ROOT_WINDOW(), False,
               SubstructureNotifyMask | SubstructureRedirectMask,
               (XEvent *) & sev);

    gdk_flush();
    gdk_error_trap_pop();
}

static GtkWidget *create_window_list_item(NetkWindow *win)
{
    const char *name = NULL;
    GString *label;
    GtkWidget *mi;

    if (netk_window_is_skip_pager(win) || netk_window_is_skip_tasklist(win))
	return NULL;
    
    if (!name)
	name = netk_window_get_name(win);
   
    label = g_string_new(name);

    if (label->len >= 15) {
    	g_string_truncate(label, 15);
	g_string_append(label, " ...");
    }

    if (netk_window_is_minimized(win)) {
	g_string_prepend(label, "[");
	g_string_append(label, "]");
    }
	
    mi = gtk_menu_item_new_with_label(label->str);
    
    g_string_free(label,TRUE);
    
    return mi;
}

static  GtkWidget *create_windowlist_menu(void)
{
    int i, n;
    GList *windows, *li;
    GtkWidget *menu3, *mi, *label;
    NetkWindow *win;
    NetkWorkspace *ws, *aws;
    GtkStyle *style;
    
    menu3 = gtk_menu_new();
    style = gtk_widget_get_style(menu3);
    
/*	mi = gtk_menu_item_new_with_label(_("Window list"));
    gtk_widget_set_sensitive(mi, FALSE);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);

    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);
*/	
    windows = netk_screen_get_windows_stacked(netk_screen);
    n = netk_screen_get_workspace_count(netk_screen);
    aws = netk_screen_get_active_workspace(netk_screen);

    for (i = 0; i < n; i++)
    {
	char ws_name[100];
	gboolean active;
	
	ws = netk_screen_get_workspace(netk_screen, i);
	
	active = (ws == aws);
	
	if (active)
	{
	    sprintf(ws_name, "<i>%s %d</i>", _("Workspace"), i+1);
	}
	else
	{
	    sprintf(ws_name, "%s <i>%s %d</i>", 
		    _("Go to"), _("Workspace"), i+1);
	}
	
	mi = gtk_menu_item_new_with_label(ws_name);
	label = gtk_bin_get_child(GTK_BIN(mi));
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_widget_show(mi);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);
	
	if (active)
	{
	    gtk_widget_set_sensitive(mi, FALSE);
	}
/*	    else
	{
	    gtk_widget_modify_fg(gtk_bin_get_child(GTK_BIN(mi)), 
				 GTK_STATE_NORMAL,
				 &(style->fg[GTK_STATE_INSENSITIVE]));
	}
*/	    
	g_signal_connect_swapped(mi, "activate", 
				 G_CALLBACK(netk_workspace_activate), ws);

	mi = gtk_separator_menu_item_new();
	gtk_widget_show(mi);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);
	
	for (li = windows; li; li = li->next)
	{
	    win = li->data;

	    if (netk_window_get_workspace(win) != ws)
		continue;
	    
	    mi = create_window_list_item(win);

	    if (!mi)
		continue;

	    gtk_widget_show(mi);
	    gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);

	    if (!active)
	    {
		gtk_widget_modify_fg(gtk_bin_get_child(GTK_BIN(mi)), 
				     GTK_STATE_NORMAL,
				     &(style->fg[GTK_STATE_INSENSITIVE]));
	    }
	    
	    g_signal_connect(mi, "activate", 
			     G_CALLBACK(activate_window), win);
	}

	mi = gtk_separator_menu_item_new();
	gtk_widget_show(mi);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);
    }

    mi = gtk_menu_item_new_with_label(_("Add workspace"));
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);
    g_signal_connect_swapped(mi, "activate", 
			     G_CALLBACK(set_num_screens), 
			     GINT_TO_POINTER(n+1));

    mi = gtk_menu_item_new_with_label(_("Delete workspace"));
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);
    g_signal_connect_swapped(mi, "activate", 
			     G_CALLBACK(set_num_screens), 
			     GINT_TO_POINTER(n-1));

    return menu3;
}

static GtkWidget *create_desktop_menu(void)
{
    GtkItemFactory *ifactory;

    ifactory = gtk_item_factory_new(GTK_TYPE_MENU, "<popup>", NULL);

    gtk_item_factory_create_items(ifactory, G_N_ELEMENTS(main_items), 
				  main_items, NULL);

    return gtk_item_factory_get_widget(ifactory, "<popup>");
}

gboolean popup_menu(GdkEventButton *ev)
{
    static GtkWidget *menu1 = NULL;
    static GtkWidget *menu3 = NULL;

    if (ev->button == 3 || 
	    (ev->button == 1 && ev->state & GDK_SHIFT_MASK))
    {
	if(menu3)
	{
	    gtk_widget_destroy(menu3);
	}
	
	menu3 = create_windowlist_menu();
	
	gtk_menu_popup(GTK_MENU(menu3), NULL, NULL, NULL, NULL, 1, ev->time);
	
	return TRUE;
    }
    else if (ev->button == 1)
    {
	if (!menu1)
	{
	    menu1 = create_desktop_menu();
	}

	gtk_menu_popup(GTK_MENU(menu1), NULL, NULL, NULL, NULL, 1, ev->time);

	return TRUE;
    }

    return FALSE;
}

