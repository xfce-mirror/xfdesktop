/*  xfce4
 *  
 *  Copyright (C) 2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *                2004 Brian Tarricone <bjt23@cornell.edu>
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

#include <ctype.h>
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
#include "backdrop-common.h"
#include "backdrop-mgr.h"

static gchar *_listdlg_last_dir;

#if !GTK_CHECK_VERSION(2, 4, 0)
static void
filesel_response_accept(GtkWidget *w, gpointer user_data)
{
	GtkDialog *dialog = GTK_DIALOG(user_data);
	gtk_dialog_response(dialog, GTK_RESPONSE_ACCEPT);
}

static void
filesel_response_cancel(GtkWidget *w, gpointer user_data)
{
	GtkDialog *dialog = GTK_DIALOG(user_data);
	gtk_dialog_response(dialog, GTK_RESPONSE_CANCEL);
}
#endif

/* add file to list */
static gboolean
check_image (const char *path)
{
    GdkPixbuf *tmp;
    GError *error = NULL;
	gboolean ret = TRUE;

    tmp = gdk_pixbuf_new_from_file(path, &error);

	if(!tmp) {
		g_warning ("Could not create image from file %s: %s\n", path,
				error->message);
		ret = FALSE;
	}
	
	g_object_unref(G_OBJECT(tmp));
	
	return ret;
}

static void
add_file(const gchar *path, GtkListStore *ls)
{
	GtkTreeIter iter;

	if(!check_image(path))
		return;

	gtk_list_store_append(ls, &iter);
	gtk_list_store_set(ls, &iter, 0, path, -1);
}

/* remove from list */
static void
remove_file(GtkTreeView *treeview)
{
	GtkTreeSelection *select;
	GtkTreeIter iter;
	GtkTreeModel *model;
	
	gtk_widget_grab_focus(GTK_WIDGET(treeview));
	
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	
	if(gtk_tree_selection_get_selected(select, &model, &iter))
		gtk_list_store_remove (GTK_LIST_STORE(model), &iter);
}

/* reading and writing files */
static void
read_file(const gchar *filename, GtkListStore *ls)
{
	gchar **files;
	gchar **file;

	if((files = get_list_from_file (filename)) != NULL) {
		for(file = files; *file != NULL; file++)
			add_file(*file, ls);
		g_strfreev (files);
	}
}

static gboolean
save_list_file(const gchar *filename, GtkListStore *ls)
{
	//GtkTreeModel *model;
	GtkTreeIter iter;
	char *file;
	FILE *fp;
	int fd;

#ifdef O_EXLOCK
	if((fd = open (filename, O_CREAT|O_EXLOCK|O_TRUNC|O_WRONLY, 0640)) < 0) {
#else
	if((fd = open (filename, O_CREAT| O_TRUNC|O_WRONLY, 0640)) < 0) {
#endif
		xfce_err (_("Could not save file %s: %s\n\n"
				"Please choose another location or press "
				"cancel in the dialog to discard your changes"),
				filename, g_strerror(errno));
		return (FALSE);
	}

    if((fp = fdopen (fd, "w")) == NULL) {
		g_warning ("Unable to fdopen(%s). This should not happen!\n", filename);
		close(fd);
		return FALSE;
	}

    fprintf (fp, "%s\n", LIST_TEXT);
	if(!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ls), &iter)) {
		fclose (fp);
		return TRUE;
	} else {
		gtk_tree_model_get(GTK_TREE_MODEL(ls), &iter, 0, &file, -1);
		fprintf(fp, "%s", file);
		g_free(file);
	}

	while(gtk_tree_model_iter_next(GTK_TREE_MODEL(ls), &iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(ls), &iter, 0, &file, -1);
		fprintf(fp, "\n%s", file);
		g_free(file);
	}

	fclose(fp);

	return TRUE;
}

/* treeview */
/* DND */

/*** the next three routines are taken straight from gnome-libs ***/
/**
 * gnome_uri_list_free_strings:
 * @list: A GList returned by gnome_uri_list_extract_uris() or gnome_uri_list_extract_filenames()
 *
 * Releases all of the resources allocated by @list.
 */
void
gnome_uri_list_free_strings (GList * list)
{
    g_list_foreach (list, (GFunc) g_free, NULL);
    g_list_free (list);
}


/**
 * gnome_uri_list_extract_uris:
 * @uri_list: an uri-list in the standard format.
 *
 * Returns a GList containing strings allocated with g_malloc
 * that have been splitted from @uri-list.
 */
GList *
gnome_uri_list_extract_uris (const gchar * uri_list)
{
    const gchar *p, *q;
    gchar *retval;
    GList *result = NULL;

    g_return_val_if_fail (uri_list != NULL, NULL);

    p = uri_list;

    /* We don't actually try to validate the URI according to RFC
     * 2396, or even check for allowed characters - we just ignore
     * comments and trim whitespace off the ends.  We also
     * allow LF delimination as well as the specified CRLF.
     */
    while (p)
    {
	if (*p != '#')
	{
	    while (isspace ((int) (*p)))
		p++;

	    q = p;
	    while (*q && (*q != '\n') && (*q != '\r'))
		q++;

	    if (q > p)
	    {
		q--;
		while (q > p && isspace ((int) (*q)))
		    q--;

		retval = (char *) g_malloc (q - p + 2);
		strncpy (retval, p, q - p + 1);
		retval[q - p + 1] = '\0';

		result = g_list_prepend (result, retval);
	    }
	}
	p = strchr (p, '\n');
	if (p)
	    p++;
    }

    return g_list_reverse (result);
}


/**
 * gnome_uri_list_extract_filenames:
 * @uri_list: an uri-list in the standard format
 *
 * Returns a GList containing strings allocated with g_malloc
 * that contain the filenames in the uri-list.
 *
 * Note that unlike gnome_uri_list_extract_uris() function, this
 * will discard any non-file uri from the result value.
 */
GList *
gnome_uri_list_extract_filenames (const gchar * uri_list)
{
    GList *tmp_list, *node, *result;

    g_return_val_if_fail (uri_list != NULL, NULL);

    result = gnome_uri_list_extract_uris (uri_list);

    tmp_list = result;
    while (tmp_list)
    {
	gchar *s = (char *) tmp_list->data;

	node = tmp_list;
	tmp_list = tmp_list->next;

	if (!strncmp (s, "file:", 5))
	{
	    /* added by Jasper Huijsmans
	       remove leading multiple slashes */
	    if (!strncmp (s + 5, "///", 3))
		node->data = g_strdup (s + 7);
	    else
		node->data = g_strdup (s + 5);
	}
	else
	{
	    node->data = g_strdup (s);
	}
	g_free (s);
    }
    return result;
}

/* data dropped */
static void
on_drag_data_received (GtkWidget * w, GdkDragContext * context,
		       int x, int y, GtkSelectionData * data,
		       guint info, guint time, gpointer user_data)
{
	GList *flist, *li;
	gchar *file;

	flist = gnome_uri_list_extract_filenames((gchar *)data->data);

	for(li = flist; li; li = li->next) {
		file = li->data;
		add_file(file, (GtkListStore *)user_data);
	}

	gtk_drag_finish(context, FALSE, (context->action == GDK_ACTION_MOVE), time);

    gnome_uri_list_free_strings(flist);
}

/* Don't use 'text/plain' as target.
 * Otherwise backdrop lists can not be dropped
 */
enum
{
    TARGET_URL,
    TARGET_STRING,
};

static GtkTargetEntry target_table[] = {
    {"text/uri-list", 0, TARGET_URL},
    {"STRING", 0, TARGET_STRING},
};

static GtkTreeView *
add_tree_view(GtkWidget *vbox, const gchar *path)
{
    GtkWidget *treeview_scroll, *treeview;
    GtkListStore *store;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;

    treeview_scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_show (treeview_scroll);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (treeview_scroll),
				    GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW
					 (treeview_scroll), GTK_SHADOW_IN);
    gtk_box_pack_start (GTK_BOX (vbox), treeview_scroll, TRUE, TRUE, 0);

    store = gtk_list_store_new (1, G_TYPE_STRING);

    treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_widget_show(treeview);
    gtk_container_add(GTK_CONTAINER(treeview_scroll), treeview);

    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);

	if(path)
		read_file(path, store);

    g_object_unref (G_OBJECT (store));

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("File", renderer,
						       "text", 0, NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	gtk_drag_dest_set(GTK_WIDGET(treeview), GTK_DEST_DEFAULT_ALL, target_table,
			G_N_ELEMENTS(target_table), GDK_ACTION_COPY|GDK_ACTION_MOVE);

    g_signal_connect(treeview, "drag_data_received",
		      G_CALLBACK(on_drag_data_received), store);
	
	return GTK_TREE_VIEW(treeview);
}

/* list buttons */
#if 0
static void
list_up_cb (GtkWidget * b, ListDialog * ld)
{
}

static void
list_down_cb (GtkWidget * b, ListDialog * ld)
{
}
#endif

static void
list_remove_cb(GtkWidget * b, GtkTreeView *treeview)
{
	remove_file(treeview);
}

static void
update_preview_cb(XfceFileChooser *chooser, gpointer data)
{
	GtkImage *preview;
	char *filename;
	GdkPixbuf *pix = NULL;
	
	preview = GTK_IMAGE(data);
	filename = xfce_file_chooser_get_filename(chooser);
	
	if(g_file_test(filename, G_FILE_TEST_IS_REGULAR))
		pix = gdk_pixbuf_new_from_file_at_size(filename, 250, 250, NULL);
	g_free(filename);
	
	if(pix) {
		gtk_image_set_from_pixbuf(preview, pix);
		g_object_unref(G_OBJECT(pix));
	}
	xfce_file_chooser_set_preview_widget_active(chooser, (pix != NULL));
}

static void
list_add_cb(GtkWidget *b, GtkTreeView *treeview)
{
	GtkWidget *chooser, *preview, *dialog;
	GtkFileFilter *filter;
	
	dialog = gtk_widget_get_toplevel(GTK_WIDGET(treeview));
	
	chooser = xfce_file_chooser_dialog_new(_("Select backdrop image file"),
			GTK_WINDOW(dialog), XFCE_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("All Files"));
	gtk_file_filter_add_pattern(filter, "*");
	xfce_file_chooser_add_filter(XFCE_FILE_CHOOSER(chooser), filter);
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Image Files"));
	gtk_file_filter_add_pattern(filter, "*.png");
	gtk_file_filter_add_pattern(filter, "*.jpg");
	gtk_file_filter_add_pattern(filter, "*.bmp");
	gtk_file_filter_add_pattern(filter, "*.svg");
	gtk_file_filter_add_pattern(filter, "*.xpm");
	gtk_file_filter_add_pattern(filter, "*.gif");
	xfce_file_chooser_add_filter(XFCE_FILE_CHOOSER(chooser), filter);
	
	xfce_file_chooser_set_local_only(XFCE_FILE_CHOOSER(chooser), TRUE);
	xfce_file_chooser_add_shortcut_folder(XFCE_FILE_CHOOSER(chooser),
			DATADIR "/xfce4/backdrops", NULL);
	
	if(_listdlg_last_dir)
		xfce_file_chooser_set_current_folder(XFCE_FILE_CHOOSER(chooser),
				_listdlg_last_dir);
	
	preview = gtk_image_new();
	gtk_widget_show(preview);
	xfce_file_chooser_set_preview_widget(XFCE_FILE_CHOOSER(chooser), preview);
	xfce_file_chooser_set_preview_widget_active(XFCE_FILE_CHOOSER(chooser), FALSE);
	xfce_file_chooser_set_preview_callback(XFCE_FILE_CHOOSER(chooser),
			(PreviewUpdateFunc)update_preview_cb, preview);
	
	gtk_widget_show(chooser);
	if(gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename;
		
		filename = xfce_file_chooser_get_filename(XFCE_FILE_CHOOSER(chooser));
		if(filename) {
			if(_listdlg_last_dir)
				g_free(_listdlg_last_dir);
			_listdlg_last_dir = g_path_get_dirname(filename);
			add_file(filename, GTK_LIST_STORE(gtk_tree_view_get_model(treeview)));
			g_free(filename);
		}
	}
	gtk_widget_destroy(chooser);
}

static void
add_list_buttons(GtkWidget *vbox, GtkTreeView *treeview)
{
    GtkWidget *hbox, *button, *image;

    hbox = gtk_hbutton_box_new ();
    gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
    gtk_widget_show (hbox);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

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
    button = gtk_button_new ();
    gtk_widget_show (button);
    gtk_container_add (GTK_CONTAINER (hbox), button);

    image = gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_BUTTON);
    gtk_widget_show (image);
    gtk_container_add (GTK_CONTAINER (button), image);

    g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(list_remove_cb), treeview);

    /* add */
    button = gtk_button_new ();
    gtk_widget_show (button);
    gtk_container_add (GTK_CONTAINER (hbox), button);

    image = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON);
    gtk_widget_show (image);
    gtk_container_add (GTK_CONTAINER (button), image);

    g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(list_add_cb), treeview);
}

static void
filename_browse_cb(GtkWidget *b, GtkWidget *file_entry)
{
	GtkWidget *chooser, *preview, *dialog;
	GtkFileFilter *filter;
	
	dialog = gtk_widget_get_toplevel(b);
	
	preview = gtk_image_new();
	chooser = xfce_file_chooser_dialog_new(_("Choose backdrop list filename"),
			GTK_WINDOW(dialog), XFCE_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("All Files"));
	gtk_file_filter_add_pattern(filter, "*");
	xfce_file_chooser_add_filter(XFCE_FILE_CHOOSER(chooser), filter);
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("List Files"));
	gtk_file_filter_add_pattern(filter, "*.list");
	xfce_file_chooser_add_filter(XFCE_FILE_CHOOSER(chooser), filter);
	
	xfce_file_chooser_set_local_only(XFCE_FILE_CHOOSER(chooser), TRUE);
	
	if(_listdlg_last_dir)
		xfce_file_chooser_set_current_folder(XFCE_FILE_CHOOSER(chooser),
				_listdlg_last_dir);
	
	gtk_widget_show(chooser);
	if(gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
		gchar *path;
		
		path = xfce_file_chooser_get_filename(XFCE_FILE_CHOOSER(chooser));
		if(path) {
			gtk_entry_set_text(GTK_ENTRY(file_entry), path);
			g_free(path);
		}
	}
	gtk_widget_destroy(chooser);
}

static GtkWidget *
add_file_entry (GtkWidget *vbox, const gchar *filename)
{
    GtkWidget *hbox, *button, *image, *file_entry;

    hbox = gtk_hbox_new (FALSE, BORDER);
    gtk_widget_show (hbox);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    file_entry = gtk_entry_new();
    gtk_widget_show(file_entry);
    gtk_entry_set_text(GTK_ENTRY(file_entry), filename);
    gtk_widget_set_size_request(file_entry, 300, -1);
    gtk_box_pack_start(GTK_BOX(hbox), file_entry, TRUE, TRUE, 0);

    button = gtk_button_new();
    gtk_widget_show(button);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

    image = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
    gtk_widget_show(image);
    gtk_container_add(GTK_CONTAINER(button), image);

    g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(filename_browse_cb), file_entry);
	
	return file_entry;
}

/* the dialog */
static void
list_mgr_dialog_new(const gchar *title, GtkWidget *parent, const gchar *path,
	GtkWidget **dialog, GtkWidget **entry, GtkListStore **ls)
{
    GtkWidget *mainvbox, *frame, *vbox, *header, *button;
	GtkTreeView *treeview;
	const gchar *filename;

	g_return_if_fail(dialog != NULL && entry != NULL && ls != NULL);
	
	if(!_listdlg_last_dir)
		_listdlg_last_dir = g_build_path(G_DIR_SEPARATOR_S, DATADIR, "xfce4",
				"backdrops", NULL);

	*dialog = gtk_dialog_new_with_buttons(_("Backdrop List"), GTK_WINDOW(parent),
			GTK_DIALOG_NO_SEPARATOR, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);

    gtk_window_set_position(GTK_WINDOW(*dialog), GTK_WIN_POS_MOUSE);
    gtk_window_set_resizable(GTK_WINDOW(*dialog), FALSE);
    gtk_widget_set_size_request(*dialog, -1, 400);

    mainvbox = GTK_DIALOG(*dialog)->vbox;

    header = xfce_create_header(NULL, title);
    gtk_widget_show(header);
    gtk_box_pack_start(GTK_BOX(mainvbox), header, FALSE, TRUE, 0);
    gtk_widget_set_size_request(header, -1, 50);

    add_spacer(GTK_BOX(mainvbox));

    frame = xfce_framebox_new(_("Image files"), FALSE);
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(mainvbox), frame, TRUE, TRUE, 0);

    vbox = gtk_vbox_new(FALSE, BORDER);
    gtk_widget_show(vbox);
    xfce_framebox_add(XFCE_FRAMEBOX(frame), vbox);

    treeview = add_tree_view(vbox, path);
	*ls = GTK_LIST_STORE(gtk_tree_view_get_model(treeview));
	if(!path)
		 path = xfce_get_homefile(_("New.list"), NULL);

    add_list_buttons(vbox, treeview);

    add_spacer(GTK_BOX(mainvbox));

    frame = xfce_framebox_new(_("List file"), FALSE);
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(mainvbox), frame, FALSE, FALSE, 0);

    vbox = gtk_vbox_new(FALSE, BORDER);
    gtk_widget_show(vbox);
    xfce_framebox_add(XFCE_FRAMEBOX(frame), vbox);

    *entry = add_file_entry(vbox, path);

    add_spacer(GTK_BOX(mainvbox));
}

/* exported interface */
void
create_list_file(GtkWidget *parent, ListMgrCb callback, gpointer data)
{
	GtkWidget *dialog = NULL, *entry = NULL;
	GtkListStore *ls = NULL;
	
    list_mgr_dialog_new(_("Create backdrop list"), parent, NULL, &dialog,
			&entry, &ls);
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename;
		filename = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
		save_list_file(filename, ls);
		(*callback)(filename, data);
		g_free(filename);
	}
	gtk_widget_destroy(dialog);
}

void
edit_list_file(const gchar *path, GtkWidget *parent, ListMgrCb callback,
		gpointer data)
{
	GtkWidget *dialog = NULL, *entry = NULL;
	GtkListStore *ls = NULL;
    
	list_mgr_dialog_new(_("Edit backdrop list"), parent, path, &dialog,
			&entry, &ls);
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename;
		filename = g_strdup(gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1));
		save_list_file(filename, ls);
		(*callback)(filename, data);
		g_free(filename);
	}
	gtk_widget_destroy(dialog);
}
