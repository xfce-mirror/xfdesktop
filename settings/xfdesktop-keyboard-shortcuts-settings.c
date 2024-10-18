/*
 *  xfdesktop
 *
 *  Copyright (C) 2024 Brian Tarricone <brian@tarricone.org>
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

#include <gtk/gtk.h>
#include <libxfce4kbd-private/xfce-shortcuts-editor.h>
#include <libxfce4ui/libxfce4ui.h>

#include "common/xfdesktop-keyboard-shortcuts.h"
#include "xfdesktop-settings.h"

void
xfdesktop_keyboard_shortcut_settings_init(XfdesktopSettings *settings) {
    GtkWidget *keyboard_tab = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml, "tab_keyboard_shortcuts"));

    GArray *editor_sections = g_array_sized_new(FALSE, TRUE, sizeof(XfceShortcutsEditorSection), 3);

    gsize n_desktop_action_entries;
    XfceGtkActionEntry *desktop_action_entries = xfdesktop_get_desktop_actions(NULL, &n_desktop_action_entries);
    XfceShortcutsEditorSection section = {
        .section_name = N_("Desktop"),
        .entries = desktop_action_entries,
        .size = n_desktop_action_entries,
    };
    g_array_append_val(editor_sections, section);

#ifdef ENABLE_DESKTOP_ICONS
    gsize n_icon_view_entries;
    XfceGtkActionEntry *icon_view_action_entries = xfdesktop_get_icon_view_actions(NULL, &n_icon_view_entries);
    section = (XfceShortcutsEditorSection){
        .section_name = N_("Icons"),
        .entries = icon_view_action_entries,
        .size = n_icon_view_entries,
    };
    g_array_append_val(editor_sections, section);

    gsize n_window_icon_manager_entries;
    XfceGtkActionEntry *window_icon_manager_action_entries = xfdesktop_get_window_icon_manager_actions(NULL, &n_window_icon_manager_entries);
    section = (XfceShortcutsEditorSection){
        .section_name = N_("Window Icons"),
        .entries = window_icon_manager_action_entries,
        .size = n_window_icon_manager_entries,
    };
    g_array_append_val(editor_sections, section);

#ifdef ENABLE_FILE_ICONS
    gsize n_file_icon_manager_entries;
    XfceGtkActionEntry *file_icon_manager_entries = xfdesktop_get_file_icon_manager_actions(NULL, &n_file_icon_manager_entries);
    section = (XfceShortcutsEditorSection){
        .section_name = N_("File Icons"),
        .entries = file_icon_manager_entries,
        .size = n_file_icon_manager_entries,
    };
    g_array_append_val(editor_sections, section);
#endif /* ENABLE_FILE_ICONS */
#endif /* ENABLE_DESKTOP_ICONS */

    gsize n_sections = editor_sections->len;
    XfceShortcutsEditorSection *sections = (XfceShortcutsEditorSection *)(gpointer)g_array_free(editor_sections, FALSE);
    GtkWidget *shortcuts_editor = xfce_shortcuts_editor_new_array(sections, n_sections);
    g_free(sections);

    gtk_container_add(GTK_CONTAINER(keyboard_tab), shortcuts_editor);
    gtk_widget_show_all(shortcuts_editor);
}
