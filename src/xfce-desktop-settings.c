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

inline gint
count_elements(const gchar **list)
{
    gint i, c = 0;
    
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
        
        g_print("picked i=%d, %s\n", i, files[i]);
        /* validate the image; if it's good, return it */
        if(xfdesktop_check_image_file(files[i]))
            break;
        
        g_print("file not valid, ditching\n");
        
        /* bad image: remove it from the list and write it out */
        save_list_file_minus_one(listfile, files, i);
        previndex = -1;
        /* loop and try again */
    } while(1);
    
    return (const gchar *)files[(previndex = i)];
}

void
xfce_desktop_settings_load_initial(XfceDesktop *desktop,
                                   McsClient *mcs_client)
{
    gchar setting_name[64];
    McsSetting *setting = NULL;
    GdkScreen *gscreen;
    gint screen, i, nmonitors;
    XfceBackdrop *backdrop;
    GdkColor color;
    
    gtk_widget_realize(GTK_WIDGET(desktop));
    gscreen = gtk_widget_get_screen(GTK_WIDGET(desktop));
    screen = gdk_screen_get_number(gscreen);
    nmonitors = xfce_desktop_get_n_monitors(desktop);
    
    if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, "xineramastretch",
            BACKDROP_CHANNEL, &setting))
    {
        xfce_desktop_set_xinerama_stretch(desktop, setting->data.v_int);
        mcs_setting_free(setting);
        setting = NULL;
    }
    
#ifdef ENABLE_DESKTOP_ICONS
    if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, "desktopiconstyle",
                                                 BACKDROP_CHANNEL, &setting))
    {
        xfce_desktop_set_icon_style(desktop, setting->data.v_int);
        mcs_setting_free(setting);
        setting = NULL;
    } else
        xfce_desktop_set_icon_style(desktop, XFCE_DESKTOP_ICON_STYLE_WINDOWS);
    
    if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, "icons_font_size",
                                             BACKDROP_CHANNEL, &setting))
    {
        xfce_desktop_set_icon_font_size(desktop, setting->data.v_int);
        mcs_setting_free(setting);
        setting = NULL;
    }
    
    if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, 
                                             "icons_use_system_font_size",
                                             BACKDROP_CHANNEL, &setting))
    {
        xfce_desktop_set_icon_use_system_font_size(desktop, setting->data.v_int);
        mcs_setting_free(setting);
        setting = NULL;
    }
    
    if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, "icons_icon_size",
                                             BACKDROP_CHANNEL, &setting))
    {
        xfce_desktop_set_icon_size(desktop, setting->data.v_int);
        mcs_setting_free(setting);
        setting = NULL;
    }
#endif
    
    for(i = 0; i < nmonitors; i++) {
        backdrop = xfce_desktop_peek_backdrop(desktop, i); 
        
        g_snprintf(setting_name, 64, "showimage_%d_%d", screen, i);
        if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, setting_name,
                BACKDROP_CHANNEL, &setting))
        {
            xfce_backdrop_set_show_image(backdrop, setting->data.v_int);
            mcs_setting_free(setting);
            setting = NULL;
        }
        
        g_snprintf(setting_name, 64, "imagepath_%d_%d", screen, i);
        if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, setting_name,
                BACKDROP_CHANNEL, &setting))
        {
            if(is_backdrop_list(setting->data.v_string)) {
                const gchar *imgfile = get_path_from_listfile(setting->data.v_string);
                xfce_backdrop_set_image_filename(backdrop, imgfile);
            } else {
                xfce_backdrop_set_image_filename(backdrop,
                                                 setting->data.v_string);
            }
            mcs_setting_free(setting);
            setting = NULL;
        } else
            xfce_backdrop_set_image_filename(backdrop, DEFAULT_BACKDROP);
        
        g_snprintf(setting_name, 64, "imagestyle_%d_%d", screen, i);
        if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, setting_name,
                BACKDROP_CHANNEL, &setting))
        {
            xfce_backdrop_set_image_style(backdrop, setting->data.v_int);
            mcs_setting_free(setting);
            setting = NULL;
        } else
            xfce_backdrop_set_image_style(backdrop, XFCE_BACKDROP_IMAGE_STRETCHED);
        
        g_snprintf(setting_name, 64, "color1_%d_%d", screen, i);
        if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, setting_name,
                BACKDROP_CHANNEL, &setting))
        {
            color.red = setting->data.v_color.red;
            color.green = setting->data.v_color.green;
            color.blue = setting->data.v_color.blue;
            xfce_backdrop_set_first_color(backdrop, &color);
            mcs_setting_free(setting);
            setting = NULL;
        } else {
            /* default color1 is #6985b7 */
            color.red = (guint16)0x6900;
            color.green = (guint16)0x8500;
            color.blue = (guint16)0xb700;
            xfce_backdrop_set_first_color(backdrop, &color);
        }
        
        g_snprintf(setting_name, 64, "color2_%d_%d", screen, i);
        if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, setting_name,
                BACKDROP_CHANNEL, &setting))
        {
            color.red = setting->data.v_color.red;
            color.green = setting->data.v_color.green;
            color.blue = setting->data.v_color.blue;
            xfce_backdrop_set_second_color(backdrop, &color);
            mcs_setting_free(setting);
            setting = NULL;
        } else {
            /* default color2 is #dbe8ff */
            color.red = (guint16)0xdb00;
            color.green = (guint16)0xe800;
            color.blue = (guint16)0xff00;
            xfce_backdrop_set_second_color(backdrop, &color);
        }
        
        g_snprintf(setting_name, 64, "colorstyle_%d_%d", screen, i);
        if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, setting_name,
                BACKDROP_CHANNEL, &setting))
        {
            xfce_backdrop_set_color_style(backdrop, setting->data.v_int);
            mcs_setting_free(setting);
            setting = NULL;
        } else
            xfce_backdrop_set_color_style(backdrop, XFCE_BACKDROP_COLOR_HORIZ_GRADIENT);
        
        g_snprintf(setting_name, 64, "brightness_%d_%d", screen, i);
        if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, setting_name,
                BACKDROP_CHANNEL, &setting))
        {
            xfce_backdrop_set_brightness(backdrop, setting->data.v_int);
            mcs_setting_free(setting);
            setting = NULL;
        } else
            xfce_backdrop_set_brightness(backdrop, 0);
    }
}

gboolean
xfce_desktop_settings_changed(McsClient *client,
                              McsAction action,
                              McsSetting *setting,
                              gpointer user_data)
{
    XfceDesktop *desktop = user_data;
    XfceBackdrop *backdrop;
    gchar *sname, *p, *q;
    gint screen, monitor;
    GdkColor color;
    gboolean handled = FALSE;
    GdkScreen *gscreen;
    
    TRACE("dummy");
    
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), FALSE);
    
    if(!strcmp(setting->name, "xineramastretch")) {
        xfce_desktop_set_xinerama_stretch(desktop, setting->data.v_int); 
        return TRUE;
    }
    
#ifdef ENABLE_DESKTOP_ICONS
    if(!strcmp(setting->name, "desktopiconstyle")) {
        xfce_desktop_set_icon_style(desktop, setting->data.v_int);
        return TRUE;
    }
    
    if(!strcmp(setting->name, "icons_icon_size")) {
        xfce_desktop_set_icon_size(desktop, setting->data.v_int);
        return TRUE;
    }
    
    if(!strcmp(setting->name, "icons_use_system_font_size")) {
        xfce_desktop_set_icon_use_system_font_size(desktop, setting->data.v_int);
        return TRUE;
    }
    
    if(!strcmp(setting->name, "icons_font_size")) {
        xfce_desktop_set_icon_font_size(desktop, setting->data.v_int);
        return TRUE;
    }
#endif
    
    /* get the screen and monitor number */
    sname = g_strdup(setting->name);
    q = g_strrstr(sname, "_");
    if(!q || q == sname) {
        g_free(sname);
        return FALSE;
    }
    p = strstr(sname, "_");
    if(!p || p == q) {
        g_free(sname);
        return FALSE;
    }
    *q = 0;
    screen = atoi(p+1);
    monitor = atoi(q+1);
    g_free(sname);
    
    gscreen = gtk_widget_get_screen(GTK_WIDGET(desktop));
    if(screen == -1 || monitor == -1
            || screen != gdk_screen_get_number(gscreen)
            || monitor >= gdk_screen_get_n_monitors(gscreen))
    {
        /* not ours */
        return FALSE;
    }
    
    backdrop = xfce_desktop_peek_backdrop(desktop, monitor);
    if(!backdrop)
        return FALSE;
    
    switch(action) {
        case MCS_ACTION_NEW:
        case MCS_ACTION_CHANGED:
            if(strstr(setting->name, "showimage") == setting->name) {
                xfce_backdrop_set_show_image(backdrop, setting->data.v_int);
                handled = TRUE;
            } else if(strstr(setting->name, "imagepath") == setting->name) {
                if(is_backdrop_list(setting->data.v_string)) {
                    const gchar *imgfile = get_path_from_listfile(setting->data.v_string);
                    xfce_backdrop_set_image_filename(backdrop, imgfile);
                } else {
                    xfce_backdrop_set_image_filename(backdrop,
                            setting->data.v_string);
                }
                handled = TRUE;
            } else if(strstr(setting->name, "imagestyle") == setting->name) {
                xfce_backdrop_set_image_style(backdrop, setting->data.v_int);
                handled = TRUE;
            } else if(strstr(setting->name, "color1") == setting->name) {
                color.red = setting->data.v_color.red;
                color.blue = setting->data.v_color.blue;
                color.green = setting->data.v_color.green;
                xfce_backdrop_set_first_color(backdrop, &color);
                handled = TRUE;
            } else if(strstr(setting->name, "color2") == setting->name) {
                color.red = setting->data.v_color.red;
                color.blue = setting->data.v_color.blue;
                color.green = setting->data.v_color.green;
                xfce_backdrop_set_second_color(backdrop, &color);
                handled = TRUE;
            } else if(strstr(setting->name, "colorstyle") == setting->name) {
                xfce_backdrop_set_color_style(backdrop, setting->data.v_int);
                handled = TRUE;
            } else if(strstr(setting->name, "brightness") == setting->name) {
                xfce_backdrop_set_brightness(backdrop, setting->data.v_int);
                handled = TRUE;
            }
            
            break;
        
        case MCS_ACTION_DELETED:
            break;
    }
    
    return handled;
}
