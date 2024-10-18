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

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "common/xfdesktop-common.h"
#include "xfconf/xfconf.h"
#include "xfdesktop-settings.h"

#define NOTEBOOK_PAGE_FILE_ICONS 3

#define TIMER_ID_KEY "xfdesktop-timer-id"
#define XFCONF_CHANNEL_KEY "xfdesktop-xfconf-channel"

static void
cb_icon_style_changed(GtkComboBox *combo, GtkBuilder *main_gxml) {
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(main_gxml, "box_icons_appearance_settings")),
                             gtk_combo_box_get_active(combo) != XFCE_DESKTOP_ICON_STYLE_NONE);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(main_gxml, "box_icons_behavior_settings")),
                             gtk_combo_box_get_active(combo) != XFCE_DESKTOP_ICON_STYLE_NONE);

#ifdef ENABLE_FILE_ICONS
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(main_gxml, "tab_file_launcher_icons_content")),
                             gtk_combo_box_get_active(combo) == XFCE_DESKTOP_ICON_STYLE_FILES);
    gtk_widget_set_visible(GTK_WIDGET(gtk_builder_get_object(main_gxml, "infobar_file_launcher_icons_disabled")),
                           gtk_combo_box_get_active(combo) != XFCE_DESKTOP_ICON_STYLE_FILES);
#endif
}

static gboolean
xfdesktop_spin_icon_size_timer(gpointer user_data) {
    TRACE("entering");

    GtkSpinButton *button = user_data;
    XfconfChannel *channel = g_object_get_data(G_OBJECT(button), XFCONF_CHANNEL_KEY);

    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel), FALSE);

    xfconf_channel_set_uint(channel,
                            DESKTOP_ICONS_ICON_SIZE_PROP,
                            gtk_spin_button_get_value(button));

    g_object_set_data(G_OBJECT(button), TIMER_ID_KEY, NULL);

    return G_SOURCE_REMOVE;
}

static void
cb_xfdesktop_spin_icon_size_changed(GtkSpinButton *button, XfconfChannel *channel) {
    TRACE("entering");

    guint timer_id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), TIMER_ID_KEY));
    if (timer_id != 0) {
        g_source_remove(timer_id);
    }

    timer_id = g_timeout_add(500, xfdesktop_spin_icon_size_timer, button);
    g_object_set_data(G_OBJECT(button), TIMER_ID_KEY, GUINT_TO_POINTER(timer_id));
}

static void
cb_xfdesktop_icon_orientation_changed(GtkComboBox *combo) {
    /* TRANSLATORS: Please split the message in half with '\n' so the dialog will not be too wide. */
    const gchar *question = _("Would you like to arrange all existing\n"
                              "icons according to the selected orientation?");
    GtkWindow *window = GTK_WINDOW (gtk_widget_get_toplevel(GTK_WIDGET(combo)));

    if (xfce_dialog_confirm(window, "view-sort-ascending", _("Arrange icons"), NULL, "%s", question)) {
        const gchar *const cmd = "xfdesktop --arrange";
        GError *error = NULL;
        if (!g_spawn_command_line_async(cmd, &error)) {
            gchar *primary = g_strdup_printf(_("Unable to launch \"%s\":"), cmd);
            xfce_message_dialog(window, _("Launch Error"),
                                "dialog-error", primary, error->message,
                                XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"),
                                GTK_RESPONSE_ACCEPT, NULL);
            g_free(primary);
            g_clear_error(&error);
        }
    }
}

void
xfdesktop_icon_settings_init(XfdesktopSettings *settings) {
    GtkWidget *combo_icons = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml, "combo_icons"));
    g_signal_connect(combo_icons, "changed",
                     G_CALLBACK(cb_icon_style_changed), settings->main_gxml);
#ifdef ENABLE_FILE_ICONS
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_icons), XFCE_DESKTOP_ICON_STYLE_FILES);
#else
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_icons), XFCE_DESKTOP_ICON_STYLE_WINDOWS);
    gtk_notebook_remove_page(GTK_NOTEBOOK(gtk_builder_get_object(settings->main_gxml, "notebook_settings")),
                             NOTEBOOK_PAGE_FILE_ICONS);
#endif
    xfconf_g_property_bind(settings->channel, DESKTOP_ICONS_STYLE_PROP, G_TYPE_INT,
                           G_OBJECT(combo_icons), "active");

    /* icon size */
    GtkWidget *spin_icon_size = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml, "spin_icon_size"));
    g_object_set_data_full(G_OBJECT(spin_icon_size),
                           XFCONF_CHANNEL_KEY,
                           g_object_ref(settings->channel),
                           g_object_unref);
    g_signal_connect(G_OBJECT(spin_icon_size), "value-changed",
                     G_CALLBACK(cb_xfdesktop_spin_icon_size_changed), NULL);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_icon_size),
                              xfconf_channel_get_uint(settings->channel,
                                                      DESKTOP_ICONS_ICON_SIZE_PROP,
                                                      DEFAULT_ICON_SIZE));

    /* font size */
    GtkWidget *chk_custom_font_size = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml,
                                                                        "chk_custom_font_size"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_custom_font_size), DEFAULT_ICON_FONT_SIZE_SET);
    xfconf_g_property_bind(settings->channel, DESKTOP_ICONS_CUSTOM_FONT_SIZE_PROP,
                           G_TYPE_BOOLEAN, G_OBJECT(chk_custom_font_size),
                           "active");
    GtkWidget *spin_font_size = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml, "spin_font_size"));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_font_size), DEFAULT_ICON_FONT_SIZE);
    xfconf_g_property_bind(settings->channel, DESKTOP_ICONS_FONT_SIZE_PROP, G_TYPE_DOUBLE,
                           G_OBJECT(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spin_font_size))),
                           "value");
    g_object_bind_property(chk_custom_font_size, "active", spin_font_size, "sensitive", G_BINDING_SYNC_CREATE);

    GtkWidget *chk_custom_font_color = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml, "chk_custom_font_color"));
    xfconf_g_property_bind(settings->channel,
                           DESTKOP_ICONS_CUSTOM_LABEL_TEXT_COLOR_PROP,
                           G_TYPE_BOOLEAN,
                           chk_custom_font_color,
                           "active");
    GtkWidget *btn_custom_font_color = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml, "btn_custom_font_color"));
    xfconf_g_property_bind(settings->channel,
                           DESTKOP_ICONS_LABEL_TEXT_COLOR_PROP,
                           G_TYPE_PTR_ARRAY,
                           btn_custom_font_color,
                           "rgba");
    g_object_bind_property(chk_custom_font_color, "active", btn_custom_font_color, "sensitive", G_BINDING_SYNC_CREATE);

    GtkWidget *chk_custom_label_bg_color = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml, "chk_custom_label_bg_color"));
    xfconf_g_property_bind(settings->channel,
                           DESTKOP_ICONS_CUSTOM_LABEL_BG_COLOR_PROP,
                           G_TYPE_BOOLEAN,
                           chk_custom_label_bg_color,
                           "active");
    GtkWidget *btn_custom_label_bg_color = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml, "btn_custom_label_bg_color"));
    xfconf_g_property_bind(settings->channel,
                           DESTKOP_ICONS_LABEL_BG_COLOR_PROP,
                           G_TYPE_PTR_ARRAY,
                           btn_custom_label_bg_color,
                           "rgba");
    g_object_bind_property(chk_custom_label_bg_color,
                           "active",
                           btn_custom_label_bg_color,
                           "sensitive",
                           G_BINDING_SYNC_CREATE);

    /* tooltip options */
    GtkWidget *chk_show_tooltips = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml, "chk_show_tooltips"));
    xfconf_g_property_bind(settings->channel, DESKTOP_ICONS_SHOW_TOOLTIP_PROP,
                           G_TYPE_BOOLEAN, G_OBJECT(chk_show_tooltips),
                           "active");
    GtkWidget *spin_tooltip_size = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml, "spin_tooltip_size"));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_tooltip_size), 64);
    xfconf_g_property_bind(settings->channel, DESKTOP_ICONS_TOOLTIP_SIZE_PROP, G_TYPE_DOUBLE,
                           G_OBJECT(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spin_tooltip_size))),
                           "value");
    g_object_bind_property(chk_show_tooltips, "active", spin_tooltip_size, "sensitive", G_BINDING_SYNC_CREATE);

    /* Orientation combo */
    GtkWidget *combo_orientation = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml, "combo_orientation"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_orientation), 0);
    xfconf_g_property_bind(settings->channel, DESKTOP_ICONS_GRAVITY_PROP, G_TYPE_INT,
                           G_OBJECT(combo_orientation), "active");
    g_signal_connect(G_OBJECT(combo_orientation), "changed",
                     G_CALLBACK(cb_xfdesktop_icon_orientation_changed), NULL);

    xfconf_g_property_bind(settings->channel, DESKTOP_ICONS_ON_PRIMARY_PROP, G_TYPE_BOOLEAN,
                           G_OBJECT(gtk_builder_get_object(settings->main_gxml, "primary")),
                           "active");

    /* single click */
    GtkWidget *chk_single_click = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml,
                                                                    "chk_single_click"));
    xfconf_g_property_bind(settings->channel, DESKTOP_ICONS_SINGLE_CLICK_PROP,
                           G_TYPE_BOOLEAN, G_OBJECT(chk_single_click),
                           "active");
    GtkWidget *chk_single_click_underline = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml,
                                                                              "chk_single_click_underline"));
    xfconf_g_property_bind(settings->channel, DESKTOP_ICONS_SINGLE_CLICK_ULINE_PROP,
                           G_TYPE_BOOLEAN, G_OBJECT(chk_single_click_underline),
                           "active");
    g_object_bind_property(chk_single_click, "active",
                           chk_single_click_underline, "sensitive",
                           G_BINDING_SYNC_CREATE);

    xfconf_g_property_bind(settings->channel, DESKTOP_ICONS_CONFIRM_SORTING_PROP, G_TYPE_BOOLEAN,
                           G_OBJECT(gtk_builder_get_object(settings->main_gxml, "chk_confirm_icon_sorting")),
                           "active");
}
