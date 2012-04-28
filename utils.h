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

#ifndef UTILS_HEADER
#define UTILS_HEADER

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <sys/stat.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/parser.h>
#include <libxml/HTMLparser.h>
#include <deadbeef/deadbeef.h>

#include "infobar.h"

/* Defines maximum number of characters that can be retrieved. */
#define MAX_TXT_SIZE 100000

/* Custom types. */
typedef enum {
    HTML = 1,
    XML = 2,
} ContentType;

typedef enum {
    LYRICS = 1,
    BIO = 2,
} CacheType;

typedef struct {
    float width;
    float height;
} Res;

/* Checks if specified file or directory is exists. */
gboolean is_exists(const char *obj);

/* Checks if the specified text contains redirect information. */
gboolean is_redirect(const char *str);

/* Checks if the specified track item is a local track or a stream. */
gboolean is_stream(DB_playItem_t *track);

/* Checks if the current track item is differs from specified one. */
gboolean is_track_changed(DB_playItem_t *track);

/* Checks if the specified cache file is old. */
gboolean is_old_cache(const char *cache_file, CacheType type);

/* Retrieves text data from the specified URL. */
int retrieve_txt_content(const char *url, char **content);

/* Retrieves image file from the specified URL and saves it locally. */
int retrieve_img_content(const char *url, const char *img);

/* Executes external script and reads its output. */
int execute_script(const char *cmd, char **out);

/* Loads content of the specified text file. */
int load_txt_file(const char *file, char **content);

/* Saves specified content to the text file. */
int save_txt_file(const char *file, const char *content);

/* Converts specified string encoding to UTF-8. */
int convert_to_utf8(const char *str, char **str_utf8);

/* Deletes new lines at the beginning of specified text data. */
int del_nl(const char *txt, char **txt_wo_nl);

/* Concatenates two lyrics texts into one, using simple separator 
 * to visually divide them. */
int concat_lyrics(const char *fst_lyr, const char *snd_lyr, char **lyr);

/* Parses redirect information and retrieves correct artist name 
 * and song title. */
int get_redirect_info(const char *str, char **artist, char **title);

/* Retrieves infomation about current artist. */
int get_artist_info(DB_playItem_t *track, char **artist);

/* Retrieves infomation about current artist and title */
int get_artist_and_title_info(DB_playItem_t *track, char **artist, char **title);

/* Retrieves information about current artist, title and album. */
int get_full_track_info(DB_playItem_t *track, char **artist, char **title, char **album);

/* Deletes biography cache for specified artist. */
int del_bio_cache(const char *artist);

/* Deletes lyrics cache for specified track. */
int del_lyr_cache(const char *artist, const char *title);

/* Creates lyrics cache file for the specified track. */
int create_lyr_cache(const char *artist, const char *title, char **txt_cache);

/* Creates biography cache files for the specified artist. */
int create_bio_cache(const char *artist, char **txt_cache, char **img_cache);

/* Encodes artist name. */
int encode_artist(const char *artist, char **eartist, const char space);

/* Encodes artist name and song title. */
int encode_artist_and_title(const char *artist, const char *title, char **eartist, char **etitle);

/* Encodes artist name, song title and album name. */
int encode_full(const char *artist, const char *title, const char *album, char **eartist, char **etitle, char **ealbum);

/* Parses content in HTML or XML format using XPath expression. */
int parse_content(const char *content, const char *pattern, char **parsed, ContentType type, int num);

/* Calculates new resolution to respectively resize image. */
void find_new_resolution(float ww, float wh, float aw, float ah, Res *res);

#endif
