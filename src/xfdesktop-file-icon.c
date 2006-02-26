/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006 Brian Tarricone, <bjt23@cornell.edu>
 *  Copyright(c) 2006 Benedikt Meurer, <benny@xfce.org>
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

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <libxfcegui4/libxfcegui4.h>

#include "xfdesktop-icon.h"
#include "xfdesktop-clipboard-manager.h"
#include "xfdesktop-file-icon.h"

#define BORDER 8


enum
{
    SIG_POS_CHANGED = 0,
    N_SIGS
};

struct _XfdesktopFileIconPrivate
{
    gint16 row;
    gint16 col;
    GdkPixbuf *pix;
    gint cur_pix_size;
    GdkRectangle extents;
    ThunarVfsInfo *info;
    GdkScreen *gscreen;
    GList *active_jobs;
};

static void xfdesktop_file_icon_icon_init(XfdesktopIconIface *iface);
static void xfdesktop_file_icon_finalize(GObject *obj);

static GdkPixbuf *xfdesktop_file_icon_peek_pixbuf(XfdesktopIcon *icon,
                                                  gint size);
static G_CONST_RETURN gchar *xfdesktop_file_icon_peek_label(XfdesktopIcon *icon);

static void xfdesktop_file_icon_set_position(XfdesktopIcon *icon,
                                             gint16 row,
                                             gint16 col);
static gboolean xfdesktop_file_icon_get_position(XfdesktopIcon *icon,
                                                 gint16 *row,
                                                 gint16 *col);

static void xfdesktop_file_icon_set_extents(XfdesktopIcon *icon,
                                            const GdkRectangle *extents);
static gboolean xfdesktop_file_icon_get_extents(XfdesktopIcon *icon,
                                                GdkRectangle *extents);

static gboolean xfdesktop_file_icon_is_drop_dest(XfdesktopIcon *icon);
static XfdesktopIconDragResult xfdesktop_file_icon_do_drop_dest(XfdesktopIcon *icon,
                                                                XfdesktopIcon *src_icon,
                                                                GdkDragAction action);

static void xfdesktop_file_icon_selected(XfdesktopIcon *icon);
static void xfdesktop_file_icon_activated(XfdesktopIcon *icon);
static void xfdesktop_file_icon_menu_popup(XfdesktopIcon *icon);

static void xfdesktop_delete_file_finished(ThunarVfsJob *job,
                                           gpointer user_data);

static void xfdesktop_file_icon_invalidate_pixbuf(XfdesktopFileIcon *icon);

static guint __signals[N_SIGS] = { 0, };
static GdkPixbuf *xfdesktop_fallback_icon = NULL;
static ThunarVfsMimeDatabase *thunar_mime_database = NULL;
static XfdesktopClipboardManager *clipboard_manager = NULL;

static GQuark xfdesktop_mime_app_quark = 0;


G_DEFINE_TYPE_EXTENDED(XfdesktopFileIcon, xfdesktop_file_icon,
                       G_TYPE_OBJECT, 0,
                       G_IMPLEMENT_INTERFACE(XFDESKTOP_TYPE_ICON,
                                             xfdesktop_file_icon_icon_init))



static void
xfdesktop_file_icon_class_init(XfdesktopFileIconClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    
    gobject_class->finalize = xfdesktop_file_icon_finalize;
    
    __signals[SIG_POS_CHANGED] = g_signal_new("position-changed",
                                              XFDESKTOP_TYPE_FILE_ICON,
                                              G_SIGNAL_RUN_LAST,
                                              G_STRUCT_OFFSET(XfdesktopFileIconClass,
                                                              position_changed),
                                              NULL, NULL,
                                              g_cclosure_marshal_VOID__VOID,
                                              G_TYPE_NONE, 0);
    
    xfdesktop_mime_app_quark = g_quark_from_static_string("xfdesktop-mime-app-quark");
}

static void
xfdesktop_file_icon_init(XfdesktopFileIcon *icon)
{
    /* grab a shared reference on the mime database */
    if(thunar_mime_database == NULL) {
        thunar_mime_database = thunar_vfs_mime_database_get_default();
        g_object_add_weak_pointer(G_OBJECT(thunar_mime_database), (gpointer) &thunar_mime_database);
    } else {
        g_object_ref(G_OBJECT(thunar_mime_database));
    }

    icon->priv = g_new0(XfdesktopFileIconPrivate, 1);
}

static void
xfdesktop_file_icon_finalize(GObject *obj)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(obj);
    GtkIconTheme *itheme = gtk_icon_theme_get_for_screen(icon->priv->gscreen);
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(itheme),
                                         G_CALLBACK(xfdesktop_file_icon_invalidate_pixbuf),
                                         icon);
    
    if(icon->priv->active_jobs) {
        GList *l;
        ThunarVfsJob *job;
        for(l = icon->priv->active_jobs; l; l = l->next) {
            job = THUNAR_VFS_JOB(l->data);
            GCallback cb = g_object_get_data(G_OBJECT(job),
                                             "--xfdesktop-file-icon-callback");
            if(cb) {
                gpointer data = g_object_get_data(G_OBJECT(obj),
                                                  "-xfdesktop-file-icon-data");
                g_signal_handlers_disconnect_by_func(G_OBJECT(job),
                                                     G_CALLBACK(cb),
                                                     data);
                g_object_set_data(G_OBJECT(job),
                                  "--xfdesktop-file-icon-callback", NULL);
            }
            thunar_vfs_job_cancel(job);
            g_object_unref(G_OBJECT(job));
        }
        g_list_free(icon->priv->active_jobs);
    }
    
    if(icon->priv->pix)
        g_object_unref(G_OBJECT(icon->priv->pix));
    
    if(icon->priv->info)
        thunar_vfs_info_unref(icon->priv->info);
    
    g_free(icon->priv);
    
    G_OBJECT_CLASS(xfdesktop_file_icon_parent_class)->finalize(obj);
}

static void
xfdesktop_file_icon_icon_init(XfdesktopIconIface *iface)
{
    iface->peek_pixbuf = xfdesktop_file_icon_peek_pixbuf;
    iface->peek_label = xfdesktop_file_icon_peek_label;
    iface->set_position = xfdesktop_file_icon_set_position;
    iface->get_position = xfdesktop_file_icon_get_position;
    iface->set_extents = xfdesktop_file_icon_set_extents;
    iface->get_extents = xfdesktop_file_icon_get_extents;
    iface->is_drop_dest = xfdesktop_file_icon_is_drop_dest;
    iface->do_drop_dest = xfdesktop_file_icon_do_drop_dest;
    iface->selected = xfdesktop_file_icon_selected;
    iface->activated = xfdesktop_file_icon_activated;
    iface->menu_popup = xfdesktop_file_icon_menu_popup;
}


static void
xfdesktop_file_icon_invalidate_pixbuf(XfdesktopFileIcon *icon)
{
    if(icon->priv->pix) {
        g_object_unref(G_OBJECT(icon->priv->pix));
        icon->priv->pix = NULL;
    }
}

XfdesktopFileIcon *
xfdesktop_file_icon_new(ThunarVfsInfo *info,
                        GdkScreen *screen)
{
    XfdesktopFileIcon *file_icon = g_object_new(XFDESKTOP_TYPE_FILE_ICON, NULL);
    file_icon->priv->info = thunar_vfs_info_ref(info);
    file_icon->priv->gscreen = screen;
    
    if(!clipboard_manager) {
        clipboard_manager = xfdesktop_clipboard_manager_get_for_display(gdk_screen_get_display(screen));
        g_object_add_weak_pointer(G_OBJECT(clipboard_manager),
                                  (gpointer)&clipboard_manager);
    } else
        g_object_ref(G_OBJECT(clipboard_manager));
    
    g_signal_connect_swapped(G_OBJECT(gtk_icon_theme_get_for_screen(screen)),
                             "changed",
                             G_CALLBACK(xfdesktop_file_icon_invalidate_pixbuf),
                             file_icon);
    
    return file_icon;
}


static GdkPixbuf *
xfdesktop_file_icon_peek_pixbuf(XfdesktopIcon *icon,
                                gint size)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    const gchar *icon_name;
    
    if(!file_icon->priv->pix || size != file_icon->priv->cur_pix_size) {
        if(file_icon->priv->pix) {
            g_object_unref(G_OBJECT(file_icon->priv->pix));
            file_icon->priv->pix = NULL;
        }

        icon_name = thunar_vfs_info_get_custom_icon(file_icon->priv->info);
        if(icon_name) {
            file_icon->priv->pix = xfce_themed_icon_load(icon_name, size);
            if(file_icon->priv->pix)
                file_icon->priv->cur_pix_size = size;
        }
            
        if(!file_icon->priv->pix) {
            /* FIXME: GtkIconTheme/XfceIconTheme */
            icon_name = thunar_vfs_mime_info_lookup_icon_name(file_icon->priv->info->mime_info,
                                                              gtk_icon_theme_get_default());
            
            if(icon_name) {
                file_icon->priv->pix = xfce_themed_icon_load(icon_name, size);
                if(file_icon->priv->pix)
                    file_icon->priv->cur_pix_size = size;
            }
        }
    }
    
    /* fallback */
    if(!file_icon->priv->pix) {
        if(xfdesktop_fallback_icon) {
            if(gdk_pixbuf_get_width(xfdesktop_fallback_icon) != size) {
                g_object_unref(G_OBJECT(xfdesktop_fallback_icon));
                xfdesktop_fallback_icon = NULL;
            }
        }
        if(!xfdesktop_fallback_icon) {
            xfdesktop_fallback_icon = xfce_pixbuf_new_from_file_at_size(DATADIR "/pixmaps/xfdesktop/xfdesktop-fallback-icon.png",
                                                                        size,
                                                                        size,
                                                                        NULL);
        }
        
        file_icon->priv->pix = g_object_ref(G_OBJECT(xfdesktop_fallback_icon));
        file_icon->priv->cur_pix_size = size;
    }
    
    return file_icon->priv->pix;
}

static G_CONST_RETURN gchar *
xfdesktop_file_icon_peek_label(XfdesktopIcon *icon)
{
    return XFDESKTOP_FILE_ICON(icon)->priv->info->display_name;
}

static void
xfdesktop_file_icon_set_position(XfdesktopIcon *icon,
                                 gint16 row,
                                 gint16 col)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    
    file_icon->priv->row = row;
    file_icon->priv->col = col;
    
    g_signal_emit(G_OBJECT(file_icon), __signals[SIG_POS_CHANGED], 0, NULL);
}

static gboolean
xfdesktop_file_icon_get_position(XfdesktopIcon *icon,
                                 gint16 *row,
                                 gint16 *col)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    
    *row = file_icon->priv->row;
    *col = file_icon->priv->col;
    
    return TRUE;
}

static void
xfdesktop_file_icon_set_extents(XfdesktopIcon *icon,
                                const GdkRectangle *extents)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    
    memcpy(&file_icon->priv->extents, extents, sizeof(GdkRectangle));
}

static gboolean
xfdesktop_file_icon_get_extents(XfdesktopIcon *icon,
                                GdkRectangle *extents)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    
    if(file_icon->priv->extents.width > 0
       && file_icon->priv->extents.height > 0)
    {
        memcpy(extents, &file_icon->priv->extents, sizeof(GdkRectangle));
        return TRUE;
    }
    
    return FALSE;
}

static gboolean
xfdesktop_file_icon_is_drop_dest(XfdesktopIcon *icon)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    return (file_icon->priv->info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY
            || file_icon->priv->info->flags & THUNAR_VFS_FILE_FLAGS_EXECUTABLE);
}

static void
xfdesktop_file_icon_drag_job_error(ThunarVfsJob *job,
                                   GError *error,
                                   gpointer user_data)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(user_data);
    XfdesktopFileIcon *src_file_icon = g_object_get_data(G_OBJECT(job),
                                                         "--xfdesktop-src-file-icon");
    GdkDragAction action = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(job),
                                                             "--xfdesktop-file-icon-action"));
    gchar *primary;
    
    g_return_if_fail(file_icon && src_file_icon);
    
    if(error) {
        primary = g_strdup_printf(_("There was an error %s \"%s\" to \"%s\":"),
                                  action == GDK_ACTION_MOVE
                                  ? _("moving")
                                  : (action == GDK_ACTION_COPY
                                     ? _("copying")
                                     : _("linking")),
                                  src_file_icon->priv->info->display_name,
                                  file_icon->priv->info->display_name);
        xfce_message_dialog(NULL, _("File Error"), GTK_STOCK_DIALOG_ERROR,
                            primary, error->message,
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        g_free(primary);
    }
}

static void
xfdesktop_file_icon_drag_job_finished(ThunarVfsJob *job,
                                      gpointer user_data)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(user_data);
    XfdesktopFileIcon *src_file_icon = g_object_get_data(G_OBJECT(job),
                                                         "--xfdesktop-src-file-icon");
    
    if(g_list_find(file_icon->priv->active_jobs, job)) {
        file_icon->priv->active_jobs = g_list_remove(file_icon->priv->active_jobs,
                                                     job);
    } else
        g_critical("ThunarVfsJob 0x%p not found in active jobs list", job);
    
    if(g_list_find(src_file_icon->priv->active_jobs, job)) {
        src_file_icon->priv->active_jobs = g_list_remove(src_file_icon->priv->active_jobs,
                                                         job);
    } else
        g_critical("ThunarVfsJob 0x%p not found in active jobs list", job);
    
    /* yes, twice is correct */
    g_object_unref(G_OBJECT(job));
    g_object_unref(G_OBJECT(job));
}

static XfdesktopIconDragResult
xfdesktop_file_icon_do_drop_dest(XfdesktopIcon *icon,
                                 XfdesktopIcon *src_icon,
                                 GdkDragAction action)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    XfdesktopFileIcon *src_file_icon = XFDESKTOP_FILE_ICON(src_icon);
    
    DBG("entering");
    
    g_return_val_if_fail(file_icon && src_file_icon,
                         XFDESKTOP_ICON_DRAG_FAILED);
    g_return_val_if_fail(xfdesktop_file_icon_is_drop_dest(icon),
                         XFDESKTOP_ICON_DRAG_FAILED);
    
    if(file_icon->priv->info->flags & THUNAR_VFS_FILE_FLAGS_EXECUTABLE) {
        GList *path_list = g_list_prepend(NULL, src_file_icon->priv->info->path);
        GError *error = NULL;
        gboolean succeeded;
        
        succeeded = thunar_vfs_info_execute(file_icon->priv->info,
                                            file_icon->priv->gscreen,
                                            path_list,
                                            xfce_get_homedir(),
                                            &error);
        g_list_free(path_list);
        
        if(!succeeded) {
            gchar *primary = g_strdup_printf(_("Failed to run \"%s\":"),
                                             file_icon->priv->info->display_name);
            xfce_message_dialog(NULL, _("Run Error"), GTK_STOCK_DIALOG_ERROR,
                                primary, error->message,
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
            g_free(primary);
            g_error_free(error);
            
            return XFDESKTOP_ICON_DRAG_FAILED;
        }
        
        return XFDESKTOP_ICON_DRAG_SUCCEEDED_NO_ACTION;
    } else {
        ThunarVfsJob *job = NULL;
        const gchar *name;
        ThunarVfsPath *dest_path;
        
        name = thunar_vfs_path_get_name(src_file_icon->priv->info->path);
        g_return_val_if_fail(name, XFDESKTOP_ICON_DRAG_FAILED);
        
        dest_path = thunar_vfs_path_relative(file_icon->priv->info->path,
                                             name);
        g_return_val_if_fail(dest_path, XFDESKTOP_ICON_DRAG_FAILED);
        
        switch(action) {
            case GDK_ACTION_MOVE:
                DBG("doing move");
                job = thunar_vfs_move_file(src_file_icon->priv->info->path,
                                           dest_path, NULL);
                break;
            
            case GDK_ACTION_COPY:
                DBG("doing copy");
                job = thunar_vfs_copy_file(src_file_icon->priv->info->path,
                                           dest_path, NULL);
                break;
            
            case GDK_ACTION_LINK:
                DBG("doing link");
                job = thunar_vfs_link_file(src_file_icon->priv->info->path,
                                           dest_path, NULL);
                break;
            
            default:
                g_warning("Unsupported drag action: %d", action);
        }
        
        thunar_vfs_path_unref(dest_path);
        
        if(job) {
            DBG("got job, action initiated");
            
            g_object_set_data(G_OBJECT(job), "--xfdesktop-file-icon-callback",
                              G_CALLBACK(xfdesktop_file_icon_drag_job_finished));
            g_object_set_data(G_OBJECT(job), "--xfdesktop-file-icon-data", icon);
            file_icon->priv->active_jobs = g_list_prepend(file_icon->priv->active_jobs,
                                                          job);
            src_file_icon->priv->active_jobs = g_list_prepend(src_file_icon->priv->active_jobs,
                                                              job);
            
            g_object_set_data(G_OBJECT(job), "--xfdesktop-src-file-icon",
                              src_file_icon);
            g_object_set_data(G_OBJECT(job), "--xfdesktop-file-icon-action",
                              GINT_TO_POINTER(action));
            g_signal_connect(G_OBJECT(job), "error",
                             G_CALLBACK(xfdesktop_file_icon_drag_job_error),
                             file_icon);
            g_signal_connect(G_OBJECT(job), "finished",
                             G_CALLBACK(xfdesktop_file_icon_drag_job_finished),
                             file_icon);
            
            g_object_ref(G_OBJECT(job));
            
            return XFDESKTOP_ICON_DRAG_SUCCEEDED_NO_ACTION;
        } else
            return XFDESKTOP_ICON_DRAG_FAILED;
    }
    
    return XFDESKTOP_ICON_DRAG_FAILED;
}

static void
xfdesktop_file_icon_selected(XfdesktopIcon *icon)
{
    /* nada */
}

static gboolean
xfdesktop_file_icon_launch_external(XfdesktopFileIcon *icon)
{
    gboolean ret = FALSE;
    gchar *folder_name = thunar_vfs_path_dup_string(icon->priv->info->path);
    gchar *display_name = gdk_screen_make_display_name(icon->priv->gscreen);
    gchar *commandline;
    gint status = 0;
    
    /* try the org.xfce.FileManager D-BUS interface first */
    commandline = g_strdup_printf("dbus-send --print-reply --dest=org.xfce.FileManager "
                                  "/org/xfce/FileManager org.xfce.FileManager.Launch "
                                  "string:\"%s\" string:\"%s\"", folder_name, display_name);
    ret = (g_spawn_command_line_sync(commandline, NULL, NULL, &status, NULL)
           && status == 0);
    g_free(commandline);

    /* hardcoded fallback to Thunar if that didn't work */
    if(!ret) {
        gchar *thunar_app = g_find_program_in_path("Thunar");
        
        if(thunar_app) {
            commandline = g_strconcat("env DISPLAY=\"", display_name, "\" ", thunar_app, " \"", folder_name, "\"", NULL);
            
            DBG("executing:\n%s\n", commandline);
            
            ret = xfce_exec(commandline, FALSE, TRUE, NULL);
            g_free(commandline);
            g_free(thunar_app);
        }
    }

    g_free(display_name);
    g_free(folder_name);
    
    return ret;
}

static void
xfdesktop_file_icon_activated(XfdesktopIcon *icon)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    ThunarVfsMimeApplication *mime_app;
    const ThunarVfsInfo *info = file_icon->priv->info;
    gboolean succeeded = FALSE;
    GList *path_list = g_list_prepend(NULL, info->path);
    
    TRACE("entering");
    
    if(info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY)
        succeeded = xfdesktop_file_icon_launch_external(file_icon);
    else if(info->flags & THUNAR_VFS_FILE_FLAGS_EXECUTABLE) {
        succeeded = thunar_vfs_info_execute(info,
                                            file_icon->priv->gscreen,
                                            NULL,
                                            xfce_get_homedir(),
                                            NULL);
    }
    
    if(!succeeded) {
        mime_app = thunar_vfs_mime_database_get_default_application(thunar_mime_database,
                                                                    info->mime_info);
        if(mime_app) {
            DBG("executing");
            
            succeeded = thunar_vfs_mime_handler_exec(THUNAR_VFS_MIME_HANDLER(mime_app),
                                                     file_icon->priv->gscreen,
                                                     path_list,
                                                     NULL); 
            g_object_unref(G_OBJECT(mime_app));
        } else
            succeeded = xfdesktop_file_icon_launch_external(file_icon);
    }    
    
    g_list_free(path_list);
}

static void
xfdesktop_file_icon_menu_rename(GtkWidget *widget,
                                gpointer user_data)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(user_data);
    GtkWidget *dlg, *entry, *lbl, *img, *hbox, *vbox, *topvbox;
    GdkPixbuf *pix;
    gchar *title, *p;
    gint w, h;
    
    title = g_strdup_printf(_("Rename \"%s\""), icon->priv->info->display_name);
    
    dlg = gtk_dialog_new_with_buttons(title, NULL, GTK_DIALOG_NO_SEPARATOR,
                                      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_ACCEPT);
    g_free(title);
    
    topvbox = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(topvbox), BORDER);
    gtk_widget_show(topvbox);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), topvbox, TRUE, TRUE, 0);
    
    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(topvbox), hbox, FALSE, FALSE, 0);
    
    gtk_icon_size_lookup(GTK_ICON_SIZE_DIALOG, &w, &h);
    pix = xfdesktop_file_icon_peek_pixbuf(XFDESKTOP_ICON(icon), w);
    if(pix) {
        img = gtk_image_new_from_pixbuf(pix);
        gtk_widget_show(img);
        gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
    }
    
    vbox = gtk_vbox_new(FALSE, BORDER);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
    
    lbl = gtk_label_new(_("Enter the new name:"));
    gtk_misc_set_alignment(GTK_MISC(lbl), 0.0, 0.5);
    gtk_widget_show(lbl);
    gtk_box_pack_start(GTK_BOX(vbox), lbl, FALSE, FALSE, 0);
    
    entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), icon->priv->info->display_name);
    if((p = g_utf8_strrchr(icon->priv->info->display_name, -1, '.'))) {
        gint offset = g_utf8_strlen(icon->priv->info->display_name, p - icon->priv->info->display_name);
        gtk_editable_set_position(GTK_EDITABLE(entry), offset);
        gtk_editable_select_region(GTK_EDITABLE(entry), 0, offset);
    }
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_widget_show(entry);
    gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);
    
    xfce_gtk_window_center_on_monitor_with_pointer(GTK_WINDOW(dlg));
    if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dlg))) {
        gchar *new_name;
        GError *error = NULL;
        
        new_name = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
        
        // FIXME: Need to re-register with the VFS monitor after successfull rename
        if(!thunar_vfs_info_rename(icon->priv->info, new_name, &error)) {
            gchar *primary = g_strdup_printf(_("Failed to rename \"%s\" to \"%s\":"),
                                               icon->priv->info->display_name, new_name);
            xfce_message_dialog(NULL, _("Error"), GTK_STOCK_DIALOG_ERROR,
                                primary, error->message,
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
            g_free(primary);
        }
        
        g_free(new_name);
    }
    
    gtk_widget_destroy(dlg);
}

static void
xfdesktop_delete_file_error(ThunarVfsJob *job,
                            GError *error,
                            gpointer user_data)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(user_data);
    gchar *primary = g_strdup_printf("There was an error deleting \"%s\":", icon->priv->info->display_name);
                                     
    xfce_message_dialog(NULL, _("Error"), GTK_STOCK_DIALOG_ERROR, primary,
                        error->message, GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                        NULL);
    
    g_free(primary);
}

static void
xfdesktop_delete_file_finished(ThunarVfsJob *job,
                               gpointer user_data)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(user_data);
    
    if(g_list_find(icon->priv->active_jobs, job))
        icon->priv->active_jobs = g_list_remove(icon->priv->active_jobs, job);
    else
        g_critical("ThunarVfsJob 0x%p not found in active jobs list", job);
    
    g_object_unref(G_OBJECT(job));
}

void
xfdesktop_file_icon_delete_file(XfdesktopFileIcon *icon)
{
    ThunarVfsJob *job;
    
    job = thunar_vfs_unlink_file(icon->priv->info->path, NULL);
    
    g_object_set_data(G_OBJECT(job), "--xfdesktop-file-icon-callback",
                      G_CALLBACK(xfdesktop_delete_file_finished));
    g_object_set_data(G_OBJECT(job), "--xfdesktop-file-icon-data", icon);
    icon->priv->active_jobs = g_list_prepend(icon->priv->active_jobs, job);
    
    g_signal_connect(G_OBJECT(job), "error",
                     G_CALLBACK(xfdesktop_delete_file_error), icon);
    g_signal_connect(G_OBJECT(job), "finished",
                     G_CALLBACK(xfdesktop_delete_file_finished), icon);
}

static void
xfdesktop_file_icon_menu_delete(GtkWidget *widget,
                                gpointer user_data)
{
    /* WARNING: do not use |widget| in this function, as it could be NULL */
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(user_data);
    gchar *primary;
    gint ret;
    
    primary = g_strdup_printf("Are you sure that you want to permanently delete \"%s\"?",
                              icon->priv->info->display_name);
    ret = xfce_message_dialog(NULL, _("Question"), GTK_STOCK_DIALOG_QUESTION,
                              primary,
                              _("If you delete a file, it is permanently lost."),
                              GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                              GTK_STOCK_DELETE, GTK_RESPONSE_ACCEPT, NULL);
    g_free(primary);
    if(GTK_RESPONSE_ACCEPT == ret)
        xfdesktop_file_icon_delete_file(icon);
}

void
xfdesktop_file_icon_trigger_delete(XfdesktopFileIcon *icon)
{
    xfdesktop_file_icon_menu_delete(NULL, icon);
}

static void
xfdesktop_file_icon_menu_executed(GtkWidget *widget,
                                  gpointer user_data)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(user_data);
    ThunarVfsMimeApplication *mime_app;
    GList *path_list = g_list_append(NULL, icon->priv->info->path);
    
    mime_app = g_object_get_qdata(G_OBJECT(widget), xfdesktop_mime_app_quark);
    g_return_if_fail(mime_app);
    
    thunar_vfs_mime_handler_exec(THUNAR_VFS_MIME_HANDLER(mime_app),
                             icon->priv->gscreen,
                             path_list,
                             NULL);
    
    g_list_free(path_list);
}

static GtkWidget *
xfdesktop_menu_item_from_mime_app(XfdesktopFileIcon *icon,
                                  ThunarVfsMimeApplication *mime_app,
                                  gint icon_size,
                                  gboolean with_mnemonic)
{
    GtkWidget *mi, *img;
    gchar *title;
    const gchar *icon_name;
    
    if(with_mnemonic) {
        title = g_strdup_printf(_("_Open With \"%s\""),
                                thunar_vfs_mime_application_get_name(mime_app));
    } else {
        title = g_strdup_printf(_("Open With \"%s\""),
                                thunar_vfs_mime_application_get_name(mime_app));
    }
    icon_name = thunar_vfs_mime_handler_lookup_icon_name(THUNAR_VFS_MIME_HANDLER(mime_app),
                                                         gtk_icon_theme_get_default());
    
    if(with_mnemonic)
        mi = gtk_image_menu_item_new_with_mnemonic(title);
    else
        mi = gtk_image_menu_item_new_with_label(title);
    g_free(title);
    
    g_object_set_qdata_full(G_OBJECT(mi), xfdesktop_mime_app_quark, mime_app,
                            (GDestroyNotify)g_object_unref);
    
    if(icon_name) {
        GdkPixbuf *pix = xfce_themed_icon_load(icon_name, icon_size);
        if(pix) {
            img = gtk_image_new_from_pixbuf(pix);
            g_object_unref(G_OBJECT(pix));
            gtk_widget_show(img);
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi),
                                          img);
        }
    }
    
    gtk_widget_show(mi);
    
    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_file_icon_menu_executed), icon);
    
    return mi;
}

static void
xfdesktop_file_icon_menu_other_app(GtkWidget *widget,
                                   gpointer user_data)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(user_data);
    
    if(!xfdesktop_file_icon_launch_external(icon)) {
        gchar *primary = g_strdup_printf(_("Unable to launch \"%s\"."),
                                         icon->priv->info->display_name);
        xfce_message_dialog(NULL, _("Error"), GTK_STOCK_DIALOG_ERROR,
                            primary,
                            _("This feature requires a file manager service present (such as that supplied by Thunar)."),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        g_free(primary);
    }
}

static void
xfdesktop_file_icon_menu_cut(GtkWidget *widget,
                             gpointer user_data)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(user_data);
    GList *files = g_list_prepend(NULL, icon);
    
    xfdesktop_clipboard_manager_cut_files(clipboard_manager, files);
    
    g_list_free(files);
}

static void
xfdesktop_file_icon_menu_copy(GtkWidget *widget,
                              gpointer user_data)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(user_data);
    GList *files = g_list_prepend(NULL, icon);
    
    xfdesktop_clipboard_manager_copy_files(clipboard_manager, files);
    
    g_list_free(files);
}

static void
xfdesktop_file_icon_menu_properties(GtkWidget *widget,
                                    gpointer user_data)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(user_data);
    GtkWidget *dlg, *table, *hbox, *lbl, *img, *spacer, *notebook, *vbox;
    gint row = 0, w, h;
    PangoFontDescription *pfd = pango_font_description_from_string("bold");
    gchar *str = NULL, buf[64];
    struct tm *tm;
    ThunarVfsUserManager *user_manager;
    ThunarVfsUser *user;
    ThunarVfsGroup *group;
    ThunarVfsFileMode mode;
    ThunarVfsMimeApplication *mime_app;
    static const gchar *access_types[4] = {
        N_("None"), N_("Write only"), N_("Read only"), N_("Read & Write")
    };
    
    gtk_icon_size_lookup(GTK_ICON_SIZE_DIALOG, &w, &h);
    
    dlg = gtk_dialog_new_with_buttons(xfdesktop_file_icon_peek_label(XFDESKTOP_ICON(icon)),
                                      NULL,
                                      GTK_DIALOG_NO_SEPARATOR,
                                      GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                                      NULL);
    gtk_window_set_icon(GTK_WINDOW(dlg),
                        xfdesktop_file_icon_peek_pixbuf(XFDESKTOP_ICON(icon), w));
    g_signal_connect(GTK_DIALOG(dlg), "response",
                     G_CALLBACK(gtk_widget_destroy), NULL);
    
    notebook = gtk_notebook_new();
    gtk_widget_show(notebook);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), notebook, TRUE, TRUE, 0);
    
    lbl = gtk_label_new(_("General"));
    gtk_widget_show(lbl);
    vbox = gtk_vbox_new(FALSE, BORDER);
    gtk_widget_show(vbox);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, lbl);
    
    table = g_object_new(GTK_TYPE_TABLE,
                         "border-width", 6,
                         "column-spacing", 12,
                         "row-spacing", 6,
                         NULL);
    gtk_widget_show(table);
    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);
    
    
    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_widget_show(hbox);
    gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, row, row + 1,
                     GTK_FILL, GTK_FILL, 0, 0);
    
    img = gtk_image_new_from_pixbuf(xfdesktop_file_icon_peek_pixbuf(XFDESKTOP_ICON(icon),
                                                                    w));
    gtk_widget_show(img);
    gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
    
    lbl = gtk_label_new(_("Name:"));
    gtk_misc_set_alignment(GTK_MISC(lbl), 1.0, 0.5);
    gtk_widget_modify_font(lbl, pfd);
    gtk_widget_show(lbl);
    gtk_box_pack_start(GTK_BOX(hbox), lbl, TRUE, TRUE, 0);
    
    lbl = gtk_label_new(xfdesktop_file_icon_peek_label(XFDESKTOP_ICON(icon)));
    gtk_misc_set_alignment(GTK_MISC(lbl), 0.0, 0.5);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 1, 2, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    
    ++row;
    
    spacer = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
    gtk_widget_set_size_request(spacer, -1, 12);
    gtk_widget_show(spacer);
    gtk_table_attach(GTK_TABLE(table), spacer, 0, 1, row, row + 1,
                     GTK_FILL, GTK_FILL, 0, 0);
    
    ++row;
    
    lbl = gtk_label_new(_("Kind:"));
    gtk_misc_set_alignment(GTK_MISC(lbl), 1.0, 0.5);
    gtk_widget_modify_font(lbl, pfd);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 0, 1, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    
    if(!strcmp(thunar_vfs_mime_info_get_name(icon->priv->info->mime_info),
               "inode/symlink"))
    {
        str = g_strdup(_("broken link"));
    } else if(icon->priv->info->type == THUNAR_VFS_FILE_TYPE_SYMLINK) {
        str = g_strdup_printf(_("link to %s"),
                              thunar_vfs_mime_info_get_comment(icon->priv->info->mime_info));
    } else
        str = g_strdup(thunar_vfs_mime_info_get_comment(icon->priv->info->mime_info));
    lbl = gtk_label_new(str);
    g_free(str);
    gtk_misc_set_alignment(GTK_MISC(lbl), 0.0, 0.5);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 1, 2, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    
    ++row;
    
    mime_app = thunar_vfs_mime_database_get_default_application(thunar_mime_database,
                                                                icon->priv->info->mime_info);
    
    if(mime_app) {
        const gchar *icon_name;
        
        lbl = gtk_label_new(_("Open With:"));
        gtk_misc_set_alignment(GTK_MISC(lbl), 1.0, 0.5);
        gtk_widget_modify_font(lbl, pfd);
        gtk_widget_show(lbl);
        gtk_table_attach(GTK_TABLE(table), lbl, 0, 1, row, row + 1,
                         GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
        
        hbox = gtk_hbox_new(FALSE, BORDER);
        gtk_widget_show(hbox);
        gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, row, row + 1,
                         GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
        
        icon_name = thunar_vfs_mime_handler_lookup_icon_name(THUNAR_VFS_MIME_HANDLER(mime_app),
                                                             gtk_icon_theme_get_default());
        if(icon_name) {
            gint w, h;
            GdkPixbuf *pix;
            
            gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, &h);
            
            pix = xfce_themed_icon_load(icon_name, w);
            if(pix) {
                img = gtk_image_new_from_pixbuf(pix);
                gtk_widget_show(img);
                gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
                g_object_unref(G_OBJECT(pix));
            }
        }
        
        lbl = gtk_label_new(thunar_vfs_mime_application_get_name(mime_app));
        gtk_misc_set_alignment(GTK_MISC(lbl), 0.0, 0.5);
        gtk_widget_show(lbl);
        gtk_box_pack_start(GTK_BOX(hbox), lbl, TRUE, TRUE, 0);
        
        ++row;
    }
    
    spacer = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
    gtk_widget_set_size_request(spacer, -1, 12);
    gtk_widget_show(spacer);
    gtk_table_attach(GTK_TABLE(table), spacer, 0, 1, row, row + 1,
                     GTK_FILL, GTK_FILL, 0, 0);
    
    ++row;
    
    lbl = gtk_label_new(_("Modified:"));
    gtk_misc_set_alignment(GTK_MISC(lbl), 1.0, 0.5);
    gtk_widget_modify_font(lbl, pfd);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 0, 1, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    
    tm = localtime(&icon->priv->info->mtime);
    strftime(buf, 64, "%Y-%m-%d %H:%M:%S", tm);
    
    lbl = gtk_label_new(buf);
    gtk_misc_set_alignment(GTK_MISC(lbl), 0.0, 0.5);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 1, 2, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    
    ++row;
    
    lbl = gtk_label_new(_("Accessed:"));
    gtk_misc_set_alignment(GTK_MISC(lbl), 1.0, 0.5);
    gtk_widget_modify_font(lbl, pfd);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 0, 1, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    
    tm = localtime(&icon->priv->info->atime);
    strftime(buf, 64, "%Y-%m-%d %H:%M:%S", tm);
    
    lbl = gtk_label_new(buf);
    gtk_misc_set_alignment(GTK_MISC(lbl), 0.0, 0.5);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 1, 2, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    
    ++row;
    
    spacer = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
    gtk_widget_set_size_request(spacer, -1, 12);
    gtk_widget_show(spacer);
    gtk_table_attach(GTK_TABLE(table), spacer, 0, 1, row, row + 1,
                     GTK_FILL, GTK_FILL, 0, 0);
    
    ++row;
    
    if(icon->priv->info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY)
        lbl = gtk_label_new(_("Free Space:"));
    else
        lbl = gtk_label_new(_("Size:"));
    gtk_misc_set_alignment(GTK_MISC(lbl), 1.0, 0.5);
    gtk_widget_modify_font(lbl, pfd);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 0, 1, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    
    if(icon->priv->info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY) {
        ThunarVfsFileSize free_space;
        if(thunar_vfs_info_get_free_space(icon->priv->info, &free_space)) {
            thunar_vfs_humanize_size(free_space, buf, 64);
            lbl = gtk_label_new(buf);
        } else
            lbl = gtk_label_new(_("unknown"));
    } else {
        thunar_vfs_humanize_size(icon->priv->info->size, buf, 64);
        str = g_strdup_printf(_("%s (%" G_GINT64_FORMAT " Bytes)"), buf,
                              (gint64)icon->priv->info->size);
        lbl = gtk_label_new(str);
        g_free(str);
    }
    gtk_misc_set_alignment(GTK_MISC(lbl), 0.0, 0.5);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 1, 2, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    
    ++row;
    
    
    /* permissions tab */
    
    lbl = gtk_label_new(_("Permissions"));;
    gtk_widget_show(lbl);
    vbox = gtk_vbox_new(FALSE, BORDER);
    gtk_widget_show(vbox);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, lbl);
    
    row = 0;
    table = g_object_new(GTK_TYPE_TABLE,
                         "border-width", 6,
                         "column-spacing", 12,
                         "row-spacing", 6,
                         NULL);
    gtk_widget_show(table);
    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);
    
    user_manager = thunar_vfs_user_manager_get_default();
    user = thunar_vfs_user_manager_get_user_by_id(user_manager,
                                                  icon->priv->info->uid);
    group = thunar_vfs_user_manager_get_group_by_id(user_manager,
                                                    icon->priv->info->gid);
    mode = icon->priv->info->mode;
    
    lbl = gtk_label_new(_("Owner:"));
    gtk_misc_set_alignment(GTK_MISC(lbl), 1.0, 0.5);
    gtk_widget_modify_font(lbl, pfd);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 0, 1, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    
    str = g_strdup_printf("%s (%s)", thunar_vfs_user_get_real_name(user),
                          thunar_vfs_user_get_name(user));
    lbl = gtk_label_new(str);
    g_free(str);
    gtk_misc_set_alignment(GTK_MISC(lbl), 0.0, 0.5);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 1, 2, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    
    ++row;
    
    lbl = gtk_label_new(_("Access:"));
    gtk_misc_set_alignment(GTK_MISC(lbl), 1.0, 0.5);
    gtk_widget_modify_font(lbl, pfd);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 0, 1, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    
    lbl = gtk_label_new(_(access_types[((mode >> (2 * 3)) & 0007) >> 1]));
    gtk_misc_set_alignment(GTK_MISC(lbl), 0.0, 0.5);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 1, 2, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    
    ++row;
    
    spacer = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
    gtk_widget_set_size_request(spacer, -1, 12);
    gtk_widget_show(spacer);
    gtk_table_attach(GTK_TABLE(table), spacer, 0, 1, row, row + 1,
                     GTK_FILL, GTK_FILL, 0, 0);
    
    ++row;
    
    lbl = gtk_label_new(_("Group:"));
    gtk_misc_set_alignment(GTK_MISC(lbl), 1.0, 0.5);
    gtk_widget_modify_font(lbl, pfd);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 0, 1, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    
    lbl = gtk_label_new(thunar_vfs_group_get_name(group));
    gtk_misc_set_alignment(GTK_MISC(lbl), 0.0, 0.5);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 1, 2, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    
    ++row;
    
    lbl = gtk_label_new(_("Access:"));
    gtk_misc_set_alignment(GTK_MISC(lbl), 1.0, 0.5);
    gtk_widget_modify_font(lbl, pfd);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 0, 1, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    
    lbl = gtk_label_new(_(access_types[((mode >> (1 * 3)) & 0007) >> 1]));
    gtk_misc_set_alignment(GTK_MISC(lbl), 0.0, 0.5);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 1, 2, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    
    ++row;
    
    spacer = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
    gtk_widget_set_size_request(spacer, -1, 12);
    gtk_widget_show(spacer);
    gtk_table_attach(GTK_TABLE(table), spacer, 0, 1, row, row + 1,
                     GTK_FILL, GTK_FILL, 0, 0);
    
    ++row;
    
    lbl = gtk_label_new(_("Others:"));
    gtk_misc_set_alignment(GTK_MISC(lbl), 1.0, 0.5);
    gtk_widget_modify_font(lbl, pfd);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 0, 1, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    
    lbl = gtk_label_new(_(access_types[((mode >> (0 * 3)) & 0007) >> 1]));
    gtk_misc_set_alignment(GTK_MISC(lbl), 0.0, 0.5);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 1, 2, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    
    ++row;
    
    g_object_unref(G_OBJECT(user_manager));
    g_object_unref(G_OBJECT(user));
    g_object_unref(G_OBJECT(group));
    
    pango_font_description_free(pfd);
    
    gtk_widget_show(dlg);
}

static gboolean
xfdesktop_file_icon_menu_deactivate_idled(gpointer user_data)
{
    gtk_widget_destroy(GTK_WIDGET(user_data));
    return FALSE;
}

static void
xfdesktop_file_icon_menu_popup(XfdesktopIcon *icon)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    ThunarVfsInfo *info = file_icon->priv->info;
    ThunarVfsMimeInfo *mime_info = info->mime_info;
    GList *mime_apps, *l;
    GtkWidget *menu, *mi, *img;
    
    menu = gtk_menu_new();
    gtk_widget_show(menu);
    g_signal_connect_swapped(G_OBJECT(menu), "deactivate",
                             G_CALLBACK(g_idle_add),
                             xfdesktop_file_icon_menu_deactivate_idled);
    
    if(info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY) {
        img = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
        gtk_widget_show(img);
        mi = gtk_image_menu_item_new_with_mnemonic(_("_Open"));
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        g_signal_connect_swapped(G_OBJECT(mi), "activate",
                                 G_CALLBACK(xfdesktop_file_icon_launch_external),
                                 file_icon);
    } else {
        if(info->flags & THUNAR_VFS_FILE_FLAGS_EXECUTABLE) {
            img = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
            gtk_widget_show(img);
            mi = gtk_image_menu_item_new_with_mnemonic(_("_Execute"));
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        }
        
        mime_apps = thunar_vfs_mime_database_get_applications(thunar_mime_database,
                                                              mime_info);
        if(mime_apps) {
            gint w, h;
            ThunarVfsMimeApplication *mime_app = mime_apps->data;
            
            if(info->flags & THUNAR_VFS_FILE_FLAGS_EXECUTABLE) {
                mi = gtk_separator_menu_item_new();
                gtk_widget_show(mi);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            }
            
            gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, &h);
            
            mi = xfdesktop_menu_item_from_mime_app(file_icon, mime_app, w,
                                                   TRUE);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            
            if(mime_apps->next) {
                GtkWidget *mime_apps_menu;
                
                if(!(info->flags & THUNAR_VFS_FILE_FLAGS_EXECUTABLE)) {
                    mi = gtk_separator_menu_item_new();
                    gtk_widget_show(mi);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                }
                
                if(g_list_length(mime_apps->next) > 3) {
                    mi = gtk_menu_item_new_with_label(_("Open With"));
                    gtk_widget_show(mi);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                    
                    mime_apps_menu = gtk_menu_new();
                    gtk_widget_show(mime_apps_menu);
                    gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi),
                                              mime_apps_menu);
                } else
                    mime_apps_menu = menu;
                
                for(l = mime_apps->next; l; l = l->next) {
                    mime_app = l->data;
                    mi = xfdesktop_menu_item_from_mime_app(file_icon,
                                                           mime_app, w,
                                                           FALSE);
                    gtk_menu_shell_append(GTK_MENU_SHELL(mime_apps_menu), mi);
                }
            }
            
            /* don't free the mime apps!  just the list! */
            g_list_free(mime_apps);
        } else {
            img = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
            gtk_widget_show(img);
            mi = gtk_image_menu_item_new_with_mnemonic(_("Open With Other _Application..."));
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            g_signal_connect(G_OBJECT(mi), "activate",
                             G_CALLBACK(xfdesktop_file_icon_menu_other_app),
                             file_icon);
        }
    }
    
    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    
    img = gtk_image_new_from_stock(GTK_STOCK_COPY, GTK_ICON_SIZE_MENU);
    gtk_widget_show(img);
    mi = gtk_image_menu_item_new_with_mnemonic(_("_Copy File"));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_file_icon_menu_copy), file_icon);
    
    img = gtk_image_new_from_stock(GTK_STOCK_CUT, GTK_ICON_SIZE_MENU);
    gtk_widget_show(img);
    mi = gtk_image_menu_item_new_with_mnemonic(_("Cu_t File"));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_file_icon_menu_cut), file_icon);
    
    img = gtk_image_new_from_stock(GTK_STOCK_DELETE, GTK_ICON_SIZE_MENU);
    gtk_widget_show(img);
    mi = gtk_image_menu_item_new_with_mnemonic(_("_Delete File"));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_file_icon_menu_delete), file_icon);
    
    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    
    mi = gtk_image_menu_item_new_with_mnemonic(_("_Rename..."));
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_file_icon_menu_rename), file_icon);
    
    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    
    img = gtk_image_new_from_stock(GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_MENU);
    gtk_widget_show(img);
    mi = gtk_image_menu_item_new_with_mnemonic(_("_Properties..."));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_file_icon_menu_properties), file_icon);
    
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0,
                   gtk_get_current_event_time());
}

void
xfdesktop_file_icon_update_info(XfdesktopFileIcon *icon,
                                ThunarVfsInfo *info)
{
    g_return_if_fail(XFDESKTOP_IS_ICON(icon) && info);
    
    thunar_vfs_info_unref(icon->priv->info);
    icon->priv->info = thunar_vfs_info_ref(info);
    
    /* FIXME: force redraw of icon? */
}

GList *
xfdesktop_file_icon_list_to_path_list(GList *icon_list)
{
    GList *path_list = NULL, *l;
    
    for(l = icon_list; l; l = l->next) {
        path_list = g_list_prepend(path_list,
                                   thunar_vfs_path_ref(XFDESKTOP_FILE_ICON(l->data)->priv->info->path));
    }
    
    return g_list_reverse(path_list);
}
