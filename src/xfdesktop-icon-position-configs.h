/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2024 Brian Tarricone, <brian@tarricone.org>
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

#ifndef __XFDESKTOP_ICON_POSITION_CONFIGS_H__
#define __XFDESKTOP_ICON_POSITION_CONFIGS_H__

#include <libxfce4windowing/libxfce4windowing.h>

G_BEGIN_DECLS

typedef struct _XfdesktopIconPositionConfigs XfdesktopIconPositionConfigs;
typedef struct _XfdesktopIconPositionConfig XfdesktopIconPositionConfig;

typedef enum {
    XFDESKTOP_ICON_POSITION_LEVEL_INVALID = -1,
    XFDESKTOP_ICON_POSITION_LEVEL_PRIMARY = 0,
    XFDESKTOP_ICON_POSITION_LEVEL_SECONDARY = 1,
    XFDESKTOP_ICON_POSITION_LEVEL_OTHER = 1,
} XfdesktopIconPositionLevel;

XfdesktopIconPositionConfigs *xfdesktop_icon_position_configs_new(GFile *file);

gboolean xfdesktop_icon_position_configs_load(XfdesktopIconPositionConfigs *configs,
                                              GError **error);

gboolean xfdesktop_icon_position_configs_lookup(XfdesktopIconPositionConfigs *configs,
                                                const gchar *icon_id,
                                                XfwMonitor **monitor,
                                                gint *row,
                                                gint *col);
void xfdesktop_icon_position_configs_remove_icon(XfdesktopIconPositionConfigs *configs,
                                                 XfdesktopIconPositionConfig *config,
                                                 const gchar *icon_id);

void xfdesktop_icon_position_configs_set_icon_position(XfdesktopIconPositionConfigs *configs,
                                                       XfdesktopIconPositionConfig *config,
                                                       const gchar *identifier,
                                                       guint row,
                                                       guint col,
                                                       guint64 last_seen_timestamp);

XfdesktopIconPositionConfig *xfdesktop_icon_position_configs_add_monitor(XfdesktopIconPositionConfigs *configs,
                                                                         XfwMonitor *monitor,
                                                                         XfdesktopIconPositionLevel level,
                                                                         GList **candidates);
void xfdesktop_icon_position_configs_assign_monitor(XfdesktopIconPositionConfigs *configs,
                                                    XfdesktopIconPositionConfig *config,
                                                    XfwMonitor *monitor);
void xfdesktop_icon_position_configs_unassign_monitor(XfdesktopIconPositionConfigs *configs,
                                                      XfwMonitor *monitor);

void xfdesktop_icon_position_configs_delete_config(XfdesktopIconPositionConfigs *configs,
                                                   XfdesktopIconPositionConfig *config);

gboolean xfdesktop_icon_position_configs_save(XfdesktopIconPositionConfigs *configs,
                                              GError **error);

void xfdesktop_icon_position_configs_free(XfdesktopIconPositionConfigs *configs);



XfdesktopIconPositionConfig *xfdesktop_icon_position_config_new(XfdesktopIconPositionLevel level);

// For config migration only
void _xfdesktop_icon_position_config_set_icon_position(XfdesktopIconPositionConfig *config,
                                                       const gchar *identifier,
                                                       guint row,
                                                       guint col);

GList *xfdesktop_icon_position_config_get_monitor_display_names(XfdesktopIconPositionConfig *config);

void xfdesktop_icon_position_config_free(XfdesktopIconPositionConfig *config);

G_END_DECLS

#endif /* __XFDESKTOP_ICON_POSITION_CONFIGS_H__ */
