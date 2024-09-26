/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006      Brian Tarricone, <brian@tarricone.org>
 *  Copyright (c) 2010-2011 Jannis Pohlmann, <jannis@xfce.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>

#include <libxfce4ui/libxfce4ui.h>

#include "xfdesktop-file-utils.h"
#include "xfdesktop-file-icon.h"

#define GET_PRIVATE(icon) ((XfdesktopFileIconPrivate *)xfdesktop_file_icon_get_instance_private(XFDESKTOP_FILE_ICON(icon)))

typedef struct _XfdesktopFileIconPrivate
{
    GIcon *gicon;
    gchar *sort_key;
} XfdesktopFileIconPrivate;

static void xfdesktop_file_icon_finalize(GObject *obj);

static gchar *xfdesktop_file_icon_get_identifier(XfdesktopIcon *icon);
static gboolean xfdesktop_file_icon_activate(XfdesktopIcon *icon,
                                             GtkWindow *window);

static void xfdesktop_file_icon_set_property(GObject *object,
                                             guint property_id,
                                             const GValue *value,
                                             GParamSpec *pspec);
static void xfdesktop_file_icon_get_property(GObject *object,
                                             guint property_id,
                                             GValue *value,
                                             GParamSpec *pspec);

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(XfdesktopFileIcon, xfdesktop_file_icon,
                                    XFDESKTOP_TYPE_ICON)

enum
{
    PROP_0,
    PROP_GICON,
};

static void
xfdesktop_file_icon_class_init(XfdesktopFileIconClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    XfdesktopIconClass *icon_class = (XfdesktopIconClass *)klass;

    gobject_class->finalize = xfdesktop_file_icon_finalize;
    gobject_class->set_property = xfdesktop_file_icon_set_property;
    gobject_class->get_property = xfdesktop_file_icon_get_property;

    icon_class->get_identifier = xfdesktop_file_icon_get_identifier;
    icon_class->activate = xfdesktop_file_icon_activate;

    g_object_class_install_property(gobject_class,
                                    PROP_GICON,
                                    g_param_spec_object("gicon",
                                                        "gicon",
                                                        "gicon",
                                                        G_TYPE_ICON,
                                                        G_PARAM_READWRITE));
}

static void
xfdesktop_file_icon_init(XfdesktopFileIcon *icon) {}

static void
xfdesktop_file_icon_finalize(GObject *obj)
{
    XfdesktopFileIconPrivate *priv = GET_PRIVATE(obj);

    if (priv->gicon != NULL) {
        g_object_unref(priv->gicon);
    }
    g_free(priv->sort_key);

    G_OBJECT_CLASS(xfdesktop_file_icon_parent_class)->finalize(obj);
}

static void
xfdesktop_file_icon_set_property(GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(object);
    XfdesktopFileIconPrivate *priv = GET_PRIVATE(object);

    switch(property_id) {
        case PROP_GICON:
            xfdesktop_file_icon_invalidate_icon(file_icon);
            priv->gicon = g_value_dup_object(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfdesktop_file_icon_get_property(GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
    XfdesktopFileIconPrivate *priv = GET_PRIVATE(object);

    switch(property_id) {
        case PROP_GICON:
            g_value_set_object(value, priv->gicon);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static gchar *
xfdesktop_file_icon_get_identifier(XfdesktopIcon *icon) {
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);

    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), NULL);

    GFile *file = xfdesktop_file_icon_peek_file(file_icon);
    gchar *identifier = g_file_get_path(file);
    if (identifier == NULL) {
        identifier = g_file_get_uri(file);
    }

    return identifier;
}

static gboolean
xfdesktop_file_icon_activate(XfdesktopIcon *icon,
                             GtkWindow *window)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    GFileInfo *info = xfdesktop_file_icon_peek_file_info(file_icon);
    GFile *file = xfdesktop_file_icon_peek_file(file_icon);
    GdkScreen *gscreen;

    TRACE("entering");

    if(!info)
        return FALSE;

    gscreen = gtk_widget_get_screen(GTK_WIDGET(window));

    if(g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
        GList link = {
            .data = file,
            .prev = NULL,
            .next = NULL,
        };
        xfdesktop_file_utils_open_folders(&link, gscreen, window);
    } else if(xfdesktop_file_utils_file_is_executable(info)) {
        xfdesktop_file_utils_execute(NULL, file, NULL, gscreen, window);
    } else {
        xfdesktop_file_utils_launch(file, gscreen, window);
    }

    return TRUE;
}


GFileInfo *
xfdesktop_file_icon_peek_file_info(XfdesktopFileIcon *icon)
{
    XfdesktopFileIconClass *klass;

    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), NULL);

    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);

    if(klass->peek_file_info)
       return klass->peek_file_info(icon);
    else
        return NULL;
}

GFileInfo *
xfdesktop_file_icon_peek_filesystem_info(XfdesktopFileIcon *icon)
{
    XfdesktopFileIconClass *klass;

    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), NULL);

    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);

    if(klass->peek_filesystem_info)
       return klass->peek_filesystem_info(icon);
    else
        return NULL;
}

GFile *
xfdesktop_file_icon_peek_file(XfdesktopFileIcon *icon)
{
    XfdesktopFileIconClass *klass;

    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), NULL);

    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);

    if(klass->peek_file)
       return klass->peek_file(icon);
    else
        return NULL;
}

void
xfdesktop_file_icon_update_file_info(XfdesktopFileIcon *icon,
                                     GFileInfo *info)
{
    XfdesktopFileIconClass *klass;

    g_return_if_fail(XFDESKTOP_IS_FILE_ICON(icon));

    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);

    if(klass->update_file_info)
       klass->update_file_info(icon, info);
}

gboolean
xfdesktop_file_icon_can_rename_file(XfdesktopFileIcon *icon)
{
    XfdesktopFileIconClass *klass;

    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), FALSE);

    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);

    if(klass->can_rename_file)
       return klass->can_rename_file(icon);
    else
        return FALSE;
}

gboolean
xfdesktop_file_icon_can_delete_file(XfdesktopFileIcon *icon)
{
    XfdesktopFileIconClass *klass;

    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), FALSE);

    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);

    if(klass->can_delete_file)
       return klass->can_delete_file(icon);
    else
        return FALSE;
}

gboolean
xfdesktop_file_icon_is_hidden_file(XfdesktopFileIcon *icon) {
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), FALSE);

    XfdesktopFileIconClass *klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    if (klass->is_hidden_file != NULL) {
        return klass->is_hidden_file(icon);
    } else {
        return FALSE;
    }
}

GIcon *
xfdesktop_file_icon_add_emblems(XfdesktopFileIcon *icon,
                                GIcon *gicon)
{
    GIcon *emblemed_icon = NULL;
    gchar **emblem_names;

    TRACE("entering");

    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), NULL);
    g_return_val_if_fail(G_IS_ICON(gicon), NULL);

    if (G_IS_EMBLEMED_ICON(gicon)) {
        emblemed_icon = g_object_ref(gicon);
    } else {
        emblemed_icon = g_emblemed_icon_new(gicon, NULL);
    }

    if(!G_IS_FILE_INFO(xfdesktop_file_icon_peek_file_info(icon)))
        return emblemed_icon;

    /* Get the list of emblems */
    emblem_names = g_file_info_get_attribute_stringv(xfdesktop_file_icon_peek_file_info(icon),
                                                     "metadata::emblems");

    if(emblem_names != NULL) {
        /* for each item in the list create an icon, pack it into an emblem,
         * and attach it to our icon. */
        for (; *emblem_names != NULL; ++emblem_names) {
            GIcon *themed_icon = g_themed_icon_new(*emblem_names);
            GEmblem *emblem = g_emblem_new(themed_icon);

            g_emblemed_icon_add_emblem(G_EMBLEMED_ICON(emblemed_icon), emblem);

            g_object_unref(emblem);
            g_object_unref(themed_icon);
        }
    }

    return emblemed_icon;
}

void
xfdesktop_file_icon_invalidate_icon(XfdesktopFileIcon *icon)
{
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON(icon));

    XfdesktopFileIconPrivate *priv = GET_PRIVATE(icon);
    g_clear_object(&priv->gicon);
}

gboolean
xfdesktop_file_icon_has_gicon(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), FALSE);
    return G_IS_ICON(GET_PRIVATE(icon)->gicon);
}

GIcon *
xfdesktop_file_icon_get_gicon(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), NULL);

    XfdesktopFileIconPrivate *priv = GET_PRIVATE(icon);
    if (priv->gicon == NULL) {
        XfdesktopFileIconClass *klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
        g_return_val_if_fail(klass->get_gicon != NULL, NULL);
        priv->gicon = klass->get_gicon(icon);
    }

    return priv->gicon != NULL ? g_object_ref(priv->gicon) : NULL;
}

gdouble
xfdesktop_file_icon_get_opacity(XfdesktopFileIcon *icon) {
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), 1.0);

    XfdesktopFileIconClass *klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    if (klass->get_icon_opacity != NULL) {
        return klass->get_icon_opacity(icon);
    } else {
        return 1.0;
    }
}

gchar *
xfdesktop_file_icon_sort_key_for_file(GFile *file)
{
    g_return_val_if_fail(G_IS_FILE(file), NULL);
    return g_file_get_uri(file);
}

const gchar *
xfdesktop_file_icon_peek_sort_key(XfdesktopFileIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), NULL);

    XfdesktopFileIconPrivate *priv = GET_PRIVATE(icon);
    if (priv->sort_key != NULL) {
        return priv->sort_key;
    } else {
        XfdesktopFileIconClass *klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
        gchar *sk = NULL;

        if (klass->get_sort_key != NULL) {
             sk = klass->get_sort_key(icon);
        } else {
            GFile *file = xfdesktop_file_icon_peek_file(icon);
            if (G_LIKELY(file != NULL)) {
                sk = xfdesktop_file_icon_sort_key_for_file(file);
            }
        }

        if (G_LIKELY(sk != NULL)) {
            priv->sort_key = sk;
        }
        return sk;
    }
}

guint
xfdesktop_file_icon_hash(gconstpointer data)
{
    XfdesktopFileIconClass *klass;

    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON((gpointer)data), 0);
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON((gpointer)data);

    klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    if (klass->hash != NULL) {
        return klass->hash(XFDESKTOP_FILE_ICON(icon));
    } else {
        GFile *file = xfdesktop_file_icon_peek_file(XFDESKTOP_FILE_ICON(icon));
        if (G_LIKELY(file != NULL)) {
            return g_file_hash(file);
        } else {
            g_warning("Attempt to get hash of file icon of type %s, but couldn't", g_type_name_from_instance((GTypeInstance *)icon));
            return 0;
        }
    }
}

gint
xfdesktop_file_icon_equal(gconstpointer a,
                          gconstpointer b)
{
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON((gpointer)a), 0);
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON((gpointer)b), 0);

    return g_str_equal(xfdesktop_file_icon_peek_sort_key(XFDESKTOP_FILE_ICON((gpointer)a)),
                       xfdesktop_file_icon_peek_sort_key(XFDESKTOP_FILE_ICON((gpointer)b)));
}
