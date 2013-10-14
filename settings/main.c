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
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
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
#include "xfdesktop-thumbnailer.h"
#include "xfdesktop-settings-ui.h"
#include "xfdesktop-settings-appearance-frame-ui.h"

#define PREVIEW_HEIGHT  96
#define MAX_ASPECT_RATIO 1.5f

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

#define XFCE_BACKDROP_IMAGE_NONE             0
#define XFCE_BACKDROP_IMAGE_SCALED           4
#define XFCE_BACKDROP_IMAGE_SPANNING_SCREENS 6

typedef struct
{
    GtkTreeModel *model;
    GtkTreeIter *iter;
} PreviewData;

typedef struct
{
    XfconfChannel *channel;
    gint screen;
    gint monitor;
    gint workspace;
    gchar *monitor_name;
    gulong image_list_loaded:1;

    WnckWindow *wnck_window;

    GtkWidget *frame_image_list;
    GtkWidget *image_iconview;
    GtkWidget *btn_folder;
    GtkWidget *chk_apply_to_all;
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
    GAsyncQueue *preview_queue;

    XfdesktopThumbnailer *thumbnailer;

    GFile *selected_folder;
    GCancellable *cancel_enumeration;
    guint add_dir_idle_id;

} AppearancePanel;

typedef struct
{
    GFileEnumerator *file_enumerator;
    GtkListStore *ls;
    GtkTreeIter *selected_iter;
    gchar *last_image;
    gchar *file_path;
    AppearancePanel *panel;
} AddDirData;

enum
{
    COL_PIX = 0,
    COL_NAME,
    COL_FILENAME,
    COL_THUMBNAIL,
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

static void cb_xfdesktop_chk_apply_to_all(GtkCheckButton *button,
                                          gpointer user_data);
static gchar *xfdesktop_settings_generate_per_workspace_binding_string(AppearancePanel *panel,
                                                                       const gchar* property);


static void
xfdesktop_settings_do_single_preview(GtkTreeModel *model,
                                     GtkTreeIter *iter)
{
    gchar *filename = NULL, *thumbnail = NULL;
    GdkPixbuf *pix, *pix_scaled = NULL;

    GDK_THREADS_ENTER ();
    gtk_tree_model_get(model, iter,
                       COL_FILENAME, &filename,
                       COL_THUMBNAIL, &thumbnail,
                       -1);
    GDK_THREADS_LEAVE ();

    if(thumbnail == NULL) {
        pix = gdk_pixbuf_new_from_file(filename, NULL);
    } else {
        pix = gdk_pixbuf_new_from_file(thumbnail, NULL);
        g_free(thumbnail);
    }

    g_free(filename);
    if(pix) {
        gint width, height;
        gdouble aspect;

        width = gdk_pixbuf_get_width(pix);
        height = gdk_pixbuf_get_height(pix);
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

    if(pix_scaled) {
        GDK_THREADS_ENTER ();
        gtk_list_store_set(GTK_LIST_STORE(model), iter,
                           COL_PIX, pix_scaled,
                           -1);
        GDK_THREADS_LEAVE ();
        g_object_unref(G_OBJECT(pix_scaled));
    }
}

static void
xfdesktop_settings_free_pdata(gpointer data)
{
    PreviewData *pdata = data;
    g_object_unref(G_OBJECT(pdata->model));
    gtk_tree_iter_free(pdata->iter);
    g_free(pdata);
}

static gpointer
xfdesktop_settings_create_previews(gpointer data)
{
    AppearancePanel *panel = data;

    while(panel->preview_queue != NULL) {
        PreviewData *pdata = NULL;

        /* Block and wait for another preview to create */
        pdata = g_async_queue_pop(panel->preview_queue);

        xfdesktop_settings_do_single_preview(pdata->model, pdata->iter);

        xfdesktop_settings_free_pdata(pdata);
    }

    return NULL;
}

static void
xfdesktop_settings_add_file_to_queue(AppearancePanel *panel, PreviewData *pdata)
{
    TRACE("entering");

    g_return_if_fail(panel != NULL);
    g_return_if_fail(pdata != NULL);

    /* Create the queue if it doesn't exist */
    if(panel->preview_queue == NULL) {
        panel->preview_queue = g_async_queue_new_full(xfdesktop_settings_free_pdata);
    }

    g_async_queue_push(panel->preview_queue, pdata);

    /* Create the thread if it doesn't exist */
    if(panel->preview_thread == NULL) {
#if GLIB_CHECK_VERSION(2, 32, 0)
        panel->preview_thread = g_thread_try_new("xfdesktop_settings_create_previews",
                                                 xfdesktop_settings_create_previews,
                                                 panel, NULL);
#else
        panel->preview_thread = g_thread_create(xfdesktop_settings_create_previews,
                                                panel, FALSE, NULL);
#endif
        if(panel->preview_thread == NULL)
        {
                g_critical("Unable to create thread for image previews.");
                /* Don't block but try to remove the data from the queue
                 * since we won't be creating previews */
                if(g_async_queue_try_pop(panel->preview_queue))
                    xfdesktop_settings_free_pdata(pdata);
        }
    }
}

static void
cb_thumbnail_ready(XfdesktopThumbnailer *thumbnailer,
                   gchar *src_file, gchar *thumb_file,
                   gpointer user_data)
{
    AppearancePanel *panel = user_data;
    GtkTreeModel *model = gtk_icon_view_get_model(GTK_ICON_VIEW(panel->image_iconview));
    GtkTreeIter iter;
    PreviewData *pdata = NULL;

    if(gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            gchar *filename = NULL;
            gtk_tree_model_get(model, &iter, COL_FILENAME, &filename, -1);

            /* We're looking for the src_file */
            if(g_strcmp0(filename, src_file) == 0) {
                /* Add the thumb_file to it */
                gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                   COL_THUMBNAIL, thumb_file, -1);

                pdata = g_new0(PreviewData, 1);
                pdata->model = g_object_ref(G_OBJECT(model));
                pdata->iter = gtk_tree_iter_copy(&iter);

                /* Create the preview image */
                xfdesktop_settings_add_file_to_queue(panel, pdata);

                g_free(filename);
                return;
            }

            g_free(filename);
        } while(gtk_tree_model_iter_next(model, &iter));
    }
}

static void
xfdesktop_settings_queue_preview(GtkTreeModel *model,
                                 GtkTreeIter *iter,
                                 AppearancePanel *panel)
{
    gchar *filename;

    gtk_tree_model_get(model, iter, COL_FILENAME, &filename, -1);

    /* Attempt to use the thumbnailer if possible */
    if(!xfdesktop_thumbnailer_queue_thumbnail(panel->thumbnailer, filename)) {
        /* Thumbnailing not possible, add it to the queue to be loaded manually */
        PreviewData *pdata;
        pdata = g_new0(PreviewData, 1);
        pdata->model = g_object_ref(G_OBJECT(model));
        pdata->iter = gtk_tree_iter_copy(iter);

        xfdesktop_settings_add_file_to_queue(panel, pdata);
    }
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

    gtk_tree_model_get(model, b, COL_COLLATE_KEY, &key_b, -1);

    ret = g_strcmp0(a, key_b);

    g_free(key_b);

    return ret;
}

static GtkTreeIter *
xfdesktop_settings_image_iconview_add(GtkTreeModel *model,
                                      const char *path,
                                      GFileInfo *info,
                                      AppearancePanel *panel)
{
    gboolean added = FALSE, found = FALSE, valid = FALSE;
    GtkTreeIter iter, search_iter;
    gchar *name = NULL, *name_utf8 = NULL, *name_markup = NULL, *size_string = NULL;
    gchar *collate_key = NULL;
    gint position = 0;
    const gchar *content_type = g_file_info_get_content_type(info);
    goffset file_size = g_file_info_get_size(info);

    if(!xfdesktop_image_file_is_valid(path))
        return NULL;

    name = g_path_get_basename(path);
    if(name) {
        guint name_length = strlen(name);
        name_utf8 = g_filename_to_utf8(name, name_length,
                                       NULL, NULL, NULL);
        if(name_utf8) {
#if GLIB_CHECK_VERSION (2, 30, 0)
            size_string = g_format_size(file_size);
#else
            size_string = g_format_size_for_display(file_size);
#endif

            /* Display the file name, file type, and file size in the tooltip. */
            name_markup = g_markup_printf_escaped(_("<b>%s</b>\nType: %s\nSize: %s"),
                                                  name_utf8, content_type, size_string);

            /* create a case sensitive collation key for sorting filenames like
             * Thunar does */
            collate_key = g_utf8_collate_key_for_filename(name, name_length);

            /* Insert sorted */
            valid = gtk_tree_model_get_iter_first(model, &search_iter);
            while(valid && !found) {
                if(image_list_compare(model, collate_key, &search_iter) <= 0) {
                    found = TRUE;
                } else {
                    valid = gtk_tree_model_iter_next(model, &search_iter);
                    position++;
                }
            }

            gtk_list_store_insert_with_values(GTK_LIST_STORE(model),
                                              &iter,
                                              position,
                                              COL_NAME, name_markup,
                                              COL_FILENAME, path,
                                              COL_COLLATE_KEY, collate_key,
                                              -1);
            xfdesktop_settings_queue_preview(model, &iter, panel);

            added = TRUE;
        }
    }

    g_free(name);
    g_free(name_utf8);
    g_free(name_markup);
    g_free(size_string);
    g_free(collate_key);

    if(added)
        return gtk_tree_iter_copy(&iter);
    else
        return NULL;
}

static void
cb_destroy_add_dir_enumeration(gpointer user_data)
{
    AddDirData *dir_data = user_data;
    AppearancePanel *panel = dir_data->panel;

    TRACE("entering");

    g_free(dir_data->file_path);
    g_free(dir_data->last_image);

    if(G_IS_FILE_ENUMERATOR(dir_data->file_enumerator))
        g_object_unref(dir_data->file_enumerator);

    g_free(dir_data);

    if(panel->cancel_enumeration) {
        g_object_unref(panel->cancel_enumeration);
        panel->cancel_enumeration = NULL;
    }
}

static gboolean
xfdesktop_image_list_add_item(gpointer user_data)
{
    AddDirData *dir_data = user_data;
    AppearancePanel *panel = dir_data->panel;
    GFileInfo *info;
    GtkTreeIter *iter;

    /* If the enumeration gets canceled/destroyed return and
     * cb_destroy_add_dir_enumeration will get called to clean up */
    if(!G_IS_FILE_ENUMERATOR(dir_data->file_enumerator))
        return FALSE;

    /* Add one item to the icon view at a time so we don't block the UI */
    if((info = g_file_enumerator_next_file(dir_data->file_enumerator, NULL, NULL))) {
        const gchar *file_name = g_file_info_get_name(info);
        gchar *buf = g_strconcat(dir_data->file_path, "/", file_name, NULL);

        iter = xfdesktop_settings_image_iconview_add(GTK_TREE_MODEL(dir_data->ls), buf, info, panel);
        if(iter) {
            if(!dir_data->selected_iter &&
               !strcmp(buf, dir_data->last_image))
            {
                dir_data->selected_iter = iter;
            } else {
                gtk_tree_iter_free(iter);
            }
        }

        g_free(buf);
        g_object_unref(info);

        /* continue on the next idle callback so the user's events get priority */
        return TRUE;
    }

    /* If we get here we're done enumerating files in the directory */

    gtk_icon_view_set_model(GTK_ICON_VIEW(panel->image_iconview),
                            GTK_TREE_MODEL(dir_data->ls));

    /* last_image is in the directory added then it should be selected */
    if(dir_data->selected_iter) {
        GtkTreePath *path;
        path = gtk_tree_model_get_path(GTK_TREE_MODEL(dir_data->ls), dir_data->selected_iter);
        if(path) {
            gtk_icon_view_select_path(GTK_ICON_VIEW(panel->image_iconview), path);
            gtk_tree_iter_free(dir_data->selected_iter);
            gtk_tree_path_free(path);
        }
     }

    /* cb_destroy_add_dir_enumeration will get called to clean up */
    return FALSE;
}

static void
xfdesktop_image_list_add_dir(GObject *source_object,
                             GAsyncResult *res,
                             gpointer user_data)
{
    AppearancePanel *panel = user_data;
    gchar *property;
    AddDirData *dir_data = g_new0(AddDirData, 1);

    TRACE("entering");

    dir_data->panel = panel;

    dir_data->ls = gtk_list_store_new(N_COLS, GDK_TYPE_PIXBUF, G_TYPE_STRING,
                                      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    /* Get the last image/current image displayed so we can select it in the
     * icon view */
    property = xfdesktop_settings_generate_per_workspace_binding_string(panel, "last-image");

    dir_data->last_image = xfconf_channel_get_string(panel->channel, property, DEFAULT_BACKDROP);

    dir_data->file_path = g_file_get_path(panel->selected_folder);

    dir_data->file_enumerator = g_file_enumerate_children_finish(panel->selected_folder,
                                                                 res,
                                                                 NULL);

    /* Individual items are added in an idle callback so everything is more
     * responsive */
    panel->add_dir_idle_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                             xfdesktop_image_list_add_item,
                                             dir_data,
                                             cb_destroy_add_dir_enumeration);

    g_free(property);
}

static void
xfdesktop_settings_update_iconview_frame_name(AppearancePanel *panel,
                                              WnckWorkspace *wnck_workspace)
{
    gchar buf[1024];
    gchar *workspace_name;

    if(panel->monitor < 0 && panel->workspace < 0)
        return;

    if(wnck_workspace == NULL) {
        WnckScreen *wnck_screen = wnck_window_get_screen(panel->wnck_window);
        wnck_workspace = wnck_screen_get_active_workspace(wnck_screen);
    }

    workspace_name = g_strdup(wnck_workspace_get_name(wnck_workspace));

    if(gdk_screen_get_n_monitors(gtk_widget_get_screen(panel->chk_apply_to_all)) > 1) {
        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(panel->chk_apply_to_all))) {
            /* Multi-monitor single workspace */
            if(panel->monitor_name) {
                g_snprintf(buf, sizeof(buf),
                           _("Wallpaper for Monitor %d (%s)"),
                           panel->monitor, panel->monitor_name);
            } else {
                g_snprintf(buf, sizeof(buf), _("Wallpaper for Monitor %d"), panel->monitor);
            }
        } else {
            /* Multi-monitor per workspace wallpaper */
            if(panel->monitor_name) {
                g_snprintf(buf, sizeof(buf),
                           _("Wallpaper for %s on Monitor %d (%s)"),
                           workspace_name, panel->monitor, panel->monitor_name);
            } else {
                g_snprintf(buf, sizeof(buf),
                           _("Wallpaper for %s on Monitor %d"),
                           workspace_name, panel->monitor);
            }
        }
    } else {
        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(panel->chk_apply_to_all))) {
            /* Single monitor and single workspace */
            g_snprintf(buf, sizeof(buf), _("Wallpaper for my desktop"));
        } else {
            /* Single monitor and per workspace wallpaper */
            g_snprintf(buf, sizeof(buf), _("Wallpaper for %s"), workspace_name);
        }
    }

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

    g_list_foreach (selected_items, (GFunc)gtk_tree_path_free, NULL);
    g_list_free(selected_items);
    g_free(current_filename);
    g_free(buf);
}

static gint
xfdesktop_settings_get_active_workspace(AppearancePanel *panel,
                                        WnckWindow *wnck_window)
{
    WnckWorkspace *wnck_workspace;
    gboolean single_workspace;
    gint workspace_num, single_workspace_num;
    WnckScreen *wnck_screen = wnck_window_get_screen(wnck_window);

    wnck_workspace = wnck_window_get_workspace(wnck_window);

    if(wnck_workspace != NULL) {
        workspace_num = wnck_workspace_get_number(wnck_workspace);
    } else {
        workspace_num = wnck_workspace_get_number(wnck_screen_get_active_workspace(wnck_screen));
    }


    single_workspace = xfconf_channel_get_bool(panel->channel,
                                               SINGLE_WORKSPACE_MODE,
                                               TRUE);

    /* If we're in single_workspace mode we need to return the workspace that
     * it was set to, if that workspace exists, otherwise return the current
     * workspace and turn off the single workspace mode */
    if(single_workspace) {
        single_workspace_num = xfconf_channel_get_int(panel->channel,
                                                      SINGLE_WORKSPACE_NUMBER,
                                                      0);
        if(single_workspace_num < wnck_screen_get_workspace_count(wnck_screen)) {
            return single_workspace_num;
        } else {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(panel->chk_apply_to_all),
                                         FALSE);
        }
    }

    return workspace_num;
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

    timer_id = g_timeout_add(500,
                             (GSourceFunc)xfdesktop_spin_icon_size_timer,
                             button);

    g_object_set_data(G_OBJECT(button), "timer-id", GUINT_TO_POINTER(timer_id));
}

static void
xfdesktop_settings_stop_image_loading(AppearancePanel *panel)
{
    /* stop any thumbnailing in progress */
    xfdesktop_thumbnailer_dequeue_all_thumbnails(panel->thumbnailer);

    /* Remove the previews in the message queue */
    if(panel->preview_queue != NULL) {
        while(g_async_queue_length(panel->preview_queue) > 0) {
            gpointer data = g_async_queue_try_pop(panel->preview_queue);
            if(data)
                xfdesktop_settings_free_pdata(data);
        }
    }

    /* Cancel any file enumeration that's running */
    if(panel->cancel_enumeration != NULL) {
        g_cancellable_cancel(panel->cancel_enumeration);
        g_object_unref(panel->cancel_enumeration);
        panel->cancel_enumeration = NULL;
    }

    /* Cancel the file enumeration for populating the icon view */
    if(panel->add_dir_idle_id != 0) {
        g_source_remove(panel->add_dir_idle_id);
        panel->add_dir_idle_id = 0;
    }
}

static void
cb_xfdesktop_bnt_exit_clicked(GtkButton *button, gpointer user_data)
{
    AppearancePanel *panel = user_data;

    xfdesktop_settings_stop_image_loading(panel);
}

static void
cb_folder_selection_changed(GtkWidget *button,
                            gpointer user_data)
{
    AppearancePanel *panel = user_data;
    gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(button));
    gchar *previous_filename = NULL;

    TRACE("entering");

    if(panel->selected_folder != NULL)
        previous_filename = g_file_get_path(panel->selected_folder);

    /* Check to see if the folder actually did change */
    if(g_strcmp0(filename, previous_filename) == 0) {
        DBG("filename %s, previous_filename %s. Nothing changed",
            filename == NULL ? "NULL" : filename,
            previous_filename == NULL ? "NULL" : previous_filename);
        g_free(filename);
        g_free(previous_filename);
        return;
    }

    TRACE("folder changed to: %s", filename);

    if(panel->selected_folder != NULL)
        g_object_unref(panel->selected_folder);

    panel->selected_folder = g_file_new_for_path(filename);

    /* Stop any previous loading since something changed */
    xfdesktop_settings_stop_image_loading(panel);

    panel->cancel_enumeration = g_cancellable_new();

    g_file_enumerate_children_async(panel->selected_folder,
                                    XFDESKTOP_FILE_INFO_NAMESPACE,
                                    G_FILE_QUERY_INFO_NONE,
                                    G_PRIORITY_DEFAULT,
                                    panel->cancel_enumeration,
                                    xfdesktop_image_list_add_dir,
                                    panel);

    g_free(filename);
    g_free(previous_filename);
}

static void
cb_xfdesktop_combo_image_style_changed(GtkComboBox *combo,
                                       gpointer user_data)
{
    AppearancePanel *panel = user_data;

    TRACE("entering");

    if(gtk_combo_box_get_active(combo) == XFCE_BACKDROP_IMAGE_NONE) {
        /* No wallpaper so set the iconview to insensitive so the user doesn't
         * pick wallpapers that have no effect. Stop popping up tooltips for
         * the now insensitive iconview and provide a tooltip explaining why
         * the iconview is insensitive. */
        gtk_widget_set_sensitive(panel->image_iconview, FALSE);
        g_object_set(G_OBJECT(panel->image_iconview),
                     "tooltip-column", -1,
                     NULL);
        gtk_widget_set_tooltip_text(panel->image_iconview,
                                    _("Image selection is unavailable while the image style is set to None."));
    } else {
        gint tooltip_column;

        /* We are expected to provide a wallpaper so make the iconview active.
         * Additionally, if we were insensitive then we need to remove the
         * global iconview tooltip and enable the individual tooltips again. */
        gtk_widget_set_sensitive(panel->image_iconview, TRUE);
        g_object_get(G_OBJECT(panel->image_iconview),
                     "tooltip-column", &tooltip_column,
                     NULL);
        if(tooltip_column == -1) {
            gtk_widget_set_tooltip_text(panel->image_iconview, NULL);
            g_object_set(G_OBJECT(panel->image_iconview),
                         "tooltip-column", COL_NAME,
                         NULL);
        }
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
    gchar *current_folder, *prop_last, *dirname;

    TRACE("entering");

    prop_last = xfdesktop_settings_generate_per_workspace_binding_string(panel, "last-image");
    current_folder = xfconf_channel_get_string(panel->channel, prop_last, DEFAULT_BACKDROP);
    dirname = g_path_get_dirname(current_folder);

    gtk_file_chooser_set_current_folder((GtkFileChooser*)panel->btn_folder, dirname);

    /* Workaround for a bug in GTK */
    cb_folder_selection_changed(panel->btn_folder, panel);

    g_free(current_folder);
    g_free(prop_last);
    g_free(dirname);
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
    gchar *monitor_name = NULL;
    WnckWorkspace *wnck_workspace = NULL;
    GdkScreen *screen;

    screen = gtk_widget_get_screen(panel->image_iconview);
    wnck_workspace = wnck_window_get_workspace(wnck_window);

    workspace_num = xfdesktop_settings_get_active_workspace(panel, wnck_window);
    screen_num = gdk_screen_get_number(screen);
    monitor_num = gdk_screen_get_monitor_at_window(screen,
                                                   gtk_widget_get_window(panel->image_iconview));
    monitor_name = gdk_screen_get_monitor_plug_name(screen, monitor_num);

    /* Check to see if something changed */
    if(panel->workspace == workspace_num &&
       panel->screen == screen_num &&
       panel->monitor_name != NULL &&
       g_strcmp0(panel->monitor_name, monitor_name) == 0) {
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
                              XFCE_BACKDROP_IMAGE_SPANNING_SCREENS);
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
cb_workspace_changed(WnckScreen *screen,
                     WnckWorkspace *workspace,
                     gpointer user_data)
{
    AppearancePanel *panel = user_data;

    /* Update background because the single workspace mode may have
     * changed due to the addition/removal of a workspace */
    cb_update_background_tab(panel->wnck_window, user_data);
}

static void
cb_monitor_changed(GdkScreen *gscreen,
                   gpointer user_data)
{
    AppearancePanel *panel = user_data;

    /* Update background because the monitor we're on may have changed */
    cb_update_background_tab(panel->wnck_window, user_data);

    /* Update the frame name because we may change from/to a single monitor */
    xfdesktop_settings_update_iconview_frame_name(panel, wnck_window_get_workspace(panel->wnck_window));
}

static void
cb_xfdesktop_chk_apply_to_all(GtkCheckButton *button,
                              gpointer user_data)
{
    AppearancePanel *panel = user_data;
    gboolean active;
    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));

    TRACE("entering");

    xfconf_channel_set_bool(panel->channel,
                            SINGLE_WORKSPACE_MODE,
                            active);

    if(active) {
        xfconf_channel_set_int(panel->channel,
                               SINGLE_WORKSPACE_NUMBER,
                               panel->workspace);
    } else {
        cb_update_background_tab(panel->wnck_window, panel);
    }

    /* update the frame name to since we changed to/from single workspace mode */
    xfdesktop_settings_update_iconview_frame_name(panel, wnck_window_get_workspace(panel->wnck_window));
}

static void
xfdesktop_settings_setup_image_iconview(AppearancePanel *panel)
{
    GtkIconView *iconview = GTK_ICON_VIEW(panel->image_iconview);

    TRACE("entering");

    panel->thumbnailer = xfdesktop_thumbnailer_new();

    g_object_set(G_OBJECT(iconview),
                "pixbuf-column", COL_PIX,
                "tooltip-column", COL_NAME,
                "selection-mode", GTK_SELECTION_BROWSE,
                "column-spacing", 1,
                "row-spacing", 1,
                "item-padding", 10,
                "margin", 2,
                NULL);

    g_signal_connect(G_OBJECT(iconview), "selection-changed",
                     G_CALLBACK(cb_image_selection_changed), panel);

    g_signal_connect(panel->thumbnailer, "thumbnail-ready",
                     G_CALLBACK(cb_thumbnail_ready), panel);
}

static void
xfdesktop_settings_dialog_setup_tabs(GtkBuilder *main_gxml,
                                     XfconfChannel *channel,
                                     GdkScreen *screen,
                                     gulong window_xid)
{
    GtkWidget *appearance_container, *chk_custom_font_size,
              *spin_font_size, *w, *box, *spin_icon_size,
              *chk_show_thumbnails, *chk_single_click, *appearance_settings,
              *bnt_exit;
    GtkBuilder *appearance_gxml;
    AppearancePanel *panel = g_new0(AppearancePanel, 1);
    GError *error = NULL;
    GtkFileFilter *filter;
    WnckScreen *wnck_screen;

    TRACE("entering");

    appearance_container = GTK_WIDGET(gtk_builder_get_object(main_gxml,
                                                             "notebook_screens"));

    bnt_exit = GTK_WIDGET(gtk_builder_get_object(main_gxml, "bnt_exit"));

    g_signal_connect(G_OBJECT(bnt_exit), "clicked",
                     G_CALLBACK(cb_xfdesktop_bnt_exit_clicked),
                     panel);

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
    panel->wnck_window = wnck_window_get(window_xid);

    if(panel->wnck_window == NULL)
        panel->wnck_window = wnck_screen_get_active_window(wnck_screen);

    /* These callbacks are for updating the image_iconview when the window
     * moves to another monitor or workspace */
    g_signal_connect(panel->wnck_window, "geometry-changed",
                     G_CALLBACK(cb_update_background_tab), panel);
    g_signal_connect(panel->wnck_window, "workspace-changed",
                     G_CALLBACK(cb_update_background_tab), panel);
    g_signal_connect(wnck_screen, "workspace-created",
                     G_CALLBACK(cb_workspace_changed), panel);
    g_signal_connect(wnck_screen, "workspace-destroyed",
                     G_CALLBACK(cb_workspace_changed), panel);
    g_signal_connect(wnck_screen, "active-workspace-changed",
                    G_CALLBACK(cb_workspace_changed), panel);
    g_signal_connect(G_OBJECT(screen), "monitors-changed",
                     G_CALLBACK(cb_monitor_changed), panel);

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
    gtk_combo_box_set_active(GTK_COMBO_BOX(panel->image_style_combo), XFCE_BACKDROP_IMAGE_SCALED);
    gtk_combo_box_set_active(GTK_COMBO_BOX(panel->color_style_combo), 0);

    /* Use these settings for all workspaces checkbox */
    panel->chk_apply_to_all =  GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                                 "chk_apply_to_all"));

    if(xfconf_channel_get_bool(channel, SINGLE_WORKSPACE_MODE, TRUE)) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(panel->chk_apply_to_all), TRUE);
    }

    g_signal_connect(G_OBJECT(panel->chk_apply_to_all), "toggled",
                    G_CALLBACK(cb_xfdesktop_chk_apply_to_all),
                    panel);

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
    cb_update_background_tab(panel->wnck_window, panel);
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
    GError *error = NULL;

#ifdef G_ENABLE_DEBUG
    /* do NOT remove this line. If something doesn't work,
     * fix your code instead! */
    g_log_set_always_fatal(G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);
#endif

    xfce_textdomain(GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

#if !GLIB_CHECK_VERSION(2, 32, 0)
    g_thread_init(NULL);
#endif
    gdk_threads_init();

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
        g_print("%s\n", "Copyright (c) 2004-2013");
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
        GtkWidget *dialog;
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
