/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004 Brian Tarricone, <bjt23@cornell.edu>
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

#ifndef _XFCE_DESKTOP_H_
#define _XFCE_DESKTOP_H_

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtkwindow.h>

#include <libxfce4mcs/mcs-client.h>

#include "xfce-backdrop.h"

G_BEGIN_DECLS

#define XFCE_TYPE_DESKTOP              (xfce_desktop_get_type())
#define XFCE_DESKTOP(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), XFCE_TYPE_DESKTOP, XfceDesktop))
#define XFCE_DESKTOP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), XFCE_TYPE_DESKTOP, XfceDesktopClass))
#define XFCE_IS_DESKTOP(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), XFCE_TYPE_DESKTOP))
#define XFCE_IS_DESKTOP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), XFCE_TYPE_DESKTOP))
#define XFCE_DESKTOP_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), XFCE_TYPE_DESKTOP, XfceDesktopClass))

typedef struct _XfceDesktop XfceDesktop;
typedef struct _XfceDesktopClass XfceDesktopClass;
typedef struct _XfceDesktopPriv XfceDesktopPriv;

struct _XfceDesktop
{
	GtkWindow window;
	
	/*< private >*/
	XfceDesktopPriv *priv;
};

struct _XfceDesktopClass
{
	GtkWindowClass parent_class;
};

GType xfce_desktop_get_type                     () G_GNUC_CONST;

GtkWidget *xfce_desktop_new(GdkScreen *gscreen, McsClient *mcs_client);

guint xfce_desktop_get_n_monitors(XfceDesktop *desktop);

XfceBackdrop *xfce_desktop_get_backdrop(XfceDesktop *desktop, guint n);

gint xfce_desktop_get_width(XfceDesktop *desktop);
gint xfce_desktop_get_height(XfceDesktop *desktop);

gboolean xfce_desktop_settings_changed(McsClient *client, McsAction action, McsSetting *setting, gpointer user_data);

G_END_DECLS

#endif
