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

#ifndef __XFDESKTOP_SETTINGS_H__
#define __XFDESKTOP_SETTINGS_H__

#include <gtk/gtk.h>
#include <libxfce4windowing/libxfce4windowing.h>
#include <xfconf/xfconf.h>

G_BEGIN_DECLS

typedef struct _XfdesktopBackgroundSettings XfdesktopBackgroundSettings;

typedef struct {
    XfconfChannel *channel;
    GtkWidget *settings_toplevel;
    GtkBuilder *main_gxml;

    XfdesktopBackgroundSettings *background_settings;
} XfdesktopSettings;

XfdesktopBackgroundSettings *xfdesktop_background_settings_init(XfdesktopSettings *settings);
void xfdesktop_menu_settings_init(XfdesktopSettings *settings);
void xfdesktop_icon_settings_init(XfdesktopSettings *settings);
void xfdesktop_file_icon_settings_init(XfdesktopSettings *settings);
void xfdesktop_keyboard_shortcut_settings_init(XfdesktopSettings *settings);

void xfdesktop_background_settings_destroy(XfdesktopBackgroundSettings *background_settings);

G_END_DECLS

#endif /* __XFDESKTOP_SETTINGS_H__ */
