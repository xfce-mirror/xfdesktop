/*   about_dialog.c */

/*  Copyright (C)  Jean-François Wauthy under GNU GPL
 *
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

#include "about_dialog.h"

/*************/
/* About box */
/*************/
void about_cb(GtkWidget *widget, gpointer data)
{
  XfceAboutInfo *info;
  GtkWidget *dialog;
  GdkPixbuf *icon;

  info = xfce_about_info_new(
      "xfce4-menueditor",
      VERSION,
      _("An menueditor for XFce4"),
      XFCE_COPYRIGHT_TEXT("2004", "Jean-Francois Wauthy"),
      XFCE_LICENSE_LGPL);
  xfce_about_info_set_homepage(info, "http://users.skynet.be/p0llux/");
  xfce_about_info_add_credit(info,
      "Jean-Francois Wauthy",
      "pollux@castor.be",
      _("Core developer"));

  icon = gdk_pixbuf_new_from_xpm_data(icon48_xpm);

  dialog = xfce_about_dialog_new(GTK_WINDOW(menueditor_app.main_window), 
				 info, icon);

  xfce_about_info_free(info);

  gtk_dialog_run(GTK_DIALOG(dialog));
  
  gtk_widget_destroy(dialog);

  g_object_unref(icon);
}
