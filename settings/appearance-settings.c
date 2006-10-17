/*  xfce4
 *  
 *  Copyright (C) 2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *  Copyright (C) 2003,2006 Benedikt Meurer <benny@xfce.org>
 *  Copyright (c) 2004 Brian Tarricone <bjt23@cornell.edu>
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
#ifdef GTK_DISABLE_DEPRECATED
#undef GTK_DISABLE_DEPRECATED
#endif
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <xfce-mcs-manager/manager-plugin.h>

#include "xfdesktop-common.h"
#include "backdrop-list-manager.h"
#include "settings-common.h"
#include "behavior-settings.h"

#define OLD_RCFILE  "settings/backdrop.xml"
#define RCFILE      "xfce4/mcs_settings/desktop.xml"
#define PLUGIN_NAME "backdrop"

/* there can be only one */
static gboolean is_running = FALSE;
static GList **screens;  /* array of lists of BackdropPanels */
static gboolean xinerama_stretch = FALSE;

static void backdrop_create_channel (McsPlugin * mcs_plugin);
static gboolean backdrop_write_options (McsPlugin * mcs_plugin);
static void run_dialog (McsPlugin * mcs_plugin);

static void
xdg_migrate_config(const gchar *old_filename, const gchar *new_filename)
{
    gchar *old_file, *new_file;
    
    new_file = xfce_resource_save_location(XFCE_RESOURCE_CONFIG, new_filename, FALSE);
    /* if the new file _does_ exist, assume we've already migrated */
    if(!g_file_test(new_file, G_FILE_TEST_IS_REGULAR)) {
        old_file = xfce_get_userfile(old_filename, NULL);
        if(g_file_test(old_file, G_FILE_TEST_IS_REGULAR)) {
            /* we have to run it again to make sure the directory exists */
            g_free(new_file);
            new_file = xfce_resource_save_location(XFCE_RESOURCE_CONFIG,
                    new_filename, TRUE);
            
            /* try atomic move first, if not, resort to read->write->delete */
            if(!link(old_file, new_file))
                unlink(old_file);
            else {
                gchar *contents = NULL;
                gsize len = 0;
                if(g_file_get_contents(old_file, &contents, &len, NULL)) {
                    FILE *fp = fopen(new_file, "w");
                    if(fp) {
                        if(fwrite(contents, len, 1, fp) == len) {
                            fclose(fp);
                            unlink(old_file);
                        } else {
                            fclose(fp);
                            g_critical("backdrop_settings.c: Unable to migrate %s to new location (error writing to file)", old_filename);
                        }
                    } else
                        g_critical("backdrop_settings.c: Unable to migrate %s to new location (error opening target file for writing)", old_filename);
                } else
                    g_critical("backdrop_settings.c: Unable to migrate %s to new location (error reading old file)", old_filename);
            }
        }
        g_free(old_file);
    }
    g_free(new_file);    
}

McsPluginInitResult
mcs_plugin_init (McsPlugin * mcs_plugin)
{
    xdg_migrate_config(OLD_RCFILE, RCFILE);
    xdg_migrate_config("backdrops.list", "xfce4/desktop/backdrops.list");
    
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    mcs_plugin->plugin_name = g_strdup (PLUGIN_NAME);
    /* the button label in the xfce-mcs-manager dialog */
    mcs_plugin->caption = g_strdup (Q_ ("Button Label|Desktop"));
    mcs_plugin->run_dialog = run_dialog;

    mcs_plugin->icon = xfce_themed_icon_load("xfce4-backdrop", 48);
    if (G_LIKELY (mcs_plugin->icon != NULL))
        g_object_set_data_full(G_OBJECT(mcs_plugin->icon), "mcs-plugin-icon-name", g_strdup("xfce4-backdrop"), g_free);

    backdrop_create_channel (mcs_plugin);

    return (MCS_PLUGIN_INIT_OK);
}

static void
backdrop_create_channel (McsPlugin * mcs_plugin)
{
    McsSetting *setting;
    gchar *rcfile;
    gint i, j, nscreens, nmonitors;
    gchar setting_name[128];

    rcfile = xfce_resource_lookup(XFCE_RESOURCE_CONFIG, RCFILE);
    if (rcfile) {
        mcs_manager_add_channel_from_file (mcs_plugin->manager, BACKDROP_CHANNEL, 
                                           rcfile);
        g_free (rcfile);
    }
    else
        mcs_manager_add_channel (mcs_plugin->manager, BACKDROP_CHANNEL);
    
    
    setting = mcs_manager_setting_lookup(mcs_plugin->manager, "xineramastretch",
            BACKDROP_CHANNEL);
    if(setting && setting->data.v_int)
        xinerama_stretch = TRUE;

    nscreens = gdk_display_get_n_screens(gdk_display_get_default());
    screens = g_new0(GList *, nscreens);
    for(i = 0; i < nscreens; i++) {
        nmonitors = gdk_screen_get_n_monitors(gdk_display_get_screen(gdk_display_get_default(), i));
        for(j = 0; j < nmonitors; j++) {
            BackdropPanel *bp = g_new0(BackdropPanel, 1);
            
            bp->xscreen = i;
            bp->monitor = j;
            
            /* the path to an image file */
            g_snprintf(setting_name, 128, "imagepath_%d_%d", i, j);
            setting = mcs_manager_setting_lookup(mcs_plugin->manager, setting_name,
                    BACKDROP_CHANNEL);
            if(setting) {
                /* note: remove file migration for 4.4 */
                gint val;
                gchar *old_loc = xfce_get_homefile(".xfce4",
                        _("backdrops.list"), NULL);
                
                if(g_utf8_validate(old_loc, -1, NULL))
                    val = g_utf8_collate(old_loc, setting->data.v_string);
                else
                    val = strcmp(old_loc, setting->data.v_string);
                if(!val) {
                    gchar new_loc[PATH_MAX];
                    g_snprintf(new_loc, PATH_MAX, "xfce4/desktop/%s",
                            _("backdrops.list"));
                    bp->image_path = xfce_resource_save_location(XFCE_RESOURCE_CONFIG,
                            new_loc, TRUE);
                    mcs_manager_set_string(mcs_plugin->manager, setting_name,
                            BACKDROP_CHANNEL, bp->image_path);
                } else
                    bp->image_path = g_strdup(setting->data.v_string);
                g_free(old_loc);
            } else {
                bp->image_path = g_strdup(DEFAULT_BACKDROP);
                mcs_manager_set_string(mcs_plugin->manager, setting_name,
                        BACKDROP_CHANNEL, bp->image_path);
            }
            
            /* the backdrop image style */
            g_snprintf(setting_name, 128, "imagestyle_%d_%d", i, j);
            setting = mcs_manager_setting_lookup(mcs_plugin->manager, setting_name,
                    BACKDROP_CHANNEL);
            if(setting)
                bp->style = setting->data.v_int;
            else {
                bp->style = XFCE_BACKDROP_IMAGE_STRETCHED;
                mcs_manager_set_int(mcs_plugin->manager, setting_name,
                        BACKDROP_CHANNEL, bp->style);
            }
            
            /* brightness */
            g_snprintf(setting_name, 128, "brightness_%d_%d", i, j);
            setting = mcs_manager_setting_lookup(mcs_plugin->manager, setting_name,
                    BACKDROP_CHANNEL);
            if(setting)
                bp->brightness = setting->data.v_int;
            else {
                bp->brightness = 0;
                mcs_manager_set_int(mcs_plugin->manager, setting_name,
                        BACKDROP_CHANNEL, bp->brightness);
            }
            
            /* color 1 */
            g_snprintf(setting_name, 128, "color1_%d_%d", i, j);
            setting = mcs_manager_setting_lookup(mcs_plugin->manager, setting_name,
                    BACKDROP_CHANNEL);
            if(setting) {
                bp->color1.red = setting->data.v_color.red;
                bp->color1.green = setting->data.v_color.green;
                bp->color1.blue = setting->data.v_color.blue;
                bp->color1.alpha = setting->data.v_color.alpha;
            } else {
                /* Just a color by default #3b5b89 */
                bp->color1.red = (guint16)0x3b00;
                bp->color1.green = (guint16)0x5b00;
                bp->color1.blue = (guint16)0x8900;
                bp->color1.alpha = (guint16)0xffff;
                mcs_manager_set_color(mcs_plugin->manager, setting_name,
                        BACKDROP_CHANNEL, &bp->color1);
            }
            
            /* color 2 */
            g_snprintf(setting_name, 128, "color2_%d_%d", i, j);
            setting = mcs_manager_setting_lookup(mcs_plugin->manager, setting_name,
                    BACKDROP_CHANNEL);
            if(setting) {
                bp->color2.red = setting->data.v_color.red;
                bp->color2.green = setting->data.v_color.green;
                bp->color2.blue = setting->data.v_color.blue;
                bp->color2.alpha = setting->data.v_color.alpha;
            } else {
                /* Just a color by default #3e689e */
                bp->color2.red = (guint16)0x3e00;
                bp->color2.green = (guint16)0x6800;
                bp->color2.blue = (guint16)0x9e00;
                bp->color2.alpha = (guint16)0xffff;
                mcs_manager_set_color(mcs_plugin->manager, setting_name,
                        BACKDROP_CHANNEL, &bp->color2);
            }
            
            g_snprintf(setting_name, 128, "showimage_%d_%d", i, j);
            setting = mcs_manager_setting_lookup(mcs_plugin->manager, setting_name,
                    BACKDROP_CHANNEL);
            if(setting)
                bp->show_image = setting->data.v_int == 0 ? FALSE : TRUE;
            else {
                bp->show_image = FALSE;
                mcs_manager_set_int(mcs_plugin->manager, setting_name,
                        BACKDROP_CHANNEL, 0);
            }
            
            /* the color style */
            g_snprintf(setting_name, 128, "colorstyle_%d_%d", i, j);
            setting = mcs_manager_setting_lookup(mcs_plugin->manager, setting_name,
                    BACKDROP_CHANNEL);
            if(setting)
                bp->color_style = setting->data.v_int;
            else {
                bp->color_style = XFCE_BACKDROP_COLOR_SOLID;
                mcs_manager_set_int(mcs_plugin->manager, setting_name,
                        BACKDROP_CHANNEL, bp->color_style);
            }
            
            screens[i] = g_list_append(screens[i], bp);
        }
    }
    
    behavior_settings_load(mcs_plugin);

    mcs_manager_notify(mcs_plugin->manager, BACKDROP_CHANNEL);
}

static gboolean
backdrop_write_options (McsPlugin * mcs_plugin)
{
    gchar *rcfile;
    gboolean result;
    
    rcfile = xfce_resource_save_location(XFCE_RESOURCE_CONFIG, RCFILE, TRUE);
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
        g_snprintf(setting_name, 128, "imagepath_%d_%d", bp->xscreen, bp->monitor);
    mcs_manager_set_string (bp->bd->plugin->manager, setting_name, BACKDROP_CHANNEL,
                bp->image_path);
    /* Assume that if the user has changed the image path (s)he actually
       wants to see it on screen, so unselect the color only checkbox
     */
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(bp->show_image_chk), TRUE);
    }

    mcs_manager_notify (bp->bd->plugin->manager, BACKDROP_CHANNEL);
}

static void
color_set_cb(GtkWidget *b, BackdropPanel *bp)
{
    GdkColor color;
    gchar setting_name[128];
    
    gtk_color_button_get_color(GTK_COLOR_BUTTON(b), &color);
    
    if(b == bp->color1_box) {
        bp->color1.red = color.red;
        bp->color1.green = color.green;
        bp->color1.blue = color.blue;
        g_snprintf(setting_name, 128, "color1_%d_%d", bp->xscreen, bp->monitor);
        mcs_manager_set_color(bp->bd->plugin->manager, setting_name,
                BACKDROP_CHANNEL, &bp->color1);
    } else if(b == bp->color2_box) {
        bp->color2.red = color.red;
        bp->color2.green = color.green;
        bp->color2.blue = color.blue;
        g_snprintf(setting_name, 128, "color2_%d_%d", bp->xscreen, bp->monitor);
        mcs_manager_set_color(bp->bd->plugin->manager, setting_name,
                BACKDROP_CHANNEL, &bp->color2);
    } else
        g_critical("backdrop_settings.c: color_set_cb() called with invalid button widget!");
    
    mcs_manager_notify(bp->bd->plugin->manager, BACKDROP_CHANNEL);
}

static void
showimage_toggle(GtkWidget *b, BackdropPanel *bp)
{
    gchar setting_name[128];
    
    bp->show_image = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b));

    gtk_widget_set_sensitive(bp->image_frame_inner, bp->show_image);

    g_snprintf(setting_name, 128, "showimage_%d_%d", bp->xscreen, bp->monitor);
    mcs_manager_set_int (bp->bd->plugin->manager, setting_name, BACKDROP_CHANNEL,
             bp->show_image ? 1 : 0);
    mcs_manager_notify (bp->bd->plugin->manager, BACKDROP_CHANNEL);
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
update_preview_cb(GtkFileChooser *chooser, gpointer data)
{
    GtkImage *preview;
    char *filename;
    GdkPixbuf *pix = NULL;
    
    preview = GTK_IMAGE(data);
    filename = gtk_file_chooser_get_filename(chooser);
    
    if(g_file_test(filename, G_FILE_TEST_IS_REGULAR))
        pix = gdk_pixbuf_new_from_file_at_size(filename, 250, 250, NULL);
    g_free(filename);
    
    if(pix) {
        gtk_image_set_from_pixbuf(preview, pix);
        g_object_unref(G_OBJECT(pix));
    }
    gtk_file_chooser_set_preview_widget_active(chooser, (pix != NULL));
}

static void
browse_cb(GtkWidget *b, BackdropPanel *bp)
{
    GtkWidget *chooser, *preview;
    GtkFileFilter *filter;
    gchar *confdir;
    
    chooser = gtk_file_chooser_dialog_new(_("Select backdrop image or list file"),
            GTK_WINDOW(bp->bd->dialog), GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL,
            GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
    
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, _("All Files"));
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, _("Image Files"));
    gtk_file_filter_add_pattern(filter, "*.png");
    gtk_file_filter_add_pattern(filter, "*.jpg");
    gtk_file_filter_add_pattern(filter, "*.bmp");
    gtk_file_filter_add_pattern(filter, "*.svg");
    gtk_file_filter_add_pattern(filter, "*.xpm");
    gtk_file_filter_add_pattern(filter, "*.gif");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, _("List Files (*.list)"));
    gtk_file_filter_add_pattern(filter, "*.list");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);
    
    gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(chooser),
            DATADIR "/xfce4/backdrops", NULL);
    confdir = xfce_resource_save_location(XFCE_RESOURCE_CONFIG,
            "xfce4/desktop/", TRUE);
    if(confdir) {
        gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(chooser),
                confdir, NULL);
        g_free(confdir);
    }
    
    if(bp->image_path) {
        gchar *tmppath = g_strdup(bp->image_path);
        gchar *p = g_strrstr(tmppath, "/");
        if(p && p != tmppath)
            *(p+1) = 0;
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), tmppath);
        g_free(tmppath);
    }
    
    preview = gtk_image_new();
    gtk_widget_show(preview);
    gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(chooser), preview);
    gtk_file_chooser_set_preview_widget_active(GTK_FILE_CHOOSER(chooser), FALSE);
    g_signal_connect(G_OBJECT(chooser), "update-preview",
                     G_CALLBACK(update_preview_cb), preview);
    
    gtk_widget_show(chooser);
    if(gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
        gchar *filename;
        
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
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
    backdrop_list_manager_edit_list_file(bp->image_path, bp->bd->dialog,
                                         (BackdropListMgrCb)set_path_cb, bp);
}

void
new_list_cb(GtkWidget *w, BackdropPanel *bp)
{
    backdrop_list_manager_create_list_file(bp->bd->dialog,
                                           (BackdropListMgrCb)set_path_cb, bp);
}

static void
add_button_box (GtkWidget *vbox, BackdropPanel *bp)
{
    GtkWidget *hbox, *new_list_button;

    hbox = gtk_hbox_new (FALSE, BORDER);
    gtk_widget_show (hbox);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    bp->edit_list_button = gtk_button_new_with_mnemonic(_("_Edit list..."));
    gtk_widget_show (bp->edit_list_button);
    gtk_box_pack_end (GTK_BOX (hbox), bp->edit_list_button, FALSE, FALSE,
            0);

    g_signal_connect (G_OBJECT (bp->edit_list_button), "clicked",
              G_CALLBACK (edit_list_cb), bp);
    
    new_list_button = gtk_button_new_with_mnemonic(_("_New list..."));
    gtk_widget_show (new_list_button);
    gtk_box_pack_end (GTK_BOX (hbox), new_list_button, FALSE, FALSE, 0);
    
    g_signal_connect (G_OBJECT (new_list_button), "clicked",
              G_CALLBACK (new_list_cb), bp);

    if (!bp->image_path || !is_backdrop_list (bp->image_path))
        gtk_widget_set_sensitive (bp->edit_list_button, FALSE);
}

/* style options */
static void
set_style(GtkWidget *combo, BackdropPanel *bp)
{
    gchar setting_name[128];
    
    bp->style = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    g_snprintf(setting_name, 128, "imagestyle_%d_%d", bp->xscreen, bp->monitor);
    mcs_manager_set_int (bp->bd->plugin->manager, setting_name, BACKDROP_CHANNEL,
             bp->style);
    mcs_manager_notify (bp->bd->plugin->manager, BACKDROP_CHANNEL);
}

static void
add_style_options (GtkWidget *vbox, GtkSizeGroup * sg, BackdropPanel *bp)
{
    GtkWidget *hbox, *label, *combo;

    hbox = gtk_hbox_new (FALSE, BORDER);
    gtk_widget_show (hbox);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    
    label = gtk_label_new_with_mnemonic(_("S_tyle:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_size_group_add_widget (sg, label);
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    
    bp->style_combo = combo = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Auto"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Centered"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Tiled"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Stretched"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Scaled"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), bp->style);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), combo);
    gtk_widget_show(combo);
    gtk_box_pack_start(GTK_BOX(hbox), combo, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(combo), "changed",
                     G_CALLBACK(set_style), bp);
}

static void
set_color_style(GtkComboBox *combo, BackdropPanel *bp)
{
    gchar setting_name[128];
    
    bp->color_style = gtk_combo_box_get_active(combo);
    
    if(bp->color_style == XFCE_BACKDROP_COLOR_SOLID)
        gtk_widget_set_sensitive(bp->color2_hbox, FALSE);
    else
        gtk_widget_set_sensitive(bp->color2_hbox, TRUE);
    
    g_snprintf(setting_name, 128, "colorstyle_%d_%d", bp->xscreen, bp->monitor);
    mcs_manager_set_int(bp->bd->plugin->manager, setting_name, BACKDROP_CHANNEL,
            bp->color_style);
    mcs_manager_notify(bp->bd->plugin->manager, BACKDROP_CHANNEL);
}

static void
update_brightness(GtkRange *w, BackdropPanel *bp)
{
    gchar setting_name[128];
    
    bp->brightness = gtk_range_get_value(w);
    g_snprintf(setting_name, 128, "brightness_%d_%d", bp->xscreen, bp->monitor);
    mcs_manager_set_int(bp->bd->plugin->manager, setting_name, BACKDROP_CHANNEL,
            bp->brightness);
    mcs_manager_notify(bp->bd->plugin->manager, BACKDROP_CHANNEL);
}

/* this is a workaround for a gtk bug.  it seems that if you move the slider
 * around a bit, and try to go back to zero, you often get "-0" displayed */
static gchar *
hscale_format(GtkScale *w, gdouble val, gpointer user_data)
{
    return g_strdup_printf("%d", (gint)val);
}

static void
add_brightness_slider(GtkWidget *vbox, BackdropPanel *bp)
{
    GtkWidget *label, *hbox, *hscale;
    
    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    label = gtk_label_new_with_mnemonic(_("A_djust Brightness:"));
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);
    
    hscale = gtk_hscale_new_with_range(-128, 127, 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), hscale);
    gtk_scale_set_draw_value(GTK_SCALE(hscale), TRUE);
    gtk_scale_set_value_pos(GTK_SCALE(hscale), GTK_POS_RIGHT);
    gtk_range_set_increments(GTK_RANGE(hscale), 1, 5);
    gtk_range_set_value(GTK_RANGE(hscale), bp->brightness);
    gtk_range_set_update_policy(GTK_RANGE(hscale), GTK_UPDATE_DISCONTINUOUS);
    gtk_widget_show(hscale);
    gtk_box_pack_start(GTK_BOX(hbox), hscale, TRUE, TRUE, 4);
    g_signal_connect(G_OBJECT(hscale), "value-changed",
            G_CALLBACK(update_brightness), bp);
    g_signal_connect(G_OBJECT(hscale), "format-value",
            G_CALLBACK(hscale_format), NULL);
}

#if 0
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
#endif

static void
manage_desktop_chk_toggled_cb(GtkToggleButton *tb, gpointer user_data)
{
    BackdropDialog *bd = user_data;
    const gchar *setting_name;
    McsSetting *setting = NULL;
    
    if(gtk_toggle_button_get_active(tb)) {
        GError *err = NULL;
        
        if(!xfce_exec(BINDIR "/xfdesktop", FALSE, FALSE, NULL)
                && !xfce_exec("xfdesktop", FALSE, FALSE, &err))
        {
            gchar *secondary = NULL;
            
            g_signal_handlers_block_by_func(G_OBJECT(tb),
                    G_CALLBACK(manage_desktop_chk_toggled_cb), user_data);
            gtk_toggle_button_set_active(tb, FALSE);
            g_signal_handlers_unblock_by_func(G_OBJECT(tb),
                    G_CALLBACK(manage_desktop_chk_toggled_cb), user_data);
            
            secondary = g_strdup_printf(_("Xfce will be unable to manage your desktop (%s)."),
                    err ? err->message : _("Unknown Error"));
            if(err)
                g_error_free(err);
            
            xfce_message_dialog(GTK_WINDOW(bd->dialog), _("Error"),
                    GTK_STOCK_DIALOG_ERROR,
                    _("Unable to start xfdesktop"), secondary,
                    GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
            g_free(secondary);
        } else
            gtk_widget_set_sensitive(bd->top_notebook, TRUE);
        
        setting_name = "managedesktop-show-warning-on";
    } else {
        Window xid;
        
        if(xfdesktop_check_is_running(&xid))
            xfdesktop_send_client_message(xid, QUIT_MESSAGE);
        gtk_widget_set_sensitive(bd->top_notebook, FALSE);
        
        setting_name = "managedesktop-show-warning";
    }
    
    setting = mcs_manager_setting_lookup(bd->plugin->manager,
            setting_name, BACKDROP_CHANNEL);
    if(!setting || setting->data.v_int) {
        GtkWidget *dlg, *lbl, *chk, *vbox;
        
        dlg = gtk_dialog_new_with_buttons(_("Information"),
                GTK_WINDOW(bd->dialog),
                GTK_DIALOG_DESTROY_WITH_PARENT|GTK_DIALOG_NO_SEPARATOR,
                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        
        vbox = gtk_vbox_new(FALSE, BORDER);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), BORDER);
        gtk_widget_show(vbox);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), vbox, TRUE, TRUE, 0);
        
        /* FIXME: i'm avoiding changing a string here.  after the string is
         * translated, remove the second string and replace it with the first */
        if(!strcmp(setting_name, "managedesktp-show-warning-on"))
            lbl = gtk_label_new(_("To ensure that this setting takes effect the next time you start Xfce, please be sure to save your session when logging out.  If you are not using the Xfce Session Manager (xfce4-session), you will need to manually edit your ~/.config/xfce4/xinitrc file.  Details are available in the documentation provided on http://xfce.org/."));
        else
            lbl = gtk_label_new(_("To ensure that Xfce does not manage your desktop the next time you start Xfce, please be sure to save your session when logging out.  If you are not using the Xfce Session Manager (xfce4-session), you will need to manually edit your ~/.config/xfce4/xinitrc file.  Details are available in the documentation provided on http://xfce.org/."));
        gtk_label_set_line_wrap(GTK_LABEL(lbl), TRUE);
        gtk_misc_set_alignment(GTK_MISC(lbl), 0.0, 0.5);
        gtk_widget_show(lbl);
        gtk_box_pack_start(GTK_BOX(vbox), lbl, FALSE, FALSE, 0);
        
        add_spacer(GTK_BOX(vbox));
        
        chk = gtk_check_button_new_with_mnemonic(_("_Do not show this again"));
        gtk_widget_show(chk);
        gtk_box_pack_start(GTK_BOX(vbox), chk, FALSE, FALSE, 0);
        
        gtk_dialog_run(GTK_DIALOG(dlg));
        
        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chk))) {
            mcs_manager_set_int(bd->plugin->manager, setting_name,
                                BACKDROP_CHANNEL, 0);
            backdrop_write_options(bd->plugin);
        }
        
        gtk_widget_destroy(dlg);
    }
}

static void
xinerama_stretch_toggled_cb(GtkToggleButton *tb, gpointer user_data)
{
    McsPlugin *mcs_plugin = user_data;
    GList *l;
    
    xinerama_stretch = gtk_toggle_button_get_active(tb);
    
    /* this assumes a single GdkScreen for the GdkDisplay.  this should be
     * the case for xinerama, but if the X server later supports some
     * monitors in xinerama mode and some not, this won't work properly. */
    if(xinerama_stretch && screens[0]) {
        for(l = screens[0]->next; l; l = l->next) {
            BackdropPanel *bp = l->data;
            gtk_widget_set_sensitive(bp->page, FALSE);
        }
    } else if(screens[0]) {
        for(l = screens[0]->next; l; l = l->next) {
            BackdropPanel *bp = l->data;
            gtk_widget_set_sensitive(bp->page, TRUE);
            if(bp->color_style == XFCE_BACKDROP_COLOR_SOLID)
                gtk_widget_set_sensitive(bp->color2_hbox, FALSE);
            if(!bp->show_image)
                gtk_widget_set_sensitive(bp->image_frame_inner, FALSE);
        }
    }
    
    mcs_manager_set_int(mcs_plugin->manager, "xineramastretch",
            BACKDROP_CHANNEL, xinerama_stretch ? 1 : 0);
    mcs_manager_notify(mcs_plugin->manager, BACKDROP_CHANNEL);
}

/* the dialog */
static BackdropDialog *
create_backdrop_dialog (McsPlugin * mcs_plugin)
{
    GtkWidget *mainvbox, *frame, *vbox, *hbox, *label, *combo,
              *button, *image, *chk;
    GtkSizeGroup *sg;
    GdkColor color;
    BackdropDialog *bd;
    gint i, j, nscreens, nmonitors = 0;
    XfceKiosk *kiosk;
    gboolean allow_custom_backdrop = TRUE;
    Window xid;

    bd = g_new0(BackdropDialog, 1);
    bd->plugin = mcs_plugin;

    /* the dialog */
    bd->dialog = xfce_titled_dialog_new_with_buttons (_("Desktop Preferences"), NULL,
                          GTK_DIALOG_NO_SEPARATOR,
                          GTK_STOCK_CLOSE,
                          GTK_RESPONSE_CANCEL,
#ifndef NO_HELP_BUTTON
                          GTK_STOCK_HELP,
                          GTK_RESPONSE_HELP,
#endif
                          NULL);

    gtk_window_set_icon_name(GTK_WINDOW(bd->dialog), "xfce4-backdrop");
    gtk_dialog_set_default_response(GTK_DIALOG(bd->dialog), GTK_RESPONSE_CANCEL);
    
    mainvbox = GTK_DIALOG (bd->dialog)->vbox;

    add_spacer(GTK_BOX(mainvbox));
    
    chk = gtk_check_button_new_with_mnemonic(_("Allow _Xfce to manage the desktop"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk),
            xfdesktop_check_is_running(&xid));
    gtk_widget_show(chk);
    gtk_box_pack_start(GTK_BOX(mainvbox), chk, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(chk), "toggled",
            G_CALLBACK(manage_desktop_chk_toggled_cb), bd);
    
    add_spacer(GTK_BOX(mainvbox));
    
    /* main notebook */
    bd->top_notebook = gtk_notebook_new();
    gtk_widget_set_sensitive(bd->top_notebook,
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chk)));
    gtk_widget_show(bd->top_notebook);
    gtk_box_pack_start(GTK_BOX(mainvbox), bd->top_notebook, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (bd->top_notebook), 6);
    
    nscreens = gdk_display_get_n_screens(gdk_display_get_default());
    if(nscreens == 1)
        nmonitors = gdk_screen_get_n_monitors(gdk_display_get_default_screen(gdk_display_get_default()));
    
    kiosk = xfce_kiosk_new("xfdesktop");
    allow_custom_backdrop = xfce_kiosk_query(kiosk, "CustomizeBackdrop");
    xfce_kiosk_free(kiosk);
    
    if(nscreens > 1 || nmonitors > 1) {
        /* only use a noteboook if we have more than one screen */
        vbox = gtk_vbox_new(FALSE, 0);
        gtk_widget_show(vbox);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
        
        if(nmonitors > 1) {
            /* we have xinerama.  it would be nice to extend this
             * to work for non-xinerama setups too, but that would
             * probably require some xfdesktop rearchitecting. */
            hbox = gtk_hbox_new(FALSE, BORDER);
            gtk_container_set_border_width(GTK_CONTAINER(hbox), BORDER);
            gtk_widget_show(hbox);
            gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
            
            chk = gtk_check_button_new_with_mnemonic(_("Stretch single backdrop onto _all monitors"));
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk), xinerama_stretch);
            gtk_widget_show(chk);
            gtk_box_pack_start(GTK_BOX(hbox), chk, TRUE, TRUE, 0);
            g_signal_connect(G_OBJECT(chk), "toggled",
                    G_CALLBACK(xinerama_stretch_toggled_cb), mcs_plugin);
            
            add_spacer(GTK_BOX(vbox));
        }
        
        bd->screens_notebook = gtk_notebook_new();
        gtk_widget_show(bd->screens_notebook);
        gtk_box_pack_start(GTK_BOX(vbox), bd->screens_notebook, FALSE, FALSE, 0);
        
        label = gtk_label_new_with_mnemonic(_("_Appearance"));
        gtk_widget_show(label);
        gtk_notebook_append_page(GTK_NOTEBOOK(bd->top_notebook), vbox, label);
    }
    
    for(i = 0; i < nscreens; i++) {
        nmonitors = gdk_screen_get_n_monitors(gdk_display_get_default_screen(gdk_display_get_default()));
        for(j = 0; j < nmonitors; j++) {
            GtkWidget *page;
            gchar screen_label[256];
            BackdropPanel *bp = g_list_nth_data(screens[i], j);
            
            if(!bp) {
                g_critical("Number of screens changed after plugin init!");
                break;
            }
            
            bp->bd = bd;
            
            bp->page = page = gtk_vbox_new(FALSE, BORDER);
            gtk_widget_show(page);
            
            add_spacer(GTK_BOX(page));
            
            /* color settings frame */
            
            frame = xfce_create_framebox(_("Color"), &bp->color_frame);
            gtk_widget_show(frame);
            gtk_box_pack_start(GTK_BOX(page), frame, FALSE, FALSE, 0);
            
            vbox = gtk_vbox_new(FALSE, BORDER);
            gtk_widget_show(vbox);
            gtk_container_add(GTK_CONTAINER(bp->color_frame), vbox);
            
            sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
            
            hbox = gtk_hbox_new(FALSE, BORDER);
            gtk_widget_show(hbox);
            gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
            
            label = gtk_label_new_with_mnemonic(_("_Color Style:"));
            gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
            gtk_size_group_add_widget(sg, label);
            gtk_widget_show(label);
            gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
            
            bp->color_style_combo = combo = gtk_combo_box_new_text();
            gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Solid Color"));
            gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Horizontal Gradient"));
            gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Vertical Gradient"));
            gtk_combo_box_set_active(GTK_COMBO_BOX(combo), bp->color_style);
            gtk_widget_show(combo);
            gtk_box_pack_start(GTK_BOX(hbox), combo, TRUE, TRUE, 0);
            g_signal_connect(G_OBJECT(combo), "changed",
                             G_CALLBACK(set_color_style), bp);
            gtk_label_set_mnemonic_widget(GTK_LABEL(label), combo);
            
            /* first color */
            hbox = gtk_hbox_new(FALSE, BORDER);
            gtk_widget_show(hbox);
            gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
            
            label = gtk_label_new_with_mnemonic(_("Fi_rst Color:"));
            gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
            gtk_size_group_add_widget(sg, label);
            gtk_widget_show(label);
            gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
            
            color.red = bp->color1.red;
            color.green = bp->color1.green;
            color.blue = bp->color1.blue;
            bp->color1_box = button = gtk_color_button_new_with_color(&color);
            gtk_widget_show(button);
            gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
            g_signal_connect(button, "color-set", G_CALLBACK(color_set_cb), bp);
            
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
            
            color.red = bp->color2.red;
            color.green = bp->color2.green;
            color.blue = bp->color2.blue;
            bp->color2_box = button = gtk_color_button_new_with_color(&color);
            gtk_widget_show(button);
            gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
            g_signal_connect(button, "color-set", G_CALLBACK(color_set_cb), bp);
            
            gtk_label_set_mnemonic_widget(GTK_LABEL(label), button);
            
            if(bp->color_style == XFCE_BACKDROP_COLOR_SOLID)
                gtk_widget_set_sensitive(hbox, FALSE);
            
            g_object_unref(G_OBJECT(sg));
            
            /* image settings frame */
            
            sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
            
            frame = xfce_create_framebox(_("Image"), &bp->image_frame);
            gtk_widget_show(frame);
            gtk_box_pack_start(GTK_BOX(page), frame, FALSE, FALSE, 0);
            
            vbox = gtk_vbox_new(FALSE, BORDER);
            gtk_widget_show(vbox);
            gtk_container_add(GTK_CONTAINER(bp->image_frame), vbox);
            
            bp->show_image_chk = gtk_check_button_new_with_mnemonic(_("Show _Image"));
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bp->show_image_chk),
                    bp->show_image);
            gtk_widget_show(bp->show_image_chk);
            gtk_box_pack_start(GTK_BOX(vbox), bp->show_image_chk, FALSE, FALSE, 0);
            g_signal_connect(G_OBJECT(bp->show_image_chk), "toggled",
                    G_CALLBACK(showimage_toggle), bp);
            
            /* inner frame */
            frame = xfce_create_framebox(NULL, &bp->image_frame_inner);
            gtk_widget_show(frame);
            gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
            
            vbox = gtk_vbox_new(FALSE, 0);
            gtk_widget_show(vbox);
            gtk_container_add(GTK_CONTAINER(bp->image_frame_inner), vbox);
            
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
            
            /* set sensitive state of image settings based on 'Show Image' */
            showimage_toggle(bp->show_image_chk, bp);
            
            /* image brightness */
            add_brightness_slider(page, bp);
            
            add_spacer(GTK_BOX(page));
            
            if(nscreens == 1 && nmonitors == 1) {
                /* add the single backdrop settings page to the main notebook */
                label = gtk_label_new_with_mnemonic(_("_Appearance"));
                gtk_widget_show(label);
                gtk_notebook_append_page(GTK_NOTEBOOK(bd->top_notebook), page, label);
            } else if((nscreens > 1 && nmonitors == 1)
                    || (nscreens == 1 && nmonitors > 1))
            {
                if(nmonitors > 1 && j > 0 && xinerama_stretch)
                    gtk_widget_set_sensitive(page, FALSE);
                    
                /* but if we have more than one, add them as pages to another */
                if(nscreens > 1)
                    g_snprintf(screen_label, 256, _("Screen %d"), i);
                else
                    g_snprintf(screen_label, 256, _("Screen %d"), j);
                label = gtk_label_new(screen_label);
                gtk_notebook_append_page(GTK_NOTEBOOK(bd->screens_notebook), page, label);
            } else {
                /* and if we have multiple screens, each with multiple monitors */
                g_snprintf(screen_label, 256, _("Screen %d, Monitor %d"), i, j);
                label = gtk_label_new(screen_label);
                gtk_notebook_append_page(GTK_NOTEBOOK(bd->screens_notebook), page, label);
            }
            
            if(!allow_custom_backdrop)
                gtk_widget_set_sensitive(page, FALSE);
            
            set_dnd_dest(bp);
        }
    }
    
    /* menu page */
    
    vbox = behavior_page_create(bd);
    gtk_widget_show(vbox);
    
    label = gtk_label_new_with_mnemonic(_("_Behavior"));
    gtk_widget_show(label);
    gtk_notebook_append_page(GTK_NOTEBOOK(bd->top_notebook), vbox, label);
    
    add_spacer(GTK_BOX(mainvbox));

    return bd;
}

static void
run_dialog_cb(GtkButton *btn, gint resp, BackdropDialog *bd)
{
    switch(resp) {
            case GTK_RESPONSE_HELP:
                xfce_exec("xfhelp4 xfdesktop.html", FALSE, FALSE, NULL);
                break;
            
            default:
                backdrop_write_options(bd->plugin);
                is_running = FALSE;
                gtk_widget_destroy(bd->dialog);
                g_free(bd);
                bd = NULL;
                break;
    }
}

static void
run_dialog (McsPlugin * mcs_plugin)
{
    static BackdropDialog *bd = NULL;
    GdkPixbuf *win_icon;
    
    if(is_running) {
        if(bd && bd->dialog) {
            gtk_window_present(GTK_WINDOW(bd->dialog));
            gtk_window_set_focus (GTK_WINDOW(bd->dialog), NULL);
            return;
        } else
            is_running = FALSE;
    }
    
    is_running = TRUE;
    
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    bd = create_backdrop_dialog(mcs_plugin);
    win_icon = xfce_themed_icon_load("xfce4-backdrop", 48);
    if(win_icon) {
        gtk_window_set_icon(GTK_WINDOW(bd->dialog), win_icon);
        g_object_unref(G_OBJECT(win_icon));
    }
    xfce_gtk_window_center_on_monitor_with_pointer (GTK_WINDOW (bd->dialog));
    g_signal_connect(G_OBJECT(bd->dialog), "response",
            G_CALLBACK(run_dialog_cb), bd);
    gtk_window_set_modal(GTK_WINDOW(bd->dialog), FALSE);
    gdk_x11_window_set_user_time(bd->dialog->window, 
            gdk_x11_get_server_time (bd->dialog->window));
    gtk_widget_show(bd->dialog);
}

/* macro defined in manager-plugin.h */
MCS_PLUGIN_CHECK_INIT
