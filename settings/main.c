/*
 *  xfdesktop
 *
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libwnck/libwnck.h>

#include "xfdesktop-common.h"
#include "xfdesktop-settings-ui.h"
#include "xfdesktop-settings-appearance-frame-ui.h"

#define PREVIEW_HEIGHT  48
#define MAX_ASPECT_RATIO 3.0f
#define PREVIEW_WIDTH 128

#define SHOW_DESKTOP_MENU_PROP               "/desktop-menu/show"
#define DESKTOP_MENU_SHOW_ICONS_PROP         "/desktop-menu/show-icons"

#define WINLIST_SHOW_WINDOWS_MENU_PROP       "/windowlist-menu/show"
#define WINLIST_SHOW_APP_ICONS_PROP          "/windowlist-menu/show-icons"
#define WINLIST_SHOW_STICKY_WIN_ONCE_PROP    "/windowlist-menu/show-sticky-once"
#define WINLIST_SHOW_WS_NAMES_PROP           "/windowlist-menu/show-workspace-names"
#define WINLIST_SHOW_WS_SUBMENUS_PROP        "/windowlist-menu/show-submenus"

#define DESKTOP_ICONS_STYLE_PROP             "/desktop-icons/style"
#define DESKTOP_ICONS_ICON_SIZE_PROP         "/desktop-icons/icon-size"
#define DESKTOP_ICONS_FONT_SIZE_PROP         "/desktop-icons/font-size"
#define DESKTOP_ICONS_CUSTOM_FONT_SIZE_PROP  "/desktop-icons/use-custom-font-size"
#define DESKTOP_ICONS_SINGLE_CLICK_PROP      "/desktop-icons/single-click"
#define DESKTOP_ICONS_SHOW_THUMBNAILS_PROP   "/desktop-icons/show-thumbnails"
#define DESKTOP_ICONS_SHOW_HOME              "/desktop-icons/file-icons/show-home"
#define DESKTOP_ICONS_SHOW_TRASH             "/desktop-icons/file-icons/show-trash"
#define DESKTOP_ICONS_SHOW_FILESYSTEM        "/desktop-icons/file-icons/show-filesystem"
#define DESKTOP_ICONS_SHOW_REMOVABLE         "/desktop-icons/file-icons/show-removable"

#define IMAGE_STLYE_SPANNING_SCREENS         6
#define XFCE_BACKDROP_IMAGE_NONE             0

typedef struct
{
    XfconfChannel *channel;
    gint screen;
    gint monitor;
    gint workspace;
    gchar *monitor_name;
    gulong image_list_loaded:1;

    GtkWidget *frame_image_list;
    GtkWidget *image_iconview;
    GtkWidget *btn_folder;
    GtkWidget *image_style_combo;
    GtkWidget *color_style_combo;
    GtkWidget *color1_btn;
    GtkWidget *color2_btn;

    gulong color1_btn_id;
    gulong color2_btn_id;

    GtkWidget *backdrop_cycle_spinbox;
    GtkWidget *backdrop_cycle_chkbox;
    GtkWidget *random_backdrop_order_chkbox;

    GThread *preview_thread;

} AppearancePanel;

enum
{
    COL_PIX = 0,
    COL_NAME,
    COL_FILENAME,
    COL_COLLATE_KEY,
    N_COLS,
};

enum
{
    COL_ICON_PIX = 0,
    COL_ICON_NAME,
    COL_ICON_ENABLED,
    COL_ICON_PROPERTY,
    N_ICON_COLS,
};


/* assumes gdk lock is held on function enter, and should be held
 * on function exit */
static void
xfdesktop_settings_do_single_preview(GtkTreeModel *model,
                                     GtkTreeIter *iter)
{
    gchar *name = NULL, *new_name = NULL, *filename = NULL;
    GdkPixbuf *pix, *pix_scaled = NULL;

    gtk_tree_model_get(model, iter,
                       COL_NAME, &name,
                       COL_FILENAME, &filename,
                       -1);

    pix = gdk_pixbuf_new_from_file(filename, NULL);
    g_free(filename);
    if(pix) {
        gint width, height;
        gdouble aspect;

        width = gdk_pixbuf_get_width(pix);
        height = gdk_pixbuf_get_height(pix);
        /* no need to escape markup; it's already done for us */
        new_name = g_strdup_printf(_("%s\n<i>Size: %dx%d</i>"),
                                   name, width, height);

        aspect = (gdouble)width / height;

        /* Keep the aspect ratio sensible otherwise the treeview looks bad */
        if(aspect > MAX_ASPECT_RATIO) {
            aspect = MAX_ASPECT_RATIO;
        }

        width = PREVIEW_HEIGHT * aspect;
        height = PREVIEW_HEIGHT;
        pix_scaled = gdk_pixbuf_scale_simple(pix, width, height,
                                             GDK_INTERP_BILINEAR);

        g_object_unref(G_OBJECT(pix));
    }
    g_free(name);

    if(new_name) {
        gtk_list_store_set(GTK_LIST_STORE(model), iter,
                           COL_NAME, new_name,
                           -1);
        g_free(new_name);
    }

    if(pix_scaled) {
        gtk_list_store_set(GTK_LIST_STORE(model), iter,
                           COL_PIX, pix_scaled,
                           -1);
        g_object_unref(G_OBJECT(pix_scaled));
    }
}

static gpointer
xfdesktop_settings_create_all_previews(gpointer data)
{
    GtkTreeModel *model = data;
    GtkTreeView *tree_view;
    GtkTreeIter iter;

    GDK_THREADS_ENTER ();

    if(gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            xfdesktop_settings_do_single_preview(model, &iter);
        } while(gtk_tree_model_iter_next(model, &iter));
    }

    /* if possible, scroll to the selected image */
    tree_view = g_object_get_data(G_OBJECT(model), "xfdesktop-tree-view");
    if(tree_view) {
        GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);

        if(gtk_tree_selection_get_mode(selection) != GTK_SELECTION_MULTIPLE
           && gtk_tree_selection_get_selected(selection, NULL, &iter))
        {
            GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
            gtk_tree_view_scroll_to_cell(tree_view, path, NULL, TRUE, 0.0, 0.0);
        }
    }
    g_object_set_data(G_OBJECT(model), "xfdesktop-tree-view", NULL);

    GDK_THREADS_LEAVE ();

    g_object_unref(G_OBJECT(model));

    return NULL;
}

static void
cb_special_icon_toggled(GtkCellRendererToggle *render, gchar *path, gpointer user_data)
{
    XfconfChannel *channel = g_object_get_data(G_OBJECT(user_data),
                                               "xfconf-channel");
    GtkTreePath *tree_path = gtk_tree_path_new_from_string(path);
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(user_data));
    GtkTreeIter iter;
    gboolean show_icon;
    gchar *icon_property = NULL;

    gtk_tree_model_get_iter(model, &iter, tree_path);
    gtk_tree_model_get(model, &iter, COL_ICON_ENABLED, &show_icon,
                       COL_ICON_PROPERTY, &icon_property, -1);

    show_icon = !show_icon;

    xfconf_channel_set_bool(channel, icon_property, show_icon);

    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                       COL_ICON_ENABLED, show_icon, -1);

    gtk_tree_path_free(tree_path);
    g_free(icon_property);
}

static void
setup_special_icon_list(GtkBuilder *gxml,
                        XfconfChannel *channel)
{
    GtkWidget *treeview;
    GtkListStore *ls;
    GtkTreeViewColumn *col;
    GtkCellRenderer *render;
    GtkTreeIter iter;
    const struct {
        const gchar *name;
        const gchar *icon;
        const gchar *icon_fallback;
        const gchar *xfconf_property;
        gboolean state;
    } icons[] = {
        { N_("Home"), "user-home", "gnome-fs-desktop",
          DESKTOP_ICONS_SHOW_HOME, TRUE },
        { N_("Filesystem"), "drive-harddisk", "gnome-dev-harddisk",
          DESKTOP_ICONS_SHOW_FILESYSTEM, TRUE },
        { N_("Trash"), "user-trash", "gnome-fs-trash-empty",
          DESKTOP_ICONS_SHOW_TRASH, TRUE },
        { N_("Removable Devices"), "drive-removable-media", "gnome-dev-removable",
          DESKTOP_ICONS_SHOW_REMOVABLE, TRUE },
        { NULL, NULL, NULL, NULL, FALSE },
    };
    int i, w;
    GtkIconTheme *itheme = gtk_icon_theme_get_default();

    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, NULL);

    ls = gtk_list_store_new(N_ICON_COLS, GDK_TYPE_PIXBUF, G_TYPE_STRING,
                            G_TYPE_BOOLEAN, G_TYPE_STRING);
    for(i = 0; icons[i].name; ++i) {
        GdkPixbuf *pix = NULL;

        if(gtk_icon_theme_has_icon(itheme, icons[i].icon))
            pix = gtk_icon_theme_load_icon(itheme, icons[i].icon, w, 0, NULL);
        else
            pix = gtk_icon_theme_load_icon(itheme, icons[i].icon_fallback, w, 0, NULL);

        gtk_list_store_append(ls, &iter);
        gtk_list_store_set(ls, &iter,
                           COL_ICON_NAME, _(icons[i].name),
                           COL_ICON_PIX, pix,
                           COL_ICON_PROPERTY, icons[i].xfconf_property,
                           COL_ICON_ENABLED,
                           xfconf_channel_get_bool(channel,
                                                   icons[i].xfconf_property,
                                                   icons[i].state),
                           -1);
        if(pix)
            g_object_unref(G_OBJECT(pix));
    }

    treeview = GTK_WIDGET(gtk_builder_get_object(gxml, "treeview_default_icons"));
    g_object_set_data(G_OBJECT(treeview), "xfconf-channel", channel);
    col = gtk_tree_view_column_new();
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), col);

    render = gtk_cell_renderer_toggle_new();
    gtk_tree_view_column_pack_start(col, render, FALSE);
    gtk_tree_view_column_add_attribute(col, render, "active", COL_ICON_ENABLED);

    g_signal_connect(G_OBJECT(render), "toggled",
                     G_CALLBACK(cb_special_icon_toggled), treeview);

    render = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, render, FALSE);
    gtk_tree_view_column_add_attribute(col, render, "pixbuf", COL_ICON_PIX);

    render = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, render, TRUE);
    gtk_tree_view_column_add_attribute(col, render, "text", COL_ICON_NAME);

    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(ls));
    g_object_unref(G_OBJECT(ls));
}


static gint
image_list_compare(GtkTreeModel *model,
                   const gchar *a,
                   GtkTreeIter *b)
{
    gchar *key_b = NULL;
    gint ret;

    gtk_tree_model_get(model, b, COL_NAME, &key_b, -1);

    ret = g_strcmp0(a, key_b);

    g_free(key_b);

    return ret;
}

static GtkTreeIter *
xfdesktop_settings_image_iconview_add(GtkTreeModel *model,
                                      const char *path)
{
    gboolean added = FALSE, found = FALSE, valid = FALSE;
    GtkTreeIter iter, search_iter;
    gchar *name = NULL, *name_utf8 = NULL, *name_markup = NULL;
    gchar *lower = NULL;

    if(!xfdesktop_image_file_is_valid(path))
        return NULL;

    name = g_path_get_basename(path);
    if(name) {
        name_utf8 = g_filename_to_utf8(name, strlen(name),
                                       NULL, NULL, NULL);
        if(name_utf8) {
            name_markup = g_markup_printf_escaped("<b>%s</b>",
                                                  name_utf8);

            lower = g_utf8_strdown(name_utf8, -1);

            /* Insert sorted */
            valid = gtk_tree_model_get_iter_first(model, &search_iter);
            while(valid && !found) {
                if(image_list_compare(model, name_markup, &search_iter) <= 0) {
                    found = TRUE;
                } else {
                    valid = gtk_tree_model_iter_next(model, &search_iter);
                }
            }

            if(!found) {
                gtk_list_store_append(GTK_LIST_STORE(model), &iter);
            } else {
                gtk_list_store_insert_before(GTK_LIST_STORE(model), &iter, &search_iter);
            }


            gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                               COL_NAME, name_markup,
                               COL_FILENAME, path,
                               COL_COLLATE_KEY, lower,
                               -1);

            added = TRUE;
        }
    }

    g_free(name);
    g_free(name_utf8);
    g_free(name_markup);
    g_free(lower);

    if(added)
        return gtk_tree_iter_copy(&iter);
    else
        return NULL;
}

static GtkTreeIter *
xfdesktop_image_list_add_dir(GtkListStore *ls,
                             const char *path,
                             const char *cur_image_file)
{
    GDir *dir;
    gboolean needs_slash = TRUE;
    const gchar *file;
    GtkTreeIter *iter, *iter_ret = NULL;
    gchar buf[PATH_MAX];

    dir = g_dir_open(path, 0, 0);
    if(!dir)
        return NULL;

    if(path[strlen(path)-1] == '/')
        needs_slash = FALSE;

    while((file = g_dir_read_name(dir))) {
        g_snprintf(buf, sizeof(buf), needs_slash ? "%s/%s" : "%s%s",
                   path, file);

        iter = xfdesktop_settings_image_iconview_add(GTK_TREE_MODEL(ls), buf);
        if(iter) {
            if(cur_image_file && !iter_ret && !strcmp(buf, cur_image_file))
                iter_ret = iter;
            else
                gtk_tree_iter_free(iter);
        }
    }

    g_dir_close(dir);

    return iter_ret;
}

static void
xfdesktop_settings_update_iconview_frame_name(AppearancePanel *panel,
                                              WnckWorkspace *wnck_workspace)
{
    gchar buf[1024];
    gchar *workspace_name;

    if(panel->monitor < 0 && panel->workspace < 0)
        return;

    workspace_name = g_strdup(wnck_workspace_get_name(wnck_workspace));

    if(panel->monitor_name) {
        g_snprintf(buf, sizeof(buf),
                   _("Wallpaper for %s on Monitor %d (%s)"),
                   workspace_name, panel->monitor, panel->monitor_name);
    } else
        g_snprintf(buf, sizeof(buf),
                   _("Wallpaper for %s on Monitor %d"),
                   workspace_name, panel->monitor);

    gtk_frame_set_label(GTK_FRAME(panel->frame_image_list), buf);

    g_free(workspace_name);
}

/* Free the returned string when done using it */
static gchar*
xfdesktop_settings_generate_per_workspace_binding_string(AppearancePanel *panel,
                                                         const gchar* property)
{
    gchar *buf = NULL;

    if(panel->monitor_name == NULL) {
        buf = g_strdup_printf("/backdrop/screen%d/monitor%d/workspace%d/%s",
                              panel->screen, panel->monitor, panel->workspace,
                              property);
    } else {
        buf = g_strdup_printf("/backdrop/screen%d/monitor%s/workspace%d/%s",
                              panel->screen, panel->monitor_name, panel->workspace,
                              property);
    }

    DBG("name %s", buf);

    return buf;
}

static void
cb_image_selection_changed(GtkIconView *icon_view,
                           gpointer user_data)
{
    AppearancePanel *panel = user_data;
    GtkTreeModel *model = gtk_icon_view_get_model(icon_view);
    GtkTreeIter iter;
    GList *selected_items = NULL;
    gchar *filename = NULL, *current_filename = NULL;
    gchar *buf;

    TRACE("entering");

    if(panel->image_list_loaded && GTK_IS_TREE_MODEL(model))
        return;

    selected_items = gtk_icon_view_get_selected_items(icon_view);

    /* We only care about the first selected item because the iconview
     * should be set to single selection mode */
    if(!selected_items || g_list_first(selected_items) == NULL)
        return;

    if(!gtk_tree_model_get_iter(model, &iter, g_list_first(selected_items)->data))
        return;

    gtk_tree_model_get(model, &iter, COL_FILENAME, &filename, -1);

    buf = xfdesktop_settings_generate_per_workspace_binding_string(panel, "last-image");

    current_filename = xfconf_channel_get_string(panel->channel, buf, "");

    /* check to see if the selection actually did change */
    if(g_strcmp0(current_filename, filename) != 0) {
        if(panel->monitor_name == NULL) {
            DBG("got %s, applying to screen %d monitor %d workspace %d", filename,
                panel->screen, panel->monitor, panel->workspace);
        } else {
            DBG("got %s, applying to screen %d monitor %s workspace %d", filename,
                panel->screen, panel->monitor_name, panel->workspace);
        }

        xfconf_channel_set_string(panel->channel, buf, filename);
    }

    g_list_free(selected_items);
    g_free(current_filename);
    g_free(buf);
}

static void
cb_xfdesktop_chk_custom_font_size_toggled(GtkCheckButton *button,
                                          gpointer user_data)
{
    GtkWidget *spin_button = GTK_WIDGET(user_data);

    TRACE("entering");

    gtk_widget_set_sensitive(spin_button,
                             gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)));
}

static void
cb_xfdesktop_chk_cycle_backdrop_toggled(GtkCheckButton *button,
                                        gpointer user_data)
{
    gboolean sensitive = FALSE;
    AppearancePanel *panel = user_data;

    TRACE("entering");

    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(panel->backdrop_cycle_chkbox))) {
           sensitive = TRUE;
    }

    gtk_widget_set_sensitive(panel->backdrop_cycle_spinbox, sensitive);
    gtk_widget_set_sensitive(panel->random_backdrop_order_chkbox, sensitive);
}

static gboolean
xfdesktop_spin_icon_size_timer(GtkSpinButton *button)
{
    XfconfChannel *channel = g_object_get_data(G_OBJECT(button), "xfconf-chanel");

    TRACE("entering");

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel), FALSE);

    xfconf_channel_set_uint(channel,
                            DESKTOP_ICONS_ICON_SIZE_PROP,
                            gtk_spin_button_get_value(button));

    return FALSE;
}

static void
cb_xfdesktop_spin_icon_size_changed(GtkSpinButton *button,
                                    gpointer user_data)
{
    guint timer_id = 0;

    TRACE("entering");

    g_object_set_data(G_OBJECT(button), "xfconf-chanel", user_data);

    timer_id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "timer-id"));
    if(timer_id != 0) {
        g_source_remove(timer_id);
        timer_id = 0;
    }

    timer_id = g_timeout_add(2000,
                             (GSourceFunc)xfdesktop_spin_icon_size_timer,
                             button);

    g_object_set_data(G_OBJECT(button), "timer-id", GUINT_TO_POINTER(timer_id));
}

static void
cb_folder_selection_changed(GtkWidget *button,
                            gpointer user_data)
{
    AppearancePanel *panel = user_data;
    gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(button));
    GtkListStore *ls;
    GtkTreeIter *iter;
    GtkTreePath *path;
    gchar *last_image, *property;

    TRACE("entering");

    ls = gtk_list_store_new(N_COLS, GDK_TYPE_PIXBUF, G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_STRING);

    property = xfdesktop_settings_generate_per_workspace_binding_string(panel, "last-image");

    last_image = xfconf_channel_get_string(panel->channel, property, NULL);

    if(last_image == NULL)
        last_image = DEFAULT_BACKDROP;

    iter = xfdesktop_image_list_add_dir(ls, filename, last_image);

    gtk_icon_view_set_model(GTK_ICON_VIEW(panel->image_iconview),
                            GTK_TREE_MODEL(ls));
 
    /* last_image is in the directory added then it should be selected */
    if(iter) {
        path = gtk_tree_model_get_path(GTK_TREE_MODEL(ls), iter);
        if(path) {
            gtk_icon_view_select_path(GTK_ICON_VIEW(panel->image_iconview), path);
            gtk_tree_iter_free(iter);
        }
     }

    /* remove any preview threads that may still be running since we've probably
     * changed to a new monitor/workspace */
    if(panel->preview_thread != NULL) {
        g_thread_unref(panel->preview_thread);
        panel->preview_thread = NULL;
    }

    /* generate previews of each image -- the new thread will own
     * the reference on the list store, so let's not unref it here */
    panel->preview_thread = g_thread_try_new("xfdesktop_settings_create_all_previews",
                                             xfdesktop_settings_create_all_previews,
                                             ls, NULL);

    if(panel->preview_thread == NULL) {
        g_critical("Failed to spawn thread; backdrop previews will be unavailable.");
        g_object_unref(G_OBJECT(ls));
    }

    g_free(property);
}

static void
cb_xfdesktop_combo_image_style_changed(GtkComboBox *combo,
                                 gpointer user_data)
{
    AppearancePanel *panel = user_data;

    TRACE("entering");

    if(gtk_combo_box_get_active(combo) == XFCE_BACKDROP_IMAGE_NONE) {
        gtk_widget_set_sensitive(panel->image_iconview, FALSE);
    } else {
        gtk_widget_set_sensitive(panel->image_iconview, TRUE);
    }
}

static void
cb_xfdesktop_combo_color_changed(GtkComboBox *combo,
                                 gpointer user_data)
{
    enum {
        COLORS_SOLID = 0,
        COLORS_HGRADIENT,
        COLORS_VGRADIENT,
        COLORS_NONE,
    };
    AppearancePanel *panel = user_data;

    TRACE("entering");

    if(gtk_combo_box_get_active(combo) == COLORS_SOLID) {
        gtk_widget_set_sensitive(panel->color1_btn, TRUE);
        gtk_widget_set_sensitive(panel->color2_btn, FALSE);
    } else if(gtk_combo_box_get_active(combo) == COLORS_NONE) {
        gtk_widget_set_sensitive(panel->color1_btn, FALSE);
        gtk_widget_set_sensitive(panel->color2_btn, FALSE);
    } else {
        gtk_widget_set_sensitive(panel->color1_btn, TRUE);
        gtk_widget_set_sensitive(panel->color2_btn, TRUE);
    }
}

static void
xfdesktop_settings_update_iconview_folder(AppearancePanel *panel)
{
    gchar *current_folder, *prop_last;

    TRACE("entering");

    prop_last = xfdesktop_settings_generate_per_workspace_binding_string(panel, "last-image");

    current_folder = xfconf_channel_get_string(panel->channel, prop_last, NULL);

    if(current_folder == NULL)
        current_folder = g_strdup(DEFAULT_BACKDROP);

    gtk_file_chooser_set_current_folder((GtkFileChooser*)panel->btn_folder,
                                  g_path_get_dirname(current_folder));

    g_free(current_folder);
    g_free(prop_last);
}

/* This function is to add or remove all the bindings for the background
 * tab. It's intended to be used when the app changes monitors or workspaces */
static void
xfdesktop_settings_background_tab_change_bindings(AppearancePanel *panel,
                                                  gboolean remove_binding)
{
    gchar *buf;
    XfconfChannel *channel = panel->channel;

    /* Style combobox */
    buf = xfdesktop_settings_generate_per_workspace_binding_string(panel, "image-style");
    if(remove_binding) {
        xfconf_g_property_unbind_by_property(channel, buf,
                               G_OBJECT(panel->image_style_combo), "active");
    } else {
        xfconf_g_property_bind(channel, buf, G_TYPE_INT,
                               G_OBJECT(panel->image_style_combo), "active");
        /* determine if the iconview is sensitive */
        cb_xfdesktop_combo_image_style_changed(GTK_COMBO_BOX(panel->image_style_combo), panel);
    }
    g_free(buf);

    /* Color options*/
    buf = xfdesktop_settings_generate_per_workspace_binding_string(panel, "color-style");
    if(remove_binding) {
        xfconf_g_property_unbind_by_property(channel, buf,
                           G_OBJECT(panel->color_style_combo), "active");
    } else {
        xfconf_g_property_bind(channel, buf, G_TYPE_INT,
                               G_OBJECT(panel->color_style_combo), "active");
        /* update the color button sensitivity */
        cb_xfdesktop_combo_color_changed(GTK_COMBO_BOX(panel->color_style_combo), panel);
    }
    g_free(buf);

    buf = xfdesktop_settings_generate_per_workspace_binding_string(panel, "color1");
    if(remove_binding) {
        xfconf_g_property_unbind(panel->color1_btn_id);
    } else {
        panel->color1_btn_id = xfconf_g_property_bind_gdkcolor(channel, buf,
                                                               G_OBJECT(panel->color1_btn),
                                                               "color");
    }
    g_free(buf);

    buf = xfdesktop_settings_generate_per_workspace_binding_string(panel, "color2");
    if(remove_binding) {
        xfconf_g_property_unbind(panel->color2_btn_id);
    } else {
        panel->color2_btn_id = xfconf_g_property_bind_gdkcolor(channel, buf,
                                                               G_OBJECT(panel->color2_btn),
                                                               "color");
    }
    g_free(buf);

    /* Cycle timer options */
    buf = xfdesktop_settings_generate_per_workspace_binding_string(panel, "backdrop-cycle-enable");
    if(remove_binding) {
        xfconf_g_property_unbind_by_property(channel, buf,
                           G_OBJECT(panel->backdrop_cycle_chkbox), "active");
    } else {
        xfconf_g_property_bind(channel, buf, G_TYPE_BOOLEAN,
                               G_OBJECT(panel->backdrop_cycle_chkbox), "active");
    }
    g_free(buf);

    buf = xfdesktop_settings_generate_per_workspace_binding_string(panel, "backdrop-cycle-timer");
    if(remove_binding) {
        xfconf_g_property_unbind_by_property(channel, buf,
                   G_OBJECT(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(panel->backdrop_cycle_spinbox))),
                   "value");
    } else {
        xfconf_g_property_bind(channel, buf, G_TYPE_UINT,
                       G_OBJECT(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(panel->backdrop_cycle_spinbox))),
                       "value");
    }
    g_free(buf);

    buf = xfdesktop_settings_generate_per_workspace_binding_string(panel, "backdrop-cycle-random-order");
    if(remove_binding) {
        xfconf_g_property_unbind_by_property(channel, buf,
                           G_OBJECT(panel->random_backdrop_order_chkbox), "active");
    } else {
        xfconf_g_property_bind(channel, buf, G_TYPE_BOOLEAN,
                               G_OBJECT(panel->random_backdrop_order_chkbox), "active");
    }
    g_free(buf);

}

static void
suboptions_set_sensitive(GtkToggleButton *btn,
                         gpointer user_data)
{
    GtkWidget *box = user_data;
    gtk_widget_set_sensitive(box, gtk_toggle_button_get_active(btn));
}

static void
cb_update_background_tab(WnckWindow *wnck_window,
                         gpointer user_data)
{
    AppearancePanel *panel = user_data;
    gint screen_num, monitor_num, workspace_num;
    WnckWorkspace *wnck_workspace = NULL;
    GdkScreen *screen;

    screen = gtk_widget_get_screen(panel->image_iconview);
    wnck_workspace = wnck_window_get_workspace(wnck_window);

    workspace_num = wnck_workspace_get_number(wnck_workspace);
    screen_num = gdk_screen_get_number(screen);
    monitor_num = gdk_screen_get_monitor_at_window(screen,
                                                   gtk_widget_get_window(panel->image_iconview));

    /* Check to see if something changed */
    if(panel->workspace == workspace_num &&
       panel->screen == screen_num &&
       panel->monitor == monitor_num) {
           return;
    }

    TRACE("screen, monitor, or workspace changed");

    /* remove the old bindings */
    if(panel->monitor != -1 && panel->workspace != -1) {
        xfdesktop_settings_background_tab_change_bindings(panel,
                                                          TRUE);
    }

    if(panel->monitor_name != NULL)
        g_free(panel->monitor_name);

    panel->workspace = workspace_num;
    panel->screen = screen_num;
    panel->monitor = monitor_num;
    panel->monitor_name = gdk_screen_get_monitor_plug_name(screen, panel->monitor);

    /* The first monitor has the option of doing the "spanning screens" style,
     * but only if there's multiple monitors attached. Remove it in all other cases.
     *
     * Remove the spanning screens option before we potentially add it again
     */
    gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(panel->image_style_combo),
                              IMAGE_STLYE_SPANNING_SCREENS);
    if(panel->monitor == 0 && gdk_screen_get_n_monitors(screen) > 1) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(panel->image_style_combo),
                                       _("Spanning screens"));
    }

    /* connect the new bindings */
    xfdesktop_settings_background_tab_change_bindings(panel,
                                                      FALSE);

    xfdesktop_settings_update_iconview_frame_name(panel, wnck_workspace);
    xfdesktop_settings_update_iconview_folder(panel);
}

static void
xfdesktop_settings_setup_image_iconview(AppearancePanel *panel)
{
    GtkIconView *iconview = GTK_ICON_VIEW(panel->image_iconview);

    TRACE("entering");

    g_object_set(G_OBJECT(iconview),
                "pixbuf-column", COL_PIX,
                "item-width", PREVIEW_WIDTH,
                "tooltip-column", COL_NAME,
                "selection-mode", GTK_SELECTION_BROWSE,
                NULL);

    g_signal_connect(G_OBJECT(iconview), "selection-changed",
                     G_CALLBACK(cb_image_selection_changed), panel);
}

static void
xfdesktop_settings_dialog_setup_tabs(GtkBuilder *main_gxml,
                                     XfconfChannel *channel,
                                     GdkScreen *screen,
                                     gulong window_xid)
{
    GtkWidget *appearance_container, *chk_custom_font_size,
              *spin_font_size, *w, *box, *spin_icon_size,
              *chk_show_thumbnails, *chk_single_click, *appearance_settings;
    GtkBuilder *appearance_gxml;
    AppearancePanel *panel = g_new0(AppearancePanel, 1);
    GError *error = NULL;
    GtkFileFilter *filter;
    WnckScreen *wnck_screen;
    WnckWindow *wnck_window = NULL;

    TRACE("entering");

    appearance_container = GTK_WIDGET(gtk_builder_get_object(main_gxml,
                                                             "notebook_screens"));

    /* Icons tab */
    /* icon size */
    spin_icon_size = GTK_WIDGET(gtk_builder_get_object(main_gxml, "spin_icon_size"));

    g_signal_connect(G_OBJECT(spin_icon_size), "value-changed",
                     G_CALLBACK(cb_xfdesktop_spin_icon_size_changed),
                     channel);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_icon_size),
                              xfconf_channel_get_uint(channel,
                                                      DESKTOP_ICONS_ICON_SIZE_PROP,
                                                      DEFAULT_ICON_SIZE));

    /* font size */
    chk_custom_font_size = GTK_WIDGET(gtk_builder_get_object(main_gxml,
                                                             "chk_custom_font_size"));
    spin_font_size = GTK_WIDGET(gtk_builder_get_object(main_gxml, "spin_font_size"));

    /* single click */
    chk_single_click = GTK_WIDGET(gtk_builder_get_object(main_gxml,
                                                         "chk_single_click"));

    g_signal_connect(G_OBJECT(chk_custom_font_size), "toggled",
                     G_CALLBACK(cb_xfdesktop_chk_custom_font_size_toggled),
                     spin_font_size);

    /* thumbnails */
    chk_show_thumbnails = GTK_WIDGET(gtk_builder_get_object(main_gxml,
                                                            "chk_show_thumbnails"));
    /* The default value when this property is not set, is 'TRUE'.
     * the bind operation defaults to 'FALSE' for unset boolean properties. 
     *
     * Make the checkbox correspond to the default behaviour.
     */
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(chk_show_thumbnails),
                                  TRUE);

    /* Background tab */
    panel->channel = channel;
    panel->screen = gdk_screen_get_number(screen);

    /* We have to force wnck to initialize */
    wnck_screen = wnck_screen_get(panel->screen);
    wnck_screen_force_update(wnck_screen);
    wnck_window = wnck_window_get(window_xid);

    if(wnck_window == NULL)
        wnck_window = wnck_screen_get_active_window(wnck_screen);

    g_signal_connect(wnck_window, "geometry-changed",
                     G_CALLBACK(cb_update_background_tab), panel);

    /* send invalid numbers so that the update_background_tab will update everything */
    panel->monitor = -1;
    panel->workspace = -1;
    panel->monitor_name = NULL;

    appearance_gxml = gtk_builder_new();
    if(!gtk_builder_add_from_string(appearance_gxml,
                                    xfdesktop_settings_appearance_frame_ui,
                                    xfdesktop_settings_appearance_frame_ui_length,
                                    &error))
    {
        g_printerr("Failed to parse appearance settings UI description: %s\n",
                   error->message);
        g_error_free(error);
        exit(1);
    }

    appearance_settings = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                            "alignment_settings"));

    panel->frame_image_list = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                                "frame_image_list"));

    gtk_table_attach_defaults(GTK_TABLE(appearance_container),
                             appearance_settings, 0,1,0,1);

    /* icon view area */
    panel->frame_image_list = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                                "frame_image_list"));

    panel->image_iconview = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                              "iconview_imagelist"));
    xfdesktop_settings_setup_image_iconview(panel);

    /* folder: file chooser button */
    panel->btn_folder = GTK_WIDGET(gtk_builder_get_object(appearance_gxml, "btn_folder"));
    g_signal_connect(G_OBJECT(panel->btn_folder), "selection-changed",
                     G_CALLBACK(cb_folder_selection_changed), panel);

    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, _("Image files"));
    gtk_file_filter_add_pixbuf_formats(filter);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(panel->btn_folder), filter);

    /* Image and color style options */
    panel->image_style_combo = GTK_WIDGET(gtk_builder_get_object(appearance_gxml, "combo_style"));

    panel->color_style_combo = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                                 "combo_colors"));

    panel->color1_btn = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                          "color1_btn"));

    panel->color2_btn = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                          "color2_btn"));

    g_signal_connect(G_OBJECT(panel->image_style_combo), "changed",
                     G_CALLBACK(cb_xfdesktop_combo_image_style_changed),
                     panel);

    g_signal_connect(G_OBJECT(panel->color_style_combo), "changed",
                     G_CALLBACK(cb_xfdesktop_combo_color_changed),
                     panel);

    /* Pick the first entries so something shows up */
    gtk_combo_box_set_active(GTK_COMBO_BOX(panel->image_style_combo), 0);
    gtk_combo_box_set_active(GTK_COMBO_BOX(panel->color_style_combo), 0);

    /* background cycle timer */
    panel->backdrop_cycle_chkbox = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                                     "chk_cycle_backdrop"));
    panel->backdrop_cycle_spinbox = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                                     "spin_backdrop_time_minutes"));
    panel->random_backdrop_order_chkbox = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                                     "chk_random_backdrop_order"));

    g_signal_connect(G_OBJECT(panel->backdrop_cycle_chkbox), "toggled",
                    G_CALLBACK(cb_xfdesktop_chk_cycle_backdrop_toggled),
                    panel);

    g_object_unref(G_OBJECT(appearance_gxml));


    /* Menus Tab */
    w = GTK_WIDGET(gtk_builder_get_object(main_gxml, "chk_show_desktop_menu"));
    xfconf_g_property_bind(channel, SHOW_DESKTOP_MENU_PROP, G_TYPE_BOOLEAN,
                           G_OBJECT(w), "active");
    box = GTK_WIDGET(gtk_builder_get_object(main_gxml, "box_menu_subopts"));
    g_signal_connect(G_OBJECT(w), "toggled",
                     G_CALLBACK(suboptions_set_sensitive), box);
    suboptions_set_sensitive(GTK_TOGGLE_BUTTON(w), box);

    xfconf_g_property_bind(channel, DESKTOP_MENU_SHOW_ICONS_PROP,
                           G_TYPE_BOOLEAN,
                           G_OBJECT(GTK_WIDGET(gtk_builder_get_object(main_gxml,
                                                                      "chk_menu_show_app_icons"))),
                           "active");

    w = GTK_WIDGET(gtk_builder_get_object(main_gxml, "chk_show_winlist_menu"));
    xfconf_g_property_bind(channel, WINLIST_SHOW_WINDOWS_MENU_PROP,
                           G_TYPE_BOOLEAN, G_OBJECT(w), "active");
    box = GTK_WIDGET(gtk_builder_get_object(main_gxml, "box_winlist_subopts"));
    g_signal_connect(G_OBJECT(w), "toggled",
                     G_CALLBACK(suboptions_set_sensitive), box);
    suboptions_set_sensitive(GTK_TOGGLE_BUTTON(w), box);

    xfconf_g_property_bind(channel, WINLIST_SHOW_APP_ICONS_PROP, G_TYPE_BOOLEAN,
                           gtk_builder_get_object(main_gxml, "chk_winlist_show_app_icons"),
                           "active");

    xfconf_g_property_bind(channel, WINLIST_SHOW_STICKY_WIN_ONCE_PROP,
                           G_TYPE_BOOLEAN,
                           gtk_builder_get_object(main_gxml, "chk_show_winlist_sticky_once"),
                           "active");

    w = GTK_WIDGET(gtk_builder_get_object(main_gxml, "chk_show_winlist_ws_names"));
    xfconf_g_property_bind(channel, WINLIST_SHOW_WS_NAMES_PROP, G_TYPE_BOOLEAN,
                           G_OBJECT(w), "active");
    box = GTK_WIDGET(gtk_builder_get_object(main_gxml, "box_winlist_names_subopts"));
    g_signal_connect(G_OBJECT(w), "toggled",
                     G_CALLBACK(suboptions_set_sensitive), box);
    suboptions_set_sensitive(GTK_TOGGLE_BUTTON(w), box);

    xfconf_g_property_bind(channel, WINLIST_SHOW_WS_SUBMENUS_PROP,
                           G_TYPE_BOOLEAN,
                           gtk_builder_get_object(main_gxml, "chk_show_winlist_ws_submenus"),
                           "active");

    w = GTK_WIDGET(gtk_builder_get_object(main_gxml, "combo_icons"));
#ifdef ENABLE_FILE_ICONS
    gtk_combo_box_set_active(GTK_COMBO_BOX(w), 2);
#else
    gtk_combo_box_set_active(GTK_COMBO_BOX(w), 1);
#endif
    xfconf_g_property_bind(channel, DESKTOP_ICONS_STYLE_PROP, G_TYPE_INT,
                           G_OBJECT(w), "active");
    xfconf_g_property_bind(channel, DESKTOP_ICONS_FONT_SIZE_PROP, G_TYPE_DOUBLE,
                           G_OBJECT(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spin_font_size))),
                           "value");
    xfconf_g_property_bind(channel, DESKTOP_ICONS_CUSTOM_FONT_SIZE_PROP,
                           G_TYPE_BOOLEAN, G_OBJECT(chk_custom_font_size),
                           "active");
    xfconf_g_property_bind(channel, DESKTOP_ICONS_SHOW_THUMBNAILS_PROP,
                           G_TYPE_BOOLEAN, G_OBJECT(chk_show_thumbnails),
                           "active");
    xfconf_g_property_bind(channel, DESKTOP_ICONS_SINGLE_CLICK_PROP,
                           G_TYPE_BOOLEAN, G_OBJECT(chk_single_click),
                           "active");

    setup_special_icon_list(main_gxml, channel);
    cb_update_background_tab(wnck_window, panel);
}

static void
xfdesktop_settings_response(GtkWidget *dialog, gint response_id)
{
    if(response_id == GTK_RESPONSE_HELP)
        xfce_dialog_show_help(GTK_WINDOW(dialog), "xfdesktop", "preferences", NULL);
    else
        gtk_main_quit();
}

static GdkNativeWindow opt_socket_id = 0;
static gboolean opt_version = FALSE;
static GOptionEntry option_entries[] = {
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
    { "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { NULL, },
};

int
main(int argc, char **argv)
{
    XfconfChannel *channel;
    GtkBuilder *gxml;
    GtkWidget *dialog;
    GError *error = NULL;

    xfce_textdomain(GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    if(!gtk_init_with_args(&argc, &argv, "", option_entries, PACKAGE, &error)) {
        if(G_LIKELY(error)) {
            g_printerr("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_printerr(_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_printerr("\n");
            g_error_free(error);
        } else
            g_error("Unable to open display.");

        return EXIT_FAILURE;
    }

    if(G_UNLIKELY(opt_version)) {
        g_print("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, VERSION, xfce_version_string());
        g_print("%s\n", "Copyright (c) 2004-2008");
        g_print("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print(_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print("\n");

        return EXIT_SUCCESS;
    }

    if(!xfconf_init(&error)) {
        xfce_message_dialog(NULL, _("Desktop Settings"),
                            GTK_STOCK_DIALOG_ERROR,
                            _("Unable to contact settings server"),
                            error->message,
                            GTK_STOCK_QUIT, GTK_RESPONSE_ACCEPT,
                            NULL);
        g_error_free(error);
        return 1;
    }


    gxml = gtk_builder_new();
    if(!gtk_builder_add_from_string(gxml, xfdesktop_settings_ui,
                                    xfdesktop_settings_ui_length,
                                    &error))
    {
        g_printerr("Failed to parse UI description: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    channel = xfconf_channel_new(XFDESKTOP_CHANNEL);

    if(opt_socket_id == 0) {
        dialog = GTK_WIDGET(gtk_builder_get_object(gxml, "prefs_dialog"));
        g_signal_connect(dialog, "response",
                         G_CALLBACK(xfdesktop_settings_response), NULL);
        gtk_window_present(GTK_WINDOW (dialog));
        xfdesktop_settings_dialog_setup_tabs(gxml, channel,
                                             gtk_widget_get_screen(dialog),
                                             GDK_WINDOW_XID(gtk_widget_get_window(dialog)));

        /* To prevent the settings dialog to be saved in the session */
        gdk_x11_set_sm_client_id("FAKE ID");

        gtk_main();
    } else {
        GtkWidget *plug, *plug_child;

        plug = gtk_plug_new(opt_socket_id);
        gtk_widget_show(plug);
        g_signal_connect(G_OBJECT(plug), "delete-event",
                         G_CALLBACK(gtk_main_quit), NULL);

        gdk_notify_startup_complete();

        plug_child = GTK_WIDGET(gtk_builder_get_object(gxml, "alignment1"));
        gtk_widget_reparent(plug_child, plug);
        gtk_widget_show(plug_child);
        xfdesktop_settings_dialog_setup_tabs(gxml, channel,
                                             gtk_widget_get_screen(plug),
                                             GDK_WINDOW_XID(gtk_widget_get_window(plug_child)));

        gtk_main();
    }

    g_object_unref(G_OBJECT(gxml));

    g_object_unref(G_OBJECT(channel));
    xfconf_shutdown();

    return 0;
}
