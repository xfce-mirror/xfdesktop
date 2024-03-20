/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004 Brian Tarricone, <brian@tarricone.org>
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
 *
 *  Random portions taken from or inspired by the original xfdesktop for xfce4:
 *     Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *     Copyright (C) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
 */

#include <libxfce4util/libxfce4util.h>

#include "xfdesktop-backdrop-cycler.h"
#include "xfdesktop-common.h"

struct _XfdesktopBackdropCycler {
    GObject parent;

    XfconfChannel *channel;
    gchar *property_prefix;

    gboolean enabled;
    XfceBackdropImageStyle image_style;
    XfceBackdropCyclePeriod period;
    guint timer;
    gboolean random_order;

    guint timer_id;
    gint prev_random_index;

    gchar *image_path;
    /* Cached list of images in the same folder as image_path */
    GList *image_files;
    /* monitor for the image_files directory */
    GFileMonitor *monitor;
};

enum {
    PROP0,
    PROP_CHANNEL,
    PROP_PROPERTY_PREFIX,
    PROP_IMAGE_STYLE,
    PROP_IMAGE_FILENAME,
    PROP_ENABLED,
    PROP_PERIOD,
    PROP_TIMER,
    PROP_RANDOM_ORDER,
};

static const struct {
    const gchar *setting_suffix;
    GType setting_type;
    const gchar *property;
} setting_bindings[] = {
    { "image-style", G_TYPE_UINT, "image-style" },
    { "cycler-cycle-enable", G_TYPE_BOOLEAN, "enabled" },
    { "cycler-cycle-period", G_TYPE_UINT, "period" },
    { "cycler-cycle-timer", G_TYPE_UINT, "timer" },
    { "cycler-cycle-random-order", G_TYPE_BOOLEAN, "random-order" },
    // image-filename has to be set manually so we can block it when needed
};


static void xfdesktop_backdrop_cycler_constructed(GObject *object);
static void xfdesktop_backdrop_cycler_set_property(GObject *object,
                                                   guint property_id,
                                                   const GValue *value,
                                                   GParamSpec *pspec);
static void xfdesktop_backdrop_cycler_get_property(GObject *object,
                                                   guint property_id,
                                                   GValue *value,
                                                   GParamSpec *pspec);
static void xfdesktop_backdrop_cycler_finalize(GObject *object);

static void cb_xfdesktop_backdrop_cycler_image_files_changed(GFileMonitor *monitor,
                                                 GFile *file,
                                                 GFile *other_file,
                                                 GFileMonitorEvent event,
                                                 gpointer user_data);

static void xfdesktop_backdrop_clear_directory_monitor(XfdesktopBackdropCycler *cycler);

static void xfdesktop_backdrop_cycler_set_image_style(XfdesktopBackdropCycler *cycler,
                                                      XfceBackdropImageStyle style);
static void xfdesktop_backdrop_cycler_set_image_filename(XfdesktopBackdropCycler *cycler,
                                                         const gchar *filename);
static void xfdesktop_backdrop_cycler_set_enabled(XfdesktopBackdropCycler *cycler,
                                                  gboolean cycle_backdrop);
static void xfdesktop_backdrop_cycler_set_period(XfdesktopBackdropCycler *cycler,
                                                 XfceBackdropCyclePeriod period);
static void xfdesktop_backdrop_cycler_set_timer(XfdesktopBackdropCycler *cycler,
                                                guint cycle_timer);
static void xfdesktop_backdrop_cycler_set_random_order(XfdesktopBackdropCycler *cycler,
                                                       gboolean random_order);

static void xfdesktop_backdrop_cycler_image_filename_changed(XfconfChannel *channel,
                                                             const gchar *property_name,
                                                             const GValue *value,
                                                             XfdesktopBackdropCycler *cycler);


G_DEFINE_TYPE(XfdesktopBackdropCycler, xfdesktop_backdrop_cycler, G_TYPE_OBJECT)


static void
xfdesktop_backdrop_cycler_class_init(XfdesktopBackdropCyclerClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->constructed = xfdesktop_backdrop_cycler_constructed;
    gobject_class->set_property = xfdesktop_backdrop_cycler_set_property;
    gobject_class->get_property = xfdesktop_backdrop_cycler_get_property;
    gobject_class->finalize = xfdesktop_backdrop_cycler_finalize;

    g_object_class_install_property(gobject_class,
                                    PROP_CHANNEL,
                                    g_param_spec_object("channel",
                                                        "channel",
                                                        "xfconf channel",
                                                        XFCONF_TYPE_CHANNEL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_PROPERTY_PREFIX,
                                    g_param_spec_string("property-prefix",
                                                        "property-prefix",
                                                        "xfconf property prefix",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_IMAGE_STYLE,
                                    g_param_spec_enum("image-style",
                                                      "image-style",
                                                      "image style",
                                                      XFCE_TYPE_BACKDROP_IMAGE_STYLE,
                                                      XFCE_BACKDROP_IMAGE_ZOOMED,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_IMAGE_FILENAME,
                                    g_param_spec_string("image-filename",
                                                        "image-filename",
                                                        "image filename",
                                                        DEFAULT_BACKDROP,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class,
                                    PROP_ENABLED,
                                    g_param_spec_boolean("enabled",
                                                         "enabled",
                                                         "enabled",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_PERIOD,
                                    g_param_spec_enum("period",
                                                      "period",
                                                      "period",
                                                      XFCE_TYPE_BACKDROP_CYCLE_PERIOD,
                                                      XFCE_BACKDROP_PERIOD_MINUTES,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_TIMER,
                                    g_param_spec_uint("timer",
                                                      "timer",
                                                      "timer",
                                                      0, G_MAXUSHORT, 10,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_RANDOM_ORDER,
                                    g_param_spec_boolean("random-order",
                                                         "random-order",
                                                         "random order",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
}

static void
xfdesktop_backdrop_cycler_init(XfdesktopBackdropCycler *cycler) {
    cycler->prev_random_index = -1;
}

static void
xfdesktop_backdrop_cycler_constructed(GObject *object) {
    G_OBJECT_CLASS(xfdesktop_backdrop_cycler_parent_class)->constructed(object);

    XfdesktopBackdropCycler *cycler = XFDESKTOP_BACKDROP_CYCLER(object);
    for (guint i = 0; i < G_N_ELEMENTS(setting_bindings); ++i) {
        gchar *property_name = g_strconcat(cycler->property_prefix, "/", setting_bindings[i].setting_suffix, NULL);
        xfconf_g_property_bind(cycler->channel,
                               property_name,
                               setting_bindings[i].setting_type,
                               cycler,
                               setting_bindings[i].property);
        g_free(property_name);
    }

    gchar *signal_name = g_strconcat("property-changed::", cycler->property_prefix, "/last-image", NULL);
    g_signal_connect(cycler->channel, signal_name,
                     G_CALLBACK(xfdesktop_backdrop_cycler_image_filename_changed), cycler);
    g_free(signal_name);
}

static void
xfdesktop_backdrop_cycler_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    XfdesktopBackdropCycler *cycler = XFDESKTOP_BACKDROP_CYCLER(object);

    switch (property_id) {
        case PROP_CHANNEL:
            cycler->channel = g_value_dup_object(value);
            break;

        case PROP_PROPERTY_PREFIX:
            cycler->property_prefix = g_value_dup_string(value);
            break;

        case PROP_IMAGE_STYLE:
            xfdesktop_backdrop_cycler_set_image_style(cycler, g_value_get_enum(value));
            break;

        case PROP_IMAGE_FILENAME:
            xfdesktop_backdrop_cycler_set_image_filename(cycler, g_value_get_string(value));
            break;

        case PROP_ENABLED:
            xfdesktop_backdrop_cycler_set_enabled(cycler, g_value_get_boolean(value));
            break;

        case PROP_PERIOD:
            xfdesktop_backdrop_cycler_set_period(cycler, g_value_get_enum(value));
            break;

        case PROP_TIMER:
            xfdesktop_backdrop_cycler_set_timer(cycler, g_value_get_uint(value));
            break;

        case PROP_RANDOM_ORDER:
            xfdesktop_backdrop_cycler_set_random_order(cycler, g_value_get_boolean(value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}


static void
xfdesktop_backdrop_cycler_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    XfdesktopBackdropCycler *cycler = XFDESKTOP_BACKDROP_CYCLER(object);

    switch (property_id) {
        case PROP_CHANNEL:
            g_value_set_object(value, cycler->channel);
            break;

        case PROP_PROPERTY_PREFIX:
            g_value_set_string(value, cycler->property_prefix);
            break;

        case PROP_IMAGE_STYLE:
            g_value_set_enum(value, cycler->image_style);
            break;

        case PROP_IMAGE_FILENAME:
            g_value_set_string(value, cycler->image_path);
            break;

        case PROP_ENABLED:
            g_value_set_boolean(value, cycler->enabled);
            break;

        case PROP_PERIOD:
            g_value_set_enum(value, cycler->period);
            break;

        case PROP_TIMER:
            g_value_set_uint(value, cycler->timer);
            break;

        case PROP_RANDOM_ORDER:
            g_value_set_boolean(value, cycler->random_order);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfdesktop_backdrop_cycler_finalize(GObject *object) {
    XfdesktopBackdropCycler *cycler = XFDESKTOP_BACKDROP_CYCLER(object);

    xfdesktop_backdrop_clear_directory_monitor(cycler);

    g_signal_handlers_disconnect_by_data(cycler->channel, cycler);

    g_list_free_full(cycler->image_files, g_free);
    g_free(cycler->image_path);

    g_free(cycler->property_prefix);
    g_object_unref(cycler->channel);

    G_OBJECT_CLASS(xfdesktop_backdrop_cycler_parent_class)->finalize(object);
}

static void
xfdesktop_backdrop_clear_directory_monitor(XfdesktopBackdropCycler *cycler)
{
    g_return_if_fail(XFDESKTOP_IS_BACKDROP_CYCLER(cycler));

    if (cycler->monitor != NULL) {
        g_signal_handlers_disconnect_by_func(cycler->monitor,
                                             G_CALLBACK(cb_xfdesktop_backdrop_cycler_image_files_changed),
                                             cycler);

        g_file_monitor_cancel(cycler->monitor);
        g_object_unref(cycler->monitor);
        cycler->monitor = NULL;
    }
}

/* we compare by the collate key so the image listing is the same as how
 * xfdesktop-settings displays the images */
static gint
glist_compare_by_collate_key(const gchar *a, const gchar *b) {
    gint ret;
    gchar *a_key = g_utf8_collate_key_for_filename(a, -1);
    gchar *b_key = g_utf8_collate_key_for_filename(b, -1);

    ret = g_strcmp0(a_key, b_key);

    g_free(a_key);
    g_free(b_key);

    return ret;
}

typedef struct {
    gchar *key;
    gchar *string;
} KeyStringPair;

static int qsort_compare_pair_by_key(const void *a, const void *b) {
    const gchar *a_key = ((const KeyStringPair *)a)->key;
    const gchar *b_key = ((const KeyStringPair *)b)->key;
    return g_strcmp0(a_key, b_key);
}

static void
xfdesktop_backdrop_cycler_update_image_filename(XfdesktopBackdropCycler *cycler,
                                                const gchar *image_filename,
                                                gboolean toggle)
{
    g_return_if_fail(XFDESKTOP_IS_BACKDROP_CYCLER(cycler));

    if (g_strcmp0(cycler->image_path, image_filename) != 0) {
        g_free(cycler->image_path);
        cycler->image_path = g_strdup(image_filename);
    }

    // Usually we'd only do the following stuff if the file name hadn't
    // changed, but we use this path to reload things when the name hasn't
    // changed, but the file contents have been rewritten.
    gchar *property_name = g_strconcat(cycler->property_prefix, "/last-image", NULL);
    g_signal_handlers_block_by_func(cycler->channel,
                                    xfdesktop_backdrop_cycler_image_filename_changed,
                                    cycler);
    if (toggle) {
        // This is used if we need the same backdrop reloaded, such as when the file gets rewritten.
        xfconf_channel_set_string(cycler->channel, property_name, "");
    }
    xfconf_channel_set_string(cycler->channel, property_name, cycler->image_path);
    g_signal_handlers_unblock_by_func(cycler->channel,
                                      xfdesktop_backdrop_cycler_image_filename_changed,
                                      cycler);

    g_free(property_name);
}

static void
cb_xfdesktop_backdrop_cycler_image_files_changed(GFileMonitor *monitor,
                                                 GFile *file,
                                                 GFile *other_file,
                                                 GFileMonitorEvent event,
                                                 gpointer user_data)
{
    XfdesktopBackdropCycler *cycler = XFDESKTOP_BACKDROP_CYCLER(user_data);
    gchar *changed_file = NULL;
    GList *item;

    switch (event) {
        case G_FILE_MONITOR_EVENT_CREATED:
            if (!xfdesktop_backdrop_cycler_is_enabled(cycler)) {
                /* If we're not cycling, do nothing, it's faster :) */
                break;
            }

            changed_file = g_file_get_path(file);

            XF_DEBUG("file added: %s", changed_file);

            /* Make sure we don't already have the new file in the list */
            if (g_list_find(cycler->image_files, changed_file)) {
                g_free(changed_file);
                return;
            }

            /* If the new file is not an image then we don't have to do
             * anything */
            if (!xfdesktop_image_file_is_valid(changed_file)) {
                g_free(changed_file);
                return;
            }

            if (!cycler->random_order) {
                /* It is an image file and we don't have it in our list, add
                 * it sorted to our list, don't free changed file, that will
                 * happen when it is removed */
                cycler->image_files = g_list_insert_sorted(cycler->image_files,
                                                                   changed_file,
                                                                   (GCompareFunc)glist_compare_by_collate_key);
            } else {
                /* Same as above except we don't care about the list's order
                 * so just add it */
                cycler->image_files = g_list_prepend(cycler->image_files, changed_file);
            }
            break;

        case G_FILE_MONITOR_EVENT_DELETED:
            if (!xfdesktop_backdrop_cycler_is_enabled(cycler)) {
                /* If we're not cycling, do nothing, it's faster :) */
                break;
            }

            changed_file = g_file_get_path(file);

            XF_DEBUG("file deleted: %s", changed_file);

            /* find the file in the list */
            item = g_list_find_custom(cycler->image_files,
                                      changed_file,
                                      (GCompareFunc)g_strcmp0);

            /* remove it */
            if (item)
                cycler->image_files = g_list_delete_link(cycler->image_files, item);

            g_free(changed_file);

            if (cycler->timer_id != 0) {
                g_source_remove(cycler->timer_id);
                cycler->timer_id = 0;
            }
            break;

        case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
            changed_file = g_file_get_path(file);

            XF_DEBUG("file changed: %s", changed_file);
            XF_DEBUG("image_path: %s", cycler->image_path);

            if (g_strcmp0(changed_file, cycler->image_path) == 0) {
                DBG("match");
                xfdesktop_backdrop_cycler_update_image_filename(cycler, changed_file, TRUE);
            }

            g_free(changed_file);
            break;

        default:
            break;
    }
}

/* Equivalent to (except for not being a stable sort), but faster than
 * g_list_sort(list, (GCompareFunc)glist_compare_by_collate_key) */
static GList *
sort_image_list(GList *list, guint list_size) {
    KeyStringPair *array;
    guint i;
    GList *l;

    TRACE("entering");

    g_assert(g_list_length(list) == list_size);

#define TEST_IMAGE_SORTING 0

#if TEST_IMAGE_SORTING
    GList *list2 = g_list_copy(list);
#endif

    /* Allocate an array of the same size as list */
    array = g_malloc(list_size * sizeof(array[0]));

    /* Copy list contents to the array and generate collation keys */
    for (l = list, i = 0; l; l = l->next, ++i) {
        array[i].string = l->data;
        array[i].key = g_utf8_collate_key_for_filename(array[i].string, -1);
    }

    /* Sort the array */
    qsort(array, list_size, sizeof(array[0]), qsort_compare_pair_by_key);

    /* Copy sorted array back to the list and deallocate the collation keys */
    for (l = list, i = 0; l; l = l->next, ++i) {
        l->data = array[i].string;
        g_free(array[i].key);
    }

    g_free(array);

#if TEST_IMAGE_SORTING
    list2 = g_list_sort(list2, (GCompareFunc)glist_compare_by_collate_key);
    if (g_list_length(list) != g_list_length(list2)) {
        printf("Image sorting test FAILED: list size is not correct.");
    } else {
        GList *l2;
        gboolean data_matches = TRUE, pointers_match = TRUE;
        for (l = list, l2 = list2; l; l = l->next, l2 = l2->next) {
            if (g_strcmp0(l->data, l2->data) != 0)
                data_matches = FALSE;
            if (l->data != l2->data)
                pointers_match = FALSE;
        }
        if (data_matches) {
            printf("Image sorting test SUCCEEDED: ");
            if (pointers_match) {
                printf("both data and pointers are correct.");
            } else {
                printf("data matches but pointers do not match. "
                        "This is caused by unstable sorting.");
            }
        }
        else {
            printf("Image sorting test FAILED: ");
            if (pointers_match) {
                printf("data does not match but pointers do match. "
                        "Something went really wrong!");
            }
            else {
                printf("neither data nor pointers match.");
            }
        }
    }
    putchar('\n');
#endif

    return list;
}

/* Returns a GList of all the image files in the parent directory of filename */
static GList *
list_image_files_in_dir(XfdesktopBackdropCycler *cycler, const gchar *filename) {
    GDir *dir;
    gboolean needs_slash = TRUE;
    const gchar *file;
    GList *files = NULL;
    guint file_count = 0;
    gchar *dir_name;

    dir_name = g_path_get_dirname(filename);

    dir = g_dir_open(dir_name, 0, 0);
    if (!dir) {
        g_free(dir_name);
        return NULL;
    }

    if (dir_name[strlen(dir_name)-1] == '/')
        needs_slash = FALSE;

    while ((file = g_dir_read_name(dir))) {
        gchar *current_file = g_strdup_printf(needs_slash ? "%s/%s" : "%s%s",
                                              dir_name, file);
        if (xfdesktop_image_file_is_valid(current_file)) {
            files = g_list_prepend(files, current_file);
            ++file_count;
        } else
            g_free(current_file);
    }

    g_dir_close(dir);
    g_free(dir_name);

    /* Only sort if there's more than 1 item and we're not randomly picking
     * images from the list */
    if (file_count > 1 && !cycler->random_order) {
        files = sort_image_list(files, file_count);
    }

    return files;
}

static void
xfdesktop_backdrop_cycler_load_image_files(XfdesktopBackdropCycler *cycler) {
    TRACE("entering");

    /* generate the image_files list if it doesn't exist and we're cycling
     * backdrops */
    if (cycler->image_files == NULL && xfdesktop_backdrop_cycler_is_enabled(cycler)) {
        xfdesktop_backdrop_clear_directory_monitor(cycler);
        cycler->image_files = list_image_files_in_dir(cycler, cycler->image_path);
    }

    /* Always monitor the directory even if we aren't cycling so we know if
     * our current wallpaper has changed by an external program/script */
    if (cycler->image_style != XFCE_BACKDROP_IMAGE_NONE && cycler->image_path != NULL && cycler->monitor == NULL) {
        gchar *dir_name = g_path_get_dirname(cycler->image_path);
        GFile *gfile = g_file_new_for_path(dir_name);

        /* monitor the directory for changes */
        cycler->monitor = g_file_monitor(gfile, G_FILE_MONITOR_NONE, NULL, NULL);
        g_signal_connect(cycler->monitor, "changed",
                         G_CALLBACK(cb_xfdesktop_backdrop_cycler_image_files_changed),
                         cycler);

        g_free(dir_name);
        g_object_unref(gfile);
    }
}

/* Gets the next valid image file in the folder. Free when done using it
 * returns NULL on fail. */
static gchar *
xfdesktop_backdrop_cycler_choose_next(XfdesktopBackdropCycler *cycler) {
    GList *current_file;
    const gchar *filename;

    TRACE("entering");

    g_return_val_if_fail(XFDESKTOP_IS_BACKDROP_CYCLER(cycler), NULL);

    filename = cycler->image_path;

    if (!cycler->image_files)
        return NULL;

    /* Get the our current background in the list */
    current_file = g_list_find_custom(cycler->image_files, filename, (GCompareFunc)g_strcmp0);

    /* if somehow we don't have a valid file, grab the first one available */
    if (current_file == NULL) {
        current_file = g_list_first(cycler->image_files);
    } else {
        /* We want the next valid image file in the dir */
        current_file = g_list_next(current_file);

        /* we hit the end of the list, wrap around to the front */
        if (current_file == NULL)
            current_file = g_list_first(cycler->image_files);
    }

    /* return a copy of our new item */
    return g_strdup(current_file->data);
}

/* Gets a random valid image file in the folder. Free when done using it.
 * returns NULL on fail. */
static gchar *
xfdesktop_backdrop_cycler_choose_random(XfdesktopBackdropCycler *cycler) {
    gint n_items = 0, cur_file;

    TRACE("entering");

    g_return_val_if_fail(XFDESKTOP_IS_BACKDROP_CYCLER(cycler), NULL);

    if (!cycler->image_files)
        return NULL;

    n_items = g_list_length(cycler->image_files);

    /* If there's only 1 item, just return it, easy */
    if (1 == n_items) {
        return g_strdup(g_list_first(cycler->image_files)->data);
    }

    do {
        /* g_random_int_range bounds to n_items-1 */
        cur_file = g_random_int_range(0, n_items);
    } while (cur_file == cycler->prev_random_index && G_LIKELY(cycler->prev_random_index != -1));

    cycler->prev_random_index = cur_file;

    /* return a copy of the new random item */
    return g_strdup(g_list_nth(cycler->image_files, cur_file)->data);
}

/* Provides a mapping of image files in the parent folder of file. It selects
 * the image based on the hour of the day scaled for how many images are in
 * the directory, using the first 24 if there are more.
 * Returns a new image path or NULL on failure. Free when done using it. */
static gchar *
xfdesktop_backdrop_cycler_choose_chronological(XfdesktopBackdropCycler *cycler) {
    GDateTime *datetime;
    GList *new_file;
    gint n_items = 0, epoch;

    TRACE("entering");

    g_return_val_if_fail(XFDESKTOP_IS_BACKDROP_CYCLER(cycler), NULL);

    if (!cycler->image_files)
        return NULL;

    n_items = g_list_length(cycler->image_files);

    /* If there's only 1 item, just return it, easy */
    if (1 == n_items) {
        return g_strdup(g_list_first(cycler->image_files)->data);
    }

    datetime = g_date_time_new_now_local();

    /* Figure out which image to display based on what time of day it is
     * and how many images we have to work with */
    epoch = (gdouble)g_date_time_get_hour(datetime) / (24.0f / MIN(n_items, 24.0f));
    XF_DEBUG("epoch %d, hour %d, items %d", epoch, g_date_time_get_hour(datetime), n_items);

    new_file = g_list_nth(cycler->image_files, epoch);

    g_date_time_unref(datetime);

    /* return a copy of our new file */
    return g_strdup(new_file->data);
}

static void
xfdesktop_backdrop_cycler_do_cycle(XfdesktopBackdropCycler *cycler) {
    g_return_if_fail(XFDESKTOP_IS_BACKDROP_CYCLER(cycler));

    TRACE("entering");

    /* sanity checks */
    if (xfdesktop_backdrop_cycler_is_enabled(cycler)) {
        gchar *new_backdrop = NULL;

        if (cycler->period == XFCE_BACKDROP_PERIOD_CHRONOLOGICAL) {
            /* chronological first */
            new_backdrop = xfdesktop_backdrop_cycler_choose_chronological(cycler);
        } else if (cycler->random_order) {
            /* then random */
            new_backdrop = xfdesktop_backdrop_cycler_choose_random(cycler);
        } else {
            /* sequential, the default */
            new_backdrop = xfdesktop_backdrop_cycler_choose_next(cycler);
        }

        /* Only emit the cycle signal if something changed */
        if (g_strcmp0(cycler->image_path, new_backdrop) != 0) {
            xfdesktop_backdrop_cycler_update_image_filename(cycler, new_backdrop, FALSE);
        }

        g_free(new_backdrop);
    }
}

static void
xfdesktop_backdrop_cycler_remove_backdrop_timer(XfdesktopBackdropCycler *cycler) {
    g_return_if_fail(XFDESKTOP_IS_BACKDROP_CYCLER(cycler));

    if (cycler->timer_id != 0) {
        g_source_remove(cycler->timer_id);
        cycler->timer_id = 0;
    }
}

static gboolean
xfdesktop_backdrop_cycler_timer(gpointer user_data) {
    g_return_val_if_fail(XFDESKTOP_IS_BACKDROP_CYCLER(user_data), FALSE);

    TRACE("entering");

    XfdesktopBackdropCycler *cycler = user_data;

    /* Don't bother with trying to cycle a cycler if we're not using images */
    if (cycler->image_style != XFCE_BACKDROP_IMAGE_NONE) {
        xfdesktop_backdrop_cycler_do_cycle(cycler);
    }

    /* Now to complicate things; we have to handle some special cases */
    guint new_cycle_interval = 0;
    switch (cycler->period) {
        case XFCE_BACKDROP_PERIOD_STARTUP:
            /* no more cycling */
            return FALSE;

        case XFCE_BACKDROP_PERIOD_CHRONOLOGICAL:
        case XFCE_BACKDROP_PERIOD_HOURLY: {
            GDateTime *local_time = g_date_time_new_now_local();
            gint minute = g_date_time_get_minute(local_time);
            gint second = g_date_time_get_second(local_time);

            /* find out how long until the next hour so we cycle on the hour */
            new_cycle_interval = ((59 - minute) * 60) + (60 - second);
            g_date_time_unref(local_time);
            break;
        }

        case XFCE_BACKDROP_PERIOD_DAILY: {
            GDateTime *local_time = g_date_time_new_now_local();
            gint hour   = g_date_time_get_hour  (local_time);
            gint minute = g_date_time_get_minute(local_time);
            gint second = g_date_time_get_second(local_time);

            /* find out how long until the next day so we cycle on the day */
            new_cycle_interval = ((23 - hour) * 60 * 60) + ((59 - minute) * 60) + (60 - second);
            g_date_time_unref(local_time);
            break;
        }

        default:
            /* no changes required */
            break;
    }

    /* Update the timer if we're trying to keep things on the hour/day */
    if (new_cycle_interval != 0) {
        xfdesktop_backdrop_cycler_remove_backdrop_timer(cycler);

        XF_DEBUG("calling g_timeout_add_seconds, interval is %d", new_cycle_interval);
        cycler->timer_id = g_timeout_add_seconds(new_cycle_interval,
                                                 xfdesktop_backdrop_cycler_timer,
                                                 cycler);

        /* We created a new instance, kill this one */
        return FALSE;
    } else {
        /* continue cycling (for seconds, minutes, hours, etc) */
        return TRUE;
    }
}

/**
 * xfdesktop_backdrop_cycler_set_image_style:
 * @cycler: An #XfdesktopBackdropCycler.
 * @style: An XfceBackdropImageStyle.
 *
 * Sets the image style to be used for the #XfdesktopBackdropCycler.
 * "STRETCHED" will stretch the image to the full width and height of the
 * #XfdesktopBackdropCycler, while "SCALED" will resize the image to fit the desktop
 * while maintaining the image's aspect ratio.
 **/
static void
xfdesktop_backdrop_cycler_set_image_style(XfdesktopBackdropCycler *cycler, XfceBackdropImageStyle style) {
    g_return_if_fail(XFDESKTOP_IS_BACKDROP_CYCLER(cycler));

    TRACE("entering");

    if (style != cycler->image_style) {
        gboolean was_enabled = xfdesktop_backdrop_cycler_is_enabled(cycler);
        cycler->image_style = style;
        if (!was_enabled && xfdesktop_backdrop_cycler_is_enabled(cycler)) {
            xfdesktop_backdrop_cycler_set_timer(cycler, cycler->timer);
        }
    }
}

/**
 * xfdesktop_backdrop_cycler_set_image_filename:
 * @cycler: An #XfdesktopBackdropCycler.
 * @filename: A filename.
 *
 * Sets the image that should be used with the #XfdesktopBackdropCycler.  The image will
 * be composited on top of the color (or color gradient).  To clear the image,
 * use this call with a @filename argument of %NULL. Makes a copy of @filename.
 **/
static void
xfdesktop_backdrop_cycler_set_image_filename(XfdesktopBackdropCycler *cycler, const gchar *filename) {
    gchar *old_dir = NULL, *new_dir = NULL;
    g_return_if_fail(XFDESKTOP_IS_BACKDROP_CYCLER(cycler));

    TRACE("entering, filename %s", filename);

    /* Don't do anything if the filename doesn't change */
    if (g_strcmp0(cycler->image_path, filename) == 0) {
        return;
    }

    /* We need to free the image_files if image_path changed directories */
    if (cycler->image_files != NULL || cycler->monitor != NULL) {
        if (cycler->image_path != NULL) {
            old_dir = g_path_get_dirname(cycler->image_path);
        }
        if (filename != NULL) {
            new_dir = g_path_get_dirname(filename);
        }

        /* Directories did change */
        if (g_strcmp0(old_dir, new_dir) != 0) {
            /* Free the image list if we had one */
            if (cycler->image_files != NULL) {
                g_list_free_full(cycler->image_files, g_free);
                cycler->image_files = NULL;
            }

            /* release the directory monitor */
            xfdesktop_backdrop_clear_directory_monitor(cycler);
        }

        g_free(old_dir);
        g_free(new_dir);
    }

    /* Now we can free the old path and setup the new one */
    g_free(cycler->image_path);

    if (filename != NULL) {
        cycler->image_path = g_strdup(filename);
    } else {
        cycler->image_path = NULL;
    }
    
    xfdesktop_backdrop_cycler_load_image_files(cycler);

    g_object_notify(G_OBJECT(cycler), "image-filename");
}

static void
xfdesktop_backdrop_cycler_set_enabled(XfdesktopBackdropCycler *cycler, gboolean cycle_backdrop) {
    g_return_if_fail(XFDESKTOP_IS_BACKDROP_CYCLER(cycler));

    TRACE("entering, cycle_backdrop ? %s", cycle_backdrop == TRUE ? "TRUE" : "FALSE");

    if (cycler->enabled != cycle_backdrop) {
        cycler->enabled = cycle_backdrop;
        /* Start or stop the cycler changing */
        xfdesktop_backdrop_cycler_set_timer(cycler, cycler->timer);

        if (cycle_backdrop) {
            /* We're cycling now, so load up an image list */
            xfdesktop_backdrop_cycler_load_image_files(cycler);
        }
        else if (cycler->image_files)
        {
            /* we're not cycling anymore, free the image files list */
            g_list_free_full(cycler->image_files, g_free);
            cycler->image_files = NULL;
        }
    }
}

/**
 * xfdesktop_backdrop_cycler_set_period:
 * @cycler: An #XfdesktopBackdropCycler.
 * @period: Determines how often the cycler will cycle.
 *
 * When cycling backdrops, the period setting will determine how fast the timer
 * will fire (seconds vs minutes) or if the periodic cycler will be based on
 * actual wall clock values or just at xfdesktop's start up.
 **/
static void
xfdesktop_backdrop_cycler_set_period(XfdesktopBackdropCycler *cycler, XfceBackdropCyclePeriod period) {
    g_return_if_fail(XFDESKTOP_IS_BACKDROP_CYCLER(cycler));
    g_return_if_fail(period != XFCE_BACKDROP_PERIOD_INVALID);

    TRACE("entering");

    if (cycler->period != period) {
        cycler->period = period;
        /* Start or stop the cycler changing */
        xfdesktop_backdrop_cycler_set_timer(cycler, cycler->timer);
    }
}

/**
 * xfdesktop_backdrop_cycler_set_timer:
 * @cycler: An #XfdesktopBackdropCycler.
 * @cycle_timer: The amount of time, based on the cycle_period, to wait before
 *               changing the background image.
 *
 * If cycle_backdrop is enabled then this function will change the cycler
 * image based on the cycle_timer and cycle_period. The cycle_timer is a value
 * between 0 and G_MAXUSHORT where a value of 0 indicates that the cycler
 * should not be changed.
 **/
static void
xfdesktop_backdrop_cycler_set_timer(XfdesktopBackdropCycler *cycler, guint cycle_timer) {
    g_return_if_fail(XFDESKTOP_IS_BACKDROP_CYCLER(cycler));

    TRACE("entering, cycle_timer = %d", cycle_timer);

    /* Sanity check to help prevent overflows */
    if (cycle_timer > G_MAXUSHORT) {
        cycle_timer = G_MAXUSHORT;
    }

    cycler->timer = cycle_timer;

    /* remove old timer first */
    xfdesktop_backdrop_cycler_remove_backdrop_timer(cycler);

    if (cycler->timer != 0 && xfdesktop_backdrop_cycler_is_enabled(cycler)) {
        guint cycle_interval = 0;

        switch (cycler->period) {
            case XFCE_BACKDROP_PERIOD_SECONDS:
                cycle_interval = cycler->timer;
                break;

            case XFCE_BACKDROP_PERIOD_MINUTES:
                cycle_interval = cycler->timer * 60;
                break;

            case XFCE_BACKDROP_PERIOD_HOURS:
                cycle_interval = cycler->timer * 60 * 60;
                break;

            case XFCE_BACKDROP_PERIOD_CHRONOLOGICAL:
            case XFCE_BACKDROP_PERIOD_STARTUP:
                /* Startup and chronological will be triggered at once */
                cycle_interval = 1;
                break;

            case XFCE_BACKDROP_PERIOD_HOURLY: {
                GDateTime *local_time = g_date_time_new_now_local();
                gint minute = g_date_time_get_minute(local_time);
                gint second = g_date_time_get_second(local_time);

                /* find out how long until the next hour so we cycle on the hour */
                cycle_interval = ((59 - minute) * 60) + (60 - second);
                g_date_time_unref(local_time);
                break;
            }

            case XFCE_BACKDROP_PERIOD_DAILY: {
                GDateTime *local_time = g_date_time_new_now_local();
                gint hour   = g_date_time_get_hour  (local_time);
                gint minute = g_date_time_get_minute(local_time);
                gint second = g_date_time_get_second(local_time);

                /* find out how long until the next day so we cycle on the day */
                cycle_interval = ((23 - hour) * 60 * 60) + ((59 - minute) * 60) + (60 - second);
                g_date_time_unref(local_time);
                break;
            }

            default:
                g_critical("Unknown backdrop-cycle-period set");
                break;
        }

        if (cycle_interval != 0) {
            XF_DEBUG("calling g_timeout_add_seconds, interval is %d", cycle_interval);
            cycler->timer_id = g_timeout_add_seconds(cycle_interval,
                                                     xfdesktop_backdrop_cycler_timer,
                                                     cycler);
        }
    }
}

/**
 * xfdesktop_backdrop_cycler_set_random_order:
 * @cycler: An #XfdesktopBackdropCycler.
 * @random_order: When TRUE and the backdrops are set to cycle, a random image
 *                image will be choosen when it cycles.
 *
 * When cycling backdrops, they will either be show sequentially (and this value
 * will be FALSE) or they will be selected at random. The images are choosen from
 * the same folder the current cycler image file is in.
 **/
static void
xfdesktop_backdrop_cycler_set_random_order(XfdesktopBackdropCycler *cycler, gboolean random_order) {
    g_return_if_fail(XFDESKTOP_IS_BACKDROP_CYCLER(cycler));

    TRACE("entering");

    if (cycler->random_order != random_order) {
        cycler->random_order = random_order;

        /* If we have an image list and care about order now, sort the list */
        if (!random_order && cycler->image_files) {
            guint num_items = g_list_length(cycler->image_files);
            if (num_items > 1) {
                cycler->image_files = sort_image_list(cycler->image_files, num_items);
            }
        }
    }
}

static void
xfdesktop_backdrop_cycler_image_filename_changed(XfconfChannel *channel,
                                                 const gchar *property_name,
                                                 const GValue *value,
                                                 XfdesktopBackdropCycler *cycler)
{
    // This callback handles external changes, like if the user changes the
    // filename in the settings UI.  If _we_ change the filename because it's
    // time to cycle, we block this signal handler temporarily.
    if (G_VALUE_HOLDS_STRING(value)) {
        xfdesktop_backdrop_cycler_set_image_filename(cycler, g_value_get_string(value));
    }
}

XfdesktopBackdropCycler *
xfdesktop_backdrop_cycler_new(XfconfChannel *channel, const gchar *property_prefix) {
    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel), NULL);
    g_return_val_if_fail(property_prefix != NULL && property_prefix[0] == '/', NULL);

    return g_object_new(XFDESKTOP_TYPE_BACKDROP_CYCLER,
                        "channel", channel,
                        "property-prefix", property_prefix,
                        NULL);
}

const gchar *
xfdesktop_backdrop_cycler_get_property_prefix(XfdesktopBackdropCycler *cycler) {
    g_return_val_if_fail(XFDESKTOP_IS_BACKDROP_CYCLER(cycler), NULL);
    return cycler->property_prefix;
}

gboolean
xfdesktop_backdrop_cycler_is_enabled(XfdesktopBackdropCycler *cycler) {
    g_return_val_if_fail(XFDESKTOP_IS_BACKDROP_CYCLER(cycler), FALSE);
    return cycler->enabled && cycler->image_style != XFCE_BACKDROP_IMAGE_NONE && cycler->image_path != NULL;
}

void
xfdesktop_backdrop_cycler_cycle_backdrop(XfdesktopBackdropCycler *cycler) {
    g_return_if_fail(XFDESKTOP_IS_BACKDROP_CYCLER(cycler));
    if (xfdesktop_backdrop_cycler_is_enabled(cycler)) {
        xfdesktop_backdrop_cycler_do_cycle(cycler);
    }
}
