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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include <glib-object.h>
#include <gdk/gdkkeysyms.h>

#ifdef HAVE_THUNARX
#include <thunarx/thunarx.h>
#endif

#include "xfdesktop-icon-view.h"
#include "xfdesktop-file-utils.h"
#include "xfdesktop-file-icon.h"
#include "xfdesktop-clipboard-manager.h"
#include "xfdesktop-file-icon-manager.h"

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#define SAVE_DELAY 7000
#define BORDER     8


static void xfdesktop_file_icon_manager_set_property(GObject *object,
                                                     guint property_id,
                                                     const GValue *value,
                                                     GParamSpec *pspec);
static void xfdesktop_file_icon_manager_get_property(GObject *object,
                                                     guint property_id,
                                                     GValue *value,
                                                     GParamSpec *pspec);
static void xfdesktop_file_icon_manager_finalize(GObject *obj);
static void xfdesktop_file_icon_manager_icon_view_manager_init(XfdesktopIconViewManagerIface *iface);

static gboolean xfdesktop_file_icon_manager_real_init(XfdesktopIconViewManager *manager,
                                                      XfdesktopIconView *icon_view);
static void xfdesktop_file_icon_manager_fini(XfdesktopIconViewManager *manager);

static gboolean xfdesktop_file_icon_manager_drag_drop(XfdesktopIconViewManager *manager,
                                                      XfdesktopIcon *drop_icon,
                                                      GdkDragContext *context,
                                                      guint16 row,
                                                      guint16 col,
                                                      guint time);
static void xfdesktop_file_icon_manager_drag_data_received(XfdesktopIconViewManager *manager,
                                                           XfdesktopIcon *drop_icon,
                                                           GdkDragContext *context,
                                                           guint16 row,
                                                           guint16 col,
                                                           GtkSelectionData *data,
                                                           guint info,
                                                           guint time);
static void xfdesktop_file_icon_manager_drag_data_get(XfdesktopIconViewManager *manager,
                                                      GList *drag_icons,
                                                      GdkDragContext *context,
                                                      GtkSelectionData *data,
                                                      guint info,
                                                      guint time);

static void xfdesktop_file_icon_manager_load_desktop_folder(XfdesktopFileIconManager *fmanager);
static void xfdesktop_file_icon_manager_load_removable_media(XfdesktopFileIconManager *fmanager);
static void xfdesktop_file_icon_manager_remove_removable_media(XfdesktopFileIconManager *fmanager);

enum
{
    PROP0 = 0,
    PROP_FOLDER,
};

struct _XfdesktopFileIconManagerPrivate
{
    gboolean inited;
    
    XfdesktopIconView *icon_view;
    
    GdkScreen *gscreen;
    
    ThunarVfsPath *folder;
    ThunarVfsMonitor *monitor;
    ThunarVfsMonitorHandle *handle;
    ThunarVfsJob *list_job;
    
    GHashTable *icons;
    GHashTable *removable_icons;
    
    gboolean show_removable_media;
    
    GList *icons_to_save;
    guint save_icons_id;
    
    GList *deferred_icons;
    
    GtkTargetList *drag_targets;
    GtkTargetList *drop_targets;
    
#ifdef HAVE_THUNARX
    GList *thunarx_menu_providers;
    GList *thunarx_properties_providers;
#endif
};


G_DEFINE_TYPE_EXTENDED(XfdesktopFileIconManager,
                       xfdesktop_file_icon_manager,
                       G_TYPE_OBJECT, 0,
                       G_IMPLEMENT_INTERFACE(XFDESKTOP_TYPE_ICON_VIEW_MANAGER,
                                             xfdesktop_file_icon_manager_icon_view_manager_init))

enum
{
    TARGET_TEXT_URI_LIST = 0,
    TARGET_XDND_DIRECT_SAVE0,
    TARGET_NETSCAPE_URL,
};

static const GtkTargetEntry drag_targets[] = {
    { "text/uri-list", 0, TARGET_TEXT_URI_LIST, },
};
static const gint n_drag_targets = (sizeof(drag_targets)/sizeof(drag_targets[0]));
static const GtkTargetEntry drop_targets[] = {
    { "text/uri-list", 0, TARGET_TEXT_URI_LIST, },
    { "XdndDirectSave0", 0, TARGET_XDND_DIRECT_SAVE0, },
    { "_NETSCAPE_URL", 0, TARGET_NETSCAPE_URL },
};
static const gint n_drop_targets = (sizeof(drop_targets)/sizeof(drop_targets[0]));

static XfdesktopClipboardManager *clipboard_manager = NULL;
static ThunarVfsMimeDatabase *thunar_mime_database = NULL;
static ThunarVfsVolumeManager *thunar_volume_manager = NULL;

static GQuark xfdesktop_mime_app_quark = 0;


static void
xfdesktop_file_icon_manager_class_init(XfdesktopFileIconManagerClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    
    gobject_class->set_property = xfdesktop_file_icon_manager_set_property;
    gobject_class->get_property = xfdesktop_file_icon_manager_get_property;
    gobject_class->finalize = xfdesktop_file_icon_manager_finalize;
    
    g_object_class_install_property(gobject_class, PROP_FOLDER,
                                    g_param_spec_boxed("folder", "Thunar VFS Folder",
                                                       "Folder this icon manager manages",
                                                       THUNAR_VFS_TYPE_PATH,
                                                       G_PARAM_READWRITE
                                                       | G_PARAM_CONSTRUCT_ONLY));
    
    xfdesktop_mime_app_quark = g_quark_from_static_string("xfdesktop-mime-app-quark");
}

static void
xfdesktop_file_icon_manager_init(XfdesktopFileIconManager *fmanager)
{
    fmanager->priv = g_new0(XfdesktopFileIconManagerPrivate, 1);
    
    /* be safe */
    fmanager->priv->gscreen = gdk_screen_get_default();
    fmanager->priv->drag_targets = gtk_target_list_new(drag_targets,
                                                       n_drag_targets);
    fmanager->priv->drop_targets = gtk_target_list_new(drop_targets,
                                                       n_drop_targets);
}

static void
xfdesktop_file_icon_manager_set_property(GObject *object,
                                         guint property_id,
                                         const GValue *value,
                                         GParamSpec *pspec)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(object);
    
    switch(property_id) {
        case PROP_FOLDER:
            fmanager->priv->folder = thunar_vfs_path_ref((ThunarVfsPath *)g_value_get_boxed(value));
            if(fmanager->priv->folder) {
                gchar *pathname = thunar_vfs_path_dup_string(fmanager->priv->folder);
                if(!g_file_test(pathname, G_FILE_TEST_EXISTS)) {
                    /* would prefer to use thunar_vfs_make_directory() here,
                     * but i don't want to use an async operation */
                    if(mkdir(pathname, 0700)) {
                        gchar *primary = g_strdup_printf(_("Xfdesktop was unable to create the folder \"%s\" to store desktop items:"),
                                                         pathname);
                        xfce_message_dialog(NULL, _("Create Folder Failed"),
                                            GTK_STOCK_DIALOG_WARNING, primary,
                                            strerror(errno), GTK_STOCK_CLOSE,
                                            GTK_RESPONSE_ACCEPT, NULL);
                        g_free(primary);
                    }
                } else if(!g_file_test(pathname, G_FILE_TEST_IS_DIR)) {
                    gchar *primary = g_strdup_printf(_("Xfdesktop is unable to use \"%s\" to hold desktop items because it is not a folder."),
                                                     pathname);
                    xfce_message_dialog(NULL, _("Create Folder Failed"),
                                        GTK_STOCK_DIALOG_WARNING, primary,
                                        _("Please delete or rename the file."),
                                        GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                                        NULL);
                    g_free(primary);
                }
                
                g_free(pathname);
            }
            break;
        
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
xfdesktop_file_icon_manager_get_property(GObject *object,
                                         guint property_id,
                                         GValue *value,
                                         GParamSpec *pspec)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(object);
    
    switch(property_id) {
        case PROP_FOLDER:
            g_value_set_boxed(value, fmanager->priv->folder);
            break;
        
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
xfdesktop_file_icon_manager_finalize(GObject *obj)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(obj);
    
    gtk_target_list_unref(fmanager->priv->drag_targets);
    gtk_target_list_unref(fmanager->priv->drop_targets);
    
    g_free(fmanager->priv);
    
    G_OBJECT_CLASS(xfdesktop_file_icon_manager_parent_class)->finalize(obj);
}

static void
xfdesktop_file_icon_manager_icon_view_manager_init(XfdesktopIconViewManagerIface *iface)
{
    iface->manager_init = xfdesktop_file_icon_manager_real_init;
    iface->manager_fini = xfdesktop_file_icon_manager_fini;
    iface->drag_drop = xfdesktop_file_icon_manager_drag_drop;
    iface->drag_data_received = xfdesktop_file_icon_manager_drag_data_received;
    iface->drag_data_get = xfdesktop_file_icon_manager_drag_data_get;
}


/* icon signal handlers */

static gboolean
xfdesktop_file_icon_launch_external(XfdesktopFileIcon *icon,
                                    GdkScreen *screen)
{
    gboolean ret = FALSE;
    const ThunarVfsInfo *info = xfdesktop_file_icon_peek_info(icon);
    gchar *folder_name = thunar_vfs_path_dup_string(info->path);
    gchar *display_name = gdk_screen_make_display_name(screen);
    gchar *commandline;
    gint status = 0;
    
    /* try the org.xfce.FileManager D-BUS interface first */
    commandline = g_strdup_printf("dbus-send --print-reply --dest=org.xfce.FileManager "
                                  "/org/xfce/FileManager org.xfce.FileManager.Launch "
                                  "string:\"%s\" string:\"%s\"", folder_name,
                                  display_name);
    ret = (g_spawn_command_line_sync(commandline, NULL, NULL, &status, NULL)
           && status == 0);
    g_free(commandline);

    /* hardcoded fallback to a file manager if that didn't work */
    if(!ret) {
        gchar *file_manager_app = g_find_program_in_path(FILE_MANAGER_FALLBACK);
        
        if(file_manager_app) {
            commandline = g_strconcat("env DISPLAY=\"", display_name, "\" \"",
                                      file_manager_app, "\" \"", folder_name,
                                      "\"", NULL);
            
            DBG("executing:\n%s\n", commandline);
            
            ret = xfce_exec(commandline, FALSE, TRUE, NULL);
            g_free(commandline);
            g_free(file_manager_app);
        }
    }

    g_free(display_name);
    g_free(folder_name);
    
    return ret;
}

static void
xfdesktop_file_icon_activated(XfdesktopIcon *icon,
                              gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    ThunarVfsMimeApplication *mime_app;
    const ThunarVfsInfo *info = xfdesktop_file_icon_peek_info(file_icon);
    ThunarVfsVolume *volume = xfdesktop_file_icon_peek_volume(file_icon);
    gboolean succeeded = FALSE;
    
    TRACE("entering");
    
    if(volume && !thunar_vfs_volume_is_mounted(volume)) {
        GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
        GError *error = NULL;
        
        if(thunar_vfs_volume_mount(volume, toplevel, &error)) {
            ThunarVfsPath *new_path = thunar_vfs_volume_get_mount_point(volume);
            
            if(!info || !thunar_vfs_path_equal(info->path, new_path)) {
                ThunarVfsInfo *new_info = thunar_vfs_info_new_for_path(new_path,
                                                                       NULL);
                if(new_info) {
                    xfdesktop_file_icon_update_info(file_icon, new_info);
                    thunar_vfs_info_unref(new_info);
                    info = new_info;
                }
            }
        }
        
        info = xfdesktop_file_icon_peek_info(file_icon);
        if(!info) {
            gchar *primary = g_strdup_printf(_("Unable to mount \"%s\":"),
                                             thunar_vfs_volume_get_name(volume));
            xfce_message_dialog(GTK_WINDOW(toplevel), _("Mount Failed"),
                                GTK_STOCK_DIALOG_ERROR, primary,
                                error ? error->message : _("Unknown error."),
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
            g_free(primary);
        }
        
        if(error)
            g_error_free(error);
        
        if(!info)  /* if we failed, and |info| is NULL, bail */
            return;
    }
    
    if(info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY) {
        succeeded = xfdesktop_file_icon_launch_external(file_icon,
                                                        fmanager->priv->gscreen);
    } else if(info->flags & THUNAR_VFS_FILE_FLAGS_EXECUTABLE) {
        succeeded = thunar_vfs_info_execute(info,
                                            fmanager->priv->gscreen,
                                            NULL,
                                            xfce_get_homedir(),
                                            NULL);
    }
    
    if(!succeeded) {
        mime_app = thunar_vfs_mime_database_get_default_application(thunar_mime_database,
                                                                    info->mime_info);
        if(mime_app) {
            GList *path_list = g_list_prepend(NULL, info->path);
            
            DBG("executing");
            
            succeeded = thunar_vfs_mime_handler_exec(THUNAR_VFS_MIME_HANDLER(mime_app),
                                                     fmanager->priv->gscreen,
                                                     path_list,
                                                     NULL); 
            g_object_unref(G_OBJECT(mime_app));
            g_list_free(path_list);
        } else {
            succeeded = xfdesktop_file_icon_launch_external(file_icon,
                                                            fmanager->priv->gscreen);
        }
    }    
    
    if(!succeeded) {
        GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
        gchar *primary = g_strdup_printf(_("Unable to launch \"%s\":"),
                                         info->display_name);
        xfce_message_dialog(GTK_WINDOW(toplevel), _("Launch Error"),
                            GTK_STOCK_DIALOG_ERROR, primary,
                            _("The associated application could not be found or executed."),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        g_free(primary);
    }
}

static void
xfdesktop_file_icon_menu_executed(GtkWidget *widget,
                                  gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopIcon *icon;
    GList *selected;
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(g_list_length(selected) == 1);
    icon = XFDESKTOP_ICON(selected->data);
    g_list_free(selected);
    
    xfdesktop_file_icon_activated(icon, fmanager);
}

static void
xfdesktop_file_icon_menu_open_all(GtkWidget *widget,
                                  gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GList *selected;
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(selected);
    
    g_list_foreach(selected, (GFunc)xfdesktop_file_icon_activated, fmanager);
    g_list_free(selected);
}

static GtkWidget *
xfdesktop_file_icon_create_entry_dialog(const gchar *title,
                                        GtkWindow *parent,
                                        GdkPixbuf *icon,
                                        const gchar *dialog_text,
                                        const gchar *entry_prefill,
                                        const gchar *accept_button_str,
                                        GtkWidget **entry_return)
{
    GtkWidget *dlg, *topvbox, *hbox, *vbox, *lbl, *entry, *btn, *img;
    
    dlg = gtk_dialog_new_with_buttons(title, parent,
                                      GTK_DIALOG_NO_SEPARATOR,
                                      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                      NULL);
    
    btn = xfce_create_mixed_button(GTK_STOCK_OK, accept_button_str);
    GTK_WIDGET_SET_FLAGS(btn, GTK_CAN_DEFAULT);
    gtk_widget_show(btn);
    gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn, GTK_RESPONSE_ACCEPT);
    
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_ACCEPT);
    
    topvbox = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(topvbox), BORDER);
    gtk_widget_show(topvbox);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), topvbox, TRUE, TRUE, 0);
    
    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(topvbox), hbox, FALSE, FALSE, 0);
    
    if(icon) {
        img = gtk_image_new_from_pixbuf(icon);
        gtk_widget_show(img);
        gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
    }
    
    vbox = gtk_vbox_new(FALSE, BORDER);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
    
    lbl = gtk_label_new(dialog_text);
    gtk_misc_set_alignment(GTK_MISC(lbl), 0.0, 0.5);
    gtk_widget_show(lbl);
    gtk_box_pack_start(GTK_BOX(vbox), lbl, FALSE, FALSE, 0);
    
    entry = gtk_entry_new();
    if(entry_prefill) {
        gchar *p;
        gtk_entry_set_text(GTK_ENTRY(entry), entry_prefill);
        if((p = g_utf8_strrchr(entry_prefill, -1, '.'))) {
            gint offset = g_utf8_strlen(entry_prefill, p - entry_prefill);
            gtk_editable_set_position(GTK_EDITABLE(entry), offset);
            gtk_editable_select_region(GTK_EDITABLE(entry), 0, offset);
        }
    }
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_widget_show(entry);
    gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);
    
    xfce_gtk_window_center_on_monitor_with_pointer(GTK_WINDOW(dlg));
    
    *entry_return = entry;
    return dlg;
}

static void
xfdesktop_file_icon_menu_rename(GtkWidget *widget,
                                gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon;
    GList *selected;
    const ThunarVfsInfo *info;
    GtkWidget *dlg, *entry = NULL, *toplevel;
    GdkPixbuf *pix;
    gchar *title;
    gint w, h;
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(g_list_length(selected) == 1);
    icon = XFDESKTOP_FILE_ICON(selected->data);
    g_list_free(selected);
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    info = xfdesktop_file_icon_peek_info(icon);
    
    /* make sure the icon doesn't get destroyed while the dialog is open */
    g_object_ref(G_OBJECT(icon));
    
    title = g_strdup_printf(_("Rename \"%s\""), info->display_name);
    gtk_icon_size_lookup(GTK_ICON_SIZE_DIALOG, &w, &h);
    pix = xfdesktop_icon_peek_pixbuf(XFDESKTOP_ICON(icon), w);
    
    dlg = xfdesktop_file_icon_create_entry_dialog(title,
                                                  GTK_WINDOW(toplevel),
                                                  pix,
                                                  _("Enter the new name:"),
                                                  info->display_name,
                                                  _("Rename"),
                                                  &entry);
    g_free(title);
    
    if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dlg))) {
        gchar *new_name;
        
        new_name = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
        xfdesktop_file_icon_rename_file(icon, new_name);
        
        g_free(new_name);
    }
    
    gtk_widget_destroy(dlg);
    g_object_unref(G_OBJECT(icon));
}

enum
{
    COL_PIX = 0,
    COL_NAME,
    N_COLS
};

static void
xfdesktop_file_icon_manager_delete_selected(XfdesktopFileIconManager *fmanager)
{
    GList *selected, *l;
    gchar *primary;
    gint ret = GTK_RESPONSE_CANCEL;
    XfdesktopIcon *icon;
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(selected);
    
    /* remove any removable volumes from the list */
    for(l = selected; l; ) {
        if(xfdesktop_file_icon_peek_volume(XFDESKTOP_FILE_ICON(l->data))) {
            GList *next = l->next;
            
            if(l->prev)
                l->prev->next = l->next;
            else  /* this is the first item; reset |selected| */
                selected = l->next;
            
            if(l->next)
                l->next->prev = l->prev;
            
            l->next = l->prev = NULL;
            g_list_free_1(l);
            
            l = next;
        } else
            l = l->next;
    }
    
    if(G_UNLIKELY(!selected))
        return;
    
    /* make sure the icons don't get destroyed while the dialog is open */
    g_list_foreach(selected, (GFunc)g_object_ref, NULL);
    
    if(g_list_length(selected) == 1) {
        icon = XFDESKTOP_ICON(selected->data);
        
        primary = g_strdup_printf(_("Are you sure that you want to delete \"%s\"?"),
                                  xfdesktop_icon_peek_label(icon));
        ret = xfce_message_dialog(GTK_WINDOW(toplevel),
                                  _("Question"), GTK_STOCK_DIALOG_QUESTION,
                                  primary,
                                  _("If you delete a file, it is permanently lost."),
                                  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                  GTK_STOCK_DELETE, GTK_RESPONSE_ACCEPT, NULL);
        g_free(primary);
    } else {
        GtkWidget *dlg, *treeview, *vbox, *sw, *cancel_btn, *delete_btn;
        GtkListStore *ls;
        GtkTreeIter itr;
        GtkTreeViewColumn *col;
        GtkCellRenderer *render;
        gint w,h;
        
        gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, &h);
        
        primary = g_strdup_printf(_("Are you sure you want to delete the following %d files?"),
                                  g_list_length(selected));
        dlg = xfce_message_dialog_new(GTK_WINDOW(toplevel),
                                      _("Delete Multiple Files"),
                                      GTK_STOCK_DIALOG_QUESTION,
                                      primary,
                                      _("If you delete a file, it is permanently lost."),
                                      NULL);
        g_free(primary);
        vbox = GTK_DIALOG(dlg)->vbox;
        
        sw = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
                                            GTK_SHADOW_ETCHED_IN);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                                       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_widget_show(sw);
        gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
        
        ls = gtk_list_store_new(N_COLS, GDK_TYPE_PIXBUF, G_TYPE_STRING);
        for(l = selected; l; l = l->next) {
            icon = XFDESKTOP_ICON(l->data);
            gtk_list_store_append(ls, &itr);
            gtk_list_store_set(ls, &itr,
                               COL_PIX, xfdesktop_icon_peek_pixbuf(icon, w),
                               COL_NAME, xfdesktop_icon_peek_label(icon),
                               -1);
        }
        
        treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ls));
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
        gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)),
                                    GTK_SELECTION_NONE);
        
        render = gtk_cell_renderer_pixbuf_new();
        col = gtk_tree_view_column_new_with_attributes("pix", render,
                                                       "pixbuf", COL_PIX, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), col);
        
        render = gtk_cell_renderer_text_new();
        g_object_set(G_OBJECT(render),
                     "ellipsize", PANGO_ELLIPSIZE_END,
                     "ellipsize-set", TRUE,
                     NULL);
        col = gtk_tree_view_column_new_with_attributes("label", render,
                                                       "text", COL_NAME, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), col);
        
        gtk_widget_show(treeview);
        gtk_container_add(GTK_CONTAINER(sw), treeview);
        
        cancel_btn = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
        GTK_WIDGET_SET_FLAGS(cancel_btn, GTK_CAN_DEFAULT);
        gtk_widget_show(cancel_btn);
        gtk_dialog_add_action_widget(GTK_DIALOG(dlg), cancel_btn,
                                     GTK_RESPONSE_CANCEL);
        
        delete_btn = gtk_button_new_from_stock(GTK_STOCK_DELETE);
        gtk_widget_show(delete_btn);
        gtk_dialog_add_action_widget(GTK_DIALOG(dlg), delete_btn,
                                     GTK_RESPONSE_ACCEPT);
        
        gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_CANCEL);
        gtk_widget_show(dlg);
        gtk_widget_grab_focus(cancel_btn);
        
        ret = gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
    }
        
    if(GTK_RESPONSE_ACCEPT == ret)
        g_list_foreach(selected, (GFunc)xfdesktop_file_icon_delete_file, NULL);
    
    g_list_foreach(selected, (GFunc)g_object_unref, NULL);
    g_list_free(selected);
}

static void
xfdesktop_file_icon_menu_mime_app_executed(GtkWidget *widget,
                                           gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon;
    ThunarVfsMimeApplication *mime_app;
    const ThunarVfsInfo *info;
    GList *path_list, *selected;
    GtkWidget *toplevel;
    GError *error = NULL;
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(g_list_length(selected) == 1);
    icon = XFDESKTOP_FILE_ICON(selected->data);
    g_list_free(selected);
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    info = xfdesktop_file_icon_peek_info(icon);
    path_list = g_list_append(NULL, info->path);
    
    mime_app = g_object_get_qdata(G_OBJECT(widget), xfdesktop_mime_app_quark);
    g_return_if_fail(mime_app);
    
    if(!thunar_vfs_mime_handler_exec(THUNAR_VFS_MIME_HANDLER(mime_app),
                                     gtk_widget_get_screen(widget),
                                     path_list,
                                     &error))
    {
        gchar *primary = g_strdup_printf(_("Unable to launch \"%s\":"),
                                         thunar_vfs_mime_application_get_name(mime_app));
        xfce_message_dialog(GTK_WINDOW(toplevel), _("Launch Error"),
                            GTK_STOCK_DIALOG_ERROR, primary, error->message,
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        g_free(primary);
        g_error_free(error);
    }
    
    g_list_free(path_list);
}

static void
xfdesktop_file_icon_menu_other_app(GtkWidget *widget,
                                   gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon;
    GList *selected;
    const ThunarVfsInfo *info;
    GdkScreen *gscreen;
    GtkWidget *toplevel;
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(g_list_length(selected) == 1);
    icon = XFDESKTOP_FILE_ICON(selected->data);
    g_list_free(selected);
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    info = xfdesktop_file_icon_peek_info(icon);
    
    gscreen = gtk_widget_get_screen(widget);
    
    if(!xfdesktop_file_icon_launch_external(icon, gscreen)) {
        gchar *primary = g_strdup_printf(_("Unable to launch \"%s\":"),
                                         info->display_name);
        xfce_message_dialog(GTK_WINDOW(toplevel),
                            _("Launch Error"), GTK_STOCK_DIALOG_ERROR,
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
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GList *files;
    
    files = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(files);
    
    xfdesktop_clipboard_manager_cut_files(clipboard_manager, files);
    
    g_list_free(files);
}

static void
xfdesktop_file_icon_menu_copy(GtkWidget *widget,
                              gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GList *files;
    
    files = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(files);
    
    xfdesktop_clipboard_manager_copy_files(clipboard_manager, files);
    
    g_list_free(files);
}

static void
xfdesktop_file_icon_menu_toggle_mount(GtkWidget *widget,
                                      gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon;
    GList *selected;
    GtkWidget *toplevel;
    ThunarVfsVolume *volume;
    GError *error = NULL;
    gboolean is_mount;
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(g_list_length(selected) == 1);
    icon = XFDESKTOP_FILE_ICON(selected->data);
    g_list_free(selected);
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    volume = xfdesktop_file_icon_peek_volume(icon);
    g_return_if_fail(volume);
    
    is_mount = !thunar_vfs_volume_is_mounted(volume);
    
    if(!is_mount)
        thunar_vfs_volume_unmount(volume, toplevel, &error);
    else {
        if(thunar_vfs_volume_mount(volume, toplevel, &error)) {
            const ThunarVfsInfo *info = xfdesktop_file_icon_peek_info(icon);
            ThunarVfsPath *new_path = thunar_vfs_volume_get_mount_point(volume);
            
            if(!info || !thunar_vfs_path_equal(info->path, new_path)) {
                ThunarVfsInfo *new_info = thunar_vfs_info_new_for_path(new_path,
                                                                       NULL);
                if(new_info) {
                    xfdesktop_file_icon_update_info(icon, new_info);
                    thunar_vfs_info_unref(new_info);
                }
            }
        }
    }
    
    if(error) {
        gchar *primary = g_strdup_printf(is_mount ? _("Unable to mount \"%s\":")
                                                  : _("Unable to unmount \"%s\":"),
                                         thunar_vfs_volume_get_name(volume));
        xfce_message_dialog(GTK_WINDOW(toplevel),
                            is_mount ? _("Mount Failed") : _("Unmount Failed"),
                            GTK_STOCK_DIALOG_ERROR, primary, error->message,
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        g_free(primary);
        g_error_free(error);
    }
}

static void
xfdesktop_file_icon_menu_eject(GtkWidget *widget,
                               gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon;
    GList *selected;
    GtkWidget *toplevel;
    ThunarVfsVolume *volume;
    GError *error = NULL;
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(g_list_length(selected) == 1);
    icon = XFDESKTOP_FILE_ICON(selected->data);
    g_list_free(selected);
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    volume = xfdesktop_file_icon_peek_volume(icon);
    g_return_if_fail(volume);
    
    if(!thunar_vfs_volume_eject(volume, toplevel, &error)) {
        gchar *primary = g_strdup_printf(_("Unable to eject \"%s\":"),
                                         thunar_vfs_volume_get_name(volume));
        xfce_message_dialog(GTK_WINDOW(toplevel), _("Eject Failed"),
                            GTK_STOCK_DIALOG_ERROR, primary, error->message,
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        g_free(primary);
        g_error_free(error);
    }
}

enum
{
    OPENWITH_COL_PIX = 0,
    OPENWITH_COL_NAME,
    OPENWITH_COL_HANDLER,
    OPENWITH_N_COLS,
};

static gboolean
xfdesktop_mime_handlers_free_ls(GtkTreeModel *model,
                                GtkTreePath *path,
                                GtkTreeIter *itr,
                                gpointer data)
{
    GObject *handler = NULL;
    
    gtk_tree_model_get(model, itr,
                       OPENWITH_COL_HANDLER, &handler,
                       -1);
    if(handler)
        g_object_unref(handler);
    
    return FALSE;
}

static void
xfdesktop_file_icon_menu_properties_destroyed(GtkWidget *widget,
                                              gpointer user_data)
{
    GtkTreeModel *model = GTK_TREE_MODEL(user_data);
    
    gtk_tree_model_foreach(model, xfdesktop_mime_handlers_free_ls, NULL);
}

static void
xfdesktop_file_icon_set_default_mime_handler(GtkComboBox *combo,
                                             gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon = g_object_get_data(G_OBJECT(combo),
                                                "--xfdesktop-icon");
    const ThunarVfsInfo *info = xfdesktop_file_icon_peek_info(icon);
    GtkTreeIter itr;
    GtkTreeModel *model;
    ThunarVfsMimeApplication *mime_app = NULL;
    GError *error = NULL;
    
    if(!gtk_combo_box_get_active_iter(combo, &itr))
        return;
    
    model = gtk_combo_box_get_model(combo);
    gtk_tree_model_get(model, &itr,
                       OPENWITH_COL_HANDLER, &mime_app,
                       -1);
    if(mime_app) {
        if(!thunar_vfs_mime_database_set_default_application(thunar_mime_database,
                                                             info->mime_info,
                                                             mime_app,
                                                             &error))
        {
            GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
            gchar *primary = g_strdup_printf(_("Unable to set default application for \"%s\" to \"%s\":"),
                                             xfdesktop_icon_peek_label(XFDESKTOP_ICON(icon)),
                                             thunar_vfs_mime_application_get_name(mime_app));
            xfce_message_dialog(GTK_WINDOW(toplevel), _("Properties Error"),
                                GTK_STOCK_DIALOG_ERROR, primary, error->message,
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
            g_free(primary);
            g_error_free(error);
        }
    }
}

static void
xfdesktop_file_icon_menu_properties(GtkWidget *widget,
                                    gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon;
    GList *selected;
    GtkWidget *dlg, *table, *hbox, *lbl, *img, *spacer, *notebook, *vbox,
              *entry, *toplevel, *combo;
    gint row = 0, w, h;
    PangoFontDescription *pfd = pango_font_description_from_string("bold");
    gchar *str = NULL, buf[64];
    gboolean is_link = FALSE;
    struct tm *tm;
    const ThunarVfsInfo *info;
    ThunarVfsUserManager *user_manager;
    ThunarVfsUser *user;
    ThunarVfsGroup *group;
    ThunarVfsFileMode mode;
    GList *mime_apps, *l;
    static const gchar *access_types[4] = {
        N_("None"), N_("Write only"), N_("Read only"), N_("Read & Write")
    };
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(g_list_length(selected) == 1);
    icon = XFDESKTOP_FILE_ICON(selected->data);
    g_list_free(selected);
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    info = xfdesktop_file_icon_peek_info(icon);
    
    gtk_icon_size_lookup(GTK_ICON_SIZE_DIALOG, &w, &h);
    
    dlg = gtk_dialog_new_with_buttons(xfdesktop_icon_peek_label(XFDESKTOP_ICON(icon)),
                                      GTK_WINDOW(toplevel),
                                      GTK_DIALOG_NO_SEPARATOR,
                                      GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                                      NULL);
    gtk_window_set_icon(GTK_WINDOW(dlg),
                        xfdesktop_icon_peek_pixbuf(XFDESKTOP_ICON(icon), w));
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
    
    img = gtk_image_new_from_pixbuf(xfdesktop_icon_peek_pixbuf(XFDESKTOP_ICON(icon),
                                                               w));
    gtk_widget_show(img);
    gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
    
    lbl = gtk_label_new(_("Name:"));
    gtk_misc_set_alignment(GTK_MISC(lbl), 1.0, 0.5);
    gtk_widget_modify_font(lbl, pfd);
    gtk_widget_show(lbl);
    gtk_box_pack_start(GTK_BOX(hbox), lbl, TRUE, TRUE, 0);
    
    entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry),
                       xfdesktop_icon_peek_label(XFDESKTOP_ICON(icon)));
    gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
    gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);  /* FIXME */
    gtk_widget_show(entry);
    gtk_table_attach(GTK_TABLE(table), entry, 1, 2, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    gtk_widget_grab_focus(entry);
    
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
                     GTK_FILL, GTK_FILL, 0, 0);
    
    if(!strcmp(thunar_vfs_mime_info_get_name(info->mime_info),
               "inode/symlink"))
    {
        str = g_strdup(_("broken link"));
        is_link = TRUE;
    } else if(info->flags & THUNAR_VFS_FILE_FLAGS_SYMLINK) {
        str = g_strdup_printf(_("link to %s"),
                              thunar_vfs_mime_info_get_comment(info->mime_info));
        is_link = TRUE;
    } else
        str = g_strdup(thunar_vfs_mime_info_get_comment(info->mime_info));
    lbl = gtk_label_new(str);
    g_free(str);
    gtk_misc_set_alignment(GTK_MISC(lbl), 0.0, 0.5);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 1, 2, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    
    ++row;
    
    if(is_link) {
        gchar *link_name, *display_name;
        
        lbl = gtk_label_new(_("Link Target:"));
        gtk_misc_set_alignment(GTK_MISC(lbl), 1.0, 0.5);
        gtk_widget_modify_font(lbl, pfd);
        gtk_widget_show(lbl);
        gtk_table_attach(GTK_TABLE(table), lbl, 0, 1, row, row + 1,
                         GTK_FILL, GTK_FILL, 0, 0);
        
        link_name = thunar_vfs_info_read_link(info, NULL);
        if(link_name) {
            display_name = g_filename_display_name(link_name);
            lbl = gtk_label_new(display_name);
            g_object_set(G_OBJECT(lbl),
                         "ellipsize", EXO_PANGO_ELLIPSIZE_START,
                         NULL);
            g_free(display_name);
            g_free(link_name);
        } else
            lbl = gtk_label_new(_("(unknown)"));
        gtk_misc_set_alignment(GTK_MISC(lbl), 0.0, 0.5);
        gtk_widget_show(lbl);
        gtk_table_attach(GTK_TABLE(table), lbl, 1, 2, row, row + 1,
                         GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
        
        ++row;
    }
    
    if(info->type != THUNAR_VFS_FILE_TYPE_DIRECTORY) {
        mime_apps = thunar_vfs_mime_database_get_applications(thunar_mime_database,
                                                              info->mime_info);
        
        if(mime_apps) {
            GtkListStore *ls;
            GtkTreeIter itr;
            GtkCellRenderer *render;
            ThunarVfsMimeHandler *handler;
            const gchar *icon_name;
            gint w, h;
            GdkPixbuf *pix;
            
            lbl = gtk_label_new(_("Open With:"));
            gtk_misc_set_alignment(GTK_MISC(lbl), 1.0, 0.5);
            gtk_widget_modify_font(lbl, pfd);
            gtk_widget_show(lbl);
            gtk_table_attach(GTK_TABLE(table), lbl, 0, 1, row, row + 1,
                             GTK_FILL, GTK_FILL, 0, 0);
            
            hbox = gtk_hbox_new(FALSE, BORDER);
            gtk_widget_show(hbox);
            gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, row, row + 1,
                             GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
            
            gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, &h);
            
            ls = gtk_list_store_new(OPENWITH_N_COLS, GDK_TYPE_PIXBUF,
                                    G_TYPE_STRING, G_TYPE_POINTER);
            for(l = mime_apps; l; l = l->next) {
                handler = THUNAR_VFS_MIME_HANDLER(l->data);
                pix = NULL;
                
                gtk_list_store_append(ls, &itr);
                
                icon_name = thunar_vfs_mime_handler_lookup_icon_name(handler,
                                                                     gtk_icon_theme_get_default());
                if(icon_name)
                    pix = xfce_themed_icon_load(icon_name, w);
                
                gtk_list_store_set(ls, &itr,
                                   OPENWITH_COL_PIX, pix,
                                   OPENWITH_COL_NAME,
                                   thunar_vfs_mime_handler_get_name(handler),
                                   OPENWITH_COL_HANDLER, handler,
                                   -1);
                
                if(pix)
                    g_object_unref(G_OBJECT(pix));
            }
            
            g_signal_connect(G_OBJECT(dlg), "destroy",
                             G_CALLBACK(xfdesktop_file_icon_menu_properties_destroyed),
                             ls);
            
            g_list_free(mime_apps);
            
            combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(ls));
            
            render = gtk_cell_renderer_pixbuf_new();
            gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), render, FALSE);
            gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(combo), render,
                                          "pixbuf", OPENWITH_COL_PIX);
            
            render = gtk_cell_renderer_text_new();
            gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), render, FALSE);
            gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(combo), render,
                                          "text", OPENWITH_COL_NAME);
            
            gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
            g_object_set_data(G_OBJECT(combo), "--xfdesktop-icon", icon);
            gtk_widget_show(combo);
            gtk_table_attach(GTK_TABLE(table), combo, 1, 2, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
            g_signal_connect(G_OBJECT(combo), "changed",
                             G_CALLBACK(xfdesktop_file_icon_set_default_mime_handler),
                             fmanager);
            
            ++row;
        }
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
                     GTK_FILL, GTK_FILL, 0, 0);
    
    tm = localtime(&info->mtime);
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
                     GTK_FILL, GTK_FILL, 0, 0);
    
    tm = localtime(&info->atime);
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
    
    if(info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY)
        lbl = gtk_label_new(_("Free Space:"));
    else
        lbl = gtk_label_new(_("Size:"));
    gtk_misc_set_alignment(GTK_MISC(lbl), 1.0, 0.5);
    gtk_widget_modify_font(lbl, pfd);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 0, 1, row, row + 1,
                     GTK_FILL, GTK_FILL, 0, 0);
    
    if(info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY) {
        ThunarVfsFileSize free_space;
        if(thunar_vfs_info_get_free_space(info, &free_space)) {
            thunar_vfs_humanize_size(free_space, buf, 64);
            lbl = gtk_label_new(buf);
        } else
            lbl = gtk_label_new(_("(unknown)"));
    } else {
        thunar_vfs_humanize_size(info->size, buf, 64);
        str = g_strdup_printf(_("%s (%" G_GINT64_FORMAT " Bytes)"), buf,
                              (gint64)info->size);
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
                                                  info->uid);
    group = thunar_vfs_user_manager_get_group_by_id(user_manager,
                                                    info->gid);
    mode = info->mode;
    
    lbl = gtk_label_new(_("Owner:"));
    gtk_misc_set_alignment(GTK_MISC(lbl), 1.0, 0.5);
    gtk_widget_modify_font(lbl, pfd);
    gtk_widget_show(lbl);
    gtk_table_attach(GTK_TABLE(table), lbl, 0, 1, row, row + 1,
                     GTK_FILL, GTK_FILL, 0, 0);
    
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
                     GTK_FILL, GTK_FILL, 0, 0);
    
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
                     GTK_FILL, GTK_FILL, 0, 0);
    
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
                     GTK_FILL, GTK_FILL, 0, 0);
    
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
                     GTK_FILL, GTK_FILL, 0, 0);
    
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
    
#ifdef HAVE_THUNARX
    if(fmanager->priv->thunarx_properties_providers) {
        GList *l, *pages, *p, *files = g_list_append(NULL, icon);
        ThunarxPropertyPageProvider *provider;
        ThunarxPropertyPage *page;
        GtkWidget *label_widget;
        const gchar *label;
        
        for(l = fmanager->priv->thunarx_properties_providers; l; l = l->next) {
            provider = THUNARX_PROPERTY_PAGE_PROVIDER(l->data);
            pages = thunarx_property_page_provider_get_pages(provider, files);
            
            for(p = pages; p; p = p->next) {
                page = THUNARX_PROPERTY_PAGE(p->data);
                label_widget = thunarx_property_page_get_label_widget(page);
                if(!label_widget) {
                    label = thunarx_property_page_get_label(page);
                    label_widget = gtk_label_new(label);
                }
                gtk_widget_show(GTK_WIDGET(page));
                gtk_widget_show(label_widget);
                gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                                         GTK_WIDGET(page), label_widget);
            }
            
            /* each page should be freed when the dialog is destroyed (?) */
            g_list_free(pages);
        }
        
        g_list_free(files);
    }
#endif
    
    gtk_widget_show(dlg);
}


static GtkWidget *
xfdesktop_menu_item_from_mime_handler(XfdesktopFileIconManager *fmanager,
                                      XfdesktopFileIcon *icon,
                                      ThunarVfsMimeHandler *mime_handler,
                                      gint icon_size,
                                      gboolean with_mnemonic,
                                      gboolean with_title_prefix)
{
    GtkWidget *mi, *img;
    gchar *title;
    const gchar *icon_name;
    
    if(!with_title_prefix)
        title = g_strdup(thunar_vfs_mime_handler_get_name(mime_handler));
    else if(with_mnemonic) {
        title = g_strdup_printf(_("_Open With \"%s\""),
                                thunar_vfs_mime_handler_get_name(mime_handler));
    } else {
        title = g_strdup_printf(_("Open With \"%s\""),
                                thunar_vfs_mime_handler_get_name(mime_handler));
    }
    icon_name = thunar_vfs_mime_handler_lookup_icon_name(mime_handler,
                                                         gtk_icon_theme_get_default());
    
    if(with_mnemonic)
        mi = gtk_image_menu_item_new_with_mnemonic(title);
    else
        mi = gtk_image_menu_item_new_with_label(title);
    g_free(title);
    
    g_object_set_qdata_full(G_OBJECT(mi), xfdesktop_mime_app_quark,
                            mime_handler,
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
                     G_CALLBACK(xfdesktop_file_icon_menu_mime_app_executed),
                     fmanager);
    
    return mi;
}

static gboolean
xfdesktop_file_icon_menu_deactivate_idled(gpointer user_data)
{
    GList *icon_list = g_object_get_data(G_OBJECT(user_data),
                                         "--xfdesktop-icon-list");
    
    gtk_widget_destroy(GTK_WIDGET(user_data));
    
    if(icon_list) {
        g_list_foreach(icon_list, (GFunc)g_object_unref, NULL);
        g_list_free(icon_list);
    }
    
    return FALSE;
}

static void
xfdesktop_file_icon_create_directory_error(ThunarVfsJob *job,
                                           GError *error,
                                           gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    const gchar *folder_name = g_object_get_data(G_OBJECT(job),
                                                 "xfdesktop-folder-name");
    gchar *primary = g_strdup_printf(_("Unable to create folder named \"%s\":"),
                                     folder_name);
    
    xfce_message_dialog(GTK_WINDOW(toplevel), _("Create Folder Failed"),
                        GTK_STOCK_DIALOG_ERROR, primary, error->message,
                        GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
    
    g_free(primary);
}

static void
xfdesktop_file_icon_menu_create_launcher(GtkWidget *widget,
                                         gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    ThunarVfsInfo *info;
    gchar *cmd = NULL, *pathstr = NULL, *display_name;
    GError *error = NULL;
    
    display_name = gdk_screen_make_display_name(fmanager->priv->gscreen);
    
    info = g_object_get_data(G_OBJECT(widget), "thunar-vfs-info");
    if(info) {
        pathstr = thunar_vfs_path_dup_string(info->path);
        cmd = g_strdup_printf("exo-desktop-item-edit \"--display=%s\" \"%s\"",
                              display_name, pathstr);
    } else {
        const gchar *type = g_object_get_data(G_OBJECT(widget), "xfdesktop-launcher-type");
        pathstr = thunar_vfs_path_dup_string(fmanager->priv->folder);
        if(G_UNLIKELY(!type))
            type = "Application";
        cmd = g_strdup_printf("exo-desktop-item-edit \"--display=%s\" --create-new --type %s \"%s\"",
                              display_name, type, pathstr);
    }
    
    if(!xfce_exec(cmd, FALSE, FALSE, &error)) {
        GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
        xfce_message_dialog(GTK_WINDOW(toplevel), _("Launch Error"),
                            GTK_STOCK_DIALOG_ERROR, 
                            _("Unable to launch \"exo-desktop-item-edit\", which is required to create and edit launchers and links on the desktop."),
                            error->message, GTK_STOCK_CLOSE,
                            GTK_RESPONSE_ACCEPT, NULL);
        g_error_free(error);
    }
    
    g_free(display_name);
    g_free(pathstr);
    g_free(cmd);
}

static void
xfdesktop_file_icon_menu_create_folder(GtkWidget *widget,
                                       gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GtkWidget *dlg, *entry = NULL, *toplevel;
    ThunarVfsMimeInfo *minfo;
    GdkPixbuf *pix = NULL;
    
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    minfo = thunar_vfs_mime_database_get_info(thunar_mime_database,
                                              "inode/directory");
    if(minfo) {
        gint w, h;
        const gchar *icon_name = thunar_vfs_mime_info_lookup_icon_name(minfo,
                                                                       gtk_icon_theme_get_default());
        gtk_icon_size_lookup(GTK_ICON_SIZE_DIALOG, &w, &h);
        pix = xfce_themed_icon_load(icon_name, w);
        thunar_vfs_mime_info_unref(minfo);
    }
    
    dlg = xfdesktop_file_icon_create_entry_dialog(_("Create New Folder"),
                                                  GTK_WINDOW(toplevel),
                                                  pix,
                                                  _("Enter the new name:"),
                                                  _("New Folder"),
                                                  _("Create"),
                                                  &entry);
    
    if(pix)
        g_object_unref(G_OBJECT(pix));
    
    if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dlg))) {
        gchar *name = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
        ThunarVfsPath *path = thunar_vfs_path_relative(fmanager->priv->folder,
                                                       name);
        ThunarVfsJob *job = thunar_vfs_make_directory(path, NULL);
        if(job) {
            g_object_set_data_full(G_OBJECT(job), "xfdesktop-folder-name",
                                   name, (GDestroyNotify)g_free);
            g_signal_connect(G_OBJECT(job), "error",
                             G_CALLBACK(xfdesktop_file_icon_create_directory_error),
                             fmanager);
            g_signal_connect(G_OBJECT(job), "finished",
                             G_CALLBACK(g_object_unref), NULL);
            /* don't free |name|, GObject will do it */
        } else
            g_free(name);
        thunar_vfs_path_unref(path);
    }
    gtk_widget_destroy(dlg);
}

static void
xfdesktop_file_icon_create_file_error(ThunarVfsJob *job,
                                      GError *error,
                                      gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    const gchar *file_name = g_object_get_data(G_OBJECT(job),
                                               "xfdesktop-file-name");
    gchar *primary = g_strdup_printf(_("Unable to create file named \"%s\":"),
                                     file_name);
    
    xfce_message_dialog(GTK_WINDOW(toplevel), _("Create File Failed"),
                        GTK_STOCK_DIALOG_ERROR, primary, error->message,
                        GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
    
    g_free(primary);
}

static ThunarVfsInteractiveJobResponse
xfdesktop_file_icon_interactive_job_ask(ThunarVfsJob *job,
                                        const gchar *message,
                                        ThunarVfsInteractiveJobResponse choices,
                                        gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    return xfdesktop_file_utils_interactive_job_ask(GTK_WINDOW(toplevel),
                                                    message, choices);
}

static void
xfdesktop_file_icon_template_item_activated(GtkWidget *mi,
                                            gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GtkWidget *dlg, *entry = NULL, *toplevel;
    GdkPixbuf *pix = NULL;
    ThunarVfsInfo *info = g_object_get_data(G_OBJECT(mi), "thunar-vfs-info");
    ThunarVfsJob *job = NULL;
    
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    if(info) {
        gchar *title;
        const gchar *icon_name;
        gint w, h;
        
        thunar_vfs_info_ref(info);
        
        title = g_strdup_printf(_("Create Document from template \"%s\""),
                                info->display_name);
        
        icon_name = thunar_vfs_mime_info_lookup_icon_name(info->mime_info,
                                                          gtk_icon_theme_get_default());
        gtk_icon_size_lookup(GTK_ICON_SIZE_DIALOG, &w, &h);
        pix = xfce_themed_icon_load(icon_name, w);
        
        dlg = xfdesktop_file_icon_create_entry_dialog(title,
                                                      GTK_WINDOW(toplevel),
                                                      pix,
                                                      _("Enter the new name:"),
                                                      info->display_name,
                                                      _("Create"),
                                                      &entry);
        g_free(title);
    } else {
        pix = gtk_widget_render_icon(GTK_WIDGET(fmanager->priv->icon_view),
                                     GTK_STOCK_NEW, GTK_ICON_SIZE_DIALOG, NULL);
        dlg = xfdesktop_file_icon_create_entry_dialog(_("Create Empty File"),
                                                      GTK_WINDOW(toplevel),
                                                      pix,
                                                      _("Enter the new name:"),
                                                      _("New Empty File"),
                                                      _("Create"),
                                                      &entry);
    }
    
    if(pix)
        g_object_unref(G_OBJECT(pix));
    
    if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dlg))) {
        gchar *name = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
        ThunarVfsPath *path = thunar_vfs_path_relative(fmanager->priv->folder,
                                                       name);
        GError *error = NULL;
        
        if(info)
            job = thunar_vfs_copy_file(info->path, path, &error);
        else
            job = thunar_vfs_create_file(path, &error);
        
        if(job) {
            g_object_set_data_full(G_OBJECT(job), "xfdesktop-file-name",
                                   name, (GDestroyNotify)g_free);
            g_signal_connect(G_OBJECT(job), "error",
                             G_CALLBACK(xfdesktop_file_icon_create_file_error),
                             fmanager);
            g_signal_connect(G_OBJECT(job), "ask",
                             G_CALLBACK(xfdesktop_file_icon_interactive_job_ask),
                             fmanager);
            g_signal_connect(G_OBJECT(job), "finished",
                             G_CALLBACK(g_object_unref), NULL);
            /* don't free |name|, GObject will do it */
        } else {
            if(error) {
                gchar *primary = g_strdup_printf(_("Unable to create file \"%s\":"), name);
                xfce_message_dialog(GTK_WINDOW(toplevel), _("Create Error"),
                                    GTK_STOCK_DIALOG_ERROR, primary,
                                    error->message, GTK_STOCK_CLOSE,
                                    GTK_RESPONSE_ACCEPT, NULL);
                g_free(primary);
                g_error_free(error);
            }
            g_free(name);
        }
        thunar_vfs_path_unref(path);        
    }
    
    gtk_widget_destroy(dlg);
    
    if(info)
        thunar_vfs_info_unref(info);
}

/* copied from Thunar, Copyright (c) 2005 Benedikt Meurer */
static gint
info_compare (gconstpointer a,
              gconstpointer b)
{
  const ThunarVfsInfo *info_a = a;
  const ThunarVfsInfo *info_b = b;
  gchar               *name_a;
  gchar               *name_b;
  gint                 result;

  /* sort folders before files */
  if (info_a->type == THUNAR_VFS_FILE_TYPE_DIRECTORY && info_b->type != THUNAR_VFS_FILE_TYPE_DIRECTORY)
    return -1;
  else if (info_a->type != THUNAR_VFS_FILE_TYPE_DIRECTORY && info_b->type == THUNAR_VFS_FILE_TYPE_DIRECTORY)
    return 1;

  /* compare by name */
  name_a = g_utf8_casefold (info_a->display_name, -1);
  name_b = g_utf8_casefold (info_b->display_name, -1);
  result = g_utf8_collate (name_a, name_b);
  g_free (name_b);
  g_free (name_a);

  return result;
}

/* copied from Thunar, Copyright (c) 2005 Benedikt Meurer, modified by Brian */
static gboolean
xfdesktop_file_icon_menu_fill_template_menu(GtkWidget *menu,
                                            ThunarVfsPath *templates_path,
                                            XfdesktopFileIconManager *fmanager)
{
  gboolean       have_templates = FALSE;
  ThunarVfsInfo *info;
  ThunarVfsPath *path;
  GtkIconTheme  *icon_theme;
  const gchar   *icon_name;
  const gchar   *name;
  GtkWidget     *submenu;
  GtkWidget     *image;
  GtkWidget     *item;
  gchar         *absolute_path;
  gchar         *label;
  gchar         *dot;
  GList         *info_list = NULL;
  GList         *lp;
  GDir          *dp;
  
  /* try to open the templates (sub)directory */
  absolute_path = thunar_vfs_path_dup_string (templates_path);
  dp = g_dir_open (absolute_path, 0, NULL);
  g_free (absolute_path);

  /* read the directory contents (if opened successfully) */
  if (G_LIKELY (dp != NULL))
    {
      /* process all files within the directory */
      for (;;)
        {
          /* read the name of the next file */
          name = g_dir_read_name (dp);
          if (G_UNLIKELY (name == NULL))
            break;
          else if (name[0] == '.')
            continue;

          /* determine the info for that file */
          path = thunar_vfs_path_relative (templates_path, name);
          info = thunar_vfs_info_new_for_path (path, NULL);
          thunar_vfs_path_unref (path);

          /* add the info (if any) to our list */
          if (G_LIKELY (info != NULL))
            info_list = g_list_insert_sorted (info_list, info, info_compare);
        }

      /* close the directory handle */
      g_dir_close (dp);
    }

  /* check if we have any infos */
  if (G_UNLIKELY (info_list == NULL))
    return FALSE;

  /* determine the icon theme for the menu */
  icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (menu));

  /* add menu items for all infos */
  for (lp = info_list; lp != NULL; lp = lp->next)
    {
      /* determine the info */
      info = lp->data;

      /* check if we have a regular file or a directory here */
      if (G_LIKELY (info->type == THUNAR_VFS_FILE_TYPE_REGULAR))
        {
          /* generate a label by stripping off the extension */
          label = g_strdup (info->display_name);
          dot = g_utf8_strrchr (label, -1, '.');
          if (G_LIKELY (dot != NULL))
            *dot = '\0';

          /* allocate a new menu item */
          item = gtk_image_menu_item_new_with_label (label);
          g_object_set_data_full (G_OBJECT (item), I_("thunar-vfs-info"), thunar_vfs_info_ref (info), (GDestroyNotify) thunar_vfs_info_unref);
          g_signal_connect (G_OBJECT (item), "activate",
                            G_CALLBACK (xfdesktop_file_icon_template_item_activated),
                            fmanager);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_show (item);

          /* lookup the icon for the mime type of that file */
          icon_name = thunar_vfs_mime_info_lookup_icon_name (info->mime_info, icon_theme);

          /* generate an image based on the named icon */
          image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
          gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
          gtk_widget_show (image);

          /* cleanup */
          g_free (label);
          
          have_templates = TRUE;
        }
      else if (info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY)
        {
          /* allocate a new submenu for the directory */
          submenu = gtk_menu_new ();
          exo_gtk_object_ref_sink (GTK_OBJECT (submenu));
          gtk_menu_set_screen (GTK_MENU (submenu), gtk_widget_get_screen (menu));

          /* fill the submenu from the folder contents */
          have_templates = xfdesktop_file_icon_menu_fill_template_menu(submenu,
                                                                       info->path,
                                                                       fmanager)
                           || have_templates;

          /* check if any items were added to the submenu */
          if (G_LIKELY (GTK_MENU_SHELL (submenu)->children != NULL))
            {
              /* hook up the submenu */
              item = gtk_image_menu_item_new_with_label (info->display_name);
              gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
              gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
              gtk_widget_show (item);

              /* lookup the icon for the mime type of that file */
              icon_name = thunar_vfs_mime_info_lookup_icon_name (info->mime_info, icon_theme);

              /* generate an image based on the named icon */
              image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
              gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
              gtk_widget_show (image);
            }

          /* cleanup */
          g_object_unref (G_OBJECT (submenu));
        }
    }

  /* release the info list */
  thunar_vfs_info_list_free (info_list);
  
  return have_templates;
}

#ifdef HAVE_THUNARX
static inline void
xfdesktop_menu_shell_append_action_list(GtkMenuShell *menu_shell,
                                        GList *actions)
{
    GList *l;
    GtkAction *action;
    GtkWidget *mi;
    
    for(l = actions; l; l = l->next) {
        action = GTK_ACTION(l->data);
        mi = gtk_action_create_menu_item(action);
        gtk_widget_show(mi);
        gtk_menu_shell_append(menu_shell, mi);    
    }
}
#endif

static void
xfdesktop_file_icon_menu_popup(XfdesktopIcon *icon,
                               gpointer user_data)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    const ThunarVfsInfo *info = xfdesktop_file_icon_peek_info(file_icon);
    ThunarVfsMimeInfo *mime_info = info ? info->mime_info : NULL;
    ThunarVfsVolume *volume = xfdesktop_file_icon_peek_volume(file_icon);
    GList *selected, *mime_apps, *l, *mime_actions = NULL;
    GtkWidget *menu, *mi, *img, *menu2;
    gboolean multi_sel, have_templates = FALSE;
    gint w = 0, h = 0;
    GdkPixbuf *pix;
    ThunarVfsMimeInfo *minfo;
    ThunarVfsPath *templates_path;
    gchar *templates_path_str;
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(selected);
    multi_sel = (g_list_length(selected) > 1);
    
    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, &h);
    
    menu = gtk_menu_new();
    gtk_widget_show(menu);
    g_signal_connect_swapped(G_OBJECT(menu), "deactivate",
                             G_CALLBACK(g_idle_add),
                             xfdesktop_file_icon_menu_deactivate_idled);
    
    /* make sure icons don't get destroyed while menu is open */
    g_list_foreach(selected, (GFunc)g_object_ref, NULL);
    g_object_set_data(G_OBJECT(menu), "--xfdesktop-icon-list", selected);
    
    if(multi_sel) {
        img = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
        gtk_widget_show(img);
        mi = gtk_image_menu_item_new_with_mnemonic(_("_Open all"));
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        g_signal_connect(G_OBJECT(mi), "activate",
                         G_CALLBACK(xfdesktop_file_icon_menu_open_all),
                         fmanager);
    } else if(info) {
        if(info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY) {
            img = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
            gtk_widget_show(img);
            mi = gtk_image_menu_item_new_with_mnemonic(_("_Open"));
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            g_signal_connect(G_OBJECT(mi), "activate",
                             G_CALLBACK(xfdesktop_file_icon_menu_other_app),
                             fmanager);
        } else {
            gboolean have_separator = FALSE;
            
            if(info->flags & THUNAR_VFS_FILE_FLAGS_EXECUTABLE) {
                img = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
                gtk_widget_show(img);
                mi = gtk_image_menu_item_new_with_mnemonic(_("_Execute"));
                gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
                gtk_widget_show(mi);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                g_signal_connect(G_OBJECT(mi), "activate",
                                 G_CALLBACK(xfdesktop_file_icon_menu_executed),
                                 fmanager);
                
                if(!g_ascii_strcasecmp("application/x-desktop",
                                       thunar_vfs_mime_info_get_name(info->mime_info)))
                {
                    mi = gtk_separator_menu_item_new();
                    gtk_widget_show(mi);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                    have_separator = TRUE;
                    
                    img = gtk_image_new_from_stock(GTK_STOCK_EDIT, GTK_ICON_SIZE_MENU);
                    gtk_widget_show(img);
                    mi = gtk_image_menu_item_new_with_mnemonic(_("_Edit Launcher"));
                    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
                    g_object_set_data_full(G_OBJECT(mi), "thunar-vfs-info",
                                           thunar_vfs_info_ref((ThunarVfsInfo *)info),
                                           (GDestroyNotify)thunar_vfs_info_unref);
                    gtk_widget_show(mi);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                    g_signal_connect(G_OBJECT(mi), "activate",
                                     G_CALLBACK(xfdesktop_file_icon_menu_create_launcher),
                                     fmanager);
                }
            }
            
            mime_apps = thunar_vfs_mime_database_get_applications(thunar_mime_database,
                                                                  mime_info);
            if(mime_apps) {
                ThunarVfsMimeHandler *mime_handler = mime_apps->data;
                GList *tmp;
                
                if(info->flags & THUNAR_VFS_FILE_FLAGS_EXECUTABLE
                   && !have_separator)
                {
                    mi = gtk_separator_menu_item_new();
                    gtk_widget_show(mi);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                }
                
                mi = xfdesktop_menu_item_from_mime_handler(fmanager, file_icon,
                                                           mime_handler,
                                                           w, TRUE, TRUE);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                tmp = thunar_vfs_mime_application_get_actions(THUNAR_VFS_MIME_APPLICATION(mime_handler));
                if(tmp)
                    mime_actions = g_list_concat(mime_actions, tmp);
                
                if(mime_apps->next) {
                    GtkWidget *mime_apps_menu;
                    gint list_len = g_list_length(mime_apps->next);
                    
                    if(!(info->flags & THUNAR_VFS_FILE_FLAGS_EXECUTABLE)
                       && list_len <= 3)
                    {
                        mi = gtk_separator_menu_item_new();
                        gtk_widget_show(mi);
                        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                    }
                    
                    if(list_len > 3) {
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
                        mime_handler = THUNAR_VFS_MIME_HANDLER(l->data);
                        mi = xfdesktop_menu_item_from_mime_handler(fmanager,
                                                                   file_icon,
                                                                   mime_handler,
                                                                   w, FALSE,
                                                                   TRUE);
                        gtk_menu_shell_append(GTK_MENU_SHELL(mime_apps_menu), mi);
                        tmp = thunar_vfs_mime_application_get_actions(THUNAR_VFS_MIME_APPLICATION(mime_handler));
                        if(tmp)
                            mime_actions = g_list_concat(mime_actions, tmp);
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
                                 fmanager);
            }
        }
    }
    
    if(mime_actions) {
        ThunarVfsMimeHandler *mime_handler;
        
        mi = gtk_separator_menu_item_new();
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        
        for(l = mime_actions; l; l = l->next) {
            mime_handler = THUNAR_VFS_MIME_HANDLER(l->data);
            mi = xfdesktop_menu_item_from_mime_handler(fmanager, file_icon,
                                                       mime_handler,
                                                       w, FALSE, FALSE);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        }
        
        /* don't free the mime actions themselves */
        g_list_free(mime_actions);
    }
    
#ifdef HAVE_THUNARX
    if(!multi_sel && info && fmanager->priv->thunarx_menu_providers) {
        GtkWidget *window = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
        GList *menu_actions, *l;
        ThunarxMenuProvider *provider;
        gboolean added_separator = FALSE;
        
        if(info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY) {
            for(l = fmanager->priv->thunarx_menu_providers; l; l = l->next) {
                provider = THUNARX_MENU_PROVIDER(l->data);
                menu_actions = thunarx_menu_provider_get_folder_actions(provider,
                                                                        window,
                                                                        THUNARX_FILE_INFO(file_icon));
                if(menu_actions && !added_separator) {
                    mi = gtk_separator_menu_item_new();
                    gtk_widget_show(mi);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                    added_separator = TRUE;
                }
                
                xfdesktop_menu_shell_append_action_list(GTK_MENU_SHELL(menu),
                                                        menu_actions);
                g_list_foreach(menu_actions, (GFunc)g_object_unref, NULL);
                g_list_free(menu_actions);
            }
        } else {
            for(l = fmanager->priv->thunarx_menu_providers; l; l = l->next) {
                provider = THUNARX_MENU_PROVIDER(l->data);
                menu_actions = thunarx_menu_provider_get_file_actions(provider,
                                                                      window,
                                                                      selected);
                if(menu_actions && !added_separator) {
                    mi = gtk_separator_menu_item_new();
                    gtk_widget_show(mi);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                    added_separator = TRUE;
                }
                
                xfdesktop_menu_shell_append_action_list(GTK_MENU_SHELL(menu),
                                                        menu_actions);
                g_list_foreach(menu_actions, (GFunc)g_object_unref, NULL);
                g_list_free(menu_actions);
            }
        }
    }
#endif
    
    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    
    /* FIXME: need to handle multi-selection properly */
    if(volume && !multi_sel) {
        mi = gtk_image_menu_item_new_with_mnemonic(_("_Mount Volume"));
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        if(thunar_vfs_volume_is_mounted(volume))
            gtk_widget_set_sensitive(mi, FALSE);
        else {
            g_signal_connect(G_OBJECT(mi), "activate",
                             G_CALLBACK(xfdesktop_file_icon_menu_toggle_mount),
                             fmanager);
        }
        
        mi = gtk_image_menu_item_new_with_mnemonic(_("_Unmount Volume"));
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        if(!thunar_vfs_volume_is_mounted(volume))
            gtk_widget_set_sensitive(mi, FALSE);
        else {
            g_signal_connect(G_OBJECT(mi), "activate",
                             G_CALLBACK(xfdesktop_file_icon_menu_toggle_mount),
                             fmanager);
        }
        
        if(thunar_vfs_volume_is_disc(volume)
           && thunar_vfs_volume_is_ejectable(volume))
        {
            mi = gtk_image_menu_item_new_with_mnemonic(_("E_ject Volume"));
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            g_signal_connect(G_OBJECT(mi), "activate",
                             G_CALLBACK(xfdesktop_file_icon_menu_eject),
                             fmanager);
        }
    } else {
        img = gtk_image_new_from_stock(GTK_STOCK_COPY, GTK_ICON_SIZE_MENU);
        gtk_widget_show(img);
        mi = gtk_image_menu_item_new_with_mnemonic(multi_sel ? _("_Copy Files")
                                                             : _("_Copy File"));
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        g_signal_connect(G_OBJECT(mi), "activate",
                         G_CALLBACK(xfdesktop_file_icon_menu_copy), fmanager);
        
        img = gtk_image_new_from_stock(GTK_STOCK_CUT, GTK_ICON_SIZE_MENU);
        gtk_widget_show(img);
        mi = gtk_image_menu_item_new_with_mnemonic(multi_sel ? _("Cu_t Files")
                                                             : _("Cu_t File"));
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        g_signal_connect(G_OBJECT(mi), "activate",
                         G_CALLBACK(xfdesktop_file_icon_menu_cut), fmanager);
        
        img = gtk_image_new_from_stock(GTK_STOCK_DELETE, GTK_ICON_SIZE_MENU);
        gtk_widget_show(img);
        mi = gtk_image_menu_item_new_with_mnemonic(multi_sel ? _("_Delete Files")
                                                             : _("_Delete File"));
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        g_signal_connect_swapped(G_OBJECT(mi), "activate",
                                 G_CALLBACK(xfdesktop_file_icon_manager_delete_selected), 
                                 fmanager);
        
        mi = gtk_separator_menu_item_new();
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        
        mi = gtk_image_menu_item_new_with_mnemonic(_("_Rename..."));
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        if(multi_sel)
            gtk_widget_set_sensitive(mi, FALSE);
        else {
            g_signal_connect(G_OBJECT(mi), "activate",
                             G_CALLBACK(xfdesktop_file_icon_menu_rename),
                             fmanager);
        }
    }
    
    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    
    img = gtk_image_new_from_stock(GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_MENU);
    gtk_widget_show(img);
    mi = gtk_image_menu_item_new_with_mnemonic(_("_Properties..."));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    if(multi_sel || !info)
        gtk_widget_set_sensitive(mi, FALSE);
    else {
        g_signal_connect(G_OBJECT(mi), "activate",
                         G_CALLBACK(xfdesktop_file_icon_menu_properties),
                         fmanager);
    }
    
    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    
    img = gtk_image_new_from_stock(GTK_STOCK_NEW, GTK_ICON_SIZE_MENU);
    gtk_widget_show(img);
    mi = gtk_image_menu_item_new_with_mnemonic(_("Create _New"));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    
    menu2 = gtk_menu_new();
    gtk_widget_show(menu2);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), menu2);
    
    mi = gtk_image_menu_item_new_with_mnemonic(_("_Launcher..."));
    minfo = thunar_vfs_mime_database_get_info(thunar_mime_database,
                                              "application/x-desktop");
    if(minfo) {
        const gchar *icon_name = thunar_vfs_mime_info_lookup_icon_name(minfo,
                                                                       gtk_icon_theme_get_default());
        pix = xfce_themed_icon_load(icon_name, w);
        if(pix) {
            img = gtk_image_new_from_pixbuf(pix);
            gtk_widget_show(img);
            g_object_unref(G_OBJECT(pix));
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
        }
    }
    g_object_set_data(G_OBJECT(mi), "xfdesktop-launcher-type", "Application");
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu2), mi);
    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_file_icon_menu_create_launcher),
                     fmanager);
    
    mi = gtk_image_menu_item_new_with_mnemonic(_("_URL Link..."));
    pix = xfce_themed_icon_load("gnome-fs-bookmark", w);  /* FIXME: icon naming spec */
    if(pix) {
        img = gtk_image_new_from_pixbuf(pix);
        gtk_widget_show(img);
        g_object_unref(G_OBJECT(pix));
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    }
    g_object_set_data(G_OBJECT(mi), "xfdesktop-launcher-type", "Link");
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu2), mi);
    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_file_icon_menu_create_launcher),
                     fmanager);
    
    mi = gtk_image_menu_item_new_with_mnemonic(_("_Folder..."));
    minfo = thunar_vfs_mime_database_get_info(thunar_mime_database,
                                              "inode/directory");
    if(minfo) {
        const gchar *icon_name = thunar_vfs_mime_info_lookup_icon_name(minfo,
                                                                       gtk_icon_theme_get_default());
        pix = xfce_themed_icon_load(icon_name, w);
        if(pix) {
            img = gtk_image_new_from_pixbuf(pix);
            gtk_widget_show(img);
            g_object_unref(G_OBJECT(pix));
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
        }
    }
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu2), mi);
    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_file_icon_menu_create_folder),
                     fmanager);
    
    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu2), mi);
    
    templates_path_str = g_build_filename(xfce_get_homedir(),
                                          "Templates",
                                          NULL);
    templates_path = thunar_vfs_path_new(templates_path_str, NULL);
    g_free(templates_path_str);
    if(templates_path) {
        have_templates = xfdesktop_file_icon_menu_fill_template_menu(menu2,
                                                                     templates_path,
                                                                     fmanager);
        thunar_vfs_path_unref(templates_path);
    }
    
    img = gtk_image_new_from_stock(GTK_STOCK_NEW, GTK_ICON_SIZE_MENU);
    gtk_widget_show(img);
    mi = gtk_image_menu_item_new_with_mnemonic(_("_Empty File"));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu2), mi);
    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_file_icon_template_item_activated),
                     fmanager);
    
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0,
                   gtk_get_current_event_time());
    
    /* don't free |selected|.  the menu deactivated handler does that */
}

static gboolean
xfdesktop_file_icon_manager_save_icons(gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    gchar relpath[PATH_MAX];
    XfceRc *rcfile;
    GList *l;
    XfdesktopIcon *icon;
    guint16 row, col;
    
    fmanager->priv->save_icons_id = 0;
    
    g_return_val_if_fail(fmanager->priv->icons_to_save, FALSE);
    
    g_snprintf(relpath, PATH_MAX, "xfce4/desktop/icons.screen%d.rc",
               gdk_screen_get_number(fmanager->priv->gscreen));
    
    rcfile = xfce_rc_config_open(XFCE_RESOURCE_CACHE, relpath, FALSE);
    if(!rcfile) {
        g_critical("Unable to determine location of icon position cache file.  " \
                   "Icon positions will not be saved.");
        return FALSE;
    }
    
    for(l = fmanager->priv->icons_to_save; l; l = l->next) {
        icon = XFDESKTOP_ICON(l->data);
        if(xfdesktop_icon_get_position(icon, &row, &col)) {
            xfce_rc_set_group(rcfile, xfdesktop_icon_peek_label(icon));
            xfce_rc_write_int_entry(rcfile, "row", row);
            xfce_rc_write_int_entry(rcfile, "col", col);
        }
        g_object_unref(G_OBJECT(icon));
    }
    g_list_free(fmanager->priv->icons_to_save);
    fmanager->priv->icons_to_save = NULL;
    
    xfce_rc_flush(rcfile);
    xfce_rc_close(rcfile);
    
    return FALSE;
}

static void
xfdesktop_file_icon_position_changed(XfdesktopFileIcon *icon,
                                     gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    
    if(fmanager->priv->save_icons_id)
        g_source_remove(fmanager->priv->save_icons_id);
    
    fmanager->priv->icons_to_save = g_list_prepend(fmanager->priv->icons_to_save,
                                                   g_object_ref(G_OBJECT(icon)));
    
    fmanager->priv->save_icons_id = g_timeout_add(SAVE_DELAY,
                                                  xfdesktop_file_icon_manager_save_icons,
                                                  fmanager);
}


/*   *****   */

static gboolean
xfdesktop_file_icon_manager_get_cached_icon_position(XfdesktopFileIconManager *fmanager,
                                                     const gchar *name,
                                                     gint16 *row,
                                                     gint16 *col)
{
    gchar relpath[PATH_MAX];
    XfceRc *rcfile;
    gboolean ret = FALSE;
    
    g_snprintf(relpath, PATH_MAX, "xfce4/desktop/icons.screen%d.rc",
               gdk_screen_get_number(fmanager->priv->gscreen));
    rcfile = xfce_rc_config_open(XFCE_RESOURCE_CACHE, relpath, TRUE);
    if(rcfile) {
        if(xfce_rc_has_group(rcfile, name)) {
            xfce_rc_set_group(rcfile, name);
            *row = xfce_rc_read_int_entry(rcfile, "row", -1);
            *col = xfce_rc_read_int_entry(rcfile, "col", -1);
            if(row >= 0 && col >= 0)
                ret = TRUE;
        }
        xfce_rc_close(rcfile);
    }
    
    return ret;
}

static XfdesktopFileIcon *
xfdesktop_file_icon_manager_add_icon(XfdesktopFileIconManager *fmanager,
                                     ThunarVfsInfo *info,
                                     gboolean defer_if_missing)
{
    XfdesktopFileIcon *icon = NULL;
    gint16 row, col;
    gboolean do_add = FALSE;
    const gchar *name;
    
    icon = xfdesktop_file_icon_new(info, fmanager->priv->gscreen);
    
    name = xfdesktop_icon_peek_label(XFDESKTOP_ICON(icon));
    if(xfdesktop_file_icon_manager_get_cached_icon_position(fmanager, name,
                                                            &row, &col))
    {
        DBG("attempting to set icon '%s' to position (%d,%d)", name, row, col);
        xfdesktop_icon_set_position(XFDESKTOP_ICON(icon), row, col);
        do_add = TRUE;
    } else {
        if(defer_if_missing) {
            fmanager->priv->deferred_icons = g_list_prepend(fmanager->priv->deferred_icons,
                                                            thunar_vfs_info_ref(info));
        } else
            do_add = TRUE;
    }
    
    if(do_add) {
        g_signal_connect(G_OBJECT(icon), "position-changed",
                         G_CALLBACK(xfdesktop_file_icon_position_changed),
                         fmanager);
        g_signal_connect(G_OBJECT(icon), "activated",
                         G_CALLBACK(xfdesktop_file_icon_activated), fmanager);
        g_signal_connect(G_OBJECT(icon), "menu-popup",
                         G_CALLBACK(xfdesktop_file_icon_menu_popup), fmanager);
        xfdesktop_icon_view_add_item(fmanager->priv->icon_view,
                                     XFDESKTOP_ICON(icon));
        g_hash_table_replace(fmanager->priv->icons,
                             thunar_vfs_path_ref(info->path), icon);
    } else {
        g_object_unref(G_OBJECT(icon));
        icon = NULL;
    }
    
    return icon;
}

static XfdesktopFileIcon *
xfdesktop_file_icon_manager_add_volume_icon(XfdesktopFileIconManager *fmanager,
                                            ThunarVfsVolume *volume)
{
    XfdesktopFileIcon *icon;
    const gchar *name;
    gint16 row, col;
    
    icon = xfdesktop_file_icon_new_for_volume(volume,
                                              fmanager->priv->gscreen);
    
    name = xfdesktop_icon_peek_label(XFDESKTOP_ICON(icon));
    if(xfdesktop_file_icon_manager_get_cached_icon_position(fmanager, name,
                                                            &row, &col))
    {
        DBG("attempting to set icon '%s' to position (%d,%d)", name, row, col);
        xfdesktop_icon_set_position(XFDESKTOP_ICON(icon), row, col);
    }
    
    g_signal_connect(G_OBJECT(icon), "position-changed",
                     G_CALLBACK(xfdesktop_file_icon_position_changed),
                     fmanager);
    g_signal_connect(G_OBJECT(icon), "activated",
                     G_CALLBACK(xfdesktop_file_icon_activated), fmanager);
    g_signal_connect(G_OBJECT(icon), "menu-popup",
                     G_CALLBACK(xfdesktop_file_icon_menu_popup), fmanager);
    xfdesktop_icon_view_add_item(fmanager->priv->icon_view,
                                 XFDESKTOP_ICON(icon));
    g_hash_table_replace(fmanager->priv->removable_icons,
                         g_object_ref(G_OBJECT(volume)), icon);
    
    return icon;
}

static gboolean
xfdesktop_file_icon_manager_key_press(GtkWidget *widget,
                                      GdkEventKey *evt,
                                      gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GList *selected;
    
    switch(evt->keyval) {
        case GDK_Delete:
        case GDK_KP_Delete:
            xfdesktop_file_icon_manager_delete_selected(fmanager);
            break;
        
        case GDK_c:
        case GDK_C:
            if(!(evt->state & GDK_CONTROL_MASK)
               || (evt->state & (GDK_SHIFT_MASK|GDK_MOD1_MASK|GDK_MOD4_MASK)))
            {
                return FALSE;
            }
            selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
            if(selected) {
                xfdesktop_clipboard_manager_copy_files(clipboard_manager,
                                                       selected);
                g_list_free(selected);
            }
            break;
        
        case GDK_x:
        case GDK_X:
            if(!(evt->state & GDK_CONTROL_MASK)
               || (evt->state & (GDK_SHIFT_MASK|GDK_MOD1_MASK|GDK_MOD4_MASK)))
            {
                return FALSE;
            }
            selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
            if(selected) {
                xfdesktop_clipboard_manager_cut_files(clipboard_manager,
                                                       selected);
                g_list_free(selected);
            }
            return TRUE;
        
        case GDK_r:
        case GDK_R:
            if(!(evt->state & GDK_CONTROL_MASK)
               || (evt->state & (GDK_SHIFT_MASK|GDK_MOD1_MASK|GDK_MOD4_MASK)))
            {
                return FALSE;
            }
            /* fall through */
        case GDK_F5:
            if(fmanager->priv->show_removable_media) {
                /* ensure we don't get double signal connects and whatnot */
                xfdesktop_file_icon_manager_remove_removable_media(fmanager);
            }
            xfdesktop_icon_view_remove_all(fmanager->priv->icon_view);
            if(fmanager->priv->icons) {
                g_hash_table_foreach_remove(fmanager->priv->icons,
                                            (GHRFunc)gtk_true, NULL);
            }
            if(fmanager->priv->show_removable_media)
                xfdesktop_file_icon_manager_load_removable_media(fmanager);
            xfdesktop_file_icon_manager_load_desktop_folder(fmanager);
            return TRUE;
    }
    
    return FALSE;
}

static void
xfdesktop_file_icon_manager_vfs_monitor_cb(ThunarVfsMonitor *monitor,
                                           ThunarVfsMonitorHandle *handle,
                                           ThunarVfsMonitorEvent event,
                                           ThunarVfsPath *handle_path,
                                           ThunarVfsPath *event_path,
                                           gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon;
    ThunarVfsInfo *info;
    
    switch(event) {
        case THUNAR_VFS_MONITOR_EVENT_CHANGED:
            DBG("got changed event");
            
            icon = g_hash_table_lookup(fmanager->priv->icons, event_path);
            if(icon) {
                DBG("found event_path in HT");
                
                info = thunar_vfs_info_new_for_path(event_path, NULL);
                if(info) {
                    xfdesktop_file_icon_update_info(icon, info);
                    thunar_vfs_info_unref(info);
                }
                
                break;
            }
            /* fall through - not sure why this is needed sometimes */
        
        case THUNAR_VFS_MONITOR_EVENT_CREATED:
            DBG("got created event");
            
            /* first make sure we don't already have an icon for this path.
             * this seems to be necessary as thunar-vfs does not emit a
             * DELETED signal if a file is overwritten with another file of
             * the same name. */
            icon = g_hash_table_lookup(fmanager->priv->icons, event_path);
            if(icon) {
                xfdesktop_icon_view_remove_item(fmanager->priv->icon_view,
                                                XFDESKTOP_ICON(icon));
                g_hash_table_remove(fmanager->priv->icons, event_path);
            }
            
            info = thunar_vfs_info_new_for_path(event_path, NULL);
            if(info) {
                if((info->flags & THUNAR_VFS_FILE_FLAGS_HIDDEN) == 0) {
                    thunar_vfs_path_ref(event_path);
                    xfdesktop_file_icon_manager_add_icon(fmanager,
                                                         info, FALSE);
                }
                
                thunar_vfs_info_unref(info);
            }
            break;
        
        case THUNAR_VFS_MONITOR_EVENT_DELETED:
            DBG("got deleted event");
            icon = g_hash_table_lookup(fmanager->priv->icons, event_path);
            if(icon) {
                xfdesktop_icon_view_remove_item(fmanager->priv->icon_view,
                                                XFDESKTOP_ICON(icon));
                g_hash_table_remove(fmanager->priv->icons, event_path);
            }
            break;
        
    }
}


static gboolean
xfdesktop_file_icon_manager_listdir_infos_ready_cb(ThunarVfsJob *job,
                                                   GList *infos,
                                                   gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GList *l;
    ThunarVfsInfo *info;

    g_return_val_if_fail(job == fmanager->priv->list_job, FALSE);
    
    TRACE("entering");
    
    for(l = infos; l; l = l->next) {
        info = l->data;
        
        DBG("got a ThunarVfsInfo: %s", info->display_name);
        
        if((info->flags & THUNAR_VFS_FILE_FLAGS_HIDDEN) != 0)
            continue;
        
        thunar_vfs_path_ref(info->path);
        xfdesktop_file_icon_manager_add_icon(fmanager, info, TRUE);
    }
    
    return FALSE;
}

static void
xfdesktop_file_icon_manager_listdir_finished_cb(ThunarVfsJob *job,
                                                gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GList *l;
    
    g_return_if_fail(job == fmanager->priv->list_job);
    
    TRACE("entering");
    
    if(fmanager->priv->deferred_icons) {
        for(l = fmanager->priv->deferred_icons; l; l = l->next) {
            xfdesktop_file_icon_manager_add_icon(fmanager,
                                                 (ThunarVfsInfo *)l->data,
                                                 FALSE);
            thunar_vfs_info_unref((ThunarVfsInfo *)l->data);
        }
        g_list_free(fmanager->priv->deferred_icons);
        fmanager->priv->deferred_icons = NULL;
    }
    
    if(!fmanager->priv->handle) {
        fmanager->priv->handle = thunar_vfs_monitor_add_directory(fmanager->priv->monitor,
                                                                  fmanager->priv->folder,
                                                                  xfdesktop_file_icon_manager_vfs_monitor_cb,
                                                                  fmanager);
    }
    
    g_object_unref(G_OBJECT(job));
    fmanager->priv->list_job = NULL;
}

static void
xfdesktop_file_icon_manager_listdir_error_cb(ThunarVfsJob *job,
                                             GError *error,
                                             gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    
    g_return_if_fail(job == fmanager->priv->list_job);
    
    g_warning("Got error from thunar-vfs on loading directory: %s",
              error->message);
}

static void
xfdesktop_file_icon_manager_load_desktop_folder(XfdesktopFileIconManager *fmanager)
{
    if(fmanager->priv->deferred_icons) {
        g_list_foreach(fmanager->priv->deferred_icons,
                       (GFunc)thunar_vfs_info_unref, NULL);
        g_list_free(fmanager->priv->deferred_icons);
        fmanager->priv->deferred_icons = NULL;
    }
    
    if(fmanager->priv->list_job) {
        thunar_vfs_job_cancel(fmanager->priv->list_job);
        g_object_unref(G_OBJECT(fmanager->priv->list_job));
    }
    
    fmanager->priv->list_job = thunar_vfs_listdir(fmanager->priv->folder,
                                                  NULL);
    
    g_signal_connect(G_OBJECT(fmanager->priv->list_job), "error",
                     G_CALLBACK(xfdesktop_file_icon_manager_listdir_error_cb),
                     fmanager);
    g_signal_connect(G_OBJECT(fmanager->priv->list_job), "finished",
                     G_CALLBACK(xfdesktop_file_icon_manager_listdir_finished_cb),
                     fmanager);
    g_signal_connect(G_OBJECT(fmanager->priv->list_job), "infos-ready",
                     G_CALLBACK(xfdesktop_file_icon_manager_listdir_infos_ready_cb),
                     fmanager);
}

static void
xfdesktop_file_icon_manager_check_icons_opacity(gpointer key,
                                                gpointer value,
                                                gpointer data)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(value);
    XfdesktopClipboardManager *cmanager = XFDESKTOP_CLIPBOARD_MANAGER(data);
    
    if(G_UNLIKELY(xfdesktop_clipboard_manager_has_cutted_file(cmanager, icon)))
        xfdesktop_file_icon_set_pixbuf_opacity(icon, 50);
    else
        xfdesktop_file_icon_set_pixbuf_opacity(icon, 100);
}

static void
xfdesktop_file_icon_manager_clipboard_changed(XfdesktopClipboardManager *cmanager,
                                              gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    
    TRACE("entering");
    
    /* slooow? */
    g_hash_table_foreach(fmanager->priv->icons,
                         xfdesktop_file_icon_manager_check_icons_opacity,
                         cmanager);
}


static void
xfdesktop_file_icon_manager_volume_changed(ThunarVfsVolume *volume,
                                           gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopIcon *icon;
    gboolean is_present = thunar_vfs_volume_is_present(volume);
    
    icon = g_hash_table_lookup(fmanager->priv->removable_icons, volume);

    if(is_present && !icon)
        xfdesktop_file_icon_manager_add_volume_icon(fmanager, volume);
    else if(!is_present && icon) {
        xfdesktop_icon_view_remove_item(fmanager->priv->icon_view, icon);
        g_hash_table_remove(fmanager->priv->removable_icons, volume);
    }
}

static void
xfdesktop_file_icon_manager_add_removable_volume(XfdesktopFileIconManager *fmanager,
                                                 ThunarVfsVolume *volume)
{
    if(!thunar_vfs_volume_is_removable(volume))
        return;
    
    if(thunar_vfs_volume_is_present(volume))
        xfdesktop_file_icon_manager_add_volume_icon(fmanager, volume);
    
    g_signal_connect(G_OBJECT(volume), "changed",
                     G_CALLBACK(xfdesktop_file_icon_manager_volume_changed),
                     fmanager);
}

static void
xfdesktop_file_icon_manager_volumes_added(ThunarVfsVolumeManager *vmanager,
                                          GList *volumes,
                                          gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GList *l;
    ThunarVfsVolume *volume;
    
    for(l = volumes; l; l = l->next) {
        volume = THUNAR_VFS_VOLUME(l->data);
        xfdesktop_file_icon_manager_add_removable_volume(fmanager, volume);
    }
}

static void
xfdesktop_file_icon_manager_volumes_removed(ThunarVfsVolumeManager *vmanager,
                                            GList *volumes,
                                            gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GList *l;
    ThunarVfsVolume *volume;
    XfdesktopIcon *icon;
    
    for(l = volumes; l; l = l->next) {
        volume = THUNAR_VFS_VOLUME(l->data);
        icon = g_hash_table_lookup(fmanager->priv->removable_icons, volume);
        if(icon) {
            xfdesktop_icon_view_remove_item(fmanager->priv->icon_view, icon);
            g_hash_table_remove(fmanager->priv->removable_icons, volume);
        }
    }
}

static void
xfdesktop_file_icon_manager_load_removable_media(XfdesktopFileIconManager *fmanager)
{
    GList *volumes, *l;
    ThunarVfsVolume *volume;
    
    /* ensure we don't re-enter if we're already set up */
    g_return_if_fail(!fmanager->priv->removable_icons);
    
    if(!thunar_volume_manager) {
        thunar_volume_manager = thunar_vfs_volume_manager_get_default();
        g_object_add_weak_pointer(G_OBJECT(thunar_volume_manager),
                                  (gpointer)&thunar_volume_manager);
    } else
       g_object_ref(G_OBJECT(thunar_volume_manager));
    
    fmanager->priv->removable_icons = g_hash_table_new_full(g_direct_hash,
                                                            g_direct_equal,
                                                            (GDestroyNotify)g_object_unref,
                                                            (GDestroyNotify)g_object_unref);
    
    volumes = thunar_vfs_volume_manager_get_volumes(thunar_volume_manager);
    
    for(l = volumes; l; l = l->next) {
        volume = THUNAR_VFS_VOLUME(l->data);
        xfdesktop_file_icon_manager_add_removable_volume(fmanager,
                                                         volume);
    }
    
    g_signal_connect(G_OBJECT(thunar_volume_manager), "volumes-added",
                     G_CALLBACK(xfdesktop_file_icon_manager_volumes_added),
                     fmanager);
    g_signal_connect(G_OBJECT(thunar_volume_manager), "volumes-removed",
                     G_CALLBACK(xfdesktop_file_icon_manager_volumes_removed),
                     fmanager);
}

static void
xfdesktop_file_icon_manager_ht_remove_removable_media(gpointer key,
                                                      gpointer value,
                                                      gpointer user_data)
{
    XfdesktopIcon *icon = XFDESKTOP_ICON(value);
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    
    xfdesktop_icon_view_remove_item(fmanager->priv->icon_view, icon);
}

static void
xfdesktop_file_icon_manager_remove_removable_media(XfdesktopFileIconManager *fmanager)
{
    GList *volumes, *l;
    
    g_return_if_fail(fmanager->priv->removable_icons);
    
    volumes = thunar_vfs_volume_manager_get_volumes(thunar_volume_manager);
    for(l = volumes; l; l = l->next) {
        g_signal_handlers_disconnect_by_func(G_OBJECT(l->data),
                                             G_CALLBACK(xfdesktop_file_icon_manager_volume_changed),
                                             fmanager);
    }
    
    g_hash_table_foreach(fmanager->priv->icons,
                         xfdesktop_file_icon_manager_ht_remove_removable_media,
                         fmanager);
    g_hash_table_destroy(fmanager->priv->removable_icons);
    fmanager->priv->removable_icons = NULL;
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(thunar_volume_manager),
                                         G_CALLBACK(xfdesktop_file_icon_manager_volumes_added),
                                         fmanager);
    g_signal_handlers_disconnect_by_func(G_OBJECT(thunar_volume_manager),
                                         G_CALLBACK(xfdesktop_file_icon_manager_volumes_removed),
                                         fmanager);
    g_object_unref(G_OBJECT(thunar_volume_manager));
}


/* virtual functions */

static gboolean
xfdesktop_file_icon_manager_real_init(XfdesktopIconViewManager *manager,
                                      XfdesktopIconView *icon_view)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
#ifdef HAVE_THUNARX
    ThunarxProviderFactory *thunarx_pfac;
#endif
    
    g_return_val_if_fail(!fmanager->priv->inited, FALSE);
    
    fmanager->priv->icon_view = icon_view;
    
    fmanager->priv->gscreen = gtk_widget_get_screen(GTK_WIDGET(icon_view));
    
    if(!clipboard_manager) {
        GdkDisplay *gdpy = gdk_screen_get_display(fmanager->priv->gscreen);
        clipboard_manager = xfdesktop_clipboard_manager_get_for_display(gdpy);
        g_object_add_weak_pointer(G_OBJECT(clipboard_manager),
                                  (gpointer)&clipboard_manager);
    } else
        g_object_ref(G_OBJECT(clipboard_manager));
    
    g_signal_connect(G_OBJECT(clipboard_manager), "changed",
                     G_CALLBACK(xfdesktop_file_icon_manager_clipboard_changed),
                     fmanager);
    
    if(!thunar_mime_database) {
        thunar_mime_database = thunar_vfs_mime_database_get_default();
        g_object_add_weak_pointer(G_OBJECT(thunar_mime_database),
                                  (gpointer)&thunar_mime_database);
    } else
        g_object_ref(G_OBJECT(thunar_mime_database));
    
    xfdesktop_icon_view_set_allow_overlapping_drops(icon_view, TRUE);
    xfdesktop_icon_view_set_selection_mode(icon_view, GTK_SELECTION_MULTIPLE);
    xfdesktop_icon_view_enable_drag_source(icon_view,
                                           GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                                           drag_targets, n_drag_targets,
                                           GDK_ACTION_LINK | GDK_ACTION_COPY
                                           | GDK_ACTION_MOVE);
    xfdesktop_icon_view_enable_drag_dest(icon_view, drop_targets,
                                         n_drop_targets, GDK_ACTION_LINK
                                         | GDK_ACTION_COPY | GDK_ACTION_MOVE);
    
    g_signal_connect(G_OBJECT(xfdesktop_icon_view_get_window_widget(icon_view)),
                     "key-press-event",
                     G_CALLBACK(xfdesktop_file_icon_manager_key_press),
                     fmanager);
    
    thunar_vfs_init();
    
    DBG("Desktop path is '%s'", thunar_vfs_path_dup_string(fmanager->priv->folder));
    
    fmanager->priv->monitor = thunar_vfs_monitor_get_default();
    
    fmanager->priv->icons = g_hash_table_new_full(thunar_vfs_path_hash,
                                                  thunar_vfs_path_equal,
                                                  (GDestroyNotify)thunar_vfs_path_unref,
                                                  (GDestroyNotify)g_object_unref);
    
    
    if(fmanager->priv->show_removable_media)
        xfdesktop_file_icon_manager_load_removable_media(fmanager);
    xfdesktop_file_icon_manager_load_desktop_folder(fmanager);
    
#ifdef HAVE_THUNARX
    thunarx_pfac = thunarx_provider_factory_get_default();
    
    fmanager->priv->thunarx_menu_providers =
        thunarx_provider_factory_list_providers(thunarx_pfac,
                                                THUNARX_TYPE_MENU_PROVIDER);
    fmanager->priv->thunarx_properties_providers =
        thunarx_provider_factory_list_providers(thunarx_pfac,
                                                THUNARX_TYPE_PROPERTY_PAGE_PROVIDER);
    
    g_object_unref(G_OBJECT(thunarx_pfac));
#endif
    
    fmanager->priv->inited = TRUE;
    
    return TRUE;
}

static void
xfdesktop_file_icon_manager_fini(XfdesktopIconViewManager *manager)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    
    g_return_if_fail(fmanager->priv->inited);
    
    fmanager->priv->inited = FALSE;
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(clipboard_manager),
                                         G_CALLBACK(xfdesktop_file_icon_manager_clipboard_changed),
                                         fmanager);
    
    g_object_unref(G_OBJECT(clipboard_manager));
    g_object_unref(G_OBJECT(thunar_mime_database));
    
    if(fmanager->priv->show_removable_media)
        xfdesktop_file_icon_manager_remove_removable_media(fmanager);
    
    if(fmanager->priv->deferred_icons) {
        g_list_foreach(fmanager->priv->deferred_icons,
                       (GFunc)thunar_vfs_info_unref, NULL);
        g_list_free(fmanager->priv->deferred_icons);
        fmanager->priv->deferred_icons = NULL;
    }
    
    if(fmanager->priv->handle) {
        thunar_vfs_monitor_remove(fmanager->priv->monitor,
                                  fmanager->priv->handle);
        fmanager->priv->handle = NULL;
    }
    
    g_object_unref(G_OBJECT(fmanager->priv->monitor));
    fmanager->priv->monitor = NULL;
    
#ifdef HAVE_THUNARX
    g_list_foreach(fmanager->priv->thunarx_menu_providers,
                   (GFunc)g_object_unref, NULL);
    g_list_free(fmanager->priv->thunarx_menu_providers);
    
    g_list_foreach(fmanager->priv->thunarx_properties_providers,
                   (GFunc)g_object_unref, NULL);
    g_list_free(fmanager->priv->thunarx_properties_providers);
#endif
    
    thunar_vfs_shutdown();
    
    g_hash_table_destroy(fmanager->priv->icons);
    fmanager->priv->icons = NULL;
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(xfdesktop_icon_view_get_window_widget(fmanager->priv->icon_view)),
                                         G_CALLBACK(xfdesktop_file_icon_manager_key_press),
                                         fmanager);
    
    xfdesktop_icon_view_unset_drag_source(fmanager->priv->icon_view);
    xfdesktop_icon_view_unset_drag_dest(fmanager->priv->icon_view);
}

static gboolean
xfdesktop_file_icon_manager_drag_drop(XfdesktopIconViewManager *manager,
                                      XfdesktopIcon *drop_icon,
                                      GdkDragContext *context,
                                      guint16 row,
                                      guint16 col,
                                      guint time)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    GtkWidget *widget = GTK_WIDGET(fmanager->priv->icon_view);
    GdkAtom target;
    
    TRACE("entering");
    
    target = gtk_drag_dest_find_target(widget, context,
                                       fmanager->priv->drop_targets);
    if(target == GDK_NONE)
        return FALSE;
    else if(target == gdk_atom_intern("XdndDirectSave0", FALSE)) {
        /* X direct save protocol implementation copied more or less from
         * Thunar, Copyright (c) Benedikt Meurer */
        gint prop_len;
        guchar *prop_text = NULL;
        ThunarVfsPath *src_path, *path;
        gchar *uri = NULL;
        
        if(drop_icon) {
            const ThunarVfsInfo *info = xfdesktop_file_icon_peek_info(XFDESKTOP_FILE_ICON(drop_icon));
            if(!info)
                return FALSE;
            
            if(!(info->type & THUNAR_VFS_FILE_TYPE_DIRECTORY))
                return FALSE;
            
            src_path = info->path;
            
        } else
            src_path = fmanager->priv->folder;
        
        if(gdk_property_get(context->source_window,
                            gdk_atom_intern("XdndDirectSave0", FALSE),
                            gdk_atom_intern("text/plain", FALSE),
                            0, 1024, FALSE, NULL, NULL, &prop_len,
                            &prop_text) && prop_text)
        {
            prop_text = g_realloc(prop_text, prop_len + 1);
            prop_text[prop_len] = 0;
            
            path = thunar_vfs_path_relative(src_path, (const gchar *)prop_text);
            uri = thunar_vfs_path_dup_uri(path);
            
            gdk_property_change(context->source_window,
                                gdk_atom_intern("XdndDirectSave0", FALSE),
                                gdk_atom_intern("text/plain", FALSE), 8,
                                GDK_PROP_MODE_REPLACE, (const guchar *)uri,
                                strlen(uri));
            
            thunar_vfs_path_unref(path);
            g_free(prop_text);
            g_free(uri);
        }
        
        if(!uri)
            return FALSE;
    } else if(target == gdk_atom_intern("_NETSCAPE_URL", FALSE)) {
        if(drop_icon) {
            /* don't allow a drop on an icon that isn't a folder (i.e., not
             * on an icon that's an executable */
            const ThunarVfsInfo *info = xfdesktop_file_icon_peek_info(XFDESKTOP_FILE_ICON(drop_icon));
            if(!info || !(info->type & THUNAR_VFS_FILE_TYPE_DIRECTORY))
                return FALSE;
        }
    }
    
    TRACE("target good");
    
    gtk_drag_get_data(widget, context, target, time);
    
    return TRUE;
}

#if 0   /* FIXME: implement me */
static void
xfdesktop_file_icon_manager_fileop_error(ThunarVfsJob *job,
                                         GError *error,
                                         gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    XfdesktopFileUtilsFileop fileop = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(job),
                                                                        "--xfdesktop-fileop"));
    
    xfdesktop_file_utils_handle_fileop_error(GTK_WINDOW(toplevel), /* ... */);
#endif

static void
xfdesktop_file_icon_manager_drag_data_received(XfdesktopIconViewManager *manager,
                                               XfdesktopIcon *drop_icon,
                                               GdkDragContext *context,
                                               guint16 row,
                                               guint16 col,
                                               GtkSelectionData *data,
                                               guint info,
                                               guint time)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    XfdesktopFileIcon *file_icon = NULL;
    const ThunarVfsInfo *tinfo = NULL;
    gboolean copy_only = TRUE, drop_ok = FALSE;
    GList *path_list;
    
    if(info == TARGET_XDND_DIRECT_SAVE0) {
        /* we don't suppose XdndDirectSave stage 3, result F, i.e., the app
         * has to save the data itself given the filename we provided in
         * stage 1 */
        if(8 == data->format && 1 == data->length && 'F' == data->data[0]) {
            gdk_property_change(context->source_window,
                                gdk_atom_intern("XdndDirectSave0", FALSE),
                                gdk_atom_intern("text/plain", FALSE), 8,
                                GDK_PROP_MODE_REPLACE, (const guchar *)"", 0);
        } else if(8 == data->format && data->length == 1
                  && 'S' == data->data[0])
        {
            /* FIXME: do we really need to do anything here?  xfdesktop should
             * detect when something changes on its own */
        }
        
        drop_ok = TRUE;
    } else if(info == TARGET_NETSCAPE_URL) {
        /* data is "URL\nTITLE" */
        ThunarVfsPath *src_path = NULL;
        gchar *exo_desktop_item_edit = g_find_program_in_path("exo-desktop-item-edit");
        
        if(drop_icon) {
            const ThunarVfsInfo *info = xfdesktop_file_icon_peek_info(XFDESKTOP_FILE_ICON(drop_icon));
            if(info && info->type & THUNAR_VFS_FILE_TYPE_DIRECTORY)
                src_path = info->path;
        } else
            src_path = fmanager->priv->folder;
        
        if(src_path && exo_desktop_item_edit) {
            gchar **parts = g_strsplit((const gchar *)data->data, "\n", -1);
            
            if(2 == g_strv_length(parts)) {
                gchar *cwd = thunar_vfs_path_dup_string(src_path);
                gchar *myargv[16];
                gint i = 0;
                
                /* use the argv form so we don't have to worry about quoting
                 * the link title */
                myargv[i++] = exo_desktop_item_edit;
                myargv[i++] = "--type=Link";
                myargv[i++] = "--url";
                myargv[i++] = parts[0];
                myargv[i++] = "--name";
                myargv[i++] = parts[1];
                myargv[i++] = "--create-new";
                myargv[i++] = cwd;
                myargv[i++] = NULL;
                
                if(xfce_exec_argv(myargv, FALSE, FALSE, NULL))
                    drop_ok = TRUE;
                
                g_free(cwd);
            }
            
            g_strfreev(parts);
        }
        
        g_free(exo_desktop_item_edit);
    } else if(info == TARGET_TEXT_URI_LIST) {
        if(drop_icon) {
            file_icon = XFDESKTOP_FILE_ICON(drop_icon);
            tinfo = xfdesktop_file_icon_peek_info(file_icon);
        }
        
        copy_only = (context->suggested_action != GDK_ACTION_MOVE);
        
        path_list = thunar_vfs_path_list_from_string((gchar *)data->data, NULL);
    
        if(path_list) {
            if(tinfo && tinfo->flags & THUNAR_VFS_FILE_FLAGS_EXECUTABLE) {
                gboolean succeeded;
                GError *error = NULL;
                
                succeeded = thunar_vfs_info_execute(tinfo,
                                                    fmanager->priv->gscreen,
                                                    path_list,
                                                    xfce_get_homedir(),
                                                    &error);
                if(!succeeded) {
                    gchar *primary = g_strdup_printf(_("Failed to run \"%s\":"),
                                                     tinfo->display_name);
                    xfce_message_dialog(NULL, _("Run Error"),
                                        GTK_STOCK_DIALOG_ERROR,
                                        primary, error->message,
                                        GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                                        NULL);
                    g_free(primary);
                    g_error_free(error);
                } else
                    drop_ok = TRUE;
            } else {
                ThunarVfsJob *job = NULL;
                GList *dest_path_list = NULL, *l;
                const gchar *name;
                ThunarVfsPath *base_dest_path, *dest_path;
                
                if(tinfo && tinfo->type == THUNAR_VFS_FILE_TYPE_DIRECTORY) {
                    base_dest_path = thunar_vfs_path_relative(fmanager->priv->folder,
                                                              thunar_vfs_path_get_name(tinfo->path));
                } else
                    base_dest_path = thunar_vfs_path_ref(fmanager->priv->folder);
                
                for(l = path_list; l; l = l->next) {
                    name = thunar_vfs_path_get_name((ThunarVfsPath *)l->data);
                    dest_path = thunar_vfs_path_relative(base_dest_path,
                                                         name);
                    dest_path_list = g_list_prepend(dest_path_list, dest_path);
                }
                thunar_vfs_path_unref(base_dest_path);
                dest_path_list = g_list_reverse(dest_path_list);
                
                if(context->suggested_action == GDK_ACTION_LINK)
                    job = thunar_vfs_link_files(path_list, dest_path_list, NULL);
                else if(copy_only)
                    job = thunar_vfs_copy_files(path_list, dest_path_list, NULL);
                else
                    job = thunar_vfs_move_files(path_list, dest_path_list, NULL);
                
                if(job) {
                    drop_ok = TRUE;
                    
#if 0  /* FIXME: implement me: need way to pass multiple files */
                    g_signal_connect(G_OBJECT(job), "error",
                                     G_CALLBACK(xfdesktop_file_icon_manager_fileop_error),
                                     fmanager);
                    g_object_set_data(G_OBJECT(job), "--xfdesktop-fileop",
                                      GINT_TO_POINTER(context->suggested_action == GDK_ACTION_LINK
                                                      ? XFDESKTOP_FILE_UTILS_FILEOP_LINK
                                                      : (copy_only
                                                         ? XFDESKTOP_FILE_UTILS_FILEOP_COPY
                                                         : XFDESKTOP_FILE_UTILS_FILEOP_MOVE)));
#endif
                    g_signal_connect(G_OBJECT(job), "ask",
                                     G_CALLBACK(xfdesktop_file_icon_interactive_job_ask),
                                     fmanager);
                    g_signal_connect(G_OBJECT(job), "finished",
                                     G_CALLBACK(g_object_unref), NULL);
                }
                
                thunar_vfs_path_list_free(dest_path_list);
            }
            
            thunar_vfs_path_list_free(path_list);
        }
    }
    
    DBG("finishing drop on desktop from external source: drop_ok=%s, copy_only=%s",
        drop_ok?"TRUE":"FALSE", copy_only?"TRUE":"FALSE");
    
    gtk_drag_finish(context, drop_ok, !copy_only, time);
}

static void
xfdesktop_file_icon_manager_drag_data_get(XfdesktopIconViewManager *manager,
                                          GList *drag_icons,
                                          GdkDragContext *context,
                                          GtkSelectionData *data,
                                          guint info,
                                          guint time)
{
    GList *path_list;
    gchar *str;
    
    TRACE("entering");
    
    g_return_if_fail(drag_icons);
    g_return_if_fail(G_LIKELY(info == TARGET_TEXT_URI_LIST));
    
    path_list = xfdesktop_file_icon_list_to_path_list(drag_icons);
    str = thunar_vfs_path_list_to_string(path_list);
    gtk_selection_data_set(data, data->target, 8, (guchar *)str, strlen(str));
    
    g_free(str);
    thunar_vfs_path_list_free(path_list);
}


/* public api */

XfdesktopIconViewManager *
xfdesktop_file_icon_manager_new(ThunarVfsPath *folder)
{
    return g_object_new(XFDESKTOP_TYPE_FILE_ICON_MANAGER,
                        "folder", folder,
                        NULL);
}

void
xfdesktop_file_icon_manager_set_show_removable_media(XfdesktopFileIconManager *manager,
                                                     gboolean show_removable_media)
{
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MANAGER(manager));
    
    if(show_removable_media == manager->priv->show_removable_media)
        return;
    
    manager->priv->show_removable_media = show_removable_media;
    
    if(manager->priv->inited) {
        if(show_removable_media)
            xfdesktop_file_icon_manager_load_removable_media(manager);
        else
            xfdesktop_file_icon_manager_remove_removable_media(manager);
    }
}

gboolean
xfdesktop_file_icon_manager_get_show_removable_media(XfdesktopFileIconManager *manager)
{
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON_MANAGER(manager), FALSE);
    return manager->priv->show_removable_media;
}
