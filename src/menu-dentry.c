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

static void menu_dentry_legacy_init();
G_INLINE_FUNC gboolean menu_dentry_legacy_need_update();
static GList *menu_dentry_legacy_add_all(GList *menu_data,
		const gchar *basepath,MenuPathType pathtype);

static const char *dentry_keywords [] = {
   "Name", "Comment", "Icon", "Hidden",
   "Categories", "OnlyShowIn", "Exec", "Terminal",
};

static char *dentry_paths[] = {
	"/usr/local/share/applications",
	"/usr/share/applications",
	"/opt/gnome/share/applications",
	"/opt/gnome2/share/applications",
	"/usr/share/applications/kde",
	NULL
};

/* these .desktop files _should_ have an OnlyShowIn key, but don't.  i'm going
 * to match by the Exec field.  FIXME: move into hashtable? */
static char *blacklist[] = {
	"gnome-control-center",
	"kmenuedit",
	"kcmshell",
	"kcontrol",
	"kpersonalizer",
	"kappfinder",
	NULL
};

static const gchar *legacy_dirs[] = {
	"/usr/share/gnome/apps",
	"/usr/local/share/gnome/apps",
	"/opt/gnome/share/apps",
	"/usr/share/applnk",
	"/usr/local/share/applnk",
	NULL
};

static time_t *dentry_mtimes = NULL;
static time_t *dentry_legacy_mtimes = NULL;
static GHashTable *dir_to_cat = NULL;


/* we don't want most command-line parameters if they're given. */
G_INLINE_FUNC gchar *
_sanitise_dentry_cmd(gchar *cmd) {
	gchar *p;
	
	/* this is the naive approach: if there's a '%' character in there, we're
	 * going to strip all parameters.  this may not be the best thing to do,
	 * but anything smarter is non-trivial and slow. */
	if(cmd && strchr(cmd, '%') && (p=strchr(cmd, ' ')))
		*p = 0;
	
	return cmd;
}

G_INLINE_FUNC gint
_get_path_depth(const gchar *path)
{
	gchar *p;
	gint cnt = 0;
	
	for(p=strchr(path, '/'); p; p=strchr(p+1, '/'))
		cnt++;
	
	return cnt;
}

/* O(n^2).  dammit. */
static void
_prune_generic_paths(GPtrArray *paths)
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
_ensure_path(const gchar *basepath, const gchar *path, GList *menu_data)
{
	MenuItem *mi;
	gchar *newpath, *p;
	
	p = (gchar *)path;
	while((p=strchr(p+1, '/'))) {
		gchar *tmppath = g_strdup(path);
		*(tmppath+(p-path)) = 0;
		menu_data = _ensure_path(basepath, tmppath, menu_data);
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

static gchar *
_build_path(const gchar *basepath, const gchar *path, const gchar *name)
{
	gchar *newpath = NULL;
	
	if(basepath) {
		gboolean free_bp = FALSE;
		
		if(basepath[strlen(basepath)-1] == '/') {
			basepath = g_strndup(basepath, strlen(basepath)-1);
			free_bp = TRUE;
		}
		if(*path == '/')
			newpath = g_strdup_printf ("%s%s/%s", basepath, path, name);
		else
			newpath = g_strdup_printf ("%s/%s/%s", basepath, path, name);
		if(free_bp)
			g_free((char *)basepath);
	} else
		newpath = g_strdup_printf ("%s/%s", path, name);
	
	return newpath;
}

static MenuItem *
menu_dentry_parse_dentry_attr(MenuItemType type, XfceDesktopEntry *de, 
		gchar const *basepath, gchar const *path)
{
	MenuItem *mi;
	gchar *name = NULL;
	gchar *cmd = NULL;
	gchar *ifile = NULL;
	gchar *p;
	int term;
	gint i;

	cmd = NULL;
	xfce_desktop_entry_get_string (de, "Exec", TRUE, &cmd);
	if(!cmd)
		return NULL;
	for(i=0; blacklist[i]; i++) {
		if(strstr(cmd, blacklist[i]) == cmd) {
			g_free(cmd);
			return NULL;
		}
	}
	
	mi = g_new0(MenuItem, 1);
	mi->type = type;
	
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
	
	while((p=strchr(name, '/')))
		*p = ' ';
	
	term = 0;
	xfce_desktop_entry_get_int (de, "Terminal", &term);
	
	xfce_desktop_entry_get_string (de, "Icon", TRUE, &ifile);
	
	mi->path = _build_path(basepath, path, name);
	
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
		mi->cmd = _sanitise_dentry_cmd(cmd);
		
	if (term)
		mi->term = TRUE;
		
	mi->icon = ifile;

	g_free (name);
	
	return mi;
}

static GList *
menu_dentry_parse_dentry (XfceDesktopEntry *de, GList *menu_data,
		const gchar *basepath, MenuPathType pathtype, gboolean is_legacy,
		const gchar *extra_cat)
{
	gchar *categories = NULL, *hidden = NULL, *path = NULL, *onlyshowin = NULL;
	gchar **catv;
	MenuItem *mi;
	gint i;
	GPtrArray *newpaths = NULL;

	xfce_desktop_entry_get_string (de, "OnlyShowIn", FALSE, &onlyshowin);
	/* each element needs to be ';'-terminated.  i'm not working around
	 * broken files. */
	if(onlyshowin && !strstr(onlyshowin, "XFCE;"))
		goto cleanup;

	xfce_desktop_entry_get_string(de, "Hidden", FALSE, &hidden);
	if(hidden && !g_strcasecmp(hidden, "true"))
		goto cleanup;	

	xfce_desktop_entry_get_string (de, "Categories", TRUE, &categories);
	
	if(categories) {
		/* hack: leave out items that look like they are KDE control panels */
		if(strstr(categories, ";X-KDE-"))
			goto cleanup;
		
		if(pathtype==MPATH_SIMPLE || pathtype==MPATH_SIMPLE_UNIQUE)
			newpaths = menuspec_get_path_simple(categories);
		else if(pathtype==MPATH_MULTI || pathtype==MPATH_MULTI_UNIQUE)
			newpaths = menuspec_get_path_multilevel(categories);
		
		if(!newpaths)
			goto cleanup;
	} else if(is_legacy) {
		newpaths = g_ptr_array_new();
		g_ptr_array_add(newpaths, g_strdup(extra_cat));
	} else
		goto cleanup;
	
	if(pathtype == MPATH_SIMPLE_UNIQUE) {
		/* grab first of the most general */
		path = g_ptr_array_index(newpaths, 0);
		menu_data = _ensure_path(basepath, path, menu_data);
		mi = menu_dentry_parse_dentry_attr(MI_APP, de, basepath, path);
		if(mi)
			menu_data = g_list_append(menu_data, mi);
	} else if(pathtype == MPATH_MULTI_UNIQUE) {
		/* grab most specific */
		path = g_ptr_array_index(newpaths, newpaths->len-1);
		menu_data = _ensure_path(basepath, path, menu_data);
		mi = menu_dentry_parse_dentry_attr(MI_APP, de, basepath, path);
		if(mi)
			menu_data = g_list_append(menu_data, mi);
	} else {
		for(i=0; i < newpaths->len; i++)
			menu_data = _ensure_path(basepath, g_ptr_array_index(newpaths, i),
					menu_data);
		if(pathtype == MPATH_MULTI)
			_prune_generic_paths(newpaths);
		for(i=0; i < newpaths->len; i++) {
			path = g_ptr_array_index(newpaths, i);
			mi = menu_dentry_parse_dentry_attr(MI_APP, de, basepath, path);
			if(mi)
				menu_data = g_list_append(menu_data, mi);
		}
	}

	cleanup:
	
	if(newpaths)
		menuspec_path_free(newpaths);
	if(onlyshowin)
		g_free(onlyshowin);
	if(hidden)
		g_free(hidden);
	if(categories)
		g_free(categories);
	
	return menu_data;
}

static GList *
menu_dentry_parse_dentry_file (const gchar *filename, GList *menu_data,
		const gchar *basepath, MenuPathType pathtype) 
{
	XfceDesktopEntry *dentry;
	dentry = xfce_desktop_entry_new (filename, dentry_keywords,
			G_N_ELEMENTS (dentry_keywords));
	xfce_desktop_entry_parse (dentry);
	menu_data = menu_dentry_parse_dentry(dentry, menu_data, basepath, pathtype,
			FALSE, NULL);
	g_object_unref (G_OBJECT (dentry));
	return menu_data;
}

GList *
menu_dentry_parse_files(const gchar *basepath, MenuPathType pathtype,
		gboolean do_legacy)
{
	GList *menu_data = NULL;
	gint i, totdirs = 0;
	gchar const *pathd;
	GDir *d;
	gchar const *n;
	gchar *s;
	gchar *catfile = g_build_filename(XFCEDATADIR,
			"xfce-registered-categories.xml", NULL);
	const gchar *kdedir = g_getenv("KDEDIR");
	gchar kde_dentry_path[PATH_MAX];
	
	if(dentry_mtimes)
		g_free(dentry_mtimes);
	
	menuspec_parse_categories(catfile);
	g_free(catfile);

	for(i = 0; dentry_paths[i]; i++) {
		pathd = dentry_paths[i];
		totdirs++;

		d = g_dir_open (pathd, 0, NULL);
		if (d) {
			while ((n = g_dir_read_name (d)) != NULL) {
				if (g_str_has_suffix (n, ".desktop")) {
					s = g_build_filename(pathd, n, NULL);
					menu_data = menu_dentry_parse_dentry_file (s, menu_data,
							basepath, pathtype);
					g_free (s);
				}
			}
			g_dir_close (d);
			d = NULL;
		}
	}

	if(kdedir && strcmp(kdedir, "/usr")) {
		g_snprintf(kde_dentry_path, PATH_MAX, "%s/share/applications/kde",
				kdedir);
		totdirs++;
		d = g_dir_open(kde_dentry_path, 0, NULL);
		if(d) {
			while((n=g_dir_read_name(d))) {
				if(g_str_has_suffix(n, ".desktop")) {
					s = g_build_filename(kde_dentry_path, n, NULL);
					menu_data = menu_dentry_parse_dentry_file(s, menu_data,
							basepath, pathtype);
					g_free(s);
				}
			}
			g_dir_close(d);
		}
	}

	if(do_legacy) {
		menu_dentry_legacy_init();
		menu_data = menu_dentry_legacy_add_all(menu_data, basepath, pathtype);
	}
	
	dentry_mtimes = g_new0(time_t, totdirs);
	menu_dentry_need_update();  /* re-init the array */

	menuspec_free();

	return menu_data;
}

gboolean
menu_dentry_need_update()
{
	gint i;
	gboolean modified = FALSE;
	struct stat st;
	
	for(i=0; dentry_paths[i]; i++) {
		if(!stat(dentry_paths[i], &st)) {
			if(st.st_mtime > dentry_mtimes[i]) {
				dentry_mtimes[i] = st.st_mtime;
				modified = TRUE;
			}
		}
	}
	
	modified = (menu_dentry_legacy_need_update() ? TRUE : modified);
	
	return (modified);
}


/*******************************************************************************
 * legacy dir support.  bleh.
 ******************************************************************************/

static GList *
menu_dentry_legacy_parse_dentry_file(const gchar *filename, GList *menu_data,
		const gchar *catdir, const gchar *basepath, MenuPathType pathtype)
{
	XfceDesktopEntry *dentry;
	gchar *category, *precat;

	/* check for a conversion into a freedeskop-compliant category */
	precat = g_hash_table_lookup(dir_to_cat, catdir);
	if(!precat)
		precat = (gchar *)catdir;
	/* check for a conversion into a user-defined display name */
	category = (gchar *)menuspec_cat_to_displayname(precat);
	if(!category)
		category = precat;
	
	dentry = xfce_desktop_entry_new(filename, dentry_keywords,
			G_N_ELEMENTS(dentry_keywords));
	xfce_desktop_entry_parse(dentry);
	menu_data = menu_dentry_parse_dentry(dentry, menu_data, basepath, pathtype,
			TRUE, category);
	g_object_unref(G_OBJECT(dentry));
	
	return menu_data;
}
static GList *
menu_dentry_legacy_process_dir(GList *menu_data, const gchar *basedir,
		const gchar *catdir, const gchar *basepath, MenuPathType pathtype)
{
	GDir *dir = NULL;
	gchar const *file;
	gchar newbasedir[PATH_MAX], fullpath[PATH_MAX];
	
	if(!(dir = g_dir_open(basedir, 0, NULL)))
		return menu_data;
	
	while((file = g_dir_read_name(dir))) {
		g_snprintf(fullpath, PATH_MAX, "%s/%s", basedir, file);
		if(g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
			if(*file == '.' || strstr(file, "Settings")) /* FIXME: this is questionable */
				continue;
			/* i've made the decision to ignore categories with subdirectories.
			 * that is, we're going to collapse them into their toplevel
			 * category.  the subcategories i've seen are rather non-compliant,
			 * and it's non-trivial and error-prone to convert them into
			 * something compliant. */
			g_snprintf(newbasedir, PATH_MAX, "%s/%s", basedir, file);
			if(!catdir)
				catdir = file;
			menu_data = menu_dentry_legacy_process_dir(menu_data, newbasedir,
					catdir, basepath, pathtype);
		} else if(catdir && g_str_has_suffix(file, ".desktop")) {
			/* we're also going to ignore category-less .desktop files. */
			menu_data = menu_dentry_legacy_parse_dentry_file(fullpath,
					menu_data, catdir, basepath, pathtype);
		}
	}
	
	g_dir_close(dir);	
	
	return menu_data;
}

static GList *
menu_dentry_legacy_add_all(GList *menu_data, const gchar *basepath,
		MenuPathType pathtype)
{
	gint i, totdirs = 0;
	const gchar *kdedir = g_getenv("KDEDIR");
	gchar extradir[PATH_MAX];
	
	if(dentry_legacy_mtimes)
		g_free(dentry_legacy_mtimes);
	
	for(i=0; legacy_dirs[i]; i++) {
		totdirs++;
		menu_data = menu_dentry_legacy_process_dir(menu_data, legacy_dirs[i],
				NULL, basepath, pathtype);
	}
	
	if(kdedir && strcmp(kdedir, "/usr")) {
		g_snprintf(extradir, PATH_MAX, "%s/share/applnk", kdedir);
		totdirs++;
		menu_data = menu_dentry_legacy_process_dir(menu_data, extradir, NULL,
				basepath, pathtype);
	}
	
	dentry_legacy_mtimes = g_new0(time_t, totdirs);
	menu_dentry_legacy_need_update();  /* re-init the array */
	
	return menu_data;
}

static void
menu_dentry_legacy_init()
{
	static gboolean is_inited = FALSE;
	
	if(is_inited)
		return;
	
	dir_to_cat = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_insert(dir_to_cat, "Internet", "Network");
	g_hash_table_insert(dir_to_cat, "OpenOffice.org", "Office");
	g_hash_table_insert(dir_to_cat, "Utilities", "Utility");
	g_hash_table_insert(dir_to_cat, "Toys", "Utility");
	g_hash_table_insert(dir_to_cat, "Multimedia", "AudioVideo");
	g_hash_table_insert(dir_to_cat, "Applications", "Core");
	
	/* we'll keep this hashtable around for the lifetime of xfdesktop.  it'll
	 * give us a slight performance boost during regenerations, and the memory
	 * requirements should be of minimal impact. */
	is_inited = TRUE;
}

gboolean
menu_dentry_legacy_need_update()
{
	gint i;
	gboolean modified = FALSE;
	struct stat st;
	
	for(i=0; legacy_dirs[i]; i++) {
		if(!stat(legacy_dirs[i], &st)) {
			if(st.st_mtime > dentry_legacy_mtimes[i]) {
				dentry_legacy_mtimes[i] = st.st_mtime;
				modified = TRUE;
			}
		}
	}
	
	return (modified);
}
