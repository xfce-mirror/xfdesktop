/*
 *  xfdesktop
 *
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *  Copyright (c) 2008 Brian Tarricone <brian@tarricone.org>
 *  Copyright (c) 2008 Jérôme Guelfucci <jerome.guelfucci@gmail.com>
 *  Copyright (c) 2011 Jannis Pohlmann <jannis@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <cairo-gobject.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4windowing/libxfce4windowing.h>
#include <xfconf/xfconf.h>

#ifdef ENABLE_X11
#include <X11/X.h>
#include <gtk/gtkx.h>
#endif

#include "common/xfdesktop-common.h"
#include "common/xfdesktop-keyboard-shortcuts.h"
#include "xfdesktop-settings-ui.h"
#include "xfdesktop-settings.h"

#ifdef HAVE_XFCE_REVISION_H
#include "xfce-revision.h"
#endif

#define SETTINGS_WINDOW_LAST_WIDTH           "/last/window-width"
#define SETTINGS_WINDOW_LAST_HEIGHT          "/last/window-height"

static void
xfdesktop_settings_response(GtkWidget *dialog, gint response_id, XfconfChannel *channel) {
    if (response_id == GTK_RESPONSE_HELP) {
        xfce_dialog_show_help_with_version(GTK_WINDOW(dialog),
                                           "xfdesktop",
                                           "start",
                                           NULL,
                                           VERSION_SHORT);
    } else {
        GdkWindowState state;
        gint width, height;

        /* don't save the state for full-screen windows */
        state = gdk_window_get_state(gtk_widget_get_window(dialog));

        if ((state & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)) == 0) {
            /* save window size */
            gtk_window_get_size(GTK_WINDOW(dialog), &width, &height);
            xfconf_channel_set_int(channel, SETTINGS_WINDOW_LAST_WIDTH, width);
            xfconf_channel_set_int(channel, SETTINGS_WINDOW_LAST_HEIGHT, height);
        }

        gtk_main_quit();
    }
}

static void
accel_map_changed(void) {
    xfdesktop_keyboard_shortcuts_save();
}

#ifdef ENABLE_X11
static Window opt_socket_id = 0;
#endif
static gboolean opt_version = FALSE;
static gboolean opt_enable_debug = FALSE;
static GOptionEntry option_entries[] = {
#ifdef ENABLE_X11
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
#endif
    { "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { "enable-debug", 'e', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_enable_debug, N_("Enable debug messages"), NULL },
    { NULL, '\0', 0, 0, NULL, NULL, NULL },
};

int
main(int argc, char **argv) {
#ifdef G_ENABLE_DEBUG
    /* do NOT remove this line. If something doesn't work,
     * fix your code instead! */
    g_log_set_always_fatal(G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);
#endif

    xfce_textdomain(GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    GError *error = NULL;
    if (!gtk_init_with_args(&argc, &argv, "", option_entries, PACKAGE, &error)) {
        if(G_LIKELY(error)) {
            g_printerr("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_printerr(_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_printerr("\n");
            g_clear_error(&error);
        } else {
            g_error("Unable to open display.");
        }
        return EXIT_FAILURE;
    }

    if (G_UNLIKELY(opt_version)) {
        g_print("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, VERSION_FULL, xfce_version_string());
        g_print("%s\n", "Copyright (c) 2004-2024");
        g_print("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print(_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print("\n");
        return EXIT_SUCCESS;
    }

    if (!xfconf_init(&error)) {
        xfce_message_dialog(NULL, _("Desktop Settings"),
                            "dialog-error",
                            _("Unable to contact settings server"),
                            error->message,
                            XFCE_BUTTON_TYPE_MIXED, "application-exit", _("Quit"), GTK_RESPONSE_ACCEPT,
                            NULL);
        g_clear_error(&error);
        return EXIT_FAILURE;
    }

    xfdesktop_settings_ui_register_resource();

    GtkBuilder *gxml = gtk_builder_new();
    if (gtk_builder_add_from_resource(gxml, "/org/xfce/xfdesktop/settings/xfdesktop-settings-ui.glade", &error) == 0) {
        g_printerr("Failed to parse UI description: %s\n", error->message);
        g_clear_error(&error);
        return EXIT_FAILURE;
    }

    xfdesktop_keyboard_shortcuts_init();
    g_signal_connect(gtk_accel_map_get(), "changed",
                     G_CALLBACK(accel_map_changed), NULL);

    XfdesktopSettings *settings = g_new(XfdesktopSettings, 1);
    settings->main_gxml = gxml;
    settings->channel = xfconf_channel_new(XFDESKTOP_CHANNEL);

    if (opt_enable_debug) {
        xfdesktop_debug_set(TRUE);
    }

#ifdef ENABLE_X11
    if (opt_socket_id != 0 && xfw_windowing_get() == XFW_WINDOWING_X11) {
        GtkWidget *plug = gtk_plug_new(opt_socket_id);
        gtk_widget_show(plug);
        g_signal_connect(G_OBJECT(plug), "delete-event",
                         G_CALLBACK(gtk_main_quit), NULL);
        settings->settings_toplevel = plug;

        gdk_notify_startup_complete();

        GtkWidget *plug_child = GTK_WIDGET(gtk_builder_get_object(gxml, "notebook_settings"));
        xfce_widget_reparent(plug_child, plug);
        gtk_widget_show(plug_child);
    } else
#endif  /* ENABLE_X11 */
    {
        GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(gxml, "prefs_dialog"));
        settings->settings_toplevel = dialog;

        g_signal_connect(dialog, "response",
                         G_CALLBACK(xfdesktop_settings_response),
                         settings->channel);
        gtk_window_set_default_size(GTK_WINDOW(dialog),
                                    xfconf_channel_get_int(settings->channel, SETTINGS_WINDOW_LAST_WIDTH, -1),
                                    xfconf_channel_get_int(settings->channel, SETTINGS_WINDOW_LAST_HEIGHT, -1));
        gtk_window_present(GTK_WINDOW (dialog));

#ifdef ENABLE_X11
        if (xfw_windowing_get() == XFW_WINDOWING_X11) {
            /* To prevent the settings dialog to be saved in the session */
            gdk_x11_set_sm_client_id("FAKE ID");
        }
#endif  /* ENABLE_X11 */
    }

    settings->background_settings = xfdesktop_background_settings_init(settings);
    g_assert(settings->background_settings != NULL);

    xfdesktop_menu_settings_init(settings);
    xfdesktop_icon_settings_init(settings);
    xfdesktop_file_icon_settings_init(settings);
    xfdesktop_keyboard_shortcut_settings_init(settings);

    gtk_main();

    xfdesktop_keyboard_shortcuts_shutdown();

    xfdesktop_background_settings_destroy(settings->background_settings);
    g_object_unref(G_OBJECT(settings->main_gxml));
    g_object_unref(G_OBJECT(settings->channel));
    g_free(settings);

    xfconf_shutdown();

    return EXIT_SUCCESS;
}
