/*
    Infobar plugin for DeaDBeeF music player
    Copyright (C) 2011 Dmitriy Simbiriatin <slpiv@mail.ru>

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


#ifndef INFOBAR_H
#define INFOBAR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <gtk/gtk.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/parser.h>
#include <libxml/HTMLparser.h>
#include <deadbeef/deadbeef.h>
#include <deadbeef/gtkui_api.h>

#include "support.h"

#define trace(fmt,...)

typedef enum {
	HTML = 1,
	XML = 2,
} ContentType;

typedef enum {
	LYRICS = 1,
	BIO = 2,
} CacheType;

static DB_misc_t plugin;
static DB_functions_t *deadbeef;
static ddb_gtkui_t *gtkui_plugin;

#define TXT_MAX 30000

#define CONF_LYRICS_ENABLED "infobar.lyrics.enabled"
#define CONF_LYRICSMANIA_ENABLED "infobar.lyrics.lyricsmania"
#define CONF_LYRICSTIME_ENABLED "infobar.lyrics.lyricstime"
#define CONF_BIO_ENABLED "infobar.bio.enabled"
#define CONF_BIO_LOCALE "infobar.bio.locale"
#define CONF_INFOBAR_WIDTH "infobar.width"
#define CONF_UPDATE_PERIOD "infobar.cache.period"
#define CONF_INFOBAR_VISIBLE "infobar.visible"
#define CONF_GTKUI_TABS_VISIBLE "gtkui.tabs.visible"

DB_FILE *infobar_cnt;

static char artist[100];
static char title[100];
static char eartist[300];
static char etitle[300];
static char eartist_lfm[300];
static char old_locale[5];

static uintptr_t infobar_mutex;
static uintptr_t infobar_cond;
static intptr_t infobar_tid;
static gboolean infobar_stopped;

static GtkWidget *infobar = NULL;
static GtkWidget *infobar_tabs = NULL;
static GtkWidget *lyrics_toggle = NULL;
static GtkWidget *bio_toggle = NULL;
static GtkWidget *lyrics_tab = NULL;
static GtkWidget *bio_tab = NULL;
static GtkWidget *bio_image = NULL;

static GtkTextBuffer *lyrics_buffer = NULL;
static GtkTextBuffer *bio_buffer = NULL;

static int create_infobar(void);
static int create_infobar_interface(void);
static int create_infobar_menu_entry(void);

static void update_lyrics_view(const char *lyrics, int size);
static void update_bio_view(const char *bio, int size, const char *img_file);
static void set_tab_visible(GtkWidget *toggle, GtkWidget *item, gboolean visible);

static int infobar_start(void);
static int infobar_stop(void);
static int infobar_connect(void);
static int infobar_disconnect(void);
static void infobar_thread(void*);
static gboolean infobar_init(void);
static int infobar_message(uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2);

static void infobar_tab_changed(GtkToggleButton *toggle, GtkWidget *widget);
static void infobar_menu_toggle(GtkMenuItem *item, gpointer data);
static void infobar_songstarted(ddb_event_track_t *ev);
static void infobar_config_changed(void);

static char *retrieve_txt_content(const char *url, int size);
static int retrieve_img_content(const char *url, const char *img);

static char	*load_content(const char *cache_file);
static char *parse_content(const char *cnt, int size, const char *pattern, ContentType type);
static int save_content(const char *cache_file, const char *buffer, int size);
static void parser_errors_handler(void *ctx, const char *msg, ...);

static int retrieve_track_lyrics(void);
static int retrieve_artist_bio(void);

static char *fetch_lyrics_from(const char *url, const char *artist, const char *title, const char *cache_file, const char *pattern);

static int get_cache_path(char *cache_path, int size, ContentType type);
static int uri_encode(char *out, int outl, const char *str, char space);
static char *convert_to_utf8(const char *str, int size);

static gboolean is_old_cache(const char *cache_file);
static gboolean is_exists(const char *obj);

DB_plugin_t *infobar_load (DB_functions_t *ddb);

#endif
