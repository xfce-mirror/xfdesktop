/*  xfce4
 *  
 *  Copyright (C) 2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <errno.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libxfce4mcs/mcs-manager.h>
#include <libxfce4util/i18n.h>
#include <libxfce4util/util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "settings_common.h"
#include "backdrop.h"
#include "backdrop-mgr.h"

/* exported interface */
gboolean is_backdrop_list(const char *path)
{
    FILE *fp;
    char buf[512];
    int size;
    gboolean is_list = FALSE;

    size = sizeof(LIST_TEXT) - 1;
    fp = fopen(path, "r");

    if(!fp)
        return FALSE;

    if(fgets(buf, size + 1, fp) && strncmp(LIST_TEXT, buf, size) == 0)
        is_list = TRUE;

    fclose(fp);
    return is_list;
}

/* things to keep track of */
typedef struct
{
    gboolean changed;
    
    GtkWidget *dialog;

    char *last_dir;
    GtkWidget *treeview;

    char *filename;
    GtkWidget *file_entry;

    ListMgrCb callback;
    gpointer data;
}
ListDialog;

/* add file to list */
static gboolean check_image(const char *path)
{
    GdkPixbuf *tmp;
    GError *error = NULL;

    tmp = gdk_pixbuf_new_from_file(path, &error);

    if (error) {
	    g_warning("Could not create image from file %s: %s\n", path,
                error->message);

	    return FALSE;
    }
    else
	    return TRUE;
}

static void add_file(const char *path, ListDialog *ld)
{
    GtkTreeModel *tm = gtk_tree_view_get_model(GTK_TREE_VIEW(ld->treeview));
    GtkTreeIter iter;

    if (!check_image(path))
        return;
    
    ld->changed = TRUE;
    
    gtk_list_store_append(GTK_LIST_STORE(tm), &iter);

    gtk_list_store_set(GTK_LIST_STORE(tm), &iter, 0, path, -1);
}

/* remove from list */
static void remove_file(ListDialog *ld)
{
    GtkTreeSelection *select;
    GtkTreeIter iter;
    GtkTreeModel *model;

    gtk_widget_grab_focus(ld->treeview);
    
    select = gtk_tree_view_get_selection (GTK_TREE_VIEW (ld->treeview));

    if (gtk_tree_selection_get_selected (select, &model, &iter))
	    gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
}

/* reading and writing files */
static void
read_file(const char *filename, ListDialog *ld)
{
    gchar **files;
    gchar **file;

    if ((files = get_list_from_file(filename)) != NULL) {
        for (file = files; *file != NULL; file++)
	        add_file(*file, ld);

        g_strfreev(files);
    }
}

static gboolean
save_list_file(ListDialog *ld)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    char *file;
    FILE *fp;
    int fd;

#ifdef O_EXLOCK
    if ((fd = open(ld->filename,O_CREAT|O_EXLOCK|O_TRUNC|O_WRONLY, 0640)) < 0) {
#else
    if ((fd = open(ld->filename, O_CREAT | O_TRUNC | O_WRONLY, 0640)) < 0) {
#endif
	    xfce_err(_("Could not save file %s: %s\n\n"
		      	   "Please choose another location or press "
		  	       "cancel in the dialog to discard your changes"), 
    			   ld->filename, g_strerror(errno));
	    return(FALSE);
    }

    if ((fp = fdopen(fd, "w")) == NULL) {
        g_warning("Unable to fdopen(%s). This should not happen!\n",
                ld->filename);
        (void)close(fd);
        return(FALSE);
    }

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(ld->treeview));

    fprintf(fp, "%s\n", LIST_TEXT);

    if (!gtk_tree_model_get_iter_first(model, &iter)) {
        fclose(fp);
        return TRUE;
    }
    else {
        gtk_tree_model_get(model, &iter, 0, &file, -1);
        fprintf(fp, "%s", file);
        g_free(file);
    }
	
    while (gtk_tree_model_iter_next(model, &iter)) {
        gtk_tree_model_get(model, &iter, 0, &file, -1);
        fprintf(fp, "\n%s", file);
        g_free(file);
    }

    (void)fclose(fp);
	
    return TRUE;
}

/* dialog response */
static gboolean list_dialog_delete(ListDialog *ld)
{
    GtkTreeModel *store;

    store = gtk_tree_view_get_model(GTK_TREE_VIEW(ld->treeview));
    gtk_list_store_clear(GTK_LIST_STORE(store));
    
    gtk_widget_destroy(ld->dialog);
    g_free(ld->filename);
    g_free(ld);

    return TRUE;
}

static void list_dialog_response(GtkWidget *w, int response, ListDialog *ld)
{
    if (response == GTK_RESPONSE_OK)
    {
	if (ld->changed)
	{
	    if (!save_list_file(ld))
	    {
		return;
	    }
	    else
	    {
		/* run callback */
		ld->callback(ld->filename, ld->data);
	    }
	}
    }
    
    list_dialog_delete(ld);
}

/* treeview */
static void
on_drag_data_received(GtkWidget * w, GdkDragContext * context,
                      int x, int y, GtkSelectionData * data,
                      guint info, guint time, ListDialog * ld)
{
    char buf[1024];
    char *file = NULL;
    char *end;

    /* copy data to buffer */
    strncpy(buf, (char *)data->data, 1023);
    buf[1023] = '\0';

    if((end = strchr(buf, '\n')))
        *end = '\0';

    if((end = strchr(buf, '\r')))
        *end = '\0';
    
    if(buf[0])
    {
        file = buf;

        if(strncmp("file:", file, 5) == 0)
        {
            file += 5;

            if(strncmp("///", file, 3) == 0)
                file += 2;
        }

	add_file(file, ld);
    }

    gtk_drag_finish(context, (file != NULL),
                    (context->action == GDK_ACTION_MOVE), time);
}

/* Don't use 'text/plain' as target.
 * Otherwise backdrop lists can not be dropped
 */
enum
{
  TARGET_STRING,
  TARGET_URL
};

static GtkTargetEntry target_table[] = {
    {"STRING", 0, TARGET_STRING},
    {"text/uri-list", 0, TARGET_URL},
};

static void add_tree_view(GtkWidget *vbox, const char *path, ListDialog *ld)
{
    GtkWidget *treeview_scroll;
    GtkListStore *store;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;

    treeview_scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_show (treeview_scroll);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (treeview_scroll), 
	    			    GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW (treeview_scroll),
	    				GTK_SHADOW_IN);
    gtk_box_pack_start(GTK_BOX(vbox), treeview_scroll, TRUE, TRUE, 0);

    store = gtk_list_store_new (1, G_TYPE_STRING);

    ld->treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
    gtk_widget_show(ld->treeview);
    gtk_container_add(GTK_CONTAINER(treeview_scroll), ld->treeview);

    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(ld->treeview), FALSE);

    if (path)
	read_file (path, ld);
    
    g_object_unref (G_OBJECT (store));

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("File", renderer,
						       "text", 0, NULL);

    gtk_tree_view_append_column (GTK_TREE_VIEW (ld->treeview), column);

    gtk_drag_dest_set(ld->treeview, GTK_DEST_DEFAULT_ALL,
                      target_table, G_N_ELEMENTS(target_table),
                      GDK_ACTION_COPY | GDK_ACTION_MOVE);

    g_signal_connect(ld->treeview, "drag_data_received",
                     G_CALLBACK(on_drag_data_received), ld);
}

/* list buttons */
#if 0
static void list_up_cb(GtkWidget *b, ListDialog *ld)
{
}

static void list_down_cb(GtkWidget *b, ListDialog *ld)
{
}
#endif

static void list_remove_cb(GtkWidget *b, ListDialog *ld)
{
    remove_file(ld);
}

static void list_add_ok(GtkWidget *b, ListDialog *ld)
{
    GtkFileSelection * fs;
    const char *path;

    fs = GTK_FILE_SELECTION(gtk_widget_get_toplevel(b));

    path = gtk_file_selection_get_filename(fs);
    
    if (path)
    {
	char *dir;
	
	dir = g_path_get_dirname(path);
	g_free(ld->last_dir);
	ld->last_dir = g_strconcat(dir, G_DIR_SEPARATOR_S, NULL);
	g_free(dir);

	add_file(path, ld);
    }

    gtk_widget_destroy(GTK_WIDGET(fs));
}

static void list_add_cb(GtkWidget *b, ListDialog *ld)
{
    static GtkFileSelection *fs = NULL;
    char *title;
    
    if (fs)
	gtk_window_present(GTK_WINDOW(fs));
    
    title = _("Select image file");
    fs = GTK_FILE_SELECTION(preview_file_selection_new (title, TRUE));
    
    gtk_file_selection_hide_fileop_buttons(fs);
    
    gtk_file_selection_set_filename(fs, ld->last_dir);
    
    gtk_window_set_transient_for (GTK_WINDOW (fs), GTK_WINDOW (ld->dialog));

    g_signal_connect (fs->ok_button, "clicked", G_CALLBACK (list_add_ok), ld);

    g_signal_connect_swapped (fs->cancel_button, "clicked", 
	    		      G_CALLBACK (gtk_widget_destroy), fs);

    g_signal_connect(fs, "delete-event", G_CALLBACK (gtk_widget_destroy), fs);

    g_object_add_weak_pointer(G_OBJECT(fs), (gpointer *)&fs);

    gtk_widget_show(GTK_WIDGET(fs));
}

static void add_list_buttons(GtkWidget *vbox, ListDialog *ld)
{
    GtkWidget *hbox, *button, *image;
    
    hbox = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_END);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    /* up 
    button = gtk_button_new();
    gtk_widget_show(button);
    gtk_container_add(GTK_CONTAINER(hbox), button);

    image = gtk_image_new_from_stock(GTK_STOCK_GO_UP, GTK_ICON_SIZE_BUTTON);
    gtk_widget_show(image);
    gtk_container_add(GTK_CONTAINER(button), image);

    g_signal_connect(button, "clicked", G_CALLBACK(list_up_cb), ld);
*/    
    /* down 
    button = gtk_button_new();
    gtk_widget_show(button);
    gtk_container_add(GTK_CONTAINER(hbox), button);

    image = gtk_image_new_from_stock(GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_BUTTON);
    gtk_widget_show(image);
    gtk_container_add(GTK_CONTAINER(button), image);

    g_signal_connect(button, "clicked", G_CALLBACK(list_down_cb), ld);
*/
    /* remove */
    button = gtk_button_new();
    gtk_widget_show(button);
    gtk_container_add(GTK_CONTAINER(hbox), button);

    image = gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_BUTTON);
    gtk_widget_show(image);
    gtk_container_add(GTK_CONTAINER(button), image);

    g_signal_connect(button, "clicked", G_CALLBACK(list_remove_cb), ld);

    /* add */
    button = gtk_button_new();
    gtk_widget_show(button);
    gtk_container_add(GTK_CONTAINER(hbox), button);

    image = gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON);
    gtk_widget_show(image);
    gtk_container_add(GTK_CONTAINER(button), image);

    g_signal_connect(button, "clicked", G_CALLBACK(list_add_cb), ld);
}

/* add file entry */
static void fs_ok_cb (GtkWidget *b, ListDialog *ld)
{
    GtkFileSelection * fs;
    const char *path;

    fs = GTK_FILE_SELECTION(gtk_widget_get_toplevel(b));

    path = gtk_file_selection_get_filename(fs);
    
    if (path)
    {
	ld->changed = TRUE;

	g_free(ld->filename);
	ld->filename = g_strdup(path);

	gtk_entry_set_text(GTK_ENTRY(ld->file_entry), path);
    }

    gtk_widget_destroy(GTK_WIDGET(fs));
}

static void filename_browse_cb (GtkWidget * b, ListDialog * ld)
{
    static GtkFileSelection *fs = NULL;
    char *title;
    
    if (fs)
	gtk_window_present(GTK_WINDOW(fs));
    
    title = _("Choose backdrop list filename");
    fs = GTK_FILE_SELECTION(preview_file_selection_new (title, TRUE));
    
    gtk_file_selection_set_filename(fs, ld->filename);
    
    gtk_window_set_transient_for (GTK_WINDOW (fs), GTK_WINDOW (ld->dialog));

    g_signal_connect (fs->ok_button, "clicked", G_CALLBACK (fs_ok_cb), ld);

    g_signal_connect_swapped (fs->cancel_button, "clicked", 
	    		      G_CALLBACK (gtk_widget_destroy), fs);

    g_signal_connect(fs, "delete-event", G_CALLBACK (gtk_widget_destroy), fs);

    g_object_add_weak_pointer(G_OBJECT(fs), (gpointer *)&fs);

    gtk_widget_show(GTK_WIDGET(fs));
}

static void add_file_entry(GtkWidget *vbox, ListDialog *ld)
{
    GtkWidget *hbox, *label, *button;

    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("File:"));
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    ld->file_entry = gtk_entry_new();
    gtk_widget_show(ld->file_entry);
    gtk_entry_set_text(GTK_ENTRY(ld->file_entry), ld->filename);
    gtk_widget_set_size_request(ld->file_entry, 300, -1);
    gtk_box_pack_start(GTK_BOX(hbox), ld->file_entry, TRUE, TRUE, 0);
    
    button = gtk_button_new_with_label("...");
    gtk_widget_show(button);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

    g_signal_connect(button, "clicked", G_CALLBACK(filename_browse_cb), ld);
}

/* the dialog */
static void list_mgr_dialog(const char *title, GtkWidget *parent, 
			     const char *path, 
			     ListMgrCb callback, gpointer data)
{
    static GtkWidget *dialog = NULL;
    GtkWidget *mainvbox, *frame, *vbox, *header, *button;
    ListDialog *ld;

    if (dialog) {
	    gtk_window_present(GTK_WINDOW(dialog));
	    return;
    }
    
    ld = g_new0(ListDialog, 1);

    ld->callback = callback;
    ld->data = data;

    if (path)
	    ld->filename = g_strdup(path);
    else
	    ld->filename = xfce_get_homefile(_("New.list"), NULL);
    
    ld->last_dir = g_build_filename(DATADIR, "xfce4", "backdrops/", NULL);
    
    dialog = gtk_dialog_new_with_buttons(_("Backdrop List"), 
	    				 GTK_WINDOW(parent),
	    				 GTK_DIALOG_NO_SEPARATOR, 
					 NULL);

    g_object_add_weak_pointer(G_OBJECT(dialog), (gpointer)&dialog);
    
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
    gtk_widget_set_size_request (dialog, -1, 400);
    
    ld->dialog = dialog;

    button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    gtk_widget_show(button);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog), button, 
	    			 GTK_RESPONSE_CANCEL);
    
    button = gtk_button_new_from_stock(GTK_STOCK_SAVE);
    gtk_widget_show(button);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog), button, 
	    			 GTK_RESPONSE_OK);
    
    g_signal_connect(dialog, "response", G_CALLBACK(list_dialog_response), ld);
    g_signal_connect_swapped(dialog, "delete-event", 
	    		     G_CALLBACK(list_dialog_delete), ld);
    
    mainvbox = GTK_DIALOG(dialog)->vbox;
    
    header = create_header(NULL, title);
    gtk_widget_show(header);
    gtk_box_pack_start(GTK_BOX(mainvbox), header, FALSE, TRUE, 0);
    gtk_widget_set_size_request(header, -1, 50);

    frame = gtk_frame_new(_("Image files"));
    gtk_container_set_border_width(GTK_CONTAINER(frame), BORDER);
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(mainvbox), frame, TRUE, TRUE, 0);
    
    vbox = gtk_vbox_new(FALSE, BORDER);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), BORDER);
    gtk_widget_show(vbox);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    add_tree_view(vbox, path, ld);
    
    add_list_buttons(vbox, ld);

    frame = gtk_frame_new(_("List file"));
    gtk_container_set_border_width(GTK_CONTAINER(frame), BORDER);
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(mainvbox), frame, FALSE, FALSE, 0);
    
    vbox = gtk_vbox_new(FALSE, BORDER);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), BORDER);
    gtk_widget_show(vbox);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    add_file_entry(vbox, ld);

    add_spacer(GTK_BOX(mainvbox));

    gtk_widget_show(dialog);
}

/* exported interface */
void create_list_file(GtkWidget *parent, ListMgrCb callback, gpointer data)
{
    list_mgr_dialog(_("Create backdrop list"), parent, NULL,
		    callback, data);
}

void edit_list_file(const char *path, GtkWidget *parent, 
		     ListMgrCb callback, gpointer data)
{
    list_mgr_dialog(_("Edit backdrop list"), parent, path, 
    		    callback, data);
}



