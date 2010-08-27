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

    gchar *name;
    gchar *command;
    gboolean needs_term;
    gboolean snotify;
    gchar *icon_name;
    gchar *icon_path;
    gboolean icon_set;
    
    gchar *command_expanded;
    gchar *dot_desktop_filename;
    
    GtkWidget *accel_label;
};

typedef struct _XfdesktopAppMenuItemClass
{
	GtkImageMenuItemClass parent;
} XfdesktopAppMenuItemClass;

enum
{
    PROP_ZERO = 0,
    PROP_TERM,
    PROP_CMD,
    PROP_ICON,
    PROP_LABEL,
    PROP_SNOTIFY,
    PROP_USE_UNDERLINE,
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

static void xfdesktop_app_menu_item_realize(GtkWidget *widget);

static void xfdesktop_app_menu_item_update_icon(XfdesktopAppMenuItem *app_menu_item);

static void _command_activate_cb(GtkMenuItem *menu_item,
                                 gpointer user_data);


G_DEFINE_TYPE(XfdesktopAppMenuItem, xfdesktop_app_menu_item, GTK_TYPE_IMAGE_MENU_ITEM)


static void
_style_set_cb(GtkWidget *w, GtkStyle *prev_style, gpointer user_data)
{
    GtkStyle *style;
    GList *children, *l;
    
    style = gtk_rc_get_style_by_paths(gtk_settings_get_default(),
                                      "GtkMenuItem", "GtkMenuItem",
                                      GTK_TYPE_IMAGE_MENU_ITEM);
    children = gtk_container_get_children(GTK_CONTAINER(w));
    for(l = children; l; l = l->next) {
        if(GTK_IS_WIDGET(l->data))
            gtk_widget_set_style(GTK_WIDGET(l->data), style);
    }
    g_list_free(children);
}

static void
_expand_percent_vars(XfdesktopAppMenuItem *app_menu_item)
{
    GString *newstr;
    gchar *p;
    
    g_return_if_fail(app_menu_item->command);
    
    newstr = g_string_sized_new(strlen(app_menu_item->command) + 1);
    
    for(p = app_menu_item->command; *p; ++p) {
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
                    if(G_LIKELY(app_menu_item->icon_name)) {
                        gchar *str = g_shell_quote(app_menu_item->icon_name);
                        g_string_append(newstr, "--icon ");
                        g_string_append(newstr, str);
                        g_free(str);
                    }
                    break;
                
                case 'c':
                    if(G_LIKELY(app_menu_item->name)) {
                        gchar *name_locale, *str;
                        gsize bread = 0;
                        GError *error = NULL;
                        
                        name_locale = g_locale_from_utf8(app_menu_item->name,
                                                         -1, &bread, NULL,
                                                         &error);
                        if(name_locale) {
                            str = g_shell_quote(name_locale);
                            g_string_append(newstr, str);
                            g_free(str);
                            g_free(name_locale);
                        } else {
                            g_warning("Invalid UTF-8 in Name at byte %u: %s",
                                      (guint)bread, error->message);
                        }
                    }
                    break;
                
                case 'k':
                    if(app_menu_item->dot_desktop_filename) {
                        gchar *str = g_shell_quote(app_menu_item->dot_desktop_filename);
                        g_string_append(newstr, str);
                        g_free(str);
                    }
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
        
    app_menu_item->command_expanded = newstr->str;
    g_string_free(newstr, FALSE);
}

static void
_command_activate_cb(GtkMenuItem *menu_item,
                     gpointer user_data)
{
    XfdesktopAppMenuItem *app_menu_item = XFDESKTOP_APP_MENU_ITEM(menu_item);
    
    g_return_if_fail(app_menu_item && app_menu_item->command);
    
    /* we do this here instead of in _new*() for 2 reasons:
     *   1. menu items that never get activated won't slow us down for no
     *      reason.
     *   2. we can't guarantee that the icon name or whatever (which can
     *      influence the result of _expand_percent_vars()) has been set
     *      when the command is first set.
     */
    if(!app_menu_item->command_expanded)
        _expand_percent_vars(app_menu_item);
    
    if(!xfce_spawn_command_line_on_screen(gtk_widget_get_screen(GTK_WIDGET(menu_item)),
                                          app_menu_item->command_expanded,
                                          app_menu_item->needs_term,
                                          app_menu_item->snotify, NULL))
    {
        g_warning("XfdesktopAppMenuItem: unable to spawn %s\n",
                  app_menu_item->command_expanded);
    }
}

static void
xfdesktop_app_menu_item_class_init(XfdesktopAppMenuItemClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;

    widget_class->realize = xfdesktop_app_menu_item_realize;
    
    gobject_class->finalize = xfdesktop_app_menu_item_finalize;
    gobject_class->set_property = xfdesktop_app_menu_item_set_property;
    gobject_class->get_property = xfdesktop_app_menu_item_get_property;
    
    g_object_class_install_property(gobject_class, PROP_TERM,
                                    g_param_spec_boolean("needs-term",
                                                         _("Needs terminal"), 
                                                         _("Whether or not the command needs a terminal to execute"),
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_WRITABLE));
    
    g_object_class_install_property(gobject_class, PROP_CMD,
                                    g_param_spec_string("command",
                                                        _("Command"),
                                                        _("The command to run when the item is clicked"),
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));
    
    g_object_class_install_property(gobject_class, PROP_ICON,
                                    g_param_spec_string("icon-name",
                                                        _("Icon name"),
                                                        _("The name of the themed icon to display next to the item"),
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));
    
    g_object_class_install_property(gobject_class, PROP_LABEL,
                                    g_param_spec_string("label",
                                                        _("Label"),
                                                        _("The label displayed in the item"),
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));
    
    g_object_class_install_property(gobject_class, PROP_SNOTIFY,
                                    g_param_spec_boolean("snotify",
                                                         _("Startup notification"),
                                                         _("Whether or not the app supports startup notification"),
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_WRITABLE));
    
    g_object_class_install_property(gobject_class, PROP_USE_UNDERLINE,
                                    g_param_spec_boolean("use-underline",
                                                         _("Use underline"),
                                                         _("Whether or not to use an underscore in the label as a keyboard mnemonic"),
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
xfdesktop_app_menu_item_init(XfdesktopAppMenuItem *app_menu_item)
{
    GtkWidget *accel_label;
    
    gtk_widget_add_events(GTK_WIDGET(app_menu_item),
                          GDK_STRUCTURE_MASK | GDK_SUBSTRUCTURE_MASK
                          | GDK_VISIBILITY_NOTIFY_MASK);
    
    g_signal_connect(G_OBJECT(app_menu_item), "activate",
                     G_CALLBACK(_command_activate_cb), NULL);
    g_signal_connect(G_OBJECT(app_menu_item), "style-set",
                     G_CALLBACK(_style_set_cb), NULL);
    
    app_menu_item->accel_label = accel_label = gtk_accel_label_new("");
    gtk_misc_set_alignment(GTK_MISC(accel_label), 0.0, 0.5);
    
    gtk_container_add(GTK_CONTAINER(app_menu_item), accel_label);
    gtk_accel_label_set_accel_widget(GTK_ACCEL_LABEL(accel_label),
                                     GTK_WIDGET(app_menu_item));
    gtk_widget_show(accel_label);
}

static void
xfdesktop_app_menu_item_set_property(GObject *object,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
    XfdesktopAppMenuItem *app_menu_item = XFDESKTOP_APP_MENU_ITEM(object);
    
    switch(prop_id) {
        case PROP_TERM:
            xfdesktop_app_menu_item_set_needs_term(app_menu_item,
                                                   g_value_get_boolean(value));
            break;
        case PROP_CMD:
            xfdesktop_app_menu_item_set_command(app_menu_item,
                                                g_value_get_string(value));
            break;
        case PROP_ICON:
            xfdesktop_app_menu_item_set_icon_name(app_menu_item,
                                                  g_value_get_string(value));
            break;
        case PROP_LABEL:
            xfdesktop_app_menu_item_set_name(app_menu_item,
                                             g_value_get_string(value));
            break;
        case PROP_SNOTIFY:
            xfdesktop_app_menu_item_set_startup_notification(app_menu_item,
                                                             g_value_get_boolean(value));
            break;
        case PROP_USE_UNDERLINE:
            gtk_label_set_use_underline(GTK_LABEL(app_menu_item->accel_label),
                                        g_value_get_boolean(value));
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
        case PROP_TERM:
            g_value_set_boolean(value, app_menu_item->needs_term);
            break;
        case PROP_CMD:
            g_value_set_string(value, app_menu_item->command);
            break;
        case PROP_ICON:
            g_value_set_string(value, app_menu_item->icon_name);
            break;
        case PROP_LABEL:
            g_value_set_string(value, app_menu_item->name);
            break;
        case PROP_SNOTIFY:
            g_value_set_boolean(value, app_menu_item->snotify);
            break;
        case PROP_USE_UNDERLINE:
            g_value_set_boolean(value,
                                gtk_label_get_use_underline(GTK_LABEL(app_menu_item->accel_label)));
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
    
    g_free(app_menu_item->name);
    g_free(app_menu_item->command);
    g_free(app_menu_item->icon_name);
    g_free(app_menu_item->icon_path);
    g_free(app_menu_item->command_expanded);
    g_free(app_menu_item->dot_desktop_filename);
    
    G_OBJECT_CLASS(xfdesktop_app_menu_item_parent_class)->finalize(object);
}

static void
xfdesktop_app_menu_item_realize(GtkWidget *widget)
{
    XfdesktopAppMenuItem *app_menu_item = XFDESKTOP_APP_MENU_ITEM(widget);

    GTK_WIDGET_CLASS(xfdesktop_app_menu_item_parent_class)->realize(widget);

    xfdesktop_app_menu_item_update_icon(app_menu_item);
}

static void
xfdesktop_app_menu_item_update_icon(XfdesktopAppMenuItem *app_menu_item)
{
    GtkWidget *img;

    if(!GTK_WIDGET_REALIZED(app_menu_item))
        return;

    img = gtk_image_menu_item_get_image(GTK_IMAGE_MENU_ITEM(app_menu_item));
    if(!img) {
        img = gtk_image_new();
        gtk_widget_show(img);
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(app_menu_item), img);
    } else
        gtk_image_clear(GTK_IMAGE(img));

    if(app_menu_item->icon_name) {
        GtkIconTheme *itheme = gtk_icon_theme_get_default();

        if(gtk_icon_theme_has_icon(itheme, app_menu_item->icon_name)) {
            gtk_image_set_from_icon_name(GTK_IMAGE(img), app_menu_item->icon_name,
                                         GTK_ICON_SIZE_MENU);
        }
    } else if(app_menu_item->icon_path) {
        GdkPixbuf *pix;
        gint w, h;

        gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, &h);
        
        pix = gdk_pixbuf_new_from_file_at_scale(app_menu_item->icon_path,
                                                w, h, TRUE, NULL);
        if(pix) {
            gtk_image_set_from_pixbuf(GTK_IMAGE(img), pix);
            g_object_unref(G_OBJECT(pix));
        }
    }
}

/**
 * xfdesktop_app_menu_item_new:
 * @returns: A new #XfdesktopAppMenuItem.
 *
 * Creates a new #XfdesktopAppMenuItem with an empty label.
 *
 * Since 4.1
 **/
GtkWidget *
xfdesktop_app_menu_item_new(void)
{
    return g_object_new(XFDESKTOP_TYPE_APP_MENU_ITEM, NULL);
}

/**
 * xfdesktop_app_menu_item_new_with_label:
 * @label: The text of the menu item.
 * @returns: A new #XfdesktopAppMenuItem.
 *
 * Creates a new #XfdesktopAppMenuItem containing a label.
 *
 * Since 4.1
 **/
GtkWidget *
xfdesktop_app_menu_item_new_with_label(const gchar *label)
{
    return g_object_new(XFDESKTOP_TYPE_APP_MENU_ITEM,
                        "label", label,
                        NULL);
}

/**
 * xfdesktop_app_menu_item_new_with_mnemonic:
 * @label: The text of the menu item, with an underscore in front of the
 *         mnemonic character.
 * @returns: A new #XfdesktopAppMenuItem.
 *
 * Creates a new #XfdesktopAppMenuItem containing a label. The label
 * will be created using gtk_label_new_with_mnemonic(), so underscores
 * in @label indicate the mnemonic for the menu item.
 *
 * Since 4.1
 **/
GtkWidget *
xfdesktop_app_menu_item_new_with_mnemonic(const gchar *label)
{
    return g_object_new(XFDESKTOP_TYPE_APP_MENU_ITEM,
                        "label", label,
                        "use-underline", TRUE,
                        NULL);
}

/**
 * xfdesktop_app_menu_item_new_with_command:
 * @label: The text of the menu item.
 * @command: The command associated with the menu item.
 * @returns: A new #XfdesktopAppMenuItem.
 *
 * Creates a new #XfdesktopAppMenuItem containing a label. The item's @activate
 * signal will be connected such that @command will run when it is clicked.
 *
 * Since 4.1
 **/
GtkWidget *
xfdesktop_app_menu_item_new_with_command(const gchar *label,
                                         const gchar *command)
{
    return g_object_new(XFDESKTOP_TYPE_APP_MENU_ITEM,
                        "label", label,
                        "command", command,
                        NULL);
}

/**
 * xfdesktop_app_menu_item_new_full:
 * @label: The text of the menu item.
 * @command: The command associated with the menu item.
 * @icon_filename: The filename of the icon.
 * @needs_term: TRUE if the application needs a terminal, FALSE if not.
 * @snotify: TRUE if the application supports startup notification, FALSE if
 *           not.
 * @returns: A new #XfdesktopAppMenuItem.
 *
 * Single-function interface to create an #XfdesktopAppMenuItem.  Has the effect of
 * calling xfdesktop_app_menu_item_new_with_label() followed by all the
 * xfdesktop_app_menu_item_set_*() functions.
 *
 * Since 4.1
 **/
GtkWidget *
xfdesktop_app_menu_item_new_full(const gchar *label,
                                 const gchar *command,
                                 const gchar *icon_filename,
                                 gboolean needs_term,
                                 gboolean snotify)
{
    return g_object_new(XFDESKTOP_TYPE_APP_MENU_ITEM,
                        "label", label,
                        "command", command,
                        "icon-name", icon_filename,
                        "needs-term", needs_term,
                        "snotify", snotify,
                        NULL);
}

#if 0
/**
 * xfdesktop_app_menu_item_new_from_desktop_entry:
 * @entry: An #XfceDesktopEntry describing the menu item to create.
 * @show_icon: Sets whether or not to show an icon in the menu item.
 * @returns: A new #XfdesktopAppMenuItem, or %NULL on error.
 *
 * Creates a new #XfdesktopAppMenuItem using parameters from the application
 * specified in a #XfceDesktopEntry object. This has the effect of calling
 * xfdesktop_app_menu_item_new_with_command(), xfdesktop_app_menu_item_set_needs_term(),
 * xfdesktop_app_menu_item_set_icon_name(), and
 * xfdesktop_app_menu_item_set_startup_notification().
 *
 * Since 4.1
 **/
GtkWidget *
xfdesktop_app_menu_item_new_from_desktop_entry(XfceDesktopEntry *entry,
                                               gboolean show_icon)
{
    XfdesktopAppMenuItem *app_menu_item;
    gchar *name = NULL, *cmd = NULL, *icon = NULL, *snotify = NULL;
    gchar *onlyshowin = NULL, *categories = NULL, *term = NULL;
    const gchar *dfile;
    
    g_return_val_if_fail(XFCE_IS_DESKTOP_ENTRY(entry), NULL);
    
    if(xfce_desktop_entry_get_string(entry, "OnlyShowIn", FALSE, &onlyshowin)
       || xfce_desktop_entry_get_string(entry, "Categories", FALSE,
                                        &categories))
    {
        if((onlyshowin && strstr(onlyshowin, "XFCE;"))
           || (categories && strstr(categories, "X-XFCE;")))
        {
            if(xfce_desktop_entry_has_translated_entry(entry, "GenericName")) {
                xfce_desktop_entry_get_string(entry, "GenericName", TRUE,
                                              &name);
            } else if(xfce_desktop_entry_has_translated_entry(entry, "Name")) {
                xfce_desktop_entry_get_string(entry, "Name", TRUE, &name);
            } else {
                xfce_desktop_entry_get_string(entry, "GenericName", FALSE,
                                              &name);
            }
        } else if(onlyshowin) {
            g_free(onlyshowin);
            g_free(categories);
            return NULL;
        }
        
        g_free(onlyshowin);
        g_free(categories);
    }
    
    app_menu_item = XFDESKTOP_APP_MENU_ITEM(xfdesktop_app_menu_item_new());
    
    if(!name && !xfce_desktop_entry_get_string(entry, "Name", TRUE, &name)) {
        gchar *tmp, *tmp1;
        
        tmp = g_filename_to_utf8(xfce_desktop_entry_get_file(entry), -1,
                                 NULL, NULL, NULL);
        if(!tmp)
            tmp = g_strdup(xfce_desktop_entry_get_file(entry));
            
        if((tmp1 = g_strrstr(tmp, ".desktop")))
            *tmp1 = 0;
        if((tmp1 = g_strrstr(tmp, "/")))
            tmp1++;
        else
            tmp1 = name;
        name = g_strdup(tmp1);
        g_free(tmp);
    }
    
    app_menu_item->name = name;
    
    if(!g_utf8_validate(name, -1, NULL)) {
        g_warning("XfdesktopAppMenuItem: 'name' failed utf8 validation for .desktop file '%s'",
                  xfce_desktop_entry_get_file(entry));
        gtk_widget_destroy(GTK_WIDGET(app_menu_item));
        return NULL;
    }
    
    gtk_label_set_text(GTK_LABEL(app_menu_item->accel_label),
                       app_menu_item->name);
    
    if(xfce_desktop_entry_get_string(entry, "Terminal", TRUE, &term)) {
        app_menu_item->needs_term = (*term == '1'
                                     || !g_ascii_strcasecmp(term, "true"));
        g_free(term);
    }
    
    if(xfce_desktop_entry_get_string(entry, "StartupNotify", TRUE, &snotify)) {
        app_menu_item->snotify = (*snotify == '1'
                                  || !g_ascii_strcasecmp(snotify, "true"));
        g_free(snotify);
    }
    
    if(!xfce_desktop_entry_get_string(entry, "Exec", TRUE, &cmd)) {
        gtk_widget_destroy(GTK_WIDGET(app_menu_item));
        return NULL;
    }

    /* remove quotes around the command (yes, people do that!) */
    if(cmd[0] == '"') {
        gint i;
        
        for(i = 1; cmd[i - 1] != '\0'; ++i) {
            if (cmd[i] != '"')
                cmd[i-1] = cmd[i];
            else {
                cmd[i-1] = cmd[i] = ' ';
                break;
            }
        }
    }

    app_menu_item->command = xfce_expand_variables(cmd, NULL);
    g_free(cmd);
    
    if(show_icon) {
        xfce_desktop_entry_get_string(entry, "Icon", TRUE, &icon);
        if(icon) {
            xfdesktop_app_menu_item_set_icon_name(app_menu_item, icon);
            g_free(icon);
        }
    }
    
    dfile = xfce_desktop_entry_get_file(entry);
    if(dfile)
        app_menu_item->dot_desktop_filename = g_strdup(dfile);
    
    return GTK_WIDGET(app_menu_item);
}
#endif

/**
 * xfdesktop_app_menu_item_set_name:
 * @app_menu_item: An #XfdesktopAppMenuItem.
 * @name: The name of the menu item the menu item.
 *
 * Sets @name as the displayed name of the #XfdesktopAppMenuItem.
 *
 * Since 4.1
 **/
void
xfdesktop_app_menu_item_set_name(XfdesktopAppMenuItem *app_menu_item,
                                 const gchar *name)
{
    g_return_if_fail(XFCE_IS_APP_MENU_ITEM(app_menu_item));
    
    if(app_menu_item->name)
        g_free(app_menu_item->name);
    
    app_menu_item->name = g_strdup(name);
    gtk_label_set_text(GTK_LABEL(app_menu_item->accel_label), name);
}

/**
 * xfdesktop_app_menu_item_set_icon_name:
 * @app_menu_item: an #XfdesktopAppMenuItem.
 * @filename: The filename of the icon.
 *
 * Sets the icon of the #XfdesktopAppMenuItem using the specified filename.  If
 * the filename doesn't have a full pathname, standard icon search paths
 * will be used.  If the filename doesn't have an extension, the best image
 * format found (if any) will be used.  If there is already an icon set, the
 * current one is freed, regardless if the icon is found or not.
 *
 * Since 4.1
 **/
void
xfdesktop_app_menu_item_set_icon_name(XfdesktopAppMenuItem *app_menu_item,
                                      const gchar *filename)
{
    g_return_if_fail(XFCE_IS_APP_MENU_ITEM(app_menu_item));

    g_free(app_menu_item->icon_name);
    app_menu_item->icon_name = NULL;
    g_free(app_menu_item->icon_path);
    app_menu_item->icon_path = NULL;

    if(filename) {
        if(g_path_is_absolute(filename))
            app_menu_item->icon_path = g_strdup(filename);
        else {
            gchar *p, *q;
            gsize len;

            /* yes, there are really broken .desktop files out there
             * messed up like this */

            /* first make sure we aren't a weird relative path */
            p = g_strrstr(filename, G_DIR_SEPARATOR_S);
            if(p)
                p++;
            else
                p = (gchar *)filename;

            len = strlen(p);

            /* now make sure we don't have an extension */
            q = g_strrstr(p, ".");
            if(q && (!strcmp(q, ".png") || !strcmp(q, ".svg")
                     || !strcmp(q, ".jpg") || !strcmp(q, ".gif")
                     || !strcmp(q, ".bmp")))
            {
                len -= strlen(q);
            }

            /* whatever's left... */
            if(p[0] && len)
                app_menu_item->icon_name = g_strndup(p, len);
        }
    }

    xfdesktop_app_menu_item_update_icon(app_menu_item);
}

/**
 * xfdesktop_app_menu_item_set_command:
 * @app_menu_item: An #XfdesktopAppMenuItem.
 * @command: The command to associate with the menu item.
 *
 * Sets @command as the command run when the #XfdesktopAppMenuItem is clicked.
 *
 * Since 4.1
 **/
void
xfdesktop_app_menu_item_set_command(XfdesktopAppMenuItem *app_menu_item,
                                    const gchar *command)
{
    g_return_if_fail(XFCE_IS_APP_MENU_ITEM(app_menu_item));
    
    if(app_menu_item->command)
        g_free(app_menu_item->command);

    app_menu_item->command = xfce_expand_variables(command, NULL);
}

/**
 * xfdesktop_app_menu_item_set_needs_term:
 * @app_menu_item: An #XfdesktopAppMenuItem.
 * @needs_term: TRUE if the application needs a terminal, FALSE if not.
 *
 * Sets whether or not the command executed by this #XfdesktopAppMenuItem requires
 * a terminal window to run.
 *
 * Since 4.1
 **/
void
xfdesktop_app_menu_item_set_needs_term(XfdesktopAppMenuItem *app_menu_item,
                                       gboolean needs_term)
{
    g_return_if_fail(XFCE_IS_APP_MENU_ITEM(app_menu_item));
    
    app_menu_item->needs_term = needs_term;
}

/**
 * xfdesktop_app_menu_item_set_startup_notification:
 * @app_menu_item: An #XfdesktopAppMenuItem.
 * @snotify: TRUE if the application supports startup notification, FALSE if
 *           not.
 *
 * Sets whether or not the application supports startup notification.
 *
 * Since 4.1
 **/
void
xfdesktop_app_menu_item_set_startup_notification(XfdesktopAppMenuItem *app_menu_item,
                                                 gboolean snotify)
{
    g_return_if_fail(XFCE_IS_APP_MENU_ITEM(app_menu_item));
    
    app_menu_item->snotify = snotify;
}

/**
 * xfdesktop_app_menu_item_get_name:
 * @app_menu_item: An #XfdesktopAppMenuItem.
 * @returns: A name/label string.
 *
 * Returns the current name/label set for the #XfdesktopAppMenuItem, or NULL.
 *
 * Since 4.1
 **/
G_CONST_RETURN gchar *
xfdesktop_app_menu_item_get_name(XfdesktopAppMenuItem *app_menu_item)
{
    g_return_val_if_fail(XFCE_IS_APP_MENU_ITEM(app_menu_item), NULL);
    
    return app_menu_item->name;
}

/**
 * xfdesktop_app_menu_item_get_icon_name:
 * @app_menu_item: An #XfdesktopAppMenuItem.
 * @returns: An icon name string.
 *
 * Returns the current icon name set for the #XfdesktopAppMenuItem, or NULL.
 *
 * Since 4.1
 **/
G_CONST_RETURN gchar *
xfdesktop_app_menu_item_get_icon_name(XfdesktopAppMenuItem *app_menu_item)
{
    g_return_val_if_fail(XFCE_IS_APP_MENU_ITEM(app_menu_item), NULL);
    
    if(app_menu_item->icon_name)
        return app_menu_item->icon_name;
    else
        return app_menu_item->icon_path;
}

/**
 * xfdesktop_app_menu_item_get_command:
 * @app_menu_item: An #XfdesktopAppMenuItem.
 * @returns: A command string.
 *
 * Returns the current command set for the #XfdesktopAppMenuItem, or NULL.
 *
 * Since 4.1
 **/
G_CONST_RETURN gchar *
xfdesktop_app_menu_item_get_command(XfdesktopAppMenuItem *app_menu_item)
{
    g_return_val_if_fail(XFCE_IS_APP_MENU_ITEM(app_menu_item), NULL);
    
    return app_menu_item->command;
}

/**
 * xfdesktop_app_menu_item_get_needs_term:
 * @app_menu_item: An #XfdesktopAppMenuItem.
 * @returns: TRUE if the item will spawn a terminal, FALSE if not.
 *
 * Checks whether or not the command executed by this #XfdesktopAppMenuItem requires
 * a terminal window to run.
 *
 * Since 4.1
 **/
gboolean
xfdesktop_app_menu_item_get_needs_term(XfdesktopAppMenuItem *app_menu_item)
{
    g_return_val_if_fail(XFCE_IS_APP_MENU_ITEM(app_menu_item), FALSE);
    
    return app_menu_item->needs_term;
}

/**
 * xfdesktop_app_menu_item_get_startup_notification:
 * @app_menu_item: An #XfdesktopAppMenuItem.
 * @returns: TRUE if the item supports startup notification, FALSE if not.
 *
 * Checks whether or not the command executed by this #XfdesktopAppMenuItem supports
 * startup notification.
 *
 * Since 4.1
 **/
gboolean
xfdesktop_app_menu_item_get_startup_notification(XfdesktopAppMenuItem *app_menu_item)
{
    g_return_val_if_fail(XFCE_IS_APP_MENU_ITEM(app_menu_item), FALSE);
    
    return app_menu_item->snotify;
}
