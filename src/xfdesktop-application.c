/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006      Brian Tarricone, <bjt23@cornell.edu>
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif


#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <xfconf/xfconf.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>

#include "xfdesktop-common.h"
#include "xfce-backdrop.h"
#include "xfce-desktop.h"
#include "menu.h"
#include "windowlist.h"

#ifdef HAVE_LIBNOTIFY
#include "xfdesktop-notify.h"
#endif

#include "xfdesktop-application.h"

static void xfdesktop_application_finalize(GObject *object);

static void session_logout(void);
static void session_die(gpointer user_data);

static void event_forward_to_rootwin(GdkScreen *gscreen, GdkEvent *event);

static gboolean scroll_cb(GtkWidget *w, GdkEventScroll *evt, gpointer user_data);

static gboolean reload_idle_cb(gpointer data);
static void cb_xfdesktop_application_reload(GAction  *action,
                                            GVariant *parameter,
                                            gpointer  data);

static void xfdesktop_handle_quit_signals(gint sig, gpointer user_data);
static void cb_xfdesktop_application_quit(GAction  *action,
                                          GVariant *parameter,
                                          gpointer  data);

static gint xfdesktop_application_get_current_screen_number(XfdesktopApplication *app);

static void cb_xfdesktop_application_menu(GAction  *action,
                                          GVariant *parameter,
                                          gpointer  data);

static void cb_xfdesktop_application_arrange(GAction  *action,
                                             GVariant *parameter,
                                             gpointer  data);

static void xfdesktop_application_startup(GApplication *g_application);
static void xfdesktop_application_start(XfdesktopApplication *app);
static void xfdesktop_application_shutdown(GApplication *g_application);

static gboolean xfdesktop_application_local_command_line(GApplication *g_application,
                                                         gchar ***arguments,
                                                         int *exit_status);
static gint xfdesktop_application_command_line(GApplication *g_application,
                                               GApplicationCommandLine *command_line);

struct _XfdesktopApplication
{
    GApplication parent;

    GtkWidget **desktops;
    XfconfChannel *channel;
    gint nscreens;
    XfceSMClient *sm_client;
};

struct _XfdesktopApplicationClass
{
    GApplicationClass parent;
};



G_DEFINE_TYPE(XfdesktopApplication, xfdesktop_application, G_TYPE_APPLICATION)


static void
xfdesktop_application_class_init(XfdesktopApplicationClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GApplicationClass *gapplication_class = G_APPLICATION_CLASS(klass);

    gobject_class->finalize = xfdesktop_application_finalize;

    gapplication_class->startup = xfdesktop_application_startup;
    gapplication_class->shutdown = xfdesktop_application_shutdown;
    gapplication_class->local_command_line = xfdesktop_application_local_command_line;
    gapplication_class->command_line = xfdesktop_application_command_line;
}

static void
xfdesktop_application_init(XfdesktopApplication *app)
{
    GSimpleAction *action;

    /* reload action */
    action = g_simple_action_new("reload", NULL);
    g_signal_connect(action, "activate", G_CALLBACK(cb_xfdesktop_application_reload), app);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(action));
    g_object_unref(action);

    /* quit action */
    action = g_simple_action_new("quit", NULL);
    g_signal_connect(action, "activate", G_CALLBACK(cb_xfdesktop_application_quit), app);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(action));
    g_object_unref(action);

    /* menu action, parameter pops up primary (TRUE) or windowlist menu */
    action = g_simple_action_new("menu", G_VARIANT_TYPE_BOOLEAN);
    g_signal_connect(action, "activate", G_CALLBACK(cb_xfdesktop_application_menu), app);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(action));
    g_object_unref(action);

    /* arrange action */
    action = g_simple_action_new("arrange", NULL);
    g_signal_connect(action, "activate", G_CALLBACK(cb_xfdesktop_application_arrange), app);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(action));
    g_object_unref(action);
}

static void
xfdesktop_application_finalize(GObject *object)
{
    G_OBJECT_CLASS(xfdesktop_application_parent_class)->finalize(object);
}

XfdesktopApplication *
xfdesktop_application_get(void)
{
    static XfdesktopApplication *app = NULL;

    if(app) {
        g_object_ref(G_OBJECT(app));
    } else {
      app = g_object_new(XFDESKTOP_TYPE_APPLICATION,
                         "application-id", "org.xfce.xfdesktop",
                         "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
                         NULL);
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
    gtk_main_quit();
    g_application_quit(G_APPLICATION(user_data));
}

static void
event_forward_to_rootwin(GdkScreen *gscreen, GdkEvent *event)
{
    XButtonEvent xev, xev2;
    Display *dpy = GDK_DISPLAY_XDISPLAY(gdk_screen_get_display(gscreen));

    if(event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE) {
        if(event->type == GDK_BUTTON_PRESS) {
            xev.type = ButtonPress;
            /*
             * rox has an option to disable the next
             * instruction. it is called "blackbox_hack". Does
             * anyone know why exactly it is needed?
             */
            XUngrabPointer(dpy, event->button.time);
        } else
            xev.type = ButtonRelease;

        xev.button = event->button.button;
        xev.x = event->button.x;    /* Needed for icewm */
        xev.y = event->button.y;
        xev.x_root = event->button.x_root;
        xev.y_root = event->button.y_root;
        xev.state = event->button.state;

        xev2.type = 0;
    } else if(event->type == GDK_SCROLL) {
        xev.type = ButtonPress;
        xev.button = event->scroll.direction + 4;
        xev.x = event->scroll.x;    /* Needed for icewm */
        xev.y = event->scroll.y;
        xev.x_root = event->scroll.x_root;
        xev.y_root = event->scroll.y_root;
        xev.state = event->scroll.state;

        xev2.type = ButtonRelease;
        xev2.button = xev.button;
    } else
        return;
    xev.window = GDK_WINDOW_XWINDOW(gdk_screen_get_root_window(gscreen));
    xev.root =  xev.window;
    xev.subwindow = None;
    xev.time = event->button.time;
    xev.same_screen = True;

    XSendEvent(dpy, xev.window, False, ButtonPressMask | ButtonReleaseMask,
            (XEvent *)&xev);
    if(xev2.type == 0)
        return;

    /* send button release for scroll event */
    xev2.window = xev.window;
    xev2.root = xev.root;
    xev2.subwindow = xev.subwindow;
    xev2.time = xev.time;
    xev2.x = xev.x;
    xev2.y = xev.y;
    xev2.x_root = xev.x_root;
    xev2.y_root = xev.y_root;
    xev2.state = xev.state;
    xev2.same_screen = xev.same_screen;

    XSendEvent(dpy, xev2.window, False, ButtonPressMask | ButtonReleaseMask,
            (XEvent *)&xev2);
}

static gboolean
scroll_cb(GtkWidget *w, GdkEventScroll *evt, gpointer user_data)
{
    event_forward_to_rootwin(gtk_widget_get_screen(w), (GdkEvent*)evt);
    return TRUE;
}

static gboolean
reload_idle_cb(gpointer data)
{
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(data);
    gint i;

    TRACE("entering");

    for(i = 0; i < app->nscreens; ++i) {
        if(app->desktops[i])
            xfce_desktop_refresh(XFCE_DESKTOP(app->desktops[i]));
    }

    menu_reload();

    g_application_release(G_APPLICATION(app));

    return FALSE;
}

static void
cb_xfdesktop_application_reload(GAction  *action,
                                GVariant *parameter,
                                gpointer  data)
{
    GApplication *g_application = G_APPLICATION(data);
    g_application_hold(g_application);
    g_idle_add((GSourceFunc)reload_idle_cb, g_application);
}

static void
xfdesktop_handle_quit_signals(gint sig,
                              gpointer user_data)
{
    gint main_level;

    for(main_level = gtk_main_level(); main_level > 0; --main_level)
        gtk_main_quit();

    g_application_quit(G_APPLICATION(user_data));
}

static void
cb_xfdesktop_application_quit(GAction  *action,
                              GVariant *parameter,
                              gpointer  data)
{
    xfdesktop_handle_quit_signals(0, data);
}

static gint
xfdesktop_application_get_current_screen_number(XfdesktopApplication *app)
{
    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen;
    gint screen_num;

    gdk_display_get_pointer(display, &screen, NULL, NULL, NULL);

    screen_num = gdk_screen_get_number(screen);

    if(screen_num >= app->nscreens) {
        g_printerr("screen_num >= app->nscreens");
        return -1;
    }

    return screen_num;
}

/* parameter is a boolean that determines whether to popup the primay or
 * windowlist menu */
static void
cb_xfdesktop_application_menu(GAction  *action,
                              GVariant *parameter,
                              gpointer  data)
{
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(data);
    gboolean popup_root_menu = g_variant_get_boolean(parameter);
    gint screen_num;

    TRACE("entering");

    screen_num = xfdesktop_application_get_current_screen_number(app);
    if(screen_num < 0)
        return;

    if(popup_root_menu) {
        xfce_desktop_popup_root_menu(XFCE_DESKTOP(app->desktops[screen_num]),
                                     0, GDK_CURRENT_TIME);
    } else {
        xfce_desktop_popup_secondary_root_menu(XFCE_DESKTOP(app->desktops[screen_num]),
                                               0, GDK_CURRENT_TIME);
    }
}

static void
cb_xfdesktop_application_arrange(GAction  *action,
                                 GVariant *parameter,
                                 gpointer  data)
{
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(data);
    gint screen_num;

    TRACE("entering");

    screen_num = xfdesktop_application_get_current_screen_number(app);
    if(screen_num < 0)
        return;

    xfce_desktop_arrange_icons(XFCE_DESKTOP(app->desktops[screen_num]));
}

static void
xfdesktop_application_startup(GApplication *g_application)
{
    XfdesktopApplication *app = XFDESKTOP_APPLICATION(g_application);

    TRACE("entering");

    g_application_hold(g_application);

    xfdesktop_application_start(app);

    G_APPLICATION_CLASS(xfdesktop_application_parent_class)->startup(g_application);
}

static void
xfdesktop_application_start(XfdesktopApplication *app)
{
    GdkDisplay *gdpy;
    GError *error = NULL;
    gint i;
    gchar buf[1024];

    TRACE("entering");

    g_return_if_fail(app != NULL);

    gdpy = gdk_display_get_default();

    app->sm_client = xfce_sm_client_get();
    xfce_sm_client_set_restart_style(app->sm_client, XFCE_SM_CLIENT_RESTART_IMMEDIATELY);
    xfce_sm_client_set_priority(app->sm_client, XFCE_SM_CLIENT_PRIORITY_DESKTOP);
    g_signal_connect(app->sm_client, "quit",
                     G_CALLBACK(session_die), app);

    if(!xfce_sm_client_connect(app->sm_client, &error) && error) {
        g_printerr("Failed to connect to session manager: %s\n", error->message);
        g_clear_error(&error);
    }


    if(!xfconf_init(&error)) {
        g_warning("%s: unable to connect to settings daemon: %s.  Defaults will be used",
                  PACKAGE, error->message);
        g_error_free(error);
        error = NULL;
    } else
        app->channel = xfconf_channel_get(XFDESKTOP_CHANNEL);

    app->nscreens = gdk_display_get_n_screens(gdpy);
    app->desktops = g_new0(GtkWidget *, app->nscreens);
    for(i = 0; i < app->nscreens; i++) {
        g_snprintf(buf, sizeof(buf), "/backdrop/screen%d/", i);
        app->desktops[i] = xfce_desktop_new(gdk_display_get_screen(gdpy, i),
                                                  app->channel, buf);
        gtk_widget_add_events(app->desktops[i], GDK_BUTTON_PRESS_MASK
                              | GDK_BUTTON_RELEASE_MASK | GDK_SCROLL_MASK);
        g_signal_connect(G_OBJECT(app->desktops[i]), "scroll-event",
                         G_CALLBACK(scroll_cb), app);
        menu_attach(XFCE_DESKTOP(app->desktops[i]));
        windowlist_attach(XFCE_DESKTOP(app->desktops[i]));
        gtk_widget_show(app->desktops[i]);
        gdk_window_lower(gtk_widget_get_window(app->desktops[i]));
    }

    for(i = 0; i < app->nscreens; ++i) {
        xfce_desktop_set_session_logout_func(XFCE_DESKTOP(app->desktops[i]),
                                             session_logout);
    }

    menu_init(app->channel);
    windowlist_init(app->channel);

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
        g_error_free(error);
    }

    gtk_main();

    g_application_release(G_APPLICATION(app));
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
    gint i;

    TRACE("entering");

    menu_cleanup();
    windowlist_cleanup();

    for(i = 0; i < app->nscreens; i++)
        gtk_widget_destroy(app->desktops[i]);
    g_free(app->desktops);

    xfconf_shutdown();

#ifdef HAVE_LIBNOTIFY
    xfdesktop_notify_uninit();
#endif

    G_APPLICATION_CLASS(xfdesktop_application_parent_class)->shutdown(g_application);
}

static gboolean
xfdesktop_application_local_command_line(GApplication *g_application,
                                         gchar ***arguments,
                                         int *exit_status)
{
    GOptionContext *octx;
    gint argc;
    GError *error = NULL;
    gboolean opt_version = FALSE;
    gboolean opt_reload = FALSE;
    gboolean opt_menu = FALSE;
    gboolean opt_windowlist = FALSE;
    gboolean opt_arrange = FALSE;
    gboolean opt_quit = FALSE;
    gboolean option_set = FALSE;
    const GOptionEntry main_entries[] = {
        { "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_version, N_("Display version information"), NULL },
        { "reload", 'R', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_reload, N_("Reload all settings"), NULL },
        { "menu", 'M', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_menu, N_("Pop up the menu (at the current mouse position)"), NULL },
        { "windowlist", 'W', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_windowlist, N_("Pop up the window list (at the current mouse position)"), NULL },
#ifdef ENABLE_FILE_ICONS
        { "arrange", 'A', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_arrange, N_("Automatically arrange all the icons on the desktop"), NULL },
#endif
        { "quit", 'Q', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_quit, N_("Cause xfdesktop to quit"), NULL },
        { NULL, 0, 0, 0, NULL, NULL, NULL }
    };

    TRACE("entering");

    argc = g_strv_length(*arguments);

    octx = g_option_context_new("");
    g_option_context_set_ignore_unknown_options(octx, TRUE);
    g_option_context_add_main_entries(octx, main_entries, NULL);
    g_option_context_add_group(octx, gtk_get_option_group(TRUE));
    g_option_context_add_group(octx, xfce_sm_client_get_option_group(argc, *arguments));

    if(!g_option_context_parse(octx, &argc, arguments, &error)) {
        g_printerr(_("Failed to parse arguments: %s\n"), error->message);
        g_option_context_free(octx);
        g_error_free(error);
        *exit_status = 1;
        return TRUE;
    }

    g_option_context_free(octx);

    if(opt_version) {
        g_print(_("This is %s version %s, running on Xfce %s.\n"), PACKAGE,
                VERSION, xfce_version_string());
        g_print(_("Built with GTK+ %d.%d.%d, linked with GTK+ %d.%d.%d."),
                GTK_MAJOR_VERSION,GTK_MINOR_VERSION, GTK_MICRO_VERSION,
                gtk_major_version, gtk_minor_version, gtk_micro_version);
        g_print("\n");
        g_print(_("Build options:\n"));
        g_print(_("    Desktop Menu:        %s\n"),
#ifdef USE_DESKTOP_MENU
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

        *exit_status = 0;
        return TRUE;
    }

    g_application_register(g_application, NULL, NULL);

    if(opt_quit) {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "quit", NULL);
        option_set = TRUE;
    } else if(opt_reload) {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "reload", NULL);
        option_set = TRUE;
    }
    if(opt_menu) {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "menu",
                                       g_variant_new_boolean(TRUE));
        option_set = TRUE;
    } else if(opt_windowlist) {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "menu",
                                       g_variant_new_boolean(FALSE));
        option_set = TRUE;
    } else if(opt_arrange) {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "arrange", NULL);
        option_set = TRUE;
    }

    if(option_set) {
        *exit_status = 0;
        return TRUE;
    }

    return G_APPLICATION_CLASS(xfdesktop_application_parent_class)->local_command_line(g_application, arguments, exit_status);
}

static gint
xfdesktop_application_command_line(GApplication *g_application,
                                   GApplicationCommandLine *command_line)
{
    /* If we don't process everything in the local command line then the options
     * won't show up when during xfdesktop --help */
    TRACE("Do nothing");

    return 0;
}
