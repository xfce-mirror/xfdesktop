/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006      Brian Tarricone, <brian@tarricone.org>
 *  Copyright(c) 2010-2011 Jannis Pohlmann, <jannis@xfce.org>
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

#ifndef __XFDESKTOP_FILE_UTILS_H__
#define __XFDESKTOP_FILE_UTILS_H__

#include <gio/gio.h>

#ifdef HAVE_THUNARX
#include <thunarx/thunarx.h>
#endif

#include "xfdesktop-file-icon.h"

typedef void (*CreateDesktopFileCallback)(GFile *file, GError *error, gpointer user_data);

gboolean xfdesktop_file_utils_is_desktop_file(GFileInfo *info);
gboolean xfdesktop_file_utils_file_is_executable(GFileInfo *info);
gchar *xfdesktop_file_utils_format_time_for_display(guint64 file_time);
GKeyFile *xfdesktop_file_utils_query_key_file(GFile *file,
                                              GCancellable *cancellable,
                                              GError **error);
gchar *xfdesktop_file_utils_get_display_name(GFile *file,
                                             GFileInfo *info);
GFile *xfdesktop_file_utils_next_new_file_name(GFile *file);

GList *xfdesktop_file_utils_file_icon_list_to_file_list(GList *icon_list);
GList *xfdesktop_file_utils_file_list_from_string(const gchar *string);
gchar *xfdesktop_file_utils_file_list_to_string(GList *file_list);
gchar **xfdesktop_file_utils_file_list_to_uri_array(GList *file_list);
void xfdesktop_file_utils_file_list_free(GList *file_list);

GdkPixbuf *xfdesktop_file_utils_get_fallback_icon(gint size,
                                                  gint scale);

void xfdesktop_file_utils_set_window_cursor(GtkWindow *window,
                                            GdkCursorType cursor_type);

gboolean xfdesktop_file_utils_app_info_launch(GAppInfo *app_info,
                                              GFile *working_directory,
                                              GList *files,
                                              GAppLaunchContext *context,
                                              GError **error);

void xfdesktop_file_utils_open_folders(GList *files,
                                       GdkScreen *screen,
                                       GtkWindow *parent);
void xfdesktop_file_utils_rename_file(GFile *file,
                                      GdkScreen *screen,
                                      GtkWindow *parent);
void xfdesktop_file_utils_bulk_rename(GFile *working_directory,
                                      GList *files,
                                      GdkScreen *screen,
                                      GtkWindow *parent);
void xfdesktop_file_utils_trash_files(GList *files,
                                       GdkScreen *screen,
                                       GtkWindow *parent);
void xfdesktop_file_utils_empty_trash(GdkScreen *screen,
                                      GtkWindow *parent);
void xfdesktop_file_utils_unlink_files(GList *files,
                                       GdkScreen *screen,
                                       GtkWindow *parent);
GFile *xfdesktop_file_utils_prompt_for_template_file_name(GFile *parent_folder,
                                                          GFile *template_file,
                                                          GtkWindow *parent);
GFile *xfdesktop_file_utils_prompt_for_new_folder_name(GFile *parent_folder,
                                                       GtkWindow *parent);

void xfdesktop_file_utils_create_file_from_template(GFile *template_file,
                                                    GFile *dest_file,
                                                    GtkWindow *parent);
void xfdesktop_file_utils_create_folder(GFile *folder,
                                        GtkWindow *parent);

/* element-type GFile */
void xfdesktop_file_utils_show_properties_dialog(GList *files,
                                                 GdkScreen *screen,
                                                 GtkWindow *parent);
void xfdesktop_file_utils_launch(GFile *file,
                                 GdkScreen *screen,
                                 GtkWindow *parent);
gboolean xfdesktop_file_utils_execute(GFile *working_directory,
                                      GFile *file,
                                      GList *files,
                                      GdkScreen *screen,
                                      GtkWindow *parent);
void xfdesktop_file_utils_display_app_chooser_dialog(GFile *file,
                                                     gboolean open,
                                                     gboolean preselect_default_checkbox,
                                                     GdkScreen *screen,
                                                     GtkWindow *parent);
void xfdesktop_file_utils_build_transfer_file_lists(GdkDragAction action,
                                                    GList *source_icons,
                                                    XfdesktopFileIcon *dest_icon,
                                                    GList **out_source_files,
                                                    GList **out_dest_files);
void xfdesktop_file_utils_transfer_files(GdkDragAction action,
                                         GList *source_files,
                                         GList *target_files,
                                         GdkScreen *screen);

void xfdesktop_file_utils_create_desktop_file(GdkScreen *screen,
                                              GFile *folder,
                                              const gchar *launcher_type,
                                              const gchar *suggested_name,
                                              const gchar *suggested_command_or_url,
                                              GCancellable *cancellable,
                                              CreateDesktopFileCallback callback,
                                              gpointer callback_data);

gboolean xfdesktop_file_utils_dbus_init(void);
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
