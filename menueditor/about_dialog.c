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
#ifdef DISABLE_CVS
void about_cb(GtkWidget *widget, gpointer data)
{
  gchar str_about[100]="";
  GtkWidget *label_about;
  GtkWidget *header;
  gchar *header_text;
  GtkWidget *hbox = gtk_hbox_new(FALSE,0);
  GtkWidget *dialog = gtk_dialog_new_with_buttons(_("About XFCE4-MenuEditor"),
						  GTK_WINDOW(menueditor_app.main_window),
						  GTK_DIALOG_DESTROY_WITH_PARENT,
						  GTK_STOCK_CLOSE,
						  GTK_RESPONSE_CLOSE,NULL);
  GdkPixbuf *icon;
  GtkWidget *image;

  /* Header */
  header_text = g_strdup_printf("%s", _("About XFce4-MenuEditor"));
  header = create_header (NULL, header_text);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->vbox), header, FALSE, FALSE, 0);
  g_free(header_text);
  
  /* Image */
  icon = gdk_pixbuf_new_from_xpm_data(icon48_xpm);
  image = gtk_image_new_from_pixbuf(icon);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

  /* Label */
  /* why cedilla doesn't work ??? UTF8 error ! */
  g_sprintf(str_about, _("This is XFce4-MenuEditor (version %s)\nLicensed under GNU-GPL\n(c) 2004 by Jean-Francois Wauthy"), VERSION);

  label_about = gtk_label_new(str_about);
  gtk_box_pack_start (GTK_BOX (hbox), label_about, FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->vbox), hbox, FALSE, FALSE, 0);
  gtk_window_set_default_size(GTK_WINDOW(dialog),315,130);

  gtk_widget_show_all(dialog);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
  g_object_unref(icon);
}
#else
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

  g_object_unref(icon);
}
#endif
