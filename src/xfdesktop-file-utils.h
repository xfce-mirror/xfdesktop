/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006 Brian Tarricone, <bjt23@cornell.edu>
 *  Copyright(c) 2010 Jannis Pohlmann, <jannis@xfce.org>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __XFDESKTOP_FILE_UTILS_H__
#define __XFDESKTOP_FILE_UTILS_H__

#include <thunar-vfs/thunar-vfs.h>
#include <dbus/dbus-glib.h>

#ifdef HAVE_THUNARX
#include <thunarx/thunarx.h>
#endif

#include "xfdesktop-file-icon.h"

ThunarVfsInteractiveJobResponse xfdesktop_file_utils_interactive_job_ask(GtkWindow *parent,
                                                                         const gchar *message,
                                                                         ThunarVfsInteractiveJobResponse choices);

typedef enum
{
    XFDESKTOP_FILE_UTILS_FILEOP_MOVE = 0,
    XFDESKTOP_FILE_UTILS_FILEOP_COPY,
    XFDESKTOP_FILE_UTILS_FILEOP_LINK,
} XfdesktopFileUtilsFileop;

void xfdesktop_file_utils_handle_fileop_error(GtkWindow *parent,
                                              const ThunarVfsInfo *src_info,
                                              const ThunarVfsInfo *dest_info,
                                              XfdesktopFileUtilsFileop fileop,
                                              GError *error);

void xfdesktop_file_utils_copy_into(GtkWindow *parent,
                                    GList *path_list,
                                    ThunarVfsPath *dest_path);
void xfdesktop_file_utils_move_into(GtkWindow *parent,
                                    GList *path_list,
                                    ThunarVfsPath *dest_path);

gchar *xfdesktop_file_utils_get_file_kind(const ThunarVfsInfo *info,
                                          gboolean *is_link);
gboolean xfdesktop_file_utils_file_is_executable(GFileInfo *info);

GList *xfdesktop_file_utils_file_icon_list_to_file_list(GList *icon_list);
gchar *xfdesktop_file_utils_file_list_to_string(GList *file_list);
void xfdesktop_file_utils_file_list_free(GList *file_list);

GdkPixbuf *xfdesktop_file_utils_get_fallback_icon(gint size);

GdkPixbuf *xfdesktop_file_utils_get_file_icon(const gchar *custom_icon_name,
                                              ThunarVfsInfo *info,
                                              gint size,
                                              const GdkPixbuf *emblem,
                                              guint opacity);

void xfdesktop_file_utils_set_window_cursor(GtkWindow *window,
                                            GdkCursorType cursor_type);

gboolean xfdesktop_file_utils_launch_fallback(const ThunarVfsInfo *info,
                                              GdkScreen *screen,
                                              GtkWindow *parent);

gboolean xfdesktop_file_utils_app_info_launch(GAppInfo *app_info,
                                              GFile *working_directory,
                                              GList *files,
                                              GAppLaunchContext *context,
                                              GError **error);

void xfdesktop_file_utils_open_folder(GFile *file,
                                      GdkScreen *screen,
                                      GtkWindow *parent);
void xfdesktop_file_utils_rename_file(GFile *file,
                                      GdkScreen *screen,
                                      GtkWindow *parent);
void xfdesktop_file_utils_unlink_files(GList *files,
                                       GdkScreen *screen,
                                       GtkWindow *parent);
void xfdesktop_file_utils_create_file(GFile *parent_folder,
                                      const gchar *content_type,
                                      GdkScreen *screen,
                                      GtkWindow *parent);
void xfdesktop_file_utils_show_properties_dialog(GFile *file,
                                                 GdkScreen *screen,
                                                 GtkWindow *parent);
void xfdesktop_file_utils_launch(GFile *file,
                                 GdkScreen *screen,
                                 GtkWindow *parent);
void xfdesktop_file_utils_display_chooser_dialog(GFile *file,
                                                 gboolean open,
                                                 GdkScreen *screen,
                                                 GtkWindow *parent);


gboolean xfdesktop_file_utils_dbus_init(void);
DBusGProxy *xfdesktop_file_utils_peek_trash_proxy(void);
DBusGProxy *xfdesktop_file_utils_peek_filemanager_proxy(void);
void xfdesktop_file_utils_dbus_cleanup(void);



#ifdef HAVE_THUNARX
gchar *xfdesktop_thunarx_file_info_get_name(ThunarxFileInfo *file_info);
gchar *xfdesktop_thunarx_file_info_get_uri(ThunarxFileInfo *file_info);
gchar *xfdesktop_thunarx_file_info_get_parent_uri(ThunarxFileInfo *file_info);
gchar *xfdesktop_thunarx_file_info_get_uri_scheme_file(ThunarxFileInfo *file_info);
gchar *xfdesktop_thunarx_file_info_get_mime_type(ThunarxFileInfo *file_info);
gboolean xfdesktop_thunarx_file_info_has_mime_type(ThunarxFileInfo *file_info,
                                                   const gchar *mime_type);
gboolean xfdesktop_thunarx_file_info_is_directory(ThunarxFileInfo *file_info);
GFile *xfdesktop_thunarx_file_info_get_location(ThunarxFileInfo *file_info);
GFileInfo *xfdesktop_thunarx_file_info_get_file_info(ThunarxFileInfo *file_info);
GFileInfo *xfdesktop_thunarx_file_info_get_filesystem_info(ThunarxFileInfo *file_info);
#endif

#endif
