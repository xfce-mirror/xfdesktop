/*
 *  menu-dentry.[ch] - routines for gathering .desktop entry data
 *
 *  Copyright (C) 2004 Danny Milosavljevic <danny.milo@gmx.net>
 *                2004 Brian Tarricone <bjt23@cornell.edu>
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

#include <gtk/gtk.h>

#include <libxml/parser.h>

#include <libxfce4util/i18n.h>
#include <libxfce4util/xfce-desktopentry.h>

#include "menu-dentry.h"
#include "menu.h"
#include "menuspec.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static const char *dentry_keywords [] = {
   "Name", "Comment", "Icon", "Hidden",
   "Categories", "OnlyShowIn", "Exec", "Terminal",
};

static char *dentry_paths[] = {
	"/usr/local/share/applications/",
	"/usr/share/applications/",
	"/usr/share/applications/kde/",
	"/usr/share/gnome/apps/",
};

/* this is to work around some abuses of the Exec= parameter, e.g.:
 * Exec=kmix -caption "%c" %i %m
 * *cough*KDE*cough*
 * this function frees the string you give it, and allocates a new one
 */
static gchar *
sanitise_dentry_cmd(gchar *cmd) {
	gchar *newstr, *p;
	
	if(!cmd || !strchr(cmd, '%') || !(p=strchr(cmd, ' ')))
		return cmd;
	*p = 0;
	
	newstr = g_malloc0(strlen(cmd)+1);
	strcpy(newstr, cmd);
	g_free(cmd);
	
	return newstr;
}

static gint
get_path_depth(const gchar *path)
{
	gchar *p;
	gint cnt = 0;
	
	for(p=strchr(path, '/'); p; p=strchr(p+1, '/'))
		cnt++;
	
	return cnt;
}

/* O(n^2).  dammit. */
static void
prune_generic_paths(GPtrArray *paths)
{
	gint i, j;
	GPtrArray *arr = g_ptr_array_sized_new(5);
	
	for(i=0; i<paths->len; i++) {
		gchar *comp = g_ptr_array_index(paths, i);
		for(j=0; j<paths->len; j++) {
			if(i == j)
				continue;
			if(strstr(comp, g_ptr_array_index(paths, j)) == comp)
				g_ptr_array_add(arr, g_ptr_array_index(paths, j));
		}
	}
	
	for(i=0; i<arr->len; i++)
		g_ptr_array_remove(paths, g_ptr_array_index(arr, i));
}

static GList *
ensure_path(const gchar *basepath, const gchar *path, GList *menu_data)
{
	MenuItem *mi;
	gchar *newpath, *p;
	
	p = (gchar *)path;
	while((p=strchr(p+1, '/'))) {
		gchar *tmppath = g_strdup(path);
		*(tmppath+(p-path)) = 0;
		menu_data = ensure_path(basepath, tmppath, menu_data);
		g_free(tmppath);
	}
	
	if(basepath) {
		if(basepath[strlen(basepath)-1] == '/' && *path == '/')
			newpath = g_strconcat(basepath, path+1, NULL);
		else if(basepath[strlen(basepath)-1] == '/' || *path == '/')
			newpath = g_strconcat(basepath, path, NULL);
		else
			newpath = g_strconcat(basepath, "/", path, NULL);
	} else
		newpath = g_strdup(path);
	
	if(!g_hash_table_lookup(menu_entry_hash, newpath)) {
		mi = g_new0(MenuItem, 1);
		mi->type = MI_SUBMENU;
		mi->path = newpath;
		menu_data = g_list_append(menu_data, mi);
		g_hash_table_insert(menu_entry_hash, mi->path, GINT_TO_POINTER(1));
	} else
		g_free(newpath);
	
	return menu_data;
}

static MenuItem *
parse_dentry_attr (MenuItemType type, XfceDesktopEntry *de, 
		gchar const *basepath, gchar const *path)
{
	MenuItem *mi;
	gchar *name = NULL;
	gchar *cmd = NULL;
	gchar *ifile = NULL;
	int term;
	
	mi = g_new0 (MenuItem, 1);
	mi->type = type;
	mi->icon = NULL;

	if (!xfce_desktop_entry_get_string (de, "Name", TRUE, &name)) {
		/* siigh.. */
		gchar *tmp, *tmp1;
		tmp = g_strdup (xfce_desktop_entry_get_file (de));
		if((tmp1 = g_strrstr(tmp, ".desktop")))
			*tmp1 = 0;
		if((tmp1 = g_strrstr(tmp, "/")))
			tmp1++;
		else
			tmp1 = name;
		name = g_strdup(tmp1);
		g_free(tmp);
	}
	
	term = 0;
	xfce_desktop_entry_get_int (de, "Terminal", &term);
	
	xfce_desktop_entry_get_string (de, "Icon", TRUE, &ifile);

	cmd = NULL;
	xfce_desktop_entry_get_string (de, "Exec", TRUE, &cmd);
	
	if(basepath) {
		gboolean free_bp = FALSE;
		if(basepath[strlen(basepath)-1] == '/') {
			basepath = g_strndup(basepath, strlen(basepath)-1);
			free_bp = TRUE;
		}
		if(*path == '/')
			mi->path = g_strdup_printf ("%s%s/%s", basepath, path, name);
		else
			mi->path = g_strdup_printf ("%s/%s/%s", basepath, path, name);
		if(free_bp)
			g_free((char *)basepath);
	} else
		mi->path = g_strdup_printf ("%s/%s", path, name);
	
	if(g_hash_table_lookup(menu_entry_hash, mi->path)) {
		g_free(mi->path);
		if(ifile)
			g_free(ifile);
		if(cmd)
			g_free(cmd);
		g_free(name);
		g_free(mi);
		return NULL;
	} else {
		gchar *p;
		while((p=strchr(name, '/')))
			*p = ' ';
		g_hash_table_insert(menu_entry_hash, mi->path, GINT_TO_POINTER(1));
	}
	
	if (cmd)
		mi->cmd = sanitise_dentry_cmd(cmd);
		
	if (term)
		mi->term = TRUE;
		
	mi->icon = ifile;

	g_free (name);
	
	return mi;
}

static GList *
parse_dentry (XfceDesktopEntry *de, GList *menu_data, const char *basepath,
		MenuPathType pathtype)
{
	gchar *categories, *hidden;
	gchar **catv;
	MenuItem *mi;
	gint i;
	GPtrArray *newpaths;
	gchar *name, *path;

	gchar *onlyshowin;
	gchar *tmp;

	categories = NULL;
	hidden = NULL;
	newpaths = NULL;
	onlyshowin = NULL;
	xfce_desktop_entry_get_string (de, "OnlyShowIn", FALSE, &onlyshowin);
	if (onlyshowin) {
		tmp = g_strdup_printf (";%s;", onlyshowin);
		g_free (onlyshowin);
		onlyshowin = tmp;
		tmp = NULL;
	}
	
	if (onlyshowin && !g_strrstr (onlyshowin, ";XFCE;")) {
		g_free (onlyshowin);
		return menu_data;
	}

	if (onlyshowin) {
		g_free (onlyshowin);
		onlyshowin = NULL;
	}
	
	xfce_desktop_entry_get_string(de, "Hidden", FALSE, &hidden);
	if(hidden && !g_strcasecmp(hidden, "true")) {
		g_free(hidden);
		return menu_data;
	}
	if (hidden)
		g_free(hidden);

	xfce_desktop_entry_get_string (de, "Categories", TRUE, &categories);
	
	/* hack: leave out items that look like they are KDE control panels */
	if(categories && strstr(categories, ";X-KDE-")) {
		g_free(categories);
		return menu_data;
	}
	
	if(pathtype==MPATH_SIMPLE || pathtype==MPATH_SIMPLE_UNIQUE)
		newpaths = menuspec_get_path_simple(categories);
	else if(pathtype==MPATH_MULTI || pathtype==MPATH_MULTI_UNIQUE)
		newpaths = menuspec_get_path_multilevel(categories);
	else
		return NULL;
	
	if(!newpaths)
		return NULL;
	
	if(pathtype == MPATH_SIMPLE_UNIQUE) {
		/* grab first of the most general */
		path = g_ptr_array_index(newpaths, 0);
		menu_data = ensure_path(basepath, path, menu_data);
		mi = parse_dentry_attr (MI_APP, de, basepath, path);
		if(mi)
			menu_data = g_list_append (menu_data, mi);
	} else if(pathtype == MPATH_MULTI_UNIQUE) {
		/* grab most specific */
		path = g_ptr_array_index(newpaths, newpaths->len-1);
		menu_data = ensure_path(basepath, path, menu_data);
		mi = parse_dentry_attr (MI_APP, de, basepath, path);
		if(mi)
			menu_data = g_list_append (menu_data, mi);
	} else {
		for(i=0; i < newpaths->len; i++)
			menu_data = ensure_path(basepath, g_ptr_array_index(newpaths, i),
					menu_data);
		if(pathtype == MPATH_MULTI)
			prune_generic_paths(newpaths);
		for(i=0; i < newpaths->len; i++) {
			path = g_ptr_array_index(newpaths, i);
			mi = parse_dentry_attr (MI_APP, de, basepath, path);
			if(mi)
				menu_data = g_list_append (menu_data, mi);
		}
	}

	if(categories)
		g_free(categories);
	
	return menu_data;
}

static GList *
parse_dentry_file (char const *filename, GList *menu_data, const char *basepath,
		MenuPathType pathtype) 
{
	XfceDesktopEntry *dentry;
	dentry = xfce_desktop_entry_new (filename, dentry_keywords,
			G_N_ELEMENTS (dentry_keywords));
	xfce_desktop_entry_parse (dentry);
	menu_data = parse_dentry (dentry, menu_data, basepath, pathtype);
	g_object_unref (G_OBJECT (dentry));
	return menu_data;
}

GList *
menu_dentry_parse_files(const char *basepath, MenuPathType pathtype)
{
	GList *menu_data = NULL;
	gint	i;
	gchar const *pathd;
	GDir *d;
	gchar const *n;
	gchar *s;
	gchar *catfile = g_build_filename(XFCEDATADIR, "xfce-registered-categories.xml", NULL);
#ifdef HAVE_GETENV
	const char *kdedir = getenv("KDEDIR");
#endif
	
	menuspec_parse_categories(catfile);
	g_free(catfile);
	for(i = 0; i < G_N_ELEMENTS (dentry_paths); i++) {
		pathd = dentry_paths[i];

		d = g_dir_open (pathd, 0, NULL);
		if (d) {
			while ((n = g_dir_read_name (d)) != NULL) {
				if (g_str_has_suffix (n, ".desktop")) {
					s = g_build_path ("/", pathd, n, NULL);
					menu_data = parse_dentry_file (s, menu_data, basepath,
							pathtype);
					g_free (s);
				}
			}
			g_dir_close (d);
			d = NULL;
		}
	}
#ifdef HAVE_GETENV
	if(kdedir) {
		gchar *kde_dentry_path = g_strdup_printf("%s/share/applications/kde/", kdedir);
		d = g_dir_open(kde_dentry_path, 0, NULL);
		if(d) {
			while((n=g_dir_read_name(d))) {
				if(g_str_has_suffix(n, ".desktop")) {
					s = g_build_path("/", kde_dentry_path, n, NULL);
					menu_data = parse_dentry_file(s, menu_data, basepath, pathtype);
					g_free(s);
				}
			}
			g_dir_close(d);
		}
		g_free(kde_dentry_path);
	}
#endif
	menuspec_free();
	
	return menu_data;
}

static time_t
dentry_mtimes[20] = {0};

gboolean
menu_dentry_need_update()
{
	gint i;
	gboolean modified;
	struct stat st;
	
	modified = FALSE;
	
	for(i = 0; i < G_N_ELEMENTS (dentry_mtimes) && i < G_N_ELEMENTS (dentry_paths); i++) {
		if(stat(dentry_paths[i], &st) == 0) {
			if (st.st_mtime > dentry_mtimes[i]) {
				dentry_mtimes[i] = st.st_mtime;
				modified = TRUE;
			}
		} else {
			if (dentry_mtimes[i] > 0) {
				/* was there, is gone now */
				modified = TRUE;
				dentry_mtimes[i] = 0;
			}
		}
	}
	
	return (modified);
}
