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

#include "menueditor.h"
#include "about_dialog.h"

/*************/
/* About box */
/*************/
void
about_cb (GtkWidget * widget, gpointer data)
{
  MenuEditor *me;
  XfceAboutInfo *info;
  GtkWidget *dialog;
  GdkPixbuf *icon;

  me = (MenuEditor *) data;

  info =
    xfce_about_info_new ("xfce4-menueditor", VERSION,
                         _("A menu editor for Xfce4"), XFCE_COPYRIGHT_TEXT ("2004", "Xfce Dev Team"), XFCE_LICENSE_GPL);
  xfce_about_info_set_homepage (info, "http://www.xfce.org/");

  /* Credits */
  xfce_about_info_add_credit (info, "Jean-François Wauthy", "pollux@xfce.org", _("Core developer"));
  xfce_about_info_add_credit (info, "Brian Tarricone", "bjt23@cornell.edu", _("Contributor"));
  xfce_about_info_add_credit (info, "Danny Milosavljevic", "danny.milo@gmx.net", _("Contributor"));
  xfce_about_info_add_credit (info, "Jens Luedicke", "perldude@lunar-linux.org", _("Contributor"));
  xfce_about_info_add_credit (info, "Francois Le Clainche", "fleclainche@xfce.org", _("Icon designer"));

  icon = xfce_icon_theme_load (me->icon_theme, "xfce4-menueditor", 48);

  dialog = xfce_about_dialog_new (GTK_WINDOW (me->main_window), info, icon);

  gtk_window_set_default_size (GTK_WINDOW (dialog), 500, 400);
  xfce_about_info_free (info);

  gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);

  g_object_unref (icon);
}
