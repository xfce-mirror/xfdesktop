/*  xfce4
 *  
 *  Copyright (C) 2004 Brian Tarricone <bjt23@cornell.edu>
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

#include <libxfcegui4/libxfcegui4.h>

#include "xfce-desktop.h"
#include "xfdesktop-common.h"
#include "settings_common.h"

enum {
    OPT_SHOWWL = 1,
#ifdef USE_DESKTOP_MENU
    OPT_SHOWDM,
#endif
#ifdef ENABLE_DESKTOP_ICONS
    OPT_ICONSSYSTEMFONT,
    OPT_ICONSICONSIZE,
    OPT_ICONSFONTSIZE,
#endif
};

/* globals */
static gboolean show_windowlist = TRUE;
#ifdef USE_DESKTOP_MENU
static gboolean show_desktopmenu = TRUE;
#endif
#ifdef ENABLE_DESKTOP_ICONS
static XfceDesktopIconStyle desktop_icon_style = XFCE_DESKTOP_ICON_STYLE_WINDOWS;
static gboolean desktop_icons_use_system_font = TRUE;
static guint desktop_icons_font_size = 12;  /* default, i guess */
static guint desktop_icons_icon_size = 32;  /* default */
#endif

static void
set_chk_option(GtkWidget *w, gpointer user_data)
{
    BackdropDialog *bd = (BackdropDialog *)user_data;
    guint opt;
    
    opt = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(w), "xfce-chknum"));
    switch(opt) {
        case OPT_SHOWWL:
            show_windowlist = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
            mcs_manager_set_int(bd->plugin->manager, "showwl", BACKDROP_CHANNEL,
                    show_windowlist ? 1 : 0);
            break;
#ifdef USE_DESKTOP_MENU
        case OPT_SHOWDM:
            show_desktopmenu = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
            mcs_manager_set_int(bd->plugin->manager, "showdm", BACKDROP_CHANNEL,
                    show_desktopmenu ? 1 : 0);
            break;
#endif
#ifdef ENABLE_DESKTOP_ICONS
        case OPT_ICONSSYSTEMFONT:
            desktop_icons_use_system_font = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
            mcs_manager_set_int(bd->plugin->manager,
                                "icons_use_system_font_size", BACKDROP_CHANNEL,
                                desktop_icons_use_system_font ? 1 : 0);
            gtk_widget_set_sensitive(bd->frame_sysfont,
                                     !desktop_icons_use_system_font);
            break;
#endif
        default:
            g_warning("xfdesktop menu: got invalid checkbox ID");
            return;
    }
    
    mcs_manager_notify(bd->plugin->manager, BACKDROP_CHANNEL);
}

static void
set_sbtn_option(GtkSpinButton *sbtn,
                gpointer user_data)
{
    BackdropDialog *bd = (BackdropDialog *)user_data;
    gint value = gtk_spin_button_get_value_as_int(sbtn);
    guint opt = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(sbtn),
                                                   "xfce-sbtnnum"));
    
    switch(opt) {
        case OPT_ICONSICONSIZE:
            desktop_icons_icon_size = value;
            mcs_manager_set_int(bd->plugin->manager, "icons_icon_size",
                                BACKDROP_CHANNEL, value);
            break;
        case OPT_ICONSFONTSIZE:
            desktop_icons_font_size = value;
            mcs_manager_set_int(bd->plugin->manager, "icons_font_size",
                                BACKDROP_CHANNEL, value);
            break;
        default:
            g_warning("got invalid sbtn ID");
            return;
    }
    
    mcs_manager_notify(bd->plugin->manager, BACKDROP_CHANNEL);
}

static void
set_di_option(GtkComboBox *combo,
              gpointer user_data)
{
    BackdropDialog *bd = (BackdropDialog *)user_data;
    
    desktop_icon_style = gtk_combo_box_get_active(combo);
    mcs_manager_set_int(bd->plugin->manager, "desktopiconstyle",
                        BACKDROP_CHANNEL, desktop_icon_style);
    mcs_manager_notify(bd->plugin->manager, BACKDROP_CHANNEL);
}

void
init_menu_settings(McsPlugin *plugin)
{
    McsSetting *setting;
    
    setting = mcs_manager_setting_lookup(plugin->manager, "showwl",
            BACKDROP_CHANNEL);
    if(setting)
        show_windowlist = setting->data.v_int == 0 ? FALSE : TRUE;
    else
        mcs_manager_set_int(plugin->manager, "showwl", BACKDROP_CHANNEL, 1);

#ifdef USE_DESKTOP_MENU
    setting = mcs_manager_setting_lookup(plugin->manager, "showdm",
            BACKDROP_CHANNEL);
    if(setting)
        show_desktopmenu = setting->data.v_int == 0 ? FALSE : TRUE;
    else
        mcs_manager_set_int(plugin->manager, "showdm", BACKDROP_CHANNEL, 1);
#endif
    
#ifdef ENABLE_DESKTOP_ICONS
    setting = mcs_manager_setting_lookup(plugin->manager, "desktopiconstyle",
            BACKDROP_CHANNEL);
    if(setting) {
        desktop_icon_style = setting->data.v_int;
# ifdef HAVE_THUNAR_VFS
        if(desktop_icon_style > XFCE_DESKTOP_ICON_STYLE_FILES)
# else
        if(desktop_icon_style > XFCE_DESKTOP_ICON_STYLE_WINDOWS)
# endif
            desktop_icon_style = XFCE_DESKTOP_ICON_STYLE_WINDOWS;
    } else
        mcs_manager_set_int(plugin->manager, "desktopiconstyle",
                            BACKDROP_CHANNEL, desktop_icon_style);
    
    setting = mcs_manager_setting_lookup(plugin->manager,
                                         "icons_use_system_font_size",
                                         BACKDROP_CHANNEL);
    if(setting)
        desktop_icons_use_system_font = setting->data.v_int ? TRUE : FALSE;
    else
        mcs_manager_set_int(plugin->manager, "icons_use_system_font_size",
                            BACKDROP_CHANNEL, 1);
    
    setting = mcs_manager_setting_lookup(plugin->manager, "icons_font_size",
                                         BACKDROP_CHANNEL);
    if(setting && setting->data.v_int > 0)
        desktop_icons_font_size =  setting->data.v_int;
    
    setting = mcs_manager_setting_lookup(plugin->manager, "icons_icon_size",
                                         BACKDROP_CHANNEL);
    if(setting && setting->data.v_int > 0)
        desktop_icons_icon_size = setting->data.v_int;
#endif
}

#if USE_DESKTOP_MENU
static void
_edit_menu_cb(GtkWidget *w, gpointer user_data)
{
    GError *err = NULL;
    
    if(!xfce_exec(BINDIR "/xfce4-menueditor", FALSE, FALSE, NULL)
            && !xfce_exec("xfce4-menueditor", FALSE, FALSE, &err))
    {
        xfce_warn(_("Unable to launch xfce4-menueditor: %s"), err->message);
        g_error_free(err);
    }
}
#endif

GtkWidget *
create_menu_page(BackdropDialog *bd)
{
    XfceKiosk *kiosk;
    GtkWidget *page, *vbox, *frame, *frame_bin, *chk;
#ifdef USE_DESKTOP_MENU
    GtkWidget *btn;
#endif
#ifdef ENABLE_DESKTOP_ICONS
    GtkWidget *combo, *sbtn, *lbl, *hbox;
#endif
    
    kiosk = xfce_kiosk_new("xfdesktop");
    
    page = gtk_vbox_new(FALSE, BORDER);
    
    add_spacer(GTK_BOX(page));
    
    frame = xfce_create_framebox(_("Menus"), &frame_bin);
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(page), frame, FALSE, FALSE, 0);
    
    vbox = gtk_vbox_new(FALSE, BORDER);
    gtk_widget_show(vbox);
    gtk_container_add(GTK_CONTAINER(frame_bin), vbox);
    
    chk = gtk_check_button_new_with_mnemonic(_("Show _window list on middle click"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk), show_windowlist);
    g_object_set_data(G_OBJECT(chk), "xfce-chknum", GUINT_TO_POINTER(OPT_SHOWWL));
    gtk_widget_show(chk);
    gtk_box_pack_start(GTK_BOX(vbox), chk, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(chk), "toggled", G_CALLBACK(set_chk_option), bd);
    
    if(!xfce_kiosk_query(kiosk, "CustomizeWindowlist"))
        gtk_widget_set_sensitive(chk, FALSE);
    
#ifdef USE_DESKTOP_MENU    
    chk = gtk_check_button_new_with_mnemonic(_("Show _desktop menu"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk), show_desktopmenu);
    g_object_set_data(G_OBJECT(chk), "xfce-chknum", GUINT_TO_POINTER(OPT_SHOWDM));
    gtk_widget_show(chk);
    gtk_box_pack_start(GTK_BOX(vbox), chk, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(chk), "toggled", G_CALLBACK(set_chk_option), bd);
    
    btn = xfce_create_mixed_button(GTK_STOCK_EDIT, _("_Edit Menu"));
    gtk_widget_show(btn);
    gtk_box_pack_start(GTK_BOX(vbox), btn, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(_edit_menu_cb), NULL);
    
    if(!xfce_kiosk_query(kiosk, "CustomizeDesktopMenu")) {
        gtk_widget_set_sensitive(chk, FALSE);
        gtk_widget_set_sensitive(btn, FALSE);
    }
#endif
    
#ifdef ENABLE_DESKTOP_ICONS
    frame = xfce_create_framebox(_("Desktop Icons"), &frame_bin);
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(page), frame, FALSE, FALSE, 0);
    
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(vbox);
    gtk_container_add(GTK_CONTAINER(frame_bin), vbox);
    
    combo = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("None"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Minimized application icons"));
# ifdef HAVE_THUNAR_VFS
    gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("File/launcher icons"));
# endif
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), desktop_icon_style);
    gtk_widget_show(combo);
    gtk_box_pack_start(GTK_BOX(vbox), combo, FALSE, FALSE, BORDER);
    g_signal_connect(G_OBJECT(combo), "changed", G_CALLBACK(set_di_option), bd);
    
    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    lbl = gtk_label_new_with_mnemonic(_("_Icon size:"));
    gtk_widget_show(lbl);
    gtk_box_pack_start(GTK_BOX(hbox), lbl, FALSE, FALSE, 0);
    
    sbtn = gtk_spin_button_new_with_range(8.0, 192.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(sbtn), desktop_icons_icon_size);
    g_object_set_data(G_OBJECT(sbtn), "xfce-sbtnnum",
                      GUINT_TO_POINTER(OPT_ICONSICONSIZE));
    gtk_widget_show(sbtn);
    gtk_box_pack_start(GTK_BOX(hbox), sbtn, FALSE, FALSE, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(lbl), sbtn);
    g_signal_connect(G_OBJECT(sbtn), "value-changed",
                     G_CALLBACK(set_sbtn_option), bd);
    
    chk = gtk_check_button_new_with_mnemonic(_("Use _system font size"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk),
                                 desktop_icons_use_system_font);
    g_object_set_data(G_OBJECT(chk), "xfce-chknum",
                      GUINT_TO_POINTER(OPT_ICONSSYSTEMFONT));
    gtk_widget_show(chk);
    gtk_box_pack_start(GTK_BOX(vbox), chk, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(chk), "toggled", G_CALLBACK(set_chk_option), bd);
    
    frame = xfce_create_framebox(NULL, &bd->frame_sysfont);
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    
    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_widget_show(hbox);
    gtk_container_add(GTK_CONTAINER(bd->frame_sysfont), hbox);
    
    lbl = gtk_label_new_with_mnemonic(_("Custom _font size:"));
    gtk_widget_show(lbl);
    gtk_box_pack_start(GTK_BOX(hbox), lbl, FALSE, FALSE, 0);
    
    sbtn = gtk_spin_button_new_with_range(4.0, 144.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(sbtn), desktop_icons_font_size);
    g_object_set_data(G_OBJECT(sbtn), "xfce-sbtnnum",
                      GUINT_TO_POINTER(OPT_ICONSFONTSIZE));
    gtk_widget_show(sbtn);
    gtk_box_pack_start(GTK_BOX(hbox), sbtn, FALSE, FALSE, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(lbl), sbtn);
    g_signal_connect(G_OBJECT(sbtn), "value-changed",
                     G_CALLBACK(set_sbtn_option), bd);
    
    gtk_widget_set_sensitive(bd->frame_sysfont, !desktop_icons_use_system_font);
    
    if(!xfce_kiosk_query(kiosk, "CustomizeDesktopIcons"))
        gtk_widget_set_sensitive(frame_bin, FALSE);
#endif
    
    xfce_kiosk_free(kiosk);
    
    return page;
}
