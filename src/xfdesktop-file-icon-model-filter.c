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

#include <glib-object.h>

#include "common/xfdesktop-common.h"
#include "xfdesktop-file-icon-model-filter.h"
#include "xfdesktop-file-icon-model.h"
#include "xfdesktop-icon-position-configs.h"
#include "xfdesktop-icon.h"
#include "xfdesktop-regular-file-icon.h"
#include "xfdesktop-special-file-icon.h"
#include "xfdesktop-volume-icon.h"

struct _XfdesktopFileIconModelFilter {
    GtkTreeModelFilter parent;

    XfconfChannel *channel;
    XfdesktopIconPositionConfigs *position_configs;
    XfwMonitor *monitor;

    gboolean show_special_home;
    gboolean show_special_filesystem;
    gboolean show_special_trash;
    gboolean show_removable_media;
    gboolean show_network_volumes;
    gboolean show_device_volumes;
    gboolean show_fixed_device_volumes;
    gboolean show_unknown_volumes;
    gboolean show_hidden_files;
};

enum {
    PROP0,
    PROP_CHANNEL,
    PROP_POSITION_CONFIGS,
    PROP_MONITOR,
    PROP_SHOW_HOME,
    PROP_SHOW_FILESYSTEM,
    PROP_SHOW_TRASH,
    PROP_SHOW_REMOVABLE,
    PROP_SHOW_NETWORK_VOLUME,
    PROP_SHOW_DEVICE_VOLUME,
    PROP_SHOW_FIXED_DEVICE_VOLUME,
    PROP_SHOW_UNKNOWN_VOLUME,
    PROP_SHOW_HIDDEN_FILES,
};

static void xfdesktop_file_icon_model_filter_constructed(GObject *object);
static void xfdesktop_file_icon_model_filter_set_property(GObject *object,
                                                          guint property_id,
                                                          const GValue *value,
                                                          GParamSpec *pspec);
static void xfdesktop_file_icon_model_filter_get_property(GObject *object,
                                                          guint property_id,
                                                          GValue *value,
                                                          GParamSpec *pspec);
static void xfdesktop_file_icon_model_filter_finalize(GObject *object);

static gboolean xfdesktop_file_icon_model_filter_visible(GtkTreeModelFilter *filter,
                                                         GtkTreeModel *child_model,
                                                         GtkTreeIter *child_iter);

static gboolean is_special_file_visible(XfdesktopFileIconModelFilter *filter,
                                        XfdesktopSpecialFileIcon *icon);
static gboolean is_volume_visible(XfdesktopFileIconModelFilter *filter,
                                  XfdesktopVolumeIcon *icon);
static gboolean is_regular_file_visible(XfdesktopFileIconModelFilter *filter,
                                        XfdesktopRegularFileIcon *icon);


G_DEFINE_TYPE(XfdesktopFileIconModelFilter, xfdesktop_file_icon_model_filter, GTK_TYPE_TREE_MODEL_FILTER)


static const struct
{
    const gchar *setting;
    GType setting_type;
    const gchar *property;
} setting_bindings[] = {
    { DESKTOP_ICONS_SHOW_FILESYSTEM, G_TYPE_BOOLEAN, "show-filesystem" },
    { DESKTOP_ICONS_SHOW_HOME, G_TYPE_BOOLEAN, "show-home" },
    { DESKTOP_ICONS_SHOW_TRASH, G_TYPE_BOOLEAN, "show-trash" },
    { DESKTOP_ICONS_SHOW_REMOVABLE, G_TYPE_BOOLEAN, "show-removable" },
    { DESKTOP_ICONS_SHOW_NETWORK_REMOVABLE, G_TYPE_BOOLEAN, "show-network-volume" },
    { DESKTOP_ICONS_SHOW_DEVICE_REMOVABLE, G_TYPE_BOOLEAN, "show-device-volume" },
    { DESKTOP_ICONS_SHOW_DEVICE_FIXED, G_TYPE_BOOLEAN, "show-fixed-device-volume" },
    { DESKTOP_ICONS_SHOW_UNKNWON_REMOVABLE, G_TYPE_BOOLEAN, "show-unknown-volume" },
    { DESKTOP_ICONS_SHOW_HIDDEN_FILES, G_TYPE_BOOLEAN, "show-hidden-files" },
};

static void
xfdesktop_file_icon_model_filter_class_init(XfdesktopFileIconModelFilterClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->constructed = xfdesktop_file_icon_model_filter_constructed;
    gobject_class->set_property = xfdesktop_file_icon_model_filter_set_property;
    gobject_class->get_property = xfdesktop_file_icon_model_filter_get_property;
    gobject_class->finalize = xfdesktop_file_icon_model_filter_finalize;

    g_object_class_install_property(gobject_class,
                                    PROP_CHANNEL,
                                    g_param_spec_object("channel",
                                                        "channel",
                                                        "xfconf channel",
                                                        XFCONF_TYPE_CHANNEL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class,
                                    PROP_POSITION_CONFIGS,
                                    g_param_spec_pointer("position-configs",
                                                         "position-configs",
                                                         "XfdesktopIconPositionConfigs",
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class,
                                    PROP_MONITOR,
                                    g_param_spec_object("monitor",
                                                        "monitor",
                                                        "xfw monitor",
                                                        XFW_TYPE_MONITOR,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class,
                                    PROP_SHOW_HOME,
                                    g_param_spec_boolean("show-home",
                                                         "show home",
                                                         "show home",
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class,
                                    PROP_SHOW_FILESYSTEM,
                                    g_param_spec_boolean("show-filesystem",
                                                         "show filesystem",
                                                         "show filesystem",
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class,
                                    PROP_SHOW_TRASH,
                                    g_param_spec_boolean("show-trash",
                                                         "show trash",
                                                         "show trash",
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class,
                                    PROP_SHOW_REMOVABLE,
                                    g_param_spec_boolean("show-removable",
                                                         "show removable",
                                                         "show removable",
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class,
                                    PROP_SHOW_NETWORK_VOLUME,
                                    g_param_spec_boolean("show-network-volume",
                                                         "show network volume",
                                                         "show network volume",
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class,
                                    PROP_SHOW_DEVICE_VOLUME,
                                    g_param_spec_boolean("show-device-volume",
                                                         "show device volume",
                                                         "show device volume",
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class,
                                    PROP_SHOW_FIXED_DEVICE_VOLUME,
                                    g_param_spec_boolean("show-fixed-device-volume",
                                                         "show fixed device volume",
                                                         "show fixed device volume",
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class,
                                    PROP_SHOW_UNKNOWN_VOLUME,
                                    g_param_spec_boolean("show-unknown-volume",
                                                         "show unknown volume",
                                                         "show unknown volume",
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class,
                                    PROP_SHOW_HIDDEN_FILES,
                                    g_param_spec_boolean("show-hidden-files",
                                                         "show-hidden-files",
                                                         "show-hidden-files",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    GtkTreeModelFilterClass *tmf_class = GTK_TREE_MODEL_FILTER_CLASS(klass);
    tmf_class->visible = xfdesktop_file_icon_model_filter_visible;
}

static void
xfdesktop_file_icon_model_filter_init(XfdesktopFileIconModelFilter *filter) {
    filter->show_special_home = TRUE;
    filter->show_special_filesystem = TRUE;
    filter->show_special_trash = TRUE;
    filter->show_removable_media = TRUE;
    filter->show_network_volumes = TRUE;
    filter->show_device_volumes = TRUE;
    filter->show_fixed_device_volumes = TRUE;
    filter->show_unknown_volumes = TRUE;
    filter->show_hidden_files = FALSE;
}

static void
xfdesktop_file_icon_model_filter_constructed(GObject *object) {
    G_OBJECT_CLASS(xfdesktop_file_icon_model_filter_parent_class)->constructed(object);

    XfdesktopFileIconModelFilter *filter = XFDESKTOP_FILE_ICON_MODEL_FILTER(object);
    for (gsize i = 0; i < G_N_ELEMENTS(setting_bindings); ++i) {
        xfconf_g_property_bind(filter->channel,
                               setting_bindings[i].setting,
                               setting_bindings[i].setting_type,
                               filter,
                               setting_bindings[i].property);
    }
}

static void
xfdesktop_file_icon_model_filter_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    XfdesktopFileIconModelFilter *filter = XFDESKTOP_FILE_ICON_MODEL_FILTER(object);

    switch (property_id) {
        case PROP_CHANNEL:
            filter->channel = g_value_dup_object(value);
            break;

        case PROP_POSITION_CONFIGS:
            filter->position_configs = g_value_get_pointer(value);
            break;

        case PROP_MONITOR:
            filter->monitor = g_value_dup_object(value);
            break;

        case PROP_SHOW_HOME:
            filter->show_special_home = g_value_get_boolean(value);
            gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(filter));
            break;

        case PROP_SHOW_FILESYSTEM:
            filter->show_special_filesystem = g_value_get_boolean(value);
            gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(filter));
            break;

        case PROP_SHOW_TRASH:
            filter->show_special_trash = g_value_get_boolean(value);
            gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(filter));
            break;

        case PROP_SHOW_REMOVABLE:
            filter->show_removable_media = g_value_get_boolean(value);
            gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(filter));
            break;

        case PROP_SHOW_NETWORK_VOLUME:
            filter->show_network_volumes = g_value_get_boolean(value);
            gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(filter));
            break;

        case PROP_SHOW_DEVICE_VOLUME:
            filter->show_device_volumes = g_value_get_boolean(value);
            gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(filter));
            break;

        case PROP_SHOW_FIXED_DEVICE_VOLUME:
            filter->show_fixed_device_volumes = g_value_get_boolean(value);
            gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(filter));
            break;

        case PROP_SHOW_UNKNOWN_VOLUME:
            filter->show_device_volumes = g_value_get_boolean(value);
            gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(filter));
            break;

        case PROP_SHOW_HIDDEN_FILES:
            filter->show_hidden_files = g_value_get_boolean(value);
            gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(filter));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfdesktop_file_icon_model_filter_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    XfdesktopFileIconModelFilter *filter = XFDESKTOP_FILE_ICON_MODEL_FILTER(object);

    switch (property_id) {
        case PROP_CHANNEL:
            g_value_set_object(value, filter->channel);
            break;

        case PROP_POSITION_CONFIGS:
            g_value_set_object(value, filter->position_configs);
            break;

        case PROP_MONITOR:
            g_value_set_object(value, filter->monitor);
            break;

        case PROP_SHOW_HOME:
            g_value_set_boolean(value, filter->show_special_home);
            break;

        case PROP_SHOW_FILESYSTEM:
            g_value_set_boolean(value, filter->show_special_filesystem);
            break;

        case PROP_SHOW_TRASH:
            g_value_set_boolean(value, filter->show_special_trash);
            break;

        case PROP_SHOW_REMOVABLE:
            g_value_set_boolean(value, filter->show_removable_media);
            break;

        case PROP_SHOW_NETWORK_VOLUME:
            g_value_set_boolean(value, filter->show_network_volumes);
            break;

        case PROP_SHOW_DEVICE_VOLUME:
            g_value_set_boolean(value, filter->show_device_volumes);
            break;

        case PROP_SHOW_FIXED_DEVICE_VOLUME:
            g_value_set_boolean(value, filter->show_fixed_device_volumes);
            break;

        case PROP_SHOW_UNKNOWN_VOLUME:
            g_value_set_boolean(value, filter->show_unknown_volumes);
            break;

        case PROP_SHOW_HIDDEN_FILES:
            g_value_set_boolean(value, filter->show_hidden_files);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfdesktop_file_icon_model_filter_finalize(GObject *object) {
    XfdesktopFileIconModelFilter *filter = XFDESKTOP_FILE_ICON_MODEL_FILTER(object);
    g_object_unref(filter->channel);
    G_OBJECT_CLASS(xfdesktop_file_icon_model_filter_parent_class)->finalize(object);
}

static gboolean
xfdesktop_file_icon_model_filter_visible(GtkTreeModelFilter *tmfilter, GtkTreeModel *child_model, GtkTreeIter *child_iter) {
    XfdesktopFileIconModelFilter *filter = XFDESKTOP_FILE_ICON_MODEL_FILTER(tmfilter);
    XfdesktopFileIcon *icon = xfdesktop_file_icon_model_get_icon(XFDESKTOP_FILE_ICON_MODEL(child_model), child_iter);
    if (icon != NULL) {
        const gchar *icon_id = xfdesktop_icon_peek_identifier(XFDESKTOP_ICON(icon));
        XfwMonitor *monitor = NULL;
        gboolean found = xfdesktop_icon_position_configs_lookup(filter->position_configs, icon_id, &monitor, NULL, NULL);

        return ((found && monitor == filter->monitor) || (!found && xfw_monitor_is_primary(filter->monitor)))
            && (
                (XFDESKTOP_IS_REGULAR_FILE_ICON(icon) && is_regular_file_visible(filter, XFDESKTOP_REGULAR_FILE_ICON(icon)))
                || (XFDESKTOP_IS_SPECIAL_FILE_ICON(icon) && is_special_file_visible(filter, XFDESKTOP_SPECIAL_FILE_ICON(icon)))
                || (XFDESKTOP_IS_VOLUME_ICON(icon) && is_volume_visible(filter, XFDESKTOP_VOLUME_ICON(icon)))
            )
            && GTK_TREE_MODEL_FILTER_CLASS(xfdesktop_file_icon_model_filter_parent_class)->visible(tmfilter, child_model, child_iter);
    } else {
        return FALSE;
    }
}

static gboolean
is_special_file_visible(XfdesktopFileIconModelFilter *filter, XfdesktopSpecialFileIcon *icon) {
    switch (xfdesktop_special_file_icon_get_icon_type(icon)) {
        case XFDESKTOP_SPECIAL_FILE_ICON_HOME:
            return filter->show_special_home;
        case XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM:
            return filter->show_special_filesystem;
        case XFDESKTOP_SPECIAL_FILE_ICON_TRASH:
            return filter->show_special_trash;
        default:
            g_assert_not_reached();
            return FALSE;
    }
}

static gboolean
is_volume_visible(XfdesktopFileIconModelFilter *filter, XfdesktopVolumeIcon *icon) {
    if (filter->show_removable_media) {
        gboolean visible;

        GVolume *volume = xfdesktop_volume_icon_peek_volume(icon);
        if (volume != NULL) {
            gboolean is_removable = g_volume_can_eject(volume);
            if (!is_removable) {
                GDrive *drive = g_volume_get_drive(volume);
                if (drive != NULL) {
                    is_removable = g_drive_is_removable(drive);
                    g_object_unref(drive);
                }
            }

            gchar *volume_type = g_volume_get_identifier(volume, G_VOLUME_IDENTIFIER_KIND_CLASS);
            gboolean is_device = g_strcmp0(volume_type, "device") == 0;
            visible = (filter->show_network_volumes && g_strcmp0(volume_type, "network") == 0)
                || (filter->show_device_volumes && is_removable && is_device)
                || (filter->show_fixed_device_volumes && !is_removable && is_device)
                || (filter->show_unknown_volumes && is_removable && volume_type == NULL);
            g_free(volume_type);
        } else {
            GMount *mount = xfdesktop_volume_icon_peek_mount(icon);
            if (mount != NULL) {
                gboolean is_removable;
                gboolean is_local;
                gboolean is_ignored_scheme;

                GFile *root = g_mount_get_root(mount);
                if (root != NULL) {
                    is_ignored_scheme =
                        g_file_has_uri_scheme(root, "gphoto2")
                        || g_file_has_uri_scheme(root, "mtp")
                        || g_file_has_uri_scheme(root, "cdda");
                    g_object_unref(root);
                } else {
                    is_ignored_scheme = FALSE;
                }

                GDrive *drive = g_mount_get_drive(mount);
                if (drive != NULL) {
                    is_removable = g_drive_is_removable(drive) || g_mount_can_eject(mount);

                    gchar *unix_device = g_drive_get_identifier(drive, G_DRIVE_IDENTIFIER_KIND_UNIX_DEVICE);
                    is_local = unix_device != NULL && unix_device[0] != '\0';
                    g_free(unix_device);

                    g_object_unref(drive);
                } else {
                    is_removable = g_mount_can_eject(mount);
                    is_local = FALSE;  // Very weak guess, maybe should look at URI
                }

                visible = !is_ignored_scheme
                    && (is_removable
                        || (is_local && (filter->show_device_volumes || filter->show_fixed_device_volumes))
                        || (!is_local && filter->show_network_volumes));
            } else {
                visible = FALSE;
            }
        }

        return visible;
    } else {
        return FALSE;
    }
}

static gboolean
is_regular_file_visible(XfdesktopFileIconModelFilter *filter, XfdesktopRegularFileIcon *icon) {
    return filter->show_hidden_files || !xfdesktop_file_icon_is_hidden_file(XFDESKTOP_FILE_ICON(icon));
}

XfdesktopFileIconModelFilter *
xfdesktop_file_icon_model_filter_new(XfconfChannel *channel,
                                     XfdesktopIconPositionConfigs *position_configs,
                                     XfwMonitor *monitor,
                                     XfdesktopFileIconModel *child)
{
    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel), NULL);
    g_return_val_if_fail(position_configs != NULL, NULL);
    g_return_val_if_fail(XFW_IS_MONITOR(monitor), NULL);
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON_MODEL(child), NULL);
    return g_object_new(XFDESKTOP_TYPE_FILE_ICON_MODEL_FILTER,
                        "channel", channel,
                        "position-configs", position_configs,
                        "monitor", monitor,
                        "child-model", child,
                        NULL);
}

XfdesktopFileIcon *
xfdesktop_file_icon_model_filter_get_icon(XfdesktopFileIconModelFilter *filter, GtkTreeIter *iter) {
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON_MODEL_FILTER(filter), NULL);
    g_return_val_if_fail(iter != NULL, NULL);

    GtkTreeIter child_iter;
    gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(filter), &child_iter, iter);
    GtkTreeModel *child = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(filter));
    return xfdesktop_file_icon_model_get_icon(XFDESKTOP_FILE_ICON_MODEL(child), &child_iter);
}
