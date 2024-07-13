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
    PROP_SCREEN,
    PROP_PARENT,
    PROP_CHANNEL,
    PROP_ICON_ON_PRIMARY,
    PROP_WORKAREA,
};

struct _XfdesktopIconViewManagerPrivate
{
    GtkWidget *parent;
    GtkFixed *container;
    XfconfChannel *channel;

    XfwScreen *xfw_screen;

    gboolean icons_on_primary;
    GdkRectangle workarea;
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
static void xfdesktop_icon_view_manager_finalize(GObject *obj);

static void xfdesktop_icon_view_manager_update_workarea(XfdesktopIconViewManager *manager);
static void xfdesktop_icon_view_manager_parent_realized(GtkWidget *parent,
                                                        XfdesktopIconViewManager *manager);
static void xfdesktop_icon_view_manager_parent_unrealized(GtkWidget *parent,
                                                          XfdesktopIconViewManager *manager);

static void xfdesktop_icon_view_manager_set_show_icons_on_primary(XfdesktopIconViewManager *manager,
                                                                  gboolean icons_on_primary);

static const struct {
    const gchar *setting;
    GType setting_type;
    const gchar *property;
} setting_bindings[] = {
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

    g_object_class_install_property(gobject_class, PROP_ICON_ON_PRIMARY,
                                    g_param_spec_boolean("icons-on-primary",
                                                         "icons on primary",
                                                         "show icons on primary desktop",
                                                         DEFAULT_ICONS_ON_PRIMARY,
                                                         PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_WORKAREA,
                                    g_param_spec_boxed("workarea",
                                                       "workarea",
                                                       "workarea",
                                                       GDK_TYPE_RECTANGLE,
                                                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

#undef PARAM_FLAGS
}

static void
xfdesktop_icon_view_manager_init(XfdesktopIconViewManager *manager)
{
    manager->priv = xfdesktop_icon_view_manager_get_instance_private(manager);

    manager->priv->icons_on_primary = DEFAULT_ICONS_ON_PRIMARY;
}

static void
xfdesktop_icon_view_manager_constructed(GObject *obj)
{
    XfdesktopIconViewManager *manager = XFDESKTOP_ICON_VIEW_MANAGER(obj);

    G_OBJECT_CLASS(xfdesktop_icon_view_manager_parent_class)->constructed(obj);

    manager->priv->container = GTK_FIXED(gtk_fixed_new());
    gtk_widget_show(GTK_WIDGET(manager->priv->container));
    gtk_container_add(GTK_CONTAINER(manager->priv->parent), GTK_WIDGET(manager->priv->container));

    for (gsize i = 0; i < G_N_ELEMENTS(setting_bindings); ++i) {
        xfconf_g_property_bind(manager->priv->channel,
                               setting_bindings[i].setting,
                               setting_bindings[i].setting_type,
                               manager,
                               setting_bindings[i].property);
    }

    g_signal_connect(manager->priv->parent, "realize",
                     G_CALLBACK(xfdesktop_icon_view_manager_parent_realized), manager);
    g_signal_connect(manager->priv->parent, "unrealize",
                     G_CALLBACK(xfdesktop_icon_view_manager_parent_unrealized), manager);

    if (gtk_widget_get_realized(manager->priv->parent)) {
        xfdesktop_icon_view_manager_parent_realized(manager->priv->parent, manager);
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
        case PROP_SCREEN:
            manager->priv->xfw_screen = g_value_dup_object(value);
            break;

        case PROP_PARENT:
            manager->priv->parent = g_value_get_object(value);
            break;

        case PROP_CHANNEL:
            manager->priv->channel = g_value_dup_object(value);
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
        case PROP_SCREEN:
            g_value_set_object(value, manager->priv->xfw_screen);
            break;

        case PROP_PARENT:
            g_value_set_object(value, manager->priv->parent);
            break;

        case PROP_CHANNEL:
            g_value_set_object(value, manager->priv->channel);
            break;

        case PROP_ICON_ON_PRIMARY:
            g_value_set_boolean(value, manager->priv->icons_on_primary);
            break;

        case PROP_WORKAREA: {
            GdkRectangle *workarea = g_new0(GdkRectangle, 1);
            *workarea = manager->priv->workarea;
            g_value_take_boxed(value, workarea);
            break;
        }

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void
xfdesktop_icon_view_manager_dispose(GObject *obj)
{
    XfdesktopIconViewManager *manager = XFDESKTOP_ICON_VIEW_MANAGER(obj);

    if (manager->priv->container != NULL) {
        gtk_widget_destroy(GTK_WIDGET(manager->priv->container));
        manager->priv->container = NULL;
    }

    if (manager->priv->parent != NULL) {
        g_signal_handlers_disconnect_by_data(manager->priv->parent, manager);
        manager->priv->parent = NULL;
    }

    G_OBJECT_CLASS(xfdesktop_icon_view_manager_parent_class)->dispose(obj);
}

static void
xfdesktop_icon_view_manager_finalize(GObject *obj) {
    XfdesktopIconViewManager *manager = XFDESKTOP_ICON_VIEW_MANAGER(obj);

    g_clear_object(&manager->priv->channel);

    for (GList *l = xfw_screen_get_monitors(manager->priv->xfw_screen); l != NULL; l = l->next) {
        g_signal_handlers_disconnect_by_data(XFW_MONITOR(l->data), manager);
    }
    g_signal_handlers_disconnect_by_data(manager->priv->xfw_screen, manager);
    g_object_unref(manager->priv->xfw_screen);

    G_OBJECT_CLASS(xfdesktop_icon_view_manager_parent_class)->finalize(obj);
}

static void
xfdesktop_icon_view_manager_parent_realized(GtkWidget *parent,
                                            XfdesktopIconViewManager *manager)
{
    xfdesktop_icon_view_manager_update_workarea(manager);

    g_signal_connect_swapped(parent, "notify::scale-factor",
                             G_CALLBACK(xfdesktop_icon_view_manager_update_workarea), manager);
    g_signal_connect_swapped(manager->priv->xfw_screen, "monitors-changed",
                             G_CALLBACK(xfdesktop_icon_view_manager_update_workarea), manager);
    for (GList *l = xfw_screen_get_monitors(manager->priv->xfw_screen); l != NULL; l = l->next) {
        g_signal_connect_swapped(XFW_MONITOR(l->data), "notify::workarea",
                                 G_CALLBACK(xfdesktop_icon_view_manager_update_workarea), manager);
    }
}

static void
xfdesktop_icon_view_manager_parent_unrealized(GtkWidget *parent,
                                              XfdesktopIconViewManager *manager)
{
    for (GList *l = xfw_screen_get_monitors(manager->priv->xfw_screen); l != NULL; l = l->next) {
        g_signal_handlers_disconnect_by_data(XFW_MONITOR(l->data), manager);
    }
    g_signal_handlers_disconnect_by_data(manager->priv->xfw_screen, manager);
    g_signal_handlers_disconnect_by_data(parent, manager);
}

static void
xfdesktop_icon_view_manager_set_show_icons_on_primary(XfdesktopIconViewManager *manager,
                                                      gboolean icons_on_primary)
{
    if (manager->priv->icons_on_primary != icons_on_primary) {
        manager->priv->icons_on_primary = icons_on_primary;
        g_object_notify(G_OBJECT(manager), "icons-on-primary");
        xfdesktop_icon_view_manager_update_workarea(manager);
    }
}

static void
xfdesktop_icon_view_manager_update_workarea(XfdesktopIconViewManager *manager)
{
    XfwMonitor *primary = xfw_screen_get_primary_monitor(manager->priv->xfw_screen);
    if (primary != NULL) {
        GdkRectangle new_workarea;
        xfw_monitor_get_workarea(primary, &new_workarea);

        if (!gdk_rectangle_equal(&manager->priv->workarea, &new_workarea)) {
            DBG("new workarea: %dx%d+%d+%d", new_workarea.width, new_workarea.height, new_workarea.x, new_workarea.y);
            manager->priv->workarea = new_workarea;
            g_object_notify(G_OBJECT(manager), "workarea");
        }
    }
}

XfwScreen *
xfdesktop_icon_view_manager_get_screen(XfdesktopIconViewManager *manager) {
    return manager->priv->xfw_screen;
}

GtkWidget *
xfdesktop_icon_view_manager_get_parent(XfdesktopIconViewManager *manager)
{
    return manager->priv->parent;
}

GtkFixed *
xfdesktop_icon_view_manager_get_container(XfdesktopIconViewManager *manager)
{
    return manager->priv->container;
}

XfconfChannel *
xfdesktop_icon_view_manager_get_channel(XfdesktopIconViewManager *manager)
{
    return manager->priv->channel;
}

gboolean
xfdesktop_icon_view_manager_get_show_icons_on_primary(XfdesktopIconViewManager *manager)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW_MANAGER(manager), DEFAULT_ICONS_ON_PRIMARY);
    return manager->priv->icons_on_primary;
}

void
xfdesktop_icon_view_manager_get_workarea(XfdesktopIconViewManager *manager,
                                         GdkRectangle *workarea)
{
    g_return_if_fail(workarea != NULL);
    *workarea = manager->priv->workarea;
}

GtkMenu *
xfdesktop_icon_view_manager_get_context_menu(XfdesktopIconViewManager *manager)
{
    XfdesktopIconViewManagerClass *klass;

    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW_MANAGER(manager), NULL);

    klass = XFDESKTOP_ICON_VIEW_MANAGER_GET_CLASS(manager);
    if (klass->get_context_menu != NULL) {
        return klass->get_context_menu(manager);
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
