/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004-2008 Brian J. Tarricone <bjt23@cornell.edu>
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

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <dbus/dbus-gtype-specialized.h>

#include "xfdesktop-common.h"
#include "xfce-desktop-settings.h"

static void
save_list_file_minus_one(const gchar *filename, const gchar **files, gint badi)
{
    FILE *fp;
    gint fd, i;

#ifdef O_EXLOCK
    if((fd = open (filename, O_CREAT|O_EXLOCK|O_TRUNC|O_WRONLY, 0640)) < 0) {
#else
    if((fd = open (filename, O_CREAT| O_TRUNC|O_WRONLY, 0640)) < 0) {
#endif
        xfce_err (_("Could not save file %s: %s\n\n"
                "Please choose another location or press "
                "cancel in the dialog to discard your changes"),
                filename, g_strerror(errno));
        return;
    }

    if((fp = fdopen (fd, "w")) == NULL) {
        g_warning ("Unable to fdopen(%s). This should not happen!\n", filename);
        close(fd);
        return;
    }

    fprintf (fp, "%s\n", LIST_TEXT);
    
    for(i = 0; files[i] && *files[i] && *files[i] != '\n'; i++) {
        if(i != badi)
            fprintf(fp, "%s\n", files[i]);
    }
    
    fclose(fp);
}

static inline gint
count_elements(const gchar **list)
{
    gint i, c = 0;
    
    if(!list)
        return 0;
    
    for(i = 0; list[i]; i++) {
        if(*list[i] && *list[i] != '\n')
            c++;
    }
    
    return c;
}

static const gchar **
get_listfile_contents(const gchar *listfile)
{
    static gchar *prevfile = NULL;
    static gchar **files = NULL;
    static time_t mtime = 0;
    struct stat st;
    
    if(!listfile) {
        if(prevfile) {
            g_free(prevfile);
            prevfile = NULL;
        }
        return NULL;
    }
    
    if(stat(listfile, &st) < 0) {
        if(prevfile) {
            g_free(prevfile);
            prevfile = NULL;
        }
        mtime = 0;
        return NULL;
    }
    
    if(!prevfile || strcmp(listfile, prevfile) || mtime < st.st_mtime) {
        if(files)
            g_strfreev(files);
        if(prevfile)
            g_free(prevfile);
    
        files = get_list_from_file(listfile);
        prevfile = g_strdup(listfile);
        mtime = st.st_mtime;
    }
    
    return (const gchar **)files;
}

static const gchar *
get_path_from_listfile(const gchar *listfile)
{
    static gboolean __initialized = FALSE;
    static gint previndex = -1;
    gint i, n;
    const gchar **files;
    
    /* NOTE: 4.3BSD random()/srandom() are a) stronger and b) faster than
    * ANSI-C rand()/srand(). So we use random() if available
    */
    if (!__initialized)    {
        guint seed = time(NULL) ^ (getpid() + (getpid() << 15));
#ifdef HAVE_SRANDOM
        srandom(seed);
#else
        srand(seed);
#endif
        __initialized = TRUE;
    }
    
    do {
        /* get the contents of the list file */
        files = get_listfile_contents(listfile);
        
        /* if zero or one item, return immediately */
        n = count_elements(files);
        if(!n)
            return NULL;
        else if(n == 1)
            return (const gchar *)files[0];
        
        /* pick a random item */
        do {
#ifdef HAVE_SRANDOM
            i = random() % n;
#else
            i = rand() % n;
#endif
            if(i != previndex) /* same as last time? */
                break;
        } while(1);
        
        DBG("picked i=%d, %s\n", i, files[i]);
        /* validate the image; if it's good, return it */
        if(xfdesktop_check_image_file(files[i]))
            break;
        
        DBG("file not valid, ditching\n");
        
        /* bad image: remove it from the list and write it out */
        save_list_file_minus_one(listfile, files, i);
        previndex = -1;
        /* loop and try again */
    } while(1);
    
    return (const gchar *)files[(previndex = i)];
}

void
xfce_desktop_settings_load_initial(XfceDesktop *desktop,
                                   XfconfChannel *channel)
{
    GdkScreen *gscreen = GTK_WINDOW(desktop)->screen;
    gint screen, i, nmonitors;
    XfceBackdrop *backdrop;
    gchar prop[256];
    
    TRACE("entering");

    g_return_if_fail(XFCONF_IS_CHANNEL(channel));
    
    xfce_desktop_freeze_updates(desktop);
    
    gtk_widget_realize(GTK_WIDGET(desktop));
    screen = gdk_screen_get_number(gscreen);
    nmonitors = xfce_desktop_get_n_monitors(desktop);
    
    g_snprintf(prop, sizeof(prop), "/backdrop/screen%d/xinerama-stretch",
               screen);
    xfce_desktop_set_xinerama_stretch(desktop,
                                      xfconf_channel_get_bool(channel,
                                                              prop,
                                                              FALSE));

#ifdef ENABLE_DESKTOP_ICONS
    xfce_desktop_set_icon_style(desktop,
                                xfconf_channel_get_int(channel,
                                                       "/desktop-icons/style",
#ifdef ENABLE_FILE_ICONS
                                                       XFCE_DESKTOP_ICON_STYLE_FILES
#else
                                                       XFCE_DESKTOP_ICON_STYLE_WINDOWS
#endif
                                                       ));

    xfce_desktop_set_icon_font_size(desktop,
                                    xfconf_channel_get_double(channel,
                                                              "/desktop-icons/font-size",
                                                              DEFAULT_ICON_FONT_SIZE));

    xfce_desktop_set_use_icon_font_size(desktop,
                                        xfconf_channel_get_bool(channel,
                                                                "/desktop-icons/use-custom-font-size",
                                                                FALSE));
    
    xfce_desktop_set_icon_size(desktop,
                               xfconf_channel_get_uint(channel,
                                                       "/desktop-icons/icon-size",
                                                       DEFAULT_ICON_SIZE));
#endif
    
    for(i = 0; i < nmonitors; i++) {
        gchar *v_str;
        GdkColor color;

        backdrop = xfce_desktop_peek_backdrop(desktop, i); 
        
        g_snprintf(prop, sizeof(prop),
                   "/backdrop/screen%d/monitor%d/image-show", screen, i);
        xfce_backdrop_set_show_image(backdrop,
                                     xfconf_channel_get_bool(channel, prop,
                                                             TRUE));

        g_snprintf(prop, sizeof(prop),
                   "/backdrop/screen%d/monitor%d/image-path", screen, i);
        v_str = xfconf_channel_get_string(channel, prop, DEFAULT_BACKDROP);
        if(is_backdrop_list(v_str)) {
            const gchar *imgfile = get_path_from_listfile(v_str);
            xfce_backdrop_set_image_filename(backdrop, imgfile);
        } else
            xfce_backdrop_set_image_filename(backdrop, v_str);
        g_free(v_str);
        
        g_snprintf(prop, sizeof(prop),
                   "/backdrop/screen%d/monitor%d/image-style", screen, i);
        xfce_backdrop_set_image_style(backdrop,
                                      xfconf_channel_get_int(channel, prop,
                                                             XFCE_BACKDROP_IMAGE_AUTO));
        
        g_snprintf(prop, sizeof(prop),
                   "/backdrop/screen%d/monitor%d/color1", screen, i);
        /* default color1 is #3b5b89 */
        color.red = (guint16)0x3b00;
        color.green = (guint16)0x5b00;
        color.blue = (guint16)0x8900;
        xfconf_channel_get_struct(channel, prop, &(color.red),
                                  XFCONF_TYPE_UINT16,
                                  XFCONF_TYPE_UINT16,
                                  XFCONF_TYPE_UINT16,
                                  G_TYPE_INVALID);
        xfce_backdrop_set_first_color(backdrop, &color);

        g_snprintf(prop, sizeof(prop),
                   "/backdrop/screen%d/monitor%d/color1", screen, i);
        /* default color2 is #3e689e */
        color.red = (guint16)0x3e00;
        color.green = (guint16)0x6800;
        color.blue = (guint16)0x9e00;
        xfconf_channel_get_struct(channel, prop, &(color.red),
                                  XFCONF_TYPE_UINT16,
                                  XFCONF_TYPE_UINT16,
                                  XFCONF_TYPE_UINT16,
                                  G_TYPE_INVALID);
        xfce_backdrop_set_second_color(backdrop, &color);
        
        g_snprintf(prop, sizeof(prop),
                   "/backdrop/screen%d/monitor%d/color-style", screen, i);
        xfce_backdrop_set_color_style(backdrop,
                                      xfconf_channel_get_int(channel, prop,
                                                             XFCE_BACKDROP_COLOR_VERT_GRADIENT));
                                      
        g_snprintf(prop, sizeof(prop),
                   "/backdrop/screen%d/monitor%d/brightness", screen, i);
        xfce_backdrop_set_brightness(backdrop,
                                     xfconf_channel_get_int(channel, prop,
                                                            0));

        g_snprintf(prop, sizeof(prop),
                   "/backdrop/screen%d/monitor%d/saturation", screen, i);
        xfce_backdrop_set_saturation(backdrop,
                                     xfconf_channel_get_double(channel, prop,
                                                               1.0));
    }
    
    xfce_desktop_thaw_updates(desktop);
    
    TRACE("exiting");
}

void
xfce_desktop_settings_changed(XfconfChannel *channel,
                              const gchar *property,
                              GValue *value,
                              gpointer user_data)
{
    XfceDesktop *desktop = user_data;
    XfceBackdrop *backdrop;
    gchar *sname, *p, *q;
    gint screen = -1, monitor = -1;
    GdkColor color;
    GdkScreen *gscreen;
    
    TRACE("dummy");
    
#ifdef ENABLE_DESKTOP_ICONS
    if(!strcmp(property, "/desktop-icons/style")) {
        xfce_desktop_set_icon_style(desktop, G_VALUE_TYPE(value)
                                             ? g_value_get_int(value)
                                             : 
#ifdef ENABLE_FILE_ICONS
                                               XFCE_DESKTOP_ICON_STYLE_FILES
#else
                                               XFCE_DESKTOP_ICON_STYLE_WINDOWS
#endif
                                    );
        return;
    } else if(!strcmp(property, "/desktop-icons/icon-size")) {
        xfce_desktop_set_icon_size(desktop, G_VALUE_TYPE(value)
                                            ? g_value_get_uint(value)
                                            : DEFAULT_ICON_SIZE);
        return;
    } else if(!strcmp(property, "/desktop-icons/use-custom-font-size")) {
        xfce_desktop_set_use_icon_font_size(desktop,
                                            G_VALUE_TYPE(value)
                                            ? g_value_get_boolean(value)
                                            : FALSE);
        return;
    } else if(!strcmp(property, "/desktop-icons/font-size")) {
        xfce_desktop_set_icon_font_size(desktop, G_VALUE_TYPE(value)
                                                 ? g_value_get_double(value)
                                                 : DEFAULT_ICON_FONT_SIZE);
        return;
    }
#endif

    /* from here on we only handle /backdrop/ properties */
    if(strncmp(property, "/backdrop/screen", 16))
        return;
    
    /* get the screen and monitor number */
    sname = g_strdup(property);

    p = strstr(sname, "/screen");
    if(p) {
        q = strstr(p+7, "/");
        if(q) {
            gchar *endptr = NULL;

            *q = 0;
            errno = 0;
            screen = strtoul(p+7, &endptr, 10);
            if((screen == ULONG_MAX && errno == ERANGE) || errno == EINVAL
               || (!endptr || *endptr))
            {
                screen = -1;
            }
            *q = '/';
        }
    }

    if(screen == -1) {
        g_free(sname);
        return;
    }

    gscreen = gtk_widget_get_screen(GTK_WIDGET(desktop));
    if(screen != gdk_screen_get_number(gscreen)) {
        g_free(sname);
        return;
    }

    /* this guy is per-screen, no per-monitor variant */
    if(strstr(property, "/xinerama-stretch")) {
        xfce_desktop_set_xinerama_stretch(desktop, G_VALUE_TYPE(value)
                                                   ? g_value_get_boolean(value)
                                                   : FALSE);
        g_free(sname);
        return;
    }

    p = strstr(sname, "/monitor");
    if(p) {
        q = strstr(p+8, "/");
        if(q) {
            gchar *endptr = NULL;

            *q = 0;
            errno = 0;
            monitor = strtoul(p+8, &endptr, 10);
            if((monitor == ULONG_MAX && errno == ERANGE) || errno == EINVAL
               || (!endptr || *endptr))
            {
                monitor = -1;
            }
            *q = '/';
        }
    }

    g_free(sname);

    if(monitor == -1 || monitor >= gdk_screen_get_n_monitors(gscreen))
        return;
    
    backdrop = xfce_desktop_peek_backdrop(desktop, monitor);
    if(!backdrop)
        return;
    
    if(strstr(property, "/image-show")) {
        xfce_backdrop_set_show_image(backdrop, G_VALUE_TYPE(value)
                                               ? g_value_get_boolean(value)
                                               : TRUE);
    } else if(strstr(property, "/image-path")) {
        const gchar *v_str = G_VALUE_TYPE(value) ? g_value_get_string(value)
                                                 : DEFAULT_BACKDROP;
        if(is_backdrop_list(v_str)) {
            const gchar *imgfile = get_path_from_listfile(v_str);
            xfce_backdrop_set_image_filename(backdrop, imgfile);
        } else
            xfce_backdrop_set_image_filename(backdrop, v_str);
    } else if(strstr(property, "/image-style")) {
        xfce_backdrop_set_image_style(backdrop, G_VALUE_TYPE(value)
                                                ? g_value_get_int(value)
                                                : XFCE_BACKDROP_IMAGE_AUTO);
    } else if(strstr(property, "/color1")) {
        /* FIXME: use |value| */
        /* default color1 is #3b5b89 */
        color.red = (guint16)0x3b00;
        color.green = (guint16)0x5b00;
        color.blue = (guint16)0x8900;
        if(G_VALUE_TYPE(value)) {
            dbus_g_type_struct_get(value,
                                   0, &color.red,
                                   1, &color.green,
                                   2, &color.blue,
                                   G_MAXUINT);
        }
        xfce_backdrop_set_first_color(backdrop, &color);
    } else if(strstr(property, "/color2")) {
        /* FIXME: use |value| */
        /* default color2 is #3e689e */
        color.red = (guint16)0x3e00;
        color.green = (guint16)0x6800;
        color.blue = (guint16)0x9e00;
        if(G_VALUE_TYPE(value)) {
            dbus_g_type_struct_get(value,
                                   0, &color.red,
                                   1, &color.green,
                                   2, &color.blue,
                                   G_MAXUINT);
        }
        xfce_backdrop_set_second_color(backdrop, &color);
    } else if(strstr(property, "/color-style")) {
        xfce_backdrop_set_color_style(backdrop, G_VALUE_TYPE(value)
                                                ? g_value_get_int(value)
                                                : XFCE_BACKDROP_COLOR_VERT_GRADIENT);
    } else if(strstr(property, "/brightness")) {
        xfce_backdrop_set_brightness(backdrop,
                                     G_VALUE_TYPE(value)
                                     ? g_value_get_int(value) : 0);
    } else if(strstr(property, "/saturation")) {
        xfce_backdrop_set_saturation(backdrop,
                                     G_VALUE_TYPE(value)
                                     ? g_value_get_double(value) : 1.0);
    }
}
