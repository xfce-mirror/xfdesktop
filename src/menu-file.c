/*
 *  menufile.[ch] - routines for parsing an xfdeskop menu xml file
 *
 *  Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *                     2003 Biju Chacko (botsie@users.sourceforge.net)
 *                     2004 Danny Milosavljevic <danny.milo@gmx.net>
 *                     2004 Brian Tarricone <bjt23@cornell.edu>
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

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <X11/X.h>
#include <X11/Xlib.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libxml/parser.h>

#include <libxfce4util/i18n.h>
#include <libxfce4util/util.h>

#include "menu-file.h"
#include "menu.h"
#include "menu-dentry.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Search path for menu.xml file */
#define SEARCHPATH	(SYSCONFDIR G_DIR_SEPARATOR_S "xfce4" G_DIR_SEPARATOR_S "%F.%L:"\
                         SYSCONFDIR G_DIR_SEPARATOR_S "xfce4" G_DIR_SEPARATOR_S "%F.%l:"\
                         SYSCONFDIR G_DIR_SEPARATOR_S "xfce4" G_DIR_SEPARATOR_S "%F")

static GList *menufiles_watch = NULL;
static gboolean using_system_menu = FALSE;

static MenuItem *
parse_node_attr (MenuItemType type, xmlDocPtr doc, xmlNodePtr cur, const char *path)
{
    MenuItem *mi = NULL;
    xmlChar *name = NULL;
    xmlChar *cmd = NULL;
    xmlChar *term = NULL;
    xmlChar *visible = NULL;
    xmlChar *ifile = NULL;
	xmlChar *snotify = NULL;

    visible = xmlGetProp (cur, "visible");
    if (visible && !xmlStrcmp (visible, (xmlChar *) "no"))
    {
        xmlFree (visible);
        return NULL;
    }

    mi = g_new0 (MenuItem, 1);

    name = xmlGetProp (cur, "name");
    if (name == NULL)
    {
        mi->path = g_build_path ("/", path, _("No Name"), NULL);
    }
    else
    {
		gchar *p;
		while((p=strchr(name, '/')))
			*p = ' ';
		mi->path = g_build_path ("/", path, name, NULL);
        if(type == MI_APP && g_hash_table_lookup(menu_entry_hash, mi->path)) {
			xmlFree(name);
			g_free(mi->path);
			g_free(mi);
			return NULL;
		} else
			g_hash_table_insert(menu_entry_hash, mi->path, GINT_TO_POINTER(1));
    }
    cmd = xmlGetProp (cur, "cmd");
    if (cmd)
    {
	/* I'm doing this so that I can do a g_free on it later */
        mi->cmd = g_strdup (cmd);
    }
    else
    {
        mi->cmd = NULL;
    }

    term = xmlGetProp (cur, "term");
    if (term && !xmlStrcmp (term, (xmlChar *) "yes"))
    {
        mi->term = TRUE;
    }
    else
    {
        mi->term = FALSE;
    }

    mi->type = type;

    ifile = xmlGetProp(cur, "icon");
    mi->icon = ifile;
    
	snotify = xmlGetProp(cur, "snotify");
	if(snotify && !xmlStrcmp(snotify, (const xmlChar *)"true"))
		mi->snotify = TRUE;
	
    /* clean up */
    if (visible)
        xmlFree (visible);

    if (name)
        xmlFree (name);

    if (cmd)
        xmlFree (cmd);

    if (term)
        xmlFree (term);
	
	if(snotify)
		xmlFree(snotify);

    return mi;
}

static gint
cmp_menu_paths(gconstpointer a, gconstpointer b)
{
	return g_strcasecmp(((MenuItem *)a)->path, ((MenuItem *)b)->path);
}

static GList *
parse_node_incl(xmlNodePtr cur, const char *path)
{
	GList *incl_data = NULL;
	xmlChar *type, *file = NULL, *style = NULL, *unique = NULL;
	xmlChar *vis = NULL, *legacy = NULL;
	gchar *fullfile = NULL;
	gboolean do_legacy = TRUE;
	
	vis = xmlGetProp(cur, "visible");
	if(vis && !xmlStrcmp(vis, (const xmlChar *)"false"))
		goto cleanup;
	
	type = xmlGetProp(cur, "type");
	if(!type)
		goto cleanup;
	
	if(!xmlStrcmp(type, (const xmlChar *)"file")) {
		file = xmlGetProp(cur, "src");
		if(file) {
			if(*file != '/')
				fullfile = g_build_filename(xfce_get_homedir(), ".xfce4",
						file, NULL);
			else
				fullfile = g_strdup(file);
			
			if(g_file_test(fullfile, G_FILE_TEST_EXISTS))
				incl_data = menu_file_parse(fullfile, path);
		}
	} else if(!xmlStrcmp(type, (const xmlChar *)"system")) {
		style = xmlGetProp(cur, "style");
		unique = xmlGetProp(cur, "unique");
		legacy = xmlGetProp(cur, "legacy");
		
		if(legacy && !xmlStrcmp(legacy, (const xmlChar *)"false"))
			do_legacy = FALSE;
		
		if(style && !xmlStrcmp(style, (const xmlChar *)"multilevel")) {
			if(unique && !xmlStrcmp(unique, (const xmlChar *)"false"))
				incl_data = menu_dentry_parse_files(path, MPATH_MULTI,
						do_legacy);
			else
				incl_data = menu_dentry_parse_files(path, MPATH_MULTI_UNIQUE,
						do_legacy);
		} else {
			if(unique && !xmlStrcmp(unique, (const xmlChar *)"false"))
				incl_data = menu_dentry_parse_files(path, MPATH_SIMPLE,
						do_legacy);
			else
				incl_data = menu_dentry_parse_files(path, MPATH_SIMPLE_UNIQUE,
						do_legacy);
		}
		
		if(style)
			xmlFree(style);
		if(unique)
			xmlFree(unique);
		if(legacy)
			xmlFree(legacy);
		
		/* FIXME: this is too slow, but i want autogenerated menus to be sorted */
		if(incl_data) {
			incl_data = g_list_sort(incl_data, cmp_menu_paths);
			using_system_menu = TRUE;
		}
	}
	
	cleanup:
	
	if(vis)
		xmlFree(vis);
	if(type)
		xmlFree(type);
	if(file)
		xmlFree(file);
	if(fullfile && !incl_data)
		g_free(fullfile);
	
	return incl_data;
}

static GList *
parse_menu_node (xmlDocPtr doc, xmlNodePtr parent, const char *path,
		 GList * menu_data)
{
    MenuItem *mi;
    xmlNodePtr cur;

    for (cur = parent->xmlChildrenNode; cur != NULL; cur = cur->next)
    {
	if ((!xmlStrcmp (cur->name, (const xmlChar *) "menu")))
	{
	    mi = parse_node_attr (MI_SUBMENU, doc, cur, path);
	    if (mi)
	    {
		menu_data = g_list_append (menu_data, mi);

		/* recurse */
		menu_data = parse_menu_node (doc, cur, mi->path, menu_data);
	    }
	}
	if ((!xmlStrcmp (cur->name, (const xmlChar *) "app")))
	{
	    mi = parse_node_attr (MI_APP, doc, cur, path);
	    if (mi)
	    {
		menu_data = g_list_append (menu_data, mi);
	    }
	}
	if ((!xmlStrcmp (cur->name, (const xmlChar *) "separator")))
	{
	    mi = parse_node_attr (MI_SEPARATOR, doc, cur, path);
	    if (mi)
	    {
		menu_data = g_list_append (menu_data, mi);
	    }
	}
	if ((!xmlStrcmp (cur->name, (const xmlChar *) "title")))
	{
	    mi = parse_node_attr (MI_TITLE, doc, cur, path);
	    if (mi)
	    {
		menu_data = g_list_append (menu_data, mi);
	    }
	}
	if ((!xmlStrcmp (cur->name, (const xmlChar *) "builtin")))
	{
	    mi = parse_node_attr (MI_BUILTIN, doc, cur, path);
	    if (mi)
	    {
		menu_data = g_list_append (menu_data, mi);
	    }
	}
	if(!xmlStrcmp(cur->name, (const xmlChar *)"include")) {
		GList *incl_data = parse_node_incl(cur, path);
		if(incl_data)
			menu_data = g_list_concat(menu_data, incl_data);
	}
    }

    g_assert (menu_data);

    return (menu_data);
}

GList *
menu_file_parse(const char *filename, const char *basepath)
{
    xmlDocPtr doc;
    xmlNodePtr cur;
	xmlChar *useicons = NULL;
    GList *menu_data = NULL, *li;
    int prevdefault;

    prevdefault = xmlSubstituteEntitiesDefault (1);

    /* Open xml menu definition File */
    doc = xmlParseFile (filename);

    xmlSubstituteEntitiesDefault (prevdefault);

    if (doc == NULL)
    {
	g_warning ("%s: Could not parse %s.\n", PACKAGE, filename);
	return NULL;
    }

    /* verify that it is not an empty file */
    cur = xmlDocGetRootElement (doc);
    if (cur == NULL)
    {
	g_warning ("%s: empty document: %s\n", PACKAGE, filename);
	xmlFreeDoc (doc);
	return NULL;
    }

    /* Verify that this file is actually related to xfdesktop */
    if (xmlStrcmp (cur->name, (const xmlChar *) "xfdesktop-menu"))
    {
	g_warning
	    ("%s: document '%s' of the wrong type, root node != xfdesktop-menu",
	     PACKAGE, filename);
	xmlFreeDoc (doc);
	return NULL;
    }
	
	useicons = xmlGetProp(cur, "showicons");
	if(useicons && !xmlStrcmp(useicons, (const xmlChar *)"false"))
		use_menu_icons = FALSE;
	else
		use_menu_icons = TRUE;
	if(useicons)
		xmlFree(useicons);
	
	if(basepath && menufiles_watch) {
		for(li = menufiles_watch; li; li = li->next)
			g_free(li->data);
		g_list_free(menufiles_watch);
		menufiles_watch = NULL;
	}
	
	using_system_menu = FALSE;
	
    if(basepath)
		menu_data = parse_menu_node (doc, cur, basepath, menu_data);
	else
		menu_data = parse_menu_node (doc, cur, "/", menu_data);

    /* clean up */
    xmlFreeDoc (doc);

	if(menu_data)
		menufiles_watch = g_list_prepend(menufiles_watch, g_strdup(filename));
	
    return menu_data;
}

gchar *
menu_file_get()
{
	gchar filename[PATH_MAX];
	const gchar *env = g_getenv("XFCE_DISABLE_USER_CONFIG");

	if(!env || !strcmp(env, "0")) {
		gchar *usermenu = xfce_get_userfile("menu.xml", NULL);
		if(g_file_test(usermenu, G_FILE_TEST_IS_REGULAR))
			return usermenu;
		g_free(usermenu);
	}

	if(xfce_get_path_localized(filename, PATH_MAX, SEARCHPATH,
			"menu.xml", G_FILE_TEST_IS_REGULAR))
	{
		is_using_system_rc = TRUE;
		return g_strdup(filename);
	}

    g_warning("%s: Could not locate a menu definition file", PACKAGE);

    return NULL;
}

gboolean
menu_file_need_update()
{
	static time_t mtime = 0;
	GList *l;
	struct stat st;
	char *filename;
	gboolean modified = FALSE;
	
	for(l=menufiles_watch; l; l=l->next) {
		filename = (char *)l->data;
		
		if(stat(filename, &st))
			menufiles_watch = g_list_remove(menufiles_watch, filename);
		else if(st.st_mtime > mtime) {
			mtime = st.st_mtime;
			modified = TRUE;
		}
	}
	
	if(using_system_menu)
		modified = (menu_dentry_need_update() ? TRUE : modified);
	
	return modified;
}
