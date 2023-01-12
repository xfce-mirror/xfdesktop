/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2022 Brian Tarricone, <brian@tarricone.org>
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
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "xfdesktop-extensions.h"

GList *
xfdesktop_g_list_last(GList *list,
                      guint *length)
{
    gint n = 0;

    if (list != NULL) {
        ++n;
        while (list->next != NULL) {
            list = list->next;
            ++n;
        }
    }

    if (length != NULL) {
        *length = n;
    }

    return list;
}

GList *
xfdesktop_g_list_append(GList *list,
                        gpointer data,
                        GList **new_link,
                        guint *new_length)
{
    GList *new_list;
    GList *last;
    guint old_length = 0;

    new_list = g_list_alloc();
    new_list->data = data;
    new_list->next = NULL;

    if (list != NULL) {
        last = xfdesktop_g_list_last(list, &old_length);
        last->next = new_list;
        new_list->prev = last;
    } else {
        new_list->prev = NULL;
        list = new_list;
    }

    if (new_link != NULL) {
        *new_link = new_list;
    }

    if (new_length != NULL) {
        *new_length = old_length + 1;
    }

    return list;
}

