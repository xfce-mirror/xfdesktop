/*
 *  Copyright (C) 2002 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *  Copyright (C) 2003 Benedikt Meurer (benedikt.meurer@unix-ag.uni-siegen.de)
 *  Copyright (c) 2004-2007,2024 Brian Tarricone <brian@tarricone.org>
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4windowing/libxfce4windowing.h>
#include <xfconf/xfconf.h>

#include "xfdesktop-common.h"

// Allow building with glib 2.72 (these appear first in 2.74).
#define XFDESKTOP_G_DEFINE_ENUM_VALUE(EnumValue, EnumNick) \
  { EnumValue, #EnumValue, EnumNick }
#define XFDESKTOP_G_DEFINE_ENUM_TYPE(TypeName, type_name, ...) \
GType \
type_name ## _get_type (void) { \
  static gsize g_define_type__static = 0; \
  if (g_once_init_enter (&g_define_type__static)) { \
    static const GEnumValue enum_values[] = { \
      __VA_ARGS__ , \
      { 0, NULL, NULL }, \
    }; \
    GType g_define_type = g_enum_register_static (g_intern_static_string (#TypeName), enum_values); \
    g_once_init_leave (&g_define_type__static, g_define_type); \
  } \
  return g_define_type__static; \
}

XFDESKTOP_G_DEFINE_ENUM_TYPE(XfceBackdropColorStyle, xfce_backdrop_color_style,
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_BACKDROP_COLOR_INVALID, "invalid"),
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_BACKDROP_COLOR_SOLID, "solid"),
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_BACKDROP_COLOR_HORIZ_GRADIENT, "horiz-gradient"),
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_BACKDROP_COLOR_VERT_GRADIENT, "vert-gradient"),
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_BACKDROP_COLOR_TRANSPARENT, "transparent")
)

XFDESKTOP_G_DEFINE_ENUM_TYPE(XfceBackdropImageStyle, xfce_backdrop_image_style,
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_BACKDROP_IMAGE_INVALID, "invalid"),
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_BACKDROP_IMAGE_NONE, "none"),
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_BACKDROP_IMAGE_CENTERED, "centered"),
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_BACKDROP_IMAGE_TILED, "tiled"),
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_BACKDROP_IMAGE_STRETCHED, "stretched"),
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_BACKDROP_IMAGE_SCALED, "scaled"),
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_BACKDROP_IMAGE_ZOOMED, "zoomed"),
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_BACKDROP_IMAGE_SPANNING_SCREENS, "spanning-screens")
)

XFDESKTOP_G_DEFINE_ENUM_TYPE(XfceBcakdropCyclePeriod, xfce_backdrop_cycle_period,
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_BACKDROP_PERIOD_INVALID, "invalid"),
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_BACKDROP_PERIOD_SECONDS, "seconds"),
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_BACKDROP_PERIOD_MINUTES, "minutes"),
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_BACKDROP_PERIOD_HOURS, "hours"),
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_BACKDROP_PERIOD_STARTUP, "startup"),
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_BACKDROP_PERIOD_HOURLY, "hourly"),
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_BACKDROP_PERIOD_DAILY, "daily"),
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_BACKDROP_PERIOD_CHRONOLOGICAL, "chronological")
)

XFDESKTOP_G_DEFINE_ENUM_TYPE(XfceDesktopIconStyle, xfce_desktop_icon_style,
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_DESKTOP_ICON_STYLE_NONE, "none"),
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_DESKTOP_ICON_STYLE_WINDOWS, "windows"),
    XFDESKTOP_G_DEFINE_ENUM_VALUE(XFCE_DESKTOP_ICON_STYLE_FILES, "files")
)

/* Free the string whe done using it */
gchar*
xfdesktop_get_monitor_name_from_gtk_widget(GtkWidget *widget, gint monitor_num)
{
    GdkWindow     *window = NULL;
    GdkDisplay    *display = NULL;
    GdkMonitor    *monitor = NULL;

    window = gtk_widget_get_window(widget);
    display = gdk_window_get_display(window);
    monitor = gdk_display_get_monitor(display, monitor_num);

    return g_strdup(gdk_monitor_get_model(monitor));
}

gint
xfdesktop_compare_paths(GFile *a, GFile *b)
{
    gchar *path_a, *path_b;
    gboolean ret;

    path_a = g_file_get_path(a);
    path_b = g_file_get_path(b);

    XF_DEBUG("a %s, b %s", path_a, path_b);

    ret = g_strcmp0(path_a, path_b);

    g_free(path_a);
    g_free(path_b);

    return ret;
}

gchar *
xfdesktop_get_file_mimetype(GFile *file)
{
    GFileInfo *file_info;
    gchar *mime_type = NULL;

    g_return_val_if_fail(G_IS_FILE(file), NULL);

    file_info = g_file_query_info(file,
                                  "standard::content-type",
                                  0,
                                  NULL,
                                  NULL);

    if(file_info != NULL) {
        mime_type = g_strdup(g_file_info_get_content_type(file_info));
        g_object_unref(file_info);
    }

    return mime_type;
}

gboolean
xfdesktop_image_file_is_valid(GFile *file)
{
    static GSList *pixbuf_formats = NULL;
    GSList *l;
    gboolean image_valid = FALSE;
    gchar *file_mimetype;

    g_return_val_if_fail(file != NULL, FALSE);

    if(pixbuf_formats == NULL) {
        pixbuf_formats = gdk_pixbuf_get_formats();
    }

    file_mimetype = xfdesktop_get_file_mimetype(file);

    if(file_mimetype == NULL)
        return FALSE;

    /* Every pixbuf format has a list of mime types we can compare against */
    for(l = pixbuf_formats; l != NULL && image_valid == FALSE; l = g_slist_next(l)) {
        gint i;
        gchar ** mimetypes = gdk_pixbuf_format_get_mime_types(l->data);

        for(i = 0; mimetypes[i] != NULL && image_valid == FALSE; i++) {
            if(g_strcmp0(file_mimetype, mimetypes[i]) == 0)
                image_valid = TRUE;
        }
         g_strfreev(mimetypes);
    }

    g_free(file_mimetype);

    return image_valid;
}

/* The image styles changed from versions prior to 4.11.
 * Auto isn't an option anymore, additionally we should handle invalid
 * values. Set them to the default of stretched. */
gint
xfce_translate_image_styles(gint input)
{
    gint style = input;

    if(style <= 0 || style > XFCE_BACKDROP_IMAGE_SPANNING_SCREENS)
        style = XFCE_BACKDROP_IMAGE_STRETCHED;

    return style;
}



/*
 * xfdesktop_remove_whitspaces:
 * remove all whitespaces from string (not only trailing or leading)
 */
gchar*
xfdesktop_remove_whitspaces(gchar* str)
{
    gchar* dest;
    guint offs, curr;

    g_return_val_if_fail(str, NULL);

    offs = 0;
    dest = str;
    for(curr=0; curr<=strlen(str); curr++) {
        if(*dest == ' ' || *dest == '\t')
            offs++;
        else if(0 != offs)
            *(dest-offs) = *dest;
        dest++;
    }

    return str;
}


static GtkWidget *
create_menu_item(const gchar *name,
                 GtkWidget *image)
{
    GtkWidget *mi;

    if (image == NULL) {
        mi = gtk_menu_item_new_with_label(name);
    } else {
        GtkSettings *settings;
        gboolean show_icons = TRUE;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        mi = gtk_image_menu_item_new_with_label(name);
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), image);
G_GNUC_END_IGNORE_DEPRECATIONS

        if (gtk_widget_has_screen(image)) {
            settings = gtk_settings_get_for_screen(gtk_widget_get_screen(image));
        } else {
            settings = gtk_settings_get_default();
        }
        gtk_widget_show(image);
        g_object_get(settings, "gtk-menu-images", &show_icons, NULL);
        gtk_widget_set_visible(image, show_icons);
    }

    gtk_widget_show(mi);

    return mi;
}

/* Adapted from garcon_gtk_menu_create_menu_item because I don't want
 * to write it over and over.
 */
GtkWidget*
xfdesktop_menu_create_menu_item_with_markup(const gchar *name,
                                            GtkWidget   *image)
{
    GtkWidget *mi = create_menu_item(name, image);
    GtkWidget *label = gtk_bin_get_child(GTK_BIN(mi));
    gtk_label_set_markup(GTK_LABEL(label), name);
    return mi;
}



GtkWidget*
xfdesktop_menu_create_menu_item_with_mnemonic(const gchar *name,
                                              GtkWidget   *image)
{
    GtkWidget *mi = create_menu_item(name, image);
    gtk_menu_item_set_use_underline(GTK_MENU_ITEM(mi), TRUE);
    return mi;
}



gint
xfdesktop_get_monitor_num(GdkDisplay *display,
                          GdkMonitor *monitor)
{
    gint i;

    g_return_val_if_fail(GDK_IS_DISPLAY(display), 0);
    g_return_val_if_fail(GDK_IS_MONITOR(monitor), 0);

    for(i=0; i<gdk_display_get_n_monitors(display); i++) {
        if(monitor == gdk_display_get_monitor(display, i))
            return i;
    }

    g_warning("unable to get the monitor number");
    return 0;
}



gint
xfdesktop_get_current_monitor_num(GdkDisplay *display)
{
    GdkSeat    *seat;
    GdkMonitor *monitor;
    gint        rootx, rooty;

    g_return_val_if_fail(GDK_IS_DISPLAY(display), 0);

    seat = gdk_display_get_default_seat(display);
    gdk_device_get_position(gdk_seat_get_pointer(seat), NULL, &rootx, &rooty);
    monitor = gdk_display_get_monitor_at_point(display, rootx, rooty);

    return xfdesktop_get_monitor_num(display, monitor);
}

/* Gets the workspace number across all workspace groups, and also returns
 * the total number of workspaces.
 */
gboolean
xfdesktop_workspace_get_number_and_total(XfwWorkspaceManager *workspace_manager,
                                         XfwWorkspace *workspace,
                                         gint *workspace_number,
                                         gint *total_workspace_count)
{
    XfwWorkspaceGroup *group = xfw_workspace_get_workspace_group(workspace);

    g_return_val_if_fail(workspace_number != NULL, FALSE);
    g_return_val_if_fail(total_workspace_count != NULL, FALSE);

    *workspace_number = -1;
    *total_workspace_count = 0;

    for (GList *l = xfw_workspace_manager_list_workspace_groups(workspace_manager);
         l != NULL;
         l = l->next)
    {
        XfwWorkspaceGroup *g = XFW_WORKSPACE_GROUP(l->data);
        if (g == group) {
            *workspace_number = *total_workspace_count + xfw_workspace_get_number(workspace);
        }
        *total_workspace_count += xfw_workspace_group_get_workspace_count(XFW_WORKSPACE_GROUP(l->data));
    }

    return *workspace_number >= 0;
}

GtkWindow *
xfdesktop_find_toplevel(GtkWidget *widget)
{
    GtkWidget *toplevel = NULL;
    GtkWidget *cur = widget;

    while (cur != NULL && toplevel == NULL) {
        if (GTK_IS_MENU(cur)) {
            toplevel = gtk_menu_get_attach_widget(GTK_MENU(cur));
            if (!GTK_IS_WINDOW(toplevel)) {
                toplevel = NULL;
            }
        }

        if (toplevel == NULL && GTK_IS_WINDOW(cur)) {
            toplevel = cur;
        }

        cur = gtk_widget_get_parent(cur);
    }

    return toplevel != NULL && GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : NULL;
}


void
xfdesktop_widget_unrealize(GtkWidget *widget,
                           gpointer data)
{
    gtk_widget_unrealize(widget);
}



void
xfdesktop_object_ref(gpointer data,
                     gpointer user_data)
{
    g_object_ref(data);
}



void
xfdesktop_object_unref(gpointer data,
                       GClosure *closure)
{
    g_object_unref(data);
}

XfwSeat *
xfdesktop_find_xfw_seat_for_gdk_seat(XfwScreen *screen, GdkSeat *gdk_seat) {
    g_return_val_if_fail(XFW_IS_SCREEN(screen), NULL);
    g_return_val_if_fail(gdk_seat == NULL || GDK_IS_SEAT(gdk_seat), NULL);

    if (gdk_seat == NULL) {
        gdk_seat = gdk_display_get_default_seat(gdk_display_get_default());
    }
    GdkDisplay *display = gdk_seat_get_display(gdk_seat);
    GList *gseats = gdk_display_list_seats(display);
    GList *xseats = xfw_screen_get_seats(screen);

    XfwSeat *xseat = NULL;

    if (g_list_length(gseats) == g_list_length(xseats)) {
        for (GList *gl = gseats, *xl = xseats; gl != NULL && xl != NULL; gl = gl->next, xl = xl->next) {
            if (gdk_seat == GDK_SEAT(gl->data)) {
                xseat = XFW_SEAT(xl->data);
                break;
            }
        }
    }

    g_list_free(gseats);

    return xseat;
}

XfwWorkspace *
xfdesktop_find_active_workspace_on_monitor(XfwScreen *screen, XfwMonitor *monitor) {
    XfwWorkspaceManager *workspace_manager = xfw_screen_get_workspace_manager(screen);
    XfwWorkspaceGroup *group = NULL;
    if (monitor != NULL) {
        for (GList *l = xfw_workspace_manager_list_workspace_groups(workspace_manager); l != NULL; l = l->next) {
            if (g_list_find(xfw_workspace_group_get_monitors(XFW_WORKSPACE_GROUP(l->data)), monitor)) {
                group = XFW_WORKSPACE_GROUP(l->data);
                return xfw_workspace_group_get_active_workspace(group);
            }
        }
    }

    return NULL;
}

#ifdef G_ENABLE_DEBUG
/* With --enable-debug=full turn on debugging messages from the start */
static gboolean enable_debug = TRUE;
#else
static gboolean enable_debug = FALSE;
#endif /* G_ENABLE_DEBUG */

#if defined(G_HAVE_ISO_VARARGS)
void
xfdesktop_debug(const char *func, const char *file, int line, const char *format, ...)
{
    va_list args;

    if(!enable_debug)
        return;

    va_start(args, format);

    fprintf(stdout, "DBG[%s:%d] %s(): ", file, line, func);
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");

    va_end(args);
}
#endif /* defined(G_HAVE_ISO_VARARGS) */

/**
 * xfdesktop_debug_set:
 * debug: TRUE to turn on the XF_DEBUG mesages.
 */
void
xfdesktop_debug_set(gboolean debug)
{
    enable_debug = debug;
    if(enable_debug)
        XF_DEBUG("debugging enabled");
}

#define LAST_SETTINGS_MIGRATION_PROP "/last-settings-migration-version"
#define CUR_SETTINGS_MIGRATION_VERSION 1

static gboolean
maybe_migrate_gdkcolor(XfconfChannel *channel,
                       const gchar *name,
                       const GValue *value,
                       int screen_num,
                       const gchar *monitor_name,
                       const char *setting_name,
                       gint n_workspaces)
{
    gboolean ok = FALSE;

    gboolean is_color1 = strcmp(setting_name, "color1") == 0;
    gboolean is_color2 = !is_color1 && strcmp(setting_name, "color2") == 0;
    if (is_color1 || is_color2) {
        if (G_VALUE_HOLDS(value, G_TYPE_PTR_ARRAY)) {
            GPtrArray *array = g_value_get_boxed(value);
            if (array->len == 4) {
                GValue *redv = g_ptr_array_index(array, 0);
                GValue *greenv = g_ptr_array_index(array, 1);
                GValue *bluev = g_ptr_array_index(array, 2);
                GValue *alphav = g_ptr_array_index(array, 3);

                if (G_VALUE_HOLDS(redv, XFCONF_TYPE_UINT16) &&
                    G_VALUE_HOLDS(greenv, XFCONF_TYPE_UINT16) &&
                    G_VALUE_HOLDS(bluev, XFCONF_TYPE_UINT16) &&
                    G_VALUE_HOLDS(alphav, XFCONF_TYPE_UINT16))
                {
                    const gchar *new_setting_name = is_color1 ? "rgba1" : "rgba2";

                    gdouble red = CLAMP((gdouble)g_value_get_uint(redv) / G_MAXUINT16, 0.0, 1.0);
                    gdouble green = CLAMP((gdouble)g_value_get_uint(greenv) / G_MAXUINT16, 0.0, 1.0);
                    gdouble blue = CLAMP((gdouble)g_value_get_uint(bluev) / G_MAXUINT16, 0.0, 1.0);
                    gdouble alpha = CLAMP((gdouble)g_value_get_uint(alphav) / G_MAXUINT16, 0.0, 1.0);

                    for (gint i = 0; i < n_workspaces; ++i) {
                        gchar *new_name = g_strdup_printf("/backdrop/screen%d/monitor%s/workspace%d/%s",
                                                          screen_num,
                                                          monitor_name,
                                                          i,
                                                          new_setting_name);
                        if (!xfconf_channel_has_property(channel, new_name)) {
                            xfconf_channel_set_array(channel, new_name,
                                                     G_TYPE_DOUBLE, &red,
                                                     G_TYPE_DOUBLE, &green,
                                                     G_TYPE_DOUBLE, &blue,
                                                     G_TYPE_DOUBLE, &alpha,
                                                     G_TYPE_INVALID);
                        } else {
                            g_message("New property %s already exists; not migrating", new_name);
                        }
                        g_free(new_name);
                    }

                    xfconf_channel_reset_property(channel, name, FALSE);
                    ok = TRUE;
                }
            }
        }

        if (!ok) {
            g_message("Unable to migrate %s to RGBA as it contains invalid data", name);
        }
    }

    return ok;
}

static gboolean
maybe_migrate_image_show(XfconfChannel *channel,
                         const gchar *name,
                         const GValue *value,
                         int screen_num,
                         const gchar *monitor_name,
                         const char *setting_name,
                         gint n_workspaces)
{
    if (g_strcmp0(setting_name, "image-show") == 0) {
        if (G_VALUE_HOLDS_BOOLEAN(value) && !g_value_get_boolean(value)) {
            for (gint i = 0; i < n_workspaces; ++i) {
                gchar *new_name = g_strdup_printf("/backdrop/screen%d/monitor%s/workspace%d/image-style",
                                                  screen_num,
                                                  monitor_name,
                                                  i);
                if (!xfconf_channel_has_property(channel, new_name)) {
                    xfconf_channel_set_int(channel, new_name, XFCE_BACKDROP_IMAGE_NONE);
                } else {
                    g_message("New property %s already exists; not migrating", new_name);
                }
                g_free(new_name);
            }
        }

        xfconf_channel_reset_property(channel, name, FALSE);

        return TRUE;
    } else {
        return FALSE;
    }
}

static void
fixup_image_filename(XfconfChannel *channel, const gchar *property_name, const GValue *value) {
    static const gchar *to_svg[] = {
        BACKGROUNDS_DIR "/xfce-stripes.png",
        BACKGROUNDS_DIR "/xfce-teal.png",
        BACKGROUNDS_DIR "/xfce-verticals.png",
    };

    if (g_str_has_suffix(property_name, "/last-image") && G_VALUE_HOLDS_STRING(value)) {
        const gchar *filename = g_value_get_string(value);
        for (gsize i = 0; i < G_N_ELEMENTS(to_svg); ++i) {
            if (g_strcmp0(filename, to_svg[i]) == 0) {
                gchar *new_filename = g_strdup(filename);
                gsize len = strlen(new_filename);
                memcpy(&new_filename[len - 3], "svg", 3);

                xfconf_channel_set_string(channel, property_name, new_filename);
                g_free(new_filename);

                break;
            }
        }
    }
}

// NB: this can only successfully migrate for monitors that are currently plugged in
void
xfdesktop_migrate_backdrop_settings(GdkDisplay *display, XfconfChannel *channel) {
    guint migration_version = xfconf_channel_get_uint(channel, LAST_SETTINGS_MIGRATION_PROP, 0);

    if (migration_version < 1) {
        XfwScreen *xfw_screen = xfw_screen_get_default();
        XfwWorkspaceManager *workspace_manager = xfw_screen_get_workspace_manager(xfw_screen);
        gint n_workspaces = g_list_length(xfw_workspace_manager_list_workspaces(workspace_manager));
        g_object_unref(xfw_screen);

        gint n_monitors = gdk_display_get_n_monitors(display);
        GHashTable *backdrop_properties = xfconf_channel_get_properties(channel, "/backdrop");

        GHashTableIter iter;
        g_hash_table_iter_init(&iter, backdrop_properties);

        gchar *name;
        GValue *value;
        while (g_hash_table_iter_next(&iter, (gpointer)&name, (gpointer)&value)) {
            if (g_strrstr(name, "/workspace") == NULL) {
                int screen_num = -1;
                int monitor_num = -1;
                char *setting_name = NULL;

                if (3 == sscanf(name, "/backdrop/screen%d/monitor%d/%ms", &screen_num, &monitor_num, &setting_name) &&
                    screen_num >= 0 &&
                    monitor_num >= 0 &&
                    setting_name != NULL)
                {
                    if (monitor_num < n_monitors) {
                        GdkMonitor *monitor = gdk_display_get_monitor(display, monitor_num);
                        const gchar *monitor_name = gdk_monitor_get_model(monitor);

                        if (!maybe_migrate_gdkcolor(channel, name, value, screen_num, monitor_name, setting_name, n_workspaces) &&
                            !maybe_migrate_image_show(channel, name, value, screen_num, monitor_name, setting_name, n_workspaces))
                        {
                            if (g_strcmp0(setting_name, "image-path") == 0) {
                                free(setting_name);
                                setting_name = strdup("last-image");
                            }

                            for (gint i = 0; i < n_workspaces; ++i) {
                                gchar *new_name = g_strdup_printf("/backdrop/screen%d/monitor%s/workspace%d/%s",
                                                                  screen_num,
                                                                  monitor_name,
                                                                  i,
                                                                  setting_name);
                                if (!xfconf_channel_has_property(channel, new_name)) {
                                    xfconf_channel_set_property(channel, new_name, value);
                                } else {
                                    g_message("New property %s already exists; not migrating", new_name);
                                }
                                g_free(new_name);
                            }
                        }
                    } else {
                        g_message("Unable to migrate old setting %s; monitor %d is not currently attached", name, monitor_num);
                    }

                    xfconf_channel_reset_property(channel, name, FALSE);
                }

                if (setting_name != NULL) {
                    free(setting_name);
                }
            }
        }

        g_hash_table_destroy(backdrop_properties);

        // Fetch them again because the names of properties may have changed
        backdrop_properties = xfconf_channel_get_properties(channel, "/backdrop");
        g_hash_table_iter_init(&iter, backdrop_properties);
        while (g_hash_table_iter_next(&iter, (gpointer)&name, (gpointer)&value)) {
            fixup_image_filename(channel,name, value);
        }
        g_hash_table_destroy(backdrop_properties);
    }

    xfconf_channel_set_uint(channel, LAST_SETTINGS_MIGRATION_PROP, CUR_SETTINGS_MIGRATION_VERSION);
}
