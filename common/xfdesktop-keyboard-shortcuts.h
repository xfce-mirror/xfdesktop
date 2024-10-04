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

#ifndef __XFDESKTOP_KEYBOARD_SHORTCUTS_H__
#define __XFDESKTOP_KEYBOARD_SHORTCUTS_H__

#include <libxfce4ui/libxfce4ui.h>

G_BEGIN_DECLS

typedef void (*ShortcutActionFixupFunc)(XfceGtkActionEntry *entry);

typedef enum {
    XFCE_DESKTOP_ACTION_RELOAD,
    XFCE_DESKTOP_ACTION_RELOAD_ALT_1,
    XFCE_DESKTOP_ACTION_RELOAD_ALT_2,
    XFCE_DESKTOP_ACTION_POPUP_PRIMARY_MENU,
    XFCE_DESKTOP_ACTION_POPUP_PRIMARY_MENU_ALT_1,
    XFCE_DESKTOP_ACTION_POPUP_SECONDARY_MENU,
    XFCE_DESKTOP_ACTION_POPUP_SECONDARY_MENU_ALT_1,
    XFCE_DESKTOP_ACTION_NEXT_BACKGROUND,
} XfceDesktopActions;

#ifdef ENABLE_DESKTOP_ICONS

typedef enum {
    XFDESKTOP_ICON_VIEW_ACTION_ACTIVATE,
    XFDESKTOP_ICON_VIEW_ACTION_ACTIVATE_ALT_1,
    XFDESKTOP_ICON_VIEW_ACTION_ACTIVATE_ALT_2,
    XFDESKTOP_ICON_VIEW_ACTION_ACTIVATE_ALT_3,
    XFDESKTOP_ICON_VIEW_ACTION_ACTIVATE_ALT_4,
    XFDESKTOP_ICON_VIEW_ACTION_TOGGLE_CURSOR,
    XFDESKTOP_ICON_VIEW_ACTION_TOGGLE_CURSOR_ALT_1,
    XFDESKTOP_ICON_VIEW_ACTION_SELECT_ALL,
    XFDESKTOP_ICON_VIEW_ACTION_UNSELECT_ALL,
    XFDESKTOP_ICON_VIEW_ACTION_ARRANGE_ICONS,
} XfdesktopIconViewActions;

typedef enum {
    XFDESKTOP_WINDOW_ICON_MANAGER_ACTION_UNMINIMIZE,
    XFDESKTOP_WINDOW_ICON_MANAGER_ACTION_CLOSE,
} XfdesktopWindowIconManagerActions;

#ifdef ENABLE_FILE_ICONS
typedef enum {
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_CREATE_FOLDER,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_CREATE_DOCUMENT,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_OPEN,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_OPEN_WITH_OTHER,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_OPEN_FILESYSTEM,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_OPEN_HOME,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_OPEN_TRASH,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_RENAME,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_CUT,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_CUT_ALT_1,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_COPY,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_COPY_ALT_1,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_PASTE,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_PASTE_ALT_1,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_PASTE_INTO_FOLDER,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_TRASH,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_TRASH_ALT_1,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_TRASH_ALT_2,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_EMPTY_TRASH,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_DELETE,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_DELETE_ALT_1,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_DELETE_ALT_2,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_TOGGLE_SHOW_HIDDEN,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_PROPERTIES,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_PROPERTIES_ALT_1,
    XFDESKTOP_FILE_ICON_MANAGER_ACTION_PROPERTIES_ALT_2,
} XfdesktopFileIconManagerActions;
#endif /* ENABLE_FILE_ICONS */

#endif /* ENABLE_DESKTOP_ICONS */


void xfdesktop_keyboard_shortcuts_init(void);
void xfdesktop_keyboard_shortcuts_save(void);
void xfdesktop_keyboard_shortcuts_shutdown(void);

XfceGtkActionEntry *xfdesktop_get_desktop_actions(ShortcutActionFixupFunc fixup_func,
                                                  gsize *n_actions);

#ifdef ENABLE_DESKTOP_ICONS
XfceGtkActionEntry *xfdesktop_get_icon_view_actions(ShortcutActionFixupFunc fixup_func,
                                                    gsize *n_actions);
XfceGtkActionEntry *xfdesktop_get_window_icon_manager_actions(ShortcutActionFixupFunc fixup_func,
                                                              gsize *n_actions);
#ifdef ENABLE_FILE_ICONS
XfceGtkActionEntry *xfdesktop_get_file_icon_manager_actions(ShortcutActionFixupFunc fixup_func,
                                                            gsize *n_actions);
#endif /* ENABLE_FILE_ICONS */
#endif /* ENABLE_DESKTOP_ICONS */

G_END_DECLS

#endif /* __XFDESKTOP_KEYBOARD_SHORTCUTS_H__ */
