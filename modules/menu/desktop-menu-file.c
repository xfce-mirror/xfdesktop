/*
 *  desktop-menu-file.[ch] - routines for parsing an xfdeskop menu xml file
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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif
#ifndef MAP_FILE
#define MAP_FILE (0)
#endif

#include <X11/X.h>
#include <X11/Xlib.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <glib.h>

#include <libxfce4util/i18n.h>
#include <libxfce4util/util.h>
#include <libxfcegui4/xfce-appmenuitem.h>
#include <libxfcegui4/icons.h>

//#define BDEBUG
#include "desktop-menu-private.h"
#include "desktop-menu.h"
#include "desktop-menu-file.h"
#include "desktop-menu-dentry.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Search path for menu.xml file */
#define SEARCHPATH	(SYSCONFDIR G_DIR_SEPARATOR_S "xfce4" G_DIR_SEPARATOR_S "%F.%L:"\
                         SYSCONFDIR G_DIR_SEPARATOR_S "xfce4" G_DIR_SEPARATOR_S "%F.%l:"\
                         SYSCONFDIR G_DIR_SEPARATOR_S "xfce4" G_DIR_SEPARATOR_S "%F")

#define MI_BUILTIN_QUIT 1

extern void quit();

struct MenuFileParserState {
	gboolean started;
	GQueue *branches;  /* GtkWidget * menus */
	GtkWidget *cur_branch;  /* current branch GtkMenuShell widget */
	GQueue *paths;  /* gchar * path names */
	gchar cur_path[2048];  /* current full path */
	XfceDesktopMenu *desktop_menu;
	gint hidelevel;
};

static gint
_find_attribute(const gchar **attribute_names, const gchar *attr)
{
	gint i;
	
	for(i=0; attribute_names[i]; i++) {
		if(!strcmp(attribute_names[i], attr))
			return i;
	}
	
	return -1;
}

static void
_do_builtin(GtkMenuItem *mi, gpointer user_data)
{
	gint type = GPOINTER_TO_INT(user_data);
	
	switch(type) {
		case MI_BUILTIN_QUIT:
			quit();
			break;
		default:
			g_warning("XfceDesktopMenu: unknown builtin type (%d)\n", type);
	}
}

static gchar *
_menu_file_get()
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
		return g_strdup(filename);
	}

    g_warning("%s: Could not locate a menu definition file", PACKAGE);

    return NULL;
}

static void
menu_file_xml_start(GMarkupParseContext *context, const gchar *element_name,
		const gchar **attribute_names, const gchar **attribute_values,
		gpointer user_data, GError **error)
{
	gint i, j, k, l, m;
	GtkWidget *mi = NULL;
	gchar tmppath[2048];
	struct MenuFileParserState *state = user_data;
	
	BD("cur_path: %s, hidelevel=%d", state->cur_path, state->hidelevel);
	
	if(!state->started && !strcmp(element_name, "xfdesktop-menu")) {
		state->desktop_menu->use_menu_icons = TRUE;  /* default */
		i = _find_attribute(attribute_names, "showicons");
		if(i != -1 && !strcmp(attribute_values[i], "false"))
			state->desktop_menu->use_menu_icons = FALSE;
		state->started = TRUE;
	} else if(!state->started)
		return;
	
	if(!strcmp(element_name, "app")) {
		if(state->hidelevel)
			return;
		
		if((i=_find_attribute(attribute_names, "visible")) != -1 &&
				(!strcmp(attribute_values[i], "false") ||
				!strcmp(attribute_values[i], "no")))
		{
			return;
		}
		
		i = _find_attribute(attribute_names, "name");
		if(i == -1)
			return;
		
		g_snprintf(tmppath, 2048, "%s/%s", state->cur_path, attribute_values[i]);
		if(g_hash_table_lookup(state->desktop_menu->menu_entry_hash, tmppath))
			return;
		
		j = _find_attribute(attribute_names, "cmd");
		if(j == -1)
			return;
		k = _find_attribute(attribute_names, "term");
		l = _find_attribute(attribute_names, "snotify");
		m = (state->desktop_menu->use_menu_icons ? _find_attribute(attribute_names, "icon") : -1);
		
		mi = xfce_app_menu_item_new_with_command(attribute_values[i],
				attribute_values[j]);
		if(k != -1 && (!strcmp(attribute_values[k], "true") || 
				!strcmp(attribute_values[k], "yes")))
		{
			xfce_app_menu_item_set_needs_term(XFCE_APP_MENU_ITEM(mi), TRUE);
		}
		if(l != -1 && !strcmp(attribute_values[l], "true"))
			xfce_app_menu_item_set_startup_notification(XFCE_APP_MENU_ITEM(mi),
					TRUE);
		if(m != -1 && *attribute_values[m])
			xfce_app_menu_item_set_icon_name(XFCE_APP_MENU_ITEM(mi),
					attribute_values[m]);
		
		gtk_widget_set_name(mi, "xfdesktopmenu");
		gtk_widget_show(mi);
		gtk_menu_shell_append(GTK_MENU_SHELL(state->cur_branch), mi);
		g_hash_table_insert(state->desktop_menu->menu_entry_hash,
				g_build_path("/", state->cur_path, attribute_values[i], NULL),
				GINT_TO_POINTER(1));
	} else if(!strcmp(element_name, "menu")) {		
		if((i=_find_attribute(attribute_names, "visible")) != -1 &&
				(!strcmp(attribute_values[i], "false") ||
				!strcmp(attribute_values[i], "no")))
		{
			state->hidelevel++;
			return;
		}
		
		if(state->hidelevel) {
			state->hidelevel++;
			return;
		}
		
		i = _find_attribute(attribute_names, "name");
		if(i == -1)
				return;
		j = (state->desktop_menu->use_menu_icons ? _find_attribute(attribute_names, "icon") : -1);
		
		if(j == -1)
			mi = gtk_menu_item_new_with_label(attribute_values[i]);
		else {
			GdkPixbuf *pix;
			GtkWidget *image;
			mi = gtk_image_menu_item_new_with_label(attribute_values[i]);
			pix = xfce_load_themed_icon(attribute_values[j], 
					_xfce_desktop_menu_icon_size);
			if(pix) {
				image = gtk_image_new_from_pixbuf(pix);
				gtk_widget_show(image);
				gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), image);
				g_object_unref(G_OBJECT(pix));
			}
		}
		gtk_widget_set_name(mi, "xfdesktopmenu");
		gtk_widget_show(mi);
		gtk_menu_shell_append(GTK_MENU_SHELL(state->cur_branch), mi);
		
		state->cur_branch = gtk_menu_new();
		gtk_widget_show(state->cur_branch);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), state->cur_branch);
		g_queue_push_tail(state->branches, state->cur_branch);
		g_queue_push_tail(state->paths, g_strdup(attribute_values[i]));
		if(!state->cur_path[1])
			g_strlcat(state->cur_path, attribute_values[i], 2048);
		else {
			gint len = strlen(state->cur_path);
			if(len < 2047) {
				state->cur_path[len++] = '/';
				state->cur_path[len] = 0;
			}
			g_strlcat(state->cur_path, attribute_values[i], 2048);
		}
		g_hash_table_insert(state->desktop_menu->menu_branches,
				g_strdup(state->cur_path), state->cur_branch);
	} else if(!strcmp(element_name, "separator")) {
		if((i=_find_attribute(attribute_names, "visible")) != -1 &&
				(!strcmp(attribute_values[i], "false") ||
				!strcmp(attribute_values[i], "no")))
		{
			return;
		}
		
		mi = gtk_separator_menu_item_new();
		gtk_widget_set_name(mi, "xfdesktopmenu");
		gtk_widget_show(mi);
		gtk_menu_shell_append(GTK_MENU_SHELL(state->cur_branch), mi);
	} else if(!strcmp(element_name, "builtin")) {
		if(state->hidelevel)
			return;
		
		if((i=_find_attribute(attribute_names, "visible")) != -1 &&
				(!strcmp(attribute_values[i], "false") ||
				!strcmp(attribute_values[i], "no")))
		{
			return;
		}
		
		i = _find_attribute(attribute_names, "name");
		if(i == -1)
			return;
		j = _find_attribute(attribute_names, "cmd");
		if(j == -1)
			return;
		k = (state->desktop_menu->use_menu_icons ? _find_attribute(attribute_names, "icon") : -1);
		
		if(k == -1)
			mi = gtk_menu_item_new_with_label(attribute_values[i]);
		else {
			GdkPixbuf *pix;
			GtkWidget *image;
			mi = gtk_image_menu_item_new_with_label(attribute_values[i]);
			pix = xfce_load_themed_icon(attribute_values[k],
					_xfce_desktop_menu_icon_size);
			if(pix) {
				image = gtk_image_new_from_pixbuf(pix);
				gtk_widget_show(image);
				gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), image);
				g_object_unref(pix);
			}
		}
		gtk_widget_set_name(mi, "xfdesktopmenu");
		g_signal_connect(G_OBJECT(mi), "activate", G_CALLBACK(_do_builtin),
				GINT_TO_POINTER(MI_BUILTIN_QUIT));
		gtk_widget_show(mi);
		gtk_menu_shell_append(GTK_MENU_SHELL(state->cur_branch), mi);
	} else if(!strcmp(element_name, "title")) {
		if(state->hidelevel)
			return;
		
		if((i=_find_attribute(attribute_names, "visible")) != -1 &&
				(!strcmp(attribute_values[i], "false") ||
				!strcmp(attribute_values[i], "no")))
		{
			return;
		}
		
		i = _find_attribute(attribute_names, "name");
		if(i == -1)
			return;
		j = _find_attribute(attribute_names, "icon");
		
		if(j == -1)
			mi = gtk_menu_item_new_with_label(attribute_values[i]);
		else {
			GdkPixbuf *pix;
			GtkWidget *image;
			mi = gtk_image_menu_item_new_with_label(attribute_values[i]);
			pix = xfce_load_themed_icon(attribute_values[k],
					_xfce_desktop_menu_icon_size);
			if(pix) {
				image = gtk_image_new_from_pixbuf(pix);
				gtk_widget_show(image);
				gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), image);
				g_object_unref(pix);
			}
		}
		gtk_widget_set_sensitive(mi, FALSE);
		gtk_widget_set_name(mi, "xfdesktopmenu");
		gtk_widget_show(mi);
		gtk_menu_shell_append(GTK_MENU_SHELL(state->cur_branch), mi);
	} else if(!strcmp(element_name, "include")) {
		if(state->hidelevel)
			return;
		
		if((i=_find_attribute(attribute_names, "visible")) != -1 &&
				(!strcmp(attribute_values[i], "false") ||
				!strcmp(attribute_values[i], "no")))
		{
			return;
		}
		
		i = _find_attribute(attribute_names, "type");
		if(i == -1)
			return;
		if(!strcmp(attribute_values[i], "file")) {
			j = _find_attribute(attribute_names, "src");
			if(j != -1) {
				if(*attribute_values[j] == '/') {
					desktop_menu_file_parse(state->desktop_menu,
							attribute_values[j], state->cur_branch,
							state->cur_path, FALSE);
				} else {
					gchar *menuincfile = xfce_get_userfile(attribute_values[j],
							NULL);
					desktop_menu_file_parse(state->desktop_menu, menuincfile,
							state->cur_branch, state->cur_path, FALSE);
					g_free(menuincfile);
				}
			}
		} else if(!strcmp(attribute_values[i], "system")) {
			gboolean do_legacy = TRUE, only_unique = TRUE;
			
			j = _find_attribute(attribute_names, "style");
			k = _find_attribute(attribute_names, "unique");
			l = _find_attribute(attribute_names, "legacy");
			
			if(k != -1 && !strcmp(attribute_values[i], "false"))
				only_unique = FALSE;				
			if(l != -1 && !strcmp(attribute_values[l], "false"))
				do_legacy = FALSE;
			
			state->desktop_menu->dentry_basepath = state->cur_path;
			BD("cur_path: %s", state->cur_path);
			state->desktop_menu->dentry_basemenu = state->cur_branch;

			if(j != -1 && !strcmp(attribute_values[j], "multilevel")) {
				if(only_unique) {
					desktop_menu_dentry_parse_files(state->desktop_menu,
							MPATH_MULTI_UNIQUE, do_legacy);
				} else {
					desktop_menu_dentry_parse_files(state->desktop_menu,
							MPATH_MULTI, do_legacy);
				}
			} else {
				if(only_unique) {
					desktop_menu_dentry_parse_files(state->desktop_menu,
							MPATH_SIMPLE_UNIQUE, do_legacy);
				} else {
					desktop_menu_dentry_parse_files(state->desktop_menu,
							MPATH_SIMPLE, do_legacy);
				}
			}
		}
	}
}

static void
menu_file_xml_end(GMarkupParseContext *context, const gchar *element_name,
		gpointer user_data, GError **error)
{
	struct MenuFileParserState *state = user_data;
	gchar *p;
	
	TRACE("dummy");
	
	if(!strcmp(element_name, "menu")) {
		if(state->hidelevel)
			state->hidelevel--;
		else {
			g_queue_pop_tail(state->branches);
			state->cur_branch = g_queue_peek_tail(state->branches);
			p = g_queue_pop_tail(state->paths);
			if(p)
				g_free(p);
			p = g_strrstr(state->cur_path, "/");
			if(p && p != state->cur_path)
				*p = 0;
			else if(p)
				*(p+1) = 0;
		}
	} else if(!strcmp(element_name, "xfdesktop-menu"))
		state->started = FALSE;
}

gboolean
desktop_menu_file_parse(XfceDesktopMenu *desktop_menu, const gchar *filename,
		GtkWidget *menu, const gchar *cur_path, gboolean is_root)
{
	gchar *other_filename = NULL;
	gchar *file_contents = NULL;
	GMarkupParseContext *gpcontext = NULL;
	int fd = -1;
	struct stat st;
	GMarkupParser gmparser = { 
		menu_file_xml_start,
		menu_file_xml_end,
		NULL,
		NULL,
		NULL
	};
	struct MenuFileParserState state;
	gboolean ret = FALSE;
	GError *err = NULL;
#ifdef HAVE_MMAP
	void *maddr;
#endif
	
	TRACE("dummy");

	g_return_val_if_fail(desktop_menu != NULL && menu != NULL, FALSE);
	
	maddr = NULL;

	if(!filename) {
		filename = other_filename = _menu_file_get();
		if(!filename)
			return FALSE;
	}
	
	if(stat(filename, &st) < 0) {
		g_warning("XfceDesktopMenu: unable to find a usable menu file\n");
		goto cleanup;
	}

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
	
	if(is_root) {
		if(desktop_menu->menufiles_watch) {
			g_list_free(desktop_menu->menufiles_watch);
			desktop_menu->menufiles_watch = NULL;
		}
		if(desktop_menu->menufile_mtimes) {
			g_free(desktop_menu->menufile_mtimes);
			desktop_menu->menufile_mtimes = NULL;
		}
		if(desktop_menu->dentrydir_mtimes) {
			g_free(desktop_menu->dentrydir_mtimes);
			desktop_menu->dentrydir_mtimes = NULL;
		}
		if(desktop_menu->legacydir_mtimes) {
			g_free(desktop_menu->legacydir_mtimes);
			desktop_menu->legacydir_mtimes = NULL;
		}
	}
	
	if(!desktop_menu->filename || other_filename) {
		if(desktop_menu->filename)
			g_free(desktop_menu->filename);
		desktop_menu->filename = g_strdup(filename);
	}
	state.started = FALSE;
	state.branches = g_queue_new();
	g_queue_push_tail(state.branches, menu);
	state.cur_branch = menu;
	state.paths = g_queue_new();
	g_queue_push_tail(state.paths, g_strdup(cur_path));
	g_strlcpy(state.cur_path, cur_path, 2048);
	state.desktop_menu = desktop_menu;
	state.hidelevel = 0;
	
	gpcontext = g_markup_parse_context_new(&gmparser, 0, &state, NULL);

    if(!g_markup_parse_context_parse(gpcontext, file_contents, st.st_size, &err)) {
		g_warning("%s: error parsing xfdesktop menu file (%d): %s\n",
				PACKAGE, err->code, err->message);
		g_error_free(err);
        goto cleanup;
	}

    if(g_markup_parse_context_end_parse(gpcontext, NULL))
		ret = TRUE;
	
	if(ret) {
		desktop_menu->menufiles_watch = g_list_prepend(desktop_menu->menufiles_watch,
				g_strdup(filename));
		if(is_root) {
			if(desktop_menu->menufile_mtimes)
				g_free(desktop_menu->menufile_mtimes);
			desktop_menu->menufile_mtimes = g_new0(time_t,
					g_list_length(desktop_menu->menufiles_watch));
			desktop_menu_file_need_update(desktop_menu);
		}
	}
	
	cleanup:
	
	if(gpcontext)
		g_markup_parse_context_free(gpcontext);
#ifdef HAVE_MMAP
	if(maddr)
		munmap(maddr, st.st_size);
	else
#endif
	if(file_contents)
		free(file_contents);
	if(other_filename)
		g_free(other_filename);
	if(fd > -1)
		close(fd);
	if(state.branches)
		g_queue_free(state.branches);
	if(state.paths) {
#if !GTK_CHECK_VERSION(2, 4, 0)
		gchar *tmp;
		while((tmp=g_queue_pop_tail(state.paths)))
			g_free(tmp);
#else
		g_queue_foreach(state.paths, (GFunc)g_free, NULL);
#endif
		g_queue_free(state.paths);
	}
	
	return ret;
}

gboolean
desktop_menu_file_need_update(XfceDesktopMenu *desktop_menu)
{
	GList *l;
	struct stat st;
	char *filename;
	gboolean modified = FALSE;
	gint i;
	
	TRACE("dummy");
	
	g_return_val_if_fail(desktop_menu != NULL, FALSE);
	
	if(!desktop_menu->menu)
		return TRUE;

	for(i=0,l=desktop_menu->menufiles_watch; l; l=l->next,i++) {
		filename = (char *)l->data;
		
		if(!stat(filename, &st) && st.st_mtime > desktop_menu->menufile_mtimes[i]) {
			desktop_menu->menufile_mtimes[i] = st.st_mtime;
			modified = TRUE;
		}
	}
	if(desktop_menu->using_system_menu)
		modified = (desktop_menu_dentry_need_update(desktop_menu) ? TRUE : modified);
	
	return modified;
}
