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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <gdk/gdkx.h>

#include <libxfcegui4/libxfcegui4.h>

#include "settings_common.h"
#include "workspaces_settings.h"
#include "workspaces-icon.h"

#define PLUGIN_NAME "workspaces"
#define RCFILE "workspaces.xml"
#define ICON_SIZE 48
#define MAX_COUNT 32

static NetkScreen *netk_screen = NULL;
static int ws_count = 1;
static char **ws_names = NULL;

static void run_dialog(McsPlugin *plugin);
static void create_workspaces_channel(McsManager *manager);
static void save_workspaces_channel(McsManager *manager);
static void set_workspace_count(McsManager *manager, int count);
static void set_workspace_names(McsManager *manager, char **names);
static void watch_workspaces_hint(McsManager *manager);

McsPluginInitResult mcs_plugin_init(McsPlugin *mcs_plugin)
{
#ifdef ENABLE_NLS
    /* This is required for UTF-8 at least - Please don't remove it */
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

    netk_screen = netk_screen_get_default();
    netk_screen_force_update(netk_screen);
    
    mcs_plugin->plugin_name = g_strdup(PLUGIN_NAME);
    mcs_plugin->caption = g_strdup(_("Desktop: workspaces"));
    mcs_plugin->run_dialog = run_dialog;
    mcs_plugin->icon = inline_icon_at_size(workspaces_icon_data, 
					   ICON_SIZE, ICON_SIZE);

    create_workspaces_channel(mcs_plugin->manager);
    
    watch_workspaces_hint(mcs_plugin->manager);

    return MCS_PLUGIN_INIT_OK;
}

/* very useful functions */
static int array_size(char **array)
{
    char **p;
    int len = 0;

    for (p = array; p && *p; p++)
	len++;

    return len;
}

static void update_names(McsManager *manager, int n)
{
    char **tmpnames;
    int i, len;

    len = array_size(ws_names);
    
    tmpnames = g_new(char*, n + 1);
    tmpnames[n] = NULL;

    for (i = 0; i < n; i++)
    {
	if (i < len)
	    tmpnames[i] = g_strdup(ws_names[i]);
	else
	{
	    const char *name;
	    NetkWorkspace *ws = netk_screen_get_workspace(netk_screen,i);

	    name = netk_workspace_get_name(ws);

	    if (name && strlen(name))
	    {
		tmpnames[i] = g_strdup(name);
	    }
	    else
	    {
		char num[4];

		snprintf(num, 3, "%d", i+1);
		tmpnames[i] = g_strdup(num);
	    }
	}
    }

    g_strfreev(ws_names);
    ws_names = tmpnames;

    set_workspace_names(manager, ws_names);
}

/* create the channel and initialize settings */
static void set_ws_count_hint(int count)
{
    gdk_error_trap_push();
    gdk_property_change(gdk_get_default_root_window(), 
	    		gdk_atom_intern("_NET_NUMBER_OF_DESKTOPS", FALSE),
			gdk_x11_xatom_to_atom(XA_CARDINAL),
			32, GDK_PROP_MODE_REPLACE, 
			(unsigned char *)&count, 1);
    gdk_flush();
    gdk_error_trap_pop();
}

static void create_workspaces_channel(McsManager *manager)
{
    McsSetting *setting;
    int len, n;

    create_channel(manager, WORKSPACES_CHANNEL, RCFILE);

    /* ws count */
    ws_count = netk_screen_get_workspace_count(netk_screen);
    
    setting = mcs_manager_setting_lookup(manager, "count", WORKSPACES_CHANNEL);

    if (setting)
    {
	ws_count = setting->data.v_int;

	/* only the first time when wm isn't running yet */
	set_ws_count_hint(ws_count);
    }

    /* ws names */
    setting = mcs_manager_setting_lookup(manager, "names", WORKSPACES_CHANNEL);

    if (setting)
    {
	ws_names = g_strsplit(setting->data.v_string, SEP_S, -1);
    }

    len = (ws_names) ? array_size(ws_names) : 0;
    n = (len > ws_count) ? len : ws_count;
    
    update_names(manager, n);

    save_workspaces_channel(manager);
}

/* save the channel to file */
static void save_workspaces_channel(McsManager *manager)
{
    save_channel(manager, WORKSPACES_CHANNEL, RCFILE);
}

/* changing settings */
static void set_workspace_count(McsManager *manager, int count)
{
    int len;

    mcs_manager_set_int(manager, "count", WORKSPACES_CHANNEL, ws_count);
    
    mcs_manager_notify(manager, WORKSPACES_CHANNEL);
    save_workspaces_channel(manager);
    
    len = array_size(ws_names);

    if (len < ws_count)
	update_names(manager, ws_count);
}

static void set_workspace_names(McsManager *manager, char **names)
{
    char *string;
    
    string = g_strjoinv(SEP_S, names);
    
    mcs_manager_set_string(manager, "names", WORKSPACES_CHANNEL, string);
    g_free(string);
    
    mcs_manager_notify(manager, WORKSPACES_CHANNEL);
    save_workspaces_channel(manager);
}

/* the dialog */
static GtkWidget *treeview;
static int treerows;

/* workspace names */
enum
{
   NUMBER_COLUMN,
   NAME_COLUMN,
   N_COLUMNS
};

static void treeview_set_rows(McsManager *manager, int n)
{
    int i;
    GtkListStore *store;
    GtkTreeModel *model;

    DBG("set %d treerows (current number: %d)\n", n, treerows);
    
    if (n == treerows)
	return;
    
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
    store = GTK_LIST_STORE(model);

    if (n < treerows)
    {
	GtkTreePath *path;
	GtkTreeIter iter;
	char num[4];

	/* we have a list so the path string is only the node index */
	snprintf(num, 3, "%d", n);
	path = gtk_tree_path_new_from_string(num);
	
	if (!gtk_tree_model_get_iter(model, &iter, path))
	{
	    g_critical("Can't get a pointer to treeview row %d", n);
	    return;
	}
	
	for (i = n; i < treerows; i++)
	{
	    /* iter gets set to next valid row, so this should work */
	    gtk_list_store_remove(store, &iter);
	}

	if (gtk_tree_path_prev(path))
	{
	    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview), path, NULL, 
					 FALSE, 0, 0);
	    gtk_tree_view_set_cursor(GTK_TREE_VIEW(treeview), path, 
		    		     NULL, FALSE);
	}

	gtk_tree_path_free(path);
    }
    else
    {
	GtkTreeIter iter;

	for (i = treerows; i < n; i++)
	{
	    char *name;
	    GtkTreePath *path;
	    
	    name = ws_names[i];

	    gtk_list_store_append(store, &iter);
	    
	    gtk_list_store_set(store, &iter, NUMBER_COLUMN, i+1, 
		    	       NAME_COLUMN, name, -1);

	    path = gtk_tree_model_get_path(model, &iter);
	    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview), path, NULL,
					 FALSE, 0, 0);
	    gtk_tree_view_set_cursor(GTK_TREE_VIEW(treeview), path, 
		    		     NULL, FALSE);

	    gtk_tree_path_free(path);
	}
    }

    treerows = n;
}

static void edit_name_dialog(GtkTreeModel *model, GtkTreeIter *iter,
			     int number, const char *name, McsManager *manager)
{
    GtkWidget *dialog, *mainvbox, *header, *hbox, *label, *entry;
    char title[512];
    int response;
    const char *tmp;

    dialog = gtk_dialog_new_with_buttons(_("Change name"), NULL,
	    				 GTK_DIALOG_NO_SEPARATOR,
					 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					 GTK_STOCK_APPLY, GTK_RESPONSE_OK,
					 NULL);
    
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    
    mainvbox = GTK_DIALOG(dialog)->vbox;

    sprintf(title, _("Workspace %d"), number); 
    header = create_header(NULL, title);
    gtk_widget_show(header);
    gtk_box_pack_start(GTK_BOX(mainvbox), header, TRUE, FALSE, 0);
    
    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), BORDER);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(mainvbox), hbox, TRUE, FALSE, 0);
    
    label = gtk_label_new(_("Name:"));
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    
    entry = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_widget_show(entry);
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
    
    gtk_entry_set_text(GTK_ENTRY(entry), name);
    
    response = GTK_RESPONSE_NONE;
    
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    
    tmp = gtk_entry_get_text(GTK_ENTRY(entry));
    
    if (response == GTK_RESPONSE_OK && tmp && strlen(tmp))
    {
	int n = number - 1;
	char *s;

	g_free(ws_names[n]);
	ws_names[n] = g_strdup(tmp);

	for (s = strchr(ws_names[n], SEP); s; s = strchr(s+1, SEP))
	{
	    /* just don't use our separator character! */
	    *s = ' ';
	}
	
	gtk_list_store_set(GTK_LIST_STORE(model), iter, 
			   NAME_COLUMN, ws_names[n], -1);

	set_workspace_names(manager, ws_names);
    }

    gtk_widget_destroy(dialog);
}

static gboolean button_pressed(GtkTreeView *tree, GdkEventButton *event,
			   McsManager *manager)
{
    GtkTreePath *path;
    GtkTreeIter iter;
    GtkTreeModel *model;

    if (gtk_tree_view_get_path_at_pos(tree, event->x, event->y, 
				      &path, NULL, NULL, NULL))
    {
	char *name;
	int number;
	
	model = gtk_tree_view_get_model(tree);
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_view_set_cursor(tree, path, NULL, FALSE);
	
	gtk_tree_model_get (model, &iter, 
			    NUMBER_COLUMN, &number, NAME_COLUMN, &name, -1);

	edit_name_dialog(model, &iter, number, name, manager);
	g_free(name);
    }

    return TRUE;
}

static void add_names_treeview(GtkWidget *vbox, McsManager *manager)
{
    GtkWidget *treeview_scroll;
    GtkListStore *store;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkTreeModel *model;
    char *markup;
    GtkWidget *label;

    treeview_scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_show (treeview_scroll);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (treeview_scroll), 
	    			    GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW (treeview_scroll),
	    				GTK_SHADOW_IN);
    gtk_box_pack_start(GTK_BOX(vbox), treeview_scroll, TRUE, TRUE, 0);

    store = gtk_list_store_new (N_COLUMNS, G_TYPE_INT, G_TYPE_STRING);
    treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
    g_object_unref (G_OBJECT (store));
    gtk_widget_show(treeview);
    gtk_container_add(GTK_CONTAINER(treeview_scroll), treeview);

    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);

    treerows=0;
    treeview_set_rows(manager, ws_count);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Number", renderer,
						       "text", NUMBER_COLUMN,
						       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Name", renderer,
						       "text", NAME_COLUMN,
						       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
    
    markup = g_strconcat("<i>", _("Click on a workspace name to edit it"), 
	    		 "</i>", NULL);
    label = gtk_label_new(markup);
    g_free(markup);
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);

    g_signal_connect (treeview, "button-press-event", 
	    	      G_CALLBACK (button_pressed), manager);    
}

/* workspace count */
static void count_changed(GtkSpinButton *spin, McsManager *manager)
{
    int n = gtk_spin_button_get_value_as_int(spin);

    ws_count = n;
    set_workspace_count(manager, n);

    treeview_set_rows(manager, n);
}

static void add_count_spinbox(GtkWidget *vbox, McsManager *manager)
{
    GtkWidget *hbox, *label, *spin;

    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

    label = gtk_label_new(_("Number of workspaces:"));
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    spin = gtk_spin_button_new_with_range(1, MAX_COUNT, 1);
    gtk_widget_show(spin);
    gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), ws_count);
    
    g_signal_connect(spin, "changed", G_CALLBACK(count_changed), manager);
}

/* we only set and save settings on exit 
 * programs should use the desktop hints, not this channel */
static void dialog_closed(McsManager *manager)
{
    GtkTreeModel *store;
    
    /* clean up list store */
    store = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
    gtk_list_store_clear(GTK_LIST_STORE(store));
}

/* run dialog */
static void run_dialog(McsPlugin *plugin )
{
    static GtkWidget *dialog = NULL;
    GtkWidget *mainvbox, *header, *frame, *vbox;

    if (dialog)
    {
	gtk_window_present(GTK_WINDOW(dialog));
	return;
    }

    dialog = gtk_dialog_new_with_buttons(_("Workspaces"),NULL,
	    				 GTK_DIALOG_NO_SEPARATOR,
					 GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
					 NULL);

    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);

    /* save channel ... */
    g_signal_connect_swapped(dialog, "response", G_CALLBACK(dialog_closed), 
	    		     plugin->manager);
    g_signal_connect_swapped(dialog, "delete-event", G_CALLBACK(dialog_closed), 
	    		     plugin->manager);
    
    /* ... and destroy dialog */
    g_signal_connect(dialog, "response",
	    	     G_CALLBACK(gtk_widget_destroy), NULL);
    g_signal_connect(dialog, "delete-event", 
	    	     G_CALLBACK(gtk_widget_destroy), NULL);
    
    g_object_add_weak_pointer(G_OBJECT(dialog), (gpointer)&dialog);

    mainvbox = GTK_DIALOG(dialog)->vbox;
    
    header = create_header(plugin->icon, _("Workspace Settings"));
    gtk_widget_show(header);
    gtk_box_pack_start(GTK_BOX(mainvbox), header, FALSE, TRUE, 0);

    add_spacer(GTK_BOX(mainvbox));

    /* Number of workspaces */
    frame = gtk_frame_new(_("Workspaces"));
    gtk_container_set_border_width(GTK_CONTAINER(frame), BORDER);
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(mainvbox), frame, FALSE, FALSE, 0);
    
    vbox = gtk_vbox_new(FALSE, BORDER);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), BORDER);
    gtk_widget_show(vbox);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    add_count_spinbox(vbox, plugin->manager);

    /* Workspace names */
    frame = gtk_frame_new(_("Workspace names"));
    gtk_container_set_border_width(GTK_CONTAINER(frame), BORDER);
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(mainvbox), frame, TRUE, TRUE, 0);
    
    vbox = gtk_vbox_new(FALSE, BORDER);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), BORDER);
    gtk_widget_show(vbox);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    add_names_treeview(vbox, plugin->manager);

    gtk_widget_set_size_request(dialog, -1, 350);

    gtk_widget_show(dialog);
}

/* watch for changes by other programs */
static void update_channel(NetkScreen *screen, NetkWorkspace *ws, 
			   McsManager *manager)
{
    ws_count = netk_screen_get_workspace_count(screen);

    set_workspace_count(manager, ws_count);
}

static void watch_workspaces_hint(McsManager *manager)
{
    g_signal_connect(netk_screen, "workspace-created",
	    	     G_CALLBACK(update_channel), manager);
    g_signal_connect(netk_screen, "workspace-destroyed",
	    	     G_CALLBACK(update_channel), manager);
}

