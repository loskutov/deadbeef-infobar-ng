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

#ifndef INFOBAR_HEADER
#define INFOBAR_HEADER

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <deadbeef/deadbeef.h>

#include "ui.h"
#include "utils.h"
#include "lyrics.h"
#include "similar.h"
#include "biography.h"

//#define trace(...) { fprintf(stderr, __VA_ARGS__); }
#define trace(fmt, ...)

/* Predefined names of infobar properties. */
#define CONF_LYRICS_ENABLED "infobar.lyrics.enabled"
#define CONF_LYRICWIKI_ENABLED "infobar.lyrics.lyricwiki"
#define CONF_LYRICSMANIA_ENABLED "infobar.lyrics.lyricsmania"
#define CONF_LYRICSTIME_ENABLED "infobar.lyrics.lyricstime"
#define CONF_MEGALYRICS_ENABLED "infobar.lyrics.megalyrics"
#define CONF_LYRICS_SCRIPT_ENABLED "infobar.lyrics.script"
#define CONF_LYRICS_ALIGNMENT "infobar.lyrics.alignment"
#define CONF_BIO_ENABLED "infobar.bio.enabled"
#define CONF_BIO_LOCALE "infobar.bio.locale"
#define CONF_LYRICS_SCRIPT_PATH "infobar.lyrics.script.path"
#define CONF_LYRICS_UPDATE_PERIOD "infobar.lyrics.cache.period"
#define CONF_BIO_UPDATE_PERIOD "infobar.bio.cache.period"
#define CONF_SIM_ENABLED "infobar.similar.enabled"
#define CONF_SIM_MAX_ARTISTS "infobar.similar.max.artists"

#define WIDGET_ID "infobar"
#define WIDGET_LABEL "Infobar"

/* deadbeef API. */
extern DB_functions_t *deadbeef;

/* UI plugin. */
extern ddb_gtkui_t *gtkui_plugin;

#endif
