/*
    Infobar plugin for DeaDBeeF music player
    Copyright (C) 2011-2012 Dmitriy Simbiriatin <dmitriy.simbiriatin@gmail.com>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef INFOBAR_UI_HEADER
#define INFOBAR_UI_HEADER

#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <deadbeef/deadbeef.h>
#include <deadbeef/gtkui_api.h>

#include "infobar.h"
#include "utils.h"

/* Initializes reference to gtkui plug-in and creates infobar interface. 
 * Should be called on plug-in startup. */
int init_ui_plugin(void);

/* Disposes reference to gtkui plug-in and saves ui settings. 
 * Should be called on plug-in shutdown. */
void free_ui_plugin(void);

/* Updates "Biography" tab with the new artist's image and biography text. */
void update_bio_view(const char *bio_txt, const char *img_file);

/* Updates "Lyrics" tab with the new lyrics. */
void update_lyrics_view(const char *lyr_txt, DB_playItem_t *track);

/* This function should be invoked, when some changes to the plug-in's 
 * configuration were made. It updates infobar view according to the 
 * new changes. */
void infobar_config_changed(void);

#endif
