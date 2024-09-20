/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2024 Brian Tarricone, <brian@tarricone.org>
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
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4windowing/libxfce4windowing.h>
#include <xfconf/xfconf.h>

#include "xfdesktop-icon-position-configs.h"
#include "xfdesktop-icon-position-migration.h"
#include "xfdesktop-icon-view.h"

#define MIGRATED_MONITORS_PROP "/desktop-icons/file-icons/migrated-4.19"

static gboolean
already_migrated(XfconfChannel *channel, XfwMonitor *monitor) {
    gchar **migrated_ids = xfconf_channel_get_string_list(channel, MIGRATED_MONITORS_PROP);
    gboolean migrated = migrated_ids != NULL
        && g_strv_contains((const gchar *const *)migrated_ids, xfw_monitor_get_identifier(monitor));
    g_strfreev(migrated_ids);
    return migrated;
}

static void
mark_migrated(XfconfChannel *channel, XfwMonitor *monitor) {
    gchar **migrated_ids = xfconf_channel_get_string_list(channel, MIGRATED_MONITORS_PROP);
    gsize len = migrated_ids != NULL ? g_strv_length(migrated_ids) : 0;

    if (migrated_ids == NULL) {
        migrated_ids = g_new0(gchar *, 2);
    } else {
        migrated_ids = g_realloc_n(migrated_ids, len + 2, sizeof(gchar *));
    }
    migrated_ids[len] = g_strdup(xfw_monitor_get_identifier(monitor));
    migrated_ids[len + 1] = NULL;

    xfconf_channel_set_string_list(channel, MIGRATED_MONITORS_PROP, (const gchar *const *)migrated_ids);
    g_strfreev(migrated_ids);
}

static gboolean
get_full_workarea_geom(XfwScreen *screen, GdkRectangle *total_workarea) {
    GList *monitors = xfw_screen_get_monitors(screen);
    if (monitors == NULL) {
        DBG("weird, no monitors");
        return FALSE;
    } else {
        XfwMonitor *first_monitor = XFW_MONITOR(monitors->data);
        xfw_monitor_get_workarea(first_monitor, total_workarea);
        for (GList *l = monitors->next; l != NULL; l = l->next) {
            XfwMonitor *monitor = XFW_MONITOR(l->data);
            GdkRectangle workarea;
            xfw_monitor_get_workarea(monitor, &workarea);
            gdk_rectangle_union(total_workarea, &workarea, total_workarea);
        }

        return TRUE;
    }
}

static gboolean
get_icon_view_geometry(XfconfChannel *channel,
                       XfwScreen *screen,
                       GdkRectangle *total_workarea,
                       GdkRectangle *monitor_workarea,
                       gint *first_row,
                       gint *first_col,
                       gint *last_row,
                       gint *last_col)
{
    GtkWidget *dummy_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_realize(dummy_win);
    GtkWidget *dummy = xfdesktop_icon_view_new(channel, screen);
    gtk_container_add(GTK_CONTAINER(dummy_win), dummy);

    gboolean success = xfdesktop_icon_view_grid_geometry_for_metrics(XFDESKTOP_ICON_VIEW(dummy),
                                                                     total_workarea,
                                                                     monitor_workarea,
                                                                     first_row,
                                                                     first_col,
                                                                     last_row,
                                                                     last_col);

    gtk_widget_destroy(dummy_win);
    return success;
}

XfdesktopIconPositionConfig *
xfdesktop_icon_positions_try_migrate(XfconfChannel *channel, XfwScreen *screen, XfwMonitor *monitor, XfdesktopIconPositionLevel level) {
    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel), NULL);
    g_return_val_if_fail(XFW_IS_SCREEN(screen), NULL);
    g_return_val_if_fail(XFW_IS_MONITOR(monitor), NULL);

    TRACE("entering (%s)", xfw_monitor_get_identifier(monitor));

    XfdesktopIconPositionConfig *config = NULL;

    GdkRectangle workarea;
    xfw_monitor_get_workarea(monitor, &workarea);

    GdkRectangle total_workarea;
    gint first_row, first_col, last_row, last_col;
    if (already_migrated(channel, monitor)
        || !get_full_workarea_geom(screen, &total_workarea)
        || !get_icon_view_geometry(channel, screen, &total_workarea, &workarea, &first_row, &first_col, &last_row, &last_col))
    {
        return NULL;
    } else {
        DBG("Trying to migrate icon positions: total_workarea=%dx%d+%d+%d, workarea=%dx%d+%d+%d, grid area=(%d, %d)->(%d, %d)",
            total_workarea.width, total_workarea.height, total_workarea.x, total_workarea.y,
            workarea.width, workarea.height, workarea.x, workarea.y,
            first_row, first_col, last_row, last_col);

        // xfdesktop 4.12 through 4.18 did a weird thing where it subtracts
        // (2 * MIN_MARGIN) from the workarea.  So we'll check for both the
        // filenames with the reasonable workarea and the weird workarea, and
        // if they both exist, we'll use whichever is newer.

        gchar *relpath = g_strdup_printf("xfce4/desktop/icons.screen%d-%dx%d.rc", 0, total_workarea.width, total_workarea.height);
        gchar *filename = xfce_resource_lookup(XFCE_RESOURCE_CONFIG, relpath);
        g_free(relpath);

        {
#define MIN_MARGIN 8
            gchar *relpath_418 = g_strdup_printf("xfce4/desktop/icons.screen%d-%dx%d.rc",
                                                 0,
                                                 total_workarea.width - 2 * MIN_MARGIN,
                                                 total_workarea.height - 2 * MIN_MARGIN);
            gchar *filename_418 = xfce_resource_lookup(XFCE_RESOURCE_CONFIG, relpath_418);
            g_free(relpath_418);

            if (filename != NULL && filename_418 != NULL) {
                GFile *file = g_file_new_for_path(filename);
                GFileInfo *file_info = g_file_query_info(file, "time::", G_FILE_QUERY_INFO_NONE, NULL, NULL);
                GFile *file_418 = g_file_new_for_path(filename_418);
                GFileInfo *file_info_418 = g_file_query_info(file_418, "time::", G_FILE_QUERY_INFO_NONE, NULL, NULL);

                if (file_info != NULL && file_info_418 != NULL) {
                    GDateTime *mtime = g_file_info_get_modification_date_time(file_info);
                    GDateTime *mtime_418 = g_file_info_get_modification_date_time(file_info_418);
                    if (g_date_time_compare(mtime_418, mtime) > 0) {
                        g_free(filename);
                        filename = filename_418;
                    } else {
                        g_free(filename_418);
                    }

                    g_date_time_unref(mtime);
                    g_date_time_unref(mtime_418);
                } else {
                    g_free(filename_418);
                }

                if (file_info != NULL) {
                    g_object_unref(file_info);
                }
                g_object_unref(file);
                if (file_info_418 != NULL) {
                    g_object_unref(file_info_418);
                }
                g_object_unref(file_418);
            } else if (filename_418 != NULL) {
                filename = filename_418;
            }
#undef MIN_MARGIN
        }

        if (filename == NULL) {
            // Try old format
            DBG("Didn't find dimensional icon position file; trying old-style path");
            gchar *old_relpath = g_strdup_printf("xfce4/desktop/icons.screen%d.rc", 0);
            filename = xfce_resource_lookup(XFCE_RESOURCE_CONFIG, old_relpath);
            g_free(old_relpath);
        }

        if (filename == NULL) {
            // Use latest as fallback
            DBG("Didn't find old-style path; using latest file");
            filename = xfce_resource_lookup(XFCE_RESOURCE_CONFIG, "xfce4/desktop/icons.screen.latest.rc");
        }

        if (filename != NULL) {
            DBG("Attempting to migrate icon positions from %s", filename);
            XfceRc *rcfile = xfce_rc_simple_open(filename, TRUE);

            if (rcfile != NULL) {
                gchar **groups = xfce_rc_get_groups(rcfile);

                config = xfdesktop_icon_position_config_new(level);
                guint nicons = 0;
                for (guint i = 0; groups[i] != NULL; ++i) {
                    xfce_rc_set_group(rcfile, groups[i]);
                    gint row = xfce_rc_read_int_entry(rcfile, "row", -1);
                    gint col = xfce_rc_read_int_entry(rcfile, "col", -1);

                    if (row >= first_row && row <= last_row && col >= first_col && col <= last_col) {
                        DBG("migrating icon \"%s\"", groups[i]);
                        gint monitor_row = row - first_row;
                        gint monitor_col = col - first_col;
                        _xfdesktop_icon_position_config_set_icon_position(config, groups[i], monitor_row, monitor_col);
                        nicons++;
                    }
                }

                if (nicons == 0) {
                    g_clear_pointer(&config, xfdesktop_icon_position_config_free);
                } else {
                    mark_migrated(channel, monitor);
                }

                g_strfreev(groups);
                xfce_rc_close(rcfile);
            }
        }
    }

    return config;
}
