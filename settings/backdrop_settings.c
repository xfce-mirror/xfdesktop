/*  xfce4
 *  
 *  Copyright (C) 2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *  Copyright (C) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
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

#include <X11/Xlib.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libxfce4util/i18n.h>
#include <libxfce4util/util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <xfce-mcs-manager/manager-plugin.h>

#include "backdrop-common.h"
#include "backdrop-icon.h"
#include "backdrop-mgr.h"
#include "settings_common.h"

#define RCFILE "backdrop.xml"
#define PLUGIN_NAME "backdrop"

#define DEFAULT_ICON_SIZE 48

#ifdef HAVE_GDK_PIXBUF_NEW_FROM_STREAM
#define gdk_pixbuf_new_from_inline gdk_pixbuf_new_from_stream
#endif

#define DEFAULT_BACKDROP (DATADIR "/xfce4/backdrops/xfce-stripes.png")

/* important stuff to keep track of */
typedef struct {
    McsPlugin *plugin;

    /* options dialog */
    GtkWidget *dialog;
	GtkWidget *top_notebook;
	GtkWidget *screens_notebook;
} BackdropDialog;

typedef struct {
	/* which screen this panel is for */
	gint xscreen;
	
	/* the settings themselves */
	gboolean set_backdrop;
	gchar *image_path;
	XfceBackdropStyle style;
	gboolean color_only;
	McsColor color1;
	McsColor color2;
	XfceColorStyle color_style;
	
	/* the panel's GUI controls */
	GtkWidget *color_frame;
	GtkWidget *color_style_combo;
	GtkWidget *color1_box;
	GtkWidget *color2_hbox;
	GtkWidget *color2_box;
	GtkWidget *color_only_chk;
	
	GtkWidget *image_frame;
	GtkWidget *file_entry;
	GtkWidget *edit_list_button;
	GtkWidget *style_combo;
	
	GtkWidget *set_backdrop_chk;
	
	/* backreference */
	BackdropDialog *bd;
} BackdropPanel;

/* there can be only one */
static gboolean is_running = FALSE;

static GList *screens;  /* list of BackdropPanels */

static int set_background = 1;
static char *backdrop_path = NULL;
static int backdrop_style = STRETCHED;
static int showimage = 1;
static McsColor backdrop_color;

static void backdrop_create_channel (McsPlugin * mcs_plugin);
static gboolean backdrop_write_options (McsPlugin * mcs_plugin);
static void run_dialog (McsPlugin * mcs_plugin);

static void
dlg_response_accept(GtkWidget *w, gpointer user_data)
{
	GtkDialog *dialog = GTK_DIALOG(user_data);
	gtk_dialog_response(dialog, GTK_RESPONSE_ACCEPT);
}

static void
dlg_response_cancel(GtkWidget *w, gpointer user_data)
{
	GtkDialog *dialog = GTK_DIALOG(user_data);
	gtk_dialog_response(dialog, GTK_RESPONSE_CANCEL);
}


static GdkPixbuf *
backdrop_icon_at_size (int width, int height)
{
    GdkPixbuf *base;

    base = gdk_pixbuf_new_from_inline (-1, backdrop_icon_data, FALSE, NULL);

    g_assert (base);

    if ((width <= 0 || height <= 0))
    {
	return base;
    }
    else
    {
	GdkPixbuf *scaled;

	scaled = gdk_pixbuf_scale_simple (base, width, height,
					  GDK_INTERP_BILINEAR);

	g_object_unref (G_OBJECT (base));

	return scaled;
    }
}

McsPluginInitResult
mcs_plugin_init (McsPlugin * mcs_plugin)
{
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    mcs_plugin->plugin_name = g_strdup (PLUGIN_NAME);
    mcs_plugin->caption = g_strdup (_("Desktop"));
    mcs_plugin->run_dialog = run_dialog;

    mcs_plugin->icon = backdrop_icon_at_size (DEFAULT_ICON_SIZE,
					      DEFAULT_ICON_SIZE);

    backdrop_create_channel (mcs_plugin);

    return (MCS_PLUGIN_INIT_OK);
}

static void
backdrop_create_channel (McsPlugin * mcs_plugin)
{
    McsSetting *setting;
    gchar *rcfile;
	gint i, nscreens;
	gchar setting_name[128];

    rcfile = xfce_get_userfile ("settings", RCFILE, NULL);
    mcs_manager_add_channel_from_file (mcs_plugin->manager, BACKDROP_CHANNEL,
				       rcfile);
    g_free (rcfile);

	nscreens = gdk_display_get_n_screens(gdk_display_get_default());
	for(i = 0; i < nscreens; i++) {
		BackdropPanel *bp = g_new0(BackdropPanel, 1);
		
		/* whether or not to set the backdrop */
		g_snprintf(setting_name, 128, "setbackdrop_%d", i);
		setting = mcs_manager_setting_lookup(mcs_plugin->manager, setting_name,
				BACKDROP_CHANNEL);
		if(setting)
			bp->set_backdrop = setting->data.v_int == 0 ? FALSE : TRUE;
		else {
			DBG ("no setbackground settings");
			mcs_manager_set_int(mcs_plugin->manager, setting_name,
					BACKDROP_CHANNEL, 1);
		}
		
		/* the path to an image file */
		g_snprintf(setting_name, 128, "path_%d", i);
		setting = mcs_manager_setting_lookup(mcs_plugin->manager, setting_name,
				BACKDROP_CHANNEL);
		if(setting)
			bp->image_path = g_strdup(setting->data.v_string);
		else {
			bp->image_path = g_strdup(DEFAULT_BACKDROP);
			mcs_manager_set_string(mcs_plugin->manager, setting_name,
					BACKDROP_CHANNEL, bp->image_path);
		}
		
		/* the backdrop image style */
		g_snprintf(setting_name, 128, "style_%d", i);
		setting = mcs_manager_setting_lookup(mcs_plugin->manager, setting_name,
				BACKDROP_CHANNEL);
		if(setting)
			bp->style = setting->data.v_int;
		else
			mcs_manager_set_int(mcs_plugin->manager, setting_name,
					BACKDROP_CHANNEL, AUTO);
		
		/* color 1 */
		g_snprintf(setting_name, 128, "color1_%d", i);
		setting = mcs_manager_setting_lookup(mcs_plugin->manager, setting_name,
				BACKDROP_CHANNEL);
		if(setting) {
			bp->color1.red = setting->data.v_color.red;
			bp->color1.green = setting->data.v_color.green;
			bp->color1.blue = setting->data.v_color.blue;
			bp->color1.alpha = setting->data.v_color.alpha;
		} else {
			/* Just a color by default #6985b7 - That number looks cool :) */
			bp->color1.red = (guint16)0x6900;
			bp->color1.green = (guint16)0x8500;
			bp->color1.blue = (guint16)0xB700;
			bp->color1.alpha = (guint16)0;
			mcs_manager_set_color(mcs_plugin->manager, setting_name,
					BACKDROP_CHANNEL, &bp->color1);
		}
		
		/* color 2 */
		g_snprintf(setting_name, 128, "color2_%d", i);
		setting = mcs_manager_setting_lookup(mcs_plugin->manager, setting_name,
				BACKDROP_CHANNEL);
		if(setting) {
			bp->color2.red = setting->data.v_color.red;
			bp->color2.green = setting->data.v_color.green;
			bp->color2.blue = setting->data.v_color.blue;
			bp->color2.alpha = setting->data.v_color.alpha;
		} else {
			/* Just a color by default #6985b7 - That number looks cool :) */
			bp->color2.red = (guint16)0x6900;
			bp->color2.green = (guint16)0x8500;
			bp->color2.blue = (guint16)0xB700;
			bp->color2.alpha = (guint16)0;
			mcs_manager_set_color(mcs_plugin->manager, setting_name,
					BACKDROP_CHANNEL, &bp->color2);
		}
		
		g_snprintf(setting_name, 128, "coloronly_%d", i);
		setting = mcs_manager_setting_lookup(mcs_plugin->manager, setting_name,
				BACKDROP_CHANNEL);
		if(setting)
			bp->color_only = setting->data.v_int == 0 ? FALSE : TRUE;
		else
			mcs_manager_set_int(mcs_plugin->manager, setting_name,
					BACKDROP_CHANNEL, 1);
		
		/* the color style */
		g_snprintf(setting_name, 128, "colorstyle_%d", i);
		setting = mcs_manager_setting_lookup(mcs_plugin->manager, setting_name,
				BACKDROP_CHANNEL);
		if(setting)
			bp->color_style = setting->data.v_int;
		else
			mcs_manager_set_int(mcs_plugin->manager, setting_name,
					BACKDROP_CHANNEL, SOLID_COLOR);
		
		screens = g_list_append(screens, bp);
	}

	mcs_manager_notify(mcs_plugin->manager, BACKDROP_CHANNEL);
}

static gboolean
backdrop_write_options (McsPlugin * mcs_plugin)
{
    gchar *rcfile;
    gboolean result;

    rcfile = xfce_get_userfile ("settings", RCFILE, NULL);
    result = mcs_manager_save_channel_to_file (mcs_plugin->manager,
					       BACKDROP_CHANNEL, rcfile);
    g_free (rcfile);

    return result;
}

/* something changed */
static void
update_path (BackdropPanel *bp)
{
	gchar setting_name[128];
	
    if (is_backdrop_list (bp->image_path))
    {
	gtk_widget_set_sensitive (bp->edit_list_button, TRUE);

	/* set style to AUTOMATIC and set insensitive
	   gtk_option_menu_set_history(GTK_OPTION_MENU(bd->style_om), AUTO);
	   gtk_widget_set_sensitive(bd->style_om, FALSE); */
    }
    else
    {
	gtk_widget_set_sensitive (bp->edit_list_button, FALSE);

	gtk_widget_set_sensitive (bp->style_combo, TRUE);
    }

    if (bp->image_path)
    {
		g_snprintf(setting_name, 128, "path_%d", bp->xscreen);
	mcs_manager_set_string (bp->bd->plugin->manager, setting_name, BACKDROP_CHANNEL,
				bp->image_path);
	/* Assume that if the user has changed the image path (s)he actually
	   wants to see it on screen, so unselect the color only checkbox
	 */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				      (bp->color_only_chk), FALSE);
    }

    mcs_manager_notify (bp->bd->plugin->manager, BACKDROP_CHANNEL);
}

static void
color_picker(GtkWidget *b, BackdropPanel *bp)
{
	GtkWidget *dialog;
	GtkWidget *button, *sel;
	GdkPixbuf *pixbuf;
	GdkColor color;
	McsColor *thecolor;
	GtkWidget *thecolorbox;
	guint32 rgba;
	gint whichcolor;
	gchar setting_name[128];
	
	whichcolor = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "xfce-colornum"));
	if(whichcolor == 1) {
		thecolor = &bp->color1;
		thecolorbox = bp->color1_box;
	} else {
		thecolor = &bp->color2;
		thecolorbox = bp->color2_box;
	}
	
	dialog = gtk_color_selection_dialog_new(_("Select backdrop color"));
	
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
	
	button = GTK_COLOR_SELECTION_DIALOG(dialog)->ok_button;
	g_signal_connect(button, "clicked", G_CALLBACK(dlg_response_accept), dialog);
	
	button = GTK_COLOR_SELECTION_DIALOG (dialog)->cancel_button;
	g_signal_connect(button, "clicked", G_CALLBACK (dlg_response_cancel), dialog);
	
	sel = GTK_COLOR_SELECTION_DIALOG(dialog)->colorsel;
	color.red = thecolor->red;
	color.green = thecolor->green;
	color.blue = thecolor->blue;
	gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(sel), &color);
	
	gtk_widget_show(dialog);
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gtk_color_selection_get_current_color(GTK_COLOR_SELECTION(sel), &color);

		thecolor->red = color.red;
		thecolor->green = color.green;
		thecolor->blue = color.blue;
	
		g_snprintf(setting_name, 128, "color%d_%d", whichcolor, bp->xscreen);
		mcs_manager_set_color(bp->bd->plugin->manager, setting_name, BACKDROP_CHANNEL,
				   thecolor);
		mcs_manager_notify(bp->bd->plugin->manager, BACKDROP_CHANNEL);
	
		pixbuf = gtk_image_get_pixbuf(GTK_IMAGE(thecolorbox));
		rgba =
		(((color.red & 0xff00) << 8) | ((color.green & 0xff00)) | ((color.
										blue &
										0xff00) >>
									   8)) << 8;
		gdk_pixbuf_fill(pixbuf, rgba);
	}
	gtk_widget_destroy(dialog);
}

static void
showimage_toggle(GtkWidget *b, BackdropPanel *bp)
{
	gchar setting_name[128];
	
    bp->color_only = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b));

    gtk_widget_set_sensitive(bp->image_frame, !bp->color_only);

	g_snprintf(setting_name, 128, "coloronly_%d", bp->xscreen);
    mcs_manager_set_int (bp->bd->plugin->manager, setting_name, BACKDROP_CHANNEL,
			 bp->color_only ? 1 : 0);
    mcs_manager_notify (bp->bd->plugin->manager, BACKDROP_CHANNEL);
}

static GtkWidget *
color_button_new(McsColor *color, GtkWidget **color_box)
{
    GtkWidget *button, *frame;
    GdkPixbuf *pixbuf;
    guint32 rgba;

    button = gtk_button_new();

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 0);
    gtk_widget_show (frame);
    gtk_container_add (GTK_CONTAINER (button), frame);

    pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 40, 16);
    rgba = (((color->red & 0xff00) << 8) | ((color->green & 0xff00))
			| ((color->blue & 0xff00) >> 8)) << 8;
    gdk_pixbuf_fill (pixbuf, rgba);

    *color_box = gtk_image_new_from_pixbuf (pixbuf);
    gtk_widget_show (*color_box);
    gtk_container_add (GTK_CONTAINER (frame), *color_box);

	return button;
}

/* dnd box */
void
on_drag_data_received (GtkWidget * w, GdkDragContext * context,
		       int x, int y, GtkSelectionData * data,
		       guint info, guint time, BackdropPanel *bp)
{
    char buf[1024];
    char *file = NULL;
    char *end;

    /* copy data to buffer */
    strncpy (buf, (char *) data->data, 1023);
    buf[1023] = '\0';

    if ((end = strchr (buf, '\n')))
	*end = '\0';

    if ((end = strchr (buf, '\r')))
	*end = '\0';

    if (buf[0])
    {
	file = buf;

	if (strncmp ("file:", file, 5) == 0)
	{
	    file += 5;

	    if (strncmp ("///", file, 3) == 0)
		file += 2;
	}

	if(bp->image_path)
		g_free (bp->image_path);
	bp->image_path = g_strdup (file);

	gtk_entry_set_text (GTK_ENTRY (bp->file_entry), bp->image_path);
	gtk_editable_set_position (GTK_EDITABLE (bp->file_entry), -1);

	update_path (bp);
    }

    gtk_drag_finish (context, (file != NULL),
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

static void
set_dnd_dest (BackdropPanel * bp)
{
    /* file entry */
    gtk_drag_dest_set (bp->file_entry, GTK_DEST_DEFAULT_ALL,
		       target_table, G_N_ELEMENTS (target_table),
		       GDK_ACTION_COPY | GDK_ACTION_MOVE);

    g_signal_connect (bp->file_entry, "drag_data_received",
		      G_CALLBACK (on_drag_data_received), bp);

	/* this isn't a good idea anymore... perhaps set it to the page? */
    /* dialog window */
    /*gtk_drag_dest_set (bd->dialog, GTK_DEST_DEFAULT_ALL,
		       target_table, G_N_ELEMENTS (target_table),
		       GDK_ACTION_COPY | GDK_ACTION_MOVE);

    g_signal_connect (bd->dialog, "drag_data_received",
		      G_CALLBACK (on_drag_data_received), bd);
	*/
}

/* file entry */
static gboolean
file_entry_lost_focus (GtkWidget * entry, GdkEventFocus * ev,
		       BackdropPanel *bp)
{
    /* always return FALSE to let event propagate */
    const char *file;

    file = gtk_entry_get_text (GTK_ENTRY (entry));

    if (bp->image_path && strcmp (file, bp->image_path) != 0)
    {
	g_free (bp->image_path);
	bp->image_path = (file ? g_strdup (file) : NULL);

	update_path (bp);
    }

    return FALSE;
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
browse_cb(GtkWidget *b, BackdropPanel *bp)
{
	GtkWidget *chooser, *preview;
	GtkFileFilter *filter;
	
	chooser = xfce_file_chooser_dialog_new(_("Select backdrop image or list file"),
			GTK_WINDOW(bp->bd->dialog), XFCE_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL,
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
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("List Files (*.list)"));
	gtk_file_filter_add_pattern(filter, "*.list");
	xfce_file_chooser_add_filter(XFCE_FILE_CHOOSER(chooser), filter);
	
	xfce_file_chooser_set_local_only(XFCE_FILE_CHOOSER(chooser), TRUE);
	xfce_file_chooser_add_shortcut_folder(XFCE_FILE_CHOOSER(chooser),
			DATADIR "/xfce4/backdrops", NULL);
	
	if(bp->image_path) {
		gchar *tmppath = g_strdup(bp->image_path);
		gchar *p = g_strrstr(tmppath, "/");
		if(p && p != tmppath)
			*p = 0;
		xfce_file_chooser_set_current_folder(XFCE_FILE_CHOOSER(chooser), tmppath);
		g_free(tmppath);
	}
	
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
			if(bp->image_path)
				g_free(bp->image_path);
			bp->image_path = filename;
			update_path(bp);
			gtk_entry_set_text(GTK_ENTRY(bp->file_entry), filename);
			gtk_editable_set_position(GTK_EDITABLE(bp->file_entry), -1);
		}
	}
	gtk_widget_destroy(chooser);
}

static void
set_path_cb(const char *path, BackdropPanel *bp)
{
	if(!is_running)
		return;
	
	if(bp->image_path)
		g_free(bp->image_path);
	
	/* if the path stays the same, the setting will not update */
	bp->image_path = "";
	update_path(bp);
	gdk_flush();
	
	bp->image_path = g_strdup(path);
	
	update_path(bp);
	
	gtk_entry_set_text(GTK_ENTRY(bp->file_entry), path);
	gtk_editable_set_position(GTK_EDITABLE(bp->file_entry), -1);
}

static void
edit_list_cb(GtkWidget *w, BackdropPanel *bp)
{
	edit_list_file(bp->image_path, bp->bd->dialog, (ListMgrCb)set_path_cb, (gpointer)bp);
}

void
new_list_cb(GtkWidget *w, BackdropPanel *bp)
{
	create_list_file(bp->bd->dialog, (ListMgrCb)set_path_cb, (gpointer)bp);
}

static void
add_button_box (GtkWidget *vbox, BackdropPanel *bp)
{
    GtkWidget *hbox, *align, *new_list_button;

    hbox = gtk_hbox_new (FALSE, BORDER);
    gtk_widget_show (hbox);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	bp->edit_list_button = gtk_button_new_with_mnemonic(_("_Edit list"));
    gtk_widget_show (bp->edit_list_button);
    gtk_box_pack_end (GTK_BOX (hbox), bp->edit_list_button, FALSE, FALSE,
			0);

    g_signal_connect (G_OBJECT (bp->edit_list_button), "clicked",
		      G_CALLBACK (edit_list_cb), bp);
	
    new_list_button = gtk_button_new_with_mnemonic(_("_New list"));
    gtk_widget_show (new_list_button);
    gtk_box_pack_end (GTK_BOX (hbox), new_list_button, FALSE, FALSE, 0);
	
	g_signal_connect (G_OBJECT (new_list_button), "clicked",
		      G_CALLBACK (new_list_cb), bp);

    if (!bp->image_path || !is_backdrop_list (bp->image_path))
		gtk_widget_set_sensitive (bp->edit_list_button, FALSE);
}

/* style options */
static void
set_style(GtkWidget *item, BackdropPanel *bp)
{
	gchar setting_name[128];
	
    bp->style = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (item),
							  "user-data"));
	g_snprintf(setting_name, 128, "style_%d", bp->xscreen);
    mcs_manager_set_int (bp->bd->plugin->manager, setting_name, BACKDROP_CHANNEL,
			 bp->style);
    mcs_manager_notify (bp->bd->plugin->manager, BACKDROP_CHANNEL);
}

static void
add_style_options (GtkWidget *vbox, GtkSizeGroup * sg, BackdropPanel *bp)
{
    GtkWidget *hbox, *label;
    GtkWidget *rb_tiled, *rb_centered, *rb_scaled, *rb_stretched, *rb_auto;
    GtkWidget *menu, *omenu;

    hbox = gtk_hbox_new (FALSE, BORDER);
    gtk_widget_show (hbox);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	
	label = gtk_label_new_with_mnemonic(_("S_tyle:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_size_group_add_widget (sg, label);
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

    menu = gtk_menu_new ();

    rb_tiled = gtk_menu_item_new_with_label (_("Tiled"));
    g_object_set_data (G_OBJECT (rb_tiled), "user-data",
		       GUINT_TO_POINTER (TILED));
    gtk_widget_show (rb_tiled);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), rb_tiled);
    g_signal_connect (rb_tiled, "activate", G_CALLBACK (set_style), bp);

    rb_centered = gtk_menu_item_new_with_label (_("Centered"));
    g_object_set_data (G_OBJECT (rb_centered), "user-data",
		       GUINT_TO_POINTER (CENTERED));
    gtk_widget_show (rb_centered);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), rb_centered);
    g_signal_connect (rb_centered, "activate", G_CALLBACK (set_style), bp);

    rb_scaled = gtk_menu_item_new_with_label (_("Scaled"));
    g_object_set_data (G_OBJECT (rb_scaled), "user-data",
		       GUINT_TO_POINTER (SCALED));
    gtk_widget_show (rb_scaled);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), rb_scaled);
    g_signal_connect (rb_scaled, "activate", G_CALLBACK (set_style), bp);

    rb_stretched = gtk_menu_item_new_with_label (_("Stretched"));
    g_object_set_data (G_OBJECT (rb_stretched), "user-data",
		       GUINT_TO_POINTER (STRETCHED));
    gtk_widget_show (rb_stretched);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), rb_stretched);
    g_signal_connect (rb_stretched, "activate", G_CALLBACK (set_style), bp);

    rb_auto = gtk_menu_item_new_with_label (_("Auto"));
    g_object_set_data (G_OBJECT (rb_auto), "user-data",
		       GUINT_TO_POINTER (AUTO));
    gtk_widget_show (rb_auto);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), rb_auto);
    g_signal_connect (rb_auto, "activate", G_CALLBACK (set_style), bp);

    bp->style_combo = omenu = gtk_option_menu_new ();
    gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
    gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), bp->style);
	
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);

    gtk_widget_show (menu);
    gtk_widget_show (omenu);
    gtk_box_pack_start (GTK_BOX (hbox), omenu, FALSE, FALSE, 6);
}

static void
set_color_style(GtkWidget *item, BackdropPanel *bp)
{
	gchar setting_name[128];
	
    bp->color_style = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (item),
							  "user-data"));
	
	if(bp->color_style == SOLID_COLOR)
		gtk_widget_set_sensitive(bp->color2_hbox, FALSE);
	else
		gtk_widget_set_sensitive(bp->color2_hbox, TRUE);
	
	g_snprintf(setting_name, 128, "colorstyle_%d", bp->xscreen);
    mcs_manager_set_int (bp->bd->plugin->manager, setting_name, BACKDROP_CHANNEL,
			 bp->color_style);
    mcs_manager_notify (bp->bd->plugin->manager, BACKDROP_CHANNEL);
}

void
toggle_set_background(GtkToggleButton *tb, BackdropPanel *bp)
{
	gchar setting_name[128];
	
    bp->set_backdrop = !gtk_toggle_button_get_active(tb);

    gtk_widget_set_sensitive (bp->image_frame, bp->set_backdrop);
    gtk_widget_set_sensitive (bp->color_frame, bp->set_backdrop);

	g_snprintf(setting_name, 128, "setbackdrop_%d", bp->xscreen);
    mcs_manager_set_int(bp->bd->plugin->manager, setting_name,
			 BACKDROP_CHANNEL, bp->set_backdrop ? 1 : 0);
    mcs_manager_notify (bp->bd->plugin->manager, BACKDROP_CHANNEL);
}

/* the dialog */
static BackdropDialog *
create_backdrop_dialog (McsPlugin * mcs_plugin)
{
    GtkWidget *mainvbox, *frame, *vbox, *hbox, *header, *label, *table, *mi,
			*menu, *button, *image;
    GtkSizeGroup *sg;
    BackdropDialog *bd;
	gint i, nscreens;

    bd = g_new0(BackdropDialog, 1);
    bd->plugin = mcs_plugin;

    /* the dialog */
    bd->dialog = gtk_dialog_new_with_buttons (_("Desktop"), NULL,
					      GTK_DIALOG_NO_SEPARATOR,
					      GTK_STOCK_CLOSE,
					      GTK_RESPONSE_ACCEPT,
#ifndef NO_HELP_BUTTON
					      GTK_STOCK_HELP,
					      GTK_RESPONSE_HELP,
#endif
					      NULL);

	gtk_dialog_set_default_response(GTK_DIALOG(bd->dialog), GTK_RESPONSE_ACCEPT);
	
    mainvbox = GTK_DIALOG (bd->dialog)->vbox;

    /* header */
    header = xfce_create_header (bd->plugin->icon, _("Desktop Settings"));
    gtk_box_pack_start (GTK_BOX (mainvbox), header, FALSE, TRUE, 0);

    add_spacer (GTK_BOX (mainvbox));
	
	/* main notebook */
	bd->top_notebook = gtk_notebook_new();
	gtk_widget_show(bd->top_notebook);
	gtk_box_pack_start(GTK_BOX(mainvbox), bd->top_notebook, TRUE, TRUE, 0);
	
	/* screens notebook */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);
	add_spacer(GTK_BOX(vbox));
	
	bd->screens_notebook = gtk_notebook_new();
	gtk_widget_show(bd->screens_notebook);
	gtk_box_pack_start(GTK_BOX(vbox), bd->screens_notebook, FALSE, FALSE, 0);
	
	label = gtk_label_new(_("Backdrops"));
	gtk_widget_show(label);
	gtk_notebook_append_page(GTK_NOTEBOOK(bd->top_notebook), vbox, label);
	
	nscreens = gdk_display_get_n_screens(gdk_display_get_default());
	for(i = 0; i < nscreens; i++) {
		GtkWidget *page;
		gchar screen_label[256];
		BackdropPanel *bp = g_list_nth_data(screens, i);
		
		bp->bd = bd;
		
		if(!bp) {
			g_critical("Number of screens changed after plugin init!");
			break;
		}
		
		bp->xscreen = i;
		
		page = gtk_vbox_new(FALSE, BORDER);
		gtk_widget_show(page);
		
		add_spacer(GTK_BOX(page));
		
		/* color settings frame */
		
		bp->color_frame = frame = xfce_framebox_new(_("Color"), TRUE);
		gtk_widget_show(frame);
		gtk_box_pack_start(GTK_BOX(page), frame, FALSE, FALSE, 0);
		
		vbox = gtk_vbox_new(FALSE, BORDER);
		gtk_widget_show(vbox);
		xfce_framebox_add(XFCE_FRAMEBOX(frame), vbox);
		
		sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
		
		hbox = gtk_hbox_new(FALSE, BORDER);
		gtk_widget_show(hbox);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		
		label = gtk_label_new_with_mnemonic(_("_Color Style:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
		gtk_size_group_add_widget(sg, label);
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		
		/* create type combo */
		menu = gtk_menu_new();
		
		mi = gtk_menu_item_new_with_label(_("Solid Color"));
		g_object_set_data(G_OBJECT(mi), "user-data", GUINT_TO_POINTER(SOLID_COLOR));
		gtk_widget_show(mi);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
		g_signal_connect(G_OBJECT(mi), "activate", G_CALLBACK(set_color_style), bp);
		
		mi = gtk_menu_item_new_with_label(_("Horizontal Gradient"));
		g_object_set_data(G_OBJECT(mi), "user-data", GUINT_TO_POINTER(HORIZ_GRADIENT));
		gtk_widget_show(mi);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
		g_signal_connect(G_OBJECT(mi), "activate", G_CALLBACK(set_color_style), bp);
		
		mi = gtk_menu_item_new_with_label(_("Vertical Gradient"));
		g_object_set_data(G_OBJECT(mi), "user-data", GUINT_TO_POINTER(VERT_GRADIENT));
		gtk_widget_show(mi);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
		g_signal_connect(G_OBJECT(mi), "activate", G_CALLBACK(set_color_style), bp);
		
		bp->color_style_combo = gtk_option_menu_new();
		gtk_widget_show(bp->color_style_combo);
		gtk_option_menu_set_menu(GTK_OPTION_MENU(bp->color_style_combo), menu);
		gtk_option_menu_set_history(GTK_OPTION_MENU(bp->color_style_combo),
				bp->color_style);
		gtk_box_pack_start(GTK_BOX(hbox), bp->color_style_combo, FALSE, FALSE, 0);
		
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), bp->color_style_combo);
		
		/* first color */
		hbox = gtk_hbox_new(FALSE, BORDER);
		gtk_widget_show(hbox);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		
		label = gtk_label_new_with_mnemonic(_("Fi_rst Color:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
		gtk_size_group_add_widget(sg, label);
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		
		button = color_button_new(&bp->color1, &bp->color1_box);
		g_object_set_data(G_OBJECT(button), "xfce-colornum", GINT_TO_POINTER(1));
		gtk_widget_show(button);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		g_signal_connect(button, "clicked", G_CALLBACK(color_picker), bp);
		
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), button);
		
		/* second color */
		bp->color2_hbox = hbox = gtk_hbox_new(FALSE, BORDER);
		gtk_widget_show(hbox);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		
		label = gtk_label_new_with_mnemonic(_("_Second Color:"));
		gtk_size_group_add_widget(sg, label);
		gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		
		button = color_button_new(&bp->color2, &bp->color2_box);
		g_object_set_data(G_OBJECT(button), "xfce-colornum", GINT_TO_POINTER(2));
		gtk_widget_show(button);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		g_signal_connect(button, "clicked", G_CALLBACK(color_picker), bp);
		
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), button);
		
		if(bp->color_style == SOLID_COLOR)
			gtk_widget_set_sensitive(hbox, FALSE);
		
		/* set color only checkbox */
		bp->color_only_chk = gtk_check_button_new_with_mnemonic(_("Set color _only"));
		gtk_widget_show(bp->color_only_chk);
		gtk_box_pack_start(GTK_BOX(vbox), bp->color_only_chk, FALSE, FALSE, 0);
		g_signal_connect(G_OBJECT(bp->color_only_chk), "toggled",
				G_CALLBACK(showimage_toggle), bp);
		
		g_object_unref(G_OBJECT(sg));
		
		/* image settings frame */
		
		sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
		
		bp->image_frame = frame = xfce_framebox_new(_("Image"), TRUE);
		gtk_widget_show(frame);
		gtk_box_pack_start(GTK_BOX(page), frame, FALSE, FALSE, 0);
		
		vbox = gtk_vbox_new(FALSE, BORDER);
		gtk_widget_show(vbox);
		xfce_framebox_add(XFCE_FRAMEBOX(frame), vbox);
		
		/* filename box */
		hbox = gtk_hbox_new(FALSE, BORDER);
		gtk_widget_show(hbox);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		
		label = gtk_label_new_with_mnemonic(_("_File:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
		gtk_size_group_add_widget(sg, label);
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		
		/* file entry */
		bp->file_entry = gtk_entry_new();
		gtk_widget_show(bp->file_entry);
		if(bp->image_path)
			gtk_entry_set_text(GTK_ENTRY(bp->file_entry), bp->image_path);
		gtk_box_pack_start(GTK_BOX(hbox), bp->file_entry, TRUE, TRUE, 0);
		g_signal_connect(bp->file_entry, "focus-out-event",
				G_CALLBACK(file_entry_lost_focus), bp);
		
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), bp->file_entry);
		
		/* browse button */
		image = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
		gtk_widget_show(image);
		button = gtk_button_new();
		gtk_container_add(GTK_CONTAINER(button), image);
		gtk_widget_show(button);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(browse_cb), bp);
		
		/* image list buttons */
		add_button_box(vbox, bp);
		
		/* style listbox */
		add_style_options(vbox, sg, bp);
		
		g_object_unref(G_OBJECT(sg));
		
		/* don't set backdrop checkbox */
		bp->set_backdrop_chk = gtk_check_button_new_with_mnemonic(_("_Don't set backdrop"));
		gtk_widget_show(bp->set_backdrop_chk);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bp->set_backdrop_chk),
				!bp->set_backdrop);
		gtk_box_pack_start(GTK_BOX(page), bp->set_backdrop_chk, FALSE, FALSE, 0);
		g_signal_connect(G_OBJECT(bp->set_backdrop_chk), "toggled",
				G_CALLBACK(toggle_set_background), bp);
		toggle_set_background(GTK_TOGGLE_BUTTON(bp->set_backdrop_chk), bp);
		
		add_spacer(GTK_BOX(page));
		
		g_snprintf(screen_label, 256, _("Screen %d"), i);
		label = gtk_label_new(screen_label);
		gtk_notebook_append_page(GTK_NOTEBOOK(bd->screens_notebook), page, label);
		
		set_dnd_dest(bp);
	}
	
	/* menu page */
	
	vbox = gtk_vbox_new(FALSE, BORDER);
	gtk_widget_show(vbox);
	//create_menu_notebook(vbox);
	
	label = gtk_label_new(_("Menu"));
	gtk_widget_show(label);
	gtk_notebook_append_page(GTK_NOTEBOOK(bd->top_notebook), vbox, label);
	
	add_spacer(GTK_BOX(mainvbox));

    return bd;
}

static void
run_dialog (McsPlugin * mcs_plugin)
{
	static BackdropDialog *bd = NULL;
	gboolean done = FALSE;
	
	if(is_running) {
		if(bd && bd->dialog) {
			gtk_window_present(GTK_WINDOW(bd->dialog));
			return;
		} else
			is_running = FALSE;
	}
	
	is_running = TRUE;

    bd = create_backdrop_dialog(mcs_plugin);
    gtk_window_set_position(GTK_WINDOW(bd->dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(bd->dialog), FALSE);
    gtk_widget_show(bd->dialog);
	while(!done) {
		switch(gtk_dialog_run(GTK_DIALOG(bd->dialog))) {
			case GTK_RESPONSE_HELP:
				xfce_exec("xfhelp4 xfdesktop.html", FALSE, FALSE, NULL);
				break;
			
			default:
				backdrop_write_options(bd->plugin);
				is_running = FALSE;
				gtk_widget_destroy(bd->dialog);
				g_free(bd);
				bd = NULL;
				done = TRUE;
				break;
		}
	}
}

/* macro defined in manager-plugin.h */
MCS_PLUGIN_CHECK_INIT
