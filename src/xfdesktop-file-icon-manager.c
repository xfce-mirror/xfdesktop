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
#include "xfdesktop-regular-file-icon.h"
#include "xfdesktop-special-file-icon.h"
#include "xfdesktop-volume-icon.h"
#include "xfdesktop-clipboard-manager.h"
#include "xfdesktop-file-properties-dialog.h"
#include "xfdesktop-dbus-bindings-trash.h"
#include "xfdesktop-dbus-bindings-filemanager.h"
#include "xfdesktop-file-icon-manager.h"

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#define SAVE_DELAY 7000
#define BORDER     8


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
    XfdesktopFileIcon *desktop_icon;
    ThunarVfsMonitor *monitor;
    ThunarVfsMonitorHandle *handle;
    ThunarVfsJob *list_job;
    
    GHashTable *icons;
    GHashTable *removable_icons;
    GHashTable *special_icons;
    
    gboolean show_removable_media;
    gboolean show_special[XFDESKTOP_SPECIAL_FILE_ICON_TRASH+1];
    
    GList *icons_to_save;
    guint save_icons_id;
    
    GList *deferred_icons;
    
    GtkTargetList *drag_targets;
    GtkTargetList *drop_targets;
    
    GList *active_trash_calls;
    
#ifdef HAVE_THUNARX
    GList *thunarx_menu_providers;
    GList *thunarx_properties_providers;
#endif
};

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

static gboolean xfdesktop_file_icon_manager_check_create_desktop_folder(ThunarVfsPath *path);
static void xfdesktop_file_icon_manager_load_desktop_folder(XfdesktopFileIconManager *fmanager);
static void xfdesktop_file_icon_manager_load_removable_media(XfdesktopFileIconManager *fmanager);
static void xfdesktop_file_icon_manager_remove_removable_media(XfdesktopFileIconManager *fmanager);


G_DEFINE_TYPE_EXTENDED(XfdesktopFileIconManager,
                       xfdesktop_file_icon_manager,
                       G_TYPE_OBJECT, 0,
                       G_IMPLEMENT_INTERFACE(XFDESKTOP_TYPE_ICON_VIEW_MANAGER,
                                             xfdesktop_file_icon_manager_icon_view_manager_init))


typedef struct
{
    XfdesktopFileIconManager *fmanager;
    DBusGProxy *proxy;
    DBusGProxyCall *call;
    GList *files;
} XfdesktopTrashFilesData;

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
    
    g_type_class_add_private(klass, sizeof(XfdesktopFileIconManagerPrivate));
    
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
    fmanager->priv = G_TYPE_INSTANCE_GET_PRIVATE(fmanager,
                                                 XFDESKTOP_TYPE_FILE_ICON_MANAGER,
                                                 XfdesktopFileIconManagerPrivate);
    
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
            if(fmanager->priv->folder)
                xfdesktop_file_icon_manager_check_create_desktop_folder(fmanager->priv->folder);
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
    
    if(fmanager->priv->inited)
        xfdesktop_file_icon_manager_fini(XFDESKTOP_ICON_VIEW_MANAGER(fmanager));
    
    gtk_target_list_unref(fmanager->priv->drag_targets);
    gtk_target_list_unref(fmanager->priv->drop_targets);
    
    thunar_vfs_path_unref(fmanager->priv->folder);
    
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



/* FIXME: remove this before 4.4.0; leave it for now to migrate older beta
* installs from the old location */
static void
__migrate_old_icon_positions(XfdesktopFileIconManager *fmanager)
{
    gchar relpath[PATH_MAX], *old_file;
    
    g_snprintf(relpath, PATH_MAX, "xfce4/desktop/icons.screen%d.rc",
               gdk_screen_get_number(fmanager->priv->gscreen));
    
    old_file = xfce_resource_save_location(XFCE_RESOURCE_CACHE, relpath, FALSE);
    
    if(G_UNLIKELY(old_file) && g_file_test(old_file, G_FILE_TEST_EXISTS)) {
        gchar *new_file = xfce_resource_save_location(XFCE_RESOURCE_CONFIG,
                                                      relpath, FALSE);
        if(G_LIKELY(new_file)) {
            if(rename(old_file, new_file)) {
                /* grumble, have to do this the hard way */
                gchar *contents = NULL;
                gsize length = 0;
                GError *error = NULL;
                
                if(g_file_get_contents(old_file, &contents, &length, &error)) {
#if GLIB_CHECK_VERSION(2, 8, 0)
                    if(!g_file_set_contents(new_file, contents, length,
                                            &error))
                    {
                        g_critical("Unable to write to %s: %s", new_file,
                                   error->message);
                        g_error_free(error);
                    }
#else
                    FILE *fp = fopen(new_file, "w");
                    gboolean success = FALSE;
                    if(fp) {
                        success = (fwrite(contents, 1, length, fp) == length);
                        success = !fclose(fp);
                    }
                    
                    if(!success) {
                        g_critical("Unable to write to %s: %s", new_file,
                                   strerror(errno));
                    }
#endif
                    
                    g_free(contents);
                } else {
                    g_critical("Unable to read from %s: %s", old_file,
                               error->message);
                    g_error_free(error);
                }
            }
        } else
            g_critical("Unable to migrate icon position file to new location.");
        
        /* i debate removing the old file even if the migration failed,
         * but i think this is the best way to avoid bug reports that
         * aren't my problem. */
        unlink(old_file);
        
        g_free(new_file);
    }
    
    g_free(old_file);
}

static gboolean
xfdesktop_file_icon_manager_check_create_desktop_folder(ThunarVfsPath *path)
{
    gboolean ret = TRUE;
    gchar *pathname;
    
    g_return_val_if_fail(path, FALSE);
    
    pathname = thunar_vfs_path_dup_string(path);
    if(!g_file_test(pathname, G_FILE_TEST_EXISTS)) {
        /* would prefer to use thunar_vfs_make_directory() here,
         * but i don't want to use an async operation */
        if(mkdir(pathname, 0700)) {
            gchar *primary = g_markup_printf_escaped(_("Xfdesktop was unable to create the folder \"%s\" to store desktop items:"),
                                                     pathname);
            xfce_message_dialog(NULL, _("Create Folder Failed"),
                                GTK_STOCK_DIALOG_WARNING, primary,
                                strerror(errno), GTK_STOCK_CLOSE,
                                GTK_RESPONSE_ACCEPT, NULL);
            g_free(primary);
            
            ret = FALSE;
        }
    } else if(!g_file_test(pathname, G_FILE_TEST_IS_DIR)) {
        gchar *primary = g_markup_printf_escaped(_("Xfdesktop is unable to use \"%s\" to hold desktop items because it is not a folder."),
                                                 pathname);
        xfce_message_dialog(NULL, _("Create Folder Failed"),
                            GTK_STOCK_DIALOG_WARNING, primary,
                            _("Please delete or rename the file."),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                            NULL);
        g_free(primary);
        
        ret = FALSE;
    }
    
    g_free(pathname);
    
    return ret;
}


/* icon signal handlers */

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
    
    xfdesktop_icon_activated(icon);
}

static void
xfdesktop_file_icon_menu_open_all(GtkWidget *widget,
                                  gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GList *selected;
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(selected);
    
    g_list_foreach(selected, (GFunc)xfdesktop_icon_activated, NULL);
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
xfdesktop_file_icon_manager_delete_files(XfdesktopFileIconManager *fmanager,
                                         GList *files)
{
    GList *l;
    gchar *primary;
    gint ret = GTK_RESPONSE_CANCEL;
    XfdesktopIcon *icon;
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    if(g_list_length(files) == 1) {
        icon = XFDESKTOP_ICON(files->data);
        
        primary = g_markup_printf_escaped(_("Are you sure that you want to delete \"%s\"?"),
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
                                  g_list_length(files));
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
        for(l = files; l; l = l->next) {
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
        g_list_foreach(files, (GFunc)xfdesktop_file_icon_delete_file, NULL);
}

static void
xfdesktop_file_icon_manager_trash_files_cb(DBusGProxy *proxy,
                                           GError *error,
                                           gpointer user_data)
{
    XfdesktopTrashFilesData *tdata = user_data;
    
    if(error) {
        /* fallback to normal deletion */
        xfdesktop_file_icon_manager_delete_files(tdata->fmanager, tdata->files);
    }
    
    tdata->fmanager->priv->active_trash_calls = g_list_remove(tdata->fmanager->priv->active_trash_calls,
                                                              tdata);
    
    g_object_unref(G_OBJECT(tdata->proxy));
    g_list_foreach(tdata->files, (GFunc)g_object_unref, NULL);
    g_list_free(tdata->files);
    
    g_free(tdata);
}

static gboolean
xfdesktop_file_icon_manager_trash_files(XfdesktopFileIconManager *fmanager,
                                        GList *files)
{
    DBusGProxy *trash_proxy = xfdesktop_file_utils_peek_trash_proxy();
    DBusGProxyCall *call;
    gchar **uris, *display_name;
    GList *l;
    gint i, nfiles;
    const ThunarVfsInfo *info;
    XfdesktopTrashFilesData *tdata;
    
    g_return_val_if_fail(files, TRUE);
    
    if(!trash_proxy)
        return FALSE;
    
    nfiles = g_list_length(files);
    uris = g_new(gchar *, nfiles + 1);
    
    for(l = files, i = 0; l; l = l->next, ++i) {
        info = xfdesktop_file_icon_peek_info(XFDESKTOP_FILE_ICON(l->data));
        uris[i] = thunar_vfs_path_dup_uri(info->path);
    }
    uris[nfiles] = NULL;
    
    display_name = gdk_screen_make_display_name(fmanager->priv->gscreen);
    
    tdata = g_new(XfdesktopTrashFilesData, 1);
    call = org_xfce_Trash_move_to_trash_async(trash_proxy, (const char **)uris,
                                              display_name,
                                              xfdesktop_file_icon_manager_trash_files_cb,
                                              tdata);
    
    if(call) {
        tdata->fmanager = fmanager;
        tdata->proxy = g_object_ref(G_OBJECT(trash_proxy));
        tdata->call = call;
        tdata->files = files;
        fmanager->priv->active_trash_calls = g_list_prepend(fmanager->priv->active_trash_calls,
                                                            tdata);
    } else
        g_free(tdata);
    
    g_strfreev(uris);
    g_free(display_name);
    
    return !!call;
}

static void
xfdesktop_file_icon_manager_delete_selected(XfdesktopFileIconManager *fmanager,
                                            gboolean force_delete)
{
    GList *selected, *l;
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(selected);
    
    /* remove anybody that's not deletable */
    for(l = selected; l; ) {
        if(!xfdesktop_file_icon_can_delete_file(XFDESKTOP_FILE_ICON(l->data))) {
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
    
    /* make sure the icons don't get destroyed while we're working */
    g_list_foreach(selected, (GFunc)g_object_ref, NULL);
    
    if(force_delete || !xfdesktop_file_icon_manager_trash_files(fmanager,
                                                                selected))
    {
        xfdesktop_file_icon_manager_delete_files(fmanager, selected);
        g_list_foreach(selected, (GFunc)g_object_unref, NULL);
        g_list_free(selected);
    }
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
        gchar *primary = g_markup_printf_escaped(_("Unable to launch \"%s\":"),
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
xfdesktop_file_icon_menu_open_folder(GtkWidget *widget,
                                     gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon;
    GList *selected;
    const ThunarVfsInfo *info;
    GtkWidget *toplevel;
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(g_list_length(selected) == 1);
    icon = XFDESKTOP_FILE_ICON(selected->data);
    g_list_free(selected);
    
    info = xfdesktop_file_icon_peek_info(icon);
    g_return_if_fail(info);
    
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    xfdesktop_file_utils_open_folder(info, fmanager->priv->gscreen,
                                     GTK_WINDOW(toplevel));
}

static void
xfdesktop_file_icon_manager_display_chooser_error(XfdesktopFileIconManager *fmanager)
{
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    xfce_message_dialog(GTK_WINDOW(toplevel),
                        _("Launch Error"), GTK_STOCK_DIALOG_ERROR,
                        _("The application chooser could not be opened."),
                        _("This feature requires a file manager service present (such as that supplied by Thunar)."),
                        GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
}

static void
xfdesktop_file_icon_manager_display_chooser_cb(DBusGProxy *proxy,
                                               GError *error,
                                               gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    xfdesktop_file_utils_set_window_cursor(GTK_WINDOW(toplevel), GDK_LEFT_PTR);
    if(error)
        xfdesktop_file_icon_manager_display_chooser_error(fmanager);
}

static void
xfdesktop_file_icon_menu_other_app(GtkWidget *widget,
                                   gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon;
    GList *selected;
    const ThunarVfsInfo *info;
    GtkWidget *toplevel;
    DBusGProxy *fileman_proxy;
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(g_list_length(selected) == 1);
    icon = XFDESKTOP_FILE_ICON(selected->data);
    g_list_free(selected);
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    info = xfdesktop_file_icon_peek_info(icon);
    g_return_if_fail(info);
    
    fileman_proxy = xfdesktop_file_utils_peek_filemanager_proxy();
    if(fileman_proxy) {
        gchar *uri = thunar_vfs_path_dup_uri(info->path);
        gchar *display_name = gdk_screen_make_display_name(fmanager->priv->gscreen);
        
        if(!org_xfce_FileManager_display_chooser_dialog_async(fileman_proxy,
                                                              uri, TRUE,
                                                              display_name,
                                                              xfdesktop_file_icon_manager_display_chooser_cb,
                                                              fmanager))
        {
            xfdesktop_file_icon_manager_display_chooser_error(fmanager);
        } else {
            GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
            xfdesktop_file_utils_set_window_cursor(GTK_WINDOW(toplevel),
                                                   GDK_WATCH);
        }
        g_free(uri);
        g_free(display_name);
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
xfdesktop_file_icon_menu_delete(GtkWidget *widget,
                                gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GdkModifierType state;
    gboolean force_delete = FALSE;
    
    if(gtk_get_current_event_state(&state) && state & GDK_SHIFT_MASK)
        force_delete = TRUE;
    
    xfdesktop_file_icon_manager_delete_selected(fmanager, force_delete);
}

static void
xfdesktop_file_icon_menu_properties(GtkWidget *widget,
                                    gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GtkWidget *parent;
    XfdesktopFileIcon *icon;
    GList *selected;
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(g_list_length(selected) == 1);
    icon = XFDESKTOP_FILE_ICON(selected->data);
    g_list_free(selected);
    
    parent = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    xfdesktop_file_properties_dialog_show(GTK_WINDOW(parent), icon,
#ifdef HAVE_THUNARX
                                          fmanager->priv->thunarx_properties_providers
#else
                                          NULL
#endif
                                          );
}

static void
xfdesktop_file_icon_manager_desktop_properties(GtkWidget *widget,
                                               gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GtkWidget *parent = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    xfdesktop_file_properties_dialog_show(GTK_WINDOW(parent),
                                          fmanager->priv->desktop_icon,
#ifdef HAVE_THUNARX
                                          fmanager->priv->thunarx_properties_providers
#else
                                          NULL
#endif
                                          );
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
    gchar *primary = g_markup_printf_escaped(_("Unable to create folder named \"%s\":"),
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
    gchar *primary = g_markup_printf_escaped(_("Unable to create file named \"%s\":"),
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
                gchar *primary = g_markup_printf_escaped(_("Unable to create file \"%s\":"), name);
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
xfdesktop_mcs_settings_launch(GtkWidget *w,
                              gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    gchar *display_name, *cmd;
    gchar buf[PATH_MAX];
    GError *error = NULL;
    
    display_name = gdk_screen_make_display_name(fmanager->priv->gscreen);
    
    cmd = g_find_program_in_path("xfce-setting-show");
    if(!cmd)
        cmd = g_strdup(BINDIR "/xfce-setting-show");
    
    g_snprintf(buf, PATH_MAX, "env DISPLAY=\"%s\" \"%s\" backdrop", display_name, cmd);
    g_free(display_name);
    g_free(cmd);
    
    if(!xfce_exec(buf, FALSE, TRUE, &error)) {
        GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
        /* printf is to be translator-friendly */
        gchar *primary = g_strdup_printf(_("Unable to launch \"%s\":"),
                                         "xfce-setting-show");
        xfce_message_dialog(GTK_WINDOW(toplevel), _("Launch Error"),
                            GTK_STOCK_DIALOG_ERROR, primary, error->message,
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        g_free(primary);
    }
}

static void
xfdesktop_file_icon_menu_popup(XfdesktopIcon *icon,
                               gpointer user_data)
{
    XfdesktopFileIcon *file_icon = XFDESKTOP_FILE_ICON(icon);
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    const ThunarVfsInfo *info = xfdesktop_file_icon_peek_info(file_icon);
    ThunarVfsMimeInfo *mime_info = info ? info->mime_info : NULL;
    GList *selected, *mime_apps, *l, *mime_actions = NULL;
    GtkWidget *menu, *mi, *img, *menu2, *menu3;
    gboolean multi_sel, have_templates = FALSE, got_custom_menu = FALSE;
    gint w = 0, h = 0;
    GdkPixbuf *pix;
    ThunarVfsMimeInfo *minfo;
    ThunarVfsPath *templates_path;
    gchar *templates_path_str;
#ifdef HAVE_THUNARX
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
#endif
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(selected);
    multi_sel = (g_list_length(selected) > 1);
    
    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, &h);
    
    if(!multi_sel) {
        menu = xfdesktop_icon_get_popup_menu(XFDESKTOP_ICON(selected->data));
        if(menu)
            got_custom_menu = TRUE;
    }
    
    if(!got_custom_menu)
        menu = gtk_menu_new();
    
    gtk_menu_set_screen(GTK_MENU(menu), fmanager->priv->gscreen);
    gtk_widget_show(menu);
    
    g_signal_connect_swapped(G_OBJECT(menu), "deactivate",
                             G_CALLBACK(g_idle_add),
                             xfdesktop_file_icon_menu_deactivate_idled);
    
    /* make sure icons don't get destroyed while menu is open */
    g_list_foreach(selected, (GFunc)g_object_ref, NULL);
    g_object_set_data(G_OBJECT(menu), "--xfdesktop-icon-list", selected);
    
    if(!got_custom_menu) {
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
            
            mi = gtk_separator_menu_item_new();
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        } else if(info) {
            if(info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY) {
                img = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
                gtk_widget_show(img);
                mi = gtk_image_menu_item_new_with_mnemonic(_("_Open"));
                gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
                gtk_widget_show(mi);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                g_signal_connect(G_OBJECT(mi), "activate",
                                 G_CALLBACK(xfdesktop_file_icon_menu_open_folder),
                                 fmanager);
                
                mi = gtk_separator_menu_item_new();
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
                    g_signal_connect(G_OBJECT(mi), "activate",
                                     G_CALLBACK(xfdesktop_file_icon_menu_executed),
                                     fmanager);
                    
                    mi = gtk_separator_menu_item_new();
                    gtk_widget_show(mi);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                    
                    if(!g_ascii_strcasecmp("application/x-desktop",
                                           thunar_vfs_mime_info_get_name(info->mime_info)))
                    {
                        ThunarVfsInfo *tmpinfo = (ThunarVfsInfo *)info;
                        img = gtk_image_new_from_stock(GTK_STOCK_EDIT, GTK_ICON_SIZE_MENU);
                        gtk_widget_show(img);
                        mi = gtk_image_menu_item_new_with_mnemonic(_("_Edit Launcher"));
                        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
                        g_object_set_data_full(G_OBJECT(mi), "thunar-vfs-info",
                                               thunar_vfs_info_ref(tmpinfo),
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
                }
                
                img = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
                gtk_widget_show(img);
                mi = gtk_image_menu_item_new_with_mnemonic(_("Open With Other _Application..."));
                gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
                gtk_widget_show(mi);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                g_signal_connect(G_OBJECT(mi), "activate",
                                 G_CALLBACK(xfdesktop_file_icon_menu_other_app),
                                 fmanager);
                
                mi = gtk_separator_menu_item_new();
                gtk_widget_show(mi);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            }
        }
        
        if(mime_actions) {
            ThunarVfsMimeHandler *mime_handler;
            
            for(l = mime_actions; l; l = l->next) {
                mime_handler = THUNAR_VFS_MIME_HANDLER(l->data);
                mi = xfdesktop_menu_item_from_mime_handler(fmanager, file_icon,
                                                           mime_handler,
                                                           w, FALSE, FALSE);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            }
            
            /* don't free the mime actions themselves */
            g_list_free(mime_actions);
            
            mi = gtk_separator_menu_item_new();
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        }
        
#ifdef HAVE_THUNARX
        if(!multi_sel && info && fmanager->priv->thunarx_menu_providers) {
            GList *menu_actions = NULL, *l;
            ThunarxMenuProvider *provider;
            
            if(info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY) {
                for(l = fmanager->priv->thunarx_menu_providers; l; l = l->next) {
                    provider = THUNARX_MENU_PROVIDER(l->data);
                    menu_actions = g_list_concat(menu_actions,
                                                 thunarx_menu_provider_get_folder_actions(provider,
                                                                                          toplevel,
                                                                                          THUNARX_FILE_INFO(file_icon)));
                }
            } else {
                for(l = fmanager->priv->thunarx_menu_providers; l; l = l->next) {
                    provider = THUNARX_MENU_PROVIDER(l->data);
                    menu_actions = g_list_concat(menu_actions,
                                                 thunarx_menu_provider_get_file_actions(provider,
                                                                                        toplevel,
                                                                                        selected));
                }
            }
            
            if(menu_actions) {
                xfdesktop_menu_shell_append_action_list(GTK_MENU_SHELL(menu),
                                                        menu_actions);
                g_list_foreach(menu_actions, (GFunc)g_object_unref, NULL);
                g_list_free(menu_actions);
                
                mi = gtk_separator_menu_item_new();
                gtk_widget_show(mi);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            }
        }
#endif
        
        mi = gtk_image_menu_item_new_from_stock(GTK_STOCK_COPY, NULL);
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        g_signal_connect(G_OBJECT(mi), "activate",
                         G_CALLBACK(xfdesktop_file_icon_menu_copy), fmanager);
        
        mi = gtk_image_menu_item_new_from_stock(GTK_STOCK_CUT, NULL);
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        if(multi_sel || xfdesktop_file_icon_can_delete_file(file_icon)) {
            g_signal_connect(G_OBJECT(mi), "activate",
                             G_CALLBACK(xfdesktop_file_icon_menu_cut),
                             fmanager);
        } else
            gtk_widget_set_sensitive(mi, FALSE);
        
        mi = gtk_image_menu_item_new_from_stock(GTK_STOCK_DELETE, NULL);
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        if(multi_sel || xfdesktop_file_icon_can_delete_file(file_icon)) {
            g_signal_connect(G_OBJECT(mi), "activate",
                             G_CALLBACK(xfdesktop_file_icon_menu_delete), 
                             fmanager);
        } else
            gtk_widget_set_sensitive(mi, FALSE);
        
        mi = gtk_separator_menu_item_new();
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        
        mi = gtk_image_menu_item_new_with_mnemonic(_("_Rename..."));
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        if(!multi_sel && xfdesktop_file_icon_can_rename_file(file_icon)) {
            g_signal_connect(G_OBJECT(mi), "activate",
                             G_CALLBACK(xfdesktop_file_icon_menu_rename),
                             fmanager);
        } else
            gtk_widget_set_sensitive(mi, FALSE);
    
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
    }
    
    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    
    mi = gtk_image_menu_item_new_with_mnemonic(_("Des_ktop"));
    pix = xfce_themed_icon_load("user-desktop", w);
    if(!pix)
        pix = xfce_themed_icon_load("gnome-fs-desktop", w);
    if(!pix)
        pix = xfce_themed_icon_load("xfce4-backdrop", w);
    if(pix) {
        img = gtk_image_new_from_pixbuf(pix);
        gtk_widget_show(img);
        g_object_unref(G_OBJECT(pix));
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    }
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    
    menu2 = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), menu2);
    gtk_widget_show(menu2);
    
    mi = gtk_image_menu_item_new_with_mnemonic(_("Create _Launcher..."));
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
    
    mi = gtk_image_menu_item_new_with_mnemonic(_("Create _URL Link..."));
    pix = xfce_themed_icon_load("gnome-fs-bookmark", w);
    if(!pix)
        pix = xfce_themed_icon_load("emblem-favorite", w);
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
    
    mi = gtk_image_menu_item_new_with_mnemonic(_("Create _Folder..."));
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
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu2), mi);
    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_file_icon_menu_create_folder),
                     fmanager);
    
    mi = gtk_menu_item_new_with_mnemonic("Create From _Template");
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu2), mi);
    
    menu3 = gtk_menu_new();
    gtk_widget_show(menu3);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), menu3);
    
    templates_path_str = g_build_filename(xfce_get_homedir(),
                                          "Templates",
                                          NULL);
    templates_path = thunar_vfs_path_new(templates_path_str, NULL);
    g_free(templates_path_str);
    if(templates_path) {
        have_templates = xfdesktop_file_icon_menu_fill_template_menu(menu3,
                                                                     templates_path,
                                                                     fmanager);
        thunar_vfs_path_unref(templates_path);
    }
    
    img = gtk_image_new_from_stock(GTK_STOCK_NEW, GTK_ICON_SIZE_MENU);
    gtk_widget_show(img);
    mi = gtk_image_menu_item_new_with_mnemonic(_("_Empty File"));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);
    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_file_icon_template_item_activated),
                     fmanager);
    
    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu2), mi);
        
    mi = gtk_image_menu_item_new_from_stock(GTK_STOCK_PASTE, NULL);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu2), mi);
    /* FIXME: implement */
    gtk_widget_set_sensitive(mi, FALSE);

    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu2), mi);
        
#ifdef HAVE_THUNARX
    for(l = fmanager->priv->thunarx_menu_providers; l; l = l->next) {
        ThunarxMenuProvider *provider = THUNARX_MENU_PROVIDER(l->data);
        GList *menu_actions = thunarx_menu_provider_get_folder_actions(provider,
                                                                       toplevel,
                                                                       THUNARX_FILE_INFO(fmanager->priv->desktop_icon));
        xfdesktop_menu_shell_append_action_list(GTK_MENU_SHELL(menu2),
                                                menu_actions);
        g_list_foreach(menu_actions, (GFunc)g_object_unref, NULL);
        g_list_free(menu_actions);
    }
    
    mi = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu2), mi);
    gtk_widget_show(mi);
#endif
    
    img = gtk_image_new_from_stock(GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
    gtk_widget_show(img);
    mi = gtk_image_menu_item_new_with_mnemonic(_("Desktop _Settings..."));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu2), mi);
    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_mcs_settings_launch), fmanager);
    
    img = gtk_image_new_from_stock(GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_MENU);
    gtk_widget_show(img);
    mi = gtk_image_menu_item_new_with_mnemonic(_("_Desktop Properties..."));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu2), mi);
    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_file_icon_manager_desktop_properties),
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
    
    rcfile = xfce_rc_config_open(XFCE_RESOURCE_CONFIG, relpath, FALSE);
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
    rcfile = xfce_rc_config_open(XFCE_RESOURCE_CONFIG, relpath, TRUE);
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


#if defined(DEBUG) && DEBUG > 0
static GList *_alive_icon_list = NULL;

static void
_icon_notify_destroy(gpointer data,
                     GObject *obj)
{
    g_assert(g_list_find(_alive_icon_list, obj));
    _alive_icon_list = g_list_remove(_alive_icon_list, obj);
    
    DBG("icon finalized: '%s'", xfdesktop_icon_peek_label(XFDESKTOP_ICON(obj)));
}
#endif

static gboolean
xfdesktop_file_icon_manager_add_icon(XfdesktopFileIconManager *fmanager,
                                     XfdesktopFileIcon *icon,
                                     gboolean defer_if_missing)
{
    gint16 row = -1, col = -1;
    gboolean do_add = FALSE;
    const gchar *name;
    
    name = xfdesktop_icon_peek_label(XFDESKTOP_ICON(icon));
    if(xfdesktop_file_icon_manager_get_cached_icon_position(fmanager, name,
                                                            &row, &col))
    {
        DBG("attempting to set icon '%s' to position (%d,%d)", name, row, col);
        xfdesktop_icon_set_position(XFDESKTOP_ICON(icon), row, col);
        do_add = TRUE;
    } else {
        if(defer_if_missing) {
            ThunarVfsInfo *info = (ThunarVfsInfo *)xfdesktop_file_icon_peek_info(icon);
            if(info) {
                fmanager->priv->deferred_icons = g_list_prepend(fmanager->priv->deferred_icons,
                                                                thunar_vfs_info_ref(info));
            }
        } else
            do_add = TRUE;
    }
    
    if(do_add) {
        g_signal_connect(G_OBJECT(icon), "position-changed",
                         G_CALLBACK(xfdesktop_file_icon_position_changed),
                         fmanager);
        g_signal_connect(G_OBJECT(icon), "menu-popup",
                         G_CALLBACK(xfdesktop_file_icon_menu_popup), fmanager);
        xfdesktop_icon_view_add_item(fmanager->priv->icon_view,
                                     XFDESKTOP_ICON(icon));
    }
    
#if defined(DEBUG) && DEBUG > 0
    if(do_add) {
        _alive_icon_list = g_list_prepend(_alive_icon_list, icon);
        g_object_weak_ref(G_OBJECT(icon), _icon_notify_destroy, NULL);
    }
#endif
    
    return do_add;
}

static XfdesktopFileIcon *
xfdesktop_file_icon_manager_add_regular_icon(XfdesktopFileIconManager *fmanager,
                                             ThunarVfsInfo *info,
                                             gboolean defer_if_missing)
{
    XfdesktopRegularFileIcon *icon = NULL;
    
    g_return_val_if_fail(fmanager && info, NULL);
    
    /* should never return NULL */
    icon = xfdesktop_regular_file_icon_new(info, fmanager->priv->gscreen);
    
    if(xfdesktop_file_icon_manager_add_icon(fmanager,
                                             XFDESKTOP_FILE_ICON(icon),
                                             defer_if_missing))
    {
        g_hash_table_replace(fmanager->priv->icons,
                             thunar_vfs_path_ref(info->path), icon);
        return XFDESKTOP_FILE_ICON(icon);
    } else {
        g_object_unref(G_OBJECT(icon));
        return NULL;
    }
}

static XfdesktopFileIcon *
xfdesktop_file_icon_manager_add_volume_icon(XfdesktopFileIconManager *fmanager,
                                            ThunarVfsVolume *volume)
{
    XfdesktopVolumeIcon *icon;
    
    g_return_val_if_fail(fmanager && volume, NULL);
    
    /* should never return NULL */
    icon = xfdesktop_volume_icon_new(volume, fmanager->priv->gscreen);
    
    if(xfdesktop_file_icon_manager_add_icon(fmanager,
                                            XFDESKTOP_FILE_ICON(icon),
                                            FALSE))
    {
        g_hash_table_replace(fmanager->priv->removable_icons,
                             g_object_ref(G_OBJECT(volume)), icon);
        return XFDESKTOP_FILE_ICON(icon);
    } else {
        g_object_unref(G_OBJECT(icon));
        return NULL;
    }
}

static XfdesktopFileIcon *
xfdesktop_file_icon_manager_add_special_file_icon(XfdesktopFileIconManager *fmanager,
                                                  XfdesktopSpecialFileIconType type)
{
    XfdesktopSpecialFileIcon *icon;
    
    /* can return NULL if it's the trash icon and dbus isn't around */
    icon = xfdesktop_special_file_icon_new(type, fmanager->priv->gscreen);
    if(!icon)
        return NULL;
    
    if(xfdesktop_file_icon_manager_add_icon(fmanager,
                                            XFDESKTOP_FILE_ICON(icon),
                                            FALSE))
    {
        g_hash_table_replace(fmanager->priv->special_icons,
                             GINT_TO_POINTER(type), icon);
        return XFDESKTOP_FILE_ICON(icon);
    } else {
        g_object_unref(G_OBJECT(icon));
        return NULL;
    }
}

static gboolean
xfdesktop_remove_icons_ht(gpointer key,
                          gpointer value,
                          gpointer user_data)
{
    xfdesktop_icon_view_remove_item(XFDESKTOP_ICON_VIEW(user_data),
                                    XFDESKTOP_ICON(value));
    return TRUE;
}

#if defined(DEBUG) && DEBUG > 0
guint _xfdesktop_icon_view_n_items(XfdesktopIconView *icon_view);
#endif

static void
xfdesktop_file_icon_manager_refresh_icons(XfdesktopFileIconManager *fmanager)
{
    gint i;
    
    /* if a save is pending, flush icon positions */
    if(fmanager->priv->save_icons_id) {
        g_source_remove(fmanager->priv->save_icons_id);
        fmanager->priv->save_icons_id = 0;
        xfdesktop_file_icon_manager_save_icons(fmanager);
    }
    
    /* ditch removable media */
    if(fmanager->priv->show_removable_media)
        xfdesktop_file_icon_manager_remove_removable_media(fmanager);
    
    /* ditch special icons */
    for(i = 0; i <= XFDESKTOP_SPECIAL_FILE_ICON_TRASH; ++i) {
        XfdesktopIcon *icon = g_hash_table_lookup(fmanager->priv->special_icons,
                                                  GINT_TO_POINTER(i));
        if(icon) {
            xfdesktop_icon_view_remove_item(fmanager->priv->icon_view, icon);
            g_hash_table_remove(fmanager->priv->special_icons,
                                GINT_TO_POINTER(i));
        }
    }

    /* ditch normal icons */
    if(fmanager->priv->icons) {
        g_hash_table_foreach_remove(fmanager->priv->icons,
                                    (GHRFunc)xfdesktop_remove_icons_ht,
                                    fmanager->priv->icon_view);
    }
    
#if defined(DEBUG) && DEBUG > 0
    g_assert(_xfdesktop_icon_view_n_items(fmanager->priv->icon_view) == 0);
    g_assert(g_list_length(_alive_icon_list) == 0);
#endif
    
    /* clear out anything left in the icon view */
    xfdesktop_icon_view_remove_all(fmanager->priv->icon_view);
    
    /* add back the special icons */
    for(i = 0; i <= XFDESKTOP_SPECIAL_FILE_ICON_TRASH; ++i) {
        if(fmanager->priv->show_special[i])
            xfdesktop_file_icon_manager_add_special_file_icon(fmanager, i);
    }
    
    /* add back removable media */
    if(fmanager->priv->show_removable_media)
        xfdesktop_file_icon_manager_load_removable_media(fmanager);
    
    /* reload and add ~/Desktop/ */
    xfdesktop_file_icon_manager_load_desktop_folder(fmanager);
}

static gboolean
xfdesktop_file_icon_manager_key_press(GtkWidget *widget,
                                      GdkEventKey *evt,
                                      gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GList *selected;
    gboolean force_delete = FALSE;
    
    switch(evt->keyval) {
        case GDK_Delete:
        case GDK_KP_Delete:
            if(evt->state & GDK_SHIFT_MASK)
                force_delete = TRUE;
            xfdesktop_file_icon_manager_delete_selected(fmanager, force_delete);
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
            xfdesktop_file_icon_manager_refresh_icons(fmanager);
            return TRUE;
        
        case GDK_F2:
            selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
            if(g_list_length(selected) == 1) {
                XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(selected->data);
                if(xfdesktop_file_icon_can_rename_file(icon)) {
                    xfdesktop_file_icon_menu_rename(NULL, fmanager);
                    return TRUE;
                }
            }
            if(selected)
                g_list_free(selected);
            break; 
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
            
            /* make sure it's not the desktop folder itself */
            if(thunar_vfs_path_equal(fmanager->priv->folder, event_path))
                return;
            
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
                    /* FIXME: memleak? */
                    /*thunar_vfs_path_ref(event_path);*/
                    xfdesktop_file_icon_manager_add_regular_icon(fmanager,
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
            } else {
                const ThunarVfsInfo *info = xfdesktop_file_icon_peek_info(fmanager->priv->desktop_icon);
                if(!info || thunar_vfs_path_equal(event_path, info->path)) {
                    DBG("~/Desktop disappeared!");
                    /* yes, refresh before and after is correct */
                    xfdesktop_file_icon_manager_refresh_icons(fmanager);
                    xfdesktop_file_icon_manager_check_create_desktop_folder(fmanager->priv->folder);
                    xfdesktop_file_icon_manager_refresh_icons(fmanager);
                }
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
        
        /* FIXME: memleak? */
        /*thunar_vfs_path_ref(info->path);*/
        xfdesktop_file_icon_manager_add_regular_icon(fmanager, info, TRUE);
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
            xfdesktop_file_icon_manager_add_regular_icon(fmanager,
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
        g_signal_handlers_disconnect_by_func(G_OBJECT(fmanager->priv->list_job),
                                             G_CALLBACK(xfdesktop_file_icon_manager_listdir_error_cb),
                                             fmanager);
        g_signal_handlers_disconnect_by_func(G_OBJECT(fmanager->priv->list_job),
                                             G_CALLBACK(xfdesktop_file_icon_manager_listdir_finished_cb),
                                             fmanager);
        g_signal_handlers_disconnect_by_func(G_OBJECT(fmanager->priv->list_job),
                                             G_CALLBACK(xfdesktop_file_icon_manager_listdir_infos_ready_cb),
                                             fmanager);
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
    XfdesktopRegularFileIcon *icon = XFDESKTOP_REGULAR_FILE_ICON(value);
    XfdesktopClipboardManager *cmanager = XFDESKTOP_CLIPBOARD_MANAGER(data);
    
    if(G_UNLIKELY(xfdesktop_clipboard_manager_has_cutted_file(cmanager, XFDESKTOP_FILE_ICON(icon))))
        xfdesktop_regular_file_icon_set_pixbuf_opacity(icon, 50);
    else
        xfdesktop_regular_file_icon_set_pixbuf_opacity(icon, 100);
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
    
    g_hash_table_foreach(fmanager->priv->removable_icons,
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
    
    DBG("refcnt of volmanager: %d", G_OBJECT(thunar_volume_manager)->ref_count);
    g_object_unref(G_OBJECT(thunar_volume_manager));
}


/* virtual functions */

static gboolean
xfdesktop_file_icon_manager_real_init(XfdesktopIconViewManager *manager,
                                      XfdesktopIconView *icon_view)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    ThunarVfsInfo *desktop_info;
    gint i;
#ifdef HAVE_THUNARX
    ThunarxProviderFactory *thunarx_pfac;
#endif
    
    g_return_val_if_fail(!fmanager->priv->inited, FALSE);
    
    fmanager->priv->icon_view = icon_view;
    
    fmanager->priv->gscreen = gtk_widget_get_screen(GTK_WIDGET(icon_view));
    
    /* FIXME: remove for 4.4.0 */
    __migrate_old_icon_positions(fmanager);
    
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
    
    fmanager->priv->monitor = thunar_vfs_monitor_get_default();
    
    fmanager->priv->icons = g_hash_table_new_full(thunar_vfs_path_hash,
                                                  thunar_vfs_path_equal,
                                                  (GDestroyNotify)thunar_vfs_path_unref,
                                                  (GDestroyNotify)g_object_unref);
    
    fmanager->priv->special_icons = g_hash_table_new_full(g_direct_hash,
                                                          g_direct_equal,
                                                          NULL,
                                                          (GDestroyNotify)g_object_unref);
    
    if(!xfdesktop_file_utils_dbus_init())
        g_warning("Unable to initialise D-Bus.  Some xfdesktop features may be unavailable.");
    
    for(i = 0; i <= XFDESKTOP_SPECIAL_FILE_ICON_TRASH; ++i) {
        if(fmanager->priv->show_special[i])
            xfdesktop_file_icon_manager_add_special_file_icon(fmanager, i);
    }
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
    
    /* keep around a dummy icon for the desktop folder for use with thunarx
     * and the properties dialog */
    desktop_info = thunar_vfs_info_new_for_path(fmanager->priv->folder, NULL);
    fmanager->priv->desktop_icon = XFDESKTOP_FILE_ICON(xfdesktop_regular_file_icon_new(desktop_info,
                                                                                       fmanager->priv->gscreen));
    thunar_vfs_info_unref(desktop_info);

    fmanager->priv->inited = TRUE;
    
    return TRUE;
}

static void
xfdesktop_file_icon_manager_fini(XfdesktopIconViewManager *manager)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    gint i;
    
    g_return_if_fail(fmanager->priv->inited);
    
    fmanager->priv->inited = FALSE;
    
    if(fmanager->priv->list_job) {
        thunar_vfs_job_cancel(fmanager->priv->list_job);
        g_signal_handlers_disconnect_by_func(G_OBJECT(fmanager->priv->list_job),
                                             G_CALLBACK(xfdesktop_file_icon_manager_listdir_error_cb),
                                             fmanager);
        g_signal_handlers_disconnect_by_func(G_OBJECT(fmanager->priv->list_job),
                                             G_CALLBACK(xfdesktop_file_icon_manager_listdir_finished_cb),
                                             fmanager);
        g_signal_handlers_disconnect_by_func(G_OBJECT(fmanager->priv->list_job),
                                             G_CALLBACK(xfdesktop_file_icon_manager_listdir_infos_ready_cb),
                                             fmanager);
        g_object_unref(G_OBJECT(fmanager->priv->list_job));
        fmanager->priv->list_job = NULL;
    }
    
    if(fmanager->priv->active_trash_calls) {
        GList *l;
        XfdesktopTrashFilesData *tdata;
        
        for(l = fmanager->priv->active_trash_calls; l; l = l->next) {
            tdata = l->data;
            dbus_g_proxy_cancel_call(tdata->proxy, tdata->call);
            g_object_unref(tdata->proxy);
            g_list_foreach(tdata->files, (GFunc)g_object_unref, NULL);
            g_list_free(tdata->files);
            g_free(tdata);
        }
        
        g_list_free(fmanager->priv->active_trash_calls);
        fmanager->priv->active_trash_calls = NULL;
    }
    
    if(fmanager->priv->save_icons_id) {
        g_source_remove(fmanager->priv->save_icons_id);
        fmanager->priv->save_icons_id = 0;
        xfdesktop_file_icon_manager_save_icons(fmanager);
    }
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(clipboard_manager),
                                         G_CALLBACK(xfdesktop_file_icon_manager_clipboard_changed),
                                         fmanager);
    
    g_object_unref(G_OBJECT(clipboard_manager));
    g_object_unref(G_OBJECT(thunar_mime_database));
    
    if(fmanager->priv->show_removable_media)
        xfdesktop_file_icon_manager_remove_removable_media(fmanager);
    
    for(i = 0; i <= XFDESKTOP_SPECIAL_FILE_ICON_TRASH; ++i) {
        XfdesktopIcon *icon = g_hash_table_lookup(fmanager->priv->special_icons,
                                                  GINT_TO_POINTER(i));
        if(icon) {
            xfdesktop_icon_view_remove_item(fmanager->priv->icon_view, icon);
            g_hash_table_remove(fmanager->priv->special_icons,
                                GINT_TO_POINTER(i));
        }
    }

    if(fmanager->priv->icons) {
        g_hash_table_foreach_remove(fmanager->priv->icons,
                                    (GHRFunc)xfdesktop_remove_icons_ht,
                                    fmanager->priv->icon_view);
    }
    
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
    
    g_object_unref(G_OBJECT(fmanager->priv->desktop_icon));
    fmanager->priv->desktop_icon = NULL;
    
#ifdef HAVE_THUNARX
    g_list_foreach(fmanager->priv->thunarx_menu_providers,
                   (GFunc)g_object_unref, NULL);
    g_list_free(fmanager->priv->thunarx_menu_providers);
    
    g_list_foreach(fmanager->priv->thunarx_properties_providers,
                   (GFunc)g_object_unref, NULL);
    g_list_free(fmanager->priv->thunarx_properties_providers);
#endif
    
    g_hash_table_destroy(fmanager->priv->special_icons);
    fmanager->priv->special_icons = NULL;
    
    g_hash_table_destroy(fmanager->priv->icons);
    fmanager->priv->icons = NULL;
    
    xfdesktop_file_utils_dbus_cleanup();
    
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
        
        copy_only = (context->action != GDK_ACTION_MOVE);
        
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
                    gchar *primary = g_markup_printf_escaped(_("Failed to run \"%s\":"),
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
                /* FIXME: icky special-case hacks */
                gboolean dest_is_volume = (drop_icon
                                           && XFDESKTOP_IS_VOLUME_ICON(drop_icon));
                gboolean dest_is_special = (drop_icon
                                            && XFDESKTOP_IS_SPECIAL_FILE_ICON(drop_icon));
                gboolean dest_is_trash = (dest_is_special
                                          && XFDESKTOP_SPECIAL_FILE_ICON_TRASH
                                             == xfdesktop_special_file_icon_get_icon_type(XFDESKTOP_SPECIAL_FILE_ICON(drop_icon)));
                
                /* if it's a volume, but we don't have |tinfo|, this just isn't
                 * going to work */
                if(!tinfo && dest_is_volume) {
                    thunar_vfs_path_list_free(path_list);
                    gtk_drag_finish(context, FALSE, FALSE, time);
                    return;
                }
                
                if(tinfo && (dest_is_special || dest_is_volume))
                    base_dest_path = thunar_vfs_path_ref(tinfo->path);
                else if(tinfo && tinfo->type == THUNAR_VFS_FILE_TYPE_DIRECTORY) {
                    base_dest_path = thunar_vfs_path_relative(fmanager->priv->folder,
                                                              thunar_vfs_path_get_name(tinfo->path));
                } else
                    base_dest_path = thunar_vfs_path_ref(fmanager->priv->folder);
                
                for(l = path_list; l; l = l->next) {
                    ThunarVfsPath *path = (ThunarVfsPath *)l->data;
                    
                    /* only work with file:// URIs here */
                    if(thunar_vfs_path_get_scheme(path) != THUNAR_VFS_PATH_SCHEME_FILE)
                        continue;
                    /* root nodes cause crashes */
                    if(thunar_vfs_path_is_root(path))
                        continue;
                    
                    name = thunar_vfs_path_get_name(path);
                    dest_path = thunar_vfs_path_relative(base_dest_path,
                                                         name);
                    dest_path_list = g_list_prepend(dest_path_list, dest_path);
                }
                thunar_vfs_path_unref(base_dest_path);
                dest_path_list = g_list_reverse(dest_path_list);
                
                if(dest_path_list) {
                    if(context->action == GDK_ACTION_LINK && !dest_is_trash)
                        job = thunar_vfs_link_files(path_list, dest_path_list, NULL);
                    else if(copy_only && !dest_is_trash)
                        job = thunar_vfs_copy_files(path_list, dest_path_list, NULL);
                    else
                        job = thunar_vfs_move_files(path_list, dest_path_list, NULL);
                    
                    thunar_vfs_path_list_free(dest_path_list);
                }
                
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
    
    path_list = xfdesktop_file_utils_file_icon_list_to_path_list(drag_icons);
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
    
    if(!manager->priv->inited)
        return;
    
    if(show_removable_media)
        xfdesktop_file_icon_manager_load_removable_media(manager);
    else
        xfdesktop_file_icon_manager_remove_removable_media(manager);
}

gboolean
xfdesktop_file_icon_manager_get_show_removable_media(XfdesktopFileIconManager *manager)
{
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON_MANAGER(manager), FALSE);
    return manager->priv->show_removable_media;
}

void
xfdesktop_file_icon_manager_set_show_special_file(XfdesktopFileIconManager *manager,
                                                  XfdesktopSpecialFileIconType type,
                                                  gboolean show_special_file)
{
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MANAGER(manager));
    g_return_if_fail(type >= 0 && type <= XFDESKTOP_SPECIAL_FILE_ICON_TRASH);
    
    if(manager->priv->show_special[type] == show_special_file)
        return;
    
    manager->priv->show_special[type] = show_special_file;
    
    if(!manager->priv->inited)
        return;
    
    if(show_special_file) {
        g_return_if_fail(!g_hash_table_lookup(manager->priv->special_icons,
                                              GINT_TO_POINTER(type)));
        xfdesktop_file_icon_manager_add_special_file_icon(manager, type);
    } else {
        XfdesktopIcon *icon = g_hash_table_lookup(manager->priv->special_icons,
                                                  GINT_TO_POINTER(type));
        if(icon) {
            xfdesktop_icon_view_remove_item(manager->priv->icon_view, icon);
            g_hash_table_remove(manager->priv->special_icons,
                                GINT_TO_POINTER(type));
        }
    }
}

gboolean
xfdesktop_file_icon_manager_get_show_special_file(XfdesktopFileIconManager *manager,
                                                  XfdesktopSpecialFileIconType type)
{
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON_MANAGER(manager), FALSE);
    g_return_val_if_fail(type >= 0 && type <= XFDESKTOP_SPECIAL_FILE_ICON_TRASH,
                         FALSE);
    
    return manager->priv->show_special[type];
}
