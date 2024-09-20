/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006 Brian Tarricone, <brian@tarricone.org>
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

#include <glib-object.h>

#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include "xfdesktop-common.h"
#include "xfdesktop-icon-view-manager.h"

#define XFDESKTOP_ICON_VIEW_MANAGER_GET_PRIVATE(manager) ((XfdesktopIconViewManagerPrivate *)xfdesktop_icon_view_manager_get_instance_private(XFDESKTOP_ICON_VIEW_MANAGER(manager)))

typedef struct _XfdesktopIconViewManagerPrivate
{
    XfwScreen *screen;
    GList *desktops;
    XfconfChannel *channel;

    gboolean icons_on_primary;
} XfdesktopIconViewManagerPrivate;

enum {
    PROP0 = 0,
    PROP_SCREEN,
    PROP_DESKTOPS,
    PROP_CHANNEL,
    PROP_ICON_ON_PRIMARY,
};

static void xfdesktop_icon_view_manager_constructed(GObject *obj);
static void xfdesktop_icon_view_manager_set_property(GObject *obj,
                                                     guint prop_id,
                                                     const GValue *value,
                                                     GParamSpec *pspec);
static void xfdesktop_icon_view_manager_get_property(GObject *obj,
                                                     guint prop_id,
                                                     GValue *value,
                                                     GParamSpec *pspec);
static void xfdesktop_icon_view_manager_finalize(GObject *obj);

static void xfdesktop_icon_view_manager_set_show_icons_on_primary(XfdesktopIconViewManager *manager,
                                                                  gboolean icons_on_primary);

static const struct {
    const gchar *setting;
    GType setting_type;
    const gchar *property;
} setting_bindings[] = {
    { DESKTOP_ICONS_ON_PRIMARY_PROP, G_TYPE_BOOLEAN, "icons-on-primary" },
};


G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(XfdesktopIconViewManager, xfdesktop_icon_view_manager, G_TYPE_OBJECT)


static void
xfdesktop_icon_view_manager_class_init(XfdesktopIconViewManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->constructed = xfdesktop_icon_view_manager_constructed;
    gobject_class->set_property = xfdesktop_icon_view_manager_set_property;
    gobject_class->get_property = xfdesktop_icon_view_manager_get_property;
    gobject_class->finalize = xfdesktop_icon_view_manager_finalize;

#define PARAM_FLAGS  (G_PARAM_READWRITE \
                      | G_PARAM_STATIC_NAME \
                      | G_PARAM_STATIC_NICK \
                      | G_PARAM_STATIC_BLURB)

    g_object_class_install_property(gobject_class,
                                    PROP_SCREEN,
                                    g_param_spec_object("screen",
                                                        "screen",
                                                        "XfwScreen",
                                                        XFW_TYPE_SCREEN,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_DESKTOPS,
                                    g_param_spec_pointer("desktops",
                                                         "desktops",
                                                         "All known XfceDesktop instances",
                                                         PARAM_FLAGS | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(gobject_class, PROP_CHANNEL,
                                    g_param_spec_object("channel",
                                                        "channel",
                                                        "xfconf channel",
                                                        XFCONF_TYPE_CHANNEL,
                                                        PARAM_FLAGS | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(gobject_class, PROP_ICON_ON_PRIMARY,
                                    g_param_spec_boolean("icons-on-primary",
                                                         "icons on primary",
                                                         "show icons on primary desktop",
                                                         DEFAULT_ICONS_ON_PRIMARY,
                                                         PARAM_FLAGS));

#undef PARAM_FLAGS
}

static void
xfdesktop_icon_view_manager_init(XfdesktopIconViewManager *manager)
{
    XfdesktopIconViewManagerPrivate *priv = XFDESKTOP_ICON_VIEW_MANAGER_GET_PRIVATE(manager);
    priv->icons_on_primary = DEFAULT_ICONS_ON_PRIMARY;
}

static void
xfdesktop_icon_view_manager_constructed(GObject *obj)
{
    XfdesktopIconViewManager *manager = XFDESKTOP_ICON_VIEW_MANAGER(obj);
    XfdesktopIconViewManagerPrivate *priv = XFDESKTOP_ICON_VIEW_MANAGER_GET_PRIVATE(manager);

    G_OBJECT_CLASS(xfdesktop_icon_view_manager_parent_class)->constructed(obj);

    for (gsize i = 0; i < G_N_ELEMENTS(setting_bindings); ++i) {
        xfconf_g_property_bind(priv->channel,
                               setting_bindings[i].setting,
                               setting_bindings[i].setting_type,
                               manager,
                               setting_bindings[i].property);
    }
}

static void
xfdesktop_icon_view_manager_set_property(GObject *obj,
                                         guint prop_id,
                                         const GValue *value,
                                         GParamSpec *pspec)
{
    XfdesktopIconViewManager *manager = XFDESKTOP_ICON_VIEW_MANAGER(obj);
    XfdesktopIconViewManagerPrivate *priv = XFDESKTOP_ICON_VIEW_MANAGER_GET_PRIVATE(manager);

    switch (prop_id) {
        case PROP_SCREEN:
            priv->screen = g_value_dup_object(value);
            break;

        case PROP_DESKTOPS:
            priv->desktops = g_list_copy(g_value_get_pointer(value));
            break;

        case PROP_CHANNEL:
            priv->channel = g_value_dup_object(value);
            break;

        case PROP_ICON_ON_PRIMARY:
            xfdesktop_icon_view_manager_set_show_icons_on_primary(manager,
                                                                  g_value_get_boolean(value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void
xfdesktop_icon_view_manager_get_property(GObject *obj,
                                         guint prop_id,
                                         GValue *value,
                                         GParamSpec *pspec)
{
    XfdesktopIconViewManagerPrivate *priv = XFDESKTOP_ICON_VIEW_MANAGER_GET_PRIVATE(obj);

    switch (prop_id) {
        case PROP_SCREEN:
            g_value_set_object(value, priv->screen);
            break;

        case PROP_DESKTOPS:
            g_value_set_pointer(value, priv->desktops);
            break;

        case PROP_CHANNEL:
            g_value_set_object(value, priv->channel);
            break;

        case PROP_ICON_ON_PRIMARY:
            g_value_set_boolean(value, priv->icons_on_primary);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void
xfdesktop_icon_view_manager_finalize(GObject *obj)
{
    XfdesktopIconViewManagerPrivate *priv = XFDESKTOP_ICON_VIEW_MANAGER_GET_PRIVATE(obj);

    g_object_unref(priv->channel);
    g_object_unref(priv->screen);
    g_list_free(priv->desktops);

    G_OBJECT_CLASS(xfdesktop_icon_view_manager_parent_class)->finalize(obj);
}

static void
xfdesktop_icon_view_manager_set_show_icons_on_primary(XfdesktopIconViewManager *manager,
                                                      gboolean icons_on_primary)
{
    XfdesktopIconViewManagerPrivate *priv = XFDESKTOP_ICON_VIEW_MANAGER_GET_PRIVATE(manager);

    if (priv->icons_on_primary != icons_on_primary) {
        priv->icons_on_primary = icons_on_primary;
        g_object_notify(G_OBJECT(manager), "icons-on-primary");
    }
}

XfwScreen *
xfdesktop_icon_view_manager_get_screen(XfdesktopIconViewManager *manager) {
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW_MANAGER(manager), NULL);
    return XFDESKTOP_ICON_VIEW_MANAGER_GET_PRIVATE(manager)->screen;
}

GList *
xfdesktop_icon_view_manager_get_desktops(XfdesktopIconViewManager *manager)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW_MANAGER(manager), NULL);
    return XFDESKTOP_ICON_VIEW_MANAGER_GET_PRIVATE(manager)->desktops;
}

XfconfChannel *
xfdesktop_icon_view_manager_get_channel(XfdesktopIconViewManager *manager)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW_MANAGER(manager), NULL);
    return XFDESKTOP_ICON_VIEW_MANAGER_GET_PRIVATE(manager)->channel;
}

gboolean
xfdesktop_icon_view_manager_get_show_icons_on_primary(XfdesktopIconViewManager *manager)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW_MANAGER(manager), DEFAULT_ICONS_ON_PRIMARY);
    return XFDESKTOP_ICON_VIEW_MANAGER_GET_PRIVATE(manager)->icons_on_primary;
}

void
xfdesktop_icon_view_manager_desktop_added(XfdesktopIconViewManager *manager, XfceDesktop *desktop) {
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW_MANAGER(manager));
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    XfdesktopIconViewManagerPrivate *priv = XFDESKTOP_ICON_VIEW_MANAGER_GET_PRIVATE(manager);
    priv->desktops = g_list_append(priv->desktops, desktop);

    XfdesktopIconViewManagerClass *klass = XFDESKTOP_ICON_VIEW_MANAGER_GET_CLASS(manager);
    klass->desktop_added(manager, desktop);

    g_object_notify(G_OBJECT(manager), "desktops");
}

void
xfdesktop_icon_view_manager_desktop_removed(XfdesktopIconViewManager *manager, XfceDesktop *desktop) {
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW_MANAGER(manager));
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    XfdesktopIconViewManagerPrivate *priv = XFDESKTOP_ICON_VIEW_MANAGER_GET_PRIVATE(manager);
    priv->desktops = g_list_remove(priv->desktops, desktop);

    XfdesktopIconViewManagerClass *klass = XFDESKTOP_ICON_VIEW_MANAGER_GET_CLASS(manager);
    klass->desktop_removed(manager, desktop);

    g_object_notify(G_OBJECT(manager), "desktops");
}

GtkMenu *
xfdesktop_icon_view_manager_get_context_menu(XfdesktopIconViewManager *manager, GtkWidget *widget) {
    XfdesktopIconViewManagerClass *klass;

    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW_MANAGER(manager), NULL);

    klass = XFDESKTOP_ICON_VIEW_MANAGER_GET_CLASS(manager);
    if (klass->get_context_menu != NULL) {
        return klass->get_context_menu(manager, widget);
    } else {
        return NULL;
    }
}

void
xfdesktop_icon_view_manager_sort_icons(XfdesktopIconViewManager *manager,
                                       GtkSortType sort_type)
{
    XfdesktopIconViewManagerClass *klass;

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW_MANAGER(manager));

    klass = XFDESKTOP_ICON_VIEW_MANAGER_GET_CLASS(manager);
    if (klass->sort_icons != NULL) {
        klass->sort_icons(manager, sort_type);
    }
}

void
xfdesktop_icon_view_manager_reload(XfdesktopIconViewManager *manager) {
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW_MANAGER(manager));
    XfdesktopIconViewManagerClass *klass = XFDESKTOP_ICON_VIEW_MANAGER_GET_CLASS(manager);
    if (klass->reload != NULL) {
        klass->reload(manager);
    }
}
