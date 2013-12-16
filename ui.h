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
#include "types.h"
#include "utils.h"

#define BIO_IMAGE_HEIGHT 220

/* Infobar widget. */
GtkWidget *infobar;

/* Creates infobar with all available tabs. */
void create_infobar(void);

/* Updates "Similar" tab with the new list of similar artists. */
void update_similar_view(SimilarInfo *similar, size_t size);

/* Updates "Biography" tab with the new artist's image and biography text. */
void update_bio_view(const char *bio_txt, const char *img_file);

/* Updates "Lyrics" tab with the new lyrics. */
void update_lyrics_view(const char *lyr_txt, DB_playItem_t *track);

/* This function should be invoked, when some changes to the plug-in's
 * configuration were made. It updates infobar view according to the
 * new changes. */
void infobar_config_changed(void);

/* Callback function to initialize widget called during plug-in connect. */
void infobar_init(struct ddb_gtkui_widget_s *widget);

/* Callback function to destroy widget called during plug-in disconnect. */
void infobar_destroy(struct ddb_gtkui_widget_s *widget);

#endif
