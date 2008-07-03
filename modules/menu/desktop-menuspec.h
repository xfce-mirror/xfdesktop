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

/**
 * @file src/menuspec.h Freedesktop.org menu spec category sorter.
 * @author Brian Tarricone <bjt23@cornell.edu>
 */

#ifndef __MENUSPEC_H__
#define __MENUSPEC_H__

#include <glib.h>

/**
 * @brief Initialises the category parser.
 * 
 * Parses the xfce-registered-categories.xml file and initialises the menu
 * path generator.  call this first before doing anything else.
 *
 * @param filename Path to the xfce-registered-categories.xml file to parse.
 * 
 * @return TRUE on success, FALSE on error.
 **/
gboolean desktop_menuspec_parse_categories(const gchar *filename);

/**
 * @brief Frees parser memory.
 *
 * Frees all allocated data associated with the parser.  Call this when you're
 * done using data associated with the category parser.
 **/
void desktop_menuspec_free();

/**
 * @brief Get a list of simplified menu paths.
 *
 * Generates simple (single-level) path names suitable for a GtkItemFactory
 * given a list of semicolon-delimited categories.
 *
 * @param categories A semicolon-delimited list of categories.
 *
 * @return A GPtrArray of pointers to path strings.
 */
GPtrArray *desktop_menuspec_get_path_simple(const gchar *categories);

/**
 * @brief Get a list of multi-level menu paths.
 *
 * Generates multi-level path names suitable for a GtkItemFactory given a list
 * of semicolon-delimited categories.
 *
 * @param categories A semicolon-delimited list of categories.
 *
 * @return A GPtrArray of pointers to path strings.
 */
GPtrArray *desktop_menuspec_get_path_multilevel(const gchar *categories);

G_CONST_RETURN gchar *desktop_menuspec_cat_to_displayname(const gchar *category);
G_CONST_RETURN gchar *desktop_menuspec_displayname_to_icon(const gchar *displayname);
/**
 * @brief Frees menu path lists.
 *
 * Frees memory associated with an array of paths returned by one of the
 * menuspec_get_path_* functions.
 *
 * @param paths A GPtrArray obtained from either menuspec_get_path_simple() or
 *              menuspec_get_path_multilevel().
 */
void desktop_menuspec_path_free(GPtrArray *paths);

#endif /* ifdef __MENUSPEC_H__ */
