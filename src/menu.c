/*  xfce4
 *  
 *  Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *                    2003 Biju Chacko (botsie@users.sourceforge.net)
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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <X11/Xlib.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libxml/parser.h>

#include <libxfce4mcs/mcs-client.h>
#include <libxfce4util/debug.h>
#include <libxfce4util/i18n.h>
#include <libxfce4util/util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "main.h"
#include "menu.h"

/* max length window list menu items */
#define WLIST_MAXLEN 20

/* Search path for menu.xml file */
#define SEARCHPATH	(SYSCONFDIR G_DIR_SEPARATOR_S "xfce4" G_DIR_SEPARATOR_S "%F.%L:"\
                         SYSCONFDIR G_DIR_SEPARATOR_S "xfce4" G_DIR_SEPARATOR_S "%F.%l:"\
                         SYSCONFDIR G_DIR_SEPARATOR_S "xfce4" G_DIR_SEPARATOR_S "%F")

/* a bit hackish, but works well enough */
static gboolean is_using_system_rc = TRUE;

static NetkScreen *netk_screen = NULL;
static GList *MainMenuData;     /* TODO: Free this at some point */
static gboolean EditMode = FALSE;
static GtkItemFactory *ifactory = NULL; /* TODO: Is this really necessary? */

/*  User menu
 *  ---------
*/
typedef enum __MenuItemTypeEnum
{
    MI_APP,
    MI_SEPARATOR,
    MI_SUBMENU,
    MI_TITLE,
    MI_BUILTIN
}
MenuItemType;

typedef struct __MenuItemStruct
{
    MenuItemType type;          /* Type of Menu Item    */
    char *path;                 /* itemfactory path to item */
    char *cmd;                  /* shell cmd to execute */
    gboolean term;              /* execute in terminal  */
    GdkPixbuf *icon;            /* icon to display      */
}
MenuItem;

void remove_factory_item(MenuItem *mi, GtkItemFactory *ifact)
{
    TRACE("dummy");
    gtk_item_factory_delete_item(ifact, mi->path);
}

void free_menu_data(GList * menu_data)
{
    MenuItem *mi;
    GList *li;

    TRACE("dummy");
    for(li = menu_data; li; li = li->next)
    {
        mi = li->data;

        if(mi)
        {
            if(mi->path)
            {
                g_free(mi->path);
            }
            if(mi->cmd)
            {
                g_free(mi->cmd);
            }
            g_free(mi);
        }
    }
    g_list_free(menu_data);
    menu_data = NULL;
}

void do_exec(gpointer callback_data, guint callback_action, GtkWidget * widget)
{
    TRACE("dummy");
    g_spawn_command_line_async((char *)callback_data, NULL);
}

void do_term_exec(gpointer callback_data, guint callback_action,
                  GtkWidget * widget)
{
    char *cmd;

    TRACE("dummy");

    cmd = g_strconcat("xfterm4 -e ", (char *)callback_data, NULL);

    g_spawn_command_line_async(cmd, NULL);

    g_free(cmd);

    return;
}

void do_builtin(gpointer callback_data, guint callback_action, GtkWidget * widget)
{
    char *builtin = (char *)callback_data;

    TRACE("dummy");
    if (!strcmp(builtin,"edit")) 
    {
        EditMode = (EditMode ? FALSE : TRUE);

        /* Need to rebuild menu, so destroy the current one */
        g_list_foreach(MainMenuData, (GFunc)remove_factory_item, ifactory);
        free_menu_data(MainMenuData);
        ifactory = NULL;
        MainMenuData = NULL;
    }
    else if (!strcmp(builtin, "quit"))
    {
        quit();
    }
}

void do_edit(gpointer callback_data, guint callback_action, GtkWidget * widget)
{
    TRACE("dummy");
    EditMode = FALSE;

    /* Need to rebuild menu, so destroy the current one */
    g_list_foreach(MainMenuData, (GFunc)remove_factory_item, ifactory);
    free_menu_data(MainMenuData);
    ifactory = NULL;
    MainMenuData = NULL;
}

static gchar *
get_menu_file(void)
{
    gchar buffer[PATH_MAX + 1];
    char *filename = NULL;
    char *path = NULL;
    const char *env;

    TRACE("dummy");
    env = g_getenv("XFCE_DISABLE_USER_CONFIG");

    if(!env || strcmp(env, "0")) {
    
        filename = xfce_get_userfile("menu.xml", NULL);
        if(g_file_test(filename, G_FILE_TEST_EXISTS))
	{
	    is_using_system_rc = FALSE;
	    
            return filename;
	}
        else
	{
            g_free(filename);
	}
    }

    is_using_system_rc = TRUE;

    /* xfce_get_path_localized(buffer, sizeof(buffer), SEARCHPATH,
       "menu.xml", G_FILE_TEST_IS_REGULAR);*/
    
    path = g_build_filename (SYSCONFDIR, "xfce4", "menu.xml", NULL);
    filename = xfce_get_file_localized(path);
    g_free(path);

    if (filename)
	return filename;

    g_warning("%s: Could not locate a menu definition file", PACKAGE);

    return NULL;
}

MenuItem *parse_node_attr(MenuItemType type, xmlDocPtr doc, xmlNodePtr cur,
                          char *path)
{
    MenuItem *mi = NULL;
    xmlChar *name = NULL;
    xmlChar *cmd = NULL;
    xmlChar *term = NULL;
    xmlChar *visible = NULL;

    TRACE("dummy");

    visible = xmlGetProp(cur, "visible");
    if(visible && !xmlStrcmp(visible, (xmlChar *) "no"))
    {
        xmlFree(visible);
        return NULL;
    }

    mi = g_new(MenuItem, 1);

    name = xmlGetProp(cur, "name");
    if(name == NULL)
    {
        mi->path = g_build_path("/", path, "No Name", NULL);
    }
    else
    {
        mi->path = g_build_path("/", path, name, NULL);
    }

    cmd = xmlGetProp(cur, "cmd");
    if(cmd)
    {
        /* I'm doing this so that I can do a g_free on it later */
        mi->cmd = g_strdup(cmd);
    }
    else
    {
        mi->cmd = NULL;
    }

    term = xmlGetProp(cur, "term");
    if(term && !xmlStrcmp(term, (xmlChar *) "yes"))
    {
        mi->term = TRUE;
    }
    else
    {
        mi->term = FALSE;
    }

    mi->type = type;
    mi->icon = NULL;            /* TODO: Load a pixbuf from icon filename */

    /* clean up */
    if(visible)
        xmlFree(visible);

    if(name)
        xmlFree(name);

    if(cmd)
        xmlFree(cmd);

    if(term)
        xmlFree(term);

    return mi;
}

GList *parse_menu_node(xmlDocPtr doc, xmlNodePtr parent, char *path,
                       GList * menu_data)
{
    MenuItem *mi;
    xmlNodePtr cur;

    TRACE("dummy");

    for(cur = parent->xmlChildrenNode; cur != NULL; cur = cur->next)
    {
        if((!xmlStrcmp(cur->name, (const xmlChar *)"menu")))
        {
            mi = parse_node_attr(MI_SUBMENU, doc, cur, path);
            if(mi)
            {
                menu_data = g_list_append(menu_data, mi);

                /* recurse */
                menu_data = parse_menu_node(doc, cur, mi->path, menu_data);
            }
        }
        if((!xmlStrcmp(cur->name, (const xmlChar *)"app")))
        {
            mi = parse_node_attr(MI_APP, doc, cur, path);
            if(mi)
            {
                menu_data = g_list_append(menu_data, mi);
            }
        }
        if((!xmlStrcmp(cur->name, (const xmlChar *)"separator")))
        {
            mi = parse_node_attr(MI_SEPARATOR, doc, cur, path);
            if(mi)
            {
                menu_data = g_list_append(menu_data, mi);
            }
        }
        if((!xmlStrcmp(cur->name, (const xmlChar *)"title")))
        {
            mi = parse_node_attr(MI_TITLE, doc, cur, path);
            if(mi)
            {
                menu_data = g_list_append(menu_data, mi);
            }
        }
        if((!xmlStrcmp(cur->name, (const xmlChar *)"builtin")))
        {
            mi = parse_node_attr(MI_BUILTIN, doc, cur, path);
            if(mi)
            {
                menu_data = g_list_append(menu_data, mi);
            }
        }
    }

    g_assert(menu_data);

    return(menu_data);
}

GList *parse_menu_file(const char *filename)
{
    xmlDocPtr doc;
    xmlNodePtr cur;
    GList *menu_data = NULL;
    int prevdefault;
    
    TRACE("dummy");

    prevdefault = xmlSubstituteEntitiesDefault(1);
    
    /* Open xml menu definition File */
    doc = xmlParseFile(filename);
    
    xmlSubstituteEntitiesDefault(prevdefault);
    
    if(doc == NULL)
    {
        g_warning("%s: Could not parse %s.\n", PACKAGE, filename);
        return NULL;
    }

    /* verify that it is not an empty file */
    cur = xmlDocGetRootElement(doc);
    if(cur == NULL)
    {
        g_warning("%s: empty document: %s\n", PACKAGE, filename);
        xmlFreeDoc(doc);
        return NULL;
    }

    DBG("Root Element: %s\n", cur->name);

    /* Verify that this file is actually related to xfdesktop */
    if(xmlStrcmp(cur->name, (const xmlChar *)"xfdesktop-menu"))
    {
        g_warning("%s: document '%s' of the wrong type, root node != xfdesktop-menu",
                PACKAGE, filename);
        xmlFreeDoc(doc);
        return NULL;
    }
    menu_data = parse_menu_node(doc, cur, "/", menu_data);

    /* clean up */
    xmlFreeDoc(doc);

    return menu_data;
}

GtkItemFactoryEntry parse_item(MenuItem * item)
{
    GtkItemFactoryEntry t;

    DBG("%s (type=%d) (term=%d)\n", item->path, item->type, item->term);

    t.path = item->path;
    t.accelerator = NULL;
    t.callback_action = 1;      /* non-zero ! */

    /* disable for now
       t.extra_data = item->icon; */

    if (!EditMode)
    {
        switch (item->type)
        {
            case MI_APP:
                t.callback = (item->term ? do_term_exec : do_exec);
                t.item_type = "<Item>";
                break;
            case MI_SEPARATOR:
                t.callback = NULL;
                t.item_type = "<Separator>";
                break;
            case MI_SUBMENU:
                t.callback = NULL;
                t.item_type = "<Branch>";
                break;
            case MI_TITLE:
                t.callback = NULL;
                t.item_type = "<Title>";
                break;
            case MI_BUILTIN:
                t.callback = do_builtin;
                t.item_type = "<Item>";
                break;
            default:
                break;
        }
    }
    else
    {
        t.callback = do_edit;
        
        if (item->type == MI_SUBMENU)
            t.item_type = "<Branch>";
        else
            t.item_type = "<Item>";
        
        if (item->type == MI_SEPARATOR)
        {
            gchar *parent_menu;

            parent_menu = g_path_get_dirname(item->path);
            g_free(item->path);
            item->path = g_strconcat(parent_menu, "--- seperator ---", NULL);
            t.path = item->path;
            
            g_free(parent_menu);
        }
    }

    return t;
}

/* returns the menu widget */
static GtkWidget *create_desktop_menu(void)
{
    struct stat st;
    static char *filename = NULL;
    static time_t mtime = 0;

    TRACE("dummy");
    if (!filename || is_using_system_rc)
    {
	if (filename)
	    g_free(filename);

        filename = get_menu_file();
    }
    
    /* may have been removed */
    if (stat(filename, &st) < 0) 
    {
        if (filename)
        {
            g_free(filename);
        }
        filename = get_menu_file();
        mtime = 0;
    }

    /* Still no luck? Something got broken! */
    if (stat(filename, &st) < 0) 
    {
        if (filename)
        {
            g_free(filename);
        }
        return NULL;
    }
    
    if (!ifactory || !MainMenuData || mtime < st.st_mtime)
    {
        GtkItemFactoryEntry entry;
        MenuItem *item = NULL;
        GList *li, *menu_data = NULL;
        
        if (!ifactory)
            ifactory = gtk_item_factory_new(GTK_TYPE_MENU, "<popup>", NULL);

        if (MainMenuData)
        {
            g_list_foreach(MainMenuData, (GFunc)remove_factory_item, ifactory);
            free_menu_data(MainMenuData);
        }
        
        /*
         * TODO: Replace the following line with code  to call multiple
         * menu parsers and merge their content
         */
        menu_data = parse_menu_file(filename);
        if(menu_data == NULL)
        {
            g_warning("%s: Error parsing menu file %s\n", 
                      PACKAGE, filename);
        }

        for(li = menu_data; li; li = li->next)
        {
            /* parse current item */
            item = (MenuItem *) li->data;

            g_assert(item != NULL);

            entry = parse_item(item);

            if (!EditMode) 
            {
                gtk_item_factory_create_item(ifactory, &entry, item->cmd, 1);
            }
            else
            {
                gtk_item_factory_create_item(ifactory, &entry, item, 1);
            }
        }

        /* clean up */
        /* free_menu_data(menu_data); */
        /* Hmmm ... if you do this the menus don't work */
        /* Lets save it in a global var and worry about it later */
        MainMenuData = menu_data;
        mtime = st.st_mtime;
    }

    return gtk_item_factory_get_widget(ifactory, "<popup>");
}

/*  Window list menu
 *  ----------------
*/
static void activate_window(GtkWidget * item, NetkWindow * win)
{
    TRACE("dummy");
    netk_window_activate(win);
}

static void set_num_screens(gpointer num)
{
    static Atom xa_NET_NUMBER_OF_DESKTOPS = 0;
    XClientMessageEvent sev;
    int n;

    TRACE("dummy");
    if(!xa_NET_NUMBER_OF_DESKTOPS)
    {
        xa_NET_NUMBER_OF_DESKTOPS =
            XInternAtom(GDK_DISPLAY(), "_NET_NUMBER_OF_DESKTOPS", False);
    }

    n = GPOINTER_TO_INT(num);

    sev.type = ClientMessage;
    sev.display = GDK_DISPLAY();
    sev.format = 32;
    sev.window = GDK_ROOT_WINDOW();
    sev.message_type = xa_NET_NUMBER_OF_DESKTOPS;
    sev.data.l[0] = n;

    gdk_error_trap_push();

    XSendEvent(GDK_DISPLAY(), GDK_ROOT_WINDOW(), False,
               SubstructureNotifyMask | SubstructureRedirectMask,
               (XEvent *) & sev);

    gdk_flush();
    gdk_error_trap_pop();
}

static GtkWidget *create_window_list_item(NetkWindow * win)
{
    const char *name = NULL;
    GString *label;
    GtkWidget *mi;

    TRACE("dummy");
    if(netk_window_is_skip_pager(win) || netk_window_is_skip_tasklist(win))
        return NULL;

    if(!name)
        name = netk_window_get_name(win);

    label = g_string_new(name);

    if(label->len >= WLIST_MAXLEN)
    {
        g_string_truncate(label, WLIST_MAXLEN);
        g_string_append(label, " ...");
    }

    if(netk_window_is_minimized(win))
    {
        g_string_prepend(label, "[");
        g_string_append(label, "]");
    }

    mi = gtk_menu_item_new_with_label(label->str);

    g_string_free(label, TRUE);

    return mi;
}

static GtkWidget *create_windowlist_menu(void)
{
    int i, n;
    GList *windows, *li;
    GtkWidget *menu3, *mi, *label;
    NetkWindow *win;
    NetkWorkspace *ws, *aws;
    GtkStyle *style;

    TRACE("dummy");
    menu3 = gtk_menu_new();
    style = gtk_widget_get_style(menu3);

/*      mi = gtk_menu_item_new_with_label(_("Window list"));
    gtk_widget_set_sensitive(mi, FALSE);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);

    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);
*/
    windows = netk_screen_get_windows_stacked(netk_screen);
    n = netk_screen_get_workspace_count(netk_screen);
    aws = netk_screen_get_active_workspace(netk_screen);

    for(i = 0; i < n; i++)
    {
        char *ws_name;
        const char *realname;
        gboolean active;
        
        ws = netk_screen_get_workspace(netk_screen, i);
        realname = netk_workspace_get_name(ws);
        
        active = (ws == aws);

        if (realname)
        {
            ws_name = g_strdup_printf("<i>%s</i>", realname);
        }
        else
        {
            ws_name = g_strdup_printf("<i>%d</i>", i+1);
        }
        
        mi = gtk_menu_item_new_with_label(ws_name);
        g_free(ws_name);
        
        label = gtk_bin_get_child(GTK_BIN(mi));
        gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);

        if(active)
            gtk_widget_set_sensitive(mi, FALSE);

        g_signal_connect_swapped(mi, "activate",
                                 G_CALLBACK(netk_workspace_activate), ws);

        mi = gtk_separator_menu_item_new();
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);

        for(li = windows; li; li = li->next)
        {
            win = li->data;

	    /* sticky windows don;t match the workspace
	     * only show them on the active workspace */
            if(netk_window_get_workspace(win) != ws &&
	       !(active && netk_window_is_sticky(win)))
	    {
                continue;
	    }

            mi = create_window_list_item(win);

            if(!mi)
                continue;

            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);

            if(!active)
            {
                gtk_widget_modify_fg(gtk_bin_get_child(GTK_BIN(mi)),
                                     GTK_STATE_NORMAL,
                                     &(style->fg[GTK_STATE_INSENSITIVE]));
            }

            g_signal_connect(mi, "activate", G_CALLBACK(activate_window), win);
        }

        mi = gtk_separator_menu_item_new();
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);
    }

    mi = gtk_menu_item_new_with_label(_("Add workspace"));
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);
    g_signal_connect_swapped(mi, "activate",
                             G_CALLBACK(set_num_screens),
                             GINT_TO_POINTER(n + 1));

    mi = gtk_menu_item_new_with_label(_("Delete workspace"));
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);
    g_signal_connect_swapped(mi, "activate",
                             G_CALLBACK(set_num_screens),
                             GINT_TO_POINTER(n - 1));

    return menu3;
}

static gboolean button_scroll_event (GtkWidget *widget, GdkEventScroll *event)
{
    gint n, active;
    NetkWorkspace *ws = NULL;

    g_return_val_if_fail (event != NULL, FALSE);  

    n = netk_screen_get_workspace_count(netk_screen);
    active = netk_workspace_get_number(netk_screen_get_active_workspace(netk_screen));

    switch(event->direction)
    {
        case GDK_SCROLL_UP:
        case GDK_SCROLL_LEFT:
            if (active > 0)
            {
                ws = netk_screen_get_workspace(netk_screen, active - 1);
            }
            else
            {
                ws = netk_screen_get_workspace(netk_screen, n - 1);
            }
            netk_workspace_activate(ws);
            break;
        case GDK_SCROLL_DOWN:
        case GDK_SCROLL_RIGHT:
            if (active < n - 1)
            {
                ws = netk_screen_get_workspace(netk_screen, active + 1);
            }
            else
            {
                ws = netk_screen_get_workspace(netk_screen, 0);
            }
            netk_workspace_activate(ws);
            break;
        default:
            break;
    }

    return TRUE;
}

/*  Initialization and event handling
 *  ---------------------------------
*/
static gboolean button_press_event(GtkWidget * win, GdkEventButton * ev,
                                   gpointer data)
{
    static GtkWidget *menu1 = NULL;
    static GtkWidget *menu3 = NULL;

    TRACE("dummy");
    if(ev->button == 2 || (ev->button == 1 && ev->state & GDK_SHIFT_MASK))
    {
        if(menu3)
        {
            gtk_widget_destroy(menu3);
        }

        menu3 = create_windowlist_menu();

        gtk_menu_popup(GTK_MENU(menu3), NULL, NULL, NULL, NULL, 0, ev->time);

        return TRUE;
    }
    else if(ev->button == 3)
    {
        menu1 = create_desktop_menu();

        if (menu1)
        {
            gtk_menu_popup(GTK_MENU(menu1), NULL, NULL, NULL, NULL, 0, ev->time);
            return TRUE;
        }
    }

    return FALSE;
}

void menu_init(GtkWidget * window, NetkScreen * screen)
{
    TRACE("dummy");
    netk_screen = screen;

    g_signal_connect(window, "button-press-event",
                     G_CALLBACK(button_press_event), NULL);
    g_signal_connect(window, "scroll-event",
                     G_CALLBACK(button_scroll_event), NULL);
}

void menu_load_settings(McsClient * client)
{
    TRACE("dummy");
}

void add_menu_callback(GHashTable * ht)
{
    TRACE("dummy");
}
