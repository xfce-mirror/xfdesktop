/*  xfce4
 *  
 *  Copyright (C) 2002 Jasper Huijsmans (huysmans@users.sourceforge.net)
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

#ifndef __XFDESKTOP_SETTING
#define __XFDESKTOP_SETTING

/* init_settings is a flag that dramatically speeds up startup 
   by avoiding the computation of the backdrop image every time 
   a settings is added at startup
 */
extern gboolean init_settings;

void run_settings_dialog(void);

void watch_settings(GtkWidget *window, NetkScreen *screen);

void load_settings(void);

void stop_watch(void);

#endif /* __XFDESKTOP_SETTING */

