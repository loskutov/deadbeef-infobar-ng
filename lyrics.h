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

#ifndef INFOBAR_LYR_HEADER
#define INFOBAR_LYR_HEADER

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <deadbeef/deadbeef.h>

#include "infobar.h"
#include "utils.h"

/* URL templates to retrieve lyrics from different sources. */
#define LW_URL_TEMP "http://lyrics.wikia.com/api.php?action=query&prop=revisions&rvprop=content&format=xml&titles=%s:%s"
#define LM_URL_TEMP "http://www.lyricsmania.com/%s_lyrics_%s.html"
#define LT_URL_TEMP "http://www.lyricstime.com/%s-%s-lyrics.html"
#define ML_URL_TEMP "http://megalyrics.ru/lyric/%s/%s.htm"
#define SR_CMD_TEMP "\"%s\" \"%s\" \"%s\" \"%s\" 2>&-"

/* XPath expressions to parse lyrics from different sources. */
#define LM_EXP "//*[@id=\"songlyrics_h\"]"
#define LT_EXP "//*[@id=\"songlyrics\"]"
#define ML_EXP "//pre[@class=\"lyric\"]"
#define LW_HTML_EXP "//lyrics"
#define LW_XML_EXP "//rev"

/* Lyrics parts. */
#define ML_LYR_BEG "<pre class=\"lyric\"><h2>Текст песни</h2>"
#define ML_LYR_END "</pre>"

/* Fetches lyrics from "http://lyrics.wikia.com". */
int fetch_lyrics_from_lyricswikia(const char *artist, const char *title, char **txt);

/* Fetches lyrics from "http://megalyrics.ru".  */
int fetch_lyrics_from_megalyrics(const char *artist, const char *title, char **txt);

/* Fetches lyrics from "http://lyricstime.com". */
int fetch_lyrics_from_lyricstime(const char *artist, const char *title, char **txt);

/* Fetches lyrics from "http://lyricsmania.com". */
int fetch_lyrics_from_lyricsmania(const char *artist, const char *title, char **txt);

/* Fetches lyrics, using external bash script. */
int fetch_lyrics_from_script(const char *artist, const char *title, const char *album, char **txt);

#endif
