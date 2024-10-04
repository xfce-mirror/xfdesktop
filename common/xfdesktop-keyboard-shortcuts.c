/*
 *  Copyright (c) 2024 Brian Tarricone <brian@tarricone.org>
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "xfdesktop-keyboard-shortcuts.h"

#define ACCEL_MAP_REL_DIR "xfce4/desktop/"
#define ACCEL_MAP_REL_FILE ACCEL_MAP_REL_DIR "accels.scm"

#define DUMMY_ACCEL_PATH "<Actions>/Xfdesktop/dummy"

#define ENTRY(id, path, accel, label) \
    { id, path, accel, XFCE_GTK_MENU_ITEM, label, NULL, NULL, G_CALLBACK(dummy_callback) }

#define DEFINE_ACTIONS_INIT_GETTER(name) \
    static gpointer \
    name ## _actions_init(gpointer data) { \
        if (data != NULL) { \
            ShortcutActionFixupFunc fixup_func = data; \
            for (gsize i = 0; i < G_N_ELEMENTS(name ## _entries); ++i) { \
                fixup_func(&name ## _entries[i]); \
            } \
        } \
        \
        xfce_gtk_translate_action_entries(name ## _entries, G_N_ELEMENTS(name ## _entries)); \
        xfce_gtk_accel_map_add_entries(name ## _entries, G_N_ELEMENTS(name ## _entries)); \
        return NULL; \
    } \
    \
    XfceGtkActionEntry * \
    xfdesktop_get_ ## name ## _actions(ShortcutActionFixupFunc fixup_func, gsize *n_actions) { \
        static GOnce once = G_ONCE_INIT; \
        g_once(&once, name ## _actions_init, fixup_func); \
        \
        if (n_actions != NULL) { \
            *n_actions = G_N_ELEMENTS(name ## _entries); \
        } \
        return name ## _entries; \
    }


typedef struct {
    XfceGtkActionEntry *entries;
    gsize n_entries;
} OnceTranslateData;

static void dummy_callback(void);
static void watch_accel_map_file(void);
static gboolean unwatch_accel_map_file(void);
static gboolean unwatch_accel_map_directory(void);

static XfceGtkActionEntry desktop_entries[] = {
    ENTRY(XFCE_DESKTOP_ACTION_RELOAD, "<Actions>/XfceDesktop/reload", "<Primary>r", N_("_Reload")),
    ENTRY(XFCE_DESKTOP_ACTION_RELOAD_ALT_1, "<Actions>/XfceDesktop/reload-1", "F5", NULL),
    ENTRY(XFCE_DESKTOP_ACTION_RELOAD_ALT_2, "<Actions>/XfceDesktop/reload-2", "Reload", NULL),
    ENTRY(XFCE_DESKTOP_ACTION_POPUP_PRIMARY_MENU, "<Actions>/XfceDesktop/primary-menu", "<Shift>F10", N_("Activate _Primary Menu")),
    ENTRY(XFCE_DESKTOP_ACTION_POPUP_PRIMARY_MENU_ALT_1, "<Actions>/XfceDesktop/primary-menu-2", "Menu", NULL),
    ENTRY(XFCE_DESKTOP_ACTION_POPUP_SECONDARY_MENU, "<Actions>/XfceDesktop/secondary-menu", "<Primary><Shift>F10", N_("Activate _Secondary Menu")),
    ENTRY(XFCE_DESKTOP_ACTION_POPUP_SECONDARY_MENU_ALT_1, "<Actions>/XfceDesktop/secondary-menu-2", "<Primary>Menu", NULL),
    ENTRY(XFCE_DESKTOP_ACTION_NEXT_BACKGROUND, "<Actions>/XfceDesktop/next-background", "", N_("Cycle Next Background")),
};

#ifdef ENABLE_DESKTOP_ICONS

static XfceGtkActionEntry icon_view_entries[] = {
    ENTRY(XFDESKTOP_ICON_VIEW_ACTION_ACTIVATE, "<Actions>/XfdesktopIconView/activate", "space", N_("_Activate Icon")),
    ENTRY(XFDESKTOP_ICON_VIEW_ACTION_ACTIVATE_ALT_1, "<Actions>/XfdesktopIconView/activate-2", "KP_Space", NULL),
    ENTRY(XFDESKTOP_ICON_VIEW_ACTION_ACTIVATE_ALT_2, "<Actions>/XfdesktopIconView/activate-3", "Return", NULL),
    ENTRY(XFDESKTOP_ICON_VIEW_ACTION_ACTIVATE_ALT_3, "<Actions>/XfdesktopIconView/activate-4", "ISO_Enter", NULL),
    ENTRY(XFDESKTOP_ICON_VIEW_ACTION_ACTIVATE_ALT_4, "<Actions>/XfdesktopIconView/activate-5", "KP_Enter", NULL),
    ENTRY(XFDESKTOP_ICON_VIEW_ACTION_TOGGLE_CURSOR, "<Actions>/XfdesktopIconView/toggle-cursor", "<Primary>space", N_("_Toggle Cursor Icon")),
    ENTRY(XFDESKTOP_ICON_VIEW_ACTION_TOGGLE_CURSOR_ALT_1, "<Actions>/XfdesktopIconView/toggle-cursor-2", "<Primary>KP_Space", NULL),
    ENTRY(XFDESKTOP_ICON_VIEW_ACTION_SELECT_ALL, "<Actions>/XfdesktopIconView/select-all", "<Primary>a", N_("Select _All")),
    ENTRY(XFDESKTOP_ICON_VIEW_ACTION_UNSELECT_ALL, "<Actions>/XfdesktopIconView/unselect-all", "Escape", N_("U_nselect All")),
    ENTRY(XFDESKTOP_ICON_VIEW_ACTION_ARRANGE_ICONS, "<Actions>/XfdesktopIconView/arrange-icons", "", N_("A_rrange Icons")),
};

static XfceGtkActionEntry window_icon_manager_entries[] = {
    ENTRY(XFDESKTOP_WINDOW_ICON_MANAGER_ACTION_UNMINIMIZE, "<Actions>/XfdesktopWindowIconManager/unminimize", "<Primary>O", N_("Un_minimize Window")),
    ENTRY(XFDESKTOP_WINDOW_ICON_MANAGER_ACTION_CLOSE, "<Actions>/XfdesktopWindowIconManager/close", "<Primary>C", N_("_Close Window")),
};

#ifdef ENABLE_FILE_ICONS
static XfceGtkActionEntry file_icon_manager_entries[] = {
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_CREATE_FOLDER, "<Actions>/XfdesktopFileIconManager/create-folder", "<Primary><Shift>n", N_("Create _Folder")),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_CREATE_DOCUMENT, "<Actions>/XfdesktopFileIconManager/create-document", "", N_("Create _Document")),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_OPEN, "<Actions>/XfdesktopFileIconManager/open", "<Primary>o", N_("_Open")),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_OPEN_WITH_OTHER, "<Actions>/XfdesktopFileIconManager/open-with-other", "", N_("Ope_n With Other Application")),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_OPEN_FILESYSTEM, "<Actions>/XfdesktopFileIconManager/open-filesystem", "", N_("Open _Filesystem")),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_OPEN_HOME, "<Actions>/XfdesktopFileIconManager/open-home", "<Alt>Home", N_("Open _Home")),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_OPEN_TRASH, "<Actions>/XfdesktopFileIconManager/open-trash", "", N_("Open _Trash")),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_RENAME, "<Actions>/XfdesktopFileIconManager/rename", "F2", N_("_Rename")),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_CUT, "<Actions>/XfdesktopFileIconManager/cut", "<Primary>x", N_("Cu_t")),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_CUT_ALT_1, "<Actions>/XfdesktopFileIconManager/cut-2", "", NULL),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_COPY, "<Actions>/XfdesktopFileIconManager/copy", "<Primary>c", N_("_Copy")),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_COPY_ALT_1, "<Actions>/XfdesktopFileIconManager/copy-2", "<Primary>Insert", NULL),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_PASTE, "<Actions>/XfdesktopFileIconManager/paste", "<Primary>v", N_("_Paste")),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_PASTE_ALT_1, "<Actions>/XfdesktopFileIconManager/paste-2", "<Shift>Insert", NULL),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_PASTE_INTO_FOLDER, "<Actions>/XfdesktopFileIconManager/paste-into-folder", "", N_("Paste _Into Folder")),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_TRASH, "<Actions>/XfdesktopFileIconManager/trash", "Delete", N_("Mo_ve to Trash")),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_TRASH_ALT_1, "<Actions>/XfdesktopFileIconManager/trash-2", "KP_Delete", NULL),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_TRASH_ALT_2, "<Actions>/XfdesktopFileIconManager/trash-3", "", NULL),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_EMPTY_TRASH, "<Actions>/XfdesktopFileIconManager/empty-trash", "", N_("_Empty Trash")),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_DELETE, "<Actions>/XfdesktopFileIconManager/delete", "<Shift>Delete", N_("_Delete")),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_DELETE_ALT_1, "<Actions>/XfdesktopFileIconManager/delete-2", "<Shift>KP_Delete", NULL),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_DELETE_ALT_2, "<Actions>/XfdesktopFileIconManager/delete-3", "", NULL),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_TOGGLE_SHOW_HIDDEN, "<Actions>/XfdesktopFileIconManager/toggle-show-hidden", "<Primary>h", N_("Show _Hidden Files")),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_PROPERTIES, "<Actions>/XfdesktopFileIconManager/properties", "<Alt>Return", N_("_Properties")),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_PROPERTIES_ALT_1, "<Actions>/XfdesktopFileIconManager/properties-2", "<Alt>ISO_Enter", NULL),
    ENTRY(XFDESKTOP_FILE_ICON_MANAGER_ACTION_PROPERTIES_ALT_2, "<Actions>/XfdesktopFileIconManager/properties-3", "<Alt>KP_Enter", NULL),
};
#endif /* ENABLE_FILE_ICONS */

#endif /* ENABLE_DESKTOP_ICONS */

G_GNUC_NORETURN static void
dummy_callback(void) {
    g_assert_not_reached();
    for (;;) {}
}

static GFileMonitor *accel_map_file_monitor = NULL;
static GFileMonitor *accel_map_dir_monitor = NULL;

static void
accel_map_changed(GtkAccelMap *accel_map, const gchar *accel_path) {
    if (g_strcmp0(accel_path, DUMMY_ACCEL_PATH) != 0) {
        xfdesktop_keyboard_shortcuts_save();
    }
}

static gchar *
find_accel_map_filename(void) {
    return xfce_resource_lookup(XFCE_RESOURCE_CONFIG, ACCEL_MAP_REL_FILE);
}

static gboolean
load_accel_map(void) {
    gchar *accel_map_filename = find_accel_map_filename();
    if (accel_map_filename != NULL) {
        gtk_accel_map_load(accel_map_filename);
        g_free(accel_map_filename);

        GdkModifierType dummy = 0;
        g_signal_emit_by_name(gtk_accel_map_get(), "changed", DUMMY_ACCEL_PATH, 0, &dummy);

        return TRUE;
    } else {
        return FALSE;
    }
}

static void
accel_map_dir_changed(GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type) {
    switch (event_type) {
        case G_FILE_MONITOR_EVENT_CREATED: {
            gchar *accel_map_filename = xfce_resource_save_location(XFCE_RESOURCE_CONFIG, ACCEL_MAP_REL_FILE, TRUE);
            if (accel_map_filename != NULL) {
                GFile *accel_map_file = g_file_new_for_path(accel_map_filename);
                if (g_file_equal(accel_map_file, file)) {
                    unwatch_accel_map_directory();
                    watch_accel_map_file();

                    load_accel_map();
                }

                g_free(accel_map_filename);
                g_object_unref(accel_map_file);
            }
            break;
        }
        default:
            break;
    }
}

static gboolean
unwatch_accel_map_directory(void) {
    if (accel_map_dir_monitor != NULL) {
        g_file_monitor_cancel(accel_map_dir_monitor);
        g_clear_object(&accel_map_dir_monitor);
        return TRUE;
    } else {
        return FALSE;
    }
}

static void
watch_accel_map_directory(void) {
    unwatch_accel_map_directory();

    gchar *accel_map_dirname = xfce_resource_save_location(XFCE_RESOURCE_CONFIG, ACCEL_MAP_REL_DIR, TRUE);
    if (accel_map_dirname != NULL) {
        GFile *accel_map_dir = g_file_new_for_path(accel_map_dirname);
        accel_map_dir_monitor = g_file_monitor_directory(accel_map_dir, G_FILE_MONITOR_NONE, NULL, NULL);
        if (accel_map_dir_monitor != NULL) {
            g_signal_connect(accel_map_dir_monitor, "changed",
                             G_CALLBACK(accel_map_dir_changed), NULL);
        }

        g_free(accel_map_dirname);
        g_object_unref(accel_map_dir);
    }
}

static void
accel_map_file_changed(GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type) {
    switch (event_type) {
        case G_FILE_MONITOR_EVENT_CHANGED:
        case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
            load_accel_map();
            break;

        case G_FILE_MONITOR_EVENT_DELETED:
            unwatch_accel_map_file();
            watch_accel_map_directory();
            break;

        default:
            break;
    }
}

static gboolean
unwatch_accel_map_file(void) {
    if (accel_map_file_monitor != NULL) {
        g_file_monitor_cancel(accel_map_file_monitor);
        g_clear_object(&accel_map_file_monitor);
        return TRUE;
    } else {
        return FALSE;
    }
}

static void
watch_accel_map_file(void) {
    unwatch_accel_map_file();

    gchar *accel_map_filename = find_accel_map_filename();
    if (accel_map_filename != NULL) {
        GFile *accel_map_file = g_file_new_for_path(accel_map_filename);
        accel_map_file_monitor = g_file_monitor(accel_map_file, G_FILE_MONITOR_NONE, NULL, NULL);
        if (accel_map_file_monitor != NULL) {
            g_signal_connect(accel_map_file_monitor, "changed",
                             G_CALLBACK(accel_map_file_changed), NULL);
        }

        g_free(accel_map_filename);
        g_object_unref(accel_map_file);
    }
}

void
xfdesktop_keyboard_shortcuts_init(void) {
    if (load_accel_map()) {
        watch_accel_map_file();
    }

    g_signal_connect(gtk_accel_map_get(), "changed",
                     G_CALLBACK(accel_map_changed), NULL);
}

void
xfdesktop_keyboard_shortcuts_save(void) {
    gchar *accel_map_filename = xfce_resource_save_location(XFCE_RESOURCE_CONFIG, ACCEL_MAP_REL_FILE, TRUE);
    if (accel_map_filename != NULL) {
        gboolean was_watching_file = unwatch_accel_map_file();
        gboolean was_watching_dir = unwatch_accel_map_directory();

        gtk_accel_map_save(accel_map_filename);

        if (was_watching_file) {
            watch_accel_map_file();
        }
        if (was_watching_dir) {
            watch_accel_map_directory();
        }

        g_free(accel_map_filename);
    }
}

void
xfdesktop_keyboard_shortcuts_shutdown(void) {
    g_signal_handlers_disconnect_by_func(gtk_accel_map_get(), accel_map_changed, NULL);
    unwatch_accel_map_file();
    unwatch_accel_map_directory();
}

DEFINE_ACTIONS_INIT_GETTER(desktop)

#ifdef ENABLE_DESKTOP_ICONS

DEFINE_ACTIONS_INIT_GETTER(icon_view)
DEFINE_ACTIONS_INIT_GETTER(window_icon_manager)

#ifdef ENABLE_FILE_ICONS
DEFINE_ACTIONS_INIT_GETTER(file_icon_manager)
#endif /* ENABLE_FILE_ICONS */

#endif /* ENABLE_DESKTOP_ICONS */
