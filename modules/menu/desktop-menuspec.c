/*
 *  xfce-registered-categories parser for generating menus
 *    Copyright (c) 2004 Brian Tarricone <bjt23@cornell.edu>
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
#include "config.h"
#endif

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h> /* free() */
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

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif
#ifndef MAP_FILE
#define MAP_FILE (0)
#endif

#include <glib.h>
#include <libxfce4util/i18n.h>

#include "desktop-menuspec.h"

#define FALLBACK_PATH _("/Other")

struct MenuspecParserState {
	gboolean started;
	gchar cur_category[128];
	GNode *cur_node;
};

struct MenuTreeSearchInfo {
	char *category;
	GNode *foundnode;
};

struct MenuTreeFindPathInfo {
	char **cats;
	GPtrArray *paths;
};

/* globals */
static GHashTable *cats_hide = NULL;
static GHashTable *cats_ignore = NULL;
static GHashTable *cat_to_displayname = NULL;
static GHashTable *displayname_to_icon = NULL;
static GHashTable *cats_orphans = NULL;
static GNode *menu_tree = NULL;

static void
tree_add_orphans(gpointer key, gpointer data, gpointer user_data)
{
	g_node_append_data(menu_tree, key);
}

static gboolean
get_paths_simple_single(GNode *node, gpointer data)
{
	struct MenuTreeFindPathInfo *mtfpi = data;
	int i;
	GNode *n;
	gchar *foundcat;
	
	for(i=0; mtfpi->cats[i]; i++) {
		if(!strcmp(mtfpi->cats[i], (char *)node->data)) {
			for(n=node; n; n=n->parent) {
				if(n->parent && ((char *)n->parent->data)[0] == '/')
					break;
			}
			if(!n)
				n = node;
                        if (cat_to_displayname)
			        foundcat = g_hash_table_lookup(cat_to_displayname, n->data);
			else
                                foundcat = NULL;
                        if(!foundcat)
				foundcat = n->data;
			g_ptr_array_add(mtfpi->paths, g_strconcat("/", foundcat, NULL));
			return TRUE;
		}
	}
	
	return FALSE;
}

static gboolean
get_paths_multilevel(GNode *node, gpointer data)
{
	struct MenuTreeFindPathInfo *mtfpi = data;
	GPtrArray *revpath = NULL;
	int i, j, totlen;
	GNode *n;
	gchar *foundcat, *newpath;
	
	for(i=0; mtfpi->cats[i]; i++) {
		if(!strcmp(mtfpi->cats[i], (char *)node->data)) {
			totlen = 0;
			revpath = g_ptr_array_new();
			for(n=node; ((char *)n->data)[0] != '/'; n=n->parent) {
                                if (cat_to_displayname)
				        foundcat = g_hash_table_lookup(cat_to_displayname, n->data);
				else
                                        foundcat = NULL;
                                if(!foundcat) {
					g_ptr_array_free(revpath, FALSE);
					revpath = NULL;
					break;
				}
				g_ptr_array_add(revpath, foundcat);
				totlen += strlen(foundcat) + 1;
			}
			if(revpath) {
				newpath = g_malloc(totlen+1);
				*newpath = 0;
				for(j=revpath->len-1; j>=0; j--) {
					g_strlcat(newpath, "/", totlen+1);
					g_strlcat(newpath, g_ptr_array_index(revpath, j), totlen+1);
				}
				newpath[totlen] = 0;
				
				g_ptr_array_add(mtfpi->paths, newpath);
				g_ptr_array_free(revpath, FALSE);
			}
		}
	}
	
	return FALSE;
}

static gboolean
menu_tree_find_node(GNode *node, gpointer data)
{
	struct MenuTreeSearchInfo *mtsi = data;
	
	if(!strcmp(node->data, mtsi->category)) {
		mtsi->foundnode = node;
		return TRUE;
	}
	
	return FALSE;
}

static void
menuspec_xml_start(GMarkupParseContext *context, const gchar *element_name,
		const gchar **attribute_names, const gchar **attribute_values,
		gpointer user_data, GError **error)
{
	gint i;
	struct MenuspecParserState *state = user_data;
	
	if(!strcmp(element_name, "category")) {
		gchar *cur_displayname = NULL, *cat_dupe = NULL, *icon = NULL;
		gboolean hide = FALSE, ignore = FALSE, toplevel = FALSE;
		GNode *newnode;
		struct MenuTreeSearchInfo mtsi;
		
		if(!state->started)
			return;
		
		for(i=0; attribute_names[i]; i++) {
			if(!strcmp(attribute_names[i], "name"))
				g_strlcpy(state->cur_category, attribute_values[i], 128);
			else if(!strcmp(attribute_names[i], "replace"))
				cur_displayname = g_strdup(attribute_values[i]);
			else if(!strcmp(attribute_names[i], "icon"))
				icon = g_strdup(attribute_values[i]);
			else if(!strcmp(attribute_names[i], "hide"))
				hide = !g_ascii_strcasecmp(attribute_values[i], "true") ? TRUE : FALSE;
			else if(!strcmp(attribute_names[i], "ignore"))
				ignore = !g_ascii_strcasecmp(attribute_values[i], "true") ? TRUE : FALSE;
			else if(!strcmp(attribute_names[i], "toplevel"))
				toplevel = !g_ascii_strcasecmp(attribute_values[i], "true") ? TRUE : FALSE;
		}
		
		if(!ignore) {
			mtsi.category = state->cur_category;
			mtsi.foundnode = NULL;
			g_node_traverse(menu_tree, G_IN_ORDER, G_TRAVERSE_ALL, -1,
					menu_tree_find_node, &mtsi);
			
			if(mtsi.foundnode) {
				if(state->cur_node != menu_tree) {
					newnode = g_node_copy(mtsi.foundnode);
					if((toplevel && state->cur_node == menu_tree) ||
							state->cur_node != menu_tree)
					{
						g_node_append(state->cur_node, newnode);
					}
					state->cur_node = newnode;
				} else
					state->cur_node = mtsi.foundnode;
				cat_dupe = mtsi.foundnode->data;
			} else {
				cat_dupe = g_strdup(state->cur_category);
				newnode = g_node_new(cat_dupe);
				if(!toplevel && state->cur_node == menu_tree)
					g_hash_table_insert(cats_orphans, cat_dupe, newnode);
				else
					g_node_append(state->cur_node, newnode);
				state->cur_node = newnode;
			}
		} else
			cat_dupe = g_strdup(state->cur_category);
		
		if(cur_displayname)
			g_hash_table_insert(cat_to_displayname, cat_dupe, cur_displayname);
		else
			g_hash_table_insert(cat_to_displayname, cat_dupe, strdup(cat_dupe));
		
		if(icon) {
			if(cur_displayname)
				g_hash_table_insert(displayname_to_icon, cur_displayname, icon);
			else
				g_hash_table_insert(displayname_to_icon, cat_dupe, icon);
		}
		
		if(hide)
			g_hash_table_insert(cats_hide, cat_dupe, GINT_TO_POINTER(1));
		
		if(ignore)
			g_hash_table_insert(cats_ignore, cat_dupe, GINT_TO_POINTER(1));
	} else if(!strcmp(element_name, "subcategory")) {
		struct MenuTreeSearchInfo mtsi;
		GNode *newnode;
		
		if(!state->started)
			return;
		
		if(cats_ignore && g_hash_table_lookup(cats_ignore, state->cur_category))
			return;
		
		if(attribute_names[0] && *attribute_names[0] &&
				!strcmp(attribute_names[0], "name"))
		{
			if(!cats_ignore || !g_hash_table_lookup(cats_ignore, attribute_values[0])) {
				mtsi.category = (char *)attribute_values[0];
				mtsi.foundnode = NULL;
				g_node_traverse(menu_tree, G_IN_ORDER, G_TRAVERSE_ALL, -1,
						menu_tree_find_node, &mtsi);
				if(mtsi.foundnode)
					newnode = g_node_copy(mtsi.foundnode);
				else
					newnode = g_node_new(g_strdup(attribute_values[0]));
				g_node_append(state->cur_node, newnode);
				g_hash_table_remove(cats_orphans, attribute_values[0]);
			}
		} else
			g_warning(_("%s: missing or unknown attribute for 'related' element\n"),
					PACKAGE);
	} else if(!strcmp(element_name, "xfce-registered-categories"))
		state->started = TRUE;
	else
		g_warning(_("%s: unknown xml element %s\n"), PACKAGE, element_name);
}

static void
menuspec_xml_end(GMarkupParseContext *context, const gchar *element_name,
		gpointer user_data, GError **error)
{
	struct MenuspecParserState *state = user_data;
	
	if(!strcmp(element_name, "category")) {
		if(state->cur_node && state->cur_node->parent)
			state->cur_node = menu_tree;
		*state->cur_category = 0;
	} else if(!strcmp(element_name, "xfce-registered-categories"))
		state->started = FALSE;
}

gboolean
desktop_menuspec_parse_categories(const gchar *filename)
{
	gchar *file_contents = NULL;
	GMarkupParseContext *gpcontext = NULL;
	int fd = -1;
	struct stat st;
	GMarkupParser gmparser = {
		menuspec_xml_start,
		menuspec_xml_end,
		NULL,
		NULL,
		NULL
	};
	struct MenuspecParserState state = { FALSE, "", NULL };
	gboolean ret = FALSE;
	GError *err = NULL;
#ifdef HAVE_MMAP
	void *maddr = NULL;
#endif
	
	if(stat(filename, &st) < 0)
		return FALSE;
	
	fd = open(filename, O_RDONLY, 0);
	if(fd < 0)
		goto cleanup;
	
#ifdef HAVE_MMAP
	maddr = mmap(NULL, st.st_size, PROT_READ, MAP_FILE|MAP_SHARED, fd, 0);
	if(maddr)
		file_contents = maddr;
	else {
#endif
		file_contents = malloc(st.st_size);
		if(!file_contents)
			goto cleanup;
		
		if(read(fd, file_contents, st.st_size) != st.st_size)
			goto cleanup;
#ifdef HAVE_MMAP
	}
#endif
	
	cats_hide = g_hash_table_new(g_str_hash, g_str_equal);
	cats_ignore = g_hash_table_new(g_str_hash, g_str_equal);
	cat_to_displayname = g_hash_table_new_full(g_str_hash, g_str_equal,
			(GDestroyNotify)g_free, (GDestroyNotify)g_free);
	displayname_to_icon = g_hash_table_new_full(g_str_hash, g_str_equal,
			NULL, (GDestroyNotify)g_free);
	cats_orphans = g_hash_table_new(g_str_hash, g_str_equal);
	menu_tree = g_node_new("/");
	state.cur_node = menu_tree;
	
	gpcontext = g_markup_parse_context_new(&gmparser, 0, &state, NULL);

    if(!g_markup_parse_context_parse(gpcontext, file_contents, st.st_size, &err)) {
		g_warning("%s: error parsing Xfce registered categories file (%d): %s\n",
				PACKAGE, err->code, err->message);
		g_error_free(err);
		g_hash_table_destroy(cats_orphans);
		desktop_menuspec_free();
        goto cleanup;
	}

    if(g_markup_parse_context_end_parse(gpcontext, NULL))
		ret = TRUE;
	
	/* if we have orphans, add them as toplevels regardless */
	g_hash_table_foreach(cats_orphans, tree_add_orphans, NULL);
	g_hash_table_destroy(cats_orphans);
	cats_orphans = NULL;
	
	cleanup:
	
	if(gpcontext)
		g_markup_parse_context_free(gpcontext);
#ifdef HAVE_MMAP
	if(maddr)
		munmap(maddr, st.st_size);
	else if(file_contents)
#endif
		free(file_contents);
	if(fd > -1)
		close(fd);
	
	return ret;
}

/* this has a worst-case of O(m*n) [m=# of cats, n=# of toplevels].  this does
 * not please me.
 */
GPtrArray *
desktop_menuspec_get_path_simple(const gchar *categories)
{
	GPtrArray *paths;
	gchar *foundcat, **cats = NULL;
	gint i;
	GNode *n;
	
	if(!menu_tree)
		return NULL;
	
	paths = g_ptr_array_new();
	
	if(categories)
		 cats = g_strsplit(categories, ";", 0);
	if(!cats) {
		g_ptr_array_add(paths, g_strdup(FALLBACK_PATH));
		return paths;
	}

	/* first check the toplevels */
	for(i=0; cats[i]; i++) {
		for(n=menu_tree->children; n; n=n->next) {
			if(!strcmp(cats[i], (char *)n->data)) {
				if (cat_to_displayname)
                                        foundcat = g_hash_table_lookup(cat_to_displayname, n->data);
                                else
                                        foundcat = NULL;
				if(!foundcat)
					foundcat = n->data;
				g_ptr_array_add(paths, g_build_path("/", foundcat, NULL));
			}
		}
	}
	
	/* if we don't have a toplevel match, let's just find whatever we can and
	 * either reduce it or temporarily promote it to a toplevel */
	if(paths->len == 0) {
		struct MenuTreeFindPathInfo mtfpi;
		
		mtfpi.cats = cats;
		mtfpi.paths = paths;
		g_node_traverse(menu_tree, G_IN_ORDER, G_TRAVERSE_ALL, -1,
				get_paths_simple_single, &mtfpi);
	}			

	g_strfreev(cats);
	
	if(paths->len == 0)
		g_ptr_array_add(paths, g_strdup(FALLBACK_PATH));
						
	return paths;
}

GPtrArray *
desktop_menuspec_get_path_multilevel(const gchar *categories)
{
	GPtrArray *paths;
	gchar **cats = NULL;
	struct MenuTreeFindPathInfo mtfpi;
	
	if(!menu_tree)
		return NULL;
	
	paths = g_ptr_array_new();
	
	if(categories)
		 cats = g_strsplit(categories, ";", 0);
	if(!cats) {
		g_ptr_array_add(paths, g_strdup(FALLBACK_PATH));
		return paths;
	}
	
	mtfpi.cats = cats;
	mtfpi.paths = paths;
	g_node_traverse(menu_tree, G_IN_ORDER, G_TRAVERSE_ALL, -1,
			get_paths_multilevel, &mtfpi);
	
	g_strfreev(cats);
	
	if(paths->len == 0)
		g_ptr_array_add(paths, g_strdup(FALLBACK_PATH));
	
	return paths;
}

G_CONST_RETURN gchar *
desktop_menuspec_cat_to_displayname(const gchar *category)
{
        if (cat_to_displayname)
        	return g_hash_table_lookup(cat_to_displayname, category);
        return NULL;
}

G_CONST_RETURN gchar *
desktop_menuspec_displayname_to_icon(const gchar *displayname)
{
        if (displayname_to_icon)
	        return g_hash_table_lookup(displayname_to_icon, displayname);
        return NULL;
}

void
desktop_menuspec_free() {
	if(cats_hide)
		g_hash_table_destroy(cats_hide);
	cats_hide = NULL;
	if(cats_ignore)
		g_hash_table_destroy(cats_ignore);
	cats_ignore = NULL;
	if(cat_to_displayname)
		g_hash_table_destroy(cat_to_displayname);
	cat_to_displayname = NULL;
	if(displayname_to_icon)
		g_hash_table_destroy(displayname_to_icon);
	displayname_to_icon = NULL;
	if(menu_tree)
		g_node_destroy(menu_tree);
	menu_tree = NULL;
}

void
desktop_menuspec_path_free(GPtrArray *paths)
{
	if(!paths)
		return;
	
	g_ptr_array_free(paths, TRUE);
}
