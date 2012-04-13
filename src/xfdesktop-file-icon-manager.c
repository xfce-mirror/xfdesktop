/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006      Brian Tarricone, <bjt23@cornell.edu>
 *  Copyright(c) 2006      Benedikt Meurer, <benny@xfce.org>
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

#include <gio/gio.h>
#include <gdk/gdkkeysyms.h>

#ifdef HAVE_THUNARX
#include <thunarx/thunarx.h>
#endif

#include "xfce-desktop.h"
#include "xfdesktop-clipboard-manager.h"
#include "xfdesktop-common.h"
#include "xfdesktop-file-icon.h"
#include "xfdesktop-file-icon-manager.h"
#include "xfdesktop-file-utils.h"
#include "xfdesktop-file-manager-proxy.h"
#include "xfdesktop-icon-view.h"
#include "xfdesktop-regular-file-icon.h"
#include "xfdesktop-special-file-icon.h"
#include "xfdesktop-trash-proxy.h"
#include "xfdesktop-volume-icon.h"
#include "xfdesktop-thumbnailer.h"

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#define SAVE_DELAY 7000
#define BORDER     8

#define SETTING_SHOW_FILESYSTEM  "/desktop-icons/file-icons/show-filesystem"
#define SETTING_SHOW_HOME        "/desktop-icons/file-icons/show-home"
#define SETTING_SHOW_TRASH       "/desktop-icons/file-icons/show-trash"
#define SETTING_SHOW_REMOVABLE   "/desktop-icons/file-icons/show-removable"
#define SETTING_SHOW_THUMBNAILS  "/desktop-icons/show-thumbnails"

enum
{
    PROP0 = 0,
    PROP_FOLDER,
    PROP_SHOW_FILESYSTEM,
    PROP_SHOW_HOME,
    PROP_SHOW_TRASH,
    PROP_SHOW_REMOVABLE,
    PROP_SHOW_THUMBNAILS
};

struct _XfdesktopFileIconManagerPrivate
{
    gboolean inited;

    XfconfChannel *channel;
    
    GtkWidget *desktop;
    XfdesktopIconView *icon_view;
    
    GdkScreen *gscreen;
    
    GFile *folder;
    XfdesktopFileIcon *desktop_icon;
    GFileMonitor *monitor;
    GFileEnumerator *enumerator;

    GVolumeMonitor *volume_monitor;
    
    GHashTable *icons;
    GHashTable *removable_icons;
    GHashTable *special_icons;
    
    gboolean show_removable_media;
    gboolean show_special[XFDESKTOP_SPECIAL_FILE_ICON_TRASH+1];
    gboolean show_thumbnails;
    
    guint save_icons_id;
    
    GList *deferred_icons;
    
    GtkTargetList *drag_targets;
    GtkTargetList *drop_targets;
    
#ifdef HAVE_THUNARX
    GList *thunarx_menu_providers;
    GList *thunarx_properties_providers;
#endif

    XfdesktopThumbnailer *thumbnailer;
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
                                                      guint time_);
static void xfdesktop_file_icon_manager_drag_data_received(XfdesktopIconViewManager *manager,
                                                           XfdesktopIcon *drop_icon,
                                                           GdkDragContext *context,
                                                           guint16 row,
                                                           guint16 col,
                                                           GtkSelectionData *data,
                                                           guint info,
                                                           guint time_);
static void xfdesktop_file_icon_manager_drag_data_get(XfdesktopIconViewManager *manager,
                                                      GList *drag_icons,
                                                      GdkDragContext *context,
                                                      GtkSelectionData *data,
                                                      guint info,
                                                      guint time_);

static gboolean xfdesktop_file_icon_manager_check_create_desktop_folder(GFile *file);
static void xfdesktop_file_icon_manager_load_desktop_folder(XfdesktopFileIconManager *fmanager);
static void xfdesktop_file_icon_manager_load_removable_media(XfdesktopFileIconManager *fmanager);
static void xfdesktop_file_icon_manager_remove_removable_media(XfdesktopFileIconManager *fmanager);

static void xfdesktop_file_icon_position_changed(XfdesktopFileIcon *icon,
                                                 gpointer user_data);

static void xfdesktop_file_icon_manager_update_image(GtkWidget *widget,
                                                     gchar *srcfile,
                                                     gchar *thumbfile,
                                                     XfdesktopFileIconManager *fmanager);

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

static GQuark xfdesktop_app_info_quark = 0;


static void
xfdesktop_file_icon_manager_class_init(XfdesktopFileIconManagerClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    
    g_type_class_add_private(klass, sizeof(XfdesktopFileIconManagerPrivate));
    
    gobject_class->set_property = xfdesktop_file_icon_manager_set_property;
    gobject_class->get_property = xfdesktop_file_icon_manager_get_property;
    gobject_class->finalize = xfdesktop_file_icon_manager_finalize;
    
    g_object_class_install_property(gobject_class, PROP_FOLDER,
                                    g_param_spec_object("folder", "Desktop Folder",
                                                       "Folder this icon manager manages",
                                                       G_TYPE_FILE,
                                                       G_PARAM_READWRITE
                                                       | G_PARAM_CONSTRUCT_ONLY
                                                       | G_PARAM_STATIC_NAME
                                                       | G_PARAM_STATIC_NICK
                                                       | G_PARAM_STATIC_BLURB));

#define XFDESKTOP_PARAM_FLAGS  (G_PARAM_READWRITE \
                                | G_PARAM_CONSTRUCT \
                                | G_PARAM_STATIC_NAME \
                                | G_PARAM_STATIC_NICK \
                                | G_PARAM_STATIC_BLURB)
    g_object_class_install_property(gobject_class, PROP_SHOW_FILESYSTEM,
                                    g_param_spec_boolean("show-filesystem",
                                                         "show filesystem",
                                                         "show filesystem",
                                                         TRUE,
                                                         XFDESKTOP_PARAM_FLAGS));
    g_object_class_install_property(gobject_class, PROP_SHOW_HOME,
                                    g_param_spec_boolean("show-home",
                                                         "show home",
                                                         "show home",
                                                         TRUE,
                                                         XFDESKTOP_PARAM_FLAGS));
    g_object_class_install_property(gobject_class, PROP_SHOW_TRASH,
                                    g_param_spec_boolean("show-trash",
                                                         "show trash",
                                                         "show trash",
                                                         TRUE,
                                                         XFDESKTOP_PARAM_FLAGS));
    g_object_class_install_property(gobject_class, PROP_SHOW_REMOVABLE,
                                    g_param_spec_boolean("show-removable",
                                                         "show removable",
                                                         "show removable",
                                                         TRUE,
                                                         XFDESKTOP_PARAM_FLAGS));
    g_object_class_install_property(gobject_class, PROP_SHOW_THUMBNAILS,
                                    g_param_spec_boolean("show-thumbnails",
                                                         "show-thumbnails",
                                                         "show-thumbnails",
                                                         TRUE,
                                                         XFDESKTOP_PARAM_FLAGS));
#undef XFDESKTOP_PARAM_FLAGS

    xfdesktop_app_info_quark = g_quark_from_static_string("xfdesktop-app-info-quark");
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

    fmanager->priv->thumbnailer = xfdesktop_thumbnailer_new();

    g_signal_connect(G_OBJECT(fmanager->priv->thumbnailer), "thumbnail-ready", G_CALLBACK(xfdesktop_file_icon_manager_update_image), fmanager);
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
            fmanager->priv->folder = g_value_dup_object(value);
            xfdesktop_file_icon_manager_check_create_desktop_folder(fmanager->priv->folder);
            break;

        case PROP_SHOW_FILESYSTEM:
            xfdesktop_file_icon_manager_set_show_special_file(fmanager,
                                                              XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM,
                                                              g_value_get_boolean(value));
            break;

        case PROP_SHOW_HOME:
            xfdesktop_file_icon_manager_set_show_special_file(fmanager,
                                                              XFDESKTOP_SPECIAL_FILE_ICON_HOME,
                                                              g_value_get_boolean(value));
            break;

        case PROP_SHOW_TRASH:
            xfdesktop_file_icon_manager_set_show_special_file(fmanager,
                                                              XFDESKTOP_SPECIAL_FILE_ICON_TRASH,
                                                              g_value_get_boolean(value));
            break;

        case PROP_SHOW_REMOVABLE:
            xfdesktop_file_icon_manager_set_show_removable_media(fmanager,
                                                                 g_value_get_boolean(value));
            break;

        case PROP_SHOW_THUMBNAILS:
            xfdesktop_file_icon_manager_set_show_thumbnails(fmanager,
                                                            g_value_get_boolean(value));
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
            g_value_set_object(value, fmanager->priv->folder);
            break;

        case PROP_SHOW_FILESYSTEM:
            g_value_set_boolean(value,
                                fmanager->priv->show_special[XFDESKTOP_SPECIAL_FILE_ICON_FILESYSTEM]);
            break;

        case PROP_SHOW_HOME:
            g_value_set_boolean(value,
                                fmanager->priv->show_special[XFDESKTOP_SPECIAL_FILE_ICON_HOME]);
            break;

        case PROP_SHOW_TRASH:
            g_value_set_boolean(value,
                                fmanager->priv->show_special[XFDESKTOP_SPECIAL_FILE_ICON_TRASH]);
            break;

        case PROP_SHOW_REMOVABLE:
            g_value_set_boolean(value, fmanager->priv->show_removable_media);
            break;

        case PROP_SHOW_THUMBNAILS:
            g_value_set_boolean(value, fmanager->priv->show_thumbnails);
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
    
    g_object_unref(G_OBJECT(fmanager->priv->channel));

    gtk_target_list_unref(fmanager->priv->drag_targets);
    gtk_target_list_unref(fmanager->priv->drop_targets);
    
    g_object_unref(fmanager->priv->folder);
    g_object_unref(fmanager->priv->thumbnailer);
    
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
    
    g_snprintf(relpath, PATH_MAX, "xfce4/desktop/icons.screen%d-%dx%d.rc",
               gdk_screen_get_number(fmanager->priv->gscreen),
               gdk_screen_get_width(fmanager->priv->gscreen),
               gdk_screen_get_height(fmanager->priv->gscreen));
    
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
                    if(!g_file_set_contents(new_file, contents, length,
                                            &error))
                    {
                        g_critical("Unable to write to %s: %s", new_file,
                                   error->message);
                        g_error_free(error);
                    }

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
xfdesktop_file_icon_manager_check_create_desktop_folder(GFile *folder)
{
    GFileInfo *info;
    GError *error = NULL;
    gboolean result = TRUE;
    gchar *primary;
    
    g_return_val_if_fail(G_IS_FILE(folder), FALSE);

    info = g_file_query_info(folder, XFDESKTOP_FILE_INFO_NAMESPACE,
                             G_FILE_QUERY_INFO_NONE, NULL, NULL);

    if(info == NULL) {
        if(!g_file_make_directory_with_parents(folder, NULL, &error)) {
            gchar *uri = g_file_get_uri(folder);
            gchar *display_name = g_filename_display_basename(uri);
            primary = g_markup_printf_escaped(_("Could not create the desktop folder \"%s\""),
                                              display_name);
            g_free(display_name);
            g_free(uri);

            xfce_message_dialog(NULL, _("Desktop Folder Error"),
                                GTK_STOCK_DIALOG_WARNING, primary,
                                error->message, GTK_STOCK_CLOSE,
                                GTK_RESPONSE_ACCEPT, NULL);
            g_free(primary);

            result = FALSE;
        }
    } else {
        if(g_file_info_get_file_type(info) != G_FILE_TYPE_DIRECTORY) {
            gchar *uri = g_file_get_uri(folder);
            gchar *display_name = g_filename_display_basename(uri);
            primary = g_markup_printf_escaped(_("Could not create the desktop folder \"%s\""),
                                              display_name);
            g_free(display_name);
            g_free(uri);

            xfce_message_dialog(NULL, _("Desktop Folder Error"),
                                GTK_STOCK_DIALOG_WARNING, primary,
                                _("A normal file with the same name already exists. "
                                  "Please delete or rename it."), GTK_STOCK_CLOSE,
                                GTK_RESPONSE_ACCEPT, NULL);
            g_free(primary);

            result = FALSE;
        }
    }

    return result;
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

static void
xfdesktop_file_icon_menu_rename(GtkWidget *widget,
                                gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon;
    GList *selected;
    GFile *file;
    GtkWidget *toplevel;
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(g_list_length(selected) == 1);
    icon = XFDESKTOP_FILE_ICON(selected->data);
    g_list_free(selected);
    
    file = xfdesktop_file_icon_peek_file(icon);
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    xfdesktop_file_utils_rename_file(file, fmanager->priv->gscreen, 
                                     GTK_WINDOW(toplevel));
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
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    GList *gfiles = NULL, *lp;

    for(lp = g_list_last(files); lp != NULL; lp = lp->prev)
        gfiles = g_list_prepend(gfiles, xfdesktop_file_icon_peek_file(lp->data));

    xfdesktop_file_utils_unlink_files(gfiles, fmanager->priv->gscreen, 
                                      GTK_WINDOW(toplevel));

    g_list_free(gfiles);
}

static void
xfdesktop_file_icon_manager_trash_files_cb(DBusGProxy *proxy,
                                           GError *error,
                                           gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = user_data;

    g_return_if_fail(fmanager);

    if(error) {
        GtkWidget *parent = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));

        xfce_message_dialog(GTK_WINDOW(parent),
                            _("Trash Error"), GTK_STOCK_DIALOG_ERROR,
                            _("The selected files could not be trashed"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
    }
}

static gboolean
xfdesktop_file_icon_manager_trash_files(XfdesktopFileIconManager *fmanager,
                                        GList *files)
{
    DBusGProxy *trash_proxy = xfdesktop_file_utils_peek_trash_proxy();
    gboolean result = TRUE;
    gchar **uris, *display_name, *startup_id;
    GList *l;
    gint i, nfiles;
    GFile *file;
    
    g_return_val_if_fail(files, TRUE);
    
    if(!trash_proxy)
        return FALSE;
    
    nfiles = g_list_length(files);
    uris = g_new(gchar *, nfiles + 1);
    
    for(l = files, i = 0; l; l = l->next, ++i) {
        file = xfdesktop_file_icon_peek_file(XFDESKTOP_FILE_ICON(l->data));
        uris[i] = g_file_get_uri(file);
    }
    uris[nfiles] = NULL;
    
    display_name = gdk_screen_make_display_name(fmanager->priv->gscreen);
    startup_id = g_strdup_printf("_TIME%d", gtk_get_current_event_time());
    
    if (!xfdesktop_trash_proxy_move_to_trash_async(trash_proxy, (const char **)uris,
                                                   display_name, startup_id,
                                                   xfdesktop_file_icon_manager_trash_files_cb, 
                                                   fmanager))
    {
        GtkWidget *parent = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));

        xfce_message_dialog(GTK_WINDOW(parent),
                            _("Trash Error"), GTK_STOCK_DIALOG_ERROR,
                            _("The selected files could not be trashed"),
                            _("This feature requires a file manager service to "
                              "be present (such as the one supplied by Thunar)."),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);

        result = FALSE;
    }
    
    g_free(startup_id);
    g_strfreev(uris);
    g_free(display_name);
    
    return result;
}

static void
xfdesktop_file_icon_manager_delete_selected(XfdesktopFileIconManager *fmanager,
                                            gboolean force_delete)
{
    GList *selected, *l;
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    if(!selected)
        return;
    
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
    
    if (!force_delete) {
        xfdesktop_file_icon_manager_trash_files(fmanager, selected);
    } else {
        xfdesktop_file_icon_manager_delete_files(fmanager, selected);
    }
      
    g_list_foreach(selected, (GFunc)g_object_unref, NULL);
    g_list_free(selected);
    
    xfdesktop_file_icon_position_changed(NULL, fmanager);
}

static void
xfdesktop_file_icon_menu_app_info_executed(GtkWidget *widget,
                                           gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon;
    GdkAppLaunchContext *context;
    GAppInfo *app_info;
    GFile *file;
    GList files, *selected;
    GtkWidget *toplevel;
    GError *error = NULL;
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(g_list_length(selected) == 1);
    icon = XFDESKTOP_FILE_ICON(selected->data);
    g_list_free(selected);
    
    /* get the app info related to this menu item */
    app_info = g_object_get_qdata(G_OBJECT(widget), xfdesktop_app_info_quark);
    if(!app_info)
        return;

    /* build a fake file list */
    file = xfdesktop_file_icon_peek_file(icon);
    files.prev = files.next = NULL;
    files.data = file;

    /* prepare the launch context and configure its screen */
    context = gdk_app_launch_context_new();
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    gdk_app_launch_context_set_screen(context, gtk_widget_get_screen(toplevel));
    
    /* try to launch the application */
    if(!xfdesktop_file_utils_app_info_launch(app_info, fmanager->priv->folder, &files,
                                             G_APP_LAUNCH_CONTEXT(context), &error))
    {
        gchar *primary = g_markup_printf_escaped(_("Unable to launch \"%s\":"),
                                                 g_app_info_get_name(app_info));
        xfce_message_dialog(GTK_WINDOW(toplevel), _("Launch Error"),
                            GTK_STOCK_DIALOG_ERROR, primary, error->message,
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        g_free(primary);
        g_error_free(error);
    }
}

static void
xfdesktop_file_icon_menu_open_folder(GtkWidget *widget,
                                     gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon;
    GList *selected;
    GFile *file;
    GtkWidget *toplevel;
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(g_list_length(selected) == 1);
    icon = XFDESKTOP_FILE_ICON(selected->data);
    g_list_free(selected);
    
    file = xfdesktop_file_icon_peek_file(icon);
    
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    xfdesktop_file_utils_open_folder(file, fmanager->priv->gscreen,
                                     GTK_WINDOW(toplevel));
}

static void
xfdesktop_file_icon_menu_open_desktop(GtkWidget *widget,
                                      gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon = fmanager->priv->desktop_icon;
    GFile *file;
    GtkWidget *toplevel;
    
    file = xfdesktop_file_icon_peek_file(icon);
    if(!file)
        return;
    
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    xfdesktop_file_utils_open_folder(file, fmanager->priv->gscreen,
                                     GTK_WINDOW(toplevel));
}

static void
xfdesktop_file_icon_menu_other_app(GtkWidget *widget,
                                   gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon;
    GtkWidget *toplevel;
    GList *selected;
    GFile *file;
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(g_list_length(selected) == 1);
    icon = XFDESKTOP_FILE_ICON(selected->data);
    g_list_free(selected);
    
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));

    file = xfdesktop_file_icon_peek_file(icon);

    xfdesktop_file_utils_display_chooser_dialog(file, TRUE, 
                                                fmanager->priv->gscreen, 
                                                GTK_WINDOW(toplevel));
}

static void
xfdesktop_file_icon_menu_cut(GtkWidget *widget,
                             gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GList *files;
    
    files = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    if(!files)
        return;
    
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
    if(!files)
        return;
    
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
xfdesktop_file_icon_menu_paste(GtkWidget *widget,
                               gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    if(widget && fmanager)
        xfdesktop_clipboard_manager_paste_files(clipboard_manager, fmanager->priv->folder, widget, NULL);
}

static void
xfdesktop_file_icon_menu_arrange_icons(GtkWidget *widget,
                                       gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    xfdesktop_icon_view_sort_icons(fmanager->priv->icon_view);
}

static void
xfdesktop_file_icon_menu_properties(GtkWidget *widget,
                                    gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GList *selected;
    XfdesktopFileIcon *icon;
    GtkWidget *toplevel;
    GFile *file;
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    g_return_if_fail(g_list_length(selected) == 1);
    icon = XFDESKTOP_FILE_ICON(selected->data);
    g_list_free(selected);
    
    file = xfdesktop_file_icon_peek_file(icon);
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    xfdesktop_file_utils_show_properties_dialog(file, fmanager->priv->gscreen, 
                                                GTK_WINDOW(toplevel));
}

static void
xfdesktop_file_icon_manager_desktop_properties(GtkWidget *widget,
                                               gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GtkWidget *parent = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    GFile *file = xfdesktop_file_icon_peek_file (fmanager->priv->desktop_icon);
    
    xfdesktop_file_utils_show_properties_dialog(file, fmanager->priv->gscreen,
                                                GTK_WINDOW(parent));
}

static GtkWidget *
xfdesktop_menu_item_from_app_info(XfdesktopFileIconManager *fmanager,
                                  XfdesktopFileIcon *icon,
                                  GAppInfo *app_info,
                                  gboolean with_mnemonic,
                                  gboolean with_title_prefix)
{
    GtkWidget *mi, *img;
    gchar *title;
    GIcon *gicon;

    if(!with_title_prefix)
        title = g_strdup(g_app_info_get_name(app_info));
    else if(with_mnemonic) {
        title = g_strdup_printf(_("_Open With \"%s\""),
                                g_app_info_get_name(app_info));
    } else {
        title = g_strdup_printf(_("Open With \"%s\""),
                                g_app_info_get_name(app_info));
    }

    if(with_mnemonic)
        mi = gtk_image_menu_item_new_with_mnemonic(title);
    else
        mi = gtk_image_menu_item_new_with_label(title);
    g_free(title);
    
    g_object_set_qdata_full(G_OBJECT(mi), xfdesktop_app_info_quark,
                            g_object_ref(app_info), g_object_unref);
    
    gicon = g_app_info_get_icon(app_info);
    img = gtk_image_new_from_gicon(gicon, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi),
                                  img);
    gtk_widget_show(img);
    gtk_widget_show(mi);
    
    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(xfdesktop_file_icon_menu_app_info_executed),
                     fmanager);
    
    return mi;
}

static gboolean
xfdesktop_file_icon_menu_free_icon_list_idled(gpointer user_data)
{
    GList *icon_list = user_data;
    
    g_list_foreach(icon_list, (GFunc)g_object_unref, NULL);
    g_list_free(icon_list);
    
    return FALSE;
}

static void
xfdesktop_file_icon_menu_free_icon_list(GtkMenu *menu,
                                        gpointer user_data)
{
    g_idle_add(xfdesktop_file_icon_menu_free_icon_list_idled, user_data);
}

static void
xfdesktop_file_icon_menu_create_launcher(GtkWidget *widget,
                                         gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GFile *file;
    gchar *cmd = NULL, *uri = NULL, *display_name;
    GError *error = NULL;
    
    display_name = gdk_screen_make_display_name(fmanager->priv->gscreen);
    
    file = g_object_get_data(G_OBJECT(widget), "file");

    if(file) {
        uri = g_file_get_uri(file);
        cmd = g_strdup_printf("exo-desktop-item-edit \"--display=%s\" \"%s\"",
                              display_name, uri);
    } else {
        const gchar *type = g_object_get_data(G_OBJECT(widget), "xfdesktop-launcher-type");
        uri = g_file_get_uri(fmanager->priv->folder);
        if(G_UNLIKELY(!type))
            type = "Application";
        cmd = g_strdup_printf("exo-desktop-item-edit \"--display=%s\" --create-new --type %s \"%s\"",
                              display_name, type, uri);
    }
    
    if(!xfce_spawn_command_line_on_screen(NULL, cmd, FALSE, FALSE, &error)) {
        GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
        xfce_message_dialog(GTK_WINDOW(toplevel), _("Launch Error"),
                            GTK_STOCK_DIALOG_ERROR, 
                            _("Unable to launch \"exo-desktop-item-edit\", which is required to create and edit launchers and links on the desktop."),
                            error->message, GTK_STOCK_CLOSE,
                            GTK_RESPONSE_ACCEPT, NULL);
        g_error_free(error);
    }
    
    g_free(display_name);
    g_free(uri);
    g_free(cmd);
}

static void
xfdesktop_file_icon_menu_create_folder(GtkWidget *widget,
                                       gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GtkWidget *toplevel;
    
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    xfdesktop_file_utils_create_file(fmanager->priv->folder, "inode/directory",
                                     fmanager->priv->gscreen,
                                     GTK_WINDOW(toplevel));
}

static void
xfdesktop_file_icon_template_item_activated(GtkWidget *mi,
                                            gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GtkWidget *toplevel;
    GFile *file = g_object_get_data(G_OBJECT(mi), "file");
    
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
    
    if(file) {
        xfdesktop_file_utils_create_file_from_template(fmanager->priv->folder, file,
                                                       fmanager->priv->gscreen,
                                                        GTK_WINDOW(toplevel));
    } else {
        xfdesktop_file_utils_create_file(fmanager->priv->folder, "text/plain",
                                         fmanager->priv->gscreen,
                                         GTK_WINDOW(toplevel));
    }
}

static gint
compare_template_files(gconstpointer a,
                       gconstpointer b)
{
  GFileInfo *info_a = g_object_get_data(G_OBJECT(a), "info");
  GFileInfo *info_b = g_object_get_data(G_OBJECT(b), "info");
  GFileType type_a = g_file_info_get_file_type(info_a);
  GFileType type_b = g_file_info_get_file_type(info_b);
  const gchar* name_a = g_file_info_get_display_name(info_a);
  const gchar* name_b = g_file_info_get_display_name(info_b);

  if(!info_a || !info_b)
    return 0;

  if(type_a == type_b) {
      return g_strcmp0(name_a, name_b);
  } else {
      if(type_a == G_FILE_TYPE_DIRECTORY)
          return -1;
      else
          return 1;
  }
}

static gboolean
xfdesktop_file_icon_menu_fill_template_menu(GtkWidget *menu,
                                            GFile *template_dir,
                                            XfdesktopFileIconManager *fmanager)
{
  GFileEnumerator *enumerator;
  GtkWidget *item, *image, *submenu;
  GFileInfo *info;
  GFile *file;
  GIcon *icon;
  GList *files = NULL, *lp;
  gchar *label, *dot;
  gboolean have_templates = FALSE;
  
  g_return_val_if_fail(G_IS_FILE(template_dir), FALSE);

  enumerator = g_file_enumerate_children(template_dir,
                                         XFDESKTOP_FILE_INFO_NAMESPACE,
                                         G_FILE_QUERY_INFO_NONE,
                                         NULL, NULL);

  if(enumerator) {
      while((info = g_file_enumerator_next_file(enumerator, NULL, NULL))) {
          file = g_file_get_child(template_dir, g_file_info_get_name(info));
          g_object_set_data_full(G_OBJECT(file), "info", info, g_object_unref);
          files = g_list_prepend(files, file);
      }

      g_object_unref(enumerator);
  }

  files = g_list_sort(files, compare_template_files);

  for(lp = files; lp != NULL; lp = lp->next) {
      file = lp->data;
      info = g_object_get_data(G_OBJECT(file), "info");

      if(g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
          /* allocate a new submenu for the directory */
          submenu = gtk_menu_new();
          g_object_ref_sink(submenu);
          gtk_menu_set_screen(GTK_MENU(submenu), gtk_widget_get_screen(menu));

          /* fill the submenu from the folder contents */
          have_templates = xfdesktop_file_icon_menu_fill_template_menu(submenu, file, fmanager)
                           || have_templates;

          /* check if any items were added to the submenu */
          if (GTK_MENU_SHELL(submenu)->children)
            {
              /* create a new menu item for the submenu */
              item = gtk_image_menu_item_new_with_label (g_file_info_get_display_name(info));
              icon = g_file_info_get_icon(info);
              image = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_MENU);
              gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
              gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
              gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
              gtk_widget_show (item);
            }

          /* cleanup */
          g_object_unref (submenu);
      } else {
          /* generate a label by stripping off the extension */
          label = g_strdup(g_file_info_get_display_name(info));
          dot = g_utf8_strrchr(label, -1, '.');
          if(dot)
              *dot = '\0';

          /* allocate a new menu item */
          item = gtk_image_menu_item_new_with_label(label);
          icon = g_file_info_get_icon(info);
          image = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_MENU);
          gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
          gtk_widget_show(item);

          g_object_set_data_full(G_OBJECT(item), "file", 
                                 g_object_ref(file), g_object_unref);

          g_signal_connect (G_OBJECT(item), "activate",
                            G_CALLBACK(xfdesktop_file_icon_template_item_activated),
                            fmanager);

          have_templates = TRUE;
      }

      g_object_unref(file);
  }

  g_list_free(files);

  return have_templates;
#if 0
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
          g_object_ref_sink (G_OBJECT (submenu));
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
#endif
  
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
xfdesktop_settings_launch(GtkWidget *w,
                          gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    gchar *cmd;
    GError *error = NULL;
    
    cmd = g_find_program_in_path("xfdesktop-settings");
    if(!cmd)
        cmd = g_strdup(BINDIR "/xfdesktop-settings");
    
    if(!xfce_spawn_command_line_on_screen(fmanager->priv->gscreen, cmd, FALSE, TRUE, &error)) {
        GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
        /* printf is to be translator-friendly */
        gchar *primary = g_strdup_printf(_("Unable to launch \"%s\":"), cmd);
        xfce_message_dialog(GTK_WINDOW(toplevel), _("Launch Error"),
                            GTK_STOCK_DIALOG_ERROR, primary, error->message,
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        g_free(primary);
        g_error_free(error);
    }

    g_free(cmd);
}

static void
xfdesktop_file_icon_manager_populate_context_menu(XfceDesktop *desktop,
                                                  GtkMenuShell *menu,
                                                  gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *file_icon = NULL;
    GFileInfo *info = NULL;
    GList *selected, *app_infos, *l;
    GtkWidget *mi, *img, *tmpl_menu;
    gboolean multi_sel, got_custom_menu = FALSE;
    GFile *templates_dir = NULL, *home_dir;
    const gchar *templates_dir_path = NULL;
#ifdef HAVE_THUNARX
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
#endif
    
    TRACE("ENTERING");
    
    selected = xfdesktop_icon_view_get_selected_items(fmanager->priv->icon_view);
    if(selected)
        file_icon = selected->data;
    else {
        /* assume click on the desktop itself */
        selected = g_list_append(selected, fmanager->priv->desktop_icon);
        file_icon = fmanager->priv->desktop_icon;
    }
    info = xfdesktop_file_icon_peek_file_info(file_icon);
    
    multi_sel = (g_list_length(selected) > 1);
    
    if(!multi_sel) {
        got_custom_menu = xfdesktop_icon_populate_context_menu(XFDESKTOP_ICON(selected->data),
                                                               GTK_WIDGET(menu));
    }
    
    /* make sure icons don't get destroyed while menu is open */
    g_list_foreach(selected, (GFunc)g_object_ref, NULL);
    g_object_set_data(G_OBJECT(menu), "--xfdesktop-icon-list", selected);
    g_signal_connect(G_OBJECT(menu), "deactivate",
                     G_CALLBACK(xfdesktop_file_icon_menu_free_icon_list),
                     selected);
    
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
            if(g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
                img = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
                gtk_widget_show(img);
                if(file_icon == fmanager->priv->desktop_icon)
                    mi = gtk_image_menu_item_new_with_mnemonic(_("_Open in New Window"));
                else
                    mi = gtk_image_menu_item_new_with_mnemonic(_("_Open"));
                gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
                gtk_widget_show(mi);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                g_signal_connect(G_OBJECT(mi), "activate",
                                 file_icon == fmanager->priv->desktop_icon
                                 ? G_CALLBACK(xfdesktop_file_icon_menu_open_desktop)
                                 : G_CALLBACK(xfdesktop_file_icon_menu_open_folder),
                                 fmanager);
                
                mi = gtk_separator_menu_item_new();
                gtk_widget_show(mi);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                
                if(file_icon == fmanager->priv->desktop_icon) {
                    GIcon *icon;

                    /* create launcher item */

                    mi = gtk_image_menu_item_new_with_mnemonic(_("Create _Launcher..."));
                    g_object_set_data(G_OBJECT(mi), "xfdesktop-launcher-type", "Application");
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                    gtk_widget_show(mi);

                    g_signal_connect(G_OBJECT(mi), "activate",
                                     G_CALLBACK(xfdesktop_file_icon_menu_create_launcher),
                                     fmanager);

                    icon = g_content_type_get_icon("application/x-desktop");
                    img = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_MENU);
                    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
                    gtk_widget_show(img);

                    /* create link item */
                    
                    mi = gtk_image_menu_item_new_with_mnemonic(_("Create _URL Link..."));
                    g_object_set_data(G_OBJECT(mi), "xfdesktop-launcher-type", "Link");
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                    gtk_widget_show(mi);

                    g_signal_connect(G_OBJECT(mi), "activate",
                                     G_CALLBACK(xfdesktop_file_icon_menu_create_launcher),
                                     fmanager);
                    
                    icon = g_themed_icon_new("insert-link");
                    img = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_MENU);
                    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
                    gtk_widget_show(img);

                    /* create folder item */

                    mi = gtk_image_menu_item_new_with_mnemonic(_("Create _Folder..."));
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                    gtk_widget_show(mi);

                    g_signal_connect(G_OBJECT(mi), "activate",
                                     G_CALLBACK(xfdesktop_file_icon_menu_create_folder),
                                     fmanager);

                    icon = g_content_type_get_icon("inode/directory");
                    img = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_MENU);
                    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
                    gtk_widget_show(img);
                    
                    /* create from template submenu */

                    mi = gtk_menu_item_new_with_mnemonic(_("Create From _Template"));
                    gtk_widget_show(mi);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                    
                    tmpl_menu = gtk_menu_new();
                    gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), tmpl_menu);
                    
                    home_dir = g_file_new_for_path(xfce_get_homedir());
                    templates_dir_path = g_get_user_special_dir(G_USER_DIRECTORY_TEMPLATES);
                    if(templates_dir_path) {
                        templates_dir = g_file_new_for_path(templates_dir_path);
                    }

                    if(templates_dir && !g_file_equal(home_dir, templates_dir))
                    {
                        xfdesktop_file_icon_menu_fill_template_menu(tmpl_menu,
                                                                    templates_dir,
                                                                    fmanager);
                    }

                    if(templates_dir)
                        g_object_unref(templates_dir);
                    g_object_unref(home_dir);

                    img = gtk_image_new_from_stock(GTK_STOCK_NEW, GTK_ICON_SIZE_MENU);
                    gtk_widget_show(img);
                    mi = gtk_image_menu_item_new_with_mnemonic(_("_Empty File"));
                    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
                    gtk_widget_show(mi);
                    gtk_menu_shell_append(GTK_MENU_SHELL(tmpl_menu), mi);
                    g_signal_connect(G_OBJECT(mi), "activate",
                                     G_CALLBACK(xfdesktop_file_icon_template_item_activated),
                                     fmanager);
                    
                    mi = gtk_separator_menu_item_new();
                    gtk_widget_show(mi);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                }
            } else {
                if(xfdesktop_file_utils_file_is_executable(info)) {
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
                    
                    if(g_content_type_equals(g_file_info_get_content_type(info),
                                             "application/x-desktop"))
                    {
                        GFile *file = xfdesktop_file_icon_peek_file(file_icon);

                        img = gtk_image_new_from_stock(GTK_STOCK_EDIT, GTK_ICON_SIZE_MENU);
                        gtk_widget_show(img);
                        mi = gtk_image_menu_item_new_with_mnemonic(_("_Edit Launcher"));
                        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
                        g_object_set_data_full(G_OBJECT(mi), "file", 
                                               g_object_ref(file), g_object_unref);
                        gtk_widget_show(mi);
                        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                        g_signal_connect(G_OBJECT(mi), "activate",
                                         G_CALLBACK(xfdesktop_file_icon_menu_create_launcher),
                                         fmanager);
                    }
                }
                
                app_infos = g_app_info_get_all_for_type(g_file_info_get_content_type(info));
                if(app_infos) {
                    GAppInfo *app_info = G_APP_INFO(app_infos->data);
                    
                    mi = xfdesktop_menu_item_from_app_info(fmanager, file_icon,
                                                           app_info, TRUE, TRUE);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                    
                    g_object_unref(app_info);

                    if(app_infos->next) {
                        GtkWidget *app_infos_menu;
                        gint list_len = g_list_length(app_infos->next);
                        
                        if(!xfdesktop_file_utils_file_is_executable(info)
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
                            
                            app_infos_menu = gtk_menu_new();
                            gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi),
                                                      app_infos_menu);
                        } else
                            app_infos_menu = (GtkWidget *)menu;
                        
                        for(l = app_infos->next; l; l = l->next) {
                            app_info = G_APP_INFO(l->data);
                            mi = xfdesktop_menu_item_from_app_info(fmanager,
                                                                   file_icon, app_info,
                                                                   FALSE, TRUE);
                            gtk_menu_shell_append(GTK_MENU_SHELL(app_infos_menu), mi);
                            g_object_unref(app_info);
                        }
                    }
                    
                    /* free the app info list */
                    g_list_free(app_infos);
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
        
#ifdef HAVE_THUNARX
        if(!multi_sel && fmanager->priv->thunarx_menu_providers) {
            GList *menu_actions = NULL;
            ThunarxMenuProvider *provider;

            if(g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
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
        
        if(file_icon == fmanager->priv->desktop_icon) {
            mi = gtk_image_menu_item_new_from_stock(GTK_STOCK_PASTE, NULL);
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            if(xfdesktop_clipboard_manager_get_can_paste(clipboard_manager)) {
                g_signal_connect(G_OBJECT(mi), "activate",
                                 G_CALLBACK(xfdesktop_file_icon_menu_paste),
                                 fmanager);
            } else
            gtk_widget_set_sensitive(mi, FALSE);
        } else {
            mi = gtk_image_menu_item_new_from_stock(GTK_STOCK_COPY, NULL);
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            g_signal_connect(G_OBJECT(mi), "activate",
                             G_CALLBACK(xfdesktop_file_icon_menu_copy),
                             fmanager);
            
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
        }
        
        mi = gtk_separator_menu_item_new();
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        
        if(file_icon == fmanager->priv->desktop_icon) {
            img = gtk_image_new_from_stock(GTK_STOCK_SORT_ASCENDING, GTK_ICON_SIZE_MENU);
            gtk_widget_show(img);
            mi = gtk_image_menu_item_new_with_mnemonic(_("Arrange Desktop _Icons"));
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            g_signal_connect(G_OBJECT(mi), "activate",
                             G_CALLBACK(xfdesktop_file_icon_menu_arrange_icons),
                             fmanager);

            img = gtk_image_new_from_stock(GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
            gtk_widget_show(img);
            mi = gtk_image_menu_item_new_with_mnemonic(_("Desktop _Settings..."));
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            g_signal_connect(G_OBJECT(mi), "activate",
                             G_CALLBACK(xfdesktop_settings_launch), fmanager);
        }
        
        img = gtk_image_new_from_stock(GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_MENU);
        gtk_widget_show(img);
        mi = gtk_image_menu_item_new_with_mnemonic(_("P_roperties..."));
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        if(multi_sel || !info)
            gtk_widget_set_sensitive(mi, FALSE);
        else {
            g_signal_connect(G_OBJECT(mi), "activate",
                             file_icon == fmanager->priv->desktop_icon
                             ? G_CALLBACK(xfdesktop_file_icon_manager_desktop_properties)
                             : G_CALLBACK(xfdesktop_file_icon_menu_properties),
                             fmanager);
        }
    }
    
    /* don't free |selected|.  the menu deactivated handler does that */
}

static void
file_icon_hash_write_icons(gpointer key,
                           gpointer value,
                           gpointer data)
{
    XfceRc *rcfile = data;
    XfdesktopIcon *icon = value;
    guint16 row, col;
    
    if(xfdesktop_icon_get_position(icon, &row, &col)) {
        xfce_rc_set_group(rcfile, xfdesktop_icon_peek_label(icon));
        xfce_rc_write_int_entry(rcfile, "row", row);
        xfce_rc_write_int_entry(rcfile, "col", col);
    }
}

static gboolean
xfdesktop_file_icon_manager_save_icons(gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    gchar relpath[PATH_MAX], *tmppath, *path;
    XfceRc *rcfile;
    gint x = 0, y = 0, width = 0, height = 0;
    
    fmanager->priv->save_icons_id = 0;

    xfdesktop_get_workarea_single(fmanager->priv->icon_view,
                                  0,
                                  &x,
                                  &y,
                                  &width,
                                  &height);

    g_snprintf(relpath, PATH_MAX, "xfce4/desktop/icons.screen%d-%dx%d.rc",
               gdk_screen_get_number(fmanager->priv->gscreen),
               width,
               height);

    path = xfce_resource_save_location(XFCE_RESOURCE_CONFIG, relpath, TRUE);
    if(!path)
        return FALSE;
    
    tmppath = g_strconcat(path, ".new", NULL);
    
    rcfile = xfce_rc_simple_open(tmppath, FALSE);
    if(!rcfile) {
        g_warning("Unable to determine location of icon position cache file.  " \
                  "Icon positions will not be saved.");
        g_free(path);
        g_free(tmppath);
        return FALSE;
    }
    
    g_hash_table_foreach(fmanager->priv->icons,
                         file_icon_hash_write_icons, rcfile);
    if(fmanager->priv->show_removable_media) {
        g_hash_table_foreach(fmanager->priv->removable_icons,
                             file_icon_hash_write_icons, rcfile);
    }
    g_hash_table_foreach(fmanager->priv->special_icons,
                         file_icon_hash_write_icons, rcfile);
    
    xfce_rc_flush(rcfile);
    xfce_rc_close(rcfile);
    
    if(rename(tmppath, path)) {
        g_warning("Unable to rename temp file to %s: %s", path,
                  strerror(errno));
        unlink(tmppath);
    }
    
    g_free(path);
    g_free(tmppath);
    
    return FALSE;
}

static void
xfdesktop_file_icon_position_changed(XfdesktopFileIcon *icon,
                                     gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    
    if(fmanager->priv->save_icons_id)
        g_source_remove(fmanager->priv->save_icons_id);
    
    fmanager->priv->save_icons_id = g_timeout_add(SAVE_DELAY,
                                                  xfdesktop_file_icon_manager_save_icons,
                                                  fmanager);
}


/*   *****   */

gboolean
xfdesktop_file_icon_manager_get_cached_icon_position(XfdesktopFileIconManager *fmanager,
                                                     const gchar *name,
                                                     gint16 *row,
                                                     gint16 *col)
{
    gchar relpath[PATH_MAX];
    gchar *filename = NULL;
    gboolean ret = FALSE;
    gint x = 0, y = 0, width = 0, height = 0;

    xfdesktop_get_workarea_single(fmanager->priv->icon_view,
                                  0,
                                  &x,
                                  &y,
                                  &width,
                                  &height);
    
    g_snprintf(relpath, PATH_MAX, "xfce4/desktop/icons.screen%d-%dx%d.rc",
               gdk_screen_get_number(fmanager->priv->gscreen),
               width,
               height);

    filename = xfce_resource_lookup(XFCE_RESOURCE_CONFIG, relpath);

    /* Check if we have to migrate from the old file format */
    if(filename == NULL) {
        g_snprintf(relpath, PATH_MAX, "xfce4/desktop/icons.screen%d.rc",
        gdk_screen_get_number(fmanager->priv->gscreen));
        filename = xfce_resource_lookup(XFCE_RESOURCE_CONFIG, relpath);
    }

    if(filename != NULL) {
        XfceRc *rcfile;
        rcfile = xfce_rc_simple_open(filename, TRUE);

        if(xfce_rc_has_group(rcfile, name)) {
            xfce_rc_set_group(rcfile, name);
            *row = xfce_rc_read_int_entry(rcfile, "row", -1);
            *col = xfce_rc_read_int_entry(rcfile, "col", -1);
            if(*row >= 0 && *col >= 0)
                ret = TRUE;
        }
        xfce_rc_close(rcfile);
        g_free(filename);
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
    GFile *file;

    file = xfdesktop_file_icon_peek_file(icon);

    if(fmanager->priv->show_thumbnails && g_file_get_path(file) != NULL) {
        xfdesktop_thumbnailer_queue_thumbnail(fmanager->priv->thumbnailer,
                                              g_file_get_path(file));
    }

    
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
                                                            g_object_ref(file));
        } else
            do_add = TRUE;
    }
    
    if(do_add) {
        g_signal_connect(G_OBJECT(icon), "position-changed",
                         G_CALLBACK(xfdesktop_file_icon_position_changed),
                         fmanager);
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
                                             GFile *file,
                                             GFileInfo *info,
                                             gboolean defer_if_missing)
{
    XfdesktopRegularFileIcon *icon = NULL;
    gboolean is_desktop_file = FALSE;
    
    g_return_val_if_fail(fmanager && G_IS_FILE(file) && G_IS_FILE_INFO(info), NULL);

    if(g_content_type_equals(g_file_info_get_content_type(info), 
                             "application/x-desktop")) 
    {
        is_desktop_file = TRUE;
    }
    else
    {
      gchar *uri = g_file_get_uri(file);
      if(g_str_has_suffix(uri, ".desktop"))
          is_desktop_file = TRUE;
      g_free(uri);
    }

    /* if it's a .desktop file, and it has Hidden=true, or an
     * OnlyShowIn Or NotShowIn that would hide it from Xfce, don't
     * show it on the desktop (bug #4022) */
    if(is_desktop_file)
    {
        gchar *path = g_file_get_path(file);
        XfceRc *rcfile = xfce_rc_simple_open(path, TRUE);
        g_free(path);

        if(rcfile) {
            const gchar *value;

            xfce_rc_set_group(rcfile, "Desktop Entry");
            if(xfce_rc_read_bool_entry(rcfile, "Hidden", FALSE)) {
                xfce_rc_close(rcfile);
                return NULL;
            }

            value = xfce_rc_read_entry(rcfile, "OnlyShowIn", NULL);
            if(value && strncmp(value, "XFCE;", 5) && !strstr(value, ";XFCE;")) {
                xfce_rc_close(rcfile);
                return NULL;
            }

            value = xfce_rc_read_entry(rcfile, "NotShowIn", NULL);
            if(value && (!strncmp(value, "XFCE;", 5) || strstr(value, ";XFCE;"))) {
                xfce_rc_close(rcfile);
                return NULL;
            }

            xfce_rc_close(rcfile);
        }
    }
    
    /* should never return NULL */
    icon = xfdesktop_regular_file_icon_new(file, info, fmanager->priv->gscreen);
    
    if(xfdesktop_file_icon_manager_add_icon(fmanager,
                                             XFDESKTOP_FILE_ICON(icon),
                                             defer_if_missing))
    {
        g_hash_table_replace(fmanager->priv->icons, g_object_ref(file), icon);
        return XFDESKTOP_FILE_ICON(icon);
    } else {
        g_object_unref(G_OBJECT(icon));
        return NULL;
    }
}

static XfdesktopFileIcon *
xfdesktop_file_icon_manager_add_volume_icon(XfdesktopFileIconManager *fmanager,
                                            GVolume *volume)
{
    XfdesktopVolumeIcon *icon;
    
    g_return_val_if_fail(fmanager && G_IS_VOLUME(volume), NULL);
    
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
        
        case GDK_v:
        case GDK_V:
            if(!(evt->state & GDK_CONTROL_MASK)
               || (evt->state & (GDK_SHIFT_MASK|GDK_MOD1_MASK|GDK_MOD4_MASK)))
            {
                return FALSE;
            }
            if(xfdesktop_clipboard_manager_get_can_paste(clipboard_manager)) {
                xfdesktop_clipboard_manager_paste_files(clipboard_manager, fmanager->priv->folder, widget, NULL);
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
xfdesktop_file_icon_manager_file_changed(GFileMonitor     *monitor,
                                         GFile            *file,
                                         GFile            *other_file,
                                         GFileMonitorEvent event,
                                         gpointer          user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopFileIcon *icon;
    GFileInfo *file_info;

    switch(event) {
        case G_FILE_MONITOR_EVENT_CHANGED:
            DBG("got changed event: %s", g_file_get_path(file));
            
            icon = g_hash_table_lookup(fmanager->priv->icons, file);
            if(icon) {
                file_info = g_file_query_info(file, XFDESKTOP_FILE_INFO_NAMESPACE,
                                              G_FILE_QUERY_INFO_NONE, NULL, NULL);
                
                if(file_info) {
                    /* update the icon if the file still exists */
                    xfdesktop_file_icon_update_file_info(icon, file_info);
                    g_object_unref(file_info);
                } else {
                    /* remove the icon as the file no longer seems to be existing */
                    xfdesktop_icon_view_remove_item(fmanager->priv->icon_view,
                                                    XFDESKTOP_ICON(icon));
                    g_hash_table_remove(fmanager->priv->icons, file);
                }
            }
            break;
        case G_FILE_MONITOR_EVENT_CREATED:
            DBG("got created event");

            /* make sure it's not the desktop folder itself */
            if(g_file_equal(fmanager->priv->folder, file))
                return;

            /* first make sure we don't already have an icon for this path.
             * this seems to be necessary to avoid inconsistencies */
            icon = g_hash_table_lookup(fmanager->priv->icons, file);
            if(icon) {
                xfdesktop_icon_view_remove_item(fmanager->priv->icon_view,
                                                XFDESKTOP_ICON(icon));
                g_hash_table_remove(fmanager->priv->icons, file);
            }
            
            file_info = g_file_query_info(file, XFDESKTOP_FILE_INFO_NAMESPACE,
                                          G_FILE_QUERY_INFO_NONE, NULL, NULL);
            if(file_info) {
                gboolean is_hidden = g_file_info_get_attribute_boolean(file_info,
                                                                       G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN);
                if(!is_hidden) {
                    xfdesktop_file_icon_manager_add_regular_icon(fmanager,
                                                                 file, file_info, 
                                                                 FALSE);
                }

                g_object_unref(file_info);
            }

            break;
        case G_FILE_MONITOR_EVENT_DELETED:
            DBG("got deleted event");

            icon = g_hash_table_lookup(fmanager->priv->icons, file);
            if(icon) {
                /* Always try to remove thumbnail so it doesn't take up
                 * space on the user's disk.
                 */
                xfdesktop_thumbnailer_delete_thumbnail(fmanager->priv->thumbnailer,
                                                       g_file_get_path(file));
                xfdesktop_icon_delete_thumbnail(XFDESKTOP_ICON(icon));

                xfdesktop_icon_view_remove_item(fmanager->priv->icon_view,
                                                XFDESKTOP_ICON(icon));
                g_hash_table_remove(fmanager->priv->icons, file);
            } else {
                if(g_file_equal(file, fmanager->priv->folder)) {
                    DBG("~/Desktop disappeared!");
                    /* yes, refresh before and after is correct */
                    xfdesktop_file_icon_manager_refresh_icons(fmanager);
                    xfdesktop_file_icon_manager_check_create_desktop_folder(fmanager->priv->folder);
                    xfdesktop_file_icon_manager_refresh_icons(fmanager);
                }
            }
            break;
        default:
            break;
    }
}

static void
xfdesktop_file_icon_manager_files_ready(GFileEnumerator *enumerator,
                                        GAsyncResult *result,
                                        gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GError *error = NULL;
    GList *files, *l;
    gboolean is_hidden;

    g_return_if_fail(enumerator == fmanager->priv->enumerator);

    files = g_file_enumerator_next_files_finish(enumerator, result, &error);

    if(!files) {
        if(error) {
            GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));
#if GTK_CHECK_VERSION (2, 18, 0)
            xfce_message_dialog(gtk_widget_is_toplevel(toplevel) ? GTK_WINDOW(toplevel) : NULL,
#else
            xfce_message_dialog(GTK_WIDGET_TOPLEVEL(toplevel) ? GTK_WINDOW(toplevel) : NULL,
#endif
                                _("Load Error"),
                                GTK_STOCK_DIALOG_WARNING, 
                                _("Failed to load the desktop folder"), error->message,
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        }

        g_object_unref(fmanager->priv->enumerator);
        fmanager->priv->enumerator = NULL;

        if(fmanager->priv->deferred_icons) {
            for(l = fmanager->priv->deferred_icons; l; l = l->next) {
                GFile *file = G_FILE(l->data);
                GFileInfo *info = g_file_query_info(file, 
                                                    XFDESKTOP_FILE_INFO_NAMESPACE,
                                                    G_FILE_QUERY_INFO_NONE, 
                                                    NULL, NULL);

                xfdesktop_file_icon_manager_add_regular_icon(fmanager, 
                                                             file, info, 
                                                             FALSE);
                g_object_unref(info);
                g_object_unref(file);
            }
            g_list_free(fmanager->priv->deferred_icons);
            fmanager->priv->deferred_icons = NULL;
        }


        if(!fmanager->priv->monitor) {
            fmanager->priv->monitor = g_file_monitor(fmanager->priv->folder,
                                                     G_FILE_MONITOR_NONE,
                                                     NULL, NULL);
            g_signal_connect(fmanager->priv->monitor, "changed",
                             G_CALLBACK(xfdesktop_file_icon_manager_file_changed),
                             fmanager);
        }
    } else {
        for(l = files; l; l = l->next) {
            DBG("got a GFileInfo: %s", g_file_info_get_display_name(l->data));
            
            is_hidden = g_file_info_get_attribute_boolean(l->data,
                                                          G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN);
            if(!is_hidden) {
                const gchar *name = g_file_info_get_name(l->data);
                GFile *file = g_file_get_child(fmanager->priv->folder, name);

                xfdesktop_file_icon_manager_add_regular_icon(fmanager, 
                                                             file, l->data,
                                                             TRUE);

                g_object_unref(file);
            }

            g_object_unref(l->data);
        }

        g_list_free(files);

        g_file_enumerator_next_files_async(fmanager->priv->enumerator,
                                           10, G_PRIORITY_DEFAULT, NULL,
                                           (GAsyncReadyCallback) xfdesktop_file_icon_manager_files_ready,
                                           fmanager);
    }
}

static void
xfdesktop_file_icon_manager_load_desktop_folder(XfdesktopFileIconManager *fmanager)
{
    if(fmanager->priv->deferred_icons) {
        g_list_foreach(fmanager->priv->deferred_icons, (GFunc)g_object_unref, NULL);
        g_list_free(fmanager->priv->deferred_icons);
        fmanager->priv->deferred_icons = NULL;
    }
    
    if(fmanager->priv->enumerator) {
        g_object_unref(fmanager->priv->enumerator);
        fmanager->priv->enumerator = NULL;
    }

    fmanager->priv->enumerator = g_file_enumerate_children(fmanager->priv->folder,
                                                           XFDESKTOP_FILE_INFO_NAMESPACE,
                                                           G_FILE_QUERY_INFO_NONE,
                                                           NULL, NULL);

    if(fmanager->priv->enumerator) {
        g_file_enumerator_next_files_async(fmanager->priv->enumerator,
                                           10, G_PRIORITY_DEFAULT, NULL,
                                           (GAsyncReadyCallback) xfdesktop_file_icon_manager_files_ready,
                                           fmanager);

    }
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
xfdesktop_file_icon_manager_volume_changed(GVolume *volume,
                                           gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopIcon *icon;
    gboolean is_present = xfdesktop_file_utils_volume_is_present(volume);
    
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
                                                 GVolume *volume)
{
    if(!xfdesktop_file_utils_volume_is_removable(volume))
        return;
    
    if(xfdesktop_file_utils_volume_is_present(volume))
        xfdesktop_file_icon_manager_add_volume_icon(fmanager, volume);
    
    g_signal_connect(G_OBJECT(volume), "changed",
                     G_CALLBACK(xfdesktop_file_icon_manager_volume_changed),
                     fmanager);
}

static void
xfdesktop_file_icon_manager_volume_added(GVolumeMonitor *monitor,
                                         GVolume *volume,
                                         gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);

    xfdesktop_file_icon_manager_add_removable_volume(fmanager, volume);
}

static void
xfdesktop_file_icon_manager_volume_removed(GVolumeMonitor *monitor,
                                           GVolume *volume,
                                           gpointer user_data)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    XfdesktopIcon *icon;
    
    icon = g_hash_table_lookup(fmanager->priv->removable_icons, volume);
    if(icon) {
        xfdesktop_icon_view_remove_item(fmanager->priv->icon_view, icon);
        g_hash_table_remove(fmanager->priv->removable_icons, volume);
    }
}

static void
xfdesktop_file_icon_manager_load_removable_media(XfdesktopFileIconManager *fmanager)
{
    GList *volumes, *l;
    
    /* ensure we don't re-enter if we're already set up */
    if(fmanager->priv->removable_icons)
        return;
    
    if(!fmanager->priv->volume_monitor) {
        fmanager->priv->volume_monitor = g_volume_monitor_get();
        g_object_add_weak_pointer(G_OBJECT(fmanager->priv->volume_monitor),
                                  (gpointer)&fmanager->priv->volume_monitor);
    } else
       g_object_ref(G_OBJECT(fmanager->priv->volume_monitor));
    
    fmanager->priv->removable_icons = g_hash_table_new_full(g_direct_hash,
                                                            g_direct_equal,
                                                            (GDestroyNotify)g_object_unref,
                                                            (GDestroyNotify)g_object_unref);
    
    volumes = g_volume_monitor_get_volumes(fmanager->priv->volume_monitor);
    for(l = volumes; l; l = l->next) {
        xfdesktop_file_icon_manager_add_removable_volume(fmanager, l->data);
        g_object_unref(l->data);
    }
    g_list_free(volumes);
    
    g_signal_connect(G_OBJECT(fmanager->priv->volume_monitor), "volume-added",
                     G_CALLBACK(xfdesktop_file_icon_manager_volume_added),
                     fmanager);
    g_signal_connect(G_OBJECT(fmanager->priv->volume_monitor), "volume-removed",
                     G_CALLBACK(xfdesktop_file_icon_manager_volume_removed),
                     fmanager);
}

static void
xfdesktop_file_icon_manager_ht_remove_removable_media(gpointer key,
                                                      gpointer value,
                                                      gpointer user_data)
{
    XfdesktopIcon *icon = XFDESKTOP_ICON(value);
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(user_data);
    GVolume *volume;

    volume = xfdesktop_volume_icon_peek_volume(XFDESKTOP_VOLUME_ICON(icon));
    if(volume) {
        g_signal_handlers_disconnect_by_func(volume,
                                             G_CALLBACK(xfdesktop_file_icon_manager_volume_changed),
                                             fmanager);
    }
    
    xfdesktop_icon_view_remove_item(fmanager->priv->icon_view, icon);
}

static void
xfdesktop_file_icon_manager_remove_removable_media(XfdesktopFileIconManager *fmanager)
{
    if(fmanager->priv->removable_icons) {
        g_hash_table_foreach(fmanager->priv->removable_icons,
                             xfdesktop_file_icon_manager_ht_remove_removable_media,
                             fmanager);
        g_hash_table_destroy(fmanager->priv->removable_icons);
        fmanager->priv->removable_icons = NULL;
    }
    
    if(fmanager->priv->volume_monitor) {
        g_signal_handlers_disconnect_by_func(G_OBJECT(fmanager->priv->volume_monitor),
                                             G_CALLBACK(xfdesktop_file_icon_manager_volume_added),
                                             fmanager);
        g_signal_handlers_disconnect_by_func(G_OBJECT(fmanager->priv->volume_monitor),
                                             G_CALLBACK(xfdesktop_file_icon_manager_volume_removed),
                                             fmanager);
    
        g_object_unref(fmanager->priv->volume_monitor);
    }
}


/* virtual functions */

static gboolean
xfdesktop_file_icon_manager_real_init(XfdesktopIconViewManager *manager,
                                      XfdesktopIconView *icon_view)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    GFileInfo *desktop_info;
    gint i;
#ifdef HAVE_THUNARX
    ThunarxProviderFactory *thunarx_pfac;
#endif

    if(fmanager->priv->inited) {
        g_warning("Initializing icon manager when already inited");
        return FALSE;
    }

    fmanager->priv->icon_view = icon_view;
    
    fmanager->priv->desktop = gtk_widget_get_toplevel(GTK_WIDGET(icon_view));
    g_signal_connect(G_OBJECT(fmanager->priv->desktop), "populate-root-menu",
                     G_CALLBACK(xfdesktop_file_icon_manager_populate_context_menu),
                     fmanager);
    
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
    
    fmanager->priv->icons = g_hash_table_new_full((GHashFunc)g_file_hash,
                                                  (GEqualFunc)g_file_equal,
                                                  (GDestroyNotify)g_object_unref,
                                                  (GDestroyNotify)g_object_unref);
    
    fmanager->priv->special_icons = g_hash_table_new_full(g_direct_hash,
                                                          g_direct_equal,
                                                          NULL,
                                                          (GDestroyNotify)g_object_unref);
    
    if(!xfdesktop_file_utils_dbus_init())
        g_warning("Unable to initialise D-Bus.  Some xfdesktop features may be unavailable.");
    
    /* do this in the reverse order stuff should be displayed */
    xfdesktop_file_icon_manager_load_desktop_folder(fmanager);
    if(fmanager->priv->show_removable_media)
        xfdesktop_file_icon_manager_load_removable_media(fmanager);
    for(i = XFDESKTOP_SPECIAL_FILE_ICON_TRASH; i >= 0; --i) {
        if(fmanager->priv->show_special[i])
            xfdesktop_file_icon_manager_add_special_file_icon(fmanager, i);
    }
    
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
    
    desktop_info = g_file_query_info(fmanager->priv->folder,
                                     XFDESKTOP_FILE_INFO_NAMESPACE,
                                     G_FILE_QUERY_INFO_NONE,
                                     NULL, NULL);

    fmanager->priv->desktop_icon = XFDESKTOP_FILE_ICON(xfdesktop_regular_file_icon_new(fmanager->priv->folder,
                                                                                       desktop_info,
                                                                                       fmanager->priv->gscreen));
    
    g_object_unref(desktop_info);

    fmanager->priv->inited = TRUE;
    
    return TRUE;
}

static void
xfdesktop_file_icon_manager_fini(XfdesktopIconViewManager *manager)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    gint i;

    if(!fmanager->priv->inited) {
        g_warning("Trying to de-init icon manager when it was never inited");
        return;
    }

    fmanager->priv->inited = FALSE;
    
    if(fmanager->priv->enumerator) {
        g_object_unref(fmanager->priv->enumerator);
        fmanager->priv->enumerator = NULL;
    }
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(fmanager->priv->desktop),
                                         G_CALLBACK(xfdesktop_file_icon_manager_populate_context_menu),
                                         fmanager);
    
    if(fmanager->priv->save_icons_id) {
        g_source_remove(fmanager->priv->save_icons_id);
        fmanager->priv->save_icons_id = 0;
        xfdesktop_file_icon_manager_save_icons(fmanager);
    }
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(clipboard_manager),
                                         G_CALLBACK(xfdesktop_file_icon_manager_clipboard_changed),
                                         fmanager);
    
    g_object_unref(G_OBJECT(clipboard_manager));
    
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
        g_list_foreach(fmanager->priv->deferred_icons, (GFunc)g_object_unref, NULL);
        g_list_free(fmanager->priv->deferred_icons);
        fmanager->priv->deferred_icons = NULL;
    }
    
    /* disconnect from the file monitor and release it */
    g_signal_handlers_disconnect_by_func(fmanager->priv->monitor,
                                         G_CALLBACK(xfdesktop_file_icon_manager_file_changed),
                                         fmanager);
    g_object_unref(fmanager->priv->monitor);
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
                                      guint time_)
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
        GFile *source_file, *file;
        gchar *uri = NULL;
        
        if(drop_icon) {
            GFileInfo *info = xfdesktop_file_icon_peek_file_info(XFDESKTOP_FILE_ICON(drop_icon));
            if(!info)
                return FALSE;
            
            if(g_file_info_get_file_type(info) != G_FILE_TYPE_DIRECTORY)
                return FALSE;
            
            source_file = xfdesktop_file_icon_peek_file(XFDESKTOP_FILE_ICON(drop_icon));
            
        } else
            source_file = fmanager->priv->folder;
        
        if(gdk_property_get(context->source_window,
                            gdk_atom_intern("XdndDirectSave0", FALSE),
                            gdk_atom_intern("text/plain", FALSE),
                            0, 1024, FALSE, NULL, NULL, &prop_len,
                            &prop_text) && prop_text)
        {
            prop_text = g_realloc(prop_text, prop_len + 1);
            prop_text[prop_len] = 0;

            file = g_file_resolve_relative_path(source_file, (const gchar *)prop_text);
            uri = g_file_get_uri(file);
            g_object_unref(file);
            
            gdk_property_change(context->source_window,
                                gdk_atom_intern("XdndDirectSave0", FALSE),
                                gdk_atom_intern("text/plain", FALSE), 8,
                                GDK_PROP_MODE_REPLACE, (const guchar *)uri,
                                strlen(uri));
            
            g_free(prop_text);
            g_free(uri);
        }
        
        if(!uri)
            return FALSE;
    } else if(target == gdk_atom_intern("_NETSCAPE_URL", FALSE)) {
        if(drop_icon) {
            /* don't allow a drop on an icon that isn't a folder (i.e., not
             * on an icon that's an executable */
            GFileInfo *info = xfdesktop_file_icon_peek_file_info(XFDESKTOP_FILE_ICON(drop_icon));
            if(!info || g_file_info_get_file_type(info) != G_FILE_TYPE_DIRECTORY)
                return FALSE;
        }
    }
    
    TRACE("target good");
    
    gtk_drag_get_data(widget, context, target, time_);
    
    return TRUE;
}

static void xfdesktop_dnd_item(GtkWidget *item, GdkDragAction *action)
{
    *action = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(item), "action"));
}

static void xfdesktop_dnd_item_cancel(GtkWidget *item, GdkDragAction *action)
{
    *action = 0;
}

/**
 * xfdesktop_dnd_menu:
 * @manager     : the #XfdesktopIconViewManager instance
 * @drop_icon   : the #XfdesktopIcon to which is being dropped.
 * @context     : the #GdkDragContext of the icons being dropped.
 * @row         : the row on the desktop to drop to.
 * @col         : the col on the desktop to drop to.
 * @ time_      : the starting time of the drag event.
 * Pops up a menu that asks the user to choose one of the
 * actions or to cancel the drop. Sets context->action to
 * the new action the user selected or 0 on cancel.
 * Portions of this code was copied from thunar-dnd.c
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 **/
static void xfdesktop_dnd_menu (XfdesktopIconViewManager *manager,
                                XfdesktopIcon *drop_icon,
                                GdkDragContext *context,
                                guint16 row,
                                guint16 col,
                                guint time_)
{
    static GdkDragAction    actions[] = { GDK_ACTION_COPY, GDK_ACTION_MOVE, GDK_ACTION_LINK };
    static const gchar      *action_names[] = { N_ ("Copy _Here") , N_ ("_Move Here") , N_ ("_Link Here") };
    static const gchar      *action_icons[] = { "stock_folder-copy", "stock_folder-move", NULL };
    GtkWidget *menu;
    GtkWidget *item;
    GtkWidget  *image;
    guint menu_item, signal_id;
    GMainLoop *loop;
    gint response;
    menu = gtk_menu_new();

    /* This adds the Copy, Move, & Link options */
    for(menu_item = 0; menu_item < G_N_ELEMENTS(actions); menu_item++) {
        item = gtk_image_menu_item_new_with_mnemonic(_(action_names[menu_item]));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(xfdesktop_dnd_item), &response);
        g_object_set_data(G_OBJECT(item), "action", GUINT_TO_POINTER(actions[menu_item]));
        /* add image to the menu item */
        if(G_LIKELY(action_icons[menu_item] != NULL)) {
            image = gtk_image_new_from_icon_name(action_icons[menu_item], GTK_ICON_SIZE_MENU);
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
            gtk_widget_show(image);
        }

        gtk_widget_show(item);
    }

    /* Add a seperator */
    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);

    /* Cancel option */
    item = gtk_image_menu_item_new_from_stock(GTK_STOCK_CANCEL, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(xfdesktop_dnd_item_cancel), &response);
    gtk_widget_show(item);

    gtk_widget_show(menu);
    g_object_ref_sink(G_OBJECT(menu));

    /* Loop until we get a user response */
    loop = g_main_loop_new(NULL, FALSE);
    signal_id = g_signal_connect_swapped(G_OBJECT(menu), "deactivate", G_CALLBACK(g_main_loop_quit), loop);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 3, time_);
    g_main_loop_run(loop);
    g_signal_handler_disconnect(G_OBJECT(menu), signal_id);
    g_main_loop_unref(loop);

    context->action = response;

    g_object_unref(G_OBJECT(menu));
}

static void
xfdesktop_file_icon_manager_drag_data_received(XfdesktopIconViewManager *manager,
                                               XfdesktopIcon *drop_icon,
                                               GdkDragContext *context,
                                               guint16 row,
                                               guint16 col,
                                               GtkSelectionData *data,
                                               guint info,
                                               guint time_)
{
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(manager);
    XfdesktopFileIcon *file_icon = NULL;
    GFileInfo *tinfo = NULL;
    GFile *tfile = NULL;
    gboolean copy_only = TRUE, drop_ok = FALSE;
    GList *file_list;
    gboolean user_selected_action = FALSE;

    TRACE("entering");

    if(context->action == GDK_ACTION_ASK) {
        xfdesktop_dnd_menu(manager, drop_icon, context, row, col, time_);

        if(context->action == 0) {
            gtk_drag_finish(context, FALSE, FALSE, time_);
            return;
        }
        /* The user picked whether to move or copy the files */
        user_selected_action = TRUE;
    }

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
        GFile *source_file = NULL;
        gchar *exo_desktop_item_edit = g_find_program_in_path("exo-desktop-item-edit");

        if(drop_icon) {
            GFileInfo *finfo = xfdesktop_file_icon_peek_file_info(XFDESKTOP_FILE_ICON(drop_icon));
            if(g_file_info_get_file_type(finfo) == G_FILE_TYPE_DIRECTORY)
                source_file = xfdesktop_file_icon_peek_file(XFDESKTOP_FILE_ICON(drop_icon));
        } else
            source_file = fmanager->priv->folder;
        
        if(source_file && exo_desktop_item_edit) {
            gchar **parts = g_strsplit((const gchar *)data->data, "\n", -1);
            
            if(2 == g_strv_length(parts)) {
                gchar *cwd = g_file_get_uri(source_file);
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
                
                if(xfce_spawn_on_screen(fmanager->priv->gscreen, NULL, myargv,
                                        NULL, G_SPAWN_SEARCH_PATH, TRUE,
                                        gtk_get_current_event_time(),
                                        NULL, NULL))
                {
                    drop_ok = TRUE;
                }
                
                g_free(cwd);
            }
            
            g_strfreev(parts);
        }
        
        g_free(exo_desktop_item_edit);
    } else if(info == TARGET_TEXT_URI_LIST) {
        if(drop_icon) {
            file_icon = XFDESKTOP_FILE_ICON(drop_icon);
            tfile = xfdesktop_file_icon_peek_file(file_icon);
            tinfo = xfdesktop_file_icon_peek_file_info(file_icon);
        }
        
        copy_only = (context->action == GDK_ACTION_COPY);
        
        if(tfile && g_file_has_uri_scheme(tfile, "trash") && copy_only) {
            gtk_drag_finish(context, FALSE, FALSE, time_);
            return;
        }
        
        file_list = xfdesktop_file_utils_file_list_from_string((const gchar *)data->data);
        if(file_list) {
            GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(fmanager->priv->icon_view));

            if(tinfo && xfdesktop_file_utils_file_is_executable(tinfo)) {
                drop_ok = xfdesktop_file_utils_execute(fmanager->priv->folder,
                                                       tfile, file_list,
                                                       fmanager->priv->gscreen,
                                                       GTK_WINDOW(toplevel));
            } else if(tfile && g_file_has_uri_scheme(tfile, "trash")) {
                /* move files to the trash */
                xfdesktop_file_utils_trash_files(file_list,
                                                 fmanager->priv->gscreen,
                                                 GTK_WINDOW(toplevel));
            } else {
                GFile *base_dest_file = NULL;
                GList *l, *dest_file_list = NULL;
                gboolean dest_is_volume = (drop_icon
                                           && XFDESKTOP_IS_VOLUME_ICON(drop_icon));
                
                /* if it's a volume, but we don't have |tinfo|, this just isn't
                 * going to work */
                if(!tinfo && dest_is_volume) {
                    xfdesktop_file_utils_file_list_free(file_list);
                    gtk_drag_finish(context, FALSE, FALSE, time_);
                    return;
                }

                if(tinfo && g_file_info_get_file_type(tinfo) == G_FILE_TYPE_DIRECTORY) {
                    base_dest_file = g_object_ref(tfile);
                } else {
                    base_dest_file = g_object_ref(fmanager->priv->folder);
                }

                /* If the user didn't pick whether to copy or move via
                 * a GDK_ACTION_ASK then determine if we should move/copy
                 * by checking if the files are on the same filesystem
                 * and are writable by the user.
                 */
                if(user_selected_action == FALSE) {
                    GFileInfo *src_info, *dest_info;
                    const gchar *src_name, *dest_name;

                    dest_info = g_file_query_info(base_dest_file,
                                                  XFDESKTOP_FILE_INFO_NAMESPACE,
                                                  G_FILE_QUERY_INFO_NONE,
                                                  NULL,
                                                  NULL);
                    src_info = g_file_query_info(file_list->data,
                                                 XFDESKTOP_FILE_INFO_NAMESPACE,
                                                 G_FILE_QUERY_INFO_NONE,
                                                 NULL,
                                                 NULL);

                    if(dest_info != NULL && src_info != NULL) {
                        dest_name = g_file_info_get_attribute_string(dest_info,
                                                G_FILE_ATTRIBUTE_ID_FILESYSTEM);
                        src_name = g_file_info_get_attribute_string(src_info,
                                                G_FILE_ATTRIBUTE_ID_FILESYSTEM);

                        if((g_strcmp0(src_name, dest_name) == 0)
                           && g_file_info_get_attribute_boolean(src_info,
                                            G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
                        {
                            copy_only = FALSE;
                            context->action = GDK_ACTION_MOVE;
                        }
                    }

                    if(dest_info != NULL)
                        g_object_unref(dest_info);
                    if(src_info != NULL)
                        g_object_unref(src_info);
                }

                for (l = file_list; l; l = l->next) {
                    gchar *dest_basename = g_file_get_basename(l->data);

                    if(dest_basename && *dest_basename != '\0') {
                        /* If we copy a file, we need to use the new absolute filename
                         * as the destination. If we move or link, we need to use the destination
                         * directory. */
                        if(copy_only) {
                            GFile *dest_file = g_file_get_child(base_dest_file, dest_basename);
                            dest_file_list = g_list_prepend(dest_file_list, dest_file);
                        } else {
                            dest_file_list = g_list_prepend(dest_file_list, base_dest_file);
                        }
                    }

                    g_free(dest_basename);
                }

                g_object_unref(base_dest_file);

                if(dest_file_list) {
                    dest_file_list = g_list_reverse(dest_file_list);

                    drop_ok = xfdesktop_file_utils_transfer_files(context->action, 
                                                                  file_list, 
                                                                  dest_file_list,
                                                                  fmanager->priv->gscreen);
                }

                if(copy_only) {
                    xfdesktop_file_utils_file_list_free(dest_file_list);
                } else {
                    g_list_free(dest_file_list);
                }
            }
        }
    }
    
    DBG("finishing drop on desktop from external source: drop_ok=%s, copy_only=%s",
        drop_ok?"TRUE":"FALSE", copy_only?"TRUE":"FALSE");
    
    gtk_drag_finish(context, drop_ok, !copy_only, time_);
}

static void
xfdesktop_file_icon_manager_drag_data_get(XfdesktopIconViewManager *manager,
                                          GList *drag_icons,
                                          GdkDragContext *context,
                                          GtkSelectionData *data,
                                          guint info,
                                          guint time_)
{
    GList *file_list;
    gchar *str;
    
    TRACE("entering");
    
    g_return_if_fail(drag_icons);
    g_return_if_fail(info == TARGET_TEXT_URI_LIST);
    
    file_list = xfdesktop_file_utils_file_icon_list_to_file_list(drag_icons);
    str = xfdesktop_file_utils_file_list_to_string(file_list);

    gtk_selection_data_set(data, data->target, 8, (guchar *)str, strlen(str));
    
    g_free(str);
    xfdesktop_file_utils_file_list_free(file_list);
}


/* public api */

XfdesktopIconViewManager *
xfdesktop_file_icon_manager_new(GFile *folder,
                                XfconfChannel *channel)
{
    XfdesktopFileIconManager *fmanager;
    
    g_return_val_if_fail(folder && channel, NULL);

    fmanager = g_object_new(XFDESKTOP_TYPE_FILE_ICON_MANAGER,
                            "folder", folder,
                            NULL);
    fmanager->priv->channel = g_object_ref(G_OBJECT(channel));

    xfconf_g_property_bind(channel, SETTING_SHOW_FILESYSTEM, G_TYPE_BOOLEAN,
                           G_OBJECT(fmanager), "show-filesystem");
    xfconf_g_property_bind(channel, SETTING_SHOW_HOME, G_TYPE_BOOLEAN,
                           G_OBJECT(fmanager), "show-home");
    xfconf_g_property_bind(channel, SETTING_SHOW_TRASH, G_TYPE_BOOLEAN,
                           G_OBJECT(fmanager), "show-trash");
    xfconf_g_property_bind(channel, SETTING_SHOW_REMOVABLE, G_TYPE_BOOLEAN,
                           G_OBJECT(fmanager), "show-removable");
    xfconf_g_property_bind(channel, SETTING_SHOW_THUMBNAILS, G_TYPE_BOOLEAN,
                           G_OBJECT(fmanager), "show-thumbnails");

    return XFDESKTOP_ICON_VIEW_MANAGER(fmanager);
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

static void
xfdesktop_file_icon_manager_requeue_thumbnails(gpointer key,
                                               gpointer value,
                                               gpointer data)
{
    GFile *file = key;
    XfdesktopFileIconManager *fmanager = XFDESKTOP_FILE_ICON_MANAGER(data);

    xfdesktop_thumbnailer_queue_thumbnail(fmanager->priv->thumbnailer,
                                          g_file_get_path(file));
}

static void
xfdesktop_file_icon_manager_remove_thumbnails(gpointer key,
                                              gpointer value,
                                              gpointer data)
{
    XfdesktopRegularFileIcon *icon = XFDESKTOP_REGULAR_FILE_ICON(value);

    xfdesktop_icon_delete_thumbnail(XFDESKTOP_ICON(icon));
}

void
xfdesktop_file_icon_manager_set_show_thumbnails(XfdesktopFileIconManager *manager,
                                                gboolean show_thumbnails)
{
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MANAGER(manager));

    if(show_thumbnails == manager->priv->show_thumbnails)
        return;

    manager->priv->show_thumbnails = show_thumbnails;

    if(!manager->priv->inited)
        return;

    if(show_thumbnails) {
        /* We have to request to create the thumbnails everytime. */
         g_hash_table_foreach(manager->priv->icons,
                         xfdesktop_file_icon_manager_requeue_thumbnails,
                         manager);
    } else {
        /* We have to remove the thumbnails because the regular file
         * icons can't easily check if thumbnails are allowed.
         */
         g_hash_table_foreach(manager->priv->icons,
                         xfdesktop_file_icon_manager_remove_thumbnails,
                         manager);
    }
}

gboolean
xfdesktop_file_icon_manager_get_show_thumbnails(XfdesktopFileIconManager *manager)
{
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON_MANAGER(manager), FALSE);
    return manager->priv->show_thumbnails;
}

void
xfdesktop_file_icon_manager_set_show_special_file(XfdesktopFileIconManager *manager,
                                                  XfdesktopSpecialFileIconType type,
                                                  gboolean show_special_file)
{
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MANAGER(manager));
    g_return_if_fail((int)type >= 0 && type <= XFDESKTOP_SPECIAL_FILE_ICON_TRASH);
    
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
    g_return_val_if_fail((int)type >= 0 && type <= XFDESKTOP_SPECIAL_FILE_ICON_TRASH,
                         FALSE);
    
    return manager->priv->show_special[type];
}

static void
xfdesktop_file_icon_manager_update_image(GtkWidget *widget,
                                         gchar *srcfile,
                                         gchar *thumbfile,
                                         XfdesktopFileIconManager *manager)
{
    GFile *file;
    XfdesktopIcon *icon;

    g_return_if_fail(srcfile && thumbfile);
    g_return_if_fail(XFDESKTOP_FILE_ICON_MANAGER(manager));

    file = g_file_new_for_path(srcfile);

    icon = g_hash_table_lookup(manager->priv->icons, file);
    if(icon)
    {
        g_object_unref(file);
        file = g_file_new_for_path(thumbfile);
        xfdesktop_icon_set_thumbnail_file(icon, file);
    }
}
