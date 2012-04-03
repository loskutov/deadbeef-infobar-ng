/*
    Infobar plugin for DeaDBeeF music player
    Copyright (C) 2012 Dmitriy Simbiriatin <dmitriy.simbiriatin@gmail.com>

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

#include <gtk/gtk.h>
#include <deadbeef/deadbeef.h>
#include <deadbeef/gtkui_api.h>

#include "infobar.h"
#include "support.h"

void init_ui_plugin(ddb_gtkui_t *ui_plugin);

void free_ui_plugin(void);

void free_bio_pixbuf(void);

void create_infobar_interface(void);

void attach_infobar_menu_entry(void);

gboolean infobar_config_changed(void);

gboolean update_bio_view(gpointer data);

gboolean update_lyrics_view(gpointer data);

#endif
