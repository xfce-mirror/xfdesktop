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
#include "xfdesktop-icon-view.h"

enum {
    PROP0 = 0,
    PROP_PARENT,
    PROP_CHANNEL,
    PROP_TOOLTIP_ICON_SIZE,
    PROP_ICON_ON_PRIMARY,
};

struct _XfdesktopIconViewManagerPrivate
{
    GtkWidget *parent;
    XfconfChannel *channel;

    gint tooltip_icon_size_xfconf;
    gboolean icons_on_primary;
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
static void xfdesktop_icon_view_manager_dispose(GObject *obj);

static void xfdesktop_icon_view_manager_set_tooltip_icon_size(XfdesktopIconViewManager *manager,
                                                              gint tooltip_icon_size);
static void xfdesktop_icon_view_manager_set_show_icons_on_primary(XfdesktopIconViewManager *manager,
                                                                  gboolean icons_on_primary);

static const struct {
    const gchar *setting;
    GType setting_type;
    const gchar *property;
} setting_bindings[] = {
    { DESKTOP_ICONS_TOOLTIP_SIZE_PROP, G_TYPE_INT, "tooltip-icon-size" },
    { DESKTOP_ICONS_ON_PRIMARY_PROP, G_TYPE_BOOLEAN, "icons-on-primary" },
};


G_DEFINE_ABSTRACT_TYPE_WITH_CODE(XfdesktopIconViewManager, xfdesktop_icon_view_manager, G_TYPE_OBJECT,
                                 G_ADD_PRIVATE(XfdesktopIconViewManager))


static void
xfdesktop_icon_view_manager_class_init(XfdesktopIconViewManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->constructed = xfdesktop_icon_view_manager_constructed;
    gobject_class->set_property = xfdesktop_icon_view_manager_set_property;
    gobject_class->get_property = xfdesktop_icon_view_manager_get_property;
    gobject_class->dispose = xfdesktop_icon_view_manager_dispose;

#define PARAM_FLAGS  (G_PARAM_READWRITE \
                      | G_PARAM_STATIC_NAME \
                      | G_PARAM_STATIC_NICK \
                      | G_PARAM_STATIC_BLURB)

    g_object_class_install_property(gobject_class, PROP_PARENT,
                                    g_param_spec_object("parent",
                                                        "Parent widget",
                                                        "Widget that serves as the parent for the icon view widget(s)",
                                                        GTK_TYPE_CONTAINER,
                                                        PARAM_FLAGS | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(gobject_class, PROP_CHANNEL,
                                    g_param_spec_object("channel",
                                                        "channel",
                                                        "xfconf channel",
                                                        XFCONF_TYPE_CHANNEL,
                                                        PARAM_FLAGS | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(gobject_class, PROP_TOOLTIP_ICON_SIZE,
                                    g_param_spec_int("tooltip-icon-size",
                                                     "tooltip icon size",
                                                     "Pixel size of tooltip icon",
                                                     MIN_TOOLTIP_ICON_SIZE, MAX_TOOLTIP_ICON_SIZE, DEFAULT_TOOLTIP_ICON_SIZE,
                                                     PARAM_FLAGS));

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
    manager->priv = xfdesktop_icon_view_manager_get_instance_private(manager);

    manager->priv->tooltip_icon_size_xfconf = DEFAULT_TOOLTIP_ICON_SIZE;
    manager->priv->icons_on_primary = DEFAULT_ICONS_ON_PRIMARY;
}

static void
xfdesktop_icon_view_manager_constructed(GObject *obj)
{
    XfdesktopIconViewManager *manager = XFDESKTOP_ICON_VIEW_MANAGER(obj);

    G_OBJECT_CLASS(xfdesktop_icon_view_manager_parent_class)->constructed(obj);

    for (gsize i = 0; i < G_N_ELEMENTS(setting_bindings); ++i) {
        xfconf_g_property_bind(manager->priv->channel,
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

    switch (prop_id) {
        case PROP_PARENT:
            manager->priv->parent = g_value_get_object(value);
            break;

        case PROP_CHANNEL:
            manager->priv->channel = g_value_dup_object(value);
            break;

        case PROP_TOOLTIP_ICON_SIZE:
            xfdesktop_icon_view_manager_set_tooltip_icon_size(manager,
                                                              g_value_get_int(value));
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
    XfdesktopIconViewManager *manager = XFDESKTOP_ICON_VIEW_MANAGER(obj);

    switch (prop_id) {
        case PROP_PARENT:
            g_value_set_object(value, manager->priv->parent);
            break;

        case PROP_CHANNEL:
            g_value_set_object(value, manager->priv->channel);
            break;

        case PROP_TOOLTIP_ICON_SIZE:
            g_value_set_int(value, manager->priv->tooltip_icon_size_xfconf);
            break;

        case PROP_ICON_ON_PRIMARY:
            g_value_set_boolean(value, manager->priv->icons_on_primary);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void
xfdesktop_icon_view_manager_dispose(GObject *obj)
{
    XfdesktopIconViewManager *manager = XFDESKTOP_ICON_VIEW_MANAGER(obj);

    manager->priv->parent = NULL;
    g_clear_object(&manager->priv->channel);

    G_OBJECT_CLASS(xfdesktop_icon_view_manager_parent_class)->dispose(obj);
}

static void
xfdesktop_icon_view_manager_set_tooltip_icon_size(XfdesktopIconViewManager *manager,
                                                  gint tooltip_icon_size)
{
    if (manager->priv->tooltip_icon_size_xfconf != tooltip_icon_size) {
        manager->priv->tooltip_icon_size_xfconf = tooltip_icon_size;
        g_object_notify(G_OBJECT(manager), "tooltip-icon-size");
    }
}

static void
xfdesktop_icon_view_manager_set_show_icons_on_primary(XfdesktopIconViewManager *manager,
                                                      gboolean icons_on_primary)
{
    if (manager->priv->icons_on_primary != icons_on_primary) {
        manager->priv->icons_on_primary = icons_on_primary;
        g_object_notify(G_OBJECT(manager), "icons-on-primary");
    }
}

GtkWidget *
xfdesktop_icon_view_manager_get_parent(XfdesktopIconViewManager *manager)
{
    return manager->priv->parent;
}

XfconfChannel *
xfdesktop_icon_view_manager_get_channel(XfdesktopIconViewManager *manager)
{
    return manager->priv->channel;
}

gint
xfdesktop_icon_view_manager_get_tooltip_icon_size(XfdesktopIconViewManager *manager,
                                                  GtkWidget *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW_MANAGER(manager), DEFAULT_TOOLTIP_ICON_SIZE);
    g_return_val_if_fail(GTK_IS_WIDGET(icon_view), DEFAULT_TOOLTIP_ICON_SIZE);

    if (manager->priv->tooltip_icon_size_xfconf >= 0) {
        return manager->priv->tooltip_icon_size_xfconf;
    } else {
        gint tooltip_size = -1;
        gtk_widget_style_get(icon_view,
                             "tooltip-size", &tooltip_size,
                             NULL);
        if (tooltip_size >= 0) {
            return tooltip_size;
        } else {
            return DEFAULT_TOOLTIP_ICON_SIZE;
        }
    }
}

gboolean
xfdesktop_icon_view_manager_get_show_icons_on_primary(XfdesktopIconViewManager *manager)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW_MANAGER(manager), DEFAULT_ICONS_ON_PRIMARY);
    return manager->priv->icons_on_primary;
}

void
xfdesktop_icon_view_manager_populate_context_menu(XfdesktopIconViewManager *manager,
                                                  GtkMenuShell *menu)
{
    XfdesktopIconViewManagerClass *klass;

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW_MANAGER(manager));

    klass = XFDESKTOP_ICON_VIEW_MANAGER_GET_CLASS(manager);
    if (klass->populate_context_menu != NULL) {
        klass->populate_context_menu(manager, menu);
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
