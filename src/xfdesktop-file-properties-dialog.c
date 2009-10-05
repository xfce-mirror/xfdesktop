/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006 Brian Tarricone, <bjt23@cornell.edu>
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

#include <gtk/gtk.h>

#include <libxfce4ui/libxfce4ui.h>

#include <thunar-vfs/thunar-vfs.h>

#ifdef HAVE_THUNARX
#include <thunarx/thunarx.h>
#endif

#include "xfdesktop-icon.h"
#include "xfdesktop-file-icon.h"
#include "xfdesktop-file-utils.h"
#include "xfdesktop-file-properties-dialog.h"
#include "xfdesktop-common.h"

#define BORDER 8

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
xfdesktop_file_properties_dialog_destroyed(GtkWidget *widget,
                                           gpointer user_data)
{
    GtkTreeModel *model = GTK_TREE_MODEL(user_data);
    gtk_tree_model_foreach(model, xfdesktop_mime_handlers_free_ls, NULL);
}

static void
xfdesktop_file_properties_dialog_set_default_mime_handler(GtkComboBox *combo,
                                                          gpointer user_data)
{
    GtkWindow *parent = user_data ? GTK_WINDOW(user_data) : NULL;
    XfdesktopFileIcon *icon = g_object_get_data(G_OBJECT(combo),
                                                "--xfdesktop-file-icon");
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
        ThunarVfsMimeDatabase *mime_database = thunar_vfs_mime_database_get_default();
        
        if(!thunar_vfs_mime_database_set_default_application(mime_database,
                                                             info->mime_info,
                                                             mime_app,
                                                             &error))
        {
            gchar *primary = g_markup_printf_escaped(_("Unable to set default application for \"%s\" to \"%s\":"),
                                                     xfdesktop_icon_peek_label(XFDESKTOP_ICON(icon)),
                                                     thunar_vfs_mime_application_get_name(mime_app));
            xfce_message_dialog(parent, _("Properties Error"),
                                GTK_STOCK_DIALOG_ERROR, primary, error->message,
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
            g_free(primary);
            g_error_free(error);
        }
        
        g_object_unref(G_OBJECT(mime_database));
    }
}

void
xfdesktop_file_properties_dialog_show(GtkWindow *parent,
                                      XfdesktopFileIcon *icon,
                                      GList *thunarx_properties_providers)
{
    GtkWidget *dlg, *table, *hbox, *lbl, *img, *spacer, *notebook, *vbox,
              *entry, *combo;
    gint row = 0, dw, dh;
    PangoFontDescription *pfd;
    gchar *str = NULL, buf[64];
    const gchar *rname;
    gboolean is_link = FALSE;
    struct tm *tm;
    const ThunarVfsInfo *info;
    ThunarVfsMimeDatabase *mime_database;
    ThunarVfsUserManager *user_manager;
    ThunarVfsUser *user;
    ThunarVfsGroup *group;
    ThunarVfsFileMode mode;
    GList *mime_apps, *l;
    static const gchar *access_types[4] = {
        N_("None"), N_("Write only"), N_("Read only"), N_("Read & Write")
    };
    
    g_return_if_fail(icon);
    info = xfdesktop_file_icon_peek_info(icon);
    g_return_if_fail(info);
    
    pfd = pango_font_description_from_string("bold");
    gtk_icon_size_lookup(GTK_ICON_SIZE_DIALOG, &dw, &dh);
    mime_database = thunar_vfs_mime_database_get_default();
    
    dlg = gtk_dialog_new_with_buttons(xfdesktop_icon_peek_label(XFDESKTOP_ICON(icon)),
                                      parent,
                                      GTK_DIALOG_NO_SEPARATOR,
                                      GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                                      NULL);
    gtk_window_set_icon(GTK_WINDOW(dlg),
                        xfdesktop_icon_peek_pixbuf(XFDESKTOP_ICON(icon), dw));
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
                                                               dw));
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
    
    str = xfdesktop_file_utils_get_file_kind(info, &is_link);
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
                         "ellipsize", PANGO_ELLIPSIZE_START,
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
        mime_apps = thunar_vfs_mime_database_get_applications(mime_database,
                                                              info->mime_info);
        
        if(mime_apps) {
            GtkListStore *ls;
            GtkTreeIter itr;
            GtkCellRenderer *render;
            ThunarVfsMimeHandler *handler;
            const gchar *icon_name;
            gint mw, mh;
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
            
            gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &mw, &mh);
            
            ls = gtk_list_store_new(OPENWITH_N_COLS, GDK_TYPE_PIXBUF,
                                    G_TYPE_STRING, G_TYPE_POINTER);
            for(l = mime_apps; l; l = l->next) {
                handler = THUNAR_VFS_MIME_HANDLER(l->data);
                pix = NULL;
                
                gtk_list_store_append(ls, &itr);
                
                icon_name = thunar_vfs_mime_handler_lookup_icon_name(handler,
                                                                     gtk_icon_theme_get_default());
                if(icon_name) {
                    GtkIconTheme *itheme = gtk_icon_theme_get_default();
                    pix = gtk_icon_theme_load_icon(itheme, icon_name, mw,
                                                   ITHEME_FLAGS, NULL);
                }
                
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
                             G_CALLBACK(xfdesktop_file_properties_dialog_destroyed),
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
            g_object_set_data(G_OBJECT(combo), "--xfdesktop-file-icon", icon);
            gtk_widget_show(combo);
            gtk_table_attach(GTK_TABLE(table), combo, 1, 2, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
            g_signal_connect(G_OBJECT(combo), "changed",
                             G_CALLBACK(xfdesktop_file_properties_dialog_set_default_mime_handler),
                             parent);
            
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
    
    rname = thunar_vfs_user_get_real_name(user);
    if(rname)
        str = g_strdup_printf("%s (%s)", rname, thunar_vfs_user_get_name(user));
    else
        str = (gchar *)thunar_vfs_user_get_name(user);
    lbl = gtk_label_new(str);
    if(rname)
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
    
    g_object_unref(G_OBJECT(mime_database));
    
    pango_font_description_free(pfd);
    
#ifdef HAVE_THUNARX
    if(thunarx_properties_providers) {
        GList *pages, *p, *files = g_list_append(NULL, icon);
        ThunarxPropertyPageProvider *provider;
        ThunarxPropertyPage *page;
        GtkWidget *label_widget;
        const gchar *label;
        
        DBG("adding property pages");
        
        for(l = thunarx_properties_providers; l; l = l->next) {
            provider = THUNARX_PROPERTY_PAGE_PROVIDER(l->data);
            pages = thunarx_property_page_provider_get_pages(provider, files);
            
            DBG("pages found: %d", pages ? g_list_length(pages) : 0);
            
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
