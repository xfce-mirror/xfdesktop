/*
 *  desktop-menu-plugin.c - xfce4-panel plugin that displays the desktop menu
 *
 *  Copyright (C) 2004-2009 Brian Tarricone, <bjt23@cornell.edu>
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
 *
 *  Contributors:
 *    Jean-Francois Wauthy (option panel for choice between icon/text)
 *    Jasper Huijsmans (menu placement function, toggle button, scaled image
 *                      fixes)
 *    Olivier Fourdan  (remote popup)
 */

#include <gtk/gtk.h>
#include <glib.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxfce4panel/xfce-panel-convenience.h>

#ifdef HAVE_LIBEXO
#include <exo/exo.h>
#endif
#ifdef HAVE_THUNAR_VFS
#include <thunar-vfs/thunar-vfs.h>
#endif

#include "desktop-menu-stub.h"
#include "xfdesktop-common.h"
#include "xfce4-popup-menu.h"

#define BORDER 8
#define DEFAULT_BUTTON_ICON  DATADIR "/pixmaps/xfce4_xicon1.png"
#define DEFAULT_BUTTON_TITLE "Xfce Menu"

/* this'll only allow one copy of the plugin; useful for forcing the
 * xfce4-popup-menu keybind to work with only one plugin */
/* #define CHECK_RUNNING_PLUGIN */

typedef struct _DMPlugin {
    XfcePanelPlugin *plugin;
    
    GtkWidget *button;
    GtkWidget *box;
    GtkWidget *image;
    GtkWidget *label;
    XfceDesktopMenu *desktop_menu;
    gboolean use_default_menu;
    gchar *menu_file;
    gchar *icon_file;
    gboolean show_menu_icons;
    gchar *button_title;
    gboolean show_button_title;
    
    GtkWidget *file_entry;
    GtkWidget *icon_entry;
    GtkWidget *icons_chk;
    GtkTooltips *tooltip;  /* needed? */
} DMPlugin;


static gchar *
dmp_get_real_path(const gchar *raw_path)
{
    if(!raw_path)
        return NULL;
    
    if(strstr(raw_path, "$XDG_CONFIG_DIRS/") == raw_path)
        return xfce_resource_lookup(XFCE_RESOURCE_CONFIG, raw_path+17);
    else if(strstr(raw_path, "$XDG_CONFIG_HOME/") == raw_path)
        return xfce_resource_save_location(XFCE_RESOURCE_CONFIG, raw_path+17, FALSE);
    else if(strstr(raw_path, "$XDG_DATA_DIRS/") == raw_path)
        return xfce_resource_lookup(XFCE_RESOURCE_DATA, raw_path+15);
    else if(strstr(raw_path, "$XDG_DATA_HOME/") == raw_path)
        return xfce_resource_save_location(XFCE_RESOURCE_DATA, raw_path+15, FALSE);
    else if(strstr(raw_path, "$XDG_CACHE_HOME/") == raw_path)
        return xfce_resource_save_location(XFCE_RESOURCE_CACHE, raw_path+16, FALSE);
    
    return xfce_expand_variables(raw_path, NULL);
}

static GdkPixbuf *
dmp_get_icon(const gchar *icon_name, gint size, GtkOrientation orientation)
{
    GdkPixbuf *pix = NULL;
    gchar *filename;
    gint w, h;
    
    filename = xfce_themed_icon_lookup(icon_name, size);
    if(!filename)
        return NULL;
    
    w = orientation == GTK_ORIENTATION_HORIZONTAL ? -1 : size;
    h = orientation == GTK_ORIENTATION_VERTICAL ? -1 : size;
    pix = gdk_pixbuf_new_from_file_at_scale(filename, w, h, TRUE, NULL);
    
    g_free(filename);
    
    return pix;
}

static void
dmp_set_size(XfcePanelPlugin *plugin, gint wsize, DMPlugin *dmp)
{
    gint width, height, size, pix_w = 0, pix_h = 0;
    GtkOrientation orientation = xfce_panel_plugin_get_orientation(plugin);
    
    size = wsize - MAX(GTK_WIDGET(dmp->button)->style->xthickness,
                       GTK_WIDGET(dmp->button)->style->ythickness) - 1;
    
    DBG("wsize: %d, size: %d", wsize, size);
    
    if(dmp->icon_file) {
        GdkPixbuf *pix = dmp_get_icon(dmp->icon_file, size, orientation);
        if(pix) {
            pix_w = gdk_pixbuf_get_width(pix);
            pix_h = gdk_pixbuf_get_height(pix);
            gtk_image_set_from_pixbuf(GTK_IMAGE(dmp->image), pix);
            g_object_unref(G_OBJECT(pix));
        }
    }
    
    width = pix_w + (wsize - size);
    height = pix_h + (wsize - size);
    
    if(dmp->show_button_title) {
        GtkRequisition req;
        
        gtk_widget_size_request(dmp->label, &req);
        if(orientation == GTK_ORIENTATION_HORIZONTAL)
            width += req.width + BORDER / 2;
        else {
            width = (width > req.width ? width : req.width
                     + GTK_WIDGET(dmp->label)->style->xthickness);
            height += req.height + BORDER / 2;
        }
    }
    
    if(dmp->icon_file && dmp->show_button_title) {
        gint delta = gtk_box_get_spacing(GTK_BOX(dmp->box));
        
        if(orientation == GTK_ORIENTATION_HORIZONTAL)
            width += delta;
        else
            height += delta;
    }
    
    DBG("width: %d, height: %d", width, height);
    
    gtk_widget_set_size_request(dmp->button, width, height);
}

static void
dmp_set_orientation(XfcePanelPlugin *plugin,
                    GtkOrientation orientation,
                    DMPlugin *dmp)
{
    if(!dmp->show_button_title)
        return;
    
    gtk_widget_set_size_request(dmp->button, -1, -1);
    
    gtk_container_remove(GTK_CONTAINER(dmp->button),
            gtk_bin_get_child(GTK_BIN(dmp->button)));
    
    if(xfce_panel_plugin_get_orientation(plugin) == GTK_ORIENTATION_HORIZONTAL)
        dmp->box = gtk_hbox_new(FALSE, BORDER / 2);
    else
        dmp->box = gtk_vbox_new(FALSE, BORDER / 2);
    gtk_container_set_border_width(GTK_CONTAINER(dmp->box), 0);
    gtk_widget_show(dmp->box);
    gtk_container_add(GTK_CONTAINER(dmp->button), dmp->box);
    
    gtk_widget_show(dmp->image);
    gtk_box_pack_start(GTK_BOX(dmp->box), dmp->image, TRUE, TRUE, 0);
    gtk_widget_show(dmp->label);
    gtk_box_pack_start(GTK_BOX(dmp->box), dmp->label, TRUE, TRUE, 0);
    
    dmp_set_size(plugin, xfce_panel_plugin_get_size(plugin), dmp);
}

static void
show_title_toggled_cb(GtkToggleButton *tb, gpointer user_data)
{
    DMPlugin *dmp = user_data;
    
    dmp->show_button_title = gtk_toggle_button_get_active(tb);
    
    if(dmp->show_button_title)
        dmp_set_orientation(dmp->plugin, xfce_panel_plugin_get_orientation(dmp->plugin), dmp);
    else {
        gtk_widget_hide(dmp->label);
        dmp_set_size(dmp->plugin, xfce_panel_plugin_get_size(dmp->plugin), dmp);
    }
}

static void
dmp_free(XfcePanelPlugin *plugin, DMPlugin *dmp)
{
    if(dmp->desktop_menu)
        xfce_desktop_menu_destroy(dmp->desktop_menu);
    if(dmp->tooltip)
        gtk_object_sink(GTK_OBJECT(dmp->tooltip));
    
    if(dmp->menu_file)
        g_free(dmp->menu_file);
    if(dmp->icon_file)
        g_free(dmp->icon_file);
    if(dmp->button_title)
        g_free(dmp->button_title);
    
    g_free(dmp);
}

static void
dmp_position_menu (GtkMenu *menu, int *x, int *y, gboolean *push_in, 
                   DMPlugin *dmp)
{
    XfceScreenPosition pos;
    GtkRequisition req;

    gtk_widget_size_request(GTK_WIDGET(menu), &req);

    gdk_window_get_origin (GTK_WIDGET (dmp->plugin)->window, x, y);

    pos = xfce_panel_plugin_get_screen_position(dmp->plugin);

    switch(pos) {
        case XFCE_SCREEN_POSITION_NW_V:
        case XFCE_SCREEN_POSITION_W:
        case XFCE_SCREEN_POSITION_SW_V:
            *x += dmp->button->allocation.width;
            *y += dmp->button->allocation.height - req.height;
            break;
        
        case XFCE_SCREEN_POSITION_NE_V:
        case XFCE_SCREEN_POSITION_E:
        case XFCE_SCREEN_POSITION_SE_V:
            *x -= req.width;
            *y += dmp->button->allocation.height - req.height;
            break;
        
        case XFCE_SCREEN_POSITION_NW_H:
        case XFCE_SCREEN_POSITION_N:
        case XFCE_SCREEN_POSITION_NE_H:
            *y += dmp->button->allocation.height;
            break;
        
        case XFCE_SCREEN_POSITION_SW_H:
        case XFCE_SCREEN_POSITION_S:
        case XFCE_SCREEN_POSITION_SE_H:
            *y -= req.height;
            break;
        
        default:  /* floating */
        {
            GdkScreen *screen = NULL;
            gint screen_width, screen_height;

            gdk_display_get_pointer(gtk_widget_get_display(GTK_WIDGET(dmp->plugin)),
                                                           &screen, x, y, NULL);
            screen_width = gdk_screen_get_width(screen);
            screen_height = gdk_screen_get_height(screen);
            if ((*x + req.width) > screen_width)
                *x -= req.width;
            if ((*y + req.height) > screen_height)
                *y -= req.height;
        }
    }

    if (*x < 0)
        *x = 0;

    if (*y < 0)
        *y = 0;

    /* TODO: wtf is this ? */
    *push_in = FALSE;
}

static gboolean
menu_destroy_idled(gpointer data)
{
    gtk_widget_destroy(GTK_WIDGET(data));
    return FALSE;
}

static void
menu_deactivated(GtkWidget *menu, gpointer user_data)
{
    DMPlugin *dmp = user_data;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dmp->button), FALSE);
    g_idle_add(menu_destroy_idled, menu);
}

static void
menu_activate(DMPlugin *dmp, gboolean at_pointer)
{
    GtkWidget *menu;
    GtkWidget *button;
    
    button = dmp->button;
    if(!dmp->desktop_menu) {
        g_critical("dmp->desktop_menu is NULL - module load failed?");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
    }
    
    if(xfce_desktop_menu_need_update(dmp->desktop_menu))
        xfce_desktop_menu_force_regen(dmp->desktop_menu);

    menu = xfce_desktop_menu_get_widget(dmp->desktop_menu);
    if(menu) {
        g_signal_connect(menu, "deactivate", 
                         G_CALLBACK(menu_deactivated), dmp);
        if (!at_pointer)
          gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
        xfce_panel_plugin_register_menu (dmp->plugin, GTK_MENU(menu));
        gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
                       (GtkMenuPositionFunc)(at_pointer ? NULL : dmp_position_menu),
                       dmp, 1, gtk_get_current_event_time());
    } else
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
}

static gboolean
dmp_message_received(GtkWidget *w, 
                        GdkEventClient *evt, 
                        gpointer user_data)
{
    DMPlugin *dmp = user_data;
    GdkScreen *gscreen;
    GdkWindow *root;
    GtkWidget *button;

    button = dmp->button;
    gscreen = gtk_widget_get_screen (button);
    root = gdk_screen_get_root_window(gscreen);
    if(!xfdesktop_popup_grab_available(root, GDK_CURRENT_TIME))
    {
        g_critical("Unable to get keyboard/mouse grab.");
        return FALSE;
    }

    if(evt->data_format == 8) {
        if(strcmp(XFCE_MENU_MESSAGE, evt->data.b) == 0) {
            menu_activate(dmp, FALSE);
            return TRUE;
        }
        if(strcmp(XFCE_MENU_AT_POINTER_MESSAGE, evt->data.b) == 0) {
            menu_activate(dmp, TRUE);
            return TRUE;
        }
    }

    return FALSE;
}

static gboolean
dmp_popup(GtkWidget *w,
          GdkEventButton *evt,
          gpointer user_data)
{
    DMPlugin *dmp = user_data;
    
    if(evt->button != 1 || ((evt->state & GDK_CONTROL_MASK)
                            && !(evt->state & (GDK_MOD1_MASK|GDK_SHIFT_MASK
                                               |GDK_MOD4_MASK))))
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), FALSE);
        return FALSE;
    }

    menu_activate(dmp, FALSE);

    return TRUE;
}

static void
dmp_set_defaults(DMPlugin *dmp)
{
    dmp->use_default_menu = TRUE;
    dmp->menu_file = NULL;
    dmp->icon_file = g_strdup(DEFAULT_BUTTON_ICON);
    dmp->show_menu_icons = TRUE;
    dmp->button_title = g_strdup(_(DEFAULT_BUTTON_TITLE));
    dmp->show_button_title = TRUE;
}

static void
dmp_read_config(XfcePanelPlugin *plugin, DMPlugin *dmp)
{
    gchar *cfgfile;
    XfceRc *rcfile;
    const gchar *value;
    
    if(!(cfgfile = xfce_panel_plugin_lookup_rc_file(plugin))) {
        dmp_set_defaults(dmp);
        return;
    }
    
    rcfile = xfce_rc_simple_open(cfgfile, TRUE);
    g_free(cfgfile);
    
    if(!rcfile) {
        dmp_set_defaults(dmp);
        return;
    }
    
    dmp->use_default_menu = xfce_rc_read_bool_entry(rcfile, "use_default_menu",
                                                    TRUE);
    
    value = xfce_rc_read_entry(rcfile, "menu_file", NULL);
    if(value) {
        g_free(dmp->menu_file);
        dmp->menu_file = g_strdup(value);
    } else
        dmp->use_default_menu = TRUE;
    
    value = xfce_rc_read_entry(rcfile, "icon_file", NULL);
    if(value) {
        g_free(dmp->icon_file);
        dmp->icon_file = g_strdup(value);
    } else
        dmp->icon_file = g_strdup(DEFAULT_BUTTON_ICON);
    
    dmp->show_menu_icons = xfce_rc_read_bool_entry(rcfile, "show_menu_icons",
                                                   TRUE);
    
    value = xfce_rc_read_entry(rcfile, "button_title", NULL);
    if(value) {
        g_free(dmp->button_title);
        dmp->button_title = g_strdup(value);
    } else
        dmp->button_title = g_strdup(_(DEFAULT_BUTTON_TITLE));
    
    dmp->show_button_title = xfce_rc_read_bool_entry(rcfile,
                                                     "show_button_title",
                                                     TRUE);
    
    xfce_rc_close(rcfile);
}

static void
dmp_write_config(XfcePanelPlugin *plugin, DMPlugin *dmp)
{
    gchar *cfgfile;
    XfceRc *rcfile;
    
    if(!(cfgfile = xfce_panel_plugin_save_location(plugin, TRUE)))
        return;
    
    rcfile = xfce_rc_simple_open(cfgfile, FALSE);
    g_free(cfgfile);
    
    xfce_rc_write_bool_entry(rcfile, "use_default_menu", dmp->use_default_menu);
    xfce_rc_write_entry(rcfile, "menu_file", dmp->menu_file ? dmp->menu_file : "");
    xfce_rc_write_entry(rcfile, "icon_file", dmp->icon_file ? dmp->icon_file : "");
    xfce_rc_write_bool_entry(rcfile, "show_menu_icons", dmp->show_menu_icons);
    xfce_rc_write_entry(rcfile, "button_title", dmp->button_title ? dmp->button_title : "");
    xfce_rc_write_bool_entry(rcfile, "show_button_title", dmp->show_button_title);
    
    xfce_rc_close(rcfile);
}

static void
dmp_menu_file_set(GtkFileChooser *fc,
                  gpointer user_data)
{
    DMPlugin *dmp = user_data;

    g_free(dmp->menu_file);
    dmp->menu_file = gtk_file_chooser_get_filename(fc);
    if(!dmp->menu_file|| !g_file_test(dmp->menu_file, G_FILE_TEST_EXISTS)) {
        g_free(dmp->menu_file);
        dmp->menu_file = NULL;
        return;
    }

    if(dmp->desktop_menu) {
        const gchar *cur_file = xfce_desktop_menu_get_menu_file(dmp->desktop_menu);
        if(strcmp(dmp->menu_file, cur_file)) {
            gchar *path;
            xfce_desktop_menu_destroy(dmp->desktop_menu);
            path = dmp_get_real_path(dmp->menu_file);
            dmp->desktop_menu = xfce_desktop_menu_new(path, TRUE);
            g_free(path);
            if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dmp->icons_chk)))
                xfce_desktop_menu_set_show_icons(dmp->desktop_menu, FALSE);
        }
    }
}

static void
dmp_icon_file_set(GtkFileChooser *fc,
                  gpointer user_data)
{
    DMPlugin *dmp = user_data;

    g_free(dmp->icon_file);
    dmp->icon_file = gtk_file_chooser_get_filename(fc);

    if(dmp->icon_file && *dmp->icon_file)
        dmp_set_size(dmp->plugin, xfce_panel_plugin_get_size(dmp->plugin), dmp);
    else
        gtk_image_set_from_pixbuf(GTK_IMAGE(dmp->image), NULL);            
}

static void
icon_chk_cb(GtkToggleButton *w, gpointer user_data)
{
    DMPlugin *dmp = user_data;
    
    dmp->show_menu_icons = gtk_toggle_button_get_active(w);
    if(dmp->desktop_menu)
        xfce_desktop_menu_set_show_icons(dmp->desktop_menu, dmp->show_menu_icons);
}

static void
dmp_use_desktop_menu_toggled_cb(GtkToggleButton *tb, gpointer user_data)
{
    DMPlugin *dmp = user_data;
    
    if(gtk_toggle_button_get_active(tb)) {
        GtkWidget *hbox;
        
        dmp->use_default_menu = TRUE;
        
        hbox = g_object_get_data(G_OBJECT(tb), "dmp-child-hbox");
        gtk_widget_set_sensitive(hbox, FALSE);
        
        if(dmp->desktop_menu)
            xfce_desktop_menu_destroy(dmp->desktop_menu);
        dmp->desktop_menu = xfce_desktop_menu_new(NULL, TRUE);
    }
}

static void
dmp_use_custom_menu_toggled_cb(GtkToggleButton *tb, gpointer user_data)
{
    DMPlugin *dmp = user_data;
    
    if(gtk_toggle_button_get_active(tb)) {
        GtkWidget *hbox;
        
        dmp->use_default_menu = FALSE;
        
        hbox = g_object_get_data(G_OBJECT(tb), "dmp-child-hbox");
        gtk_widget_set_sensitive(hbox, TRUE);
        
        if(dmp->menu_file && g_file_test(dmp->menu_file, G_FILE_TEST_EXISTS)) {
            if(dmp->desktop_menu)
                xfce_desktop_menu_destroy(dmp->desktop_menu);
            dmp->desktop_menu = xfce_desktop_menu_new(dmp->menu_file, TRUE);
        }
    }
}

static gboolean
dmp_button_title_focus_out_cb(GtkWidget *w, GdkEventFocus *evt,
        gpointer user_data)
{
    DMPlugin *dmp = user_data;
    
    if(dmp->button_title)
        g_free(dmp->button_title);
    dmp->button_title = gtk_editable_get_chars(GTK_EDITABLE(w), 0, -1);
    
    gtk_tooltips_set_tip(dmp->tooltip, dmp->button, dmp->button_title, NULL);
    gtk_label_set_text(GTK_LABEL(dmp->label), dmp->button_title);
    dmp_set_size(dmp->plugin, xfce_panel_plugin_get_size(dmp->plugin), dmp);
    
    return FALSE;
}

#if 0
static void
dmp_edit_menu_clicked_cb(GtkWidget *w, gpointer user_data)
{
    DMPlugin *dmp = user_data;
    GError *err = NULL;
    const gchar *menu_file = NULL;
    gchar cmd[PATH_MAX];
    
    g_return_if_fail(dmp && dmp->desktop_menu);
    
    
    if(dmp->use_default_menu)
        g_snprintf(cmd, PATH_MAX, "%s/xfce4-menueditor", BINDIR);
    else {
        menu_file = xfce_desktop_menu_get_menu_file(dmp->desktop_menu);
        if(!menu_file)
            return;
        g_snprintf(cmd, PATH_MAX, "%s/xfce4-menueditor \"%s\"", BINDIR,
                   menu_file);
    }
    
    if(xfce_exec(cmd, FALSE, FALSE, NULL))
        return;
    
    if(dmp->use_default_menu)
        g_strlcpy(cmd, "xfce4-menueditor", PATH_MAX);
    else
        g_snprintf(cmd, PATH_MAX, "xfce4-menueditor \"%s\"", menu_file);
    
    if(!xfce_exec(cmd, FALSE, FALSE, &err)) {
        xfce_warn(_("Unable to launch xfce4-menueditor: %s"), err->message);
        g_error_free(err);
    }
}
#endif

static void
dmp_options_dlg_response_cb(GtkDialog *dialog, gint response, DMPlugin *dmp)
{
    gtk_widget_destroy(GTK_WIDGET(dialog));
    xfce_panel_plugin_unblock_menu(dmp->plugin);
    dmp_write_config(dmp->plugin, dmp);
}

static GtkWidget *
dmp_create_file_chooser_button(DMPlugin *dmp,
                               gboolean is_icon)
{
    GtkWidget *chooser;
    GtkFileFilter *filter;
    const gchar *title;
    
    if(is_icon)
        title = _("Select Icon");
    else
        title = _("Select Menu File");
    
    chooser = gtk_file_chooser_button_new(title, GTK_FILE_CHOOSER_ACTION_OPEN);

    if(is_icon) {
        gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(chooser),
                DATADIR "/pixmaps", NULL);
    } else {
        gchar *dir;

        dir = xfce_resource_save_location(XFCE_RESOURCE_CONFIG, "menus/",
                                          FALSE);
        if(dir) {
            gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(chooser),
                                                 dir, NULL);
            g_free(dir);
        }

        dir = xfce_resource_lookup(XFCE_RESOURCE_CONFIG, "menus/");
        if(dir) {
            gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(chooser),
                                                 dir, NULL);
            g_free(dir);
        }
    }

    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, _("All Files"));
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);
    gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(chooser), filter);

    filter = gtk_file_filter_new();
    if(is_icon) {
        gtk_file_filter_set_name(filter, _("Image Files"));
        gtk_file_filter_add_pixbuf_formats(filter);
    } else {
        gtk_file_filter_set_name(filter, _("Menu Files"));
        gtk_file_filter_add_pattern(filter, "*.menu");
    }
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);
    
#ifdef HAVE_LIBEXO
    if(is_icon)
        exo_gtk_file_chooser_add_thumbnail_preview(GTK_FILE_CHOOSER(chooser));
#endif

    gtk_widget_show(chooser);

    return chooser;
}

static void
dmp_create_options(XfcePanelPlugin *plugin, DMPlugin *dmp)
{
    GtkWidget *dlg, *topvbox, *vbox, *hbox, *frame, *frame_bin, *spacer;
    GtkWidget *label, *chk, *radio, *entry;

    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");
    
    xfce_panel_plugin_block_menu(plugin);
    
    dlg = xfce_titled_dialog_new_with_buttons(_("Xfce Menu"),
                        GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(plugin))),
                        GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
                        GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(dlg), 0);
    g_signal_connect(G_OBJECT(dlg), "response",
                     G_CALLBACK(dmp_options_dlg_response_cb), dmp);
    
    topvbox = gtk_vbox_new(FALSE, BORDER / 2);
    gtk_widget_show(topvbox);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), topvbox, TRUE, TRUE, 0);
    
    frame = xfce_create_framebox(_("Button"), &frame_bin);
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(topvbox), frame, FALSE, FALSE, 0);
    
    vbox = gtk_vbox_new(FALSE, BORDER / 2);
    gtk_widget_show(vbox);
    gtk_container_add(GTK_CONTAINER(frame_bin), vbox);
    
    hbox = gtk_hbox_new(FALSE, BORDER / 2);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), BORDER / 2);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    label = gtk_label_new_with_mnemonic(_("Button _title:"));
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    
    entry = gtk_entry_new();
    if(dmp->button_title)
        gtk_entry_set_text(GTK_ENTRY(entry), dmp->button_title);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
    gtk_widget_show(entry);
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(entry), "focus-out-event",
            G_CALLBACK(dmp_button_title_focus_out_cb), dmp);
    
    chk = gtk_check_button_new_with_mnemonic(_("_Show title in button"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk), dmp->show_button_title);
    gtk_widget_show(chk);
    gtk_box_pack_start(GTK_BOX(vbox), chk, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(chk), "toggled",
            G_CALLBACK(show_title_toggled_cb), dmp);
    
    frame = xfce_create_framebox(_("Menu File"), &frame_bin);
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(topvbox), frame, FALSE, FALSE, 0);
    
    vbox = gtk_vbox_new(FALSE, BORDER / 2);
    gtk_widget_show(vbox);
    gtk_container_add(GTK_CONTAINER(frame_bin), vbox);
    
    /* 2nd radio button's child hbox */
    hbox = gtk_hbox_new(FALSE, BORDER / 2);
    gtk_widget_show(hbox);
    
    radio = gtk_radio_button_new_with_mnemonic(NULL, _("Use default _desktop menu file"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), dmp->use_default_menu);
    gtk_widget_show(radio);
    gtk_box_pack_start(GTK_BOX(vbox), radio, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(radio), "toggled",
            G_CALLBACK(dmp_use_desktop_menu_toggled_cb), dmp);
    g_object_set_data(G_OBJECT(radio), "dmp-child-hbox", hbox);
    
    radio = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(radio),
            _("Use _custom menu file:"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), !dmp->use_default_menu);
    gtk_widget_show(radio);
    gtk_box_pack_start(GTK_BOX(vbox), radio, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(radio), "toggled",
            G_CALLBACK(dmp_use_custom_menu_toggled_cb), dmp);
    g_object_set_data(G_OBJECT(radio), "dmp-child-hbox", hbox);
    
    /* now pack in the child hbox */
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    spacer = gtk_alignment_new(0.5, 0.5, 1, 1);
    gtk_widget_show(spacer);
    gtk_box_pack_start(GTK_BOX(hbox), spacer, FALSE, FALSE, 0);
    gtk_widget_set_size_request(spacer, 16, -1);
    
    dmp->file_entry = dmp_create_file_chooser_button(dmp, FALSE);
    if(dmp->menu_file)
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dmp->file_entry), dmp->menu_file);
    else if(dmp->desktop_menu) {
        dmp->menu_file = g_strdup(xfce_desktop_menu_get_menu_file(dmp->desktop_menu));
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dmp->file_entry), dmp->menu_file);
    }
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), dmp->file_entry);
    gtk_widget_show(dmp->file_entry);
    gtk_box_pack_start(GTK_BOX(hbox), dmp->file_entry, TRUE, TRUE, 3);
    g_signal_connect(G_OBJECT(dmp->file_entry), "file-set",
                     G_CALLBACK(dmp_menu_file_set), dmp);
    
    gtk_widget_set_sensitive(hbox, !dmp->use_default_menu);
    
#if 0  /* we don't have a menu editor anymore... */
    spacer = gtk_alignment_new(0.5, 0.5, 1, 1);
    gtk_widget_show(spacer);
    gtk_box_pack_start(GTK_BOX(vbox), spacer, FALSE, FALSE, 0);
    gtk_widget_set_size_request(spacer, -1, 4);
    
    hbox = gtk_hbox_new(FALSE, BORDER / 2);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    btn = xfce_create_mixed_button(GTK_STOCK_EDIT, _("_Edit Menu"));
    gtk_widget_show(btn);
    gtk_box_pack_end(GTK_BOX(hbox), btn, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(btn), "clicked",
            G_CALLBACK(dmp_edit_menu_clicked_cb), dmp);
#endif
    
    frame = xfce_create_framebox(_("Icons"), &frame_bin);
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(topvbox), frame, FALSE, FALSE, 0);
    
    vbox = gtk_vbox_new(FALSE, BORDER / 2);
    gtk_widget_show(vbox);
    gtk_container_add(GTK_CONTAINER(frame_bin), vbox);
    
    hbox = gtk_hbox_new(FALSE, BORDER / 2);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    label = gtk_label_new_with_mnemonic(_("_Button icon:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    
    dmp->icon_entry = dmp_create_file_chooser_button(dmp, TRUE);
    if(dmp->icon_file)
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dmp->icon_entry), dmp->icon_file);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), dmp->icon_entry);
    gtk_widget_show(dmp->icon_entry);
    gtk_box_pack_start(GTK_BOX(hbox), dmp->icon_entry, TRUE, TRUE, 3);
    g_signal_connect(G_OBJECT(dmp->icon_entry), "file-set",
                     G_CALLBACK(dmp_icon_file_set), dmp);
    
    dmp->icons_chk = chk = gtk_check_button_new_with_mnemonic(_("Show _icons in menu"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk), dmp->show_menu_icons);
    gtk_widget_show(chk);
    gtk_box_pack_start(GTK_BOX(vbox), chk, FALSE, FALSE, BORDER / 2);
    g_signal_connect(G_OBJECT(chk), "toggled", G_CALLBACK(icon_chk_cb), dmp);
    
    gtk_widget_show(dlg);
}

#ifdef XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL_FULL
static gboolean
desktop_menu_plugin_preinit(int argc,
                            char **argv)
{
#ifdef HAVE_THUNAR_VFS
    g_thread_init(NULL);
#endif
    return TRUE;
}
#endif

static gboolean 
desktop_menu_plugin_check(GdkScreen *gscreen)
{
#ifdef CHECK_RUNNING_PLUGIN
    gchar selection_name[32];
    Atom selection_atom;
    
    g_snprintf(selection_name, 
               sizeof(selection_name), 
               XFCE_MENU_SELECTION"%d", 
               gdk_screen_get_number (gscreen));
    selection_atom = XInternAtom(GDK_DISPLAY(), selection_name, False);

    if(XGetSelectionOwner(GDK_DISPLAY(), selection_atom)) {
        xfce_info(_("There is already a panel menu registered for this screen"));
        return FALSE;
    }
#endif

    return TRUE;
}

static gboolean
dmp_set_selection(DMPlugin *dmp)
{
    GdkScreen *gscreen;
    gchar selection_name[32];
    Atom selection_atom;
    GtkWidget *win;
    Window xwin;

    win = gtk_invisible_new();
    gtk_widget_realize(win);
    xwin = GDK_WINDOW_XID(GTK_WIDGET(win)->window);

    gscreen = gtk_widget_get_screen (win);
    g_snprintf(selection_name, 
               sizeof(selection_name), 
               XFCE_MENU_SELECTION"%d", 
               gdk_screen_get_number (gscreen));
    selection_atom = XInternAtom(GDK_DISPLAY(), selection_name, False);

    if(XGetSelectionOwner(GDK_DISPLAY(), selection_atom)) {
        gtk_widget_destroy (win);
        return FALSE;
    }

    XSelectInput(GDK_DISPLAY(), xwin, PropertyChangeMask);
    XSetSelectionOwner(GDK_DISPLAY(), selection_atom, xwin, GDK_CURRENT_TIME);

    g_signal_connect(G_OBJECT(win), "client-event",
                     G_CALLBACK(dmp_message_received), dmp);
    
    return TRUE;
}

static DMPlugin *
dmp_new(XfcePanelPlugin *plugin)
{
    DMPlugin *dmp = g_new0(DMPlugin, 1);

    dmp->plugin = plugin;
    dmp_read_config(plugin, dmp);
    
    dmp->tooltip = gtk_tooltips_new();
    
    dmp->button = xfce_create_panel_toggle_button();
    gtk_widget_set_name(dmp->button, "xfce-menu-button");
    gtk_widget_show(dmp->button);
    gtk_tooltips_set_tip(dmp->tooltip, dmp->button, dmp->button_title, NULL);
    
    if(xfce_panel_plugin_get_orientation(plugin) == GTK_ORIENTATION_HORIZONTAL)
        dmp->box = gtk_hbox_new(FALSE, BORDER / 2);
    else
        dmp->box = gtk_vbox_new(FALSE, BORDER / 2);
    gtk_container_set_border_width(GTK_CONTAINER(dmp->box), 0);
    gtk_widget_show(dmp->box);
    gtk_container_add(GTK_CONTAINER(dmp->button), dmp->box);
    
    dmp->image = gtk_image_new();
    g_object_ref(G_OBJECT(dmp->image));
    gtk_widget_show(dmp->image);
    gtk_box_pack_start(GTK_BOX(dmp->box), dmp->image, TRUE, TRUE, 0);
    
    dmp->label = gtk_label_new(dmp->button_title);
    g_object_ref(G_OBJECT(dmp->label));
    if(dmp->show_button_title)
        gtk_widget_show(dmp->label);
    gtk_box_pack_start(GTK_BOX(dmp->box), dmp->label, TRUE, TRUE, 0);
    
    dmp->desktop_menu = xfce_desktop_menu_new(!dmp->use_default_menu
                                                ? dmp->menu_file : NULL,
                                              TRUE);
    if(dmp->desktop_menu) {
        xfce_desktop_menu_set_show_icons(dmp->desktop_menu,
                                         dmp->show_menu_icons);
        xfce_desktop_menu_start_autoregen(dmp->desktop_menu, 10);
    }
    g_signal_connect(G_OBJECT(dmp->button), "button-press-event",
            G_CALLBACK(dmp_popup), dmp);
    dmp_set_selection(dmp);

    return dmp;
}

static void
desktop_menu_plugin_construct(XfcePanelPlugin *plugin)
{
    DMPlugin *dmp;
#if 0
    GtkWidget *mi, *img;
#endif
    
    xfce_textdomain(GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");
    
#ifdef HAVE_THUNAR_VFS
    thunar_vfs_init();
#endif
    
    if(!(dmp = dmp_new(plugin)))
        exit(1);
    
    xfce_panel_plugin_add_action_widget(plugin, dmp->button);
    gtk_container_add(GTK_CONTAINER(plugin), dmp->button);
    
#if 0
    /* Add edit menu option to right click menu */
    img = gtk_image_new_from_stock(GTK_STOCK_EDIT, GTK_ICON_SIZE_MENU);
    gtk_widget_show(img);
    mi = gtk_image_menu_item_new_with_label(_("Edit Menu"));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    gtk_widget_show(mi);
    xfce_panel_plugin_menu_insert_item(plugin, GTK_MENU_ITEM(mi));
    g_signal_connect(G_OBJECT(mi), "activate", 
                     G_CALLBACK(dmp_edit_menu_clicked_cb), dmp);
#endif
    
    g_signal_connect(plugin, "free-data",
                     G_CALLBACK(dmp_free), dmp);
    g_signal_connect(plugin, "save",
                     G_CALLBACK(dmp_write_config), dmp);
    g_signal_connect(plugin, "configure-plugin",
                     G_CALLBACK(dmp_create_options), dmp);
    g_signal_connect(plugin, "size-changed",
                     G_CALLBACK(dmp_set_size), dmp);
    g_signal_connect(plugin, "orientation-changed",
                     G_CALLBACK(dmp_set_orientation), dmp);
    
    xfce_panel_plugin_menu_show_configure(plugin);
    
    dmp_set_size(plugin, xfce_panel_plugin_get_size(plugin), dmp);
}

#ifdef XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL_FULL
XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL_FULL(desktop_menu_plugin_construct,
                                         desktop_menu_plugin_preinit,
                                         desktop_menu_plugin_check)
#else
XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL_WITH_CHECK(desktop_menu_plugin_construct,
                                               desktop_menu_plugin_check)
#endif
