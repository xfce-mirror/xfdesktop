/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004-2008 Brian Tarricone <bjt23@cornell.edu>
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
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 *  Random portions taken from or inspired by the original xfdesktop for xfce4:
 *     Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *     Copyright (C) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4windowing/libxfce4windowing.h>

#include "windowlist.h"
#include "xfdesktop-common.h"

#define WLIST_MAXLEN 30

static gboolean show_windowlist = TRUE;
static gboolean wl_show_icons = TRUE;
static gboolean wl_show_ws_names = TRUE;
static gboolean wl_submenus = FALSE;
static gboolean wl_sticky_once = FALSE;
static gboolean wl_add_remove_options = TRUE;

static void
add_workspace(GtkWidget *w, gpointer data) {
    XfwWorkspaceGroup *group = XFW_WORKSPACE_GROUP(data);
    xfw_workspace_group_create_workspace(group, NULL, NULL);
}

static void
remove_workspace(GtkWidget *w, gpointer data) {
    XfwWorkspace *workspace = XFW_WORKSPACE(data);
    gint workspace_num = xfw_workspace_get_number(workspace);
    const gchar *workspace_name = xfw_workspace_get_name(workspace);
    XfwWorkspace *current_workspace = xfw_workspace_group_get_active_workspace(xfw_workspace_get_workspace_group(workspace));
    gint current_workspace_num = -1;
    const gchar *current_workspace_name = NULL;
    gchar *rm_label_short = NULL, *rm_label_long = NULL;

    if (current_workspace != NULL) {
        current_workspace_num = xfw_workspace_get_number(current_workspace);
        current_workspace_name = xfw_workspace_get_name(current_workspace);
    }

    if (workspace_name == NULL) {
        rm_label_short = g_strdup_printf(_("Remove Workspace %d"), xfw_workspace_get_number(workspace));
        if (current_workspace_num > -1) {
            rm_label_long = g_strdup_printf(_("Do you really want to remove workspace %d?\nNote: You are currently on workspace %d."),
                                            workspace_num, current_workspace_num);
        } else {
            rm_label_long = g_strdup_printf(_("Do you really want to remove workspace %d"),
                                            workspace_num);
        }
    } else {
        gchar *last_ws_name_esc = g_markup_escape_text(workspace_name, strlen(workspace_name));
        rm_label_short = g_strdup_printf(_("Remove Workspace '%s'"), last_ws_name_esc);
        if (current_workspace_name != NULL) {
            rm_label_long = g_strdup_printf(_("Do you really want to remove workspace '%s'?\nNote: You are currently on workspace '%s'."),
                                            last_ws_name_esc, current_workspace_name);
        } else {
            rm_label_long = g_strdup_printf(_("Do you really want to remove workspace '%s'?"), last_ws_name_esc);
        }
        g_free(last_ws_name_esc);
    }

    /* Popup a dialog box confirming that the user wants to remove a
     * workspace */
    if (xfce_dialog_confirm(NULL, NULL, _("Remove"), rm_label_long,
                           "%s", rm_label_short))
    {
        xfw_workspace_remove(workspace, NULL);
    }

    if (rm_label_short != NULL)
        g_free(rm_label_short);
    if (rm_label_long != NULL)
        g_free(rm_label_long);
}

static void
activate_window(GtkWidget *w, gpointer user_data)
{
    XfwWindow *xfw_window = user_data;

    if(!xfw_window_is_pinned(xfw_window)) {
        xfw_workspace_activate(xfw_window_get_workspace(xfw_window), NULL);
    }
    xfw_window_activate(xfw_window, gtk_get_current_event_time(), NULL);
}

static void
window_destroyed_cb(gpointer data, GObject *where_the_object_was)
{
    GtkWidget *mi = data;
    GtkWidget *menu = gtk_widget_get_parent(mi);

    if(mi && menu)
        gtk_container_remove(GTK_CONTAINER(menu), mi);
}

static void
mi_destroyed_cb(GtkWidget *object, gpointer user_data)
{
    g_object_weak_unref(G_OBJECT(user_data),
                        (GWeakNotify)window_destroyed_cb, object);
}

static void
menulist_set_label_flags(GtkWidget *widget, gpointer data)
{
    gboolean *done = data;
    if(*done)
        return;
    if(GTK_IS_LABEL(widget)) {
        GtkLabel *label = GTK_LABEL(widget);
        gtk_label_set_use_markup(label, TRUE);
        gtk_label_set_ellipsize(label, PANGO_ELLIPSIZE_MIDDLE);
        gtk_label_set_max_width_chars(label, 24);
        *done = TRUE;
    } else if(GTK_IS_CONTAINER (widget)) {
        gtk_container_forall(GTK_CONTAINER (widget), menulist_set_label_flags,
                             done);
    }
}


static GtkWidget *
menu_item_from_xfw_window(XfwWindow *xfw_window,
                          gint icon_width,
                          gint icon_height,
                          gint scale_factor)
{
    GtkWidget *mi, *img = NULL;
    gchar *title;
    GString *label;
    GdkPixbuf *icon, *tmp = NULL;
    gint w, h;
    gboolean truncated = FALSE;

    title = g_markup_escape_text(xfw_window_get_name(xfw_window), -1);
    if(!title)
        return NULL;

    label = g_string_new(title);
    g_free(title);

    if (xfw_window_is_active(xfw_window)) {
        g_string_prepend(label, "<b><i>");
        g_string_append(label, "</i></b>");
    }

    /* add some padding to the right */
    if(gtk_widget_get_default_direction() == GTK_TEXT_DIR_RTL)
        g_string_prepend(label, "  ");
    else
        g_string_append(label, "  ");

    if(wl_show_icons) {
        cairo_surface_t *surface = NULL;

        gint icon_size = icon_width > icon_height ? icon_width : icon_height;
        icon = xfw_window_get_icon(xfw_window, icon_size * scale_factor);
        if (icon != NULL) {
            w = gdk_pixbuf_get_width(icon);
            h = gdk_pixbuf_get_height(icon);
            if (w != icon_width * scale_factor || h != icon_height * scale_factor) {
                tmp = gdk_pixbuf_scale_simple(icon,
                                              icon_width * scale_factor,
                                              icon_height * scale_factor,
                                              GDK_INTERP_BILINEAR);
            }
        }

        if (icon != NULL && xfw_window_is_minimized(xfw_window)) {
            if( tmp == NULL)
                tmp = gdk_pixbuf_copy(icon);
            /* minimized window, fade out app icon */
            gdk_pixbuf_saturate_and_pixelate(tmp, tmp, 0.55, TRUE);
        }

        if(tmp) {
            surface = gdk_cairo_surface_create_from_pixbuf(tmp, scale_factor, NULL);
            g_object_unref(G_OBJECT(tmp));
        } else {
            surface = gdk_cairo_surface_create_from_pixbuf(icon, scale_factor, NULL);
        }

        if (surface != NULL) {
            img = gtk_image_new_from_surface(surface);
            cairo_surface_destroy(surface);
        }
    }

    mi = xfdesktop_menu_create_menu_item_with_markup(label->str, img);

    g_string_free(label, TRUE);

    gtk_container_forall(GTK_CONTAINER(mi), menulist_set_label_flags,
                         &truncated);

    return mi;
}

static void
set_label_color_insensitive(GtkWidget *lbl)
{
    GdkRGBA         fg_color;
    PangoAttrList  *attrs;
    PangoAttribute *foreground;

    g_return_if_fail(GTK_IS_LABEL(lbl));

    gtk_style_context_get_color(gtk_widget_get_style_context (lbl),
                                GTK_STATE_FLAG_INSENSITIVE,
                                &fg_color);

    attrs = pango_attr_list_new();
    foreground = pango_attr_foreground_new((guint16)(fg_color.red * G_MAXUINT16),
                                           (guint16)(fg_color.green * G_MAXUINT16),
                                           (guint16)(fg_color.blue * G_MAXUINT16));
    pango_attr_list_insert(attrs, foreground);
    gtk_label_set_attributes (GTK_LABEL(lbl), attrs);
    pango_attr_list_unref (attrs);
}

GtkMenuShell *
windowlist_populate(GtkMenuShell *menu, gint scale_factor)
{
    GtkMenuShell *top_menu = menu;
    GdkScreen *gscreen;
    GList *menu_children;
    XfwScreen *xfw_screen;
    XfwWorkspaceManager *workspace_manager;
    GList *groups;
    gint group_num = 0;
    gboolean has_multiple_groups = FALSE;
    gint w, h;

    if(!show_windowlist)
        return top_menu;

    if(gtk_widget_has_screen(GTK_WIDGET(menu)))
        gscreen = gtk_widget_get_screen(GTK_WIDGET(menu));
    else
        gscreen = gdk_display_get_default_screen(gdk_display_get_default());

    /* check to see if the menu is empty.  if not, add the windowlist to a
     * submenu */
    menu_children = gtk_container_get_children(GTK_CONTAINER(menu));
    if(menu_children) {
        GtkWidget *mi;

        GtkWidget *tmpmenu = gtk_menu_new();
        gtk_menu_set_screen(GTK_MENU(tmpmenu), gscreen);
        gtk_menu_set_reserve_toggle_size (GTK_MENU (tmpmenu), FALSE);

        mi = gtk_separator_menu_item_new();
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

        mi = gtk_menu_item_new_with_label(_("Window List"));
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

        gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), tmpmenu);
        menu = (GtkMenuShell *)tmpmenu;
        g_list_free(menu_children);
    }

    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, &h);

    xfw_screen = xfw_screen_get_default();
    workspace_manager = xfw_screen_get_workspace_manager(xfw_screen);
    groups = xfw_workspace_manager_list_workspace_groups(workspace_manager);
    if (g_list_nth(groups, 1) != NULL) {
        has_multiple_groups = TRUE;
    }

    for (GList *gl = groups; gl != NULL; gl = gl->next, ++group_num) {
        XfwWorkspaceGroup *group = XFW_WORKSPACE_GROUP(gl->data);
        XfwWorkspace *active_workspace = xfw_workspace_group_get_active_workspace(group);
        // Keep this around outside the next loop so we know the info about the last workspace
        XfwWorkspace *xfw_workspace = NULL;

        DBG("ws group num %d", group_num);

        for (GList *wl = xfw_workspace_group_list_workspaces(group);
             wl != NULL;
             wl = wl->next)
        {
            GtkWidget *submenu = (GtkWidget *)menu, *mi, *label;
            xfw_workspace = XFW_WORKSPACE(wl->data);

            DBG("ws num %d", xfw_workspace_get_number(xfw_workspace));

            if(wl_show_ws_names || wl_submenus) {
                const gchar *ws_name = xfw_workspace_get_name(xfw_workspace);
                gchar *ws_label = NULL;

                /* Workspace header */
                if(ws_name == NULL || *ws_name == '\0') {
                    gint ws_num = xfw_workspace_get_number(xfw_workspace);
                    if (has_multiple_groups) {
                        ws_label = g_strdup_printf(_("<b>Group %d, Workspace %d</b>"), group_num+1, ws_num+1);
                    } else {
                        ws_label = g_strdup_printf(_("<b>Workspace %d</b>"), ws_num+1);
                    }
                } else {
                    gchar *ws_name_esc = g_markup_escape_text(ws_name, strlen(ws_name));
                    ws_label = g_strdup_printf("<b>%s</b>", ws_name_esc);
                    g_free(ws_name_esc);
                }

                mi = gtk_menu_item_new_with_label(ws_label);
                g_free(ws_label);
                label = gtk_bin_get_child(GTK_BIN(mi));
                gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
                /* center the workspace header */
                gtk_label_set_xalign (GTK_LABEL(label), 0.5f);
                /* If it's not the active workspace, make the color insensitive */
                if(xfw_workspace != active_workspace) {
                    set_label_color_insensitive(label);
                }
                gtk_widget_show(mi);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                if(!wl_submenus) {
                    g_signal_connect_swapped(G_OBJECT(mi), "activate",
                                             G_CALLBACK(xfw_workspace_activate),
                                             xfw_workspace);
                }

                if(wl_submenus) {
                    submenu = gtk_menu_new();
                    gtk_menu_set_reserve_toggle_size (GTK_MENU (submenu), FALSE);
                    gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), submenu);
                }
            }

            for(GList *l = xfw_screen_get_windows_stacked(xfw_screen); l; l = l->next) {
                XfwWindow *xfw_window = XFW_WINDOW(l->data);

                if((xfw_window_get_workspace(xfw_window) != xfw_workspace
                    && (!xfw_window_is_pinned(xfw_window)
                        || (wl_sticky_once && xfw_workspace != active_workspace)))
                    || xfw_window_is_skip_pager(xfw_window)
                    || xfw_window_is_skip_tasklist(xfw_window))
                {
                    /* the window isn't on the current WS AND isn't sticky,
                     * OR,
                     * the window is set to skip the pager,
                     * OR,
                     * the window is set to skip the tasklist
                     */
                    continue;
                }

                mi = menu_item_from_xfw_window(xfw_window, w, h, scale_factor);
                if(!mi)
                    continue;

                if(xfw_workspace != active_workspace
                   && (!xfw_window_is_pinned(xfw_window)
                       || xfw_workspace != active_workspace))
                {
                    /* The menu item has a GtkBox of which one of the children
                     * is the label we want to modify */
                    GList *items = gtk_container_get_children(GTK_CONTAINER(gtk_bin_get_child(GTK_BIN(mi))));
                    GList *li;

                    for(li = items; li != NULL; li = li->next)
                    {
                        if(GTK_IS_LABEL(li->data))
                        {
                            set_label_color_insensitive(li->data);
                            break;
                        }
                    }
                }

                gtk_widget_show(mi);
                gtk_menu_shell_append(GTK_MENU_SHELL(submenu), mi);
                g_object_weak_ref(G_OBJECT(xfw_window),
                                  (GWeakNotify)window_destroyed_cb, mi);
                g_signal_connect(G_OBJECT(mi), "activate",
                                 G_CALLBACK(activate_window), xfw_window);
                g_signal_connect(G_OBJECT(mi), "destroy",
                                 G_CALLBACK(mi_destroyed_cb), xfw_window);
            }

            // FIXME: need to not add after last item
            if(!wl_submenus || wl_add_remove_options) {
                mi = gtk_separator_menu_item_new();
                gtk_widget_show(mi);
                gtk_menu_shell_append(GTK_MENU_SHELL(submenu), mi);
            }
        }

        if (wl_add_remove_options) {
            GtkWidget *img, *mi;

            if ((xfw_workspace_group_get_capabilities(group) & XFW_WORKSPACE_GROUP_CAPABILITIES_CREATE_WORKSPACE) != 0) {
                /* 'add workspace' item */
                if(wl_show_icons) {
                    img = gtk_image_new_from_icon_name("list-add", GTK_ICON_SIZE_MENU);
                    mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("_Add Workspace"), img);
                } else
                    mi = gtk_menu_item_new_with_mnemonic(_("_Add Workspace"));
                gtk_widget_show(mi);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                g_signal_connect(G_OBJECT(mi), "activate",
                                 G_CALLBACK(add_workspace), group);
            }

            if (xfw_workspace != NULL && (xfw_workspace_get_capabilities(xfw_workspace) & XFW_WORKSPACE_CAPABILITIES_REMOVE) != 0) {
                const gchar *ws_name = xfw_workspace_get_name(xfw_workspace);
                gint nworkspaces = xfw_workspace_group_get_workspace_count(group);
                gchar *rm_label = NULL;

                /* 'remove workspace' item */
                if (ws_name == NULL)
                    rm_label = g_strdup_printf(_("_Remove Workspace %d"), nworkspaces);
                else {
                    gchar *ws_name_esc = g_markup_escape_text(ws_name, strlen(ws_name));
                    rm_label = g_strdup_printf(_("_Remove Workspace '%s'"), ws_name_esc);
                    g_free(ws_name_esc);
                }
                if(wl_show_icons) {
                    img = gtk_image_new_from_icon_name("list-remove", GTK_ICON_SIZE_MENU);
                    mi = xfdesktop_menu_create_menu_item_with_mnemonic(rm_label, img);
                } else
                    mi = gtk_menu_item_new_with_mnemonic(rm_label);
                g_free(rm_label);
                if(nworkspaces == 1)
                    gtk_widget_set_sensitive(mi, FALSE);
                gtk_widget_show(mi);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
                g_signal_connect(G_OBJECT(mi), "activate",
                                 G_CALLBACK(remove_workspace), xfw_workspace);
            }
        }

        // Clear it out in case the next group has no workspaces
        xfw_workspace = NULL;
    }

    g_object_unref(xfw_screen);

    return top_menu;
}

static void
windowlist_settings_changed(XfconfChannel *channel,
                            const gchar *property,
                            const GValue *value,
                            gpointer user_data)
{
    if(!strcmp(property, "/windowlist-menu/show"))
        show_windowlist = G_VALUE_TYPE(value) ? g_value_get_boolean(value) : TRUE;
    else if(!strcmp(property, "/windowlist-menu/show-icons"))
        wl_show_icons = G_VALUE_TYPE(value) ? g_value_get_boolean(value) : TRUE;
    else if(!strcmp(property, "/windowlist-menu/show-workspace-names"))
        wl_show_ws_names = G_VALUE_TYPE(value) ? g_value_get_boolean(value) : TRUE;
    else if(!strcmp(property, "/windowlist-menu/show-submenus"))
        wl_submenus = G_VALUE_TYPE(value) ? g_value_get_boolean(value) : FALSE;
    else if(!strcmp(property, "/windowlist-menu/show-sticky-once"))
        wl_sticky_once = G_VALUE_TYPE(value) ? g_value_get_boolean(value) : FALSE;
    else if(!strcmp(property, "/windowlist-menu/show-add-remove-workspaces"))
        wl_add_remove_options = G_VALUE_TYPE(value) ? g_value_get_boolean(value) : TRUE;
}

void
windowlist_init(XfconfChannel *channel)
{
    if(channel) {
        show_windowlist = xfconf_channel_get_bool(channel,
                                                  "/windowlist-menu/show",
                                                  TRUE);

        wl_show_icons = xfconf_channel_get_bool(channel,
                                                "/windowlist-menu/show-icons",
                                                TRUE);

        wl_show_ws_names = xfconf_channel_get_bool(channel,
                                                   "/windowlist-menu/show-workspace-names",
                                                   TRUE);

        wl_submenus = xfconf_channel_get_bool(channel,
                                              "/windowlist-menu/show-submenus",
                                              FALSE);

        wl_sticky_once = xfconf_channel_get_bool(channel,
                                                 "/windowlist-menu/show-sticky-once",
                                                 FALSE);

        wl_add_remove_options = xfconf_channel_get_bool(channel,
                                                        "/windowlist-menu/show-add-remove-workspaces",
                                                        TRUE);

        g_signal_connect(G_OBJECT(channel), "property-changed",
                         G_CALLBACK(windowlist_settings_changed), NULL);
    }
}

void
windowlist_cleanup(void)
{
    /* notused */
}
