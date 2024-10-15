/*
 *  xfdesktop
 *
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *  Copyright (c) 2008 Brian Tarricone <brian@tarricone.org>
 *  Copyright (c) 2008 Jérôme Guelfucci <jerome.guelfucci@gmail.com>
 *  Copyright (c) 2011 Jannis Pohlmann <jannis@xfce.org>
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
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "common/xfdesktop-common.h"
#include "gtk/gtk.h"
#include "xfdesktop-settings.h"

#define XFCONF_CHANNEL_KEY "xfdesktop-xfconf-channel"

enum {
    COL_ICON_PIX = 0,
    COL_ICON_NAME,
    COL_ICON_ENABLED,
    COL_ICON_PROPERTY,
    COL_ICON_SENSITIVE,
    N_ICON_COLS,
};

static void
cb_special_icon_toggled(GtkCellRendererToggle *render, gchar *path, GtkWidget *treeview) {
    XfconfChannel *channel = g_object_get_data(G_OBJECT(treeview), XFCONF_CHANNEL_KEY);
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));

    GtkTreePath *tree_path = gtk_tree_path_new_from_string(path);

    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter, tree_path);

    gboolean show_icon;
    gchar *icon_property = NULL;
    gtk_tree_model_get(model, &iter,
                       COL_ICON_ENABLED, &show_icon,
                       COL_ICON_PROPERTY, &icon_property,
                       -1);

    show_icon = !show_icon;
    xfconf_channel_set_bool(channel, icon_property, show_icon);

    gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
                       COL_ICON_ENABLED, show_icon,
                       -1);

    if (g_strcmp0(icon_property, DESKTOP_ICONS_SHOW_REMOVABLE) == 0) {
        GtkTreeIter child_iter;
        if (gtk_tree_model_iter_children(model, &child_iter, &iter)) {
            do {
                gtk_tree_store_set(GTK_TREE_STORE(model), &child_iter,
                                   COL_ICON_SENSITIVE, show_icon,
                                   -1);
            } while (gtk_tree_model_iter_next(model, &child_iter));
        }
    }

    gtk_tree_path_free(tree_path);
    g_free(icon_property);
}


static void
special_icon_list_init(GtkBuilder *gxml, XfconfChannel *channel) {
    const struct {
        const gchar *name;
        const gchar *icon_names[2];
        const gchar *xfconf_property;
        gboolean state;
    } icons[] = {
        { N_("Home"), { "user-home", "gnome-fs-desktop" },
          DESKTOP_ICONS_SHOW_HOME, TRUE },
        { N_("File System"), { "drive-harddisk", "gnome-dev-harddisk" },
          DESKTOP_ICONS_SHOW_FILESYSTEM, TRUE },
        { N_("Trash"), { "user-trash", "gnome-fs-trash-empty" },
          DESKTOP_ICONS_SHOW_TRASH, TRUE },
        { N_("Devices"), { "drive-harddisk", "drive-removable-media" },
          DESKTOP_ICONS_SHOW_REMOVABLE, TRUE },
        { N_("Network Shares"), { "gtk-network", "gnome-dev-network" },
          DESKTOP_ICONS_SHOW_NETWORK_REMOVABLE, TRUE },
        { N_("Removable Disks and Drives"), { "drive-harddisk-usb", "gnome-dev-removable-usb" },
          DESKTOP_ICONS_SHOW_DEVICE_REMOVABLE, TRUE },
        { N_("Fixed Disks and Drives"), { "drive-harddisk-system", "drive-harddisk", },
          DESKTOP_ICONS_SHOW_DEVICE_FIXED, TRUE },
        { N_("Other Devices"), { "multimedia-player", "phone" },
          DESKTOP_ICONS_SHOW_UNKNWON_REMOVABLE, TRUE },
    };
    const gsize REMOVABLE_DEVICES = 4;

    gint w;
    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, NULL);

    gboolean removable_devices_enabled = xfconf_channel_get_bool(channel, DESKTOP_ICONS_SHOW_REMOVABLE, TRUE);

    GtkTreeStore *ts = gtk_tree_store_new(N_ICON_COLS,
                                          G_TYPE_ICON,
                                          G_TYPE_STRING,
                                          G_TYPE_BOOLEAN,
                                          G_TYPE_STRING,
                                          G_TYPE_BOOLEAN);

    GtkTreeIter parent_iter;
    for (gsize i = 0; i < G_N_ELEMENTS(icons); ++i) {
        GIcon *icon = g_themed_icon_new_from_names((char **)icons[i].icon_names, G_N_ELEMENTS(icons[i].icon_names));

        GtkTreeIter *iter, child_iter;
        if (i < REMOVABLE_DEVICES) {
            gtk_tree_store_append(ts, &parent_iter, NULL);
            iter = &parent_iter;
        } else {
            gtk_tree_store_append(ts, &child_iter, &parent_iter);
            iter = &child_iter;
        }

        gboolean sensitive = iter == &parent_iter || removable_devices_enabled;
        gtk_tree_store_set(ts, iter,
                           COL_ICON_NAME, _(icons[i].name),
                           COL_ICON_PIX, icon,
                           COL_ICON_PROPERTY, icons[i].xfconf_property,
                           COL_ICON_ENABLED, xfconf_channel_get_bool(channel, icons[i].xfconf_property, icons[i].state),
                           COL_ICON_SENSITIVE, sensitive,
                           -1);
        if (icon != NULL) {
            g_object_unref(icon);
        }
    }

    GtkWidget *treeview = GTK_WIDGET(gtk_builder_get_object(gxml, "treeview_default_icons"));
    g_object_set_data(G_OBJECT(treeview), XFCONF_CHANNEL_KEY, channel);
    GtkTreeViewColumn *col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_spacing(col, 6);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), col);

    GtkCellRenderer *render = gtk_cell_renderer_toggle_new();
    gtk_tree_view_column_pack_start(col, render, FALSE);
    gtk_tree_view_column_add_attribute(col, render, "active", COL_ICON_ENABLED);
    gtk_tree_view_column_add_attribute(col, render, "sensitive", COL_ICON_SENSITIVE);

    g_signal_connect(G_OBJECT(render), "toggled",
                     G_CALLBACK(cb_special_icon_toggled), treeview);

    render = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, render, FALSE);
    gtk_tree_view_column_add_attribute(col, render, "gicon", COL_ICON_PIX);
    gtk_tree_view_column_add_attribute(col, render, "sensitive", COL_ICON_SENSITIVE);

    render = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, render, TRUE);
    gtk_tree_view_column_add_attribute(col, render, "text", COL_ICON_NAME);
    gtk_tree_view_column_add_attribute(col, render, "sensitive", COL_ICON_SENSITIVE);

    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(ts));
    g_object_unref(G_OBJECT(ts));

    gtk_tree_view_expand_all(GTK_TREE_VIEW(treeview));
}


void
xfdesktop_file_icon_settings_init(XfdesktopSettings *settings) {
    /* show hidden files */
    GtkWidget *chk_show_hidden_files = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml,
                                                                         "chk_show_hidden_files"));
    xfconf_g_property_bind(settings->channel, DESKTOP_ICONS_SHOW_HIDDEN_FILES,
                           G_TYPE_BOOLEAN, G_OBJECT(chk_show_hidden_files),
                           "active");

    /* thumbnails */
    GtkWidget *chk_show_thumbnails = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml,
                                                                       "chk_show_thumbnails"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_show_thumbnails), TRUE);
    xfconf_g_property_bind(settings->channel, DESKTOP_ICONS_SHOW_THUMBNAILS,
                           G_TYPE_BOOLEAN, G_OBJECT(chk_show_thumbnails),
                           "active");

    GtkWidget *chk_show_delete_option = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml, "chk_show_delete_option"));
    xfconf_g_property_bind(settings->channel, DESKTOP_MENU_DELETE, G_TYPE_BOOLEAN,
                           G_OBJECT(chk_show_delete_option), "active");

    GtkWidget *chk_sort_folders_before_files = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml, "chk_sort_folders_before_files"));
    xfconf_g_property_bind(settings->channel, DESKTOP_ICONS_SORT_FOLDERS_BEFORE_FILES_PROP, G_TYPE_BOOLEAN,
                           G_OBJECT(chk_sort_folders_before_files), "active");

    special_icon_list_init(settings->main_gxml, settings->channel);
}
