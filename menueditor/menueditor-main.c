/* $Id$ */
/*
 * Copyright (c) 2006 Jean-François Wauthy (pollux@xfce.org)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <stdlib.h>

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "menueditor-main-window.h"

/* entry point */
int
main (int argc, char **argv)
{
  GtkWidget *mainwin;

  g_set_application_name (_("Xfce4-MenuEditor"));

  if (argc > 1 && (!strcmp (argv[1], "--version") || !strcmp (argv[1], "-V"))) {
    g_print ("\tThis is %s version %s for Xfce %s\n", PACKAGE, VERSION, xfce_version_string ());
    g_print ("\tbuilt with GTK+-%d.%d.%d, ", GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);
    g_print ("linked with GTK+-%d.%d.%d.\n", gtk_major_version, gtk_minor_version, gtk_micro_version);

    exit (EXIT_SUCCESS);
  }

  gtk_init (&argc, &argv);

  xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

  mainwin = menueditor_main_window_new ();

  gtk_widget_show (mainwin);

  gtk_main ();
  return EXIT_SUCCESS;
}
