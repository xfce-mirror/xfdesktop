/*  xfce4
 *  
 *  Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *  		  2003 Biju Chako (botsie@myrealbox.com)
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

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <X11/Xlib.h>
#include <assert.h>
#include <libxml/parser.h>

#include "main.h"
#include "menu.h"
#include "debug.h"

static NetkScreen *netk_screen = NULL;
static GList *MainMenuData;     /* TODO: Free this at some point */

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
    char *path;                 /* path to item         */
    char *cmd;                  /* shell cmd to execute */
    gboolean term;              /* execute in terminal  */
    GdkPixbuf *icon;            /* icon to display      */
}
MenuItem;

void do_exec(gpointer callback_data, guint callback_action, GtkWidget * widget)
{
    g_spawn_command_line_async((char *)callback_data, NULL);
}

void do_term_exec(gpointer callback_data, guint callback_action,
                  GtkWidget * widget)
{
    char *cmd;
    static const char *term_cmd = NULL;

    if(!term_cmd)
    {
        term_cmd = g_getenv("TERMCMD");

        if(!term_cmd)
            term_cmd = "xterm";
    }

    cmd = g_strconcat(term_cmd, " -e ", (char *)callback_data, NULL);

    g_spawn_command_line_async(cmd, NULL);

    g_free(cmd);

    return;
}

char *get_menu_file(void)
{
    char *filename = NULL;
    const char *env = NULL;

    env = g_getenv("XFCE_DISABLE_USER_CONFIG");

    if(!env || strcmp(env, "0"))
    {
        filename =
            g_build_filename(g_get_home_dir(), ".xfce4", "menu.xml", NULL);
        if(g_file_test(filename, G_FILE_TEST_EXISTS))
        {
            return filename;

        }
	else
	{
	    g_free(filename);
	}
    }

    filename = g_build_filename(SYSCONFDIR, "xfce4", "menu.xml", NULL);
    if(g_file_test(filename, G_FILE_TEST_EXISTS))
    {
        return filename;
    }
    else
    {
	g_free(filename);
    }

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

    DBG("\n");

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

    DBG("\n");

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
    assert(menu_data);
    return menu_data;
}

GList *parse_menu_file(const char *filename)
{
    xmlDocPtr doc;
    xmlNodePtr cur;
    GList *menu_data = NULL;
    
    /* Open xml menu definition File */
    doc = xmlParseFile(filename);
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
        default:
            break;
    }

    return t;
}

void remove_factory_item(MenuItem *mi, GtkItemFactory *ifactory)
{
    gtk_item_factory_delete_item(ifactory, mi->path);
}

void free_menu_data(GList * menu_data)
{
    MenuItem *mi;
    GList *li;

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
}

/* returns the menu widget */
static GtkWidget *create_desktop_menu(void)
{
    struct stat st;
    static char *filename = NULL;
    static time_t ctime = 0;
    static GtkItemFactory *ifactory = NULL;

    if (!filename)
	filename = get_menu_file();
    
    /* may have been removed */
    if (!stat(filename, &st))
    {
	g_free(filename);
	filename = get_menu_file();
	ctime = 0;
    }

    if (!ifactory || !MainMenuData || ctime < st.st_ctime)
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
	    assert(item != NULL);
	    entry = parse_item(item);
	    gtk_item_factory_create_item(ifactory, &entry, item->cmd, 1);
	}

	/* clean up */
	/* free_menu_data(menu_data); */
	/* Hmmm ... if you do this the menus don't work */
	/* Lets save it in a global var and worry about it later */
	MainMenuData = menu_data;
	ctime = st.st_ctime;
    }

    return gtk_item_factory_get_widget(ifactory, "<popup>");
}

/*  Window list menu
 *  ----------------
*/
static void activate_window(GtkWidget * item, NetkWindow * win)
{
    netk_window_activate(win);
}

static void set_num_screens(gpointer num)
{
    static Atom xa_NET_NUMBER_OF_DESKTOPS = 0;
    XClientMessageEvent sev;
    int n;

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

    if(netk_window_is_skip_pager(win) || netk_window_is_skip_tasklist(win))
        return NULL;

    if(!name)
        name = netk_window_get_name(win);

    label = g_string_new(name);

    if(label->len >= 15)
    {
        g_string_truncate(label, 15);
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

    menu3 = gtk_menu_new();
    style = gtk_widget_get_style(menu3);

/*	mi = gtk_menu_item_new_with_label(_("Window list"));
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
        char ws_name[100];
        gboolean active;

        ws = netk_screen_get_workspace(netk_screen, i);

        active = (ws == aws);

        if(active)
        {
            sprintf(ws_name, "<i>%s %d</i>", _("Workspace"), i + 1);
        }
        else
        {
            sprintf(ws_name, "%s <i>%s %d</i>",
                    _("Go to"), _("Workspace"), i + 1);
        }

        mi = gtk_menu_item_new_with_label(ws_name);
        label = gtk_bin_get_child(GTK_BIN(mi));
        gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);

        if(active)
        {
            gtk_widget_set_sensitive(mi, FALSE);
        }
/*	    else
	{
	    gtk_widget_modify_fg(gtk_bin_get_child(GTK_BIN(mi)), 
				 GTK_STATE_NORMAL,
				 &(style->fg[GTK_STATE_INSENSITIVE]));
	}
*/
        g_signal_connect_swapped(mi, "activate",
                                 G_CALLBACK(netk_workspace_activate), ws);

        mi = gtk_separator_menu_item_new();
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);

        for(li = windows; li; li = li->next)
        {
            win = li->data;

            if(netk_window_get_workspace(win) != ws)
                continue;

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

/*  Initialization and event handling
 *  ---------------------------------
*/
static gboolean button_press_event(GtkWidget * win, GdkEventButton * ev,
                                   gpointer data)
{
    static GtkWidget *menu1 = NULL;
    static GtkWidget *menu3 = NULL;

    if(ev->button == 3 || (ev->button == 1 && ev->state & GDK_SHIFT_MASK))
    {
        if(menu3)
        {
            gtk_widget_destroy(menu3);
        }

        menu3 = create_windowlist_menu();

        gtk_menu_popup(GTK_MENU(menu3), NULL, NULL, NULL, NULL, 1, ev->time);

        return TRUE;
    }
    else if(ev->button == 1)
    {
        menu1 = create_desktop_menu();

        gtk_menu_popup(GTK_MENU(menu1), NULL, NULL, NULL, NULL, 1, ev->time);

        return TRUE;
    }

    return FALSE;
}

void menu_init(GtkWidget * window, NetkScreen * screen)
{
    netk_screen = screen;

    g_signal_connect(window, "button-press-event",
                     G_CALLBACK(button_press_event), NULL);
}

void menu_load_settings(McsClient * client)
{
}

void add_menu_callback(GHashTable * ht)
{
}
