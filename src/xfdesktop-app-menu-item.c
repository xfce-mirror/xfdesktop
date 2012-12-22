/*
 * A GtkImageMenuItem subclass that handles menu items that are
 * intended to represent launchable applications.
 *
 * Copyright (c) 2004-2007,2009 Brian Tarricone <bjt23@cornell.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "xfdesktop-app-menu-item.h"

struct _XfdesktopAppMenuItem
{
    GtkImageMenuItem parent;

    GarconMenuItem *item;

    GtkWidget *accel_label;
};

typedef struct _XfdesktopAppMenuItemClass
{
	GtkImageMenuItemClass parent;
} XfdesktopAppMenuItemClass;

enum
{
    PROP_0,
    PROP_ITEM
};


static void xfdesktop_app_menu_item_set_property(GObject *object,
                                                 guint prop_id,
                                                 const GValue *value,
                                                 GParamSpec *pspec);
static void xfdesktop_app_menu_item_get_property(GObject *object,
                                                 guint prop_id,
                                                 GValue *value,
                                                 GParamSpec *pspec);
static void xfdesktop_app_menu_item_finalize(GObject *object);

static void xfdesktop_app_menu_item_changed(XfdesktopAppMenuItem *app_menu_item);

static void xfdesktop_app_menu_item_activate(XfdesktopAppMenuItem *app_menu_item);


G_DEFINE_TYPE(XfdesktopAppMenuItem, xfdesktop_app_menu_item, GTK_TYPE_IMAGE_MENU_ITEM)



static gboolean global_show_icons = TRUE;



static void
xfdesktop_app_menu_item_class_init(XfdesktopAppMenuItemClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;

    gobject_class->finalize = xfdesktop_app_menu_item_finalize;
    gobject_class->set_property = xfdesktop_app_menu_item_set_property;
    gobject_class->get_property = xfdesktop_app_menu_item_get_property;

    g_object_class_install_property(gobject_class, PROP_ITEM,
                                    g_param_spec_object("item", NULL, NULL,
                                                        GARCON_TYPE_MENU_ITEM,
                                                        G_PARAM_STATIC_STRINGS
                                                        | G_PARAM_READWRITE
                                                        | G_PARAM_CONSTRUCT_ONLY));
}

static void
xfdesktop_app_menu_item_init(XfdesktopAppMenuItem *app_menu_item)
{
    g_signal_connect(G_OBJECT(app_menu_item), "activate",
                     G_CALLBACK(xfdesktop_app_menu_item_activate), NULL);
}

static void
xfdesktop_app_menu_item_set_property(GObject *object,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
    XfdesktopAppMenuItem *app_menu_item = XFDESKTOP_APP_MENU_ITEM(object);

    switch(prop_id) {
        case PROP_ITEM:
            if(app_menu_item->item) {
                g_signal_handlers_disconnect_by_func(G_OBJECT(app_menu_item->item),
                     G_CALLBACK(xfdesktop_app_menu_item_changed), app_menu_item);
                g_object_unref(G_OBJECT(app_menu_item->item));
            }
            app_menu_item->item = g_value_dup_object(value);
            g_signal_connect_swapped(G_OBJECT(app_menu_item->item), "changed",
                                     G_CALLBACK(xfdesktop_app_menu_item_changed), app_menu_item);
            xfdesktop_app_menu_item_changed (app_menu_item);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
xfdesktop_app_menu_item_get_property(GObject *object,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
    XfdesktopAppMenuItem *app_menu_item = XFDESKTOP_APP_MENU_ITEM(object);

    switch(prop_id) {
        case PROP_ITEM:
            g_value_set_object(value, app_menu_item->item);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
xfdesktop_app_menu_item_finalize(GObject *object)
{
    XfdesktopAppMenuItem *app_menu_item = XFDESKTOP_APP_MENU_ITEM(object);

    g_return_if_fail(app_menu_item != NULL);

    if(app_menu_item->item) {
        g_signal_handlers_disconnect_by_func(G_OBJECT(app_menu_item->item),
                     G_CALLBACK(xfdesktop_app_menu_item_changed), app_menu_item);
        g_object_unref(G_OBJECT(app_menu_item->item));
    }

    G_OBJECT_CLASS(xfdesktop_app_menu_item_parent_class)->finalize(object);
}

static void
xfdesktop_app_menu_item_set_icon(XfdesktopAppMenuItem *app_menu_item)
{
    const gchar *icon_name;
    gint w, h, size;
    GdkPixbuf *pixbuf = NULL;
    GtkWidget *image = NULL;
    GtkIconTheme *icon_theme;
    gchar *p, *name = NULL;
    gchar *filename;

    icon_name = garcon_menu_item_get_icon_name(app_menu_item->item);
    icon_theme = gtk_icon_theme_get_default();

    if(G_LIKELY(icon_name)) {
        gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, &h);
        size = MIN(w, h);

        if(gtk_icon_theme_has_icon(icon_theme, icon_name))
            image = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_MENU);
        else {
            if (g_path_is_absolute(icon_name)) {
                pixbuf = gdk_pixbuf_new_from_file_at_scale(icon_name, w, h, TRUE, NULL);
            } else {
                /* try to lookup names like application.png in the theme */
                p = strrchr(icon_name, '.');
                if (p) {
                    name = g_strndup(icon_name, p - icon_name);
                    pixbuf = gtk_icon_theme_load_icon(icon_theme, name, size, 0, NULL);
                    g_free (name);
                    name = NULL;
                }

                /* maybe they point to a file in the pixbufs folder */
                if (G_UNLIKELY(pixbuf == NULL)) {
                    filename = g_build_filename("pixmaps", icon_name, NULL);
                    name = xfce_resource_lookup(XFCE_RESOURCE_DATA, filename);
                    g_free(filename);
                }

                if(name) {
                    pixbuf = gdk_pixbuf_new_from_file_at_scale(name, w, h, TRUE, NULL);
                    g_free(name);
                }
            }

            /* Turn the pixbuf into a gtk_image */
            if(G_LIKELY(pixbuf)) {
                image = gtk_image_new_from_pixbuf(pixbuf);
                g_object_unref(G_OBJECT(pixbuf));
            }
        }
    }

    if(!GTK_IS_IMAGE(image))
        image = gtk_image_new();

    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(app_menu_item), image);
}

static void
xfdesktop_app_menu_item_changed(XfdesktopAppMenuItem *app_menu_item)
{
    const gchar *label;
#if !GTK_CHECK_VERSION (2, 16, 0)
    GtkWidget   *child;
#endif

    g_return_if_fail(XFCE_IS_APP_MENU_ITEM(app_menu_item));
    g_return_if_fail(GARCON_IS_MENU_ITEM(app_menu_item->item));

    if(global_show_icons)
        xfdesktop_app_menu_item_set_icon(app_menu_item);

    label = garcon_menu_item_get_name(app_menu_item->item);
    if (G_UNLIKELY (label == NULL))
      label = "";

#if GTK_CHECK_VERSION(2, 16, 0)
    gtk_menu_item_set_label(GTK_MENU_ITEM(app_menu_item), label);
#else
    child = gtk_bin_get_child(GTK_BIN (app_menu_item));
    if (child == NULL) {
        child = gtk_accel_label_new(label);
        gtk_container_add(GTK_CONTAINER(app_menu_item), child);
        gtk_misc_set_alignment(GTK_MISC(child), 0.0, 0.5);
        gtk_accel_label_set_accel_widget(GTK_ACCEL_LABEL(child), GTK_WIDGET(app_menu_item));
        gtk_widget_show(child);
    } else {
        g_return_if_fail(GTK_IS_LABEL(child));
        gtk_label_set_text(GTK_LABEL(child), label);
    }
#endif
}

static void
xfdesktop_app_menu_item_append_quote (GString     *string,
                                      const gchar *unquoted)
{
  gchar *quoted;

  quoted = g_shell_quote(unquoted);
  g_string_append(string, quoted);
  g_free(quoted);
}

static gchar *
xfdesktop_app_menu_item_command(XfdesktopAppMenuItem *app_menu_item)
{
    GString *newstr;
    const gchar *p;
    const gchar *command;
    const gchar *var;
    gchar *uri, *filename;

    command = garcon_menu_item_get_command(app_menu_item->item);
    if(command == NULL)
        return NULL;

    newstr = g_string_sized_new(100);

    for(p = command; *p; ++p) {
        if('%' == *p) {
            ++p;
            switch(*p) {
                /* we don't care about these since we aren't passing filenames */
                case 'f':
                case 'F':
                case 'u':
                case 'U':
                /* these are all deprecated */
                case 'd':
                case 'D':
                case 'n':
                case 'N':
                case 'v':
                case 'm':
                    break;

                case 'i':
                    var = garcon_menu_item_get_icon_name(app_menu_item->item);
                    if(G_LIKELY(var)) {
                        g_string_append(newstr, "--icon ");
                        xfdesktop_app_menu_item_append_quote(newstr, var);
                    }
                    break;

                case 'c':
                    var = garcon_menu_item_get_name(app_menu_item->item);
                    if(G_LIKELY(var))
                        xfdesktop_app_menu_item_append_quote(newstr, var);
                    break;

                case 'k':
                    uri = garcon_menu_item_get_uri(app_menu_item->item);
                    if(G_LIKELY(uri)) {
                        filename = g_filename_from_uri(uri, NULL, NULL);
                        xfdesktop_app_menu_item_append_quote(newstr, filename);
                        g_free(filename);
                    }
                    g_free(uri);
                    break;

                case '%':
                    g_string_append_c(newstr, '%');
                    break;

                default:
                    g_warning("Invalid field code in Exec line: %%%c", *p);
                    break;
            }
        } else
            g_string_append_c(newstr, *p);
    }

    return g_string_free(newstr, FALSE);
}

static void
xfdesktop_app_menu_item_activate (XfdesktopAppMenuItem *app_menu_item)
{
   gchar *command;
   GError *error = NULL;

   command = xfdesktop_app_menu_item_command(app_menu_item);
   if (command == NULL)
       return;

   if(!xfce_spawn_command_line_on_screen(gtk_widget_get_screen(GTK_WIDGET(app_menu_item)),
                                         command,
                                         garcon_menu_item_requires_terminal(app_menu_item->item),
                                         garcon_menu_item_supports_startup_notification(app_menu_item->item),
                                         &error)) {
        g_warning("XfdesktopAppMenuItem: unable to spawn %s: %s",
                 command, error->message);
        g_error_free(error);
    }
}

GtkWidget *
xfdesktop_app_menu_item_new (GarconMenuItem *item)
{
    g_return_val_if_fail(GARCON_IS_MENU_ITEM(item), NULL);

    return g_object_new (XFDESKTOP_TYPE_APP_MENU_ITEM,
                         "item", item, NULL);
}


void
xfdesktop_app_menu_item_set_show_icon (gboolean show_icon)
{
    global_show_icons = show_icon;
}
