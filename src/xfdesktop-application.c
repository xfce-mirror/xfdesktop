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

#include "xfdesktop-common.h"
#include "xfce-backdrop.h"
#include "xfce-desktop.h"
#include "menu.h"
#include "windowlist.h"

#ifdef ENABLE_X11
#include "xfdesktop-x11.h"
#endif

#ifdef HAVE_LIBNOTIFY
#include "xfdesktop-notify.h"
#endif

#include "xfdesktop-application.h"

static void xfdesktop_application_finalize(GObject *object);

static void session_logout(void);
static void session_die(gpointer user_data);

static gboolean reload_idle_cb(gpointer data);
static void cb_xfdesktop_application_reload(GAction  *action,
                                            GVariant *parameter,
                                            gpointer  data);

static void cb_xfdesktop_application_next(GAction  *action,
                                          GVariant *parameter,
                                          gpointer  data);

static void xfdesktop_handle_quit_signals(gint sig, gpointer user_data);
static void cb_xfdesktop_application_quit(GAction  *action,
                                          GVariant *parameter,
                                          gpointer  data);

static void cb_xfdesktop_application_menu(GAction  *action,
                                          GVariant *parameter,
                                          gpointer  data);

static void cb_xfdesktop_application_arrange(GAction  *action,
                                             GVariant *parameter,
                                             gpointer  data);

static void cb_xfdesktop_application_debug(GAction  *action,
                                           GVariant *parameter,
                                           gpointer  data);

static void xfdesktop_application_startup(GApplication *g_application);
static void xfdesktop_application_start(XfdesktopApplication *app);
static void xfdesktop_application_shutdown(GApplication *g_application);

static gint xfdesktop_application_handle_local_options(GApplication *g_application,
                                                       GVariantDict *options);
static gint xfdesktop_application_command_line(GApplication *g_application,
                                               GApplicationCommandLine *command_line);

typedef struct {
    gboolean version;
    gboolean enable_debug;
    gboolean disable_debug;

    gboolean has_remote_command;
} XfdesktopLocalArgs;

struct _XfdesktopApplication
{
    GtkApplication parent;

    GtkWidget *desktop;
    XfconfChannel *channel;

    XfceSMClient *sm_client;
    GCancellable *cancel;

    XfdesktopLocalArgs *args;

#ifdef ENABLE_X11
    gboolean disable_wm_check;
    WaitForWM *wfwm;

    GdkWindow *selection_window;
#endif
};

struct _XfdesktopApplicationClass
{
    GtkApplicationClass parent;
};

const gchar *fallback_CSS =
"XfdesktopIconView.view {"
"	background: transparent;"
"}"
"XfdesktopIconView.view.label {"
"	background: @theme_bg_color;"
"	border-radius: 3px;"
"	text-shadow: 1px 1px 2px black;"
"}"
"XfdesktopIconView.view.label:selected:backdrop {"
"	background: alpha(@theme_selected_bg_color, 0.5);"
"	color: @theme_selected_fg_color;"
"	text-shadow: 0 1px 1px black;"
"}"
"XfdesktopIconView.view.label:selected {"
"	background: @theme_selected_bg_color;"
"	color: @theme_selected_fg_color;"
"}"
"XfdesktopIconView.rubberband {"
"	background: alpha(@theme_selected_bg_color, 0.2);"
"	border: 1px solid @theme_selected_bg_color;"
"	border-radius: 0;"
"}"
"XfdesktopIconView:active {"
"	color: @theme_selected_bg_color;"
"}";


G_DEFINE_TYPE(XfdesktopApplication, xfdesktop_application, GTK_TYPE_APPLICATION)


static void
xfdesktop_application_class_init(XfdesktopApplicationClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GApplicationClass *gapplication_class = G_APPLICATION_CLASS(klass);

    gobject_class->finalize = xfdesktop_application_finalize;

    gapplication_class->startup = xfdesktop_application_startup;
    gapplication_class->shutdown = xfdesktop_application_shutdown;
    gapplication_class->handle_local_options = xfdesktop_application_handle_local_options;
    gapplication_class->command_line = xfdesktop_application_command_line;
}

static void
xfdesktop_application_add_action(XfdesktopApplication *app, GAction *action)
{
    g_action_map_add_action(G_ACTION_MAP(app), action);
}

static void
xfdesktop_application_init(XfdesktopApplication *app)
{
    XfdesktopLocalArgs *args = g_new0(XfdesktopLocalArgs, 1);
    const GOptionEntry main_entries[] = {
        { "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &args->version, N_("Display version information"), NULL },
        { "reload", 'R', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, NULL, N_("Reload all settings"), NULL },
        { "next", 'N', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, NULL, N_("Advance to the next wallpaper on the current workspace"), NULL },
        { "menu", 'M', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, NULL, N_("Pop up the menu (at the current mouse position)"), NULL },
        { "windowlist", 'W', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, NULL, N_("Pop up the window list (at the current mouse position)"), NULL },
#ifdef ENABLE_FILE_ICONS
        { "arrange", 'A', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, NULL, N_("Automatically arrange all the icons on the desktop"), NULL },
#endif
        { "enable-debug", 'e', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &args->enable_debug, N_("Enable debug messages"), NULL },
        { "disable-debug", 'd', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &args->disable_debug, N_("Disable debug messages"), NULL },
#ifdef ENABLE_X11
        { "disable-wm-check", 'D', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &app->disable_wm_check, N_("Do not wait for a window manager on startup"), NULL },
#endif
        { "quit", 'Q', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, NULL, N_("Cause xfdesktop to quit"), NULL },
        G_OPTION_ENTRY_NULL
    };
    GSimpleAction *action;

    app->args = args;
    app->cancel = g_cancellable_new();

    g_application_add_main_option_entries(G_APPLICATION(app), main_entries);

    /* reload action */
    action = g_simple_action_new("reload", NULL);
    g_signal_connect(action, "activate", G_CALLBACK(cb_xfdesktop_application_reload), app);
    xfdesktop_application_add_action(app, G_ACTION(action));
    g_object_unref(action);

    /* next action */
    action = g_simple_action_new("next", NULL);
    g_signal_connect(action, "activate", G_CALLBACK(cb_xfdesktop_application_next), app);
    xfdesktop_application_add_action(app, G_ACTION(action));
    g_object_unref(action);

    /* quit action */
    action = g_simple_action_new("quit", NULL);
    g_signal_connect(action, "activate", G_CALLBACK(cb_xfdesktop_application_quit), app);
    xfdesktop_application_add_action(app, G_ACTION(action));
    g_object_unref(action);

    /* menu action, parameter pops up primary (TRUE) or windowlist menu */
    action = g_simple_action_new("menu", G_VARIANT_TYPE_BOOLEAN);
    g_signal_connect(action, "activate", G_CALLBACK(cb_xfdesktop_application_menu), app);
    xfdesktop_application_add_action(app, G_ACTION(action));
    g_object_unref(action);

    /* arrange action */
    action = g_simple_action_new("arrange", NULL);
    g_signal_connect(action, "activate", G_CALLBACK(cb_xfdesktop_application_arrange), app);
    xfdesktop_application_add_action(app, G_ACTION(action));
    g_object_unref(action);

    /* debug action, parameter toggles debug state */
    action = g_simple_action_new("debug", G_VARIANT_TYPE_BOOLEAN);
    g_signal_connect(action, "activate", G_CALLBACK(cb_xfdesktop_application_debug), app);
    xfdesktop_application_add_action(app, G_ACTION(action));
    g_object_unref(action);
}

static void
xfdesktop_application_finalize(GObject *object)
{
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(object);

    g_free(app->args);

#ifdef ENABLE_X11
    xfdesktop_x11_wait_for_wm_destroy(app->wfwm);
    if (app->selection_window != NULL) {
        gdk_window_destroy(app->selection_window);
    }

    if (xfw_windowing_get() == XFW_WINDOWING_X11) {
        xfdesktop_x11_set_compat_properties(NULL);
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
    } else {
        g_object_ref(app);
    }

    return app;
}

static void
session_logout(void)
{
    XfceSMClient *sm_client;

    sm_client = xfce_sm_client_get();
    xfce_sm_client_request_shutdown(sm_client, XFCE_SM_CLIENT_SHUTDOWN_HINT_ASK);
}

static void
session_die(gpointer user_data)
{
    gint main_level;
    XfdesktopApplication *app;

    TRACE("entering");

    /* Ensure we always have a valid reference so we can quit xfdesktop */
    app = xfdesktop_application_get();

    /* Cancel the wait for wm check if it's still running */
    g_cancellable_cancel(app->cancel);

    for(main_level = gtk_main_level(); main_level > 0; --main_level)
        gtk_main_quit();

    g_application_quit(G_APPLICATION(app));
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

    /* If xfdesktop never started there's nothing to reload, xfdesktop will
     * now startup */
    if(!app->desktop)
        return FALSE;

    /* reload all the desktop */
    if(app->desktop)
        xfce_desktop_refresh(XFCE_DESKTOP(app->desktop), FALSE, TRUE);

    g_application_release(G_APPLICATION(app));

    return FALSE;
}

static void
cb_xfdesktop_application_reload(GAction  *action,
                                GVariant *parameter,
                                gpointer  data)
{
    GApplication *g_application;

    if(!data || !G_IS_APPLICATION(data))
        return;

    g_application = G_APPLICATION(data);

    /* hold the app so it doesn't quit while a queue up a refresh */
    g_application_hold(g_application);
    g_idle_add(reload_idle_cb, g_application);
}

static void
cb_xfdesktop_application_next(GAction  *action,
                              GVariant *parameter,
                              gpointer  data)
{
    XfdesktopApplication *app;

    TRACE("entering");

    g_return_if_fail(XFDESKTOP_IS_APPLICATION(data));

    app = XFDESKTOP_APPLICATION(data);

    /* If xfdesktop never started there's nothing to do here */
    if(!app->desktop)
        return;

    /* reload the desktop forcing the wallpaper to advance */
    if(app->desktop)
        xfce_desktop_refresh(XFCE_DESKTOP(app->desktop), TRUE, TRUE);
}

static void
xfdesktop_handle_quit_signals(gint sig,
                              gpointer user_data)
{
    TRACE("entering");

    g_return_if_fail(XFDESKTOP_IS_APPLICATION(user_data));

    session_die(user_data);
}

static void
cb_xfdesktop_application_quit(GAction  *action,
                              GVariant *parameter,
                              gpointer  data)
{
    XfdesktopApplication *app;

    TRACE("entering");

    g_return_if_fail(XFDESKTOP_IS_APPLICATION(data));

    app = XFDESKTOP_APPLICATION(data);

    /* If the user told xfdesktop to quit, set the restart style to something
     * where it won't restart itself */
    if(app->sm_client && XFCE_IS_SM_CLIENT(app->sm_client)) {
        xfce_sm_client_set_restart_style(app->sm_client,
                                         XFCE_SM_CLIENT_RESTART_NORMAL);
    }

    session_die(app);
}


/* parameter is a boolean that determines whether to popup the primay or
 * windowlist menu */
static void
cb_xfdesktop_application_menu(GAction  *action,
                              GVariant *parameter,
                              gpointer  data)
{
    XfdesktopApplication *app;
    gboolean popup_root_menu;

    TRACE("entering");

    /* sanity checks */
    if(!data || !XFDESKTOP_IS_APPLICATION(data))
        return;

    if(!g_variant_is_of_type(parameter, G_VARIANT_TYPE_BOOLEAN))
        return;

    popup_root_menu = g_variant_get_boolean(parameter);
    app = XFDESKTOP_APPLICATION(data);

    if(popup_root_menu) {
        xfce_desktop_popup_root_menu(XFCE_DESKTOP(app->desktop),
                                     0, GDK_CURRENT_TIME);
    } else {
        xfce_desktop_popup_secondary_root_menu(XFCE_DESKTOP(app->desktop),
                                               0, GDK_CURRENT_TIME);
    }
}

static void
cb_xfdesktop_application_arrange(GAction  *action,
                                 GVariant *parameter,
                                 gpointer  data)
{
    XfdesktopApplication *app;

    TRACE("entering");

    /* sanity check */
    if(!data || !XFDESKTOP_IS_APPLICATION(data))
        return;

    app = XFDESKTOP_APPLICATION(data);

    xfce_desktop_arrange_icons(XFCE_DESKTOP(app->desktop));
}

/* parameter is a boolean that determines whether to enable or disable the
 * debug messages */
static void
cb_xfdesktop_application_debug(GAction  *action,
                               GVariant *parameter,
                               gpointer  data)
{
    TRACE("entering");

    /* sanity checks */
    if(!data || !XFDESKTOP_IS_APPLICATION(data))
        return;

    if(!g_variant_is_of_type(parameter, G_VARIANT_TYPE_BOOLEAN))
        return;

    /* Toggle the debug state */
    xfdesktop_debug_set(g_variant_get_boolean(parameter));
}

static void
xfdesktop_application_startup(GApplication *g_application)
{
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(g_application);

    TRACE("entering");

    if (app->args->has_remote_command) {
        g_printerr(PACKAGE " is not running\n");
        exit(1);
    }

    g_clear_pointer(&app->args, g_free);

    G_APPLICATION_CLASS(xfdesktop_application_parent_class)->startup(g_application);

#ifdef ENABLE_X11
    if(!app->disable_wm_check && xfw_windowing_get() == XFW_WINDOWING_X11) {
        app->wfwm = xfdesktop_x11_wait_for_wm(g_application,
                                              app->cancel,
                                              (WMFoundCallback)xfdesktop_application_start);
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
}

static void
desktop_destroyed(XfceDesktop *desktop, XfdesktopApplication *app) {
    gtk_application_remove_window(GTK_APPLICATION(app), GTK_WINDOW(desktop));
    app->desktop = NULL;
}

static void
xfdesktop_application_start(XfdesktopApplication *app)
{
    GtkSettings *settings;
    GdkDisplay *gdpy;
    GdkScreen *gscreen;
    gint screen_num;
    GError *error = NULL;
    gchar buf[1024];

    TRACE("entering");

    g_return_if_fail(app != NULL);

    settings = gtk_settings_get_default();
    g_signal_connect (settings, "notify::gtk-theme-name", G_CALLBACK (xfdesktop_application_theme_changed), NULL);
    xfdesktop_application_theme_changed (settings, app);

#ifdef ENABLE_X11
    xfdesktop_x11_wait_for_wm_destroy(app->wfwm);
    app->wfwm = NULL;
#endif

    gdpy = gdk_display_get_default();
    gscreen = gdk_display_get_default_screen(gdpy);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    screen_num = gdk_screen_get_number(gscreen);
G_GNUC_END_IGNORE_DEPRECATIONS

#ifdef ENABLE_X11
    if (xfw_windowing_get() == XFW_WINDOWING_X11) {
        app->selection_window = xfdesktop_x11_set_desktop_manager_selection(gscreen, &error);
        if (app->selection_window == NULL) {
            g_error("%s", error->message);
            g_error_free(error);
            exit(1);
        }
    }
#endif

    /* setup the session management options */
    app->sm_client = xfce_sm_client_get();
    g_object_add_weak_pointer(G_OBJECT(app->sm_client), (gpointer *)&app->sm_client);
    xfce_sm_client_set_restart_style(app->sm_client, XFCE_SM_CLIENT_RESTART_IMMEDIATELY);
    xfce_sm_client_set_priority(app->sm_client, XFCE_SM_CLIENT_PRIORITY_DESKTOP);
    g_signal_connect(app->sm_client, "quit", G_CALLBACK(session_die), app);

    if(!xfce_sm_client_connect(app->sm_client, &error) && error) {
        g_printerr("Failed to connect to session manager: %s\n", error->message);
        g_clear_error(&error);
    }

    if(!xfconf_init(&error)) {
        g_warning("%s: unable to connect to settings daemon: %s.  Defaults will be used",
                  PACKAGE, error->message);
        g_clear_error(&error);
        error = NULL;
    } else {
        app->channel = xfconf_channel_get(XFDESKTOP_CHANNEL);
        g_object_add_weak_pointer(G_OBJECT(app->channel), (gpointer *)&app->channel);
    }

    g_snprintf(buf, sizeof(buf), "/backdrop/screen%d/", screen_num);
    app->desktop = xfce_desktop_new(gscreen, app->channel, buf);
    gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(app->desktop));
    g_signal_connect(app->desktop, "destroy",
                     G_CALLBACK(desktop_destroyed), app);

    gtk_widget_add_events(app->desktop, GDK_BUTTON_PRESS_MASK
                          | GDK_BUTTON_RELEASE_MASK);
    if (xfw_windowing_get() == XFW_WINDOWING_X11) {
        /* hook into the scroll event so we can forward it to the window
         * manager */
        gtk_widget_add_events(app->desktop, GDK_SCROLL_MASK);
        g_signal_connect(G_OBJECT(app->desktop), "scroll-event",
                         G_CALLBACK(scroll_cb), app);
    } else if (xfw_windowing_get() == XFW_WINDOWING_WAYLAND) {
#ifdef ENABLE_WAYLAND
        GtkWindow *window = GTK_WINDOW(app->desktop);

        if (!gtk_layer_is_supported()) {
            g_critical("Your compositor must support the zwlr_layer_shell_v1 protocol");
            exit(1);
        }

        gtk_layer_init_for_window(window);
        gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_BACKGROUND);
        gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
        gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
        gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_TOP, 0);
        gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_LEFT, 0);
        gtk_layer_set_namespace(window, "desktop");
#else  /* !ENABLE_WAYLAND */
        g_critical("xfdesktop was not built with Wayland support");
        exit(1);
#endif  /* ENABLE_WAYLAND */
    }

    /* display the desktop and try to put it at the bottom */
    gtk_widget_realize(app->desktop);
#ifdef ENABLE_X11
    if (xfw_windowing_get() == XFW_WINDOWING_X11) {
        xfdesktop_x11_set_compat_properties(app->desktop);
    }
#endif  /* ENABLE_X11 */
    gdk_window_lower(gtk_widget_get_window(app->desktop));
    gtk_widget_show_all(app->desktop);

    xfce_desktop_set_session_logout_func(XFCE_DESKTOP(app->desktop),
                                         session_logout);


    menu_init(app->channel);
    windowlist_init(app->channel);

    /* hook up to the different quit signals */
    if(xfce_posix_signal_handler_init(&error)) {
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

gint
xfdesktop_application_run(XfdesktopApplication *app, int argc, char **argv)
{
    return g_application_run(G_APPLICATION(app), argc, argv);
}

static void
xfdesktop_application_shutdown(GApplication *g_application)
{
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(g_application);

    TRACE("entering");

#ifdef ENABLE_X11
    xfdesktop_x11_wait_for_wm_destroy(app->wfwm);
    app->wfwm = NULL;
#endif

    if (app->channel != NULL) {
        menu_cleanup(app->channel);
        windowlist_cleanup(app->channel);
        app->channel = NULL;
    }

    if (app->desktop != NULL) {
        gtk_widget_destroy(app->desktop);
    }

    xfconf_shutdown();

    if (app->sm_client != NULL) {
        g_object_unref(app->sm_client);
    }

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
                VERSION, xfce_version_string());
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
    } else if (args->enable_debug) {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "debug",
                                       g_variant_new_boolean(TRUE));
    } else if (args->disable_debug) {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "debug",
                                       g_variant_new_boolean(FALSE));
    }

    if (check_bool_option(options, "quit", FALSE)
        ||check_bool_option(options, "reload", FALSE) ||
        check_bool_option(options, "next", FALSE) ||
        check_bool_option(options, "menu", FALSE) ||
        check_bool_option(options, "windowlist", FALSE) ||
        check_bool_option(options, "arrange", FALSE))
    {
        app->args->has_remote_command = TRUE;
    }

    return G_APPLICATION_CLASS(xfdesktop_application_parent_class)->handle_local_options(g_application, options);
}

static gint
xfdesktop_application_command_line(GApplication *g_application,
                                   GApplicationCommandLine *command_line)
{
    GVariantDict *options = g_application_command_line_get_options_dict(command_line);

    TRACE("entering");

    /* handle our defined remote options */
    if (check_bool_option(options, "quit", FALSE)) {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "quit", NULL);
    } else if (check_bool_option(options, "reload", FALSE)) {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "reload", NULL);
    } else if (check_bool_option(options, "next", FALSE)) {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "next", NULL);
    } else if (check_bool_option(options, "menu", FALSE)) {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "menu",
                                       g_variant_new_boolean(TRUE));
    } else if (check_bool_option(options, "windowlist", FALSE)) {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "menu",
                                       g_variant_new_boolean(FALSE));
    } else if (check_bool_option(options, "arrange", FALSE)) {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "arrange", NULL);
    } else if (g_application_command_line_get_is_remote(command_line)) {
        g_application_command_line_printerr(command_line, PACKAGE " is already running\n");
        return 1;
    }

    return 0;
}
