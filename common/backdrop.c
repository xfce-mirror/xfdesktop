/*
 *  Copyright (C) 2002 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *  Copyright (C) 2003 Benedikt Meurer (benedikt.meurer@unix-ag.uni-siegen.de)
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib.h>
#include <libxfcegui4/dialogs.h>

#include "backdrop.h"


gchar **
get_list_from_file(const gchar *filename)
{
    gchar *contents;
    GError *error;
    gchar **files;
    gsize length;

    files = NULL;

    if (!g_file_get_contents(filename, &contents, &length, &error)) {
        xfce_err("Unable to get backdrop image list from file %s: %s",
                filename, error->message);
        g_error_free(error);
        return(NULL);
    }

    if (strncmp(LIST_TEXT, contents, sizeof(LIST_TEXT) - 1) != 0) {
        xfce_err("Not a backdrop image list file: %s", filename);
        goto finished;
    }

    files = g_strsplit(contents + sizeof(LIST_TEXT), "\n", -1);

finished:
    g_free(contents);

    return(files);
}

