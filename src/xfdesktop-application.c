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

static void xfdesktop_application_finalize(GObject *object);

static void session_logout(void);
static void session_die(gpointer user_data);

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

#ifdef ENABLE_X11
static void cancel_wait_for_wm(XfdesktopApplication *app);
#endif

typedef struct {
    gboolean version;
    gboolean has_remote_only_command;
} XfdesktopLocalArgs;

struct _XfdesktopApplication
{
    GtkApplication parent;

    GtkWidget *desktop;
    XfconfChannel *channel;

    XfceSMClient *sm_client;

    XfdesktopLocalArgs *args;

#ifdef ENABLE_X11
    GCancellable *cancel_wait_for_wm;
    gboolean disable_wm_check;

    GdkWindow *selection_window;
#endif
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
"	background-color: shade(@theme_selected_bg_color, 0.5);"
"	border-radius: 3px;"
"	color: @theme_selected_fg_color;"
"	text-shadow: 0 1px 1px black;"
"}"
"XfdesktopIconView.view.label:backdrop {"
"	background-color: alpha(shade(@theme_selected_bg_color, 0.5), 0.5);"
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

    gobject_class->finalize = xfdesktop_application_finalize;

    gapplication_class->startup = xfdesktop_application_startup;
    gapplication_class->shutdown = xfdesktop_application_shutdown;
    gapplication_class->handle_local_options = xfdesktop_application_handle_local_options;
    gapplication_class->command_line = xfdesktop_application_command_line;
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

    g_application_add_main_option_entries(G_APPLICATION(app), main_entries);

    for (gsize i = 0; i < G_N_ELEMENTS(actions); ++i) {
        GSimpleAction *action = g_simple_action_new(actions[i].name, actions[i].arg_type);
        g_signal_connect(action, "activate", G_CALLBACK(xfdesktop_application_action_activated), app);
        g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(action));
        g_object_unref(action);
    }
}

static void
xfdesktop_application_finalize(GObject *object)
{
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(object);

    g_free(app->args);

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

#ifdef ENABLE_X11
    cancel_wait_for_wm(app);
#endif

    for(main_level = gtk_main_level(); main_level > 0; --main_level)
        gtk_main_quit();

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

    if (app->desktop != NULL) {
        xfce_desktop_refresh(XFCE_DESKTOP(app->desktop), FALSE, TRUE);
    }
    g_application_release(G_APPLICATION(app));

    return FALSE;
}

static void
xfdesktop_application_action_activated(GAction *action, GVariant *parameter, gpointer data) {
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(data);
    const gchar *name = g_action_get_name(action);

    TRACE("entering: %s", name);

    if (g_strcmp0(name, ACTION_RELOAD) == 0) {
        if (app->desktop != NULL) {
            /* hold the app so it doesn't quit while we queue up a refresh */
            g_application_hold(G_APPLICATION(app));
            g_idle_add(reload_idle_cb, app);
        }
    } else if (g_strcmp0(name, ACTION_NEXT) == 0) {
        if (app->desktop != NULL) {
            xfce_desktop_refresh(XFCE_DESKTOP(app->desktop), TRUE, TRUE);
        }
    } else if (g_strcmp0(name, ACTION_MENU) == 0) {
        if (app->desktop != NULL && g_variant_is_of_type(parameter, G_VARIANT_TYPE_BOOLEAN)) {
            if (g_variant_get_boolean(parameter)) {
                xfce_desktop_popup_root_menu(XFCE_DESKTOP(app->desktop),
                                             0, GDK_CURRENT_TIME);
            } else {
                xfce_desktop_popup_secondary_root_menu(XFCE_DESKTOP(app->desktop),
                                                       0, GDK_CURRENT_TIME);
            }
        }
    } else if (g_strcmp0(name, ACTION_ARRANGE) == 0) {
        if (app->desktop != NULL) {
            xfce_desktop_arrange_icons(XFCE_DESKTOP(app->desktop));
        }
    } else if (g_strcmp0(name, ACTION_QUIT) == 0) {
        /* If the user told xfdesktop to quit, set the restart style to something
         * where it won't restart itself */
        if (app->sm_client && XFCE_IS_SM_CLIENT(app->sm_client)) {
            xfce_sm_client_set_restart_style(app->sm_client, XFCE_SM_CLIENT_RESTART_NORMAL);
        }

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
xfdesktop_handle_quit_signals(gint sig,
                              gpointer user_data)
{
    TRACE("entering");

    g_return_if_fail(XFDESKTOP_IS_APPLICATION(user_data));

    session_die(user_data);
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

static void
xfdesktop_application_shutdown(GApplication *g_application)
{
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(g_application);

    TRACE("entering");

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
