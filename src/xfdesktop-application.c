/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006,2024 Brian Tarricone, <brian@tarricone.org>
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
/* Random portions taken from or inspired by the original xfdesktop for xfce4:
 *     Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *     Copyright (C) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
 *  X event forwarding code:
 *     Copyright (c) 2004 Nils Rennebarth
 * Additional portions taken from https://bugzilla.xfce.org/attachment.cgi?id=3751
 * which is in xfce4-panel git commit id 2a8de2b1b019eaef543e34764c999a409fe2bef9
 * and adapted for xfdesktop.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>

#ifdef ENABLE_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#endif  /* ENABLE_X11 */

#ifdef ENABLE_WAYLAND
#include <gtk-layer-shell.h>
#endif  /* ENABLE_WAYLAND */

#include <xfconf/xfconf.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4windowing/libxfce4windowing.h>

#include "common/xfdesktop-keyboard-shortcuts.h"
#include "menu.h"
#include "windowlist.h"
#include "xfce-desktop.h"
#include "xfdesktop-common.h"
#include "xfdesktop-backdrop-manager.h"

#ifdef ENABLE_DESKTOP_ICONS
#include "xfdesktop-icon-view-manager.h"
#include "xfdesktop-window-icon-manager.h"
#ifdef ENABLE_FILE_ICONS
#include "xfdesktop-file-icon-manager.h"
#endif
#endif

#ifdef ENABLE_X11
#include "xfdesktop-x11.h"
#endif

#ifdef HAVE_LIBNOTIFY
#include "xfdesktop-notify.h"
#endif

#include "xfdesktop-application.h"

#ifdef HAVE_XFCE_REVISION_H
#include "xfce-revision.h"
#endif

#define ACTION_RELOAD "reload"
#define ACTION_NEXT "next"
#define ACTION_MENU "menu"
#define ACTION_ARRANGE "arrange"
#define ACTION_DEBUG "debug"
#define ACTION_QUIT "quit"

#define OPTION_VERSION "version"
#define OPTION_RELOAD ACTION_RELOAD
#define OPTION_NEXT ACTION_NEXT
#define OPTION_MENU ACTION_MENU
#define OPTION_WINDOWLIST "windowlist"
#define OPTION_ARRANGE ACTION_ARRANGE
#define OPTION_ENABLE_DEBUG "enable-debug"
#define OPTION_DISABLE_DEBUG "disable-debug"
#define OPTION_QUIT ACTION_QUIT
#define OPTION_DISABLE_WM_CHECK "disable-wm-check"

typedef GtkMenu *(*PopulateMenuFunc)(GtkMenu *, gint);

static void xfdesktop_application_constructed(GObject *object);
static void xfdesktop_application_set_property(GObject *object,
                                               guint property_id,
                                               const GValue *value,
                                               GParamSpec *pspec);
static void xfdesktop_application_get_property(GObject *object,
                                               guint property_id,
                                               GValue *value,
                                               GParamSpec *pspec);
static void xfdesktop_application_finalize(GObject *object);

static void session_logout(XfdesktopApplication *app);
static void session_die(XfdesktopApplication *app);

static void xfdesktop_application_action_activated(GAction *action,
                                                   GVariant *parameter,
                                                   gpointer data);
static void xfdesktop_handle_quit_signals(gint sig, gpointer user_data);

static void xfdesktop_application_startup(GApplication *g_application);
static void xfdesktop_application_start(XfdesktopApplication *app);
static void xfdesktop_application_shutdown(GApplication *g_application);

static gint xfdesktop_application_handle_local_options(GApplication *g_application,
                                                       GVariantDict *options);
static gint xfdesktop_application_command_line(GApplication *g_application,
                                               GApplicationCommandLine *command_line);

static void popup_root_menu(XfdesktopApplication *app,
                            XfceDesktop *desktop,
                            guint button,
                            gint x,
                            gint y,
                            guint activate_time);
static void popup_secondary_root_menu(XfdesktopApplication *app,
                                      XfceDesktop *desktop,
                                      guint button,
                                      gint x,
                                      gint y,
                                      guint activate_time);

static gboolean xfce_desktop_button_press_event(GtkWidget *widget,
                                                GdkEventButton *evt,
                                                XfdesktopApplication *app);
static gboolean xfce_desktop_button_release_event(GtkWidget *widget,
                                                  GdkEventButton *evt,
                                                  XfdesktopApplication *app);
static gboolean xfce_desktop_popup_menu(GtkWidget *widget,
                                        XfdesktopApplication *app);
static gboolean xfce_desktop_delete_event(GtkWidget *w,
                                          GdkEventAny *evt,
                                          XfdesktopApplication *app);

static void xfdesktop_application_set_icon_style(XfdesktopApplication *app,
                                                 XfceDesktopIconStyle style);

static void desktop_action_fixup(XfceGtkActionEntry *entry);

#ifdef ENABLE_X11
static void cancel_wait_for_wm(XfdesktopApplication *app);
#endif

enum {
    PROP0,
    PROP_ICON_STYLE,
};

typedef struct {
    gboolean version;
    gboolean has_remote_only_command;
} XfdesktopLocalArgs;

struct _XfdesktopApplication
{
    GtkApplication parent;

    XfconfChannel *channel;
    XfwScreen *screen;
    GdkScreen *gdkscreen;
    XfdesktopBackdropManager *backdrop_manager;

    GtkAccelGroup *accel_group;

    GList *desktops;  // XfceDesktop
    GHashTable *monitors;  // XfwMonitor -> XfceDesktop

    XfdesktopLocalArgs *args;

#ifdef ENABLE_X11
    GCancellable *cancel_wait_for_wm;
    gboolean disable_wm_check;

    GdkWindow *selection_window;

    XfceSMClient *sm_client;
#endif

    XfceDesktopIconStyle icon_style;
#ifdef ENABLE_DESKTOP_ICONS
    XfdesktopIconViewManager *icon_view_manager;
#endif

    GtkMenu *active_root_menu;
};

struct _XfdesktopApplicationClass
{
    GtkApplicationClass parent;
};

const gchar *fallback_CSS =
"XfdesktopIconView.view {"
"	background-color: transparent;"
"}"
"XfdesktopIconView.view:active {"
"	color: @theme_selected_bg_color;"
"}"
"XfdesktopIconView.view.label {"
"	background-color: transparent;"
"	border-radius: 3px;"
"	color: @theme_selected_fg_color;"
"	text-shadow: 1px 1px 2px black;"
"}"
"XfdesktopIconView.view.label:backdrop {"
"	background-color: transparent;"
"}"
"XfdesktopIconView.view.label:selected {"
"	background-color: @theme_selected_bg_color;"
"}"
"XfdesktopIconView.view.label:selected:backdrop {"
"	background-color: alpha(@theme_selected_bg_color, 0.5);"
"}"
"XfdesktopIconView.rubberband {"
"	background-color: alpha(@theme_selected_bg_color, 0.2);"
"	border: 1px solid @theme_selected_bg_color;"
"	border-radius: 0;"
"}";


G_DEFINE_TYPE(XfdesktopApplication, xfdesktop_application, GTK_TYPE_APPLICATION)


static void
xfdesktop_application_class_init(XfdesktopApplicationClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GApplicationClass *gapplication_class = G_APPLICATION_CLASS(klass);

    gobject_class->constructed = xfdesktop_application_constructed;
    gobject_class->set_property = xfdesktop_application_set_property;
    gobject_class->get_property = xfdesktop_application_get_property;
    gobject_class->finalize = xfdesktop_application_finalize;

    gapplication_class->startup = xfdesktop_application_startup;
    gapplication_class->shutdown = xfdesktop_application_shutdown;
    gapplication_class->handle_local_options = xfdesktop_application_handle_local_options;
    gapplication_class->command_line = xfdesktop_application_command_line;

    g_object_class_install_property(gobject_class,
                                    PROP_ICON_STYLE,
                                    g_param_spec_enum("icon-style",
                                                      "icon-style",
                                                      "icon-style",
                                                      XFCE_TYPE_DESKTOP_ICON_STYLE,
                                                      ICON_STYLE_DEFAULT,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
xfdesktop_application_init(XfdesktopApplication *app)
{
    XfdesktopLocalArgs *args = g_new0(XfdesktopLocalArgs, 1);
    const GOptionEntry main_entries[] = {
        { OPTION_VERSION, 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &args->version, N_("Display version information"), NULL },
        { OPTION_RELOAD, 'R', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, NULL, N_("Reload all settings"), NULL },
        { OPTION_NEXT, 'N', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, NULL, N_("Advance to the next wallpaper on the current workspace"), NULL },
        { OPTION_MENU, 'M', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, NULL, N_("Pop up the menu (at the current mouse position)"), NULL },
        { OPTION_WINDOWLIST, 'W', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, NULL, N_("Pop up the window list (at the current mouse position)"), NULL },
#ifdef ENABLE_FILE_ICONS
        { OPTION_ARRANGE, 'A', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, NULL, N_("Automatically arrange all the icons on the desktop"), NULL },
#endif
        { OPTION_ENABLE_DEBUG, 'e', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, NULL, N_("Enable debug messages"), NULL },
        { OPTION_DISABLE_DEBUG, 'd', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, NULL, N_("Disable debug messages"), NULL },
#ifdef ENABLE_X11
        { OPTION_DISABLE_WM_CHECK, 'D', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &app->disable_wm_check, N_("Do not wait for a window manager on startup"), NULL },
#endif
        { ACTION_QUIT, 'Q', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, NULL, N_("Cause xfdesktop to quit"), NULL },
        G_OPTION_ENTRY_NULL
    };
    const struct {
        const gchar *name;
        const GVariantType *arg_type;
    } actions[] = {
        { ACTION_RELOAD, NULL },
        { ACTION_NEXT, NULL },
        { ACTION_QUIT, NULL },
        { ACTION_MENU, G_VARIANT_TYPE_BOOLEAN },
        { ACTION_ARRANGE, NULL },
        { ACTION_DEBUG, G_VARIANT_TYPE_BOOLEAN },
    };

    app->args = args;
    app->icon_style = -1;
    app->monitors = g_hash_table_new(g_direct_hash, g_direct_equal);

    g_application_add_main_option_entries(G_APPLICATION(app), main_entries);

    for (gsize i = 0; i < G_N_ELEMENTS(actions); ++i) {
        GSimpleAction *action = g_simple_action_new(actions[i].name, actions[i].arg_type);
        g_signal_connect(action, "activate", G_CALLBACK(xfdesktop_application_action_activated), app);
        g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(action));
        g_object_unref(action);
    }
}

static void
xfdesktop_application_constructed(GObject *object) {
    G_OBJECT_CLASS(xfdesktop_application_parent_class)->constructed(object);

    XfdesktopApplication *app = XFDESKTOP_APPLICATION(object);

    GError *error = NULL;
    if (!xfconf_init(&error)) {
        g_warning("%s: unable to connect to settings daemon: %s.  Defaults will be used",
                  PACKAGE, error->message);
        g_clear_error(&error);
        error = NULL;
    } else {
        app->channel = xfconf_channel_get(XFDESKTOP_CHANNEL);
        g_object_add_weak_pointer(G_OBJECT(app->channel), (gpointer *)&app->channel);
    }

}

static void
xfdesktop_application_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(object);

    switch (property_id) {
        case PROP_ICON_STYLE:
            xfdesktop_application_set_icon_style(app, g_value_get_enum(value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfdesktop_application_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(object);

    switch (property_id) {
        case PROP_ICON_STYLE:
            g_value_set_enum(value, app->icon_style);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfdesktop_application_finalize(GObject *object)
{
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(object);

    if (app->accel_group != NULL) {
        gsize n_actions;
        XfceGtkActionEntry *actions = xfdesktop_get_desktop_actions(desktop_action_fixup, &n_actions);
        xfce_gtk_accel_group_disconnect_action_entries(app->accel_group, actions, n_actions);
        g_object_unref(app->accel_group);
    }

    g_signal_handlers_disconnect_by_data(gtk_accel_map_get(), app);
    xfdesktop_keyboard_shortcuts_shutdown();

    g_free(app->args);
    g_hash_table_destroy(app->monitors);

#ifdef ENABLE_X11
    cancel_wait_for_wm(app);

    if (app->selection_window != NULL) {
        gdk_window_destroy(app->selection_window);
    }
#endif

    G_OBJECT_CLASS(xfdesktop_application_parent_class)->finalize(object);
}

/**
 * xfdesktop_application_get:
 *
 * Singleton. Additional calls increase the reference count.
 *
 * Return value: #XfdesktopApplication, free with g_object_unref.
 **/
XfdesktopApplication *
xfdesktop_application_get(void)
{
    static XfdesktopApplication *app = NULL;

    if(app == NULL) {
       app = g_object_new(XFDESKTOP_TYPE_APPLICATION,
                          "application-id", "org.xfce.xfdesktop",
                          "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
                          NULL);
       g_object_add_weak_pointer(G_OBJECT(app), (gpointer)&app);
    } else {
        g_object_ref(app);
    }

    return app;
}

static void
session_logout(XfdesktopApplication *app) {
#ifdef ENABLE_X11
    if (app->sm_client != NULL) {
        xfce_sm_client_request_shutdown(app->sm_client, XFCE_SM_CLIENT_SHUTDOWN_HINT_ASK);
    }
#endif
}

static void
session_die(XfdesktopApplication *app) {
    TRACE("entering");

    // Release our own hold on the app
    g_object_ref(app);
    g_application_release(G_APPLICATION(app));

#ifdef ENABLE_X11
    cancel_wait_for_wm(app);
#endif

    for (guint main_level = gtk_main_level(); main_level > 0; --main_level) {
        gtk_main_quit();
    }

    g_application_quit(G_APPLICATION(app));
    g_object_unref(app);
}

static gboolean
scroll_cb(GtkWidget *w, GdkEventScroll *evt, gpointer user_data)
{
#ifdef ENABLE_X11
    if (xfw_windowing_get() == XFW_WINDOWING_X11) {
        xfdesktop_x11_desktop_scrolled(w, evt);
        return TRUE;
    }
#endif

    return FALSE;
}

static gboolean
reload_idle_cb(gpointer data)
{
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(data);

    TRACE("entering");

    for (GList *l = app->desktops; l != NULL; l = l->next) {
        XfceDesktop *desktop = XFCE_DESKTOP(l->data);
        xfce_desktop_refresh(desktop);
    }

#ifdef ENABLE_DESKTOP_ICONS
    if (app->icon_view_manager != NULL) {
        xfdesktop_icon_view_manager_reload(app->icon_view_manager);
    }
#endif

    g_application_release(G_APPLICATION(app));

    return FALSE;
}

static XfceDesktop *
find_desktop_for_monitor(XfdesktopApplication *app, GdkMonitor *monitor) {
    g_return_val_if_fail(GDK_IS_MONITOR(monitor), NULL);

    for (GList *l = app->desktops; l != NULL; l = l->next) {
        XfceDesktop *desktop = XFCE_DESKTOP(l->data);
        if (xfw_monitor_get_gdk_monitor(xfce_desktop_get_monitor(desktop)) == monitor) {
            return desktop;
        }
    }

    DBG("No XfceDesktop found for monitor '%s'", gdk_monitor_get_model(monitor));
    return NULL;
}

static XfceDesktop *
find_active_desktop(XfdesktopApplication *app) {
    XfceDesktop *desktop = NULL;
    GdkDisplay *display = gdk_display_get_default();

#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY(display)) {
        // Wayland doesn't allow getting the absolute pointer position
        GdkSeat *seat = gdk_display_get_default_seat(display);
        GdkDevice *device = gdk_seat_get_pointer(seat);
        gint pointer_x, pointer_y;
        gdk_device_get_position(device, NULL, &pointer_x, &pointer_y);
        GdkMonitor *monitor = gdk_display_get_monitor_at_point(display, pointer_x, pointer_y);
        if (monitor != NULL) {
            desktop = find_desktop_for_monitor(app, monitor);
        }
    }
#endif

#ifdef ENABLE_DESKTOP_ICONS
    if (desktop == NULL && app->icon_view_manager != NULL) {
        desktop = xfdesktop_icon_view_manager_get_focused_desktop(app->icon_view_manager);
    }
#endif

    if (desktop == NULL) {
        for (GList *l = app->desktops; l != NULL; l = l->next) {
            XfceDesktop *a_desktop = XFCE_DESKTOP(l->data);
            if (xfce_desktop_is_active(a_desktop)) {
                desktop = a_desktop;
                break;
            }
        }
    }

    if (desktop == NULL) {
        GdkMonitor *monitor = gdk_display_get_monitor(display, 0);
        desktop = find_desktop_for_monitor(app, monitor);
    }

    if (G_UNLIKELY(desktop == NULL)) {
        if (app->desktops != NULL) {
            desktop = XFCE_DESKTOP(g_list_nth_data(app->desktops, 0));
        }
    }

    return desktop;
}

static void
desktop_action_reload(XfdesktopApplication *app) {
    /* hold the app so it doesn't quit while we queue up a refresh */
    g_application_hold(G_APPLICATION(app));
    g_idle_add(reload_idle_cb, app);
}

static void
desktop_action_primary_menu(XfdesktopApplication *app) {
    XfceDesktop *desktop = find_active_desktop(app);
    if (desktop != NULL) {
        popup_root_menu(app, desktop, 0, -1, -1, GDK_CURRENT_TIME);
    }
}

static void
desktop_action_secondary_menu(XfdesktopApplication *app) {
    XfceDesktop *desktop = find_active_desktop(app);
    if (desktop != NULL) {
        popup_secondary_root_menu(app, desktop, 0, -1, -1, GDK_CURRENT_TIME);
    }
}

static void
desktop_action_next_background(XfdesktopApplication *app) {
    for (GList *l = app->desktops; l != NULL; l = l->next) {
        XfceDesktop *desktop = XFCE_DESKTOP(l->data);
        xfce_desktop_cycle_backdrop(XFCE_DESKTOP(desktop));
    }
}

static void
desktop_action_fixup(XfceGtkActionEntry *entry) {
    switch (entry->id) {
        case XFCE_DESKTOP_ACTION_RELOAD:
        case XFCE_DESKTOP_ACTION_RELOAD_ALT_1:
        case XFCE_DESKTOP_ACTION_RELOAD_ALT_2:
            entry->callback = G_CALLBACK(desktop_action_reload);
            break;
        case XFCE_DESKTOP_ACTION_POPUP_PRIMARY_MENU:
        case XFCE_DESKTOP_ACTION_POPUP_PRIMARY_MENU_ALT_1:
            entry->callback = G_CALLBACK(desktop_action_primary_menu);
            break;
        case XFCE_DESKTOP_ACTION_POPUP_SECONDARY_MENU:
        case XFCE_DESKTOP_ACTION_POPUP_SECONDARY_MENU_ALT_1:
            entry->callback = G_CALLBACK(desktop_action_secondary_menu);
            break;
        case XFCE_DESKTOP_ACTION_NEXT_BACKGROUND:
            entry->callback = G_CALLBACK(desktop_action_next_background);
            break;
        default:
            g_assert_not_reached();
            break;
    }
}

static void
xfdesktop_application_action_activated(GAction *action, GVariant *parameter, gpointer data) {
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(data);
    const gchar *name = g_action_get_name(action);

    TRACE("entering: %s", name);

    if (g_strcmp0(name, ACTION_RELOAD) == 0) {
        desktop_action_reload(app);
    } else if (g_strcmp0(name, ACTION_NEXT) == 0) {
        desktop_action_next_background(app);
    } else if (g_strcmp0(name, ACTION_MENU) == 0) {
        if (g_variant_is_of_type(parameter, G_VARIANT_TYPE_BOOLEAN)) {
            if (g_variant_get_boolean(parameter)) {
                desktop_action_primary_menu(app);
            } else {
                desktop_action_secondary_menu(app);
            }
        }
    } else if (g_strcmp0(name, ACTION_ARRANGE) == 0) {
#ifdef ENABLE_DESKTOP_ICONS
        if (app->icon_view_manager != NULL) {
            xfdesktop_icon_view_manager_sort_icons(app->icon_view_manager,
                                                   GTK_SORT_ASCENDING,
                                                   XFDESKTOP_ICON_VIEW_MANAGER_SORT_ALL_DESKTOPS);
        }
#endif
    } else if (g_strcmp0(name, ACTION_QUIT) == 0) {
#ifdef ENABLE_X11
        /* If the user told xfdesktop to quit, set the restart style to something
         * where it won't restart itself */
        if (app->sm_client && XFCE_IS_SM_CLIENT(app->sm_client)) {
            xfce_sm_client_set_restart_style(app->sm_client, XFCE_SM_CLIENT_RESTART_NORMAL);
        }
#endif

        session_die(app);
    } else if (g_strcmp0(name, ACTION_DEBUG) == 0) {
        if (g_variant_is_of_type(parameter, G_VARIANT_TYPE_BOOLEAN)) {
            xfdesktop_debug_set(g_variant_get_boolean(parameter));
        }
    } else {
        g_message("Unhandled action '%s'", name);
    }
}

static void
xfdesktop_handle_quit_signals(gint sig, gpointer user_data) {
    TRACE("entering");
    session_die(XFDESKTOP_APPLICATION(user_data));
}

#ifdef ENABLE_X11
static void
cancel_wait_for_wm(XfdesktopApplication *app) {
    if (app->cancel_wait_for_wm != NULL) {
        g_cancellable_cancel(app->cancel_wait_for_wm);
        g_clear_object(&app->cancel_wait_for_wm);
        g_application_release(G_APPLICATION(app));
    }
}

static void
wait_for_wm_complete(WaitForWMStatus status, gpointer data) {
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(data);

    switch (status) {
        case WAIT_FOR_WM_CANCELLED:
            // NB: dont't touch 'app' here, as it may have already been freed.
            break;

        case WAIT_FOR_WM_FAILED:
            g_printerr("No window manager registered on screen 0. "
                       "To start the xfdesktop without this check, run with --disable-wm-check.\n");
            // Intentionally fall through

        case WAIT_FOR_WM_SUCCESSFUL:
            g_clear_object(&app->cancel_wait_for_wm);
            xfdesktop_application_start(app);
            g_application_release(G_APPLICATION(app));
            break;
    }
}
#endif

static void
accel_map_changed(XfdesktopApplication *app) {
    TRACE("entering");

    gsize n_actions;
    XfceGtkActionEntry *actions = xfdesktop_get_desktop_actions(desktop_action_fixup, &n_actions);

    if (app->accel_group != NULL) {
        xfce_gtk_accel_group_disconnect_action_entries(app->accel_group, actions, n_actions);
    } else {
        app->accel_group = gtk_accel_group_new();
    }

    xfce_gtk_accel_group_connect_action_entries(app->accel_group, actions, n_actions, app);
}

static void
xfdesktop_application_startup(GApplication *g_application)
{
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(g_application);

    TRACE("entering");

    if (app->args->has_remote_only_command) {
        g_printerr(PACKAGE " is not running\n");
        exit(1);
    }

    g_clear_pointer(&app->args, g_free);

    G_APPLICATION_CLASS(xfdesktop_application_parent_class)->startup(g_application);

    xfdesktop_keyboard_shortcuts_init();

    accel_map_changed(app);
    g_signal_connect_data(gtk_accel_map_get(),
                          "changed",
                          G_CALLBACK(accel_map_changed),
                          app,
                          NULL,
                          G_CONNECT_SWAPPED | G_CONNECT_AFTER);

#ifdef ENABLE_X11
    if(!app->disable_wm_check && xfw_windowing_get() == XFW_WINDOWING_X11) {
        g_application_hold(g_application);
        app->cancel_wait_for_wm = g_cancellable_new();
        xfdesktop_x11_wait_for_wm(wait_for_wm_complete,
                                  g_application,
                                  app->cancel_wait_for_wm);
    } else
#endif  /* ENABLE_X11 */
    {
        /* directly launch */
        xfdesktop_application_start(app);
    }
}

static void
xfdesktop_application_theme_changed (GtkSettings *settings,
                                     XfdesktopApplication *app)
{
    GtkCssProvider *provider = NULL;
    static GtkCssProvider *custom_provider = NULL;
    gchar *theme;
    gchar *css;

    g_object_get(settings, "gtk-theme-name", &theme, NULL);

    provider = gtk_css_provider_get_named(theme, NULL);
    css = gtk_css_provider_to_string (provider);

    if (g_strrstr (css, "XfdesktopIconView") != NULL) {
        DBG("XfdesktopIconView section found in theme %s", theme);
        if (custom_provider != NULL) {
            gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (),
                                                          GTK_STYLE_PROVIDER(custom_provider));
            g_clear_object (&custom_provider);
        }
    } else {
        DBG("XfdesktopIconView section not found in theme %s, setting our fallback", theme);
        if (custom_provider != NULL) {
            gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (),
                                                          GTK_STYLE_PROVIDER(custom_provider));
            g_clear_object (&custom_provider);
        }
        custom_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(custom_provider,
                                        fallback_CSS,
                                        -1,
                                        NULL);
        gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                   GTK_STYLE_PROVIDER(custom_provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    g_free(css);
    g_free(theme);
}

static void
desktop_destroyed(XfceDesktop *desktop, XfdesktopApplication *app) {
    gtk_application_remove_window(GTK_APPLICATION(app), GTK_WINDOW(desktop));
    g_hash_table_remove(app->monitors, xfce_desktop_get_monitor(desktop));
    app->desktops = g_list_remove(app->desktops, desktop);
}

static GtkWidget *
create_desktop(XfdesktopApplication *app, XfwMonitor *monitor) {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gint screen_num = gdk_screen_get_number(app->gdkscreen);
G_GNUC_END_IGNORE_DEPRECATIONS
    gchar *property_prefix = g_strdup_printf("/backdrop/screen%d/", screen_num);
    GtkWidget *desktop = xfce_desktop_new(app->gdkscreen,
                                          monitor,
                                          app->channel,
                                          property_prefix,
                                          app->backdrop_manager);
    g_free(property_prefix);

    gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(desktop));
    g_signal_connect(desktop, "destroy",
                     G_CALLBACK(desktop_destroyed), app);

    gtk_window_add_accel_group(GTK_WINDOW(desktop), app->accel_group);
    gtk_widget_add_events(desktop, GDK_BUTTON_PRESS_MASK
                          | GDK_BUTTON_RELEASE_MASK);
    g_signal_connect(desktop, "button-press-event",
                     G_CALLBACK(xfce_desktop_button_press_event), app);
    g_signal_connect(desktop, "button-release-event",
                     G_CALLBACK(xfce_desktop_button_release_event), app);
    g_signal_connect(desktop, "popup-menu",
                     G_CALLBACK(xfce_desktop_popup_menu), app);
    g_signal_connect(desktop, "delete-event",
                     G_CALLBACK(xfce_desktop_delete_event), app);

    if (xfw_windowing_get() == XFW_WINDOWING_X11) {
        /* hook into the scroll event so we can forward it to the window
         * manager */
        gtk_widget_add_events(desktop, GDK_SCROLL_MASK);
        g_signal_connect(G_OBJECT(desktop), "scroll-event",
                         G_CALLBACK(scroll_cb), app);
    }

    gtk_widget_show_all(desktop);

    return desktop;
}

static void
add_monitor_desktop(XfdesktopApplication *app, XfwMonitor *monitor) {
    if (!g_hash_table_contains(app->monitors, monitor)) {
        DBG("adding %s", xfw_monitor_get_description(monitor));

        GtkWidget *desktop = create_desktop(app, monitor);
        app->desktops = g_list_append(app->desktops, desktop);
        g_hash_table_insert(app->monitors, monitor, desktop);

#ifdef ENABLE_DESKTOP_ICONS
        if (app->icon_view_manager != NULL) {
            xfdesktop_icon_view_manager_desktop_added(app->icon_view_manager, XFCE_DESKTOP(desktop));
        }
#endif
    }
}

static void
remove_monitor_desktop(XfdesktopApplication *app, XfwMonitor *monitor) {
    XfceDesktop *desktop = g_hash_table_lookup(app->monitors, monitor);
    if (desktop != NULL) {
        DBG("removing %s", xfw_monitor_get_description(monitor));

#ifdef ENABLE_DESKTOP_ICONS
        if (app->icon_view_manager != NULL) {
            xfdesktop_icon_view_manager_desktop_removed(app->icon_view_manager, desktop);
        }
#endif

        gtk_widget_destroy(GTK_WIDGET(desktop));

        g_assert(g_list_find(app->desktops, desktop) == NULL);
        g_assert(!g_hash_table_contains(app->monitors, monitor));
    }
}

// For each mirror set, we ensure the first monitor in the set is shown, and
// the rest are hidden.
static void
handle_new_mirror_sets(XfdesktopApplication *app, GList *mirror_sets) {
    for (GList *lms = mirror_sets; lms != NULL; lms = lms->next) {
        GList *mirror_set = lms->data;

        XfwMonitor *preferred_monitor = g_list_nth_data(mirror_set, 0);
        g_assert(preferred_monitor != NULL);
        add_monitor_desktop(app, preferred_monitor);

        for (GList *lm = mirror_set->next; lm != NULL; lm = lm->next) {
            XfwMonitor *hidden_monitor = XFW_MONITOR(lm->data);
            remove_monitor_desktop(app, hidden_monitor);
        }
    }
}

// A monitor is "better" if it is primary.  If there are no primary monitors, a
// monitor is "better" if it has a larger logical pixel area.
static gint
mirror_set_monitors_compare(gconstpointer a, gconstpointer b) {
    XfwMonitor *am = XFW_MONITOR((gpointer)a);
    XfwMonitor *bm = XFW_MONITOR((gpointer)b);

    if (xfw_monitor_is_primary(am)) {
        return -1;
    } else if (xfw_monitor_is_primary(bm)) {
        return 1;
    } else {
        GdkRectangle a_geom;
        xfw_monitor_get_logical_geometry(am, &a_geom);
        gint a_area = a_geom.width * a_geom.height;

        GdkRectangle b_geom;
        xfw_monitor_get_logical_geometry(bm, &b_geom);
        gint b_area = a_geom.width * a_geom.height;

        return CLAMP(b_area - a_area, -1, 1);
    }
}

static GList *  // GList of GList of XfwMonitor (aka List<List<XfwMonitor>>)
build_monitor_mirror_sets(XfdesktopApplication *app) {
    GList *mirror_sets = NULL;

    // First we build a list of "mirror sets".  This is a list of lists.  Each
    // list contains a series of monitors that mirror each other.  If a monitor
    // has no mirror, it will be the only monitor in the list.
    GList *monitors = g_list_copy(xfw_screen_get_monitors(app->screen));
    for (GList *lm = monitors; lm != NULL;) {
        XfwMonitor *monitor = XFW_MONITOR(lm->data);
        GList *cur_mirror_set = g_list_append(NULL, monitor);

        GdkRectangle geom;
        xfw_monitor_get_logical_geometry(monitor, &geom);

        GList *remaining = lm->next;
        g_list_free_1(lm);
        if (remaining != NULL) {
            remaining->prev = NULL;
        }

        for (GList *lr = remaining; lr != NULL;) {
            GList *cur = lr;
            lr = lr->next;

            XfwMonitor *a_monitor = XFW_MONITOR(cur->data);
            GdkRectangle a_geom;
            xfw_monitor_get_logical_geometry(a_monitor, &a_geom);

            // We define a mirror as two monitors with the same x & y
            // coordinates.  It's possible that someone could set up a geometry
            // where monitors overlap, but not with the same x & y coordinates.
            // But I think we are just not going to handle that situation, in
            // which case things will just be broken, and that's life.
            if (a_geom.x == geom.x && a_geom.y == geom.y) {
                remaining = g_list_delete_link(remaining, cur);
                cur_mirror_set = g_list_append(cur_mirror_set, a_monitor);
            }
        }

        mirror_sets = g_list_append(mirror_sets, cur_mirror_set);

        lm = remaining;
    }

    // Now we sort each mirror set to decide which monitor in the mirror set is
    // the one that gets an XfceDesktop associated with it.  That monitor will
    // be placed first in that mirror set.
    for (GList *ls = mirror_sets; ls != NULL; ls = ls->next) {
        GList *mirror_set = ls->data;
        mirror_set = g_list_sort(mirror_set, mirror_set_monitors_compare);
        ls->data = mirror_set;
    }

    return mirror_sets;
}

static void
handle_monitors_changed(XfdesktopApplication *app) {
    GList *mirror_sets = build_monitor_mirror_sets(app);
    handle_new_mirror_sets(app, mirror_sets);
    g_list_free_full(mirror_sets, (GDestroyNotify)g_list_free);
}

static void
monitor_changed(XfwMonitor *monitor, GParamSpec *pspec, XfdesktopApplication *app) {
    TRACE("entering, %s", xfw_monitor_get_description(monitor));
    handle_monitors_changed(app);
}

static void
screen_monitor_added(XfwScreen *screen, XfwMonitor *monitor, XfdesktopApplication *app) {
    TRACE("entering, %s", xfw_monitor_get_description(monitor));

    g_signal_connect(monitor, "notify::logical-geometry",
                     G_CALLBACK(monitor_changed), app);
    g_signal_connect(monitor, "notify::is-primary",
                     G_CALLBACK(monitor_changed), app);

    handle_monitors_changed(app);
}

static void
screen_monitor_removed(XfwScreen *screen, XfwMonitor *monitor, XfdesktopApplication *app) {
    TRACE("entering, %s", xfw_monitor_get_description(monitor));
    g_signal_handlers_disconnect_by_data(monitor, app);
    remove_monitor_desktop(app, monitor);
    handle_monitors_changed(app);
}

static void
xfdesktop_application_start(XfdesktopApplication *app)
{
    GtkSettings *settings;
    GdkDisplay *gdpy;
    GError *error = NULL;

    TRACE("entering");

    if (xfw_windowing_get() == XFW_WINDOWING_WAYLAND) {
#ifdef ENABLE_WAYLAND
        if (!gtk_layer_is_supported()) {
            g_critical("Your compositor must support the zwlr_layer_shell_v1 protocol");
            exit(1);
        }
#else
        g_critical("xfdesktop was not built with Wayland support");
        exit(1);
#endif
    }

    app->screen = xfw_screen_get_default();

    settings = gtk_settings_get_default();
    g_signal_connect (settings, "notify::gtk-theme-name", G_CALLBACK (xfdesktop_application_theme_changed), NULL);
    xfdesktop_application_theme_changed (settings, app);

    gdpy = gdk_display_get_default();
    app->gdkscreen = gdk_display_get_default_screen(gdpy);

#ifdef ENABLE_X11
    if (xfw_windowing_get() == XFW_WINDOWING_X11) {
        app->selection_window = xfdesktop_x11_set_desktop_manager_selection(app->gdkscreen, &error);
        if (app->selection_window == NULL) {
            g_error("%s", error->message);
            g_error_free(error);
            exit(1);
        }

        /* setup the session management options */
        app->sm_client = xfce_sm_client_get();
        g_object_add_weak_pointer(G_OBJECT(app->sm_client), (gpointer *)&app->sm_client);
        xfce_sm_client_set_restart_style(app->sm_client, XFCE_SM_CLIENT_RESTART_IMMEDIATELY);
        xfce_sm_client_set_priority(app->sm_client, XFCE_SM_CLIENT_PRIORITY_DESKTOP);
        g_signal_connect_swapped(app->sm_client, "quit", G_CALLBACK(session_die), app);

        if(!xfce_sm_client_connect(app->sm_client, &error) && error) {
            g_printerr("Failed to connect to session manager: %s\n", error->message);
            g_clear_error(&error);
        }
    }
#endif

    if (app->channel != NULL) {
        xfdesktop_migrate_backdrop_settings(gdk_display_get_default(), app->channel);
    }

    app->backdrop_manager = xfdesktop_backdrop_manager_new(app->screen, app->channel);

    menu_init(app->channel);
    windowlist_init(app->channel);

    GList *monitors = xfw_screen_get_monitors(app->screen);
    for (GList *l = monitors; l != NULL; l = l->next) {
        XfwMonitor *monitor = XFW_MONITOR(l->data);
        screen_monitor_added(app->screen, monitor, app);
    }

    g_signal_connect(app->screen, "monitor-added",
                     G_CALLBACK(screen_monitor_added), app);
    g_signal_connect(app->screen, "monitor-removed",
                     G_CALLBACK(screen_monitor_removed), app);

    if (g_list_length(app->desktops) == 1) {
        xfce_desktop_set_is_active(XFCE_DESKTOP(g_list_nth_data(app->desktops, 0)), TRUE);
    }

    xfconf_g_property_bind(app->channel, DESKTOP_ICONS_STYLE_PROP, XFCE_TYPE_DESKTOP_ICON_STYLE, app, "icon-style");
    if ((gint)app->icon_style == -1) {
        XfceDesktopIconStyle icon_style = xfconf_channel_get_int(app->channel,
                                                                 DESKTOP_ICONS_STYLE_PROP,
                                                                 ICON_STYLE_DEFAULT);
        xfdesktop_application_set_icon_style(app, icon_style);
    }

    // Put a hold on the app, because at times we may have no monitors
    // (suspend/resume, etc.), which will cause us to destroy all our
    // toplevels, which will cause GApplication to quit.
    g_application_hold(G_APPLICATION(app));

    /* hook up to the different quit signals */
    if (xfce_posix_signal_handler_init(&error)) {
        xfce_posix_signal_handler_set_handler(SIGHUP,
                                              xfdesktop_handle_quit_signals,
                                              app, NULL);
        xfce_posix_signal_handler_set_handler(SIGINT,
                                              xfdesktop_handle_quit_signals,
                                              app, NULL);
        xfce_posix_signal_handler_set_handler(SIGTERM,
                                              xfdesktop_handle_quit_signals,
                                              app, NULL);
    } else {
        g_warning("Unable to set up POSIX signal handlers: %s", error->message);
        g_clear_error(&error);
    }
}

static void
xfdesktop_application_shutdown(GApplication *g_application)
{
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(g_application);

    TRACE("entering");

    if (app->active_root_menu != NULL) {
        gtk_menu_shell_deactivate(GTK_MENU_SHELL(app->active_root_menu));
        app->active_root_menu = NULL;
    }


#ifdef ENABLE_X11
    cancel_wait_for_wm(app);

    if (xfw_windowing_get() == XFW_WINDOWING_X11) {
        xfdesktop_x11_set_compat_properties(NULL);
    }
#endif

    if (app->channel != NULL) {
        menu_cleanup(app->channel);
        windowlist_cleanup(app->channel);
        app->channel = NULL;
    }

    g_hash_table_remove_all(app->monitors);
    // Do this carefully, since desktop_destroy will remove each
    // desktop from the list as it's destroyed
    GList *ld = app->desktops;
    while (ld != NULL) {
        GtkWidget *desktop = GTK_WIDGET(ld->data);
        ld = ld->next;
        gtk_widget_destroy(desktop);
    }
    g_assert(app->desktops == NULL);

    if (app->screen != NULL) {
        g_signal_handlers_disconnect_by_data(app->screen, app);
        g_clear_object(&app->screen);
    }

    xfconf_shutdown();

#ifdef ENABLE_X11
    if (app->sm_client != NULL) {
        g_object_unref(app->sm_client);
    }
#endif

#ifdef HAVE_LIBNOTIFY
    xfdesktop_notify_uninit();
#endif

    G_APPLICATION_CLASS(xfdesktop_application_parent_class)->shutdown(g_application);
}

static gboolean
check_bool_option(GVariantDict *options, const gchar *name, gboolean default_value) {
    gboolean value = FALSE;
    if (g_variant_dict_lookup(options, name, "b", &value)) {
        return value;
    } else {
        return default_value;
    }
}

// If this function returns 0 or a postitive integer, it instructs GApplication
// to stop doing what it's doing, and exit with that status code.  Return -1 to
// have it continue with startup.
static gint
xfdesktop_application_handle_local_options(GApplication *g_application, GVariantDict *options) {
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(g_application);
    XfdesktopLocalArgs *args = app->args;

    TRACE("entering");

    /* Print the version info and exit */
    if (args->version) {
        g_print(_("This is %s version %s, running on Xfce %s.\n"), PACKAGE,
                VERSION_FULL, xfce_version_string());
        g_print(_("Built with GTK+ %d.%d.%d, linked with GTK+ %d.%d.%d."),
                GTK_MAJOR_VERSION,GTK_MINOR_VERSION, GTK_MICRO_VERSION,
                gtk_major_version, gtk_minor_version, gtk_micro_version);
        g_print("\n");
        g_print(_("Build options:\n"));
        g_print(_("    Desktop Menu:        %s\n"),
#ifdef ENABLE_DESKTOP_MENU
                _("enabled")
#else
                _("disabled")
#endif
                );
        g_print(_("    Desktop Icons:       %s\n"),
#ifdef ENABLE_DESKTOP_ICONS
                _("enabled")
#else
                _("disabled")
#endif
                );
        g_print(_("    Desktop File Icons:  %s\n"),
#ifdef ENABLE_FILE_ICONS
                _("enabled")
#else
                _("disabled")
#endif
                );

        return 0;
    }

    if (check_bool_option(options, OPTION_QUIT, FALSE) ||
        check_bool_option(options, OPTION_RELOAD, FALSE) ||
        check_bool_option(options, OPTION_NEXT, FALSE) ||
        check_bool_option(options, OPTION_MENU, FALSE) ||
        check_bool_option(options, OPTION_WINDOWLIST, FALSE) ||
        check_bool_option(options, OPTION_ARRANGE, FALSE))
    {
        app->args->has_remote_only_command = TRUE;
    }

    return G_APPLICATION_CLASS(xfdesktop_application_parent_class)->handle_local_options(g_application, options);
}

static gboolean
handle_option(GApplication *app, GVariantDict *options) {
    static const struct {
        const gchar *option_name;
        const gchar *action_name;
        gboolean has_arg;
        gboolean arg_value;
    } options_to_actions[] = {
        { OPTION_QUIT, ACTION_QUIT, FALSE, FALSE, },
        { OPTION_RELOAD, ACTION_RELOAD, FALSE, FALSE },
        { OPTION_NEXT, ACTION_NEXT, FALSE, FALSE, },
        { OPTION_MENU, ACTION_MENU, TRUE, TRUE },
        { OPTION_WINDOWLIST, ACTION_MENU, TRUE, FALSE },
        { OPTION_ARRANGE, ACTION_ARRANGE, FALSE, FALSE, },
        { OPTION_ENABLE_DEBUG, ACTION_DEBUG, TRUE, TRUE },
        { OPTION_DISABLE_DEBUG, ACTION_DEBUG, TRUE, FALSE },
    };

    for (gsize i = 0; i < G_N_ELEMENTS(options_to_actions); ++i) {
        if (check_bool_option(options, options_to_actions[i].option_name, FALSE)) {
            GVariant *arg = NULL;
            if (options_to_actions[i].has_arg) {
                arg = g_variant_new_boolean(options_to_actions[i].arg_value);
            }

            g_action_group_activate_action(G_ACTION_GROUP(app), options_to_actions[i].action_name, arg);

            return TRUE;
        }
    }

    return FALSE;
}

static gint
xfdesktop_application_command_line(GApplication *g_application,
                                   GApplicationCommandLine *command_line)
{
    GVariantDict *options = g_application_command_line_get_options_dict(command_line);

    TRACE("entering");

    if (!handle_option(g_application, options) && g_application_command_line_get_is_remote(command_line)) {
        g_application_command_line_printerr(command_line, PACKAGE " is already running\n");
        return 1;
    }

    return 0;
}

static gboolean
xfce_desktop_menu_destroy_idled(gpointer data)
{
    gtk_widget_destroy(GTK_WIDGET(data));
    return FALSE;
}

static void
xfce_desktop_menu_deactivated(GtkWidget *menu, XfdesktopApplication *app) {
    if (app->active_root_menu == GTK_MENU(menu)) {
        app->active_root_menu = NULL;
    }
    g_idle_add(xfce_desktop_menu_destroy_idled, menu);
}

static void
do_menu_popup(XfdesktopApplication *app,
              XfceDesktop *desktop,
              guint button,
              gint x,
              gint y,
              guint activate_time,
              gboolean populate_from_icon_view,
              PopulateMenuFunc populate_func)
{
    GdkScreen *screen;
    GtkMenu *menu = NULL;

    DBG("entering");

    if (app->active_root_menu != NULL) {
        gtk_menu_shell_deactivate(GTK_MENU_SHELL(app->active_root_menu));
        app->active_root_menu = NULL;
    }

    if (gtk_widget_has_screen(GTK_WIDGET(desktop))) {
        screen = gtk_widget_get_screen(GTK_WIDGET(desktop));
    } else {
        screen = gdk_display_get_default_screen(gdk_display_get_default());
    }

#ifdef ENABLE_DESKTOP_ICONS
    if (populate_from_icon_view && app->icon_view_manager != NULL) {
        menu = xfdesktop_icon_view_manager_get_context_menu(app->icon_view_manager, desktop, x, y);
    }
#endif

    menu = (*populate_func)(menu, gtk_widget_get_scale_factor(GTK_WIDGET(desktop)));

    if (menu != NULL) {
        gtk_menu_set_screen(menu, screen);
        gtk_menu_attach_to_widget(menu, GTK_WIDGET(desktop), NULL);
        /* if the toplevel is the garcon menu, it loads items asynchronously; calling _show() forces
         * loading to complete.  otherwise, the _get_children() call would return NULL */
        gtk_widget_show(GTK_WIDGET(menu));
        g_signal_connect(menu, "deactivate",
                         G_CALLBACK(xfce_desktop_menu_deactivated), app);

        /* Per gtk_menu_popup's documentation "for conflict-resolve initiation of
         * concurrent requests for mouse/keyboard grab requests." */
        if (activate_time == 0) {
            activate_time = gtk_get_current_event_time();
        }

        app->active_root_menu = menu;
        xfce_gtk_menu_popup_until_mapped(menu, NULL, NULL, NULL, NULL, button, activate_time);
    }
}


static void
popup_root_menu(XfdesktopApplication *app, XfceDesktop *desktop, guint button, gint x, gint y, guint activate_time) {
    DBG("entering");
    do_menu_popup(app, desktop, button, x, y, activate_time, TRUE, menu_populate);
}

static void
popup_secondary_root_menu(XfdesktopApplication *app,
                          XfceDesktop *desktop,
                          guint button,
                          gint x,
                          gint y,
                          guint activate_time)
{
    DBG("entering");
    do_menu_popup(app, desktop, button, x, y, activate_time, FALSE, windowlist_populate);
}

static gboolean
icon_view_active(XfdesktopApplication *app) {
#ifdef ENABLE_DESKTOP_ICONS
    return app->icon_view_manager != NULL;
#else
    return FALSE;
#endif
}

static void
update_active_desktop(XfdesktopApplication *app, XfceDesktop *desktop) {
    for (GList *l = app->desktops; l != NULL; l = l->next) {
        XfceDesktop *a_desktop = XFCE_DESKTOP(l->data);
        if (a_desktop != desktop) {
            xfce_desktop_set_is_active(a_desktop, FALSE);
        }
    }
    xfce_desktop_set_is_active(desktop, TRUE);
}

static gboolean
xfce_desktop_button_press_event(GtkWidget *w, GdkEventButton *evt, XfdesktopApplication *app) {
    guint button = evt->button;
    guint state = evt->state;
    XfceDesktop *desktop = XFCE_DESKTOP(w);

    DBG("entering");

    g_return_val_if_fail(XFCE_IS_DESKTOP(w), FALSE);

    update_active_desktop(app, desktop);

    if(evt->type == GDK_BUTTON_PRESS) {
        if(button == 3 || (button == 1 && (state & GDK_SHIFT_MASK))) {
            /* no icons on the desktop, grab the focus and pop up the menu */
            if (!icon_view_active(app) && !gtk_widget_has_grab(w)) {
                gtk_grab_add(w);
            }

            popup_root_menu(app, desktop, button, evt->x, evt->y, evt->time);
            return TRUE;
        } else if(button == 2 || (button == 1 && (state & GDK_SHIFT_MASK)
                                  && (state & GDK_CONTROL_MASK)))
        {
            /* always grab the focus and pop up the menu */
            if (!icon_view_active(app) && !gtk_widget_has_grab(w)) {
                gtk_grab_add(w);
            }

            popup_secondary_root_menu(app, desktop, button, evt->x, evt->y, evt->time);
            return TRUE;
        }
    }

    return FALSE;
}

static gboolean
xfce_desktop_button_release_event(GtkWidget *w, GdkEventButton *evt, XfdesktopApplication *app) {
    DBG("entering");

    gtk_grab_remove(w);

    return FALSE;
}

/* This function gets called when the user presses the menu key on the keyboard.
 * Or Shift+F10 or whatever key binding the user has chosen. */
static gboolean
xfce_desktop_popup_menu(GtkWidget *w, XfdesktopApplication *app) {
    GdkEvent *evt;
    gint x, y;
    guint button, etime;

    DBG("entering");

    XfceDesktop *desktop = XFCE_DESKTOP(w);
    update_active_desktop(app, desktop);

    evt = gtk_get_current_event();
    if(evt != NULL && (GDK_BUTTON_PRESS == evt->type || GDK_BUTTON_RELEASE == evt->type)) {
        button = evt->button.button;
        x = evt->button.x;
        y = evt->button.y;
        etime = evt->button.time;
    } else {
        button = 0;
        x = -1;
        y = -1;
        etime = gtk_get_current_event_time();
    }

    popup_root_menu(app, desktop, button, x, y, etime);

    gdk_event_free((GdkEvent*)evt);
    return TRUE;
}

static gboolean
xfce_desktop_delete_event(GtkWidget *w, GdkEventAny *evt, XfdesktopApplication *app) {
    session_logout(app);
    return TRUE;
}

static void
xfdesktop_application_set_icon_style(XfdesktopApplication *app, XfceDesktopIconStyle style) {
    g_return_if_fail(style <= XFCE_DESKTOP_ICON_STYLE_FILES);

#ifdef ENABLE_DESKTOP_ICONS
    if (style == app->icon_style) {
        return;
    }

    app->icon_style = style;

    // FIXME: probably should ensure manager actually got freed and any icon view
    // instances are no longer present as children
    g_clear_object(&app->icon_view_manager);

    switch (app->icon_style) {
        case XFCE_DESKTOP_ICON_STYLE_NONE:
            /* nada */
            break;

        case XFCE_DESKTOP_ICON_STYLE_WINDOWS:
            app->icon_view_manager = xfdesktop_window_icon_manager_new(app->screen,
                                                                       app->channel,
                                                                       app->accel_group,
                                                                       app->backdrop_manager,
                                                                       app->desktops);
            break;

#ifdef ENABLE_FILE_ICONS
        case XFCE_DESKTOP_ICON_STYLE_FILES: {
            const gchar *desktop_path = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
            GFile *file = g_file_new_for_path(desktop_path);
            app->icon_view_manager = xfdesktop_file_icon_manager_new(app->screen,
                                                                     app->gdkscreen,
                                                                     app->channel,
                                                                     app->accel_group,
                                                                     app->backdrop_manager,
                                                                     app->desktops,
                                                                     file);
            g_object_unref(file);
            break;
        }
#endif

        default:
            g_critical("Unusable XfceDesktopIconStyle: %d.  Unable to " \
                       "display desktop icons.",
                       app->icon_style);
            break;
    }

    for (GList *l = app->desktops; l != NULL; l = l->next) {
        gtk_widget_queue_draw(GTK_WIDGET(l->data));
    }
#endif
}
