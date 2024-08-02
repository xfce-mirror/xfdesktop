/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004-2023 The Xfce Development Team
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

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <cairo-xlib.h>
#include <gdk/gdkx.h>

#include "xfdesktop-common.h"
#include "xfdesktop-x11.h"

#define XFDESKTOP_SELECTION_FMT  "XFDESKTOP_SELECTION_%d"
#define XFDESKTOP_IMAGE_FILE_FMT "XFDESKTOP_IMAGE_FILE_%d"

/* disable setting the x background for bug 7442 */
//#define DISABLE_FOR_BUG7442

typedef struct _WaitForWM {
    WaitForWMCompleteCallback complete_callback;
    gpointer complete_data;
    GCancellable *cancellable;

    Display *dpy;
    Atom *atoms;
    guint atom_count;
    gboolean have_wm;
    guint counter;
} WaitForWM;


static void
event_forward_to_rootwin(GdkScreen *gscreen, GdkEvent *event)
{
    XButtonEvent xev, xev2;
    Display *dpy = GDK_DISPLAY_XDISPLAY(gdk_screen_get_display(gscreen));

    if(event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE) {
        if(event->type == GDK_BUTTON_PRESS) {
            xev.type = ButtonPress;
            /*
             * rox has an option to disable the next
             * instruction. it is called "blackbox_hack". Does
             * anyone know why exactly it is needed?
             */
            XUngrabPointer(dpy, event->button.time);
        } else
            xev.type = ButtonRelease;

        xev.button = event->button.button;
        xev.x = event->button.x;    /* Needed for icewm */
        xev.y = event->button.y;
        xev.x_root = event->button.x_root;
        xev.y_root = event->button.y_root;
        xev.state = event->button.state;

        xev2.type = 0;
    } else if(event->type == GDK_SCROLL) {
        xev.type = ButtonPress;
        xev.button = event->scroll.direction + 4;
        xev.x = event->scroll.x;    /* Needed for icewm */
        xev.y = event->scroll.y;
        xev.x_root = event->scroll.x_root;
        xev.y_root = event->scroll.y_root;
        xev.state = event->scroll.state;

        xev2.type = ButtonRelease;
        xev2.button = xev.button;
    } else
        return;
    xev.window = GDK_WINDOW_XID(gdk_screen_get_root_window(gscreen));
    xev.root =  xev.window;
    xev.subwindow = None;
    xev.time = event->button.time;
    xev.same_screen = True;

    XSendEvent(dpy, xev.window, False, ButtonPressMask | ButtonReleaseMask,
            (XEvent *)&xev);
    if(xev2.type == 0)
        return;

    /* send button release for scroll event */
    xev2.window = xev.window;
    xev2.root = xev.root;
    xev2.subwindow = xev.subwindow;
    xev2.time = xev.time;
    xev2.x = xev.x;
    xev2.y = xev.y;
    xev2.x_root = xev.x_root;
    xev2.y_root = xev.y_root;
    xev2.state = xev.state;
    xev2.same_screen = xev.same_screen;

    XSendEvent(dpy, xev2.window, False, ButtonPressMask | ButtonReleaseMask,
            (XEvent *)&xev2);
}

gboolean
xfdesktop_x11_desktop_scrolled(GtkWidget *widget, GdkEventScroll *event) {
    event_forward_to_rootwin(gtk_widget_get_screen(widget), (GdkEvent *)event);
    return TRUE;
}

void
xfdesktop_x11_set_root_image_file_property(GdkScreen *gscreen, gint monitor_idx, const gchar *filename) {
    GdkDisplay *display = gdk_screen_get_display(gscreen);
    gchar *property_name = g_strdup_printf(XFDESKTOP_IMAGE_FILE_FMT, monitor_idx);

    gdk_x11_display_error_trap_push(display);
    if (filename != NULL) {
        gdk_property_change(gdk_screen_get_root_window(gscreen),
                            gdk_atom_intern(property_name, FALSE),
                            gdk_x11_xatom_to_atom(XA_STRING),
                            8,
                            GDK_PROP_MODE_REPLACE,
                            (guchar *)filename,
                            strlen(filename)+1);
    } else {
        gdk_property_delete(gdk_screen_get_root_window(gscreen),
                            gdk_atom_intern(property_name, FALSE));
    }
    gdk_x11_display_error_trap_pop_ignored(display);

    g_free(property_name);
}

void
xfdesktop_x11_set_root_image_surface(GdkScreen *gscreen, cairo_surface_t *surface) {
#ifndef DISABLE_FOR_BUG7442
    GdkWindow *groot = gdk_screen_get_root_window(gscreen);
    GdkAtom prop_atom = gdk_atom_intern("_XROOTPMAP_ID", FALSE);
    cairo_pattern_t *pattern = NULL;

    if (surface != NULL) {
        Pixmap pixmap_id = cairo_xlib_surface_get_drawable(surface);
        pattern = cairo_pattern_create_for_surface(surface);

        GdkDisplay *display = gdk_screen_get_display(gscreen);
        xfw_windowing_error_trap_push(display);

        /* set root property for transparent Eterms */
        gdk_property_change(groot,
                            prop_atom,
                            gdk_atom_intern("PIXMAP", FALSE),
                            32,
                            GDK_PROP_MODE_REPLACE,
                            (guchar *)&pixmap_id,
                            1);
        /* there really should be a standard for this crap... */

        xfw_windowing_error_trap_pop_ignored(display);
    } else {
        gdk_property_delete(groot, prop_atom);
    }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    /* and set the root window's BG surface, because aterm is somewhat lame. */
    gdk_window_set_background_pattern(groot, pattern);
G_GNUC_END_IGNORE_DEPRECATIONS

    if (pattern != NULL) {
        cairo_pattern_destroy(pattern);
    }
#endif
}

void
xfdesktop_x11_set_compat_properties(GtkWidget *desktop) {
    GdkDisplay *gdpy = desktop != NULL
        ? gdk_screen_get_display(gtk_widget_get_screen(desktop))
        : gdk_display_get_default();
    GdkWindow *groot = desktop != NULL
        ? gdk_screen_get_root_window(gtk_widget_get_screen(desktop))
        : gdk_screen_get_root_window(gdk_screen_get_default());

    gdk_x11_display_error_trap_push(gdpy);

    GdkAtom xfce_desktop_window_atom = gdk_atom_intern_static_string("XFCE_DESKTOP_WINDOW");
    GdkAtom nautilus_desktop_window_atom = gdk_atom_intern_static_string("NAUTILUS_DESKTOP_WINDOW_ID");

    if (desktop != NULL) {
        Window xid = gdk_x11_window_get_xid(gtk_widget_get_window(desktop));

        gdk_property_change(groot,
                            xfce_desktop_window_atom,
                            gdk_atom_intern("WINDOW", FALSE), 32,
                            GDK_PROP_MODE_REPLACE, (guchar *)&xid, 1);

        gdk_property_change(groot,
                            nautilus_desktop_window_atom,
                            gdk_atom_intern("WINDOW", FALSE), 32,
                            GDK_PROP_MODE_REPLACE, (guchar *)&xid, 1);
    } else {
        gdk_property_delete(groot, xfce_desktop_window_atom);
        gdk_property_delete(groot, nautilus_desktop_window_atom);
    }

    gdk_x11_display_error_trap_pop_ignored(gdpy);
}

GdkWindow *
xfdesktop_x11_set_desktop_manager_selection(GdkScreen *gscreen, GError **error) {
    g_return_val_if_fail(GDK_IS_SCREEN(gscreen), NULL);
    g_return_val_if_fail(error != NULL && *error == NULL, NULL);

    GdkDisplay *gdisplay = gdk_screen_get_display(gscreen);
    Display *display = gdk_x11_display_get_xdisplay(gdisplay);
    gint screen_num = gdk_x11_screen_get_screen_number(gscreen);

    gchar *common_selection_name = g_strdup_printf("_NET_DESKTOP_MANAGER_S%d", screen_num);
    GdkAtom common_selection_atom = gdk_atom_intern(common_selection_name, FALSE);

    gchar *xfce_selection_name = g_strdup_printf(XFDESKTOP_SELECTION_FMT, screen_num);
    GdkAtom xfce_selection_atom = gdk_atom_intern(xfce_selection_name, FALSE);

    GdkWindow *selection_window = NULL;

    // We have to use Xlib for these, as the GDK functions only return
    // the selection owner if it's owned by a GdkWindow known to GDK,
    // which will never be the case.
    if (XGetSelectionOwner(display, gdk_x11_atom_to_xatom_for_display(gdisplay, common_selection_atom)) != None) {
        *error = g_error_new(G_IO_ERROR,
                             G_IO_ERROR_ADDRESS_IN_USE,
                             "Another desktop manager is already running on screen %d",
                             screen_num);
    } else if (XGetSelectionOwner(display, gdk_x11_atom_to_xatom_for_display(gdisplay, xfce_selection_atom)) != None) {
        *error = g_error_new(G_IO_ERROR,
                             G_IO_ERROR_ADDRESS_IN_USE,
                             "Another instance of xfdesktop is already running on screen %d",
                             screen_num);
    } else {
        GdkWindowAttr attrs = {
            .window_type = GDK_WINDOW_TOPLEVEL,
            .override_redirect = TRUE,
            .width = 1,
            .height = 1,
            .x = -100,
            .y = -100,
            .wmclass_name = "xfdesktop",
            .wmclass_class = "Xfdesktop",
            .title = "Xfdesktop Manager Selection",
            .event_mask = GDK_BUTTON_PRESS |
                GDK_PROPERTY_CHANGE_MASK |
                GDK_STRUCTURE_MASK |
                GDK_SUBSTRUCTURE_MASK,
        };
        selection_window = gdk_window_new(gdk_screen_get_root_window(gscreen),
                                          &attrs,
                                          GDK_WA_TITLE |
                                          GDK_WA_WMCLASS |
                                          GDK_WA_X |
                                          GDK_WA_Y |
                                          GDK_WA_NOREDIR);

        if (!gdk_selection_owner_set_for_display(gdisplay,
                                                 selection_window,
                                                 common_selection_atom,
                                                 GDK_CURRENT_TIME,
                                                 TRUE))
        {
            *error = g_error_new(G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Unable to acquire selection '%s'",
                                 common_selection_name);
        } else if (!gdk_selection_owner_set_for_display(gdisplay,
                                                        selection_window,
                                                        xfce_selection_atom,
                                                        GDK_CURRENT_TIME,
                                                        TRUE))
        {
            *error = g_error_new(G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Unable to acquire selection '%s'",
                                 xfce_selection_name);
        } else {
            Window new_owner = XGetSelectionOwner(display, gdk_x11_atom_to_xatom_for_display(gdisplay, xfce_selection_atom));
            if (new_owner != gdk_x11_window_get_xid(selection_window)) {
                *error = g_error_new(G_IO_ERROR,
                                     G_IO_ERROR_FAILED,
                                     "Failed to acquire selection '%s'",
                                     xfce_selection_name);
            } else {
                Window xroot = gdk_x11_window_get_xid(gdk_screen_get_root_window(gscreen));
                XClientMessageEvent xev;
                xev.type = ClientMessage;
                xev.window = xroot;
                xev.message_type = XInternAtom(display, "MANAGER", False);
                xev.format = 32;
                xev.data.l[0] = GDK_CURRENT_TIME;
                xev.data.l[1] = gdk_x11_atom_to_xatom_for_display(gdisplay, xfce_selection_atom);
                xev.data.l[2] = gdk_x11_window_get_xid(selection_window);
                xev.data.l[3] = 0;    /* manager specific data */
                xev.data.l[4] = 0;    /* manager specific data */
                XSendEvent(display, xroot, False, StructureNotifyMask, (XEvent *)&xev);
            }
        }
    }

    if (*error != NULL) {
        g_clear_pointer(&selection_window, gdk_window_destroy);
    }

    g_free(common_selection_name);
    g_free(xfce_selection_name);

    return selection_window;
}

static void
wait_for_wm_free(WaitForWM *wfwm) {
    g_object_unref(wfwm->cancellable);

    g_free(wfwm->atoms);
    XCloseDisplay(wfwm->dpy);

    g_free(wfwm);
}

static gboolean
cb_wait_for_wm_timeout(gpointer data)
{
    WaitForWM *wfwm = data;
    guint i;
    gboolean have_wm = TRUE;

    /* Check if it was canceled. This way xfdesktop doesn't start up if
     * we're quitting */
    if (g_cancellable_is_cancelled(wfwm->cancellable)) {
        return FALSE;
    }

    for(i = 0; i < wfwm->atom_count; i++) {
        if (XGetSelectionOwner(wfwm->dpy, wfwm->atoms[i]) == None) {
            XF_DEBUG("window manager not ready on screen %d", i);
            have_wm = FALSE;
            break;
        }
    }

    wfwm->have_wm = have_wm;

    /* abort if a window manager is found or 5 seconds expired */
    return wfwm->counter++ < 20 * 5 && !wfwm->have_wm;
}

static void
cb_wait_for_wm_timeout_destroyed(gpointer data)
{
    WaitForWM *wfwm = data;
    WaitForWMStatus status;

    /* Check if it was canceled. This way xfdesktop doesn't start up if
     * we're quitting */

    if (g_cancellable_is_cancelled(wfwm->cancellable)) {
        status = WAIT_FOR_WM_CANCELLED;
    } else {
        status = wfwm->have_wm ? WAIT_FOR_WM_SUCCESSFUL : WAIT_FOR_WM_FAILED;
    }

    // Inform the caller that we're done
    wfwm->complete_callback(status, wfwm->complete_data);
    wait_for_wm_free(wfwm);
}

void
xfdesktop_x11_wait_for_wm(WaitForWMCompleteCallback complete_callback,
                          gpointer complete_data,
                          GCancellable *cancellable)
{
    WaitForWM *wfwm;
    guint i;
    gchar **atom_names;

    /* setup data for wm checking */
    wfwm = g_new0(WaitForWM, 1);
    wfwm->complete_callback = complete_callback;
    wfwm->complete_data = complete_data;
    wfwm->cancellable = g_object_ref(cancellable);
    wfwm->dpy = XOpenDisplay(NULL);
    wfwm->have_wm = FALSE;
    wfwm->counter = 0;

    /* preload wm atoms for all screens */
    wfwm->atom_count = XScreenCount(wfwm->dpy);
    wfwm->atoms = g_new(Atom, wfwm->atom_count);
    atom_names = g_new0(gchar *, wfwm->atom_count + 1);

    for (i = 0; i < wfwm->atom_count; i++) {
        atom_names[i] = g_strdup_printf("WM_S%d", i);
    }

    if (!XInternAtoms(wfwm->dpy, atom_names, wfwm->atom_count, False, wfwm->atoms)) {
        wfwm->atom_count = 0;
    }

    g_strfreev(atom_names);

    /* setup timeout to check for a window manager */
    g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE,
                       50,
                       cb_wait_for_wm_timeout,
                       wfwm,
                       cb_wait_for_wm_timeout_destroyed);
}

gboolean
xfdesktop_x11_get_full_workarea(GdkScreen *gscreen, GdkRectangle *workarea) {
    gboolean ret = FALSE;
    GdkRectangle new_workarea = { 0, };

    GdkDisplay *gdisplay = gdk_screen_get_display(gscreen);
    Display *dpy = gdk_x11_display_get_xdisplay(gdisplay);
    Window root = gdk_x11_window_get_xid(gdk_screen_get_root_window(gscreen));
    Atom property = XInternAtom(dpy, "_NET_WORKAREA", False);
    Atom actual_type = None;
    gint actual_format = 0, first_id;
    gulong nitems = 0, bytes_after = 0, offset = 0, tmp_size = 0;
    unsigned char *data_p = NULL;
    gint scale_factor = gdk_monitor_get_scale_factor(gdk_display_get_monitor(gdisplay, 0));

    // We only really support a single workarea that all workspaces share,
    // otherwise this would be 'ws_num * 4'
    first_id = 0;

    gdk_x11_display_error_trap_push(gdisplay);

    do {
        if (Success == XGetWindowProperty(dpy, root, property, offset,
                                          G_MAXULONG, False, XA_CARDINAL,
                                          &actual_type, &actual_format, &nitems,
                                          &bytes_after, &data_p))
        {
            gint i;
            gulong *data;

            if(actual_format != 32 || actual_type != XA_CARDINAL) {
                XFree(data_p);
                break;
            }

            tmp_size = (actual_format / 8) * nitems;
            if(actual_format == 32) {
                tmp_size *= sizeof(long)/4;
            }

            data = g_malloc(tmp_size);
            memcpy(data, data_p, tmp_size);

            i = offset / 32;  /* first element id in this batch */

            /* there's probably a better way to do this. */
            if(i + (glong)nitems >= first_id && first_id - (glong)offset >= 0)
                new_workarea.x = data[first_id - offset] / scale_factor;
            if(i + (glong)nitems >= first_id + 1 && first_id - (glong)offset + 1 >= 0)
                new_workarea.y = data[first_id - offset + 1] / scale_factor;
            if(i + (glong)nitems >= first_id + 2 && first_id - (glong)offset + 2 >= 0)
                new_workarea.width = data[first_id - offset + 2] / scale_factor;
            if(i + (glong)nitems >= first_id + 3 && first_id - (glong)offset + 3 >= 0) {
                new_workarea.height = data[first_id - offset + 3] / scale_factor;
                ret = TRUE;
                XFree(data_p);
                g_free(data);
                break;
            }

            offset += actual_format * nitems;
            XFree(data_p);
            g_free(data);
        } else {
            break;
        }
    } while (bytes_after > 0);

    gdk_x11_display_error_trap_pop_ignored(gdisplay);

    if (ret) {
        *workarea = new_workarea;
    }

    return ret;
}
