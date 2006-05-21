/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006 Brian Tarricone, <bjt23@cornell.edu>
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

#define EXO_API_SUBJECT_TO_CHANGE
#include <thunar-vfs/thunar-vfs.h>

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
                                              ThunarVfsInfo *src_info,
                                              ThunarVfsInfo *dest_info,
                                              XfdesktopFileUtilsFileop fileop,
                                              GError *error);

#endif
