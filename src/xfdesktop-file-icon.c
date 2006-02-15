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

static void xfdesktop_file_icon_selected(XfdesktopIcon *icon);
static void xfdesktop_file_icon_activated(XfdesktopIcon *icon);
static void xfdesktop_file_icon_menu_popup(XfdesktopIcon *icon);

static void xfdesktop_delete_file_finished(ThunarVfsJob *job,
                                           gpointer user_data);


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
    
    if(icon->priv->active_jobs) {
        GList *l;
        ThunarVfsJob *job;
        for(l = icon->priv->active_jobs; l; l = l->next) {
            job = THUNAR_VFS_JOB(l->data);
            g_signal_handlers_disconnect_by_func(G_OBJECT(job),
                                                 G_CALLBACK(xfdesktop_delete_file_finished),
                                                 icon);
            thunar_vfs_job_cancel(job);
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
    iface->selected = xfdesktop_file_icon_selected;
    iface->activated = xfdesktop_file_icon_activated;
    iface->menu_popup = xfdesktop_file_icon_menu_popup;
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
    
    title = g_strconcat(with_mnemonic ? _("_Open With ") : _("Open With "),
                        thunar_vfs_mime_application_get_name(mime_app),
                        NULL);
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
    GtkWidget *dlg, *table, *hbox, *lbl, *img, *spacer;
    gint row = 0, w, h;
    PangoFontDescription *pfd = pango_font_description_from_string("bold");
    gchar *str = NULL;
    
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
    
    table = gtk_table_new(3, 2, TRUE);
    g_object_set(G_OBJECT(table),
                 "border-width", 6,
                 "column-spacing", 12,
                 "row-spacing", 6,
                 NULL);
    gtk_widget_show(table);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), table, TRUE, TRUE, 0);
    
    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_widget_show(hbox);
    gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, row, row + 1,
                     GTK_FILL, GTK_FILL, 0, 0);
    
    img = gtk_image_new_from_pixbuf(xfdesktop_file_icon_peek_pixbuf(XFDESKTOP_ICON(icon),
                                                                    w));
    gtk_widget_show(img);
    gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, TRUE, 0);
    
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
    
    /* FIXME: finish this. */
    
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
            
            gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, &h);
            
            mi = xfdesktop_menu_item_from_mime_app(file_icon, mime_app, w,
                                                   TRUE);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            
            if(mime_apps->next) {
                mi = gtk_separator_menu_item_new();
                gtk_widget_show(mi);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                
                for(l = mime_apps->next; l; l = l->next) {
                    mime_app = l->data;
                    mi = xfdesktop_menu_item_from_mime_app(file_icon,
                                                           mime_app, w,
                                                           FALSE);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
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
