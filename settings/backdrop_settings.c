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

/*  partly taken from gnome theme-switcher capplet 
 *  copyright (c) Jonathan Blandford <jrb@gnome.org>
*/

#include "settings_common.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include <libxfcegui4/libxfcegui4.h>

#include "backdrop_settings.h"
#include "backdrop-icon.h"
#include "backdrop-mgr.h"

#define RCFILE "backdrop.xml"
#define PLUGIN_NAME "backdrop"

#define DEFAULT_ICON_SIZE 48

#ifndef DATADIR
#define DATADIR "/usr/local/share/"
#endif

#ifdef HAVE_GDK_PIXBUF_NEW_FROM_STREAM
#define gdk_pixbuf_new_from_inline gdk_pixbuf_new_from_stream
#endif

#define DEFAULT_BACKDROP (DATADIR "/xfce4/backdrops/xfce4logo.png")

/* important stuff to keep track of */
typedef struct
{
    McsPlugin *plugin;

    /* options dialog */
    GtkWidget *dialog;

    GdkColor color;
    GtkWidget *color_box;
    GtkWidget *color_only_checkbox;
    
    GtkWidget *file_entry;
    GtkWidget *edit_list_button;
    GSList *style_rb_group;
}
BackdropDialog;

/* there can be only one */
static gboolean is_running = FALSE;

static char *backdrop_path = NULL;
static int backdrop_style = CENTERED;
static int showimage = 1;
static McsColor backdrop_color;

static void backdrop_create_channel(McsPlugin * mcs_plugin);
static gboolean backdrop_write_options(McsPlugin * mcs_plugin);
static void run_dialog(McsPlugin * mcs_plugin);

static GdkPixbuf *backdrop_icon_at_size(int width, int height)
{
    GdkPixbuf *base;

    base = gdk_pixbuf_new_from_inline(-1, backdrop_icon_data, FALSE, NULL);

    g_assert(base);

    if((width <= 0 || height <= 0))
    {
        return base;
    }
    else
    {
        GdkPixbuf *scaled;
        int w, h;

        w = width > 0 ? width : gdk_pixbuf_get_width(base);
        h = height > 0 ? height : gdk_pixbuf_get_height(base);

        scaled = gdk_pixbuf_scale_simple(base, w, h, GDK_INTERP_HYPER);
        g_object_unref(G_OBJECT(base));

        return scaled;
    }
}

McsPluginInitResult mcs_plugin_init(McsPlugin * mcs_plugin)
{
    mcs_plugin->plugin_name = g_strdup(PLUGIN_NAME);
    mcs_plugin->caption = g_strdup(_("Desktop: backdrop"));
    mcs_plugin->run_dialog = run_dialog;
    mcs_plugin->icon = backdrop_icon_at_size(DEFAULT_ICON_SIZE,
                                             DEFAULT_ICON_SIZE);

    backdrop_create_channel(mcs_plugin);

    return (MCS_PLUGIN_INIT_OK);
}

static void backdrop_create_channel(McsPlugin * mcs_plugin)
{
    McsSetting *setting;
    const gchar *home = g_getenv("HOME");
    gchar *rcfile = g_build_filename(home, ".xfce4", "settings",
                                     RCFILE, NULL);

    mcs_manager_add_channel_from_file(mcs_plugin->manager, BACKDROP_CHANNEL, 
	    			      rcfile);
    g_free(rcfile);

    setting = mcs_manager_setting_lookup(mcs_plugin->manager, "path", 
	    				 BACKDROP_CHANNEL);
    if(setting)
    {
        if(backdrop_path)
        {
            g_free(backdrop_path);
        }

        backdrop_path = g_strdup(setting->data.v_string);
    }
    else
    {
	if (!backdrop_path)
	    backdrop_path = g_strdup(DEFAULT_BACKDROP);

	mcs_manager_set_string(mcs_plugin->manager, "path", BACKDROP_CHANNEL, 
		    	       backdrop_path);
    }

    setting = mcs_manager_setting_lookup(mcs_plugin->manager, "style", 
	    				 BACKDROP_CHANNEL);
    if(setting)
    {
        backdrop_style = setting->data.v_int;
    }
    else
    {
	mcs_manager_set_int(mcs_plugin->manager, "style", BACKDROP_CHANNEL, 
		    	    backdrop_style);
    }

    setting = mcs_manager_setting_lookup(mcs_plugin->manager, "color", 
	    				 BACKDROP_CHANNEL);
    if(setting)
    {
	backdrop_color.red   = setting->data.v_color.red;
        backdrop_color.green = setting->data.v_color.green;
        backdrop_color.blue  = setting->data.v_color.blue;
        backdrop_color.alpha = setting->data.v_color.alpha;
    }
    else
    {
	backdrop_color.red   = (guint16) 12079;
        backdrop_color.green = (guint16) 13107;
        backdrop_color.blue  = (guint16) 20046;
        backdrop_color.alpha = (guint16) 0;
	mcs_manager_set_color(mcs_plugin->manager, "color", BACKDROP_CHANNEL, 
		    	      &backdrop_color);
    }

    setting = mcs_manager_setting_lookup(mcs_plugin->manager, "showimage", 
	    				 BACKDROP_CHANNEL);
    if(setting)
    {
        showimage = setting->data.v_int;
    }
    else
    {
	mcs_manager_set_int(mcs_plugin->manager, "showimage", BACKDROP_CHANNEL, 
		    	    showimage);
    }

    mcs_manager_notify(mcs_plugin->manager, BACKDROP_CHANNEL);
}

static gboolean backdrop_write_options(McsPlugin * mcs_plugin)
{
    const gchar *home = g_getenv("HOME");
    gchar *rcfile = g_build_filename(home, ".xfce4", "settings", RCFILE, NULL);
    gboolean result;

    result = mcs_manager_save_channel_to_file(mcs_plugin->manager,
                                              BACKDROP_CHANNEL, rcfile);
    g_free(rcfile);

    return result;
}

/* sub header */
static void add_sub_header(GtkWidget *vbox, const char *name)
{
    char *markup;
    GtkWidget *label;

    markup = g_strconcat("<b>", name, "</b>", NULL);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    g_free(markup);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_misc_set_padding(GTK_MISC(label), 0, 4);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
}

/* something changed */
static void update_path(BackdropDialog *bd)
{
    if (is_backdrop_list(backdrop_path))
	gtk_widget_set_sensitive(bd->edit_list_button, TRUE);
    else
	gtk_widget_set_sensitive(bd->edit_list_button, FALSE);

    if (backdrop_path)
    {
        mcs_manager_set_string(bd->plugin->manager, "path", BACKDROP_CHANNEL, backdrop_path);
    }
    
    mcs_manager_notify(bd->plugin->manager, BACKDROP_CHANNEL);
}

static void update_style(BackdropDialog *bd)
{
    mcs_manager_set_int(bd->plugin->manager, "style", BACKDROP_CHANNEL, 
	    		backdrop_style);

    mcs_manager_notify(bd->plugin->manager, BACKDROP_CHANNEL);
}

static void update_color(BackdropDialog *bd)
{
    mcs_manager_set_color(bd->plugin->manager, "color", BACKDROP_CHANNEL, 
	    		  &backdrop_color);

    mcs_manager_notify(bd->plugin->manager, BACKDROP_CHANNEL);
}

static void update_showimage(BackdropDialog *bd)
{
    mcs_manager_set_int(bd->plugin->manager, "showimage", BACKDROP_CHANNEL, 
	    		showimage);

    mcs_manager_notify(bd->plugin->manager, BACKDROP_CHANNEL);
}

/* dialog responses */
static void dialog_delete(BackdropDialog * bd)
{
    is_running = FALSE;

    backdrop_write_options(bd->plugin);

    gtk_widget_destroy(bd->dialog);
    g_free(bd);
}

static void dialog_response(GtkWidget * dialog, int response,
                            BackdropDialog * bd)
{
    dialog_delete(bd);
}

/* color button */
static void set_color(GtkWidget *b, BackdropDialog *bd)
{
    GdkColor color;
    GtkColorSelectionDialog *dialog;
    GtkColorSelection *sel;
    GdkPixbuf *pixbuf;
    guint32 rgba;

    if (!is_running)
	return;
    
    dialog = GTK_COLOR_SELECTION_DIALOG(gtk_widget_get_toplevel(b));

    sel = GTK_COLOR_SELECTION(dialog->colorsel);

    gtk_color_selection_get_current_color(sel, &color);

    backdrop_color.red = color.red;
    backdrop_color.green = color.green;
    backdrop_color.blue = color.blue;
    bd->color = color;
    
    update_color(bd);
    
    pixbuf = gtk_image_get_pixbuf(GTK_IMAGE(bd->color_box));
    rgba = (((color.red & 0xff00) << 8) | ((color.green & 0xff00)) | ((color.blue & 0xff00) >> 8)) << 8;
    gdk_pixbuf_fill(pixbuf, rgba);
}

static void color_picker(GtkWidget *b, BackdropDialog *bd)
{
    static GtkWidget *dialog = NULL;
    GtkWidget *button, *sel;

    if (dialog)
    {
	gtk_window_present(GTK_WINDOW(dialog));
	return;
    }
    
    dialog = gtk_color_selection_dialog_new(_("Select background color"));
    g_object_add_weak_pointer(G_OBJECT(dialog), (gpointer)&dialog);

    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);

    button = GTK_COLOR_SELECTION_DIALOG(dialog)->ok_button;
    g_signal_connect(button, "clicked", G_CALLBACK(set_color), bd);
    g_signal_connect_swapped(button, "clicked", 
	    		     G_CALLBACK(gtk_widget_destroy), dialog);

    button = GTK_COLOR_SELECTION_DIALOG(dialog)->cancel_button;
    g_signal_connect_swapped(button, "clicked", 
	    		     G_CALLBACK(gtk_widget_destroy), dialog);

    sel = GTK_COLOR_SELECTION_DIALOG(dialog)->colorsel;
    
    gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(sel), &(bd->color));
    gtk_widget_show(dialog);
}

static void showimage_toggle(GtkWidget *b, BackdropDialog *bd)
{
    showimage = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b)) ? 0 : 1;

    update_showimage(bd);
}

static void add_color_button(GtkWidget *vbox, BackdropDialog *bd)
{
    GtkWidget *hbox, *label, *frame, *button;
    GtkSizeGroup *sg;
    GdkPixbuf *pixbuf;
    GdkColor color;
    guint32 rgba;

    color.red = backdrop_color.red;
    color.green = backdrop_color.green;
    color.blue = backdrop_color.blue;
    bd->color = color;
    
    sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
    
    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

    label = gtk_label_new(_("Background color:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_size_group_add_widget(sg, label);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);

    button = gtk_button_new();
    gtk_widget_show(button);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, TRUE, 0);
    
    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_container_set_border_width(GTK_CONTAINER(frame), 0);
    gtk_widget_show(frame);
    gtk_container_add(GTK_CONTAINER(button), frame);

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 40, 16);
    rgba = (((color.red & 0xff00) << 8) | ((color.green & 0xff00)) | ((color.blue & 0xff00) >> 8)) << 8;
    gdk_pixbuf_fill(pixbuf, rgba);

    bd->color_box = gtk_image_new_from_pixbuf(pixbuf);
    gtk_widget_show(bd->color_box);
    gtk_container_add(GTK_CONTAINER(frame), bd->color_box);

    g_signal_connect(button, "clicked", G_CALLBACK(color_picker), bd);
    
    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

    label = gtk_label_new(_("Use color only:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_size_group_add_widget(sg, label);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);

    bd->color_only_checkbox = gtk_check_button_new();
    gtk_widget_show(bd->color_only_checkbox);
    gtk_box_pack_start(GTK_BOX(hbox), bd->color_only_checkbox, FALSE, TRUE, 0);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bd->color_only_checkbox), 
	    			 showimage == 0);
    
    g_signal_connect(bd->color_only_checkbox, "toggled", 
	    	     G_CALLBACK(showimage_toggle), bd);
    
    g_object_unref(sg);
}

/* dnd box */
void
on_drag_data_received(GtkWidget * w, GdkDragContext * context,
                      int x, int y, GtkSelectionData * data,
                      guint info, guint time, BackdropDialog * bd)
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

        g_free(backdrop_path);
        backdrop_path = g_strdup(file);

	gtk_entry_set_text(GTK_ENTRY(bd->file_entry), backdrop_path);
	gtk_editable_set_position(GTK_EDITABLE(bd->file_entry), -1);

	update_path(bd);
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

static void set_dnd_dest(BackdropDialog * bd)
{
    /* file entry */
    gtk_drag_dest_set(bd->file_entry, GTK_DEST_DEFAULT_ALL,
                      target_table, G_N_ELEMENTS(target_table),
                      GDK_ACTION_COPY | GDK_ACTION_MOVE);

    g_signal_connect(bd->file_entry, "drag_data_received",
                     G_CALLBACK(on_drag_data_received), bd);

    /* dialog window */
    gtk_drag_dest_set(bd->dialog, GTK_DEST_DEFAULT_ALL,
                      target_table, G_N_ELEMENTS(target_table),
                      GDK_ACTION_COPY | GDK_ACTION_MOVE);

    g_signal_connect(bd->dialog, "drag_data_received",
                     G_CALLBACK(on_drag_data_received), bd);
}

/* file entry */
static gboolean file_entry_lost_focus(GtkWidget *entry, GdkEventFocus *ev, 
				  BackdropDialog *bd)
{
    /* always return FALSE to let event propagate */
    const char *file;

    file = gtk_entry_get_text(GTK_ENTRY(entry));

    if (backdrop_path && strcmp(file, backdrop_path) != 0)
    {
	g_free(backdrop_path);
	backdrop_path = (file ? g_strdup(file) : NULL);

	update_path(bd);
    }

    return FALSE;
}

static void add_file_entry(GtkWidget *vbox, GtkSizeGroup *sg, 
			   BackdropDialog *bd)
{
    GtkWidget *hbox, *label;
    
    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("File:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_widget_show(label);
    gtk_size_group_add_widget(sg, label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    bd->file_entry = gtk_entry_new();
    if(backdrop_path)
    {
        gtk_entry_set_text(GTK_ENTRY(bd->file_entry), backdrop_path);
	gtk_editable_set_position(GTK_EDITABLE(bd->file_entry), -1);
    }
    
    gtk_widget_show(bd->file_entry);
    gtk_box_pack_start(GTK_BOX(hbox), bd->file_entry, TRUE, TRUE, 0);

    g_signal_connect(bd->file_entry, "focus-out-event",
                     G_CALLBACK(file_entry_lost_focus), bd);
}

/* button box */
/* file selection with optional image preview */
static void fs_ok_cb (GtkWidget *b, BackdropDialog *bd)
{
    GtkFileSelection * fs;
    const char *path;

    if (!is_running)
	return;

    fs = GTK_FILE_SELECTION(gtk_widget_get_toplevel(b));

    path = gtk_file_selection_get_filename(fs);
    
    if (path)
    {
	g_free(backdrop_path);
	backdrop_path = g_strdup(path);

	update_path(bd);
	
	gtk_entry_set_text(GTK_ENTRY(bd->file_entry), path);
	gtk_editable_set_position(GTK_EDITABLE(bd->file_entry), -1);
    }

    gtk_widget_destroy(GTK_WIDGET(fs));
}

static void
browse_cb (GtkWidget * b, BackdropDialog * bd)
{
    static GtkFileSelection *fs = NULL;
    char *title;
    
    if (fs)
	gtk_window_present(GTK_WINDOW(fs));
    
    title = _("Select backdrop image or list file");
    fs = GTK_FILE_SELECTION(preview_file_selection_new (title, TRUE));
    
    gtk_file_selection_hide_fileop_buttons(fs);
    
    if (backdrop_path)
    {
	gtk_file_selection_set_filename(fs, backdrop_path);
    }
    else
    {
	char *dir = g_build_filename(DATADIR, "xfce4", "backdrops/", NULL);
	gtk_file_selection_set_filename(fs, dir);
	g_free(dir);
    }
    
    gtk_window_set_transient_for (GTK_WINDOW (fs), GTK_WINDOW (bd->dialog));

    g_signal_connect (fs->ok_button, "clicked", G_CALLBACK (fs_ok_cb), bd);

    g_signal_connect_swapped (fs->cancel_button, "clicked", 
	    		      G_CALLBACK (gtk_widget_destroy), fs);

    g_signal_connect(fs, "delete-event", G_CALLBACK (gtk_widget_destroy), fs);

    g_object_add_weak_pointer(G_OBJECT(fs), (gpointer *)&fs);

    gtk_widget_show(GTK_WIDGET(fs));
}

static void set_path_cb(const char *path, BackdropDialog *bd)
{
    if (!is_running)
	return;

    g_free(backdrop_path);

    /* if the path stays the same, the setting will not update */
    backdrop_path = "";
    update_path(bd);
    gdk_flush();
    
    backdrop_path = g_strdup(path);

    update_path(bd);
    
    gtk_entry_set_text(GTK_ENTRY(bd->file_entry), path);
    gtk_editable_set_position(GTK_EDITABLE(bd->file_entry), -1);
}

static void edit_list_cb(GtkWidget *w, BackdropDialog *bd)
{
    edit_list_file(backdrop_path, bd->dialog, (ListMgrCb) set_path_cb, 
	           (gpointer)bd);
}

void new_list_cb(GtkWidget *w, BackdropDialog *bd)
{
    create_list_file(bd->dialog, (ListMgrCb)set_path_cb, (gpointer)bd);
}

static void add_button_box(GtkWidget *vbox, GtkSizeGroup *sg, 
			   BackdropDialog *bd)
{
    GtkWidget *hbox, *align, *browse_button, *new_list_button;
    
    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    align = gtk_alignment_new(0, 0, 0, 0);
    gtk_size_group_add_widget(sg, align);
    gtk_widget_show(align);
    gtk_box_pack_start(GTK_BOX(hbox), align, FALSE, FALSE, 0);

    browse_button = gtk_button_new_with_mnemonic("_Browse...");
    gtk_widget_show(browse_button);
    gtk_box_pack_start(GTK_BOX(hbox), browse_button, FALSE, FALSE, 0);
    
    new_list_button = gtk_button_new_with_label(_("New list"));
    gtk_widget_show(new_list_button);
    gtk_box_pack_start(GTK_BOX(hbox), new_list_button, FALSE, FALSE, 0);

    bd->edit_list_button = gtk_button_new_with_label(_("Edit list"));
    gtk_widget_show(bd->edit_list_button);
    gtk_box_pack_start(GTK_BOX(hbox), bd->edit_list_button, FALSE, FALSE, 0);

    g_signal_connect(G_OBJECT(browse_button), "clicked",
                     G_CALLBACK(browse_cb), bd);

    g_signal_connect(G_OBJECT(new_list_button), "clicked",
                     G_CALLBACK(new_list_cb), bd);

    g_signal_connect(G_OBJECT(bd->edit_list_button), "clicked",
                     G_CALLBACK(edit_list_cb), bd);

    if(!backdrop_path || !is_backdrop_list(backdrop_path))
        gtk_widget_set_sensitive(bd->edit_list_button, FALSE);
}

/* style options */
static void set_tiled(BackdropDialog *bd)
{
    backdrop_style = TILED;
    update_style(bd);
}

static void set_scaled(BackdropDialog *bd)
{
    backdrop_style = SCALED;
    update_style(bd);
}

static void set_centered(BackdropDialog *bd)
{
    backdrop_style = CENTERED;
    update_style(bd);
}

static void set_auto(BackdropDialog *bd)
{
    backdrop_style = AUTO;
    update_style(bd);
}

static void add_style_options(GtkWidget *vbox, GtkSizeGroup *sg, 
			      BackdropDialog *bd)
{
    GtkWidget *hbox, *label, *rb_tiled, *rb_scaled, *rb_centered, *rb_auto;
    GtkRadioButton *rb;
    
    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new ("Style:");
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
    gtk_size_group_add_widget(sg, label);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    rb_tiled = gtk_radio_button_new_with_mnemonic (NULL, "_Tiled");
    gtk_widget_show(rb_tiled);
    gtk_box_pack_start (GTK_BOX (hbox), rb_tiled, FALSE, FALSE, 0);
    
    rb = GTK_RADIO_BUTTON(rb_tiled);
    
    rb_scaled = gtk_radio_button_new_with_mnemonic_from_widget(rb, "_Scaled");
    gtk_widget_show(rb_scaled);
    gtk_box_pack_start (GTK_BOX (hbox), rb_scaled, FALSE, FALSE, 0);

    rb_centered = 
	gtk_radio_button_new_with_mnemonic_from_widget(rb, "_Centered");
    gtk_widget_show(rb_centered);
    gtk_box_pack_start (GTK_BOX (hbox), rb_centered, FALSE, FALSE, 0);

    rb_auto = gtk_radio_button_new_with_mnemonic_from_widget(rb, "_Automatic");
    gtk_widget_show(rb_auto);
    gtk_box_pack_start (GTK_BOX (hbox), rb_auto, FALSE, FALSE, 0);

    bd->style_rb_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(rb_tiled));
    
    g_signal_connect_swapped(rb_tiled, "toggled", 
	    		     G_CALLBACK(set_tiled), bd);
    g_signal_connect_swapped(rb_scaled, "toggled", 
	    		     G_CALLBACK(set_scaled), bd);
    g_signal_connect_swapped(rb_centered, "toggled", 
	    		     G_CALLBACK(set_centered), bd);
    g_signal_connect_swapped(rb_auto, "toggled", 
	    		     G_CALLBACK(set_auto), bd);
    
    switch (backdrop_style)
    {
	case TILED:
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb_tiled), TRUE);
	    break;
	case SCALED:
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb_scaled), TRUE);
	    break;
	case CENTERED:
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb_centered), TRUE);
	    break;
	default:
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb_auto), TRUE);
    }
}

/* the dialog */
static GtkWidget *create_backdrop_dialog(McsPlugin * mcs_plugin)
{
    GtkWidget *mainvbox, *vbox, *header;
    GtkSizeGroup *sg;
    BackdropDialog *bd;

    bd = g_new0(BackdropDialog, 1);
    bd->plugin = mcs_plugin;

    /* the dialog */
    bd->dialog = gtk_dialog_new_with_buttons(_("Backdrop preferences"), NULL,
                                             GTK_DIALOG_NO_SEPARATOR, 
					     GTK_STOCK_CLOSE,
					     GTK_RESPONSE_OK,
					     NULL);

    gtk_window_set_position(GTK_WINDOW(bd->dialog), GTK_WIN_POS_CENTER);

    g_signal_connect(bd->dialog, "response", G_CALLBACK(dialog_response), bd);
    g_signal_connect_swapped(bd->dialog, "delete_event",
                             G_CALLBACK(dialog_delete), bd);

    mainvbox = GTK_DIALOG(bd->dialog)->vbox;
    
    /* header */
    header = create_header(bd->plugin->icon, _("Background settings"));
    gtk_box_pack_start(GTK_BOX(mainvbox), header, FALSE, TRUE, 0);

    /* color vbox */
    vbox = gtk_vbox_new(FALSE, BORDER);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), BORDER);
    gtk_box_pack_start(GTK_BOX(mainvbox), vbox, TRUE, TRUE, 0);
    gtk_widget_show(vbox);

    // add_sub_header(vbox, _("Color"));
    
    add_color_button(vbox, bd);

    add_spacer(GTK_BOX(mainvbox));

    /* image vbox */
    vbox = gtk_vbox_new(FALSE, BORDER);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), BORDER);
    gtk_box_pack_start(GTK_BOX(mainvbox), vbox, TRUE, TRUE, 0);
    gtk_widget_show(vbox);

    add_sub_header(vbox, _("Image"));
    
    /* file entry and style radio buttons */
    sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

    add_file_entry(vbox, sg, bd);
    add_button_box(vbox, sg, bd);
    add_style_options(vbox, sg, bd);

    add_spacer(GTK_BOX(mainvbox));

    set_dnd_dest(bd);

    return bd->dialog;
}

static void run_dialog(McsPlugin * mcs_plugin)
{
    static GtkWidget *dialog = NULL;

    if(is_running)
    {
        gtk_window_present(GTK_WINDOW(dialog));
	return;
    }

    dialog = create_backdrop_dialog(mcs_plugin);
    is_running = TRUE;

    gtk_widget_show(dialog);
}

/* macro defined in manager-plugin.h */
MCS_PLUGIN_CHECK_INIT
