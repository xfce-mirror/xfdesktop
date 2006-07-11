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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>

#include <libxfcegui4/libxfcegui4.h>

#include "xfdesktop-file-utils.h"

ThunarVfsInteractiveJobResponse
xfdesktop_file_utils_interactive_job_ask(GtkWindow *parent,
                                         const gchar *message,
                                         ThunarVfsInteractiveJobResponse choices)
{
    GtkWidget *dlg, *btn;
    gint resp;
    
    dlg = xfce_message_dialog_new(parent, _("Question"),
                                  GTK_STOCK_DIALOG_QUESTION, NULL, message,
                                  NULL);
    
    if(choices & THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_CANCEL) {
        btn = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
        gtk_widget_show(btn);
        gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn,
                                     THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_CANCEL);
    }
    
    if(choices & THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_NO) {
        btn = gtk_button_new_from_stock(GTK_STOCK_NO);
        gtk_widget_show(btn);
        gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn,
                                     THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_NO);
    }
    
    if(choices & THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES_ALL) {
        btn = gtk_button_new_with_mnemonic(_("Yes to _all"));
        gtk_widget_show(btn);
        gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn,
                                     THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES_ALL);
    }
    
    if(choices & THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES) {
        btn = gtk_button_new_from_stock(GTK_STOCK_YES);
        gtk_widget_show(btn);
        gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn,
                                     THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES);
    }
    
    resp = gtk_dialog_run(GTK_DIALOG(dlg));
    
    gtk_widget_destroy(dlg);
    
    return (ThunarVfsInteractiveJobResponse)resp;
}

void
xfdesktop_file_utils_handle_fileop_error(GtkWindow *parent,
                                         ThunarVfsInfo *src_info,
                                         ThunarVfsInfo *dest_info,
                                         XfdesktopFileUtilsFileop fileop,
                                         GError *error)
{
    if(error) {
        gchar *primary_fmt, *primary;
        
        switch(fileop) {
            case XFDESKTOP_FILE_UTILS_FILEOP_MOVE:
                primary_fmt = _("There was an error moving \"%s\" to \"%s\":");
                break;
            case XFDESKTOP_FILE_UTILS_FILEOP_COPY:
                primary_fmt = _("There was an error copying \"%s\" to \"%s\":");
                break;
            case XFDESKTOP_FILE_UTILS_FILEOP_LINK:
                primary_fmt = _("There was an error linking \"%s\" to \"%s\":");
                break;
            default:
                return;
        }
        
        primary = g_strdup_printf(primary_fmt,
                                  src_info->display_name,
                                  dest_info->display_name);
        xfce_message_dialog(parent, _("File Error"), GTK_STOCK_DIALOG_ERROR,
                            primary, error->message,
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        g_free(primary);
    }
}

gchar *
xfdesktop_file_utils_get_file_kind(const ThunarVfsInfo *info,
                                   gboolean *is_link)
{
    gchar *str = NULL;

    if(!strcmp(thunar_vfs_mime_info_get_name(info->mime_info),
               "inode/symlink"))
    {
        str = g_strdup(_("broken link"));
        if(is_link)
            *is_link = TRUE;
    } else if(info->flags & THUNAR_VFS_FILE_FLAGS_SYMLINK) {
        str = g_strdup_printf(_("link to %s"),
                              thunar_vfs_mime_info_get_comment(info->mime_info));
        if(is_link)
            *is_link = TRUE;
    } else {
        str = g_strdup(thunar_vfs_mime_info_get_comment(info->mime_info));
        if(is_link)
            *is_link = FALSE;
    }
    
    return str;
}
