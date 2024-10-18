/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2022 Brian Tarricone, <brian@tarricone.org>
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

#include "libxfce4windowing/libxfce4windowing.h"
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <libxfce4util/libxfce4util.h>

#include "xfdesktop-common.h"
#include "xfdesktop-extensions.h"
#include "xfdesktop-file-icon-model.h"
#include "xfdesktop-file-icon.h"
#include "xfdesktop-icon.h"
#include "xfdesktop-icon-view-model.h"
#include "xfdesktop-marshal.h"
#include "xfdesktop-regular-file-icon.h"
#include "xfdesktop-special-file-icon.h"
#include "xfdesktop-thumbnailer.h"
#include "xfdesktop-volume-icon.h"

#define PENDING_NEW_FILES_TIMEOUT  (60)

struct _XfdesktopFileIconModel
{
    XfdesktopIconViewModel parent;

    GdkScreen *gdkscreen;
    XfconfChannel *channel;
    GFile *folder;

    GFileMonitor *monitor;
    GFileEnumerator *enumerator;
    GCancellable *cancel_enumeration;

    GVolumeMonitor *volume_monitor;
    GHashTable *volume_icons;  // { GVolume | GMount } -> XfdesktopVolumeIcon

    GFileMonitor *metadata_monitor;
    guint metadata_timer;

    XfdesktopThumbnailer *thumbnailer;

    // This hash table is slightly more complicated than I'd like.  While the values
    // are simply XfdesktopFileIcon instances, the keys are obtained via
    // xfdesktop_file_icon_peek_sort_key().  But if we need to look up an icon and we
    // don't have the icon to get the sort key from, we use:
    //   * Regular file icons: xfdesktop_file_icon_sort_key_for_file()
    //   * Special file icons: xfdesktop_file_icon_sort_key_for_file()
    //   * Volume icons: xfdesktop_volume_icon_sort_key_for_volume()
    GHashTable *icons;

    gboolean show_thumbnails;
    gboolean sort_folders_before_files;
};

struct _XfdesktopFileIconModelClass
{
    XfdesktopIconViewModelClass parent_class;
};

typedef enum {
    PROP0,
    PROP_GDK_SCREEN,
    PROP_CHANNEL,
    PROP_FOLDER,
    PROP_SHOW_THUMBNAILS,
    PROP_SORT_FOLDERS_BEFORE_FILES,
} ModelPropertyId;

enum {
    SIG_READY,
    SIG_ERROR,
    SIG_ICON_POSITION_REQUEST,
    N_SIGS,
};

static void xfdesktop_file_icon_model_constructed(GObject *object);
static void xfdesktop_file_icon_model_set_property(GObject *object,
                                                   guint property_id,
                                                   const GValue *value,
                                                   GParamSpec *pspec);
static void xfdesktop_file_icon_model_get_property(GObject *object,
                                                   guint property_id,
                                                   GValue *value,
                                                   GParamSpec *pspec);
static void xfdesktop_file_icon_model_finalize(GObject *object);

static void xfdesktop_file_icon_model_tree_model_init(GtkTreeModelIface *iface);

static void xfdesktop_file_icon_model_get_value(GtkTreeModel *model,
                                                GtkTreeIter *iter,
                                                gint column,
                                                GValue *value);

static void xfdesktop_file_icon_model_item_free(XfdesktopIconViewModel *ivmodel,
                                                gpointer item);

static void volume_added(GVolumeMonitor *monitor,
                         GVolume *volume,
                         XfdesktopFileIconModel *fmodel);
static void volume_removed(GVolumeMonitor *monitor,
                           GVolume *volume,
                           XfdesktopFileIconModel *fmodel);
static void volume_changed(GVolumeMonitor *monitor,
                           GVolume *volume,
                           XfdesktopFileIconModel *fmodel);
static void mount_added(GVolumeMonitor *monitor,
                        GMount *mount,
                        XfdesktopFileIconModel *fmodel);
static void mount_removed(GVolumeMonitor *monitor,
                          GMount *mount,
                          XfdesktopFileIconModel *fmodel);
static void mount_changed(GVolumeMonitor *monitor,
                          GMount *mount,
                          XfdesktopFileIconModel *fmodel);
static void mount_pre_unmount(GVolumeMonitor *monitor,
                              GMount *mount,
                              XfdesktopFileIconModel *fmodel);

static void thumbnail_ready(GtkWidget *widget,
                            gchar *srcfile,
                            gchar *thumbnail_filename,
                            XfdesktopFileIconModel *fmodel);

static void xfdesktop_file_icon_model_set_show_thumbnails(XfdesktopFileIconModel *fmodel,
                                                          gboolean show);


G_DEFINE_TYPE_WITH_CODE(XfdesktopFileIconModel,
                        xfdesktop_file_icon_model,
                        XFDESKTOP_TYPE_ICON_VIEW_MODEL,
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL, xfdesktop_file_icon_model_tree_model_init))

G_DEFINE_QUARK("xfdesktop-file-icon-model-error-quark", xfdesktop_file_icon_model_error)


static guint signals[N_SIGS] = { 0, };

static const struct {
    const gchar *setting;
    GType setting_type;
    const gchar *property;
} setting_bindings[] = {
    { DESKTOP_ICONS_SHOW_THUMBNAILS, G_TYPE_BOOLEAN, "show-thumbnails" },
    { DESKTOP_ICONS_SORT_FOLDERS_BEFORE_FILES_PROP, G_TYPE_BOOLEAN, "sort-folders-before-files" },
};

static void
xfdesktop_file_icon_model_class_init(XfdesktopFileIconModelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->constructed = xfdesktop_file_icon_model_constructed;
    gobject_class->set_property = xfdesktop_file_icon_model_set_property;
    gobject_class->get_property = xfdesktop_file_icon_model_get_property;
    gobject_class->finalize = xfdesktop_file_icon_model_finalize;

    g_object_class_install_property(gobject_class,
                                    PROP_GDK_SCREEN,
                                    g_param_spec_object("gdk-screen",
                                                        "gdk-screen",
                                                        "GdkScreen",
                                                        GDK_TYPE_SCREEN,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class,
                                    PROP_CHANNEL,
                                    g_param_spec_object("channel",
                                                        "channel",
                                                        "xfconf channel",
                                                        XFCONF_TYPE_CHANNEL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class,
                                    PROP_FOLDER,
                                    g_param_spec_object("folder",
                                                        "Desktop Folder",
                                                        "Folder this icon manager manages",
                                                        G_TYPE_FILE,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class,
                                    PROP_SHOW_THUMBNAILS,
                                    g_param_spec_boolean("show-thumbnails",
                                                         "show-thumbnails",
                                                         "show-thumbnails",
                                                         TRUE,
                                                         G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class,
                                    PROP_SORT_FOLDERS_BEFORE_FILES,
                                    g_param_spec_boolean("sort-folders-before-files",
                                                         "sort-folders-before-files",
                                                         "sort-folders-before-files",
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    XfdesktopIconViewModelClass *ivmodel_class = XFDESKTOP_ICON_VIEW_MODEL_CLASS(klass);
    ivmodel_class->model_item_ref = g_object_ref;
    ivmodel_class->model_item_free = xfdesktop_file_icon_model_item_free;
    ivmodel_class->model_item_hash = xfdesktop_file_icon_hash;
    ivmodel_class->model_item_equal = xfdesktop_file_icon_equal;

    signals[SIG_READY] = g_signal_new("ready",
                                      XFDESKTOP_TYPE_FILE_ICON_MODEL,
                                      G_SIGNAL_RUN_LAST,
                                      0,
                                      NULL,
                                      NULL,
                                      g_cclosure_marshal_VOID__VOID,
                                      G_TYPE_NONE,
                                      0);

    signals[SIG_ERROR] = g_signal_new("error",
                                      XFDESKTOP_TYPE_FILE_ICON_MODEL,
                                      G_SIGNAL_RUN_LAST,
                                      0,
                                      NULL,
                                      NULL,
                                      g_cclosure_marshal_VOID__BOXED,
                                      G_TYPE_NONE,
                                      1,
                                      G_TYPE_ERROR);

    signals[SIG_ICON_POSITION_REQUEST] = g_signal_new("icon-position-request",
                                                      XFDESKTOP_TYPE_FILE_ICON_MODEL,
                                                      G_SIGNAL_RUN_LAST,
                                                      0,
                                                      NULL,
                                                      NULL,
                                                      xfdesktop_marshal_OBJECT__OBJECT_POINTER_POINTER,
                                                      XFW_TYPE_MONITOR,
                                                      3,
                                                      XFDESKTOP_TYPE_FILE_ICON,
                                                      G_TYPE_POINTER,
                                                      G_TYPE_POINTER);
}

static void
xfdesktop_file_icon_model_tree_model_init(GtkTreeModelIface *iface)
{
    iface->get_value = xfdesktop_file_icon_model_get_value;
}

static void
xfdesktop_file_icon_model_init(XfdesktopFileIconModel *fmodel) {
    fmodel->show_thumbnails = TRUE;
    fmodel->sort_folders_before_files = TRUE;
    fmodel->icons = g_hash_table_new_full(g_str_hash,
                                          g_str_equal,
                                          g_free,
                                          g_object_unref);
    fmodel->volume_icons = g_hash_table_new_full(g_direct_hash, g_direct_equal, g_object_unref, NULL);

}

static void
xfdesktop_file_icon_model_constructed(GObject *object) {
    G_OBJECT_CLASS(xfdesktop_file_icon_model_parent_class)->constructed(object);

    XfdesktopFileIconModel *fmodel = XFDESKTOP_FILE_ICON_MODEL(object);

    fmodel->thumbnailer = xfdesktop_thumbnailer_new();
    g_signal_connect(G_OBJECT(fmodel->thumbnailer), "thumbnail-ready",
                     G_CALLBACK(thumbnail_ready), fmodel);

    fmodel->volume_monitor = g_volume_monitor_get();
    g_signal_connect(fmodel->volume_monitor, "volume-added",
                     G_CALLBACK(volume_added), fmodel);
    g_signal_connect(fmodel->volume_monitor, "volume-removed",
                     G_CALLBACK(volume_removed), fmodel);
    g_signal_connect(fmodel->volume_monitor, "volume-changed",
                     G_CALLBACK(volume_changed), fmodel);
    g_signal_connect(fmodel->volume_monitor, "mount-added",
                     G_CALLBACK(mount_added), fmodel);
    g_signal_connect(fmodel->volume_monitor, "mount-removed",
                     G_CALLBACK(mount_removed), fmodel);
    g_signal_connect(fmodel->volume_monitor, "mount-changed",
                     G_CALLBACK(mount_changed), fmodel);
    g_signal_connect(fmodel->volume_monitor, "mount-pre-unmount",
                     G_CALLBACK(mount_pre_unmount), fmodel);

    for (gsize i = 0; i < G_N_ELEMENTS(setting_bindings); ++i) {
        xfconf_g_property_bind(fmodel->channel,
                               setting_bindings[i].setting,
                               setting_bindings[i].setting_type,
                               fmodel,
                               setting_bindings[i].property);
    }
}

static void
xfdesktop_file_icon_model_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    XfdesktopFileIconModel *fmodel = XFDESKTOP_FILE_ICON_MODEL(object);

    switch (property_id) {
        case PROP_GDK_SCREEN:
            fmodel->gdkscreen = g_value_get_object(value);
            break;

        case PROP_CHANNEL:
            fmodel->channel = g_value_dup_object(value);
            break;

        case PROP_FOLDER:
            fmodel->folder = g_value_dup_object(value);
            break;

        case PROP_SHOW_THUMBNAILS:
            xfdesktop_file_icon_model_set_show_thumbnails(fmodel, g_value_get_boolean(value));
            break;

        case PROP_SORT_FOLDERS_BEFORE_FILES:
            fmodel->sort_folders_before_files = g_value_get_boolean(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfdesktop_file_icon_model_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    XfdesktopFileIconModel *fmodel = XFDESKTOP_FILE_ICON_MODEL(object);

    switch (property_id) {
        case PROP_GDK_SCREEN:
            g_value_set_object(value, fmodel->gdkscreen);
            break;

        case PROP_CHANNEL:
            g_value_set_object(value, fmodel->channel);
            break;

        case PROP_FOLDER:
            g_value_set_object(value, fmodel->folder);
            break;

        case PROP_SHOW_THUMBNAILS:
            g_value_set_boolean(value, fmodel->show_thumbnails);
            break;

        case PROP_SORT_FOLDERS_BEFORE_FILES:
            g_value_set_boolean(value, fmodel->sort_folders_before_files);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfdesktop_file_icon_model_finalize(GObject *object) {
    XfdesktopFileIconModel *fmodel = XFDESKTOP_FILE_ICON_MODEL(object);

    /* remove any pending metadata changes */
    if (fmodel->metadata_timer != 0) {
        g_source_remove(fmodel->metadata_timer);
    }

    if (fmodel->cancel_enumeration != NULL) {
        g_cancellable_cancel(fmodel->cancel_enumeration);
        g_object_unref(fmodel->cancel_enumeration);
    }
    if (fmodel->enumerator != NULL) {
        g_object_unref(fmodel->enumerator);
    }

    /* disconnect from the file monitor and release it */
    if (fmodel->monitor) {
        g_signal_handlers_disconnect_by_data(fmodel->monitor, fmodel);
        g_object_unref(fmodel->monitor);
    }

    /* Same for the file metadata monitor */
    if (fmodel->metadata_monitor) {
        g_signal_handlers_disconnect_by_data(fmodel->metadata_monitor, fmodel);
        g_object_unref(fmodel->metadata_monitor);
    }

    g_object_unref(fmodel->volume_monitor);

    g_object_unref(fmodel->thumbnailer);

    g_hash_table_destroy(fmodel->volume_icons);
    g_hash_table_destroy(fmodel->icons);
    g_object_unref(fmodel->folder);
    g_object_unref(fmodel->channel);

    G_OBJECT_CLASS(xfdesktop_file_icon_model_parent_class)->finalize(object);
}

static void
xfdesktop_file_icon_model_get_value(GtkTreeModel *model,
                                    GtkTreeIter *iter,
                                    gint column,
                                    GValue *value)
{
    gpointer model_item;
    XfdesktopFileIcon *icon;

    g_return_if_fail(iter != NULL);

    model_item = xfdesktop_icon_view_model_get_model_item(XFDESKTOP_ICON_VIEW_MODEL(model), iter);
    g_return_if_fail(model_item != NULL && XFDESKTOP_IS_FILE_ICON(model_item));
    icon = XFDESKTOP_FILE_ICON(model_item);

    switch (column) {
        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_IMAGE: {
            GIcon *gicon = xfdesktop_file_icon_get_gicon(icon);
            if (icon != NULL) {
                g_value_init(value, G_TYPE_ICON);
                g_value_set_object(value, gicon);
            }
            break;
        }

        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_IMAGE_OPACITY:
            g_value_init(value, G_TYPE_DOUBLE);
            g_value_set_double(value, xfdesktop_file_icon_get_opacity(icon));
            break;

        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_LABEL: {
            const gchar *label = xfdesktop_icon_peek_label(XFDESKTOP_ICON(icon));
            if (label != NULL) {
                g_value_init(value, G_TYPE_STRING);
                g_value_set_string(value, label);
            }
            break;
        }

        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_ROW: {
            gint16 row = -1, col = -1;

            g_value_init(value, G_TYPE_INT);
            xfdesktop_icon_get_position(XFDESKTOP_ICON(icon), &row, &col);
            g_value_set_int(value, row);
            break;
        }

        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_COL: {
            gint16 row = -1, col = -1;

            g_value_init(value, G_TYPE_INT);
            xfdesktop_icon_get_position(XFDESKTOP_ICON(icon), &row, &col);
            g_value_set_int(value,col);
            break;
        }

        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_SORT_PRIORITY: {
            gint priority;

            if (XFDESKTOP_IS_SPECIAL_FILE_ICON(icon)) {
                priority = 0;
            } else if (XFDESKTOP_IS_VOLUME_ICON(icon)) {
                priority = 1;
            } else if (XFDESKTOP_IS_REGULAR_FILE_ICON(icon)) {
                if (XFDESKTOP_FILE_ICON_MODEL(model)->sort_folders_before_files) {
                    GFileInfo *file_info = xfdesktop_file_icon_peek_file_info(icon);
                    if (g_file_info_get_file_type(file_info) == G_FILE_TYPE_DIRECTORY) {
                        priority = 2;
                    } else {
                        priority = 3;
                    }
                } else {
                    priority = 2;
                }
            } else {
                priority = 4;
            }

            g_value_init(value, G_TYPE_INT);
            g_value_set_int(value, priority);
            break;
        }

        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_TOOLTIP_IMAGE: {
            GIcon *gicon = xfdesktop_file_icon_get_gicon(icon);
            if (icon != NULL) {
                g_value_init(value, G_TYPE_ICON);
                g_value_set_object(value, gicon);
            }
            break;
        }

        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_TOOLTIP_TEXT: {
            const gchar *tip_text = xfdesktop_icon_peek_tooltip(XFDESKTOP_ICON(icon));
            if (tip_text != NULL) {
                g_value_init(value, G_TYPE_STRING);
                g_value_set_string(value, tip_text);
            }
            break;
        }

        default:
            g_warning("Invalid XfdesktopWindowIconManager column %d", column);
            break;
    }
}

static void
xfdesktop_file_icon_model_item_free(XfdesktopIconViewModel *ivmodel,
                                    gpointer item)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(item);

    g_signal_handlers_disconnect_by_data(icon, ivmodel);
    g_object_unref(icon);
}

static gboolean
check_create_desktop_folder(XfdesktopFileIconModel *fmodel) {
    gboolean result = TRUE;

    GFileInfo *info = g_file_query_info(fmodel->folder,
                                        XFDESKTOP_FILE_INFO_NAMESPACE,
                                        G_FILE_QUERY_INFO_NONE,
                                        NULL,
                                        NULL);

    if (info == NULL) {
        GError *error = NULL;
        if (!g_file_make_directory_with_parents(fmodel->folder, NULL, &error)) {
            GError *error2 = g_error_new_literal(XFDESKTOP_FILE_ICON_MODEL_ERROR,
                                                 XFDESKTOP_FILE_ICON_MODEL_ERROR_CANT_CREATE_DESKTOP_FOLDER,
                                                 error->message);
            g_error_free(error2);
            g_signal_emit(fmodel, signals[SIG_ERROR], 0, error2);
            result = FALSE;
            g_error_free(error);
        }
    } else if (g_file_info_get_file_type(info) != G_FILE_TYPE_DIRECTORY) {
        GError *error = g_error_new_literal(XFDESKTOP_FILE_ICON_MODEL_ERROR,
                                            XFDESKTOP_FILE_ICON_MODEL_ERROR_DESKTOP_NOT_FOLDER,
                                            _("A normal file with the same name already exists. Please delete or rename it."));
        g_signal_emit(fmodel, signals[SIG_ERROR], 0, error);
        result = FALSE;
        g_error_free(error);
    }

    if (info != NULL) {
        g_object_unref(info);
    }

    return result;
}

static void
add_icon(XfdesktopFileIconModel *fmodel, XfdesktopFileIcon *icon) {
    XfwMonitor *monitor = NULL;
    gint16 row = -1, col = -1;
    g_signal_emit(fmodel, signals[SIG_ICON_POSITION_REQUEST], 0, icon, &row, &col, &monitor);
    xfdesktop_icon_set_monitor(XFDESKTOP_ICON(icon), monitor);
    if (row != -1 && col != -1) {
        xfdesktop_icon_set_position(XFDESKTOP_ICON(icon), row, col);
    }

    g_signal_connect_swapped(icon, "label-changed",
                             G_CALLBACK(xfdesktop_icon_view_model_changed), fmodel);
    g_signal_connect_swapped(icon, "pixbuf-changed",
                             G_CALLBACK(xfdesktop_icon_view_model_changed), fmodel);

    XF_DEBUG("adding icon %s to icon view", xfdesktop_icon_peek_label(XFDESKTOP_ICON(icon)));
    g_hash_table_replace(fmodel->icons, g_strdup(xfdesktop_file_icon_peek_sort_key(icon)), icon);
    xfdesktop_icon_view_model_append(XFDESKTOP_ICON_VIEW_MODEL(fmodel), icon, icon, NULL);
}

static void
remove_icon(XfdesktopFileIconModel *fmodel, XfdesktopFileIcon *icon) {
    GFile *file = xfdesktop_file_icon_peek_file(icon);
    if (G_LIKELY(file != NULL)) {
        gchar *filename = g_file_get_path(file);

        if (G_LIKELY(filename != NULL)) {
            xfdesktop_thumbnailer_dequeue_thumbnail(fmodel->thumbnailer, filename);
            g_free(filename);
        }
    }

    if (XFDESKTOP_IS_VOLUME_ICON(icon)) {
        XfdesktopVolumeIcon *volume_icon = XFDESKTOP_VOLUME_ICON(icon);
        g_hash_table_remove(fmodel->volume_icons, xfdesktop_volume_icon_peek_volume(volume_icon));
        g_hash_table_remove(fmodel->volume_icons, xfdesktop_volume_icon_peek_mount(volume_icon));
    }

    XF_DEBUG("removing icon %s from icon view", xfdesktop_icon_peek_label(XFDESKTOP_ICON(icon)));
    xfdesktop_icon_view_model_remove(XFDESKTOP_ICON_VIEW_MODEL(fmodel), icon);
    g_hash_table_remove(fmodel->icons, xfdesktop_file_icon_peek_sort_key(icon));
}

static void
add_special_file_icon(XfdesktopFileIconModel *fmodel, XfdesktopSpecialFileIconType type) {
    XfdesktopSpecialFileIcon *icon = xfdesktop_special_file_icon_new(type, fmodel->gdkscreen);
    if (icon != NULL) {
        add_icon(fmodel, XFDESKTOP_FILE_ICON(icon));
    }
}

static void
add_volume_icon(XfdesktopFileIconModel *fmodel, GVolume *volume) {
#if defined(DEBUG_TRACE) && DEBUG_TRACE > 0
    {
        gchar *name = g_volume_get_name(volume);
        TRACE("entering: '%s'", name);
        g_free(name);
    }
#endif

    XfdesktopVolumeIcon *icon = xfdesktop_volume_icon_new_for_volume(volume, fmodel->gdkscreen);
    g_hash_table_insert(fmodel->volume_icons, g_object_ref(volume), icon);
    add_icon(fmodel, XFDESKTOP_FILE_ICON(icon));
}

static void
add_mount_icon(XfdesktopFileIconModel *fmodel, GMount *mount) {
#if defined(DEBUG_TRACE) && DEBUG_TRACE > 0
    {
        gchar *name = g_mount_get_name(mount);
        TRACE("entering: '%s'", name);
        g_free(name);
    }
#endif

    XfdesktopVolumeIcon *icon = xfdesktop_volume_icon_new_for_mount(mount, fmodel->gdkscreen);
    g_hash_table_insert(fmodel->volume_icons, g_object_ref(mount), icon);
    add_icon(fmodel, XFDESKTOP_FILE_ICON(icon));
}

static void
volume_added(GVolumeMonitor *monitor, GVolume *volume, XfdesktopFileIconModel *fmodel) {
    TRACE("entering");
    if (!g_hash_table_contains(fmodel->volume_icons, volume)) {
        add_volume_icon(fmodel, volume);
    }
}

static void
volume_removed(GVolumeMonitor *monitor, GVolume *volume, XfdesktopFileIconModel *fmodel) {
    XfdesktopFileIcon *icon = g_hash_table_lookup(fmodel->volume_icons, volume);
    if (icon != NULL) {
        g_hash_table_remove(fmodel->volume_icons, volume);
        remove_icon(fmodel, icon);
    }
}

static void
volume_changed(GVolumeMonitor *monitor, GVolume *volume, XfdesktopFileIconModel *fmodel) {
    XfdesktopFileIcon *icon = g_hash_table_lookup(fmodel->volume_icons, volume);
    if (icon != NULL) {
        xfdesktop_icon_view_model_changed(XFDESKTOP_ICON_VIEW_MODEL(fmodel), icon);
    }
}

static void
mount_added(GVolumeMonitor *monitor, GMount *mount, XfdesktopFileIconModel *fmodel) {
    TRACE("entering");
    XfdesktopFileIcon *icon = g_hash_table_lookup(fmodel->volume_icons, mount);
    if (icon == NULL && !g_mount_is_shadowed(mount)) {
        GVolume *volume = g_mount_get_volume(mount);
        if (volume != NULL) {
            DBG("got existing volume for mount");
            icon = g_hash_table_lookup(fmodel->volume_icons, volume);
            if (icon != NULL) {
                DBG("got existing icon for volume for mount");
                xfdesktop_volume_icon_mounted(XFDESKTOP_VOLUME_ICON(icon), mount);
                xfdesktop_icon_view_model_changed(XFDESKTOP_ICON_VIEW_MODEL(fmodel), icon);
                g_hash_table_insert(fmodel->volume_icons, g_object_ref(mount), icon);
            }
            g_object_unref(volume);
        } else {
            add_mount_icon(fmodel, mount);
        }
    }
}

static void
mount_removed(GVolumeMonitor *monitor, GMount *mount, XfdesktopFileIconModel *fmodel) {
    XfdesktopFileIcon *icon = g_hash_table_lookup(fmodel->volume_icons, mount);
    if (icon != NULL) {
        g_hash_table_remove(fmodel->volume_icons, mount);

        XfdesktopVolumeIcon *vicon = XFDESKTOP_VOLUME_ICON(icon);
        xfdesktop_volume_icon_unmounted(vicon);

        if (xfdesktop_volume_icon_peek_volume(vicon) == NULL && xfdesktop_volume_icon_peek_mount(vicon) == NULL) {
            remove_icon(fmodel, icon);
        }
    }
}

static void
mount_changed(GVolumeMonitor *monitor, GMount *mount, XfdesktopFileIconModel *fmodel) {
    XfdesktopFileIcon *icon = g_hash_table_lookup(fmodel->volume_icons, mount);
    if (icon != NULL) {
        xfdesktop_icon_view_model_changed(XFDESKTOP_ICON_VIEW_MODEL(fmodel), icon);
    }
}

static void
mount_pre_unmount(GVolumeMonitor *monitor, GMount *mount, XfdesktopFileIconModel *fmodel) {
    XfdesktopFileIcon *icon = g_hash_table_lookup(fmodel->volume_icons, mount);
    if (icon == NULL) {
        GVolume *volume = g_mount_get_volume(mount);
        if (volume != NULL) {
            icon = g_hash_table_lookup(fmodel->volume_icons, volume);
            g_object_unref(volume);
        }
    }

    if (icon != NULL) {
        // XXX: do we need to bother with this?
        xfdesktop_icon_view_model_changed(XFDESKTOP_ICON_VIEW_MODEL(fmodel), icon);
    }
}

static void
load_removable_media(XfdesktopFileIconModel *fmodel) {
    DBG("entering");

    GList *volumes = g_volume_monitor_get_volumes(fmodel->volume_monitor);
    for (GList *l = volumes; l != NULL; l = l->next) {
        GVolume *volume = G_VOLUME(l->data);
        volume_added(fmodel->volume_monitor, volume, fmodel);
        g_object_unref(volume);
    }
    g_list_free(volumes);

    GList *mounts = g_volume_monitor_get_mounts(fmodel->volume_monitor);
    for (GList *l = mounts; l != NULL; l = l->next) {
        GMount *mount = G_MOUNT(l->data);
        mount_added(fmodel->volume_monitor, mount, fmodel);
        g_object_unref(mount);
    }
    g_list_free(mounts);
}

static void
queue_thumbnail(XfdesktopFileIconModel *fmodel, XfdesktopRegularFileIcon *icon) {
    if (fmodel->show_thumbnails) {
        GFile *file = xfdesktop_file_icon_peek_file(XFDESKTOP_FILE_ICON(icon));
        if (file != NULL) {
            gchar *path = g_file_get_path(file);
            if (path != NULL) {
                xfdesktop_thumbnailer_queue_thumbnail(fmodel->thumbnailer, path);
                g_free(path);
            }
        }
    }
}

static void
add_regular_icon(XfdesktopFileIconModel *fmodel,
                 GFile *file,
                 GFileInfo *info,
                 gint16 row,
                 gint16 col,
                 gboolean defer_if_missing)
{
    XfdesktopRegularFileIcon *icon = xfdesktop_regular_file_icon_new(fmodel->channel, fmodel->gdkscreen, file, info);
    if (row >= 0 && col >= 0) {
        xfdesktop_icon_set_position(XFDESKTOP_ICON(icon), row, col);
    }
    add_icon(fmodel, XFDESKTOP_FILE_ICON(icon));

    queue_thumbnail(fmodel, icon);
}

static void
file_monitor_changed(GFileMonitor *monitor,
                     GFile *file,
                     GFile *other_file,
                     GFileMonitorEvent event,
                     XfdesktopFileIconModel *fmodel)
{
    switch (event) {
        case G_FILE_MONITOR_EVENT_RENAMED:
        case G_FILE_MONITOR_EVENT_MOVED_OUT: {
            gint16 row = -1, col = -1;

            XF_DEBUG("got a moved event");

            gchar *ht_key = xfdesktop_file_icon_sort_key_for_file(file);
            XfdesktopFileIcon *icon = g_hash_table_lookup(fmodel->icons, ht_key);
            g_free(ht_key);

            if (icon != NULL) {
                /* Get the old position so we can use it for the new icon */
                if(!xfdesktop_icon_get_position(XFDESKTOP_ICON(icon), &row, &col)) {
                    /* Failed to get position... not supported? */
                    row = col = 0;
                }
                XF_DEBUG("row %d, col %d", row, col);

                /* Remove the old icon */
                remove_icon(fmodel, icon);
            }

            /* In case of MOVED_OUT, other_file will be NULL */
            if (other_file != NULL) {
                /* Check to see if there's already an other_file represented on
                 * the desktop and remove it so there aren't duplicated icons
                 * present. */
                ht_key = xfdesktop_file_icon_sort_key_for_file(other_file);
                XfdesktopFileIcon *moved_icon = g_hash_table_lookup(fmodel->icons, ht_key);
                g_free(ht_key);
                if (moved_icon != NULL) {
                    /* Since we're replacing an existing icon, get that location
                     * to use instead */
                    if (!xfdesktop_icon_get_position(XFDESKTOP_ICON(moved_icon), &row, &col)) {
                        /* Failed to get position... not supported? */
                        row = col = 0;
                    }
                    XF_DEBUG("row %d, col %d", row, col);

                    remove_icon(fmodel, moved_icon);
                }

                if (xfdesktop_compare_paths(g_file_get_parent(other_file), fmodel->folder)) {
                    XF_DEBUG("icon moved off the desktop");
                    /* Nothing moved, this is actually a delete */
                } else {
                    GFileInfo *file_info = g_file_query_info(other_file,
                                                             XFDESKTOP_FILE_INFO_NAMESPACE,
                                                             G_FILE_QUERY_INFO_NONE,
                                                             NULL,
                                                             NULL);

                    if (file_info != NULL) {
                        add_regular_icon(fmodel, other_file, file_info, row, col, FALSE);
                        g_object_unref(file_info);
                    }
                }
            }

            break;
        }

        case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
        case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT: {
            XF_DEBUG("got changed event");

            gchar *ht_key = xfdesktop_file_icon_sort_key_for_file(file);
            XfdesktopFileIcon *icon = g_hash_table_lookup(fmodel->icons, ht_key);
            g_free(ht_key);
            if (icon != NULL) {
                GFileInfo *file_info = g_file_query_info(file,
                                                         XFDESKTOP_FILE_INFO_NAMESPACE,
                                                         G_FILE_QUERY_INFO_NONE,
                                                         NULL,
                                                         NULL);

                if (file_info != NULL) {
                    /* update the icon if the file still exists */
                    xfdesktop_file_icon_update_file_info(icon, file_info);
                    queue_thumbnail(fmodel, XFDESKTOP_REGULAR_FILE_ICON(icon));

                    g_object_unref(file_info);
                } else {
                    /* Remove the icon as it doesn't seem to exist */
                    remove_icon(fmodel, icon);
                }
            }
            break;
        }

        case G_FILE_MONITOR_EVENT_MOVED_IN:
        case G_FILE_MONITOR_EVENT_CREATED:
            XF_DEBUG("got created event");

            if (!g_file_equal(fmodel->folder, file)) {
                /* first make sure we don't already have an icon for this path.
                 * this seems to be necessary to avoid inconsistencies */
                gchar *ht_key = xfdesktop_file_icon_sort_key_for_file(file);
                XfdesktopFileIcon *icon = g_hash_table_lookup(fmodel->icons, ht_key);
                g_free(ht_key);
                if (icon != NULL) {
                    remove_icon(fmodel, icon);
                }

                GFileInfo *file_info = g_file_query_info(file,
                                                         XFDESKTOP_FILE_INFO_NAMESPACE,
                                                         G_FILE_QUERY_INFO_NONE,
                                                         NULL,
                                                         NULL);
                if (file_info != NULL) {
                    add_regular_icon(fmodel, file, file_info, -1, -1, TRUE);
                    g_object_unref(file_info);
                }
            }
            break;

        case G_FILE_MONITOR_EVENT_DELETED: {
            XF_DEBUG("got deleted event");

            gchar *ht_key = xfdesktop_file_icon_sort_key_for_file(file);
            XfdesktopFileIcon *icon = g_hash_table_lookup(fmodel->icons, ht_key);
            g_free(ht_key);
            if (icon != NULL) {
                gchar *filename = g_file_get_path(file);
                xfdesktop_thumbnailer_dequeue_thumbnail(fmodel->thumbnailer, filename);
                /* Always try to remove thumbnail so it doesn't take up
                 * space on the user's disk. */
                xfdesktop_thumbnailer_delete_thumbnail(fmodel->thumbnailer, filename);
                remove_icon(fmodel, icon);
                g_free(filename);
            } else if (g_file_equal(file, fmodel->folder)) {
                XF_DEBUG("~/Desktop disappeared!");
                /* yes, reload before and after is correct */
                xfdesktop_file_icon_model_reload(fmodel);
                check_create_desktop_folder(fmodel);
                xfdesktop_file_icon_model_reload(fmodel);
            }

            break;
        }

        default:
            break;
    }
}

static void
update_file_info(gpointer key, gpointer value, gpointer user_data) {
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(value);

    if (icon != NULL && (XFDESKTOP_IS_REGULAR_FILE_ICON(icon) || XFDESKTOP_IS_SPECIAL_FILE_ICON(icon))) {
        GFile *file = xfdesktop_file_icon_peek_file(icon);
        GFileInfo *file_info = g_file_query_info(file, XFDESKTOP_FILE_INFO_NAMESPACE,
                                      G_FILE_QUERY_INFO_NONE, NULL, NULL);

        if (file_info != NULL) {
            /* update the icon if the file still exists */
            xfdesktop_file_icon_update_file_info(icon, file_info);
            g_object_unref(file_info);
        }
    }
}

static gboolean
metadata_update_timer(gpointer user_data)
{
    XfdesktopFileIconModel *fmodel = XFDESKTOP_FILE_ICON_MODEL(user_data);
    fmodel->metadata_timer = 0;
    g_hash_table_foreach(fmodel->icons, update_file_info, fmodel);

    return FALSE;
}

static void
metadata_monitor_changed(GFileMonitor *monitor,
                         GFile *file,
                         GFile *other_file,
                         GFileMonitorEvent event,
                         XfdesktopFileIconModel *fmodel)
{
    if (event == G_FILE_MONITOR_EVENT_CHANGED) {
        XF_DEBUG("metadata file changed event");

        /* remove any pending metadata changes */
        if (fmodel->metadata_timer != 0) {
            g_source_remove(fmodel->metadata_timer);
            fmodel->metadata_timer = 0;
        }

        /* cool down timer so we don't call this due to multiple file
         * changes at the same time. */
        fmodel->metadata_timer = g_timeout_add_seconds(5, metadata_update_timer, fmodel);
    }
}

static void
enumerator_files_ready(GFileEnumerator *enumerator, GAsyncResult *result, XfdesktopFileIconModel *fmodel) {
    DBG("entering");

    /* Make sure not to reference fmodel if we have been cancelled */
    GError *error = NULL;
    GList *files = g_file_enumerator_next_files_finish(enumerator, result, &error);
    if (files == NULL && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        DBG("cancelled");
        g_error_free(error);
    } else if (files == NULL) {
        g_clear_object(&fmodel->enumerator);

        if (error != NULL) {
            GError *error2 = g_error_new_literal(XFDESKTOP_FILE_ICON_MODEL_ERROR,
                                                 XFDESKTOP_FILE_ICON_MODEL_ERROR_FOLDER_LIST_FAILED,
                                                 error->message);
            g_error_free(error);
            g_signal_emit(fmodel, signals[SIG_ERROR], 0, error2);
            g_error_free(error2);
        } else {
            if (fmodel->monitor == NULL) {
                fmodel->monitor = g_file_monitor(fmodel->folder, G_FILE_MONITOR_WATCH_MOVES, NULL, NULL);
                g_signal_connect(fmodel->monitor, "changed",
                                 G_CALLBACK(file_monitor_changed), fmodel);
            }

            /* initialize the metadata watching the gvfs files since it doesn't
             * send notification messages when monitored files change */
            if (fmodel->metadata_monitor == NULL) {
                gchar *location = xfce_resource_lookup(XFCE_RESOURCE_DATA, "gvfs-metadata/");
                if (location != NULL) {
                    GFile *metadata_location = g_file_new_for_path(location);

                    fmodel->metadata_monitor = g_file_monitor(metadata_location, G_FILE_MONITOR_NONE, NULL, NULL);
                    g_signal_connect(fmodel->metadata_monitor, "changed",
                                     G_CALLBACK(metadata_monitor_changed), fmodel);

                    g_object_unref(metadata_location);
                    g_free(location);
                }
            }

            g_signal_emit(fmodel, signals[SIG_READY], 0);
        }
    } else {
        for (GList *l = files; l; l = l->next) {
            GFileInfo *info = G_FILE_INFO(l->data);
            const gchar *name = g_file_info_get_name(info);
            GFile *file = g_file_get_child(fmodel->folder, name);

            add_regular_icon(fmodel, file, info, -1, -1, TRUE);

            g_object_unref(file);
            g_object_unref(info);
        }
        g_list_free(files);

        g_file_enumerator_next_files_async(fmodel->enumerator,
                                           10,
                                           G_PRIORITY_DEFAULT,
                                           fmodel->cancel_enumeration,
                                           (GAsyncReadyCallback)enumerator_files_ready,
                                           fmodel);
    }
}

static void
file_enumerator_ready(GFile *file, GAsyncResult *result, XfdesktopFileIconModel *fmodel) {
    GError *error = NULL;
    /* Make sure we don't access fmodel until after we have checked for cancellation */
    GFileEnumerator *enumerator = g_file_enumerate_children_finish(file, result, &error);
    if (enumerator == NULL) {
        if (error != NULL) {
            if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED) == FALSE) {
                GError *error2 = g_error_new_literal(XFDESKTOP_FILE_ICON_MODEL_ERROR,
                                                     XFDESKTOP_FILE_ICON_MODEL_ERROR_FOLDER_LIST_FAILED,
                                                     error->message);
                g_signal_emit(fmodel, signals[SIG_ERROR], 0, error2);
                g_error_free(error2);
            } else {
                DBG("cancelled");
            }
            g_error_free(error);
        }
    } else {
        g_clear_object(&fmodel->enumerator);
        fmodel->enumerator = enumerator;
        g_file_enumerator_next_files_async(fmodel->enumerator,
                                           10, G_PRIORITY_DEFAULT, fmodel->cancel_enumeration,
                                           (GAsyncReadyCallback)enumerator_files_ready,
                                           fmodel);
    }
}

static void
thumbnail_ready(GtkWidget *widget, gchar *srcfile, gchar *thumbnail_filename, XfdesktopFileIconModel *fmodel) {
    g_return_if_fail(srcfile != NULL && thumbnail_filename != NULL);

    GFile *file = g_file_new_for_path(srcfile);

    gchar *ht_key = xfdesktop_file_icon_sort_key_for_file(file);
    XfdesktopIcon *icon = g_hash_table_lookup(fmodel->icons, ht_key);
    g_free(ht_key);

    if (icon != NULL) {
        GFile *thumb_file = g_file_new_for_path(thumbnail_filename);
        xfdesktop_icon_set_thumbnail_file(icon, thumb_file);
    }

    g_object_unref(file);
}

static void
xfdesktop_file_icon_model_set_show_thumbnails(XfdesktopFileIconModel *fmodel, gboolean show) {
    if (fmodel->show_thumbnails != show) {
        fmodel->show_thumbnails = show;

        GHashTableIter iter;
        g_hash_table_iter_init(&iter, fmodel->icons);

        XfdesktopFileIcon *icon;
        while (g_hash_table_iter_next(&iter, NULL, (gpointer)&icon)) {
            if (XFDESKTOP_IS_REGULAR_FILE_ICON(icon)) {
                if (show) {
                    queue_thumbnail(fmodel, XFDESKTOP_REGULAR_FILE_ICON(icon));
                } else {
                    xfdesktop_icon_delete_thumbnail(XFDESKTOP_ICON(icon));
                }
            }

        }
    }
}

XfdesktopFileIconModel *
xfdesktop_file_icon_model_new(XfconfChannel *channel, GFile *folder, GdkScreen *gdkscreen) {
    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel), NULL);
    g_return_val_if_fail(G_IS_FILE(folder), NULL);
    return g_object_new(XFDESKTOP_TYPE_FILE_ICON_MODEL,
                        "channel", channel,
                        "folder", folder,
                        "gdk-screen", gdkscreen,
                        NULL);
}

XfdesktopFileIcon *
xfdesktop_file_icon_model_get_icon(XfdesktopFileIconModel *fmodel,
                                   GtkTreeIter *iter)
{
    gpointer model_item;

    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON_MODEL(fmodel), NULL);

    model_item = xfdesktop_icon_view_model_get_model_item(XFDESKTOP_ICON_VIEW_MODEL(fmodel), iter);
    if (model_item != NULL) {
        return XFDESKTOP_FILE_ICON(model_item);
    } else {
        return NULL;
    }
}

XfdesktopFileIcon *
xfdesktop_file_icon_model_get_icon_for_file(XfdesktopFileIconModel *fmodel, GFile *file) {
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON_MODEL(fmodel), NULL);
    g_return_val_if_fail(G_IS_FILE(file), NULL);

    gchar *ht_sort_key = xfdesktop_file_icon_sort_key_for_file(file);
    XfdesktopFileIcon *icon = g_hash_table_lookup(fmodel->icons, ht_sort_key);
    g_free(ht_sort_key);

    return icon;
}

gboolean
xfdesktop_file_icon_model_get_icon_iter(XfdesktopFileIconModel *fmodel,
                                        XfdesktopFileIcon *icon,
                                        GtkTreeIter *iter)
{
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON_MODEL(fmodel), FALSE);
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), FALSE);
    g_return_val_if_fail(iter != NULL, FALSE);

    return xfdesktop_icon_view_model_get_iter_for_key(XFDESKTOP_ICON_VIEW_MODEL(fmodel), icon, iter);
}

void
xfdesktop_file_icon_model_reload(XfdesktopFileIconModel *fmodel) {
    TRACE("entering");

    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MODEL(fmodel));

    if (fmodel->cancel_enumeration != NULL) {
        g_cancellable_cancel(fmodel->cancel_enumeration);
        g_object_unref(fmodel->cancel_enumeration);
    }
    fmodel->cancel_enumeration = g_cancellable_new();

    GList *icons = g_hash_table_get_values(fmodel->icons);
    for (GList *l = icons; l != NULL; l = l->next) {
        XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(l->data);
        remove_icon(fmodel, icon);
    }
    g_list_free(icons);
    g_assert(g_hash_table_size(fmodel->icons) == 0);
    g_assert(g_hash_table_size(fmodel->volume_icons) == 0);

    xfdesktop_icon_view_model_clear(XFDESKTOP_ICON_VIEW_MODEL(fmodel));

    if (check_create_desktop_folder(fmodel)) {
        g_clear_object(&fmodel->enumerator);

        for (gint i = 0; i <= XFDESKTOP_SPECIAL_FILE_ICON_TRASH; ++i) {
            DBG("adding special file type %d", i);
            add_special_file_icon(fmodel, i);
        }

        load_removable_media(fmodel);

        g_file_enumerate_children_async(fmodel->folder,
                                        XFDESKTOP_FILE_INFO_NAMESPACE,
                                        G_FILE_QUERY_INFO_NONE,
                                        G_PRIORITY_DEFAULT,
                                        fmodel->cancel_enumeration,
                                        (GAsyncReadyCallback)file_enumerator_ready,
                                        fmodel);
    }
}
