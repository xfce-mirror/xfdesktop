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

#include <cairo-gobject.h>
#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4windowing/libxfce4windowing.h>
#include <xfconf/xfconf.h>

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#include <gtk/gtkx.h>
#include <libxfce4windowing/xfw-x11.h>
#endif  /* ENABLE_X11 */

#include "common/xfdesktop-common.h"
#include "xfdesktop-settings.h"
#include "xfdesktop-thumbnailer.h"

#define MAX_ASPECT_RATIO 1.5f
#define PREVIEW_HEIGHT 96
#define PREVIEW_WIDTH (PREVIEW_HEIGHT * MAX_ASPECT_RATIO)

struct _XfdesktopBackgroundSettings {
    XfdesktopSettings *settings;

    XfwScreen *xfw_screen;
    gint screen;
    gint monitor;
    gint workspace;
    gchar *monitor_name;

    XfwWindow *xfw_window;
    /* We keep track of the current workspace number because
     * sometimes fetching the active workspace returns NULL. */
    gint active_workspace;

    gboolean image_list_loaded;

    GtkWidget *infobar;
    GtkWidget *infobar_label;
    GtkWidget *label_header;
    GtkWidget *image_iconview;
    GtkWidget *btn_folder;
    GtkWidget *btn_folder_apply;
    GtkWidget *chk_apply_to_all;
    GtkWidget *image_style_combo;
    GtkWidget *color_style_combo;
    GtkWidget *color1_btn;
    GtkWidget *color2_btn;

    gulong color1_btn_id;
    gulong color2_btn_id;

    /* backdrop cycling options */
    GtkWidget *backdrop_cycle_chkbox;
    GtkWidget *combo_backdrop_cycle_period;
    GtkWidget *backdrop_cycle_spinbox;
    GtkWidget *random_backdrop_order_chkbox;

    guint preview_id;
    GAsyncQueue *preview_queue;

    XfdesktopThumbnailer *thumbnailer;

    GFile *selected_folder;
    GCancellable *cancel_enumeration;
    guint add_dir_idle_id;

    guint last_image_signal_id;
};

typedef struct {
    GtkTreeModel *model;
    GtkTreeIter *iter;
    GdkPixbuf *pix;
    gint scale_factor;
} PreviewData;

typedef struct {
    GFileEnumerator *file_enumerator;
    GtkListStore *ls;
    GtkTreeIter *selected_iter;
    gchar *last_image;
    gchar *file_path;
    XfdesktopBackgroundSettings *background_settings;
} AddDirData;

enum {
    COL_PIX = 0,
    COL_SURFACE,
    COL_NAME,
    COL_FILENAME,
    COL_THUMBNAIL,
    COL_COLLATE_KEY,
    N_COLS,
};

static gchar *xfdesktop_settings_generate_per_workspace_binding_string(XfdesktopBackgroundSettings *background_settings,
                                                                       const gchar *property);
static gchar *xfdesktop_settings_get_backdrop_image(XfdesktopBackgroundSettings *background_settings);

static void cb_xfdesktop_chk_apply_to_all(GtkCheckButton *button,
                                          XfdesktopBackgroundSettings *background_settings);



static gboolean
path_has_image_files(GFile *dir) {
    GFileEnumerator *enumerator = g_file_enumerate_children(dir,
                                                            G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                                            G_FILE_QUERY_INFO_NONE,
                                                            NULL,
                                                            NULL);
    if (enumerator != NULL) {
        gboolean has_image_files = FALSE;

        for (;;) {
            GFileInfo *file_info = g_file_enumerator_next_file(enumerator, NULL, NULL);
            if (file_info == NULL) {
                break;
            } else {
                const gchar *content_type = g_file_info_get_content_type(file_info);
                has_image_files = content_type != NULL && g_str_has_prefix(content_type, "image/");
                g_object_unref(file_info);

                if (has_image_files) {
                    break;
                }
            }
        }

        g_object_unref(enumerator);
        return has_image_files;
    } else {
        return FALSE;
    }
}

static gint
xfdesktop_g_file_compare(gconstpointer a, gconstpointer b) {
    GFile *fa = G_FILE((gpointer)a);
    GFile *fb = G_FILE((gpointer)b);
    return g_file_equal(fa, fb) ? 0 : -1;
}

static GList *
find_background_directories(void) {
    GList *directories = NULL;

    gchar **xfce_background_dirs = xfce_resource_lookup_all(XFCE_RESOURCE_DATA, "backgrounds/xfce/");
    gchar **background_dirs = xfce_resource_lookup_all(XFCE_RESOURCE_DATA, "backgrounds/");
    gchar **dirs_dirs[] = {
        xfce_background_dirs,
        background_dirs,
    };

    for (gsize i = 0; i < G_N_ELEMENTS(dirs_dirs); ++i) {
        for (gsize j = 0; dirs_dirs[i][j] != NULL; ++j) {
            GFile *dir = g_file_new_for_path(dirs_dirs[i][j]);
            if (path_has_image_files(dir) && g_list_find_custom(directories, dir, xfdesktop_g_file_compare) == NULL) {
                directories = g_list_prepend(directories, dir);
            } else {
                g_object_unref(dir);
            }
        }
        g_strfreev(dirs_dirs[i]);
    }

    directories = g_list_reverse(directories);

    GFile *default_background = g_file_new_for_path(DEFAULT_BACKDROP);
    GFile *parent = g_file_get_parent(default_background);
    if (g_list_find_custom(directories, parent, xfdesktop_g_file_compare) == NULL) {
        directories = g_list_prepend(directories, parent);
    } else {
        g_object_unref(parent);
    }
    g_object_unref(default_background);

    return directories;
}

static void
xfdesktop_settings_free_pdata(PreviewData *pdata) {
    g_object_unref(pdata->model);

    gtk_tree_iter_free(pdata->iter);

    if (pdata->pix != NULL) {
        g_object_unref(pdata->pix);
    }

    g_free(pdata);
}

static void
xfdesktop_settings_do_single_preview(PreviewData *pdata) {
    g_return_if_fail(pdata);

    GtkTreeModel *model = pdata->model;
    GtkTreeIter *iter = pdata->iter;

    gchar *filename = NULL, *thumbnail = NULL;
    gtk_tree_model_get(model, iter,
                       COL_FILENAME, &filename,
                       COL_THUMBNAIL, &thumbnail,
                       -1);

    /* If we didn't create a thumbnail there might not be a thumbnailer service
     * or it may not support that format */
    GdkPixbuf *pix;
    if (thumbnail == NULL) {
        XF_DEBUG("generating thumbnail for filename %s", filename);
        pix = gdk_pixbuf_new_from_file_at_scale(filename,
                                                PREVIEW_WIDTH * pdata->scale_factor, PREVIEW_HEIGHT * pdata->scale_factor,
                                                TRUE, NULL);
    } else {
        XF_DEBUG("loading thumbnail %s", thumbnail);
        pix = gdk_pixbuf_new_from_file_at_scale(thumbnail,
                                                PREVIEW_WIDTH * pdata->scale_factor, PREVIEW_HEIGHT * pdata->scale_factor,
                                                TRUE, NULL);
        g_free(thumbnail);
    }

    g_free(filename);

    if (pix != NULL) {
        cairo_surface_t *surface;

        pdata->pix = pix;

        /* set the image */
        surface = gdk_cairo_surface_create_from_pixbuf(pix, pdata->scale_factor, NULL);
        gtk_list_store_set(GTK_LIST_STORE(pdata->model), pdata->iter,
                           COL_PIX, pdata->pix,
                           COL_SURFACE, surface,
                           -1);
        cairo_surface_destroy(surface);

    }
    xfdesktop_settings_free_pdata(pdata);
}

static gboolean
xfdesktop_settings_create_previews(gpointer data) {
    XfdesktopBackgroundSettings *background_settings = data;

    if(background_settings->preview_queue != NULL) {
        PreviewData *pdata = g_async_queue_try_pop(background_settings->preview_queue);

        if (pdata != NULL) {
            xfdesktop_settings_do_single_preview(pdata);
            /* Continue on the next idle time */
            return TRUE;
        } else {
            /* Nothing left, remove the queue, we're done with it */
            g_async_queue_unref(background_settings->preview_queue);
            background_settings->preview_queue = NULL;
        }
    }

    /* clear the idle source */
    background_settings->preview_id = 0;

    /* stop this idle source */
    return FALSE;
}

static void
xfdesktop_settings_add_file_to_queue(XfdesktopBackgroundSettings *background_settings, PreviewData *pdata)
{
    TRACE("entering");

    g_return_if_fail(background_settings != NULL);
    g_return_if_fail(pdata != NULL);

    /* Create the queue if it doesn't exist */
    if (background_settings->preview_queue == NULL) {
        XF_DEBUG("creating preview queue");
        background_settings->preview_queue = g_async_queue_new_full((GDestroyNotify)xfdesktop_settings_free_pdata);
    }

    g_async_queue_push(background_settings->preview_queue, pdata);

    /* Create the previews in an idle callback */
    if (background_settings->preview_id == 0) {
        background_settings->preview_id = g_idle_add(xfdesktop_settings_create_previews, background_settings);
    }
}

static void
cb_thumbnail_ready(XfdesktopThumbnailer *thumbnailer,
                   gchar *src_file,
                   gchar *thumb_file,
                   XfdesktopBackgroundSettings *background_settings)
{
    GtkTreeModel *model = gtk_icon_view_get_model(GTK_ICON_VIEW(background_settings->image_iconview));
    GtkTreeIter iter;
    PreviewData *pdata = NULL;

    if (gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            gchar *filename = NULL;
            gtk_tree_model_get(model, &iter, COL_FILENAME, &filename, -1);

            /* We're looking for the src_file */
            if (g_strcmp0(filename, src_file) == 0) {
                /* Add the thumb_file to it */
                gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                   COL_THUMBNAIL, thumb_file, -1);

                pdata = g_new0(PreviewData, 1);
                pdata->model = GTK_TREE_MODEL(g_object_ref(G_OBJECT(model)));
                pdata->iter = gtk_tree_iter_copy(&iter);
                pdata->pix = NULL;
                pdata->scale_factor = gtk_widget_get_scale_factor(GTK_WIDGET(background_settings->image_iconview));

                /* Create the preview image */
                xfdesktop_settings_add_file_to_queue(background_settings, pdata);

                g_free(filename);
                break;
            }

            g_free(filename);
        } while (gtk_tree_model_iter_next(model, &iter));
    }
}

static void
xfdesktop_settings_queue_preview(GtkTreeModel *model,
                                 GtkTreeIter *iter,
                                 XfdesktopBackgroundSettings *background_settings)
{
    gchar *filename = NULL;
    gtk_tree_model_get(model, iter, COL_FILENAME, &filename, -1);

    /* Attempt to use the thumbnailer if possible */
    if (!xfdesktop_thumbnailer_queue_thumbnail(background_settings->thumbnailer, filename)) {
        /* Thumbnailing not possible, add it to the queue to be loaded manually */
        PreviewData *pdata;
        pdata = g_new0(PreviewData, 1);
        pdata->model = GTK_TREE_MODEL(g_object_ref(G_OBJECT(model)));
        pdata->iter = gtk_tree_iter_copy(iter);
        pdata->scale_factor = gtk_widget_get_scale_factor(GTK_WIDGET(background_settings->image_iconview));

        XF_DEBUG("Thumbnailing failed, adding %s manually.", filename);
        xfdesktop_settings_add_file_to_queue(background_settings, pdata);
    }

    g_free(filename);
}


static gint
image_list_compare(GtkTreeModel *model, const gchar *a, GtkTreeIter *b) {
    gchar *key_b = NULL;
    gtk_tree_model_get(model, b, COL_COLLATE_KEY, &key_b, -1);

    gint ret = g_strcmp0(a, key_b);

    g_free(key_b);

    return ret;
}

static GtkTreeIter *
xfdesktop_settings_image_iconview_add(GtkTreeModel *model,
                                      GFile *file,
                                      GFileInfo *info,
                                      XfdesktopBackgroundSettings *background_settings)
{
    gboolean added = FALSE;
    GtkTreeIter iter;

    if (xfdesktop_image_file_is_valid(file)) {
        gchar *name = g_file_get_basename(file);
        if (name != NULL) {
            guint name_length = strlen(name);
            gchar *name_utf8 = g_filename_to_utf8(name, name_length, NULL, NULL, NULL);
            if (name_utf8 != NULL) {
                goffset file_size = g_file_info_get_size(info);
                gchar *size_string = g_format_size(file_size);

                /* Display the file name, file type, and file size in the tooltip. */
                const gchar *content_type = g_file_info_get_content_type(info);
                gchar *name_markup = g_markup_printf_escaped(_("<b>%s</b>\nType: %s\nSize: %s"),
                                                             name_utf8,
                                                             content_type,
                                                             size_string);

                /* create a case sensitive collation key for sorting filenames like
                 * Thunar does */
                gchar *collate_key = g_utf8_collate_key_for_filename(name, name_length);

                /* Insert sorted */
                GtkTreeIter search_iter;
                gboolean valid = gtk_tree_model_get_iter_first(model, &search_iter);
                gboolean found = FALSE;
                gint position = 0;
                while (valid && !found) {
                    if (image_list_compare(model, collate_key, &search_iter) <= 0) {
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
                                                  COL_FILENAME, g_file_peek_path(file),
                                                  COL_COLLATE_KEY, collate_key,
                                                  -1);
                xfdesktop_settings_queue_preview(model, &iter, background_settings);

                added = TRUE;

                g_free(name_markup);
                g_free(size_string);
                g_free(collate_key);
            }

            g_free(name_utf8);
        }

        g_free(name);
    }

    if (added) {
        return gtk_tree_iter_copy(&iter);
    } else {
        return NULL;
    }
}

static void
dir_data_free(AddDirData *dir_data) {
    XfdesktopBackgroundSettings *background_settings = dir_data->background_settings;

    TRACE("entering");

    g_free(dir_data->file_path);
    g_free(dir_data->last_image);
    if (dir_data->file_enumerator != NULL) {
        g_object_unref(dir_data->file_enumerator);
    }
    g_free(dir_data);

    if (background_settings->cancel_enumeration) {
        g_object_unref(background_settings->cancel_enumeration);
        background_settings->cancel_enumeration = NULL;
    }

    background_settings->add_dir_idle_id = 0;
}

static void
cb_enumerator_file_ready(GObject *source, GAsyncResult *res, gpointer user_data) {
    AddDirData *dir_data = user_data;

    GError *error = NULL;
    GList *file_infos = g_file_enumerator_next_files_finish(G_FILE_ENUMERATOR(source), res, &error);
    if (error != NULL) {
        if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            XfdesktopBackgroundSettings *background_settings = dir_data->background_settings;
            xfce_dialog_show_error(GTK_WINDOW(background_settings->settings->settings_toplevel),
                                   error,
                                   _("Unable to load images from folder \"%s\""),
                                   g_file_peek_path(background_settings->selected_folder));
            g_error_free(error);
        }
        dir_data_free(dir_data);
    } else if (file_infos == NULL) {
        XfdesktopBackgroundSettings *background_settings = dir_data->background_settings;

        gtk_icon_view_set_model(GTK_ICON_VIEW(background_settings->image_iconview),
                                GTK_TREE_MODEL(dir_data->ls));

        if (dir_data->selected_iter != NULL) {
            GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(dir_data->ls), dir_data->selected_iter);
            gtk_icon_view_select_path(GTK_ICON_VIEW(background_settings->image_iconview), path);
            gtk_tree_path_free(path);
        } else {
            gchar *prop_name = xfdesktop_settings_generate_per_workspace_binding_string(background_settings, "backdrop-cycle-enable");
            gboolean backdrop_cycle_enable = xfconf_channel_get_bool(background_settings->settings->channel, prop_name, FALSE);

            if (backdrop_cycle_enable) {
                gtk_widget_show(background_settings->btn_folder_apply);
            }

            g_free(prop_name);
        }

        dir_data_free(dir_data);
    } else {

        XfdesktopBackgroundSettings *background_settings = dir_data->background_settings;
        for (GList *l = file_infos; l != NULL; l = l->next) {
            GFileInfo *info = G_FILE_INFO(l->data);
            GFile *file = g_file_enumerator_get_child(dir_data->file_enumerator, info);

            GtkTreeIter *iter = xfdesktop_settings_image_iconview_add(GTK_TREE_MODEL(dir_data->ls),
                                                                      file,
                                                                      info,
                                                                      background_settings);
            if (iter != NULL) {
                if (dir_data->selected_iter == NULL && g_strcmp0(g_file_peek_path(file), dir_data->last_image) == 0) {
                    dir_data->selected_iter = iter;
                } else {
                    gtk_tree_iter_free(iter);
                }
            }

            g_object_unref(file);
            g_object_unref(info);
        }
        g_list_free(file_infos);

        g_file_enumerator_next_files_async(dir_data->file_enumerator,
                                           5,
                                           G_PRIORITY_DEFAULT_IDLE,
                                           background_settings->cancel_enumeration,
                                           cb_enumerator_file_ready,
                                           dir_data);
    }
}

static void
xfdesktop_image_list_add_dir(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    TRACE("entering");

    XfdesktopBackgroundSettings *background_settings = user_data;

    GError *error = NULL;
    GFileEnumerator *enumerator = g_file_enumerate_children_finish(background_settings->selected_folder,
                                                                   res,
                                                                   &error);
    if (enumerator == NULL) {
        xfce_dialog_show_error(GTK_WINDOW(background_settings->settings->settings_toplevel),
                               error,
                               _("Unable to load images from folder \"%s\""),
                               g_file_peek_path(background_settings->selected_folder));
        g_error_free(error);
    } else {
        AddDirData *dir_data = g_new0(AddDirData, 1);
        dir_data->background_settings = background_settings;
        dir_data->file_enumerator = enumerator;

        dir_data->ls = gtk_list_store_new(N_COLS,
                                          GDK_TYPE_PIXBUF,
                                          CAIRO_GOBJECT_TYPE_SURFACE,
                                          G_TYPE_STRING,
                                          G_TYPE_STRING,
                                          G_TYPE_STRING,
                                          G_TYPE_STRING);

        /* Get the last image/current image displayed so we can select it in the
         * icon view */
        dir_data->last_image = xfdesktop_settings_get_backdrop_image(background_settings);
        dir_data->file_path = g_file_get_path(background_settings->selected_folder);

        g_file_enumerator_next_files_async(dir_data->file_enumerator,
                                           5,
                                           G_PRIORITY_DEFAULT_IDLE,
                                           background_settings->cancel_enumeration,
                                           cb_enumerator_file_ready,
                                           dir_data);
    }
}

static XfwWorkspace *
find_workspace_by_number(XfwWorkspaceManager *workspace_manager, gint workspace_num) {
    gint total_workspaces = 0;
    for (GList *l = xfw_workspace_manager_list_workspace_groups(workspace_manager);
         l != NULL;
         l = l->next)
    {
        XfwWorkspaceGroup *group = XFW_WORKSPACE_GROUP(l->data);
        gint n_workspaces = xfw_workspace_group_get_workspace_count(group);
        if (total_workspaces + n_workspaces > workspace_num) {
            return g_list_nth_data(xfw_workspace_group_list_workspaces(group),
                                   workspace_num - total_workspaces);
        }
    }
    return NULL;
}

static gint
workspace_count_total(XfwWorkspaceManager *workspace_manager) {
    gint n_ws = 0;
    for (GList *l = xfw_workspace_manager_list_workspace_groups(workspace_manager);
         l != NULL;
         l = l->next)
    {
        n_ws += xfw_workspace_group_get_workspace_count(XFW_WORKSPACE_GROUP(l->data));
    }
    return n_ws;
}

static void
xfdesktop_settings_update_iconview_frame_name(XfdesktopBackgroundSettings *background_settings,
                                              XfwWorkspace *xfw_workspace)
{
    /* Don't update the name until we find our window */
    if (background_settings->xfw_window == NULL) {
        return;
    }

    g_return_if_fail(background_settings->monitor >= 0 && background_settings->workspace >= 0);

    XfwScreen *screen = xfw_window_get_screen(background_settings->xfw_window);
    XfwWorkspaceManager *workspace_manager = xfw_screen_get_workspace_manager(screen);

    /* If it's a pinned window get the active workspace */
    XfwWorkspace *workspace;
    if (xfw_workspace == NULL) {
        workspace = find_workspace_by_number(workspace_manager, background_settings->active_workspace);
    } else {
        workspace = xfw_workspace;
    }

    if (workspace == NULL) {
        g_critical("active workspace is not in list");
        return;
    }

    gchar *workspace_name = g_strdup(xfw_workspace_get_name(workspace));

    gchar *label_text;
    if (gdk_display_get_n_monitors(gtk_widget_get_display(background_settings->chk_apply_to_all)) > 1) {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(background_settings->chk_apply_to_all))) {
            /* Multi-monitor single workspace */
            if (background_settings->monitor_name != NULL) {
                label_text = g_strdup_printf(_("Wallpaper for Monitor %d (%s)"),
                                             background_settings->monitor,
                                             background_settings->monitor_name);
            } else {
                label_text = g_strdup_printf(_("Wallpaper for Monitor %d"), background_settings->monitor);
            }

            /* This is for the infobar letting the user know how to configure
             * multiple monitor setups */
            gtk_label_set_text(GTK_LABEL(background_settings->infobar_label),
                               _("Move this dialog to the display you "
                                 "want to edit the settings for."));
        } else {
            /* Multi-monitor per workspace wallpaper */
            if (background_settings->monitor_name != NULL) {
                label_text = g_strdup_printf(_("Wallpaper for %s on Monitor %d (%s)"),
                                             workspace_name,
                                             background_settings->monitor,
                                             background_settings->monitor_name);
            } else {
                label_text = g_strdup_printf(_("Wallpaper for %s on Monitor %d"),
                                             workspace_name,
                                             background_settings->monitor);
            }

            /* This is for the infobar letting the user know how to configure
             * multiple monitor/workspace setups */
            gtk_label_set_text(GTK_LABEL(background_settings->infobar_label),
                               _("Move this dialog to the display and "
                                 "workspace you want to edit the settings for."));
        }

        gtk_widget_show(background_settings->infobar);
    } else {
        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(background_settings->chk_apply_to_all)) ||
           (workspace_count_total(workspace_manager) == 1)) {
            /* Single monitor and single workspace */
            label_text = g_strdup(_("Wallpaper for my desktop"));

            /* No need for the infobar */
            gtk_widget_hide(background_settings->infobar);
        } else {
            /* Single monitor and per workspace wallpaper */
            label_text = g_strdup_printf(_("Wallpaper for %s"), workspace_name);

            /* This is for the infobar letting the user know how to configure
             * multiple workspace setups */
            gtk_label_set_text(GTK_LABEL(background_settings->infobar_label),
                               _("Move this dialog to the workspace you "
                                 "want to edit the settings for."));
            gtk_widget_show(background_settings->infobar);
        }
    }

    /* This label is for which workspace/monitor we're on */
    gtk_label_set_text(GTK_LABEL(background_settings->label_header), label_text);

    g_free(workspace_name);
    g_free(label_text);
}

/* Free the returned string when done using it */
static gchar *
xfdesktop_settings_generate_per_workspace_binding_string(XfdesktopBackgroundSettings *background_settings,
                                                         const gchar *property)
{
    gchar *buf;
    if (background_settings->monitor_name == NULL) {
        buf = g_strdup_printf("/backdrop/screen%d/monitor%d/workspace%d/%s",
                              background_settings->screen,
                              background_settings->monitor,
                              background_settings->workspace,
                              property);
    } else {
        buf = xfdesktop_remove_whitspaces(g_strdup_printf("/backdrop/screen%d/monitor%s/workspace%d/%s",
                              background_settings->screen,
                              background_settings->monitor_name,
                              background_settings->workspace,
                              property));
    }

    XF_DEBUG("name %s", buf);

    return buf;
}

static gchar *
xfdesktop_settings_generate_old_binding_string(XfdesktopBackgroundSettings *background_settings,
                                               const gchar* property)
{
    gchar *buf = g_strdup_printf("/backdrop/screen%d/monitor%d/%s",
                                 background_settings->screen,
                                 background_settings->monitor,
                                 property);

    XF_DEBUG("name %s", buf);

    return buf;
}

/* Attempts to load the backdrop from the current location followed by using
 * how previous versions of xfdesktop (before 4.11) did. This matches how
 * xfdesktop searches for backdrops.
 * Free the returned string when done using it. */
static gchar *
xfdesktop_settings_get_backdrop_image(XfdesktopBackgroundSettings *background_settings) {
    /* Get the last image/current image displayed, if available */
    gchar *property = xfdesktop_settings_generate_per_workspace_binding_string(background_settings, "last-image");
    gchar *last_image = xfconf_channel_get_string(background_settings->settings->channel, property, NULL);

    /* Try the previous version or fall back to our provided default */
    if (last_image == NULL) {
        gchar *old_property = xfdesktop_settings_generate_old_binding_string(background_settings,
                                                                             "image-path");
        last_image = xfconf_channel_get_string(background_settings->settings->channel,
                                               old_property,
                                               DEFAULT_BACKDROP);
        g_free(old_property);
    }

    g_free(property);

    return last_image;
}

static void
cb_image_selection_changed(GtkIconView *icon_view, XfdesktopBackgroundSettings *background_settings) {
    TRACE("entering");

    GtkTreeModel *model = gtk_icon_view_get_model(icon_view);
    if (background_settings->image_list_loaded && GTK_IS_TREE_MODEL(model)) {
        return;
    }

    GList *selected_items = gtk_icon_view_get_selected_items(icon_view);

    /* We only care about the first selected item because the iconview
     * should be set to single selection mode */
    if (selected_items == NULL) {
        return;
    }

    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter(model, &iter, g_list_nth_data(selected_items, 0))) {
        g_list_free_full(selected_items, (GDestroyNotify)gtk_tree_path_free);
        return;
    }

    gtk_widget_hide(background_settings->btn_folder_apply);

    gchar *filename = NULL;
    gtk_tree_model_get(model, &iter, COL_FILENAME, &filename, -1);

    /* Get the current/last image, handles migrating from old versions */
    gchar *current_filename = xfdesktop_settings_get_backdrop_image(background_settings);

    /* check to see if the selection actually did change */
    if (g_strcmp0(current_filename, filename) != 0) {
        if (background_settings->monitor_name == NULL) {
            XF_DEBUG("got %s, applying to screen %d monitor %d workspace %d", filename,
                     background_settings->screen, background_settings->monitor, background_settings->workspace);
        } else {
            XF_DEBUG("got %s, applying to screen %d monitor %s workspace %d", filename,
                     background_settings->screen, background_settings->monitor_name, background_settings->workspace);
        }

        /* Get the property location to save our changes, always save to new
         * location */
        gchar *buf = xfdesktop_settings_generate_per_workspace_binding_string(background_settings, "last-image");
        XF_DEBUG("Saving to %s/%s", buf, filename);
        xfconf_channel_set_string(background_settings->settings->channel, buf, filename);
        g_free(buf);
    }

    g_list_free_full(selected_items, (GDestroyNotify)gtk_tree_path_free);
    g_free(current_filename);
    g_free(filename);
}

static void
update_backdrop_random_order_chkbox(XfdesktopBackgroundSettings *background_settings) {
    /* For the random check box to be active the combo_backdrop_cycle_period
     * needs to be active and needs to not be set to chronological */
    gboolean sensitive;
    if (gtk_widget_get_sensitive(background_settings->combo_backdrop_cycle_period)) {
        gint period = gtk_combo_box_get_active(GTK_COMBO_BOX(background_settings->combo_backdrop_cycle_period));
        sensitive = period != XFCE_BACKDROP_PERIOD_CHRONOLOGICAL;
    } else {
        sensitive = FALSE;
    }

    gtk_widget_set_sensitive(background_settings->random_backdrop_order_chkbox, sensitive);
}

static void
update_backdrop_cycle_spinbox(XfdesktopBackgroundSettings *background_settings) {
    /* For the spinbox to be active the combo_backdrop_cycle_period needs to be
     * active and needs to be set to something where the spinbox would apply */
    gboolean sensitive;
    if (gtk_widget_get_sensitive(background_settings->combo_backdrop_cycle_period)) {
        gint period = gtk_combo_box_get_active(GTK_COMBO_BOX(background_settings->combo_backdrop_cycle_period));
        sensitive = period == XFCE_BACKDROP_PERIOD_SECONDS
            || period == XFCE_BACKDROP_PERIOD_MINUTES
            || period == XFCE_BACKDROP_PERIOD_HOURS;
    } else {
        sensitive = FALSE;
    }

    gtk_widget_set_sensitive(background_settings->backdrop_cycle_spinbox, sensitive);
}

static void
cb_combo_backdrop_cycle_period_change(GtkComboBox *combo, XfdesktopBackgroundSettings *background_settings) {
    /* determine if the spin box should be sensitive */
    update_backdrop_cycle_spinbox(background_settings);
    /* determine if the random check box should be sensitive */
    update_backdrop_random_order_chkbox(background_settings);
}

static void
cb_xfdesktop_chk_cycle_backdrop_toggled(GtkCheckButton *button, XfdesktopBackgroundSettings *background_settings) {
    TRACE("entering");

    gboolean sensitive = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(background_settings->backdrop_cycle_chkbox));

    /* The cycle backdrop toggles the period and random widgets */
    gtk_widget_set_sensitive(background_settings->combo_backdrop_cycle_period, sensitive);
    /* determine if the spin box should be sensitive */
    update_backdrop_cycle_spinbox(background_settings);
    /* determine if the random check box should be sensitive */
    update_backdrop_random_order_chkbox(background_settings);

    if (!sensitive) {
        gtk_widget_hide(background_settings->btn_folder_apply);
    } else {
        GList *selected = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(background_settings->image_iconview));
        if (selected == NULL) {
            gtk_widget_show(background_settings->btn_folder_apply);
        } else {
            g_list_free_full(selected, (GDestroyNotify)gtk_tree_path_free);
        }
    }
}

static void
stop_image_loading(XfdesktopBackgroundSettings *background_settings) {
    XF_DEBUG("xfdesktop_settings_stop_image_loading");
    /* stop any thumbnailing in progress */
    xfdesktop_thumbnailer_dequeue_all_thumbnails(background_settings->thumbnailer);

    /* stop the idle preview callback */
    if (background_settings->preview_id != 0) {
        g_source_remove(background_settings->preview_id);
        background_settings->preview_id = 0;
    }

    /* Remove the previews in the message queue */
    if (background_settings->preview_queue != NULL) {
        g_async_queue_unref(background_settings->preview_queue);
        background_settings->preview_queue = NULL;
    }

    /* Cancel any file enumeration that's running */
    if (background_settings->cancel_enumeration != NULL) {
        g_cancellable_cancel(background_settings->cancel_enumeration);
        g_object_unref(background_settings->cancel_enumeration);
        background_settings->cancel_enumeration = NULL;
    }

    /* Cancel the file enumeration for populating the icon view */
    if (background_settings->add_dir_idle_id != 0) {
        g_source_remove(background_settings->add_dir_idle_id);
        background_settings->add_dir_idle_id = 0;
    }
}

static gboolean
update_icon_view_model(XfdesktopBackgroundSettings *background_settings) {
    TRACE("entering");

    gchar *new_folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(background_settings->btn_folder));
    gchar *previous_folder = NULL;
    if (background_settings->selected_folder != NULL) {
        previous_folder = g_file_get_path(background_settings->selected_folder);
    }

    /* Check to see if the folder actually did change */
    if (g_strcmp0(new_folder, previous_folder) == 0) {
        gchar *current_filename = xfdesktop_settings_get_backdrop_image(background_settings);
        gchar *current_folder = g_path_get_dirname(current_filename);
        g_free(current_filename);

        /* workaround another gtk bug - if the user sets the file chooser
         * button to something then it can't be changed with a set folder
         * call anymore. */
        if(g_strcmp0(new_folder, current_folder) == 0) {
            XF_DEBUG("folder didn't change");
            g_free(current_folder);
            g_free(new_folder);
            g_free(previous_folder);
            return FALSE;
        } else {
            g_free(new_folder);
            new_folder = current_folder;
        }
    }

    TRACE("folder changed to: %s", new_folder);

    if (background_settings->selected_folder != NULL) {
        g_object_unref(background_settings->selected_folder);
    }
    background_settings->selected_folder = g_file_new_for_path(new_folder);

    /* Stop any previous loading since something changed */
    stop_image_loading(background_settings);

    background_settings->cancel_enumeration = g_cancellable_new();

    g_file_enumerate_children_async(background_settings->selected_folder,
                                    XFDESKTOP_FILE_INFO_NAMESPACE,
                                    G_FILE_QUERY_INFO_NONE,
                                    G_PRIORITY_DEFAULT,
                                    background_settings->cancel_enumeration,
                                    xfdesktop_image_list_add_dir,
                                    background_settings);

    g_free(new_folder);
    g_free(previous_folder);

    return TRUE;
}

static void
cb_folder_selection_changed(GtkWidget *button, XfdesktopBackgroundSettings *background_settings) {
    update_icon_view_model(background_settings);
}

static void
cb_folder_apply_clicked(GtkWidget *button, XfdesktopBackgroundSettings *background_settings) {
    GList *selected = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(background_settings->image_iconview));
    if (G_LIKELY(selected == NULL)) {
        GtkTreeModel *model = gtk_icon_view_get_model(GTK_ICON_VIEW(background_settings->image_iconview));
        GtkTreeIter first;
        if (gtk_tree_model_get_iter_first(model, &first)) {
            GtkTreePath *path = gtk_tree_model_get_path(model, &first);
            if (G_LIKELY(path != NULL)) {
                gtk_icon_view_select_path(GTK_ICON_VIEW(background_settings->image_iconview), path);
                gtk_tree_path_free(path);
            }
        }
    } else {
        g_list_free_full(selected, (GDestroyNotify)gtk_tree_path_free);
    }

    gtk_widget_hide(button);
}

static void
cb_xfdesktop_combo_image_style_changed(GtkComboBox *combo, XfdesktopBackgroundSettings *background_settings) {
    TRACE("entering");

    if (gtk_combo_box_get_active(combo) == XFCE_BACKDROP_IMAGE_NONE) {
        /* No wallpaper so set the iconview to insensitive so the user doesn't
         * pick wallpapers that have no effect. Stop popping up tooltips for
         * the now insensitive iconview and provide a tooltip explaining why
         * the iconview is insensitive. */
        gtk_widget_set_sensitive(background_settings->image_iconview, FALSE);
        g_object_set(G_OBJECT(background_settings->image_iconview),
                     "tooltip-column", -1,
                     NULL);
        gtk_widget_set_tooltip_text(background_settings->image_iconview,
                                    _("Image selection is unavailable while the image style is set to None."));
    } else {

        /* We are expected to provide a wallpaper so make the iconview active.
         * Additionally, if we were insensitive then we need to remove the
         * global iconview tooltip and enable the individual tooltips again. */
        gtk_widget_set_sensitive(background_settings->image_iconview, TRUE);
        gint tooltip_column;
        g_object_get(G_OBJECT(background_settings->image_iconview),
                     "tooltip-column", &tooltip_column,
                     NULL);
        if (tooltip_column == -1) {
            gtk_widget_set_tooltip_text(background_settings->image_iconview, NULL);
            g_object_set(G_OBJECT(background_settings->image_iconview),
                         "tooltip-column", COL_NAME,
                         NULL);
        }
    }
}

static void
cb_xfdesktop_combo_color_changed(GtkComboBox *combo, XfdesktopBackgroundSettings *background_settings) {
    enum {
        COLORS_SOLID = 0,
        COLORS_HGRADIENT,
        COLORS_VGRADIENT,
        COLORS_NONE,
    };

    TRACE("entering");

    if (gtk_combo_box_get_active(combo) == COLORS_SOLID) {
        gtk_widget_set_sensitive(background_settings->color1_btn, TRUE);
        gtk_widget_set_sensitive(background_settings->color2_btn, FALSE);
    } else if (gtk_combo_box_get_active(combo) == COLORS_NONE) {
        gtk_widget_set_sensitive(background_settings->color1_btn, FALSE);
        gtk_widget_set_sensitive(background_settings->color2_btn, FALSE);
    } else {
        gtk_widget_set_sensitive(background_settings->color1_btn, TRUE);
        gtk_widget_set_sensitive(background_settings->color2_btn, TRUE);
    }
}

static gboolean
xfdesktop_settings_update_iconview_folder(XfdesktopBackgroundSettings *background_settings) {
    /* If we haven't found our window return now and wait for that */
    if (background_settings->xfw_window == NULL) {
        return FALSE;
    }

    TRACE("entering");

    gchar *current_folder = xfdesktop_settings_get_backdrop_image(background_settings);
    gchar *dirname = g_path_get_dirname(current_folder);

    XF_DEBUG("current_folder %s, dirname %s", current_folder, dirname);

    gtk_file_chooser_set_current_folder((GtkFileChooser*)background_settings->btn_folder, dirname);

    /* Workaround for a bug in GTK */
    gboolean ret = update_icon_view_model(background_settings);

    g_free(current_folder);
    g_free(dirname);

    return ret;
}

static void
last_image_changed(XfconfChannel *channel,
                   const gchar *property_name,
                   const GValue *value,
                   XfdesktopBackgroundSettings *background_settings)
{
    if (!xfdesktop_settings_update_iconview_folder(background_settings)) {
        if (G_VALUE_HOLDS_STRING(value)) {
            const gchar *cur_filename = g_value_get_string(value);
            GtkTreeModel *model = gtk_icon_view_get_model(GTK_ICON_VIEW(background_settings->image_iconview));
            if (model != NULL) {
                GtkTreeIter iter;
                if (gtk_tree_model_get_iter_first(model, &iter)) {
                    do {
                        gchar *filename;
                        gtk_tree_model_get(model, &iter,
                                           COL_FILENAME, &filename,
                                           -1);
                        gboolean matches = g_strcmp0(cur_filename, filename) == 0;
                        g_free(filename);

                        if (matches) {
                            g_signal_handlers_block_by_func(background_settings->image_iconview,
                                                            cb_image_selection_changed,
                                                            background_settings);

                            GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
                            gtk_icon_view_select_path(GTK_ICON_VIEW(background_settings->image_iconview), path);
                            gtk_tree_path_free(path);

                            g_signal_handlers_unblock_by_func(background_settings->image_iconview,
                                                              cb_image_selection_changed,
                                                              background_settings);
                            break;
                        }
                    } while (gtk_tree_model_iter_next(model, &iter));
                }
            }
        }
    }
}

/* This function is to add or remove all the bindings for the background
 * tab. It's intended to be used when the app changes monitors or workspaces.
 * It reverts the items back to their defaults before binding any new settings
 * that way if the setting isn't present, the default correctly displays. */
static void
xfdesktop_settings_background_tab_change_bindings(XfdesktopBackgroundSettings *background_settings,
                                                  gboolean remove_binding)
{
    gchar *buf, *old_property = NULL;
    XfconfChannel *channel = background_settings->settings->channel;

    /* Image style combo box */
    buf = xfdesktop_settings_generate_per_workspace_binding_string(background_settings, "image-style");
    if (remove_binding) {
        xfconf_g_property_unbind_by_property(channel,
                                             buf,
                                             G_OBJECT(background_settings->image_style_combo),
                                             "active");
    } else {
        /* If the current image style doesn't exist, try to load the old one */
        if (!xfconf_channel_has_property(channel, buf)) {
            gint image_style;
            old_property = xfdesktop_settings_generate_old_binding_string(background_settings, "image-style");

            /* default to zoomed when trying to migrate (zoomed was part of how
             * auto worked in 4.10)*/
            image_style = xfconf_channel_get_int(channel, old_property, XFCE_BACKDROP_IMAGE_ZOOMED);

            /* xfce_translate_image_styles will do sanity checking */
            gtk_combo_box_set_active(GTK_COMBO_BOX(background_settings->image_style_combo),
                                     xfce_translate_image_styles(image_style));

            g_free(old_property);
        }

        xfconf_g_property_bind(channel, buf, G_TYPE_INT,
                               G_OBJECT(background_settings->image_style_combo), "active");
        /* determine if the iconview is sensitive */
        cb_xfdesktop_combo_image_style_changed(GTK_COMBO_BOX(background_settings->image_style_combo), background_settings);
    }
    g_free(buf);

    buf = xfdesktop_settings_generate_per_workspace_binding_string(background_settings, "last-image");
    if (remove_binding) {
        if (background_settings->last_image_signal_id != 0) {
            g_signal_handler_disconnect(channel, background_settings->last_image_signal_id);
            background_settings->last_image_signal_id = 0;
        }
    } else {
        gchar *signal = g_strconcat("property-changed::", buf, NULL);
        background_settings->last_image_signal_id = g_signal_connect(channel, signal,
                                                                     G_CALLBACK(last_image_changed), background_settings);
        g_free(signal);
    }
    g_free(buf);

    /* Color style combo box */
    buf = xfdesktop_settings_generate_per_workspace_binding_string(background_settings, "color-style");
    if (remove_binding) {
        xfconf_g_property_unbind_by_property(channel,
                                             buf,
                                             G_OBJECT(background_settings->color_style_combo),
                                             "active");
    } else {
        /* If the current color style doesn't exist, try to load the old one */
        if (!xfconf_channel_has_property(channel, buf)) {
            gint color_style;
            old_property = xfdesktop_settings_generate_old_binding_string(background_settings, "color-style");

            /* default to solid when trying to migrate */
            color_style = xfconf_channel_get_int(channel, old_property, XFCE_BACKDROP_COLOR_SOLID);

            /* sanity check */
            if(color_style < 0 || color_style > XFCE_BACKDROP_COLOR_TRANSPARENT) {
                g_warning("invalid color style, setting to solid");
                color_style = XFCE_BACKDROP_COLOR_SOLID;
            }

            gtk_combo_box_set_active(GTK_COMBO_BOX(background_settings->color_style_combo),
                                     color_style);
            g_free(old_property);
        }

        xfconf_g_property_bind(channel, buf, G_TYPE_INT,
                               G_OBJECT(background_settings->color_style_combo), "active");
        /* update the color button sensitivity */
        cb_xfdesktop_combo_color_changed(GTK_COMBO_BOX(background_settings->color_style_combo), background_settings);
    }
    g_free(buf);

    /* color 1 button */
    /* Fixme: we will need to migrate from color1 to rgba1 (GdkColor to GdkRGBA) */
    buf = xfdesktop_settings_generate_per_workspace_binding_string(background_settings, "rgba1");
    if (remove_binding) {
        xfconf_g_property_unbind(background_settings->color1_btn_id);
    } else {
        /* If the first color doesn't exist, try to load the old one */
        if (!xfconf_channel_has_property(channel, buf)) {
            GValue value = { 0, };
            old_property = xfdesktop_settings_generate_old_binding_string(background_settings, "rgba1");

            xfconf_channel_get_property(channel, old_property, &value);

            if (G_VALUE_HOLDS_BOXED(&value)) {
                gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(background_settings->color1_btn),
                                           g_value_get_boxed(&value));
                g_value_unset(&value);
            } else {
                /* revert to showing our default color */
                GdkRGBA color1;
                color1.red = 0.5f;
                color1.green = 0.5f;
                color1.blue = 0.5f;
                color1.alpha = 1.0f;
                gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(background_settings->color1_btn), &color1);
            }

            g_free(old_property);
        }

        /* Now bind to the new value */
        background_settings->color1_btn_id = xfconf_g_property_bind(channel,
                                                                    buf,
                                                                    G_TYPE_PTR_ARRAY,
                                                                    G_OBJECT(background_settings->color1_btn),
                                                                    "rgba");
    }
    g_free(buf);

    /* color 2 button */
    /* Fixme: we will need to migrate from color1 to rgba1 (GdkColor to GdkRGBA) */
    buf = xfdesktop_settings_generate_per_workspace_binding_string(background_settings, "rgba2");
    if (remove_binding) {
        xfconf_g_property_unbind(background_settings->color2_btn_id);
    } else {
        /* If the 2nd color doesn't exist, try to load the old one */
        if (!xfconf_channel_has_property(channel, buf)) {
            GValue value = { 0, };
            old_property = xfdesktop_settings_generate_old_binding_string(background_settings, "rgba2");

            xfconf_channel_get_property(channel, old_property, &value);

            if(G_VALUE_HOLDS_BOXED(&value)) {
                gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(background_settings->color2_btn),
                                           g_value_get_boxed(&value));
                g_value_unset(&value);
            } else {
                /* revert to showing our default color */
                GdkRGBA color2;
                color2.red = 0.5f;
                color2.green = 0.5f;
                color2.blue = 0.5f;
                color2.alpha = 1.0f;
                gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(background_settings->color2_btn), &color2);
            }

            g_free(old_property);
        }

        /* Now bind to the new value */
        background_settings->color2_btn_id = xfconf_g_property_bind(channel,
                                                                    buf,
                                                                    G_TYPE_PTR_ARRAY,
                                                                    G_OBJECT(background_settings->color2_btn),
                                                                    "rgba");
    }
    g_free(buf);

    /* enable cycle timer check box */
    buf = xfdesktop_settings_generate_per_workspace_binding_string(background_settings, "backdrop-cycle-enable");
    if (remove_binding) {
        xfconf_g_property_unbind_by_property(channel,
                                             buf,
                                             G_OBJECT(background_settings->backdrop_cycle_chkbox),
                                             "active");
    } else {
        /* The default is disable cycling, set that before we bind to anything */
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(background_settings->backdrop_cycle_chkbox), FALSE);

        xfconf_g_property_bind(channel, buf, G_TYPE_BOOLEAN,
                               G_OBJECT(background_settings->backdrop_cycle_chkbox), "active");
    }
    g_free(buf);

    /* backdrop cycle period combo box */
    buf = xfdesktop_settings_generate_per_workspace_binding_string(background_settings, "backdrop-cycle-period");
    if (remove_binding) {
        xfconf_g_property_unbind_by_property(channel, buf,
                                             G_OBJECT(background_settings->combo_backdrop_cycle_period),
                                             "active");
    } else {
        /* default is minutes, set that before we bind to it */
        gtk_combo_box_set_active(GTK_COMBO_BOX(background_settings->combo_backdrop_cycle_period), XFCE_BACKDROP_PERIOD_MINUTES);

        xfconf_g_property_bind(channel, buf, G_TYPE_INT,
                               G_OBJECT(background_settings->combo_backdrop_cycle_period), "active");
        /* determine if the cycle timer spin box is sensitive */
        cb_combo_backdrop_cycle_period_change(GTK_COMBO_BOX(background_settings->combo_backdrop_cycle_period), background_settings);
    }
    g_free(buf);

    /* cycle timer spin box */
    buf = xfdesktop_settings_generate_per_workspace_binding_string(background_settings, "backdrop-cycle-timer");
    GtkAdjustment *adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(background_settings->backdrop_cycle_spinbox));
    if (remove_binding) {
        xfconf_g_property_unbind_by_property(channel, buf, G_OBJECT(adj), "value");
    } else {
        guint current_timer = xfconf_channel_get_uint(channel, buf, 10);
        /* Update the spin box before we bind to it */
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(background_settings->backdrop_cycle_spinbox), current_timer);

        xfconf_g_property_bind(channel, buf, G_TYPE_UINT, adj, "value");
    }
    g_free(buf);

    /* cycle random order check box */
    buf = xfdesktop_settings_generate_per_workspace_binding_string(background_settings, "backdrop-cycle-random-order");
    if (remove_binding) {
        xfconf_g_property_unbind_by_property(channel, buf,
                                             G_OBJECT(background_settings->random_backdrop_order_chkbox),
                                             "active");
    } else {
        /* The default is sequential, set that before we bind to anything */
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(background_settings->random_backdrop_order_chkbox), FALSE);

        xfconf_g_property_bind(channel, buf, G_TYPE_BOOLEAN,
                               G_OBJECT(background_settings->random_backdrop_order_chkbox), "active");
    }
    g_free(buf);
}

static gint
xfdesktop_settings_get_active_workspace(XfdesktopBackgroundSettings *background_settings, XfwWindow *xfw_window) {
    XfwScreen *xfw_screen = xfw_window_get_screen(xfw_window);
    XfwWorkspaceManager *workspace_manager = xfw_screen_get_workspace_manager(xfw_screen);

    XfwWorkspace *workspace = xfw_window_get_workspace(xfw_window);

    /* If workspace is NULL that means it's pinned and we just need to
     * use the active/current workspace */
    gint workspace_num = -1, n_workspaces = 0;
    if (workspace != NULL) {
        xfdesktop_workspace_get_number_and_total(workspace_manager, workspace, &workspace_num, &n_workspaces);
    } else {
        workspace_num = background_settings->active_workspace;
        n_workspaces = workspace_count_total(workspace_manager);
    }

    gboolean single_workspace = xfconf_channel_get_bool(background_settings->settings->channel,
                                                        SINGLE_WORKSPACE_MODE,
                                                        TRUE);

    /* If we're in single_workspace mode we need to return the workspace that
     * it was set to, if that workspace exists, otherwise return the current
     * workspace and turn off the single workspace mode */
    if (single_workspace) {
        gint single_workspace_num = xfconf_channel_get_int(background_settings->settings->channel,
                                                           SINGLE_WORKSPACE_NUMBER,
                                                           0);
        if (single_workspace_num < n_workspaces) {
            return single_workspace_num;
        } else {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(background_settings->chk_apply_to_all), FALSE);
        }
    }

    return workspace_num;
}

static void
cb_update_background_tab(XfwWindow *xfw_window, XfdesktopBackgroundSettings *background_settings) {
    /* If we haven't found our window return now and wait for that */
    if (background_settings->xfw_window == NULL) {
        return;
    }

    /* Get all the new settings for comparison */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gint screen_num = gdk_screen_get_number(gtk_widget_get_screen(background_settings->image_iconview));
G_GNUC_END_IGNORE_DEPRECATIONS
    XfwWorkspace *workspace = xfw_window_get_workspace(background_settings->xfw_window);
    gint workspace_num = xfdesktop_settings_get_active_workspace(background_settings, xfw_window);
    GdkWindow *window = gtk_widget_get_window(background_settings->image_iconview);
    GdkDisplay *display = gdk_window_get_display(window);
    GdkMonitor *monitor = gdk_display_get_monitor_at_window(display, window);
    gint monitor_num = xfdesktop_get_monitor_num(display, monitor);
    gchar *monitor_name = xfdesktop_get_monitor_name_from_gtk_widget(background_settings->image_iconview, monitor_num);

    /* Most of the time we won't change monitor, screen, or workspace so try
     * to bail out now if we can */
    /* Check to see if we haven't changed workspace or screen */
    if (background_settings->workspace == workspace_num && background_settings->screen == screen_num) {
        /* do we support monitor names? */
        if(background_settings->monitor_name != NULL || monitor_name != NULL) {
            /* Check if we haven't changed monitors (by name) */
            if(g_strcmp0(background_settings->monitor_name, monitor_name) == 0) {
                g_free(monitor_name);
                return;
            }
        }
    }

    TRACE("screen, monitor, or workspace changed");

    /* remove the old bindings if there are any */
    if (background_settings->color1_btn_id || background_settings->color2_btn_id) {
        XF_DEBUG("removing old bindings");
        xfdesktop_settings_background_tab_change_bindings(background_settings, TRUE);
    }

    g_free(monitor_name);

    background_settings->workspace = workspace_num;
    background_settings->screen = screen_num;
    background_settings->monitor = monitor_num;
    g_free(background_settings->monitor_name);
    background_settings->monitor_name = xfdesktop_get_monitor_name_from_gtk_widget(background_settings->image_iconview, monitor_num);

    /* The first monitor has the option of doing the "spanning screens" style,
     * but only if there's multiple monitors attached. Remove it in all other cases.
     *
     * Remove the spanning screens option before we potentially add it again
     */
    gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(background_settings->image_style_combo),
                              XFCE_BACKDROP_IMAGE_SPANNING_SCREENS);

    if (background_settings->monitor == 0
        && gdk_display_get_n_monitors(gtk_widget_get_display(background_settings->image_style_combo)) > 1)
    {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(background_settings->image_style_combo),
                                       _("Spanning screens"));
    }

    /* connect the new bindings */
    xfdesktop_settings_background_tab_change_bindings(background_settings, FALSE);

    xfdesktop_settings_update_iconview_frame_name(background_settings, workspace);
    xfdesktop_settings_update_iconview_folder(background_settings);
}

static void
cb_workspace_changed(XfwWorkspaceGroup *group,
                     XfwWorkspace *previously_active_workspace,
                     XfdesktopBackgroundSettings *background_settings)
{
    /* Update the current workspace number */
    XfwWorkspace *active_workspace = xfw_workspace_group_get_active_workspace(group);
    gint new_num = -1;
    if (active_workspace != NULL) {
        XfwWorkspaceManager *workspace_manager = xfw_workspace_group_get_workspace_manager(group);
        gint n_ws;
        xfdesktop_workspace_get_number_and_total(workspace_manager, active_workspace, &new_num, &n_ws);
    }

    /* This will sometimes fail to update */
    if (new_num != -1) {
        background_settings->active_workspace = new_num;
    }

    XF_DEBUG("active_workspace now %d", background_settings->active_workspace);

    /* Call cb_update_background_tab in case this is a pinned window */
    if (background_settings->xfw_window != NULL) {
        cb_update_background_tab(background_settings->xfw_window, background_settings);
    }
}

static gboolean
is_our_window(XfdesktopBackgroundSettings *background_settings, XfwWindow *window) {
    gboolean matches = FALSE;
    GtkWidget *toplevel = background_settings->settings->settings_toplevel;

#ifdef ENABLE_X11
    if (xfw_windowing_get() == XFW_WINDOWING_X11) {
        GdkWindow *toplevel_window = gtk_widget_get_window(toplevel);
        Window our_xid = gdk_x11_window_get_xid(toplevel_window);
        Window window_xid = xfw_window_x11_get_xid(window);
        Window cur_win = our_xid;
        GdkDisplay *gdpy = gdk_window_get_display(toplevel_window);
        Display *dpy = gdk_x11_display_get_xdisplay(gdpy);
        Window root_win = gdk_x11_window_get_xid(gdk_screen_get_root_window(gtk_widget_get_screen(toplevel)));

        // When we're embedded in the settings manager, the XID of 'window' is that of the settings
        // manager's main window, whereas our XID is the ID of the GtkPlug window.
        while (cur_win != window_xid && cur_win != root_win) {
            Window parent_win = 0;
            Window *children = NULL;
            unsigned int n_children = 0;

            if (XQueryTree(dpy, cur_win, &root_win, &parent_win, &children, &n_children) != 0) {
                if (children != NULL) {
                    XFree(children);
                    children = NULL;
                }
                cur_win = parent_win;
            } else {
                cur_win = 0;
                break;
            }
        }

        matches = cur_win == window_xid;
    } else
#endif  /* ENABLE_X11 */
#ifdef ENABLE_WAYLAND
    if (xfw_windowing_get() == XFW_WINDOWING_WAYLAND) {
        GtkWindow *toplevel_win = GTK_WINDOW(toplevel);
        const gchar *window_name = xfw_window_get_name(window);
        const gchar *toplevel_name = gtk_window_get_title(toplevel_win);

        g_message("No good way to match a GdkWindow and XfwWindow for Wayland; guessing");

        if (window_name != NULL && g_strcmp0(window_name, toplevel_name) == 0
            && gtk_window_is_maximized(toplevel_win) == xfw_window_is_maximized(window)
            && ((gdk_window_get_state(gtk_widget_get_window(toplevel)) & GDK_WINDOW_STATE_FULLSCREEN) != 0) == xfw_window_is_fullscreen(window))
        {
            matches = TRUE;
        }
        // TODO: check window geometry?
    } else
#endif  /* ENABLE_WAYLAND */
    {
        g_assert_not_reached();
    }

    return matches;
}

static void
cb_window_opened(XfwScreen *screen, XfwWindow *window, XfdesktopBackgroundSettings *background_settings) {
    /* If we already found our window, exit */
    if (background_settings->xfw_window != NULL) {
        return;
    }

    /* If they don't match then it's not our window, exit */
    if (!is_our_window(background_settings, window)) {
        return;
    }

    XF_DEBUG("Found our window");
    background_settings->xfw_window = window;

    /* These callbacks are for updating the image_iconview when the window
     * moves to another monitor or workspace */
    g_signal_connect(background_settings->xfw_window, "geometry-changed",
                     G_CALLBACK(cb_update_background_tab), background_settings);
    g_signal_connect(background_settings->xfw_window, "workspace-changed",
                     G_CALLBACK(cb_update_background_tab), background_settings);

    XfwWorkspaceGroup *group = NULL;
    XfwWorkspace *workspace = xfw_window_get_workspace(window);
    if (workspace == NULL) {
        XfwWorkspaceManager *workspace_manager = xfw_screen_get_workspace_manager(screen);
        for (GList *l = xfw_workspace_manager_list_workspace_groups(workspace_manager);
             l != NULL;
             l = l->next)
        {
            workspace = xfw_workspace_group_get_active_workspace(XFW_WORKSPACE_GROUP(l->data));
            if (workspace != NULL) {
                group = XFW_WORKSPACE_GROUP(l->data);
                break;
            }
        }
    } else {
        group = xfw_workspace_get_workspace_group(workspace);
    }

    if (group != NULL) {
        /* Update the active workspace number */
        cb_workspace_changed(group, NULL, background_settings);
    } else {
        g_message("Couldn't find active workspace or group");
    }

    /* Update the background settings */
    cb_update_background_tab(window, background_settings);

    /* Update the frame name */
    xfdesktop_settings_update_iconview_frame_name(background_settings, workspace);
}

#ifdef ENABLE_X11
static void
cb_active_window_changed(XfwScreen *screen,
                         XfwWindow *previously_active_window,
                         XfdesktopBackgroundSettings *background_settings)
{
    XfwWindow *window = xfw_screen_get_active_window(screen);

    if (background_settings->xfw_window == NULL && window != NULL) {
        cb_window_opened(screen, window, background_settings);

        if (background_settings->xfw_window != NULL) {
            g_signal_handlers_disconnect_by_func(screen, cb_active_window_changed, background_settings);
        }
    }
}
#endif

static void
cb_monitor_changed(GdkScreen *gscreen, XfdesktopBackgroundSettings *background_settings) {
    if (gdk_display_get_n_monitors(gdk_screen_get_display(gscreen)) > 0) {
        /* Update background because the monitor we're on may have changed */
        cb_update_background_tab(background_settings->xfw_window, background_settings);

        /* Update the frame name because we may change from/to a single monitor */
        xfdesktop_settings_update_iconview_frame_name(background_settings,
                                                      xfw_window_get_workspace(background_settings->xfw_window));
    }
}

static void
cb_xfdesktop_chk_apply_to_all(GtkCheckButton *button, XfdesktopBackgroundSettings *background_settings) {
    TRACE("entering");

    gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
    xfconf_channel_set_bool(background_settings->settings->channel,
                            SINGLE_WORKSPACE_MODE,
                            active);

    if (active) {
        xfconf_channel_set_int(background_settings->settings->channel,
                               SINGLE_WORKSPACE_NUMBER,
                               background_settings->workspace);
    } else {
        cb_update_background_tab(background_settings->xfw_window, background_settings);
    }

    /* update the frame name to since we changed to/from single workspace mode */
    xfdesktop_settings_update_iconview_frame_name(background_settings,
                                                  xfw_window_get_workspace(background_settings->xfw_window));
}

static void
xfdesktop_settings_setup_image_iconview(XfdesktopBackgroundSettings *background_settings) {
    GtkIconView *iconview = GTK_ICON_VIEW(background_settings->image_iconview);
    GList *cells;

    TRACE("entering");

    background_settings->thumbnailer = xfdesktop_thumbnailer_new();

    g_object_set(G_OBJECT(iconview),
                "pixbuf-column", COL_PIX,
                "tooltip-column", COL_NAME,
                "selection-mode", GTK_SELECTION_BROWSE,
                "column-spacing", 1,
                "row-spacing", 1,
                "item-padding", 10,
                "margin", 2,
                NULL);

    cells = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(iconview));
    for (GList *l = cells; l != NULL; l = l->next) {
        if (GTK_IS_CELL_RENDERER_PIXBUF(l->data)) {
            gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(iconview),
                                           GTK_CELL_RENDERER(l->data),
                                           "surface", COL_SURFACE,
                                           NULL);
            break;
        }
    }
    g_list_free(cells);

    g_signal_connect(G_OBJECT(iconview), "selection-changed",
                     G_CALLBACK(cb_image_selection_changed), background_settings);

    g_signal_connect(background_settings->thumbnailer, "thumbnail-ready",
                     G_CALLBACK(cb_thumbnail_ready), background_settings);
}

static void
cb_workspace_group_created(XfwWorkspaceManager *manager,
                           XfwWorkspaceGroup *group,
                           XfdesktopBackgroundSettings *background_settings)
{
    g_signal_connect(group, "active-workspace-changed",
                     G_CALLBACK(cb_workspace_changed), background_settings);
}

static void
cb_workspace_count_changed(XfwWorkspaceManager *manager,
                           XfwWorkspace *workspace,
                           XfdesktopBackgroundSettings *background_settings)
{
    /* Update background because the single workspace mode may have
     * changed due to the addition/removal of a workspace */
    if (background_settings->xfw_window != NULL) {
        cb_update_background_tab(background_settings->xfw_window, background_settings);
    }
}

static void
workspace_tracking_init(XfdesktopBackgroundSettings *background_settings) {
    background_settings->xfw_screen = xfw_screen_get_default();
    GdkScreen *gscreen = gtk_widget_get_screen(background_settings->settings->settings_toplevel);

#ifdef ENABLE_X11
    if (xfw_windowing_get() == XFW_WINDOWING_X11) {
        if (GTK_IS_PLUG(background_settings->settings->settings_toplevel)) {
            /* In a GtkPlug setting there isn't an easy way to find our window
             * in cb_window_opened so we'll just get the screen's active window */
            background_settings->xfw_window = xfw_screen_get_active_window(background_settings->xfw_screen);

            if (background_settings->xfw_window != NULL) {
                /* These callbacks are for updating the image_iconview when the window
                 * moves to another monitor or workspace */
                g_signal_connect(background_settings->xfw_window, "geometry-changed",
                                 G_CALLBACK(cb_update_background_tab), background_settings);
                g_signal_connect(background_settings->xfw_window, "workspace-changed",
                                 G_CALLBACK(cb_update_background_tab), background_settings);
            } else {
                // XfwScreen doesn't know the active window yet, so wait until it
                // figures it out
                g_signal_connect(background_settings->xfw_screen, "active-window-changed",
                                 G_CALLBACK(cb_active_window_changed), background_settings);
            }
        }

        background_settings->screen = gdk_x11_screen_get_screen_number(gscreen);
    } else 
#endif
    {
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        background_settings->screen = gdk_screen_get_number(gscreen);
        G_GNUC_END_IGNORE_DEPRECATIONS
    }

    XfwWorkspaceManager *workspace_manager = xfw_screen_get_workspace_manager(background_settings->xfw_screen);
    g_signal_connect(workspace_manager, "workspace-group-created",
                     G_CALLBACK(cb_workspace_group_created), background_settings);
    g_signal_connect(workspace_manager, "workspace-created",
                     G_CALLBACK(cb_workspace_count_changed), background_settings);
    g_signal_connect(workspace_manager, "workspace-destroyed",
                     G_CALLBACK(cb_workspace_count_changed), background_settings);

    /* watch for workspace changes */
    for (GList *l = xfw_workspace_manager_list_workspace_groups(workspace_manager);
         l != NULL;
         l = l->next)
    {
        g_signal_connect(l->data, "active-workspace-changed",
                         G_CALLBACK(cb_workspace_changed), background_settings);
    }

    /* watch for window-opened so we can find our window and track its changes */
    g_signal_connect(background_settings->xfw_screen, "window-opened",
                     G_CALLBACK(cb_window_opened), background_settings);

}

XfdesktopBackgroundSettings *
xfdesktop_background_settings_init(XfdesktopSettings *settings) {
    g_return_val_if_fail(settings != NULL, NULL);
    g_return_val_if_fail(GTK_IS_BUILDER(settings->main_gxml), NULL);

    GtkBuilder *appearance_gxml = gtk_builder_new();
    GError *error = NULL;
    if (gtk_builder_add_from_resource(appearance_gxml,
                                      "/org/xfce/xfdesktop/settings/xfdesktop-settings-appearance-frame-ui.glade",
                                      &error) == 0)
    {
        g_printerr("Failed to parse appearance settings UI description: %s\n",
                   error->message);
        g_clear_error(&error);
        return NULL;
    }

    XfdesktopBackgroundSettings *background_settings = g_new0(XfdesktopBackgroundSettings, 1);
    background_settings->settings = settings;

    workspace_tracking_init(background_settings);

    GdkScreen *screen = gtk_widget_get_screen(background_settings->settings->settings_toplevel);
    /* watch for monitor changes */
    g_signal_connect(G_OBJECT(screen), "monitors-changed",
                     G_CALLBACK(cb_monitor_changed), background_settings);

    GtkWidget *appearance_container = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml, "notebook_screens"));

    GtkWidget *appearance_settings = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                                       "background_tab_vbox"));

    /* Add the background tab widgets to the main window and don't display the
     * notebook label/tab */
    gtk_notebook_append_page(GTK_NOTEBOOK(appearance_container),
                             appearance_settings, NULL);
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(appearance_container), FALSE);

    /* icon view area */
    background_settings->infobar = GTK_WIDGET(gtk_builder_get_object(appearance_gxml, "infobar_header"));
    background_settings->infobar_label = GTK_WIDGET(gtk_builder_get_object(appearance_gxml, "infobar_label"));

    background_settings->label_header = GTK_WIDGET(gtk_builder_get_object(appearance_gxml, "label_header"));

    background_settings->image_iconview = GTK_WIDGET(gtk_builder_get_object(appearance_gxml, "iconview_imagelist"));
    xfdesktop_settings_setup_image_iconview(background_settings);

    /* folder: file chooser button */
    background_settings->btn_folder = GTK_WIDGET(gtk_builder_get_object(appearance_gxml, "btn_folder"));
    g_signal_connect(G_OBJECT(background_settings->btn_folder), "selection-changed",
                     G_CALLBACK(cb_folder_selection_changed), background_settings);
    background_settings->btn_folder_apply = GTK_WIDGET(gtk_builder_get_object(appearance_gxml, "btn_folder_apply"));
    g_signal_connect(G_OBJECT(background_settings->btn_folder_apply), "clicked",
                     G_CALLBACK(cb_folder_apply_clicked), background_settings);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, _("Image files"));
    gtk_file_filter_add_pixbuf_formats(filter);
    gtk_file_filter_add_mime_type(filter, "inode/directory");
    gtk_file_filter_add_mime_type(filter, "application/x-directory");
    gtk_file_filter_add_mime_type(filter, "text/directory");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(background_settings->btn_folder), filter);

    /* Change the title of the file chooser dialog */
    gtk_file_chooser_button_set_title(GTK_FILE_CHOOSER_BUTTON(background_settings->btn_folder), _("Select a Directory"));

    /* Get default wallpaper folders */
    GList *background_dirs = find_background_directories();
    for (GList *l = background_dirs; l != NULL; l = l->next) {
        GFile *file = G_FILE(l->data);
        gchar *uri_path = g_file_get_uri(file);
        gtk_file_chooser_add_shortcut_folder_uri(GTK_FILE_CHOOSER(background_settings->btn_folder), uri_path, NULL);
        g_free(uri_path);
    }
    g_list_free_full(background_dirs, g_object_unref);

    /* Image and color style options */
    background_settings->image_style_combo = GTK_WIDGET(gtk_builder_get_object(appearance_gxml, "combo_style"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(background_settings->image_style_combo), XFCE_BACKDROP_IMAGE_ZOOMED);
    g_signal_connect(G_OBJECT(background_settings->image_style_combo), "changed",
                     G_CALLBACK(cb_xfdesktop_combo_image_style_changed),
                     background_settings);

    background_settings->color_style_combo = GTK_WIDGET(gtk_builder_get_object(appearance_gxml, "combo_colors"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(background_settings->color_style_combo), XFCE_BACKDROP_COLOR_SOLID);
    g_signal_connect(G_OBJECT(background_settings->color_style_combo), "changed",
                     G_CALLBACK(cb_xfdesktop_combo_color_changed),
                     background_settings);

    background_settings->color1_btn = GTK_WIDGET(gtk_builder_get_object(appearance_gxml, "color1_btn"));
    background_settings->color2_btn = GTK_WIDGET(gtk_builder_get_object(appearance_gxml, "color2_btn"));

    /* Use these settings for all workspaces checkbox */
    background_settings->chk_apply_to_all = GTK_WIDGET(gtk_builder_get_object(appearance_gxml, "chk_apply_to_all"));
    if (xfconf_channel_get_bool(background_settings->settings->channel, SINGLE_WORKSPACE_MODE, TRUE)) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(background_settings->chk_apply_to_all), TRUE);
    }
    g_signal_connect(G_OBJECT(background_settings->chk_apply_to_all), "toggled",
                    G_CALLBACK(cb_xfdesktop_chk_apply_to_all),
                    background_settings);

    /* background cycle timer */
    background_settings->backdrop_cycle_chkbox = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                                                   "chk_cycle_backdrop"));
    g_signal_connect(G_OBJECT(background_settings->backdrop_cycle_chkbox), "toggled",
                    G_CALLBACK(cb_xfdesktop_chk_cycle_backdrop_toggled),
                    background_settings);

    background_settings->combo_backdrop_cycle_period = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                                                         "combo_cycle_backdrop_period"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(background_settings->combo_backdrop_cycle_period),
                             XFCE_BACKDROP_PERIOD_MINUTES);
    g_signal_connect(G_OBJECT(background_settings->combo_backdrop_cycle_period), "changed",
                     G_CALLBACK(cb_combo_backdrop_cycle_period_change),
                     background_settings);

    background_settings->backdrop_cycle_spinbox = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                                                    "spin_backdrop_time"));
    background_settings->random_backdrop_order_chkbox = GTK_WIDGET(gtk_builder_get_object(appearance_gxml,
                                                                                          "chk_random_backdrop_order"));

    g_object_unref(G_OBJECT(appearance_gxml));

    cb_update_background_tab(background_settings->xfw_window, background_settings);

    return background_settings;
}

void
xfdesktop_background_settings_destroy(XfdesktopBackgroundSettings *background_settings) {
    GdkScreen *screen = gtk_widget_get_screen(background_settings->settings->settings_toplevel);
    g_signal_handlers_disconnect_by_data(screen, background_settings);
    if (background_settings->last_image_signal_id != 0) {
        g_signal_handler_disconnect(background_settings->settings->channel,
                                    background_settings->last_image_signal_id);
    }
    stop_image_loading(background_settings);
    g_free(background_settings->monitor_name);
    g_object_unref(background_settings->xfw_screen);
    g_free(background_settings);
}
