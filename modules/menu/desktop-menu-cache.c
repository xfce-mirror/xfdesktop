/*
 *  desktop-menu-cache.[ch] - routines for caching a generated menu file
 *
 *  Copyright (C) 2004 Brian Tarricone <bjt23@cornell.edu>
 *                2004 Benedikt Meurer <benny@xfce.org>
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

#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>

#include "desktop-menu-cache.h"

#define CACHE_CONF_FILE "xfdesktop/menu-cache.rc"
#define MENU_CACHE_FILE "xfdesktop/menu-cache.xml"

typedef struct
{
	DesktopMenuCacheType type;
	
	gchar *name;
	gchar *cmd;
	gchar *icon;
	gboolean needs_term;
	gboolean snotify;
} DesktopMenuCacheEntry;

typedef struct
{
	FILE *fp;
	gint depth;
} TraverseData;

static GNode *menu_tree = NULL;
static GHashTable *menu_hash = NULL;
static GList *menu_files = NULL;
static GList *dentry_dirs = NULL;
gboolean using_system_menu = TRUE;

static void
desktop_menu_cache_entry_destroy(DesktopMenuCacheEntry *entry)
{
	if(entry->name)
		g_free(entry->name);
	if(entry->cmd)
		g_free(entry->cmd);
	if(entry->icon)
		g_free(entry->icon);
	
	g_free(entry);
}

static gboolean
dmc_free_tree_data(GNode *node, gpointer data)
{
	if(node->data)
		desktop_menu_cache_entry_destroy((DesktopMenuCacheEntry *)node->data);
	
	return FALSE;
}

static void
cache_node_children(GNode *node, gpointer data)
{
	DesktopMenuCacheEntry *entry = node->data;
	TraverseData *td = data;
	FILE *fp = td->fp;
	gchar tabs[64];  /* if the user has a > 63-level submenu, they deserve a segfault */
	
	g_return_if_fail(entry);
	
	memset(tabs, '\t', td->depth);
	tabs[td->depth] = 0;
	
	switch(entry->type) {
		case DM_TYPE_ROOT:
			g_critical("%s: cache_node_children() run ON the root node!", PACKAGE);
			return;
		
		case DM_TYPE_MENU:
			fprintf(fp, "%s<menu name=\"%s\" icon=\"%s\">\n", tabs,
					entry->name ? entry->name : "",
					entry->icon ? entry->icon : "");
			td->depth++;
			g_node_children_foreach(node, G_TRAVERSE_ALL, cache_node_children, td);
			td->depth--;
			fprintf(fp, "%s</menu>\n", tabs);
			break;
		
		case DM_TYPE_APP:
			fprintf(fp, "%s<app name=\"%s\" cmd=\"%s\" icon=\"%s\" term=\"%s\" snotify=\"%s\" />\n",
					tabs,
					entry->name ? entry->name : "",
					entry->cmd ? entry->cmd : "",
					entry->icon ? entry->icon : "",
					entry->needs_term ? "true" : "false",
					entry->snotify ? "true" : "false");
			break;
		
		case DM_TYPE_TITLE:
			fprintf(fp, "%s<title name=\"%s\" icon=\"%s\" />\n", tabs,
					entry->name ? entry->name : "",
					entry->icon ? entry->icon : "");
			break;
		
		case DM_TYPE_SEPARATOR:
			fprintf(fp, "%s<separator />\n", tabs);
			break;
		
		case DM_TYPE_BUILTIN:
			fprintf(fp, "%s<builtin name=\"%s\" cmd=\"%s\" icon=\"%s\" />\n",
				tabs,
				entry->name ? entry->name : "",
				entry->cmd ? entry->cmd : "",
				entry->icon ? entry->icon : "");
			break;
		
		default:
			g_warning("%s: Got unknown cache entry type (%d)", PACKAGE, entry->type);
			break;
	}
}
	

void
desktop_menu_cache_init(GtkWidget *root_menu)
{
	DesktopMenuCacheEntry *root_entry;
	
	g_return_if_fail(root_menu);
	
	root_entry = g_new0(DesktopMenuCacheEntry, 1);
	
	root_entry->type = DM_TYPE_ROOT;
	root_entry->name = g_strdup("/");
	menu_tree = g_node_new(root_entry);
	
	menu_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
	g_hash_table_insert(menu_hash, root_menu, menu_tree);
}

gchar *
desktop_menu_cache_is_valid(GHashTable **menufile_mtimes,
		GHashTable **dentrydir_mtimes, gboolean *using_system_menu)
{
	gchar *cache_file = NULL, buf[128];
	XfceRc *rcfile;
	gint i, mtime;
	const gchar *location;
	struct stat st;
	
	g_return_val_if_fail(menufile_mtimes != NULL && dentrydir_mtimes != NULL
			&& using_system_menu != NULL, NULL);
	
	cache_file = xfce_resource_save_location(XFCE_RESOURCE_CACHE,
			MENU_CACHE_FILE, FALSE);
	if(!cache_file)
		return NULL;
	if(!g_file_test(cache_file, G_FILE_TEST_EXISTS)) {
		g_free(cache_file);
		return NULL;
	}
	
	rcfile = xfce_rc_config_open(XFCE_RESOURCE_CACHE, CACHE_CONF_FILE, TRUE);
	if(!rcfile)
		return NULL;
	
	if(xfce_rc_has_group(rcfile, "settings")) {
		xfce_rc_set_group(rcfile, "settings");
		
		*using_system_menu = xfce_rc_read_bool_entry(rcfile, "using_system_menu", FALSE);
	}
	
	*menufile_mtimes = g_hash_table_new_full(g_str_hash, g_str_equal,
				(GDestroyNotify)g_free, NULL);
	if(xfce_rc_has_group(rcfile, "files")) {
		xfce_rc_set_group(rcfile, "files");
		
		for(i = 0; TRUE; i++) {
			g_snprintf(buf, 128, "location%d", i);
			location = xfce_rc_read_entry(rcfile, buf, NULL);
			if(!location)
				break;
			g_snprintf(buf, 128, "mtime%d", i);
			mtime = xfce_rc_read_int_entry(rcfile, buf, -1);
			if(mtime == -1)
				break;
			
			if(!stat(location, &st)) {
				if(st.st_mtime > mtime) {
					xfce_rc_close(rcfile);
					g_hash_table_destroy(*menufile_mtimes);
					*menufile_mtimes = NULL;
					TRACE("exiting - failed");
					return NULL;
				} else {
					g_hash_table_insert(*menufile_mtimes, g_strdup(location),
							GINT_TO_POINTER(st.st_mtime));
				}
			}
		}
	}
	
	*dentrydir_mtimes = g_hash_table_new_full(g_str_hash, g_str_equal,
				(GDestroyNotify)g_free, NULL);
	if(xfce_rc_has_group(rcfile, "directories")) {
		xfce_rc_set_group(rcfile, "directories");
		
		for(i = 0; TRUE; i++) {
			g_snprintf(buf, 128, "location%d", i);
			location = xfce_rc_read_entry(rcfile, buf, NULL);
			if(!location)
				break;
			g_snprintf(buf, 128, "mtime%d", i);
			mtime = xfce_rc_read_int_entry(rcfile, buf, -1);
			if(mtime == -1)
				break;
			
			if(!stat(location, &st)) {
				if(st.st_mtime > mtime) {
					xfce_rc_close(rcfile);
					g_hash_table_destroy(*dentrydir_mtimes);
					*dentrydir_mtimes = NULL;
					g_hash_table_destroy(*menufile_mtimes);
					*menufile_mtimes = NULL;
					TRACE("exiting - failed");
					return NULL;
				} else {
					g_hash_table_insert(*dentrydir_mtimes, g_strdup(location),
							GINT_TO_POINTER(st.st_mtime));
				}
			}
		}
	}
	
	xfce_rc_close(rcfile);
	
	return cache_file;
}

void
desktop_menu_cache_add_entry(DesktopMenuCacheType type, const gchar *name,
		const gchar *cmd, const gchar *icon, gboolean needs_term,
		gboolean snotify, GtkWidget *parent_menu, gint position,
		GtkWidget *menu_widget)
{
	DesktopMenuCacheEntry *entry;
	GNode *parent_node = NULL, *entry_node;
	
	if(!menu_tree)
		return;
	
	g_return_if_fail(parent_menu);
	
	parent_node = g_hash_table_lookup(menu_hash, parent_menu);
	if(!parent_node) {
		g_critical("%s: Attempt to add new cache entry without first adding the parent.", PACKAGE);
		return;
	}
	
	entry = g_new0(DesktopMenuCacheEntry, 1);
	entry->type = type;
	if(name)
		entry->name = g_markup_escape_text(name, strlen(name));
	if(cmd)
		entry->cmd = g_markup_escape_text(cmd, strlen(cmd));
	if(icon)
		entry->icon = g_markup_escape_text(icon, strlen(icon));
	entry->needs_term = needs_term;
	entry->snotify = snotify;
	
	entry_node = g_node_new(entry);
	g_node_insert(parent_node, position, entry_node);
	
	if(type == DM_TYPE_MENU)
		g_hash_table_insert(menu_hash, menu_widget, entry_node);
}

void
desktop_menu_cache_add_menufile(const gchar *menu_file)
{
	if(!menu_tree)
		return;
	
	g_return_if_fail(menu_file);
	
	menu_files = g_list_append(menu_files, g_strdup(menu_file));
}

void
desktop_menu_cache_add_dentrydir(const gchar *dentry_dir)
{
	if(!menu_tree)
		return;
	
	g_return_if_fail(dentry_dir);
	
	dentry_dirs = g_list_append(dentry_dirs, g_strdup(dentry_dir));
	using_system_menu = TRUE;
}

void
desktop_menu_cache_flush()
{
	gchar *cache_file = NULL, buf[128];
	XfceRc *rcfile;
	GList *l;
	gint i;
	FILE *fp;
	TraverseData td;
	struct stat st;
	
	if(!menu_tree)
		return;
	
	TRACE("entering");
	
	rcfile = xfce_rc_config_open(XFCE_RESOURCE_CACHE, CACHE_CONF_FILE, FALSE);
	if(!rcfile) {
		g_critical("%s: Unable to write to '%s'.  Desktop menu wil not be cached",
				PACKAGE, CACHE_CONF_FILE);
		return;
	}
	
	xfce_rc_set_group(rcfile, "settings");
	xfce_rc_write_bool_entry(rcfile, "using_system_menu", using_system_menu);
	
	xfce_rc_set_group(rcfile, "files");
	for(i = 0, l = menu_files; l; l = l->next, i++) {
		gchar *file = l->data;
		if(!stat(file, &st)) {
			g_snprintf(buf, 128, "location%d", i);
			xfce_rc_write_entry(rcfile, buf, file);
			g_snprintf(buf, 128, "mtime%d", i);
			xfce_rc_write_int_entry(rcfile, buf, st.st_mtime);
		}
	}
	xfce_rc_set_group(rcfile, "directories");
	for(i = 0, l = dentry_dirs; l; l = l->next, i++) {
		gchar *dir = l->data;
		if(!stat(dir, &st)) {
			g_snprintf(buf, 128, "location%d", i);
			xfce_rc_write_entry(rcfile, buf, dir);
			g_snprintf(buf, 128, "mtime%d", i);
			xfce_rc_write_int_entry(rcfile, buf, st.st_mtime);
		}
	}
	
	xfce_rc_flush(rcfile);
	xfce_rc_close(rcfile);
	
	cache_file = xfce_resource_save_location(XFCE_RESOURCE_CACHE,
			MENU_CACHE_FILE, TRUE);
	fp = fopen(cache_file, "w");
	if(!fp) {
		g_critical("%s: Unable to write to '%s'.  Desktop menu wil not be cached",
				PACKAGE, cache_file);
		g_free(cache_file);
		return;
	}
	g_free(cache_file);
	
	fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE xfdesktop-menu>\n\n", fp);
	fputs("<xfdesktop-menu>\n", fp);
	
	if(menu_tree) {
		td.fp = fp;
		td.depth = 1;
		g_node_children_foreach(menu_tree, G_TRAVERSE_ALL, cache_node_children, &td);
	}
	
	fputs("</xfdesktop-menu>\n", fp);
	
	fclose(fp);
	
	TRACE("exiting");
}

void
desktop_menu_cache_cleanup()
{
	GList *l;
	
	if(menu_tree) {
		g_node_traverse(menu_tree, G_IN_ORDER, G_TRAVERSE_ALL, -1,
				(GNodeTraverseFunc)dmc_free_tree_data, NULL);
		g_node_destroy(menu_tree);
		menu_tree = NULL;
	}
	
	for(l = menu_files; l; l = l->next)
		g_free(l->data);
	if(menu_files) {
		g_list_free(menu_files);
		menu_files = NULL;
	}
	
	for(l = dentry_dirs; l; l = l->next)
		g_free(l->data);
	if(dentry_dirs) {
		g_list_free(dentry_dirs);
		dentry_dirs = NULL;
	}
}
