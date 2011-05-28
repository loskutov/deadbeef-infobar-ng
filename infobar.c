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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
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

//#define trace(...) { fprintf(stderr, __VA_ARGS__); }
#define trace(fmt,...)

typedef enum {
	HTML = 1,
	XML = 2,
} ContentType;

typedef enum {
	LYRICS = 1,
	BIO = 2,
} CacheType;

typedef enum {
	LYRICSWIKIA = 1,
	LYRICSMANIA = 2,
	LYRICSTIME = 3,
	MEGALYRICS = 4,
	LASTFM = 5,
} SourceType;

typedef struct {
	char *txt;
	int size;
} LyricsViewData;

typedef struct {
	char *txt;
	char *img;
	int size;
} BioViewData;

static DB_misc_t plugin;
static DB_functions_t *deadbeef;
static ddb_gtkui_t *gtkui_plugin;

#define TXT_MAX 100000

#define CONF_LYRICS_ENABLED "infobar.lyrics.enabled"
#define CONF_LYRICSWIKIA_ENABLED "infobar.lyrics.lyricswikia"
#define CONF_LYRICSMANIA_ENABLED "infobar.lyrics.lyricsmania"
#define CONF_LYRICSTIME_ENABLED "infobar.lyrics.lyricstime"
#define CONF_MEGALYRICS_ENABLED "infobar.lyrics.megalyrics"
#define CONF_LYRICS_ALIGNMENT "infobar.lyrics.alignment"
#define CONF_BIO_ENABLED "infobar.bio.enabled"
#define CONF_BIO_LOCALE "infobar.bio.locale"
#define CONF_INFOBAR_WIDTH "infobar.width"
#define CONF_LYRICS_UPDATE_PERIOD "infobar.lyrics.cache.period"
#define CONF_BIO_UPDATE_PERIOD "infobar.bio.cache.period"
#define CONF_INFOBAR_VISIBLE "infobar.visible"

DB_FILE *infobar_cnt;

static char artist[100];
static char title[100];

static char old_artist[100];
static char old_title[100];

gboolean artist_changed = TRUE;

static uintptr_t infobar_mutex;
static uintptr_t infobar_cond;
static intptr_t infobar_tid;
static gboolean infobar_stopped;

static GtkWidget *infobar;
static GtkWidget *infobar_tabs;
static GtkWidget *lyrics_toggle;
static GtkWidget *bio_toggle;
static GtkWidget *lyrics_tab;
static GtkWidget *bio_tab;
static GtkWidget *bio_image;

GtkTextBuffer *lyrics_buffer;
GtkTextBuffer *bio_buffer;

static int
get_cache_path(char *cache_path, int size, ContentType type) {
	int res = -1;

	const char *home_cache = getenv("XDG_CACHE_HOME");

	switch(type) {
	case LYRICS:
		res = snprintf(cache_path, size, home_cache ? "%s/deadbeef/lyrics" : "%s/.cache/deadbeef/lyrics",
				home_cache ? home_cache : getenv("HOME"));
		break;
	case BIO:
		res = snprintf(cache_path, size, home_cache ? "%s/deadbeef/bio" : "%s/.cache/deadbeef/bio",
				home_cache ? home_cache : getenv("HOME"));
		break;
	}
	return res;
}

static gboolean
update_bio_view(gpointer data) {
	trace("infobar: update bio view started\n");
	
    GtkTextIter begin, end;
	BioViewData *bio_data = (BioViewData*) data;
	
	gdk_threads_enter();

    if(bio_image && bio_data->img)
    	gtk_image_set_from_file(GTK_IMAGE(bio_image), bio_data->img);

    if(bio_buffer) {
		trace("infobar: inserting bio data to the buffer\n");
    	gtk_text_buffer_get_iter_at_line (bio_buffer, &begin, 0);
    	gtk_text_buffer_get_end_iter (bio_buffer, &end);
    	gtk_text_buffer_delete (bio_buffer, &begin, &end);

    	if(bio_data->txt && bio_data->size > 0) {
    		gtk_text_buffer_insert(bio_buffer, &begin, 
				bio_data->txt, bio_data->size);
    	} else {
    		gtk_text_buffer_insert(bio_buffer, &begin,
				"Biography not found.", -1);
    	}
    }
    
    gdk_threads_leave();
    
    if(bio_data->txt) free(bio_data->txt);
    if(bio_data->img) free(bio_data->img);
    if(bio_data) free(bio_data);
    return FALSE;
}

static gboolean
update_lyrics_view(gpointer data) {
	trace("infobar: update lyrics view started\n");
	
    GtkTextIter begin, end;
	LyricsViewData *lyr_data = (LyricsViewData*) data;
	
	gdk_threads_enter();

    if(lyrics_buffer) {
		trace("infobar: inserting lyrics data to the buffer\n");
    	gtk_text_buffer_get_iter_at_line (lyrics_buffer, &begin, 0);
    	gtk_text_buffer_get_end_iter (lyrics_buffer, &end);
    	gtk_text_buffer_delete (lyrics_buffer, &begin, &end);
		
		gtk_text_buffer_insert_with_tags_by_name(GTK_TEXT_BUFFER(lyrics_buffer), 
				&begin, title, -1, "bold", "large", NULL);
    	gtk_text_buffer_insert(lyrics_buffer, &begin, "\n", -1);
    	gtk_text_buffer_insert_with_tags_by_name(GTK_TEXT_BUFFER(lyrics_buffer),
				&begin, artist, -1, "italic", NULL);
    	gtk_text_buffer_insert(lyrics_buffer, &begin, "\n\n", -1);

    	if(lyr_data->txt && lyr_data->size > 0) {
    		gtk_text_buffer_insert(lyrics_buffer, &begin, 
				lyr_data->txt, lyr_data->size);
    	} else {
    		gtk_text_buffer_insert(lyrics_buffer, &begin, 
				"Lyrics not found.", -1);
    	}
    }
    
    gdk_threads_leave();
    
    if(lyr_data->txt) free(lyr_data->txt);
    if(lyr_data) free(lyr_data);
    return FALSE;
}

static void
delete_cache_clicked(void) {
	int res = -1;
	
	GtkWidget *main_wnd = gtkui_plugin->get_mainwin();
	GtkWidget *dlt_dlg = gtk_message_dialog_new(GTK_WINDOW(main_wnd), GTK_DIALOG_MODAL, 
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "Cache files for the current track wiil be removed. Continue?");
	
	gint choise = gtk_dialog_run(GTK_DIALOG(dlt_dlg));
	switch(choise) {
	case GTK_RESPONSE_YES:
	{
		char lyrics_path[512] = {0};
		res = get_cache_path(lyrics_path, sizeof(lyrics_path), LYRICS);
		if(res > 0) {
			char lyrics_file[512] = {0};
			res = snprintf(lyrics_file, sizeof(lyrics_file), "%s/%s-%s", lyrics_path, artist, title);
			if(res > 0) {
				res = remove(lyrics_file);
				if(res != 0) {
					trace("infobar: failed to remove lyrics cache file\n");
				}
			} 
		}
		
		char bio_path[512] = {0};
		res = get_cache_path(bio_path, sizeof(bio_path), BIO);
		if(res > 0) {
			char bio_file[512] = {0};
			res = snprintf(bio_file, sizeof(bio_file), "%s/%s", bio_path, artist);
			if(res > 0) {
				res = remove(bio_file);
				if(res != 0) {
					trace("infobar: failed to remove bio cache file\n");
				}
			}
			
			char bio_img[512] = {0};
			res = snprintf(bio_img, sizeof(bio_img), "%s/%s_img", bio_path, artist);
			if(res > 0) {
				res = remove(bio_img);
				if(res != 0) {
					trace("infobar: failed to remove bio image file\n");
				}
			}
		}
		memset(old_artist, 0, sizeof(old_artist));
		memset(old_title, 0, sizeof(old_title));
	}
		break;
	case GTK_RESPONSE_NO:
		break;
	}
	gtk_widget_destroy(dlt_dlg);
}

static void
set_tab_visible(GtkWidget *toggle, GtkWidget *item, gboolean visible) {
	if(visible) {
		gtk_widget_show(toggle);
		gtk_widget_show(item);
	} else {
		gtk_widget_hide(toggle);
		gtk_widget_hide(item);
	}
}

static gboolean
infobar_config_changed(void) {
	gboolean state = FALSE;

	gdk_threads_enter();

	state = deadbeef->conf_get_int(CONF_LYRICS_ENABLED, 1);
	if(lyrics_toggle && lyrics_tab)
		set_tab_visible(lyrics_toggle, lyrics_tab, state);

	state = deadbeef->conf_get_int(CONF_BIO_ENABLED, 1);
	if(bio_toggle && bio_tab)
		set_tab_visible(bio_toggle, bio_tab, state);
		
	gdk_threads_leave();
		
	return FALSE;
}

static void
infobar_menu_toggle(GtkMenuItem *item, gpointer data) {
    gboolean state = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item));
	state ? gtk_widget_show(infobar) : gtk_widget_hide(infobar); 
    deadbeef->conf_set_int(CONF_INFOBAR_VISIBLE, (gint) state);
}

static void
infobar_tab_changed(GtkToggleButton *toggle, GtkWidget *widget) {
	gint index = gtk_notebook_page_num(GTK_NOTEBOOK(infobar_tabs), widget);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(infobar_tabs), index);
}

static int
create_infobar(void) {
	trace("infobar: creating infobar\n");

	if(infobar)
		return 0;

	infobar = gtk_vbox_new(FALSE, 0);

	infobar_tabs = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(infobar_tabs), FALSE);

	GtkWidget *infobar_toggles = gtk_hbox_new(FALSE, 0);

	lyrics_toggle = gtk_radio_button_new_with_label(NULL, "Lyrics");
	gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(lyrics_toggle), FALSE);

	bio_toggle = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(lyrics_toggle), "Biography");
	gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(bio_toggle), FALSE);

	lyrics_tab = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(lyrics_tab),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	GtkWidget *bio_scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(bio_scroll),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	GtkWidget *lyrics_view = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(lyrics_view), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(lyrics_view), GTK_WRAP_WORD);
	
	int just_type = 0;
	int align = deadbeef->conf_get_int(CONF_LYRICS_ALIGNMENT, 2);
	
	switch(align) {
	case 1: just_type = GTK_JUSTIFY_LEFT; 
		break;
	case 2: just_type = GTK_JUSTIFY_CENTER; 
		break;
	case 3: just_type = GTK_JUSTIFY_RIGHT; 
		break;
	default:
		just_type = GTK_JUSTIFY_CENTER;
	}
	gtk_text_view_set_justification(GTK_TEXT_VIEW(lyrics_view), just_type);

	GtkWidget *bio_view = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(bio_view), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(bio_view), GTK_WRAP_WORD);
	
	lyrics_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(lyrics_view));
	gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(lyrics_buffer), "bold", "weight", PANGO_WEIGHT_BOLD, NULL);
	gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(lyrics_buffer), "large", "scale", PANGO_SCALE_LARGE, NULL);
	gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(lyrics_buffer), "italic", "style", PANGO_STYLE_ITALIC, NULL);
	
	bio_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(bio_view));

	bio_tab = gtk_vpaned_new();
	bio_image = gtk_image_new();
	
	GtkWidget *dlt_btn = gtk_button_new();
	gtk_widget_set_tooltip_text(dlt_btn, "Remove current cache files.");
	g_signal_connect(dlt_btn, "clicked", G_CALLBACK(delete_cache_clicked), NULL);
	
	GtkWidget *dlt_img = gtk_image_new_from_stock(GTK_STOCK_REFRESH, GTK_ICON_SIZE_SMALL_TOOLBAR); 
	gtk_button_set_image(GTK_BUTTON(dlt_btn), dlt_img);

	gtk_container_add(GTK_CONTAINER(lyrics_tab), lyrics_view);
	gtk_container_add(GTK_CONTAINER(bio_scroll), bio_view);

	gtk_paned_pack1(GTK_PANED(bio_tab), bio_image, FALSE, TRUE);
	gtk_paned_pack2(GTK_PANED(bio_tab), bio_scroll, TRUE, TRUE);

	gtk_box_pack_start(GTK_BOX(infobar_toggles), lyrics_toggle, FALSE, FALSE, 1);
	gtk_box_pack_start(GTK_BOX(infobar_toggles), bio_toggle, FALSE, FALSE, 1);
	gtk_box_pack_start(GTK_BOX(infobar_toggles), dlt_btn, FALSE, FALSE, 1);

	gtk_notebook_append_page(GTK_NOTEBOOK(infobar_tabs), lyrics_tab, NULL);
	gtk_notebook_append_page(GTK_NOTEBOOK(infobar_tabs), bio_tab, NULL);

	g_signal_connect(lyrics_toggle, "toggled", G_CALLBACK(infobar_tab_changed), lyrics_tab);
	g_signal_connect(bio_toggle, "toggled", G_CALLBACK(infobar_tab_changed), bio_tab);

	gtk_box_pack_start(GTK_BOX(infobar), infobar_toggles, FALSE, TRUE, 1);
	gtk_box_pack_start(GTK_BOX(infobar), infobar_tabs, TRUE, TRUE, 1);

	int width = deadbeef->conf_get_int(CONF_INFOBAR_WIDTH, 250);
	gtk_widget_set_size_request(infobar, width, -1);

	gtk_widget_show_all(infobar);
	return 0;
}

static int
create_infobar_interface(void) {
	trace("infobar: modifying player's inteface\n");

	int res = -1;

	res = create_infobar();
	if(res < 0) {
		trace("infobar: failed to create infobar\n");
		return -1;
	}

	GtkWidget *ddb_main = lookup_widget(gtkui_plugin->get_mainwin(), "vbox1");
	GtkWidget *ddb_tabs= lookup_widget(gtkui_plugin->get_mainwin(), "tabstrip");
	GtkWidget *ddb_playlist = lookup_widget(gtkui_plugin->get_mainwin(), "frame1");

	g_object_ref(ddb_tabs);
	g_object_ref(ddb_playlist);

	GtkWidget *playlist_box = gtk_vbox_new(FALSE, 0);

	gtk_container_remove(GTK_CONTAINER(ddb_main), ddb_tabs);
	gtk_container_remove(GTK_CONTAINER(ddb_main), ddb_playlist);

	gtk_box_pack_start(GTK_BOX(playlist_box), ddb_tabs, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(playlist_box), ddb_playlist, TRUE, TRUE, 0);

	g_object_unref(ddb_tabs);
	g_object_unref(ddb_playlist);

	GtkWidget *ddb_main_new = gtk_hpaned_new();

	gtk_paned_pack1(GTK_PANED(ddb_main_new), playlist_box, TRUE, TRUE);
	gtk_paned_pack2(GTK_PANED(ddb_main_new), infobar, FALSE, TRUE);

	gtk_container_add(GTK_CONTAINER(ddb_main), ddb_main_new);

	gtk_box_reorder_child(GTK_BOX(ddb_main), ddb_main_new, 2);
	
	gtk_widget_show(ddb_main_new);
	gtk_widget_show(playlist_box);

	gboolean state = deadbeef->conf_get_int(CONF_INFOBAR_VISIBLE, 0);
	state ? gtk_widget_show(infobar) : gtk_widget_hide(infobar); 

	return 0;
}

static int
create_infobar_menu_entry(void) {
    trace("infobar: creating infobar menu entry\n");

    GtkWidget *menu_item = gtk_check_menu_item_new_with_mnemonic ("_Infobar");
    if(!menu_item)
    	return -1;

    GtkWidget *view_menu = lookup_widget(gtkui_plugin->get_mainwin(), "View_menu");

    gtk_container_add(GTK_CONTAINER(view_menu), menu_item);

    gboolean state = deadbeef->conf_get_int(CONF_INFOBAR_VISIBLE, 0);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), state);

    g_signal_connect(menu_item, "activate", G_CALLBACK(infobar_menu_toggle), NULL);
    gtk_widget_show(menu_item);

	return 0;
}

static int
uri_encode(char *out, int outl, const char *str, SourceType type) {
	int l = outl;

    while (*str) {
        if (outl <= 1)
            return -1;

        if (!(
            (*str >= '0' && *str <= '9') ||
            (*str >= 'a' && *str <= 'z') ||
            (*str >= 'A' && *str <= 'Z') ||
            (*str == ' ') ||
            (*str == '\'')
        ))
        {
            if (outl <= 3)
                return -1;

            snprintf (out, outl, "%%%02x", (uint8_t)*str);
            outl -= 3; ++str; out += 3;
        }
        else {
        	if(*str == ' ') {
        		switch(type) {
				case LYRICSWIKIA:
				case LYRICSMANIA:
					*out = '_';
					break;
				case LYRICSTIME:
				case MEGALYRICS:
					*out = '-';
					break;
				case LASTFM:
					*out = '+';
					break;
				}
        	} else if(*str == '\'') {
				switch(type) {
				case LYRICSMANIA:
					++str;
					*out = *str;
					break;
				case LYRICSTIME:
				case MEGALYRICS:
					*out = '-';
					break;
				case LYRICSWIKIA:
				case LASTFM:
					break;
				}	
        	} else {
        		*out = *str;
        	}
            ++out; ++str; --outl;
        }
    }
    *out = 0;
    return l - outl;
}

static int
is_dir(const char *dir, mode_t mode)
{
	int res = -1;
	
    char *tmp = strdup(dir);
    char *slash = tmp;
    struct stat st;
    
    do {
        slash = strstr(slash + 1, "/");
        if(slash)
			*slash = 0;
            
        res = stat(tmp, &st);
        if(res == -1) {
            res = mkdir(tmp, mode);
            if(res != 0) {
                trace("infobar: failed to create %s\n", tmp);
                free(tmp);
                return -1;
            }
        }
        if(slash)
			*slash = '/';
			
    } while(slash);

    free(tmp);
    return 0;
}

static gboolean
is_exists(const char *obj) {
	struct stat st;
	if(stat(obj, &st) != 0)
		return FALSE;

	return TRUE;
}

static gboolean
is_old_cache(const char *cache_file, CacheType type) {
	int res = -1;
	int upd_period = 0;
	time_t tm = time(NULL);

	struct stat st;
	res = stat(cache_file, &st);
	if(res == 0) {
		switch(type) {
		case LYRICS:
			upd_period = deadbeef->conf_get_int(CONF_LYRICS_UPDATE_PERIOD, 0);
			break;
		case BIO:
			upd_period = deadbeef->conf_get_int(CONF_BIO_UPDATE_PERIOD, 24);
			break;
		}
		if(upd_period == 0) {
			return FALSE;
		}
		if(upd_period > 0 && tm - st.st_mtime > upd_period * 60 * 60) {
			return TRUE;
		} else {
			return FALSE;
		}
	}
	return TRUE;
}

static char*
convert_to_utf8(const char *str, int size) {
	int res = -1;

	const char *str_cs = deadbeef->junk_detect_charset(str);
	if(!str_cs)
		return NULL;

	char *str_cnv = calloc(size * 4, sizeof(char));
	if(!str_cnv)
		return NULL;

	res = deadbeef->junk_iconv(str, size, str_cnv, size * 4, str_cs, "utf-8");
	if(res < 0) {
		free(str_cnv);
		return NULL;
	}

	return str_cnv;
}

static char*
load_content(const char *cache_file) {
	int res = -1;
	int cnt_size = 0;

	char *cnt = NULL;
	FILE *in_file = NULL;

	in_file = fopen(cache_file, "r");
	if(!in_file) {
		trace("infobar: failed to open content file %s\n", cache_file);
		goto cleanup;
	}

	res = fseek(in_file, 0, SEEK_END);
	if(res != 0)
		goto cleanup;

	cnt_size = ftell(in_file);
	rewind(in_file);

	cnt = calloc(cnt_size + 1, sizeof(char));
	if(!cnt)
		goto cleanup;

	res = fread(cnt, 1, cnt_size, in_file);
	if(res != cnt_size) {
		trace("infobar: failed to read content file %s\n", cache_file);
	}

cleanup:
	if(in_file) fclose(in_file);
	return cnt;
}

static int
save_content(const char *cache_file, const char *buffer, int size) {
	int res = -1;
	int ret_value = 0;

	FILE *out_file = NULL;

	out_file = fopen(cache_file, "w+");
	if(!out_file) {
		trace("infobar: failed to open content file %s\n", cache_file);
		ret_value = -1;
		goto cleanup;
	}

	res = fwrite(buffer, 1, size, out_file);
	if(res <= 0) {
		trace("infobar: failed to write to the content %s\n", cache_file);
		ret_value = -1;
	}

cleanup:
	if(out_file) fclose(out_file);
	return ret_value;
}

static void
parser_errors_handler(void *ctx, const char *msg, ...) {}

static char*
parse_content(const char *cnt, int size, const char *pattern, ContentType type, int node_num) {
	xmlDocPtr doc = NULL;
	xmlXPathContextPtr ctx = NULL;
	xmlXPathObjectPtr obj = NULL;
	xmlNodePtr node = NULL;

	char *parsed_cnt = NULL;

	xmlSetGenericErrorFunc(NULL, parser_errors_handler);

	switch(type) {
	case HTML:
		doc = htmlReadMemory(cnt, size, NULL, "utf-8", (HTML_PARSE_RECOVER | HTML_PARSE_NONET));
		break;
	case XML:
		doc = xmlReadMemory(cnt, size, NULL, "utf-8", (XML_PARSE_RECOVER | XML_PARSE_NONET));
		break;
	default:
		goto cleanup;
	}

	xmlSetGenericErrorFunc(NULL, NULL);

	if(!doc)
		goto cleanup;

	ctx = xmlXPathNewContext(doc);
	if(!ctx)
		goto cleanup;

	obj = xmlXPathEvalExpression((xmlChar*)pattern, ctx);
	if(!obj || !obj->nodesetval->nodeMax)
		goto cleanup;

	node = obj->nodesetval->nodeTab[node_num];
	if(!node)
		goto cleanup;

	parsed_cnt = (char*)xmlNodeGetContent(node);

cleanup:
	if(obj) xmlXPathFreeObject(obj);
	if(ctx) xmlXPathFreeContext(ctx);
	if(doc) xmlFreeDoc(doc);
	return parsed_cnt;
}

static char*
retrieve_txt_content(const char *url, int size) {
	int res = -1;

    char *txt = NULL;
    infobar_cnt = NULL;

    infobar_cnt = deadbeef->fopen(url);
    if(!infobar_cnt) {
    	trace("infobar: failed to open %s\n", url);
    	goto cleanup;
    }

    txt = calloc(size + 1, sizeof(char));
    if(!txt)
    	goto cleanup;

    res = deadbeef->fread(txt, 1, size, infobar_cnt);
    if(res <= 0) {
    	trace("infobar: failed to retrieve a content from %s\n", url);
    }

cleanup:
    if(infobar_cnt) deadbeef->fclose(infobar_cnt);
    infobar_cnt = NULL;
    return txt;
}

static int
retrieve_img_content(const char *url, const char *img) {
	int ret_value  = 0;

	infobar_cnt = NULL;
	FILE *out_file = NULL;

	infobar_cnt = deadbeef->fopen(url);
	if(!infobar_cnt) {
		trace("infobar: failed to open %s\n", url);
		ret_value = -1;
		goto cleanup;
	}

	out_file = fopen(img, "wb+");
	if(!out_file) {
		trace("infobar: failed to open %s", img);
		ret_value = -1;
		goto cleanup;
	}

	int len = 0;
	char tmp[4096] = {0};

	while((len = deadbeef->fread(tmp, 1, sizeof(tmp), infobar_cnt)) > 0) {
		if(fwrite(tmp, 1, len, out_file) != len) {
			trace ("infobar: failed to write to the %s\n", img);
			ret_value = -1;
			break;
		}
	}

cleanup:
	if(out_file) fclose(out_file);
	if(infobar_cnt) deadbeef->fclose(infobar_cnt);
	infobar_cnt = NULL;
	return ret_value;
}

static int
retrieve_artist_bio(void) {
	trace("infobar: retrieve artist bio started\n");

	int res = -1;
	int ret_value = 0;

	char *bio = NULL;
	char *cnt = NULL;
	char *img_url = NULL;
	char *img_file = NULL;

	int bio_size = 0;
	int img_size = 0;
	int cnt_size = 0;
	
	char eartist[300] = {0};
	
	BioViewData *data = malloc(sizeof(BioViewData));
	if(!data)
		goto cleanup;

	deadbeef->mutex_lock(infobar_mutex);
	
	trace("infobar: checking if the artist is the same\n");
	if(!artist_changed) 
		goto cleanup;

	trace("infobar: retrieving bio cache path\n");
	char cache_path[512] = {0};
	res = get_cache_path(cache_path, sizeof(cache_path), BIO);
	if(res == 0) {
		trace("infobar: failed to retrieve bio cache dir\n");
		ret_value = -1;
		goto cleanup;
	}

	trace("infobar: creating bio cache dir\n");
	if(!is_exists(cache_path)) {
		res = is_dir(cache_path, 0755);
		if(res < 0) {
			trace("infobar: failed to create bio cache dir\n");
			ret_value = -1;
			goto cleanup;
		}
	}

	trace("infobar: forming a path to the bio cache file\n");
	char cache_file[512] = {0};
	res = snprintf(cache_file, sizeof(cache_file), "%s/%s", cache_path, artist);
	if(res == 0) {
		trace("infobar: failed to form a path to the bio cache file\n");
		ret_value = -1;
		goto cleanup;
	}

	trace("infobar: forming a path to the bio image file\n");
	res = asprintf(&img_file, "%s/%s_img", cache_path, artist);
	if(res == -1) {
		trace("infobar: failed to form a path to the bio image file\n");
		ret_value = -1;
		goto cleanup;
	}

	trace("infobar: encoding artist's name\n");
	res = uri_encode(eartist, sizeof(eartist), artist, LASTFM);
	if(res == -1) {
		trace("infobar: failed to encode artist's name\n");
		ret_value = -1;
		goto cleanup;
	}

	char cur_locale[5] = {0};
	deadbeef->conf_get_str(CONF_BIO_LOCALE, "en", cur_locale, sizeof(cur_locale));
	
	trace("infobar: forming bio download url\n");
	char track_url[512] = {0};
	res = snprintf(track_url, sizeof(track_url), "http://ws.audioscrobbler.com/2.0/?method=artist.getinfo&artist=%s&lang=%s&api_key=b25b959554ed76058ac220b7b2e0a026",
			eartist, cur_locale);
	if(res == 0) {
		trace("infobar: failed to form bio download url\n");
		ret_value = -1;
		goto cleanup;
	}

	if(!is_exists(cache_file) ||
		is_old_cache(cache_file, BIO)) {
		trace("infobar: trying to download artist's bio\n");
		cnt = retrieve_txt_content(track_url, TXT_MAX);
		if(!cnt) {
			trace("infobar: failed to download artist's bio\n");
			ret_value = -1;
			goto cleanup;
		}

		cnt_size = strlen(cnt);
		bio = parse_content(cnt, cnt_size, "/lfm/artist/bio/content", XML, 0);
		if(bio) {
			bio_size = strlen(bio);
			
			char *tmp = NULL;

			tmp = parse_content(bio, bio_size, "/html/body", HTML, 0);
			if(tmp) {
				free(bio);
				bio = tmp;
				bio_size = strlen(bio);
			}
			
			if(deadbeef->junk_detect_charset(bio)) {
				trace("infobar: converting bio to the utf-8\n");
				tmp = convert_to_utf8(bio, bio_size);
				if(tmp) {
					free(bio);
					bio = tmp;
					bio_size = strlen(bio);
				}
			}
	
			trace("infobar: trying to save retrieved bio to the cache file\n");
			res = save_content(cache_file, bio, bio_size);
			if(res < 0) {
				trace("infobar: failed to save retrieved bio\n");
				ret_value = -1;
				goto cleanup;
			}
		}
	} else {
		trace("infobar: trying to load bio from the cache file\n");
		bio = load_content(cache_file);
		if(bio) {
			bio_size = strlen(bio);
		}
	}

	if(!is_exists(img_file) || 
		is_old_cache(img_file, BIO)) {
		if(!cnt) {
			cnt = retrieve_txt_content(track_url, TXT_MAX);
			if(!cnt) {
				trace("infobar: failed to download artist's bio\n");
				ret_value = -1;
				goto cleanup;
			}
		}

		cnt_size = strlen(cnt);
		img_url = parse_content(cnt, cnt_size, "//image[@size=\"extralarge\"]", XML, 0);
		if(img_url) {
			img_size = strlen(img_url);
		}	

		if(img_url && img_size > 0) {
			trace("infobar: trying to download artist's image\n");
			res = retrieve_img_content(img_url, img_file);
			if(res < 0) {
				trace("infobar: failed to download artist's image\n");
				ret_value = -1;
				goto cleanup;
			}
		}
	}

cleanup:
	if(infobar_mutex) {
		deadbeef->mutex_unlock(infobar_mutex);
	}

	if(data) {
		data->txt = bio;
		data->img = img_file;
		data->size = bio_size;
	}
	
	if(artist_changed) {
		trace("infobar: starting bio view update\n");
		g_idle_add((GSourceFunc)update_bio_view, data);
	}
	
	if(cnt) free(cnt);
	if(img_url) free(img_url);
	return ret_value;
}

static gboolean
is_redirect(const char* lyrics) {
	if(!lyrics) {
		return FALSE;
	}
	char tmp[9] = {0};
	memcpy(tmp, lyrics, sizeof(tmp));
	
	for(int i = 0; i < sizeof(tmp); ++i) {
		tmp[i] = tolower(tmp[i]);
	}
		
	if(memcmp(tmp, "#redirect", sizeof(tmp)) == 0) {
		return TRUE;
	}
	return FALSE;
}
		
static int 
get_redirect_info(const char *buf, int size, char *new_artist, int asize, char *new_title, int tsize) {
	int begin = 0, mid = 0, end = 0;
	
	for(int i = 0; i < size; ++i) {
		if(buf[i] == '[' &&
		   buf[++i] == '[')
		{
			begin = i + 1;
		}
		if(buf[i] == ':') {
			mid = i;
		}
		if(buf[i] == ']') {
			end = i - 1;
		}
	}
	int red_asize = mid - begin;
	int red_tsize = end - mid;
	
	if(asize < red_asize || 
	   tsize < red_tsize) 
	{
		return -1;
	}
	memcpy(new_artist, buf + begin, red_asize);
	memcpy(new_title, buf + mid + 1, red_tsize - 1);
	return 0;
}

static int
get_new_lines_count(const char *buf, int size) {
	int nlnum = 0;
	
	for(int i = 0; i < size; ++i) {
		if(buf[i] == '\n' ||
		  (buf[i] == '\r' &&
		   buf[i + 1] == '\n')) 
		{
			++nlnum;
		} else { 
			break;
		}
	}
	return nlnum;
}

static char*
cleanup_new_lines(const char *buf, int size, int nlnum) {
	char *cld_buf = NULL;
	
	cld_buf = calloc(size + 1, sizeof(char));
	if(!cld_buf) {
		return NULL;
	}
	memcpy(cld_buf, buf + nlnum, size - nlnum + 1);
	return cld_buf;
}

static char*
fetch_lyrics_from(const char *url, const char *artist, const char *title, const char *cache_file, const char *pattern, ContentType type) {
	int res = -1;

	char *cnt = NULL;
	char *lyrics = NULL;

	int cnt_size = 0;
	int lyrics_size = 0;

	trace("infobar: forming lyrics download url\n");
	char track_url[512] = {0};
	res = snprintf(track_url, sizeof(track_url), url, artist, title);
	if(res == 0) {
		trace("infobar: failed to form lyrics download url\n");
		return NULL;
	}

	cnt = retrieve_txt_content(track_url, TXT_MAX);
	if(!cnt) {
		trace("infobar: failed to download track's lyrics\n");
		return NULL;
	}

	cnt_size = strlen(cnt);
	trace("infobar: parsing retrieved lyrics\n");
	lyrics = parse_content(cnt, cnt_size, pattern, type, 0);
	if(lyrics) {
		if(deadbeef->junk_detect_charset(lyrics)) {
			lyrics_size = strlen(lyrics);
			trace("infobar: converting lyrics to the utf-8\n");
			char *tmp = convert_to_utf8(lyrics, lyrics_size);
			if(tmp) {
				free(lyrics);
				lyrics = tmp;
			}
		}
	}
	free(cnt);
	return lyrics;
}

static int
retrieve_track_lyrics(void) {
	trace("infobar: retrieve track lyrics started\n");

	int res = -1;
	int ret_value = 0;
	int lyrics_size = 0;

	char *lyrics = NULL;

	gboolean wikia = FALSE;
	gboolean time = FALSE;
	gboolean mania = FALSE;
	gboolean mega = FALSE;
	
	char eartist[300] = {0};
	char etitle[300] = {0};
		
	LyricsViewData *data = malloc(sizeof(LyricsViewData));
	if(!data)
		goto cleanup;

	deadbeef->mutex_lock(infobar_mutex);

	trace("infobar: retrieving lyrics cache path\n");
	char cache_path[512] = {0};
	res = get_cache_path(cache_path, sizeof(cache_path), LYRICS);
	if(res == 0) {
		trace("infobar: failed to retrieve lyrics cache path\n");
		ret_value = -1;
		goto cleanup;
	}

	trace("infobar: creating cache dir\n");
	if(!is_exists(cache_path)) {
		res = is_dir(cache_path, 0755);
		if(res < 0) {
			trace("infobar: failed to create lyrics cache dir\n");
			ret_value = -1;
			goto cleanup;
		}
	}

	trace("infobar: forming a path to the lyrics cache file\n")
	char cache_file[512] = {0};
	res = snprintf(cache_file, sizeof(cache_file), "%s/%s-%s", cache_path, artist, title);
	if(res == 0) {
		trace("infobar: failed to form a path to the lyrics cache file\n");
		ret_value = -1;
		goto cleanup;
	}
	
	if(!is_exists(cache_file) ||
		is_old_cache(cache_file, LYRICS)) {
			
		trace("infobar: trying to fetch lyrics from lyricswikia\n");
		wikia = deadbeef->conf_get_int(CONF_LYRICSWIKIA_ENABLED, 1);
		if(wikia && !lyrics && lyrics_size == 0) {
			memset(eartist, 0, sizeof(eartist));
			memset(etitle, 0, sizeof(etitle));
			
			if(uri_encode(eartist, sizeof(eartist), artist, LYRICSWIKIA) != -1 &&
			   uri_encode(etitle, sizeof(etitle), title, LYRICSWIKIA) != -1) {
			
				lyrics = fetch_lyrics_from("http://lyrics.wikia.com/api.php?action=query&prop=revisions&rvprop=content&format=xml&titles=%s:%s",
						eartist, etitle, cache_file, "//rev", XML);
				if(lyrics) {
					lyrics_size = strlen(lyrics);
				}
				
				if(is_redirect(lyrics) && lyrics_size > 0) {
					trace("infobar: lyricswikia gave a redirect\n");
						
					char tmp_artist[100] = {0};
					char tmp_title[100] = {0};
						
					res = get_redirect_info(lyrics, lyrics_size, tmp_artist, 
							sizeof(tmp_artist), tmp_title, sizeof(tmp_title));
					if(res == 0) {						
						memset(eartist, 0, sizeof(eartist));
						memset(etitle, 0, sizeof(etitle));
										
						if(uri_encode(eartist, sizeof(eartist), tmp_artist, LYRICSWIKIA) != -1 &&
						   uri_encode(etitle, sizeof(etitle), tmp_title, LYRICSWIKIA) != -1) {
							free(lyrics);			
								
							trace("infobar: trying to fetch lyrics with new eartist and etitile\n");
							lyrics = fetch_lyrics_from("http://lyrics.wikia.com/api.php?action=query&prop=revisions&rvprop=content&format=xml&titles=%s:%s",
									eartist, etitle, cache_file, "//rev", XML);
							if(lyrics) {
								lyrics_size = strlen(lyrics);
							}
						}
					}
				}
				
				if(lyrics && lyrics_size > 0) {
					trace("infobar: parsing content, retrieved from lyricswikia\n");
					char *tmp1 = parse_content(lyrics, lyrics_size, "//lyrics", HTML, 0);
					if(tmp1) {
						trace("infobar: checking if one more lyrics is available\n");
						char *tmp2 = parse_content(lyrics, lyrics_size, "//lyrics", HTML, 1);
						if(tmp2) {
							int tmp1_size = strlen(tmp1);
							int tmp2_size = strlen(tmp2);
							
							free(lyrics);
							lyrics = calloc(tmp1_size + tmp2_size + 1, sizeof(char));
							if(lyrics) {
								strncpy(lyrics, tmp1, tmp1_size);
								strncat(lyrics, tmp2, tmp2_size);
								lyrics_size = tmp1_size + tmp2_size;
							}
							free(tmp1);
							free(tmp2);
						} else {
							free(lyrics);
							lyrics = tmp1;
							lyrics_size = strlen(lyrics);
						}
					}
				}
			}
		}

		trace("infobar: trying to fetch lyrics from lyricsmania\n");
		mania = deadbeef->conf_get_int(CONF_LYRICSMANIA_ENABLED, 1);
		if(mania && !lyrics && lyrics_size == 0) {
			memset(eartist, 0, sizeof(eartist));
			memset(etitle, 0, sizeof(etitle));
			
			if(uri_encode(eartist, sizeof(eartist), artist, LYRICSMANIA) != -1 &&
			   uri_encode(etitle, sizeof(etitle), title, LYRICSMANIA) != -1) {
				
				lyrics = fetch_lyrics_from("http://www.lyricsmania.com/%s_lyrics_%s.html",
						etitle, eartist, cache_file, "//*[@id=\"songlyrics_h\"]", HTML);
				if(lyrics) {
					lyrics_size = strlen(lyrics);
				}
			}
		}
	
		trace("infobar: trying to fetch lyrics from lyricstime\n");
		time = deadbeef->conf_get_int(CONF_LYRICSTIME_ENABLED, 1);
		if(time && !lyrics && lyrics_size == 0) {
			memset(eartist, 0, sizeof(eartist));
			memset(etitle, 0, sizeof(etitle));
			
			if(uri_encode(eartist, sizeof(eartist), artist, LYRICSTIME) != -1 &&
			   uri_encode(etitle, sizeof(etitle), title, LYRICSTIME) != -1) {
				   
				lyrics = fetch_lyrics_from("http://www.lyricstime.com/%s-%s-lyrics.html",
						eartist, etitle, cache_file, "//*[@id=\"songlyrics\"]", HTML);
				if(lyrics) {
					lyrics_size = strlen(lyrics);
				}
			}
		}
	
		trace("infobar: trying to fetch lyrics from megalyrics\n");
		mega = deadbeef->conf_get_int(CONF_MEGALYRICS_ENABLED, 1);
		if(mega && !lyrics && lyrics_size == 0) {
			memset(eartist, 0, sizeof(eartist));
			memset(etitle, 0, sizeof(etitle));
			
			if(uri_encode(eartist, sizeof(eartist), artist, MEGALYRICS) != -1 &&
			   uri_encode(etitle, sizeof(etitle), title, MEGALYRICS) != -1) {
				   
				lyrics = fetch_lyrics_from("http://megalyrics.ru/lyric/%s/%s.htm",
						eartist, etitle, cache_file, "//pre[@class=\"lyric\"]", HTML);
				if(lyrics) {
					lyrics_size = strlen(lyrics);
				}
			}
		}
		
		trace("infobar: trying to remove new line symbols\n");
		if(lyrics && lyrics_size > 0) {
			int nlnum = 0;
			
			nlnum = get_new_lines_count(lyrics, lyrics_size);
			if(nlnum > 0) {
				char *tmp = cleanup_new_lines(lyrics, lyrics_size, nlnum);
				if(tmp) {
					free(lyrics);
					lyrics = tmp;
					lyrics_size = strlen(lyrics);
				}
			}
		}
	
		trace("infobar: trying to save retrieved lyrics to the cache file\n");
		if(lyrics && lyrics_size > 0) {
			res = save_content(cache_file, lyrics, lyrics_size);
			if(res < 0) {
				trace("infobar: failed to save retrieved track's lyrics\n");
				ret_value = -1;
				goto cleanup;
			}
		}
	} else {
		trace("infobar: trying to load lyrics from the cache file\n");
		lyrics = load_content(cache_file);
		if(lyrics) {
			lyrics_size = strlen(lyrics);
		}
	}

cleanup:
	if(infobar_mutex) {
		deadbeef->mutex_unlock(infobar_mutex);
	}
	
	if(data) {
		data->txt = lyrics;
		data->size = lyrics_size;
	}
	
	trace("infobar: starting lyrics view update\n");
	g_idle_add((GSourceFunc)update_lyrics_view, data);
	return ret_value;
}

static int 
get_track_info(DB_playItem_t *track, char *artist, int asize, char *title, int tsize) {
	deadbeef->pl_lock();

	const char *cur_artist = deadbeef->pl_find_meta(track, "artist");
	const char *cur_title =  deadbeef->pl_find_meta(track, "title");
	
	if(!cur_artist || !cur_title) {
		deadbeef->pl_unlock();
		return -1;
	}
	
	strncpy(artist, cur_artist, asize);
	strncpy(title, cur_title, tsize);

	deadbeef->pl_unlock();
	return 0;
}

static void
infobar_songstarted(ddb_event_track_t *ev) {
	trace("infobar: infobar song started\n");

	int res = -1;
	
	if(!ev->track)
		return;
	
	trace("infobar: retrieving playing track\n");
	DB_playItem_t *pl_track = deadbeef->streamer_get_playing_track();
	if(!pl_track) {
		trace("infobar: playing track is null\n");
		return;
	}	
		
	if(ev->track != pl_track) {
		trace("infobar: event track is not the same as the current playing track\n");
		deadbeef->pl_item_unref(pl_track);
		return;
	} 
	deadbeef->pl_item_unref(pl_track);
		
	if(!deadbeef->conf_get_int(CONF_INFOBAR_VISIBLE, 0)) {
		trace("infobar: infobar is set to non visible\n");
		return;
	}
		
	if(!deadbeef->conf_get_int(CONF_LYRICS_ENABLED, 1) &&
			!deadbeef->conf_get_int(CONF_BIO_ENABLED, 1)) {
		trace("infobar: lyrics and bio are disabled\n");		
		return;
	}

	deadbeef->mutex_lock(infobar_mutex);
		
	memset(artist, 0, sizeof(artist));
	memset(title, 0, sizeof(title));
	
	res = get_track_info(ev->track, artist, sizeof(artist), title, sizeof(title));
	if(res == -1) {
		trace("infobar: failed to get track info\n");
	    deadbeef->mutex_unlock(infobar_mutex);
    	return;
	}
	
	trace("infobar: current playing artist: %s, title, %s\n", artist, title);
	
	if(strcmp(old_artist, artist) == 0 && 
			strcmp(old_title, title) == 0) {
		trace("infobar: same artist and title\n");
		deadbeef->mutex_unlock(infobar_mutex);
		return;
	}
	
	res = strcmp(old_artist, artist);
	if(res == 0) {
		artist_changed = FALSE;
	} else {
		artist_changed = TRUE;
	}
	
	memset(old_artist, 0, sizeof(old_artist));
	strncpy(old_artist, artist, sizeof(old_artist));
	
	memset(old_title, 0, sizeof(old_title));
	strncpy(old_title, title, sizeof(old_title));
	
	deadbeef->mutex_unlock(infobar_mutex);
	deadbeef->cond_signal(infobar_cond);
}

static void
infobar_songchanged(void) {
	if(infobar_cnt) {
		if(infobar_mutex) {
			deadbeef->mutex_unlock(infobar_mutex);
		}
		deadbeef->fabort(infobar_cnt);
		infobar_cnt = NULL;
	}
}

static void
infobar_thread(void *ctx) {
	for(;;) {
        trace("infobar: infobar thread started\n");

        deadbeef->cond_wait(infobar_cond, infobar_mutex);
        deadbeef->mutex_unlock(infobar_mutex);

		trace("infobar: condition signaled\n");
        if(infobar_stopped) {
            return;
        }

        if(deadbeef->conf_get_int(CONF_LYRICS_ENABLED, 1)) {
            trace("infobar: retrieving song's lyrics...\n");
            retrieve_track_lyrics();
        }

        if(deadbeef->conf_get_int(CONF_BIO_ENABLED, 1)) {
            trace("infobar: retrieving artist's bio...\n");
            retrieve_artist_bio();
        }
    }
}

static gboolean 
is_stream(DB_playItem_t *track) {
	if(deadbeef->pl_get_item_duration(track) <= 0.000000) {
		return TRUE;
	}
	return FALSE;
}

static int
infobar_message(uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2) {
	switch(id) {
	case DB_EV_SONGSTARTED:
	{
		trace("infobar: recieved songstarted message\n");
		ddb_event_track_t* event = (ddb_event_track_t*) ctx;
		if(!event->track) 
			return 0;
		if(!is_stream(event->track))
			infobar_songstarted(event);
	}
		break;
	case DB_EV_TRACKINFOCHANGED:
	{
		trace("infobar: recieved trackinfochanged message\n");
		ddb_event_track_t* event = (ddb_event_track_t*) ctx;
		if(!event->track) 
			return 0;
		if(is_stream(event->track))
			infobar_songstarted(event);
	}
		break;
	case DB_EV_SONGCHANGED:
		infobar_songchanged();
		break;
	case DB_EV_CONFIGCHANGED:
		g_idle_add((GSourceFunc)infobar_config_changed, NULL);
		break;
	}
	return 0;
}

static gboolean
infobar_init(void) {
	trace("infobar: starting up infobar plugin\n");

	int res = -1;

	res = create_infobar_interface();
	if(res < 0)
		return TRUE;

	res = create_infobar_menu_entry();
	if(res < 0)
		return TRUE;

	infobar_config_changed();
	return FALSE;
}

static int
infobar_connect(void) {
	trace("infobar: connecting infobar plugin\n");

	gtkui_plugin = (ddb_gtkui_t*)deadbeef->plug_get_for_id("gtkui");
	if(!gtkui_plugin)
		return -1;

	g_idle_add((GSourceFunc)infobar_init, NULL);
	return 0;
}

static int
infobar_disconnect(void) {
	trace("infobar: disconnecting infobar plugin\n");
	gtkui_plugin = NULL;
	return 0;
}

static int
infobar_start(void) {
	trace("infobar: starting infobar plugin\n");

	infobar_stopped = FALSE;

	infobar_cond = deadbeef->cond_create();
	infobar_mutex = deadbeef->mutex_create_nonrecursive();
	infobar_tid = deadbeef->thread_start_low_priority(infobar_thread, NULL);
	return 0;
}

static int
infobar_stop(void) {
	trace("infobar: stopping infobar plugin\n");

	infobar_stopped = TRUE;

	if(infobar_cnt) {
		deadbeef->fabort(infobar_cnt);
		infobar_cnt = NULL;
	}

	if(infobar_tid) {
		deadbeef->cond_signal(infobar_cond);
		deadbeef->thread_join(infobar_tid);
		infobar_tid = 0;
	}

	if(infobar_mutex) {
		deadbeef->mutex_unlock(infobar_mutex);
		deadbeef->mutex_free(infobar_mutex);
		infobar_mutex = 0;
	}

	if(infobar_cond) {
		deadbeef->cond_free(infobar_cond);
		infobar_cond = 0;
	}
	return 0;
}

static const char settings_dlg[] =
    "property \"Enable lyrics\" checkbox infobar.lyrics.enabled 1;"
    "property \"Fetch from Lyricswikia\" checkbox infobar.lyrics.lyricswikia 1;"
	"property \"Fetch from Lyricsmania\" checkbox infobar.lyrics.lyricsmania 1;"
	"property \"Fetch from Lyricstime\" checkbox infobar.lyrics.lyricstime 1;"
	"property \"Fetch from Megalyrics\" checkbox infobar.lyrics.megalyrics 1;"
	"property \"Enable biography\" checkbox infobar.bio.enabled 1;"
	"property \"Biography locale\" entry infobar.bio.locale \"en\";"
	"property \"Lyrics alignment type\" entry infobar.lyrics.alignment 2;"
	"property \"Lyrics cache update period (hr)\" entry infobar.lyrics.cache.period 0;"
	"property \"Biography cache update period (hr)\" entry infobar.bio.cache.period 24;"
	"property \"Default sidebar width (px)\" entry infobar.width 250;"
;

static DB_misc_t plugin = {
	.plugin.api_vmajor = 1,
    .plugin.api_vminor = 0,
    .plugin.version_major = 1,
    .plugin.version_minor = 0,
    .plugin.type = DB_PLUGIN_MISC,
    .plugin.name = "Infobar plugin",
    .plugin.descr = "Fetches and shows song's lyrics and artist's biography.\n\n"
    				"To change the biography's locale, set an appropriate ISO 639-2 locale code.\n"
    				"See http://en.wikipedia.org/wiki/List_of_ISO_639-2_codes for more infomation.\n\n"
    				"Lyrics alignment types:\n1 - Left\n2 - Center\n3 - Right\n(changing requires restart)\n\n"
    				"You can set cache update period to 0 if you don't want to update the cache at all.",
    .plugin.copyright =
        "Copyright (C) 2011 Dmitriy Simbiriatin <slpiv@mail.ru>\n"
        "\n"
        "This program is free software; you can redistribute it and/or\n"
        "modify it under the terms of the GNU General Public License\n"
        "as published by the Free Software Foundation; either version 2\n"
        "of the License, or (at your option) any later version.\n"
        "\n"
        "This program is distributed in the hope that it will be useful,\n"
        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
        "GNU General Public License for more details.\n"
        "\n"
        "You should have received a copy of the GNU General Public License\n"
        "along with this program; if not, write to the Free Software\n"
        "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.\n"
    ,
    .plugin.website = "https://bitbucket.org/Not_eXist/deadbeef-infobar",
    .plugin.start = infobar_start,
    .plugin.stop = infobar_stop,
    .plugin.connect	= infobar_connect,
    .plugin.disconnect = infobar_disconnect,
    .plugin.configdialog = settings_dlg,
    .plugin.message = infobar_message,
};

DB_plugin_t *ddb_infobar_load (DB_functions_t *ddb) {
    deadbeef = ddb;
    return DB_PLUGIN(&plugin);
}
