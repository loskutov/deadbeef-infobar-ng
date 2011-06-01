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

typedef struct {
	char *txt;
	int len;
} LyricsViewData;

typedef struct {
	char *txt;
	char *img;
	int len;
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
#define CONF_BIO_IMAGE_HEIGHT "infobar.bio.image.height"
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
static GtkWidget *lyr_toggle;
static GtkWidget *bio_toggle;
static GtkWidget *lyr_tab;
static GtkWidget *bio_tab;
static GtkWidget *bio_image;
static GdkPixbuf *bio_pixbuf;

GtkTextBuffer *lyr_buffer;
GtkTextBuffer *bio_buffer;

static int
get_cache_path(char *cache_path, int len, ContentType type) {
	int res = -1;

	const char *home_cache = getenv("XDG_CACHE_HOME");

	switch(type) {
	case LYRICS:
		res = snprintf(cache_path, len, home_cache ? "%s/deadbeef/lyrics" : "%s/.cache/deadbeef/lyrics",
				home_cache ? home_cache : getenv("HOME"));
		break;
	case BIO:
		res = snprintf(cache_path, len, home_cache ? "%s/deadbeef/bio" : "%s/.cache/deadbeef/bio",
				home_cache ? home_cache : getenv("HOME"));
		break;
	}
	return res;
}

static gboolean
update_bio_view(gpointer data) {	
    GtkTextIter begin, end;
	BioViewData *bio_data = (BioViewData*) data;
	
	gdk_threads_enter();

    if(bio_image) {
		if(bio_pixbuf) {
			g_object_unref(bio_pixbuf);
			bio_pixbuf = NULL;
		}
    	bio_pixbuf = gdk_pixbuf_new_from_file(bio_data->img, NULL);
		gtk_widget_queue_draw(bio_image);
	}

    if(bio_buffer) {
    	gtk_text_buffer_get_iter_at_line (bio_buffer, &begin, 0);
    	gtk_text_buffer_get_end_iter (bio_buffer, &end);
    	gtk_text_buffer_delete (bio_buffer, &begin, &end);

    	if(bio_data->txt && bio_data->len > 0) {
    		gtk_text_buffer_insert(bio_buffer, &begin, 
				bio_data->txt, bio_data->len);
    	} else {
    		gtk_text_buffer_insert(bio_buffer, &begin,
				"Biography not found.", -1);
    	}
    }
    
    gdk_threads_leave();
    
    if(bio_data->txt) {
		free(bio_data->txt);
    }
    if(bio_data->img) {
		 free(bio_data->img);
    }
    if(bio_data) {
		free(bio_data);
    }
    return FALSE;
}

static gboolean
update_lyrics_view(gpointer data) {	
    GtkTextIter begin, end;
	LyricsViewData *lyr_data = (LyricsViewData*) data;
	
	gdk_threads_enter();

    if(lyr_buffer) {
    	gtk_text_buffer_get_iter_at_line (lyr_buffer, &begin, 0);
    	gtk_text_buffer_get_end_iter (lyr_buffer, &end);
    	gtk_text_buffer_delete (lyr_buffer, &begin, &end);
		
		gtk_text_buffer_insert_with_tags_by_name(GTK_TEXT_BUFFER(lyr_buffer), 
				&begin, title, -1, "bold", "large", NULL);
    	gtk_text_buffer_insert(lyr_buffer, &begin, "\n", -1);
    	gtk_text_buffer_insert_with_tags_by_name(GTK_TEXT_BUFFER(lyr_buffer),
				&begin, artist, -1, "italic", NULL);
    	gtk_text_buffer_insert(lyr_buffer, &begin, "\n\n", -1);

    	if(lyr_data->txt && lyr_data->len > 0) {
    		gtk_text_buffer_insert(lyr_buffer, &begin, 
				lyr_data->txt, lyr_data->len);
    	} else {
    		gtk_text_buffer_insert(lyr_buffer, &begin, 
				"Lyrics not found.", -1);
    	}
    }
    
    gdk_threads_leave();
    
    if(lyr_data->txt) {
		free(lyr_data->txt);
    }
    if(lyr_data) {
		free(lyr_data);
    }
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
		char lyr_path[512] = {0};
		res = get_cache_path(lyr_path, sizeof(lyr_path), LYRICS);
		if(res > 0) {
			char lyr_file[512] = {0};
			res = snprintf(lyr_file, sizeof(lyr_file), "%s/%s-%s", lyr_path, artist, title);
			if(res > 0) {
				res = remove(lyr_file);
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
	gdk_threads_enter();

	gboolean state = FALSE;
	state = deadbeef->conf_get_int(CONF_LYRICS_ENABLED, 1);
	if(lyr_toggle && lyr_tab) {
		set_tab_visible(lyr_toggle, lyr_tab, state);
	}
	state = deadbeef->conf_get_int(CONF_BIO_ENABLED, 1);
	if(bio_toggle && bio_tab) {
		set_tab_visible(bio_toggle, bio_tab, state);
	}
		
	gdk_threads_leave();
	return FALSE;
}

static gboolean
infobar_menu_toggle(GtkMenuItem *item, gpointer data) {
    gboolean state = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item));
	state ? gtk_widget_show(infobar) : gtk_widget_hide(infobar); 
    deadbeef->conf_set_int(CONF_INFOBAR_VISIBLE, (gint) state);
    return FALSE;
}

static gboolean
infobar_tab_changed(GtkToggleButton *toggle, GtkWidget *widget) {
	gint index = gtk_notebook_page_num(GTK_NOTEBOOK(infobar_tabs), widget);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(infobar_tabs), index);
	return FALSE;
}

static gboolean
bio_image_expose(GtkWidget *image, GdkEventExpose *event, gpointer data) {
	if(bio_pixbuf) {
		float ww = gdk_pixbuf_get_width(bio_pixbuf);
		float wh = gdk_pixbuf_get_height(bio_pixbuf);
	
		float aw = image->allocation.width;
		float ah = image->allocation.height;
	
		if(aw < 10) aw = 10;
		if(ah < 10) ah = 10;
		
		float w = 0, h = 0;
		float ratio = wh / ww;
	
		if(ww > wh) {
			w = ww > aw ? aw : ww;
			h = w * ratio;
		} else {
			h = wh > ah ? ah : wh;
			w = h / ratio;
		}

		if(w > aw) {
			w = aw;
			h = w * ratio;
		}
		if(h > ah) {
			h = ah;
			w = h / ratio;
		}
		
		int pos_x = (aw - w) / 2;
		int pos_y = (ah - h) / 2;	
		
		GdkPixbuf *sld = gdk_pixbuf_scale_simple(bio_pixbuf, w, h, GDK_INTERP_BILINEAR);
		if(sld) {
			cairo_t *cr = gdk_cairo_create(image->window);
			if(cr) {				
				gdk_cairo_set_source_pixbuf(cr, sld, pos_x, pos_y);
				cairo_paint(cr);
				cairo_destroy(cr);
			}
			g_object_unref(sld);
		}
	}
	return FALSE;
}

static void
create_infobar(void) {
	trace("infobar: creating infobar\n");
		
	infobar = gtk_vbox_new(FALSE, 0);

	infobar_tabs = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(infobar_tabs), FALSE);

	GtkWidget *infobar_toggles = gtk_hbox_new(FALSE, 0);

	lyr_toggle = gtk_radio_button_new_with_label(NULL, "Lyrics");
	gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(lyr_toggle), FALSE);

	bio_toggle = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(lyr_toggle), "Biography");
	gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(bio_toggle), FALSE);

	lyr_tab = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(lyr_tab),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	GtkWidget *bio_scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(bio_scroll),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	GtkWidget *lyr_view = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(lyr_view), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(lyr_view), GTK_WRAP_WORD);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(lyr_view), FALSE);
	gtk_widget_set_can_focus(lyr_view, FALSE);
	
	int just_type = 0;
	int align = deadbeef->conf_get_int(CONF_LYRICS_ALIGNMENT, 1);
	
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
	gtk_text_view_set_justification(GTK_TEXT_VIEW(lyr_view), just_type);

	GtkWidget *bio_view = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(bio_view), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(bio_view), GTK_WRAP_WORD);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(bio_view), FALSE);
	gtk_widget_set_can_focus(bio_view, FALSE);
	
	lyr_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(lyr_view));
	gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(lyr_buffer), "bold", "weight", PANGO_WEIGHT_BOLD, NULL);
	gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(lyr_buffer), "large", "scale", PANGO_SCALE_LARGE, NULL);
	gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(lyr_buffer), "italic", "style", PANGO_STYLE_ITALIC, NULL);
	
	bio_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(bio_view));
	bio_tab = gtk_vpaned_new();
	
	GtkWidget *img_frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(img_frame), GTK_SHADOW_IN);
	
	bio_image = gtk_drawing_area_new();
	gtk_widget_set_app_paintable(bio_image, TRUE);
	g_signal_connect(bio_image, "expose-event", G_CALLBACK(bio_image_expose), NULL);
	
	GtkWidget *dlt_btn = gtk_button_new();
	gtk_widget_set_tooltip_text(dlt_btn, "Remove current cache files");
	g_signal_connect(dlt_btn, "clicked", G_CALLBACK(delete_cache_clicked), NULL);
	
	GtkWidget *dlt_img = gtk_image_new_from_stock(GTK_STOCK_REFRESH, GTK_ICON_SIZE_SMALL_TOOLBAR); 
	gtk_button_set_image(GTK_BUTTON(dlt_btn), dlt_img);

	gtk_container_add(GTK_CONTAINER(lyr_tab), lyr_view);
	gtk_container_add(GTK_CONTAINER(bio_scroll), bio_view);
	gtk_container_add(GTK_CONTAINER(img_frame), bio_image);

	gtk_paned_pack1(GTK_PANED(bio_tab), img_frame, FALSE, TRUE);
	gtk_paned_pack2(GTK_PANED(bio_tab), bio_scroll, TRUE, TRUE);

	gtk_box_pack_start(GTK_BOX(infobar_toggles), lyr_toggle, FALSE, FALSE, 1);
	gtk_box_pack_start(GTK_BOX(infobar_toggles), bio_toggle, FALSE, FALSE, 1);
	gtk_box_pack_start(GTK_BOX(infobar_toggles), dlt_btn, FALSE, FALSE, 1);

	gtk_notebook_append_page(GTK_NOTEBOOK(infobar_tabs), lyr_tab, NULL);
	gtk_notebook_append_page(GTK_NOTEBOOK(infobar_tabs), bio_tab, NULL);

	g_signal_connect(lyr_toggle, "toggled", G_CALLBACK(infobar_tab_changed), lyr_tab);
	g_signal_connect(bio_toggle, "toggled", G_CALLBACK(infobar_tab_changed), bio_tab);

	gtk_box_pack_start(GTK_BOX(infobar), infobar_toggles, FALSE, TRUE, 1);
	gtk_box_pack_start(GTK_BOX(infobar), infobar_tabs, TRUE, TRUE, 1);
	
	gint handle_width;
	gtk_widget_style_get(bio_tab, "handle-size", &handle_width, NULL);
	
	int height = deadbeef->conf_get_int(CONF_BIO_IMAGE_HEIGHT, 200);
	gtk_widget_set_size_request(img_frame, -1, height + handle_width);

	gtk_widget_show_all(infobar);
}

static void
create_infobar_interface(void) {
	trace("infobar: modifying player's inteface\n");
	
	gdk_threads_enter();

	create_infobar();

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
	
	gint handle_width;
	gtk_widget_style_get(bio_tab, "handle-size", &handle_width, NULL);
	
	int width = deadbeef->conf_get_int(CONF_INFOBAR_WIDTH, 250);
	gtk_widget_set_size_request(infobar, width + handle_width, -1);
	
	gtk_widget_show(ddb_main_new);
	gtk_widget_show(playlist_box);

	gboolean state = deadbeef->conf_get_int(CONF_INFOBAR_VISIBLE, 0);
	state ? gtk_widget_show(infobar) : gtk_widget_hide(infobar); 

	gdk_threads_leave();
}

static void
create_infobar_menu_entry(void) {
    trace("infobar: creating infobar menu entry\n");
	
	gdk_threads_enter();
	
    GtkWidget *menu_item = gtk_check_menu_item_new_with_mnemonic ("_Infobar");
    GtkWidget *view_menu = lookup_widget(gtkui_plugin->get_mainwin(), "View_menu");

    gtk_container_add(GTK_CONTAINER(view_menu), menu_item);

    gboolean state = deadbeef->conf_get_int(CONF_INFOBAR_VISIBLE, 0);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), state);

    g_signal_connect(menu_item, "activate", G_CALLBACK(infobar_menu_toggle), NULL);
    gtk_widget_show(menu_item);

	gdk_threads_leave();
}

static int
uri_encode(char *out, int outl, const char *str, char space) {
    int l = outl;

    while (*str) {
        if (outl <= 1)
            return -1;

        if (!(
            (*str >= '0' && *str <= '9') ||
            (*str >= 'a' && *str <= 'z') ||
            (*str >= 'A' && *str <= 'Z') ||
            (*str == ' ') ||
            (*str == '\'') ||
            (*str == '/')
        ))
        {
            if (outl <= 3)
                return -1;

            snprintf (out, outl, "%%%02x", (uint8_t)*str);
            outl -= 3; str++; out += 3;
        }
        else {
			*out = *str == ' ' ? space : *str;
            out++; str++; outl--;
        }
    }
    *out = 0;
    return l - outl;
}

static int
is_dir(const char *dir, mode_t mode)
{
	int res = -1;
	struct stat st;
    
    char *tmp = strdup(dir);
    char *slash = tmp;
    
    do {
        slash = strstr(slash + 1, "/");
        if(slash) {
			*slash = 0;
        }
        res = stat(tmp, &st);
        if(res == -1) {
            res = mkdir(tmp, mode);
            if(res != 0) {
                trace("infobar: failed to create %s\n", tmp);
                free(tmp);
                return -1;
            }
        }
        if(slash) {
			*slash = '/';
		}
    } while(slash);

    free(tmp);
    return 0;
}

static gboolean
is_exists(const char *obj) {
	struct stat st;
	
	if(stat(obj, &st) != 0) {
		return FALSE;
	}
	return TRUE;
}

static gboolean
is_old_cache(const char *cache_file, CacheType type) {
	int res = -1;
	int uperiod = 0;
	time_t tm = time(NULL);

	struct stat st;
	res = stat(cache_file, &st);
	if(res == 0) {
		switch(type) {
		case LYRICS:
			uperiod = deadbeef->conf_get_int(CONF_LYRICS_UPDATE_PERIOD, 0);
			break;
		case BIO:
			uperiod = deadbeef->conf_get_int(CONF_BIO_UPDATE_PERIOD, 24);
			break;
		}
		
		if(uperiod == 0) {
			return FALSE;
		}
		
		if(uperiod > 0 && tm - st.st_mtime > uperiod * 60 * 60) {
			return TRUE;
		} else {
			return FALSE;
		}
	}
	return TRUE;
}

static char*
convert_to_utf8(const char *buf, int len) {
	int res = -1;

	const char *buf_cs = deadbeef->junk_detect_charset(buf);
	if(!buf_cs) {
		trace("infobar: failed to get cur encoding\n");
		return NULL;
	}
	
	char *buf_cnv = calloc(len * 4, sizeof(char));
	if(!buf_cnv) {
		return NULL;
	}
	
	res = deadbeef->junk_iconv(buf, len, buf_cnv, len * 4, buf_cs, "utf-8");
	if(res < 0) {
		trace("infobar: failed to convert to utf-8\n");
		free(buf_cnv);
		return NULL;
	}
	return buf_cnv;
}

static char*
load_content(const char *cache_file) {
	int res = -1;

	FILE *in_file = fopen(cache_file, "r");
	if(!in_file) {
		trace("infobar: failed to open %s\n", cache_file);
		return NULL;
	}

	res = fseek(in_file, 0, SEEK_END);
	if(res != 0) {
		trace("infobar: failed to seek %s\n", cache_file);
		fclose(in_file);
		return NULL;
	}
	
	int size = ftell(in_file);
	rewind(in_file);

	char *cnt = calloc(size + 1, sizeof(char));
	if(cnt) {
		res = fread(cnt, 1, size, in_file);
		if(res != size) {
			trace("infobar: failed to read %s\n", cache_file);
		}
	}
	fclose(in_file);
	return cnt;
}

static int
save_content(const char *cache_file, const char *buf, int size) {
	int res = -1;

	FILE *out_file = fopen(cache_file, "w+");
	if(!out_file) {
		trace("infobar: failed to open %s\n", cache_file);
		return -1;
	}

	res = fwrite(buf, 1, size, out_file);
	if(res <= 0) {
		trace("infobar: failed to write to %s\n", cache_file);
		fclose(out_file);
		return -1;
	}
	fclose(out_file);
	return 0;
}

static void
parser_errors_handler(void *ctx, const char *msg, ...) {}

static char*
parse_content(const char *cnt, int size, const char *pattern, ContentType type, int nnum) {
	char *pcnt = NULL;
	xmlDocPtr doc = NULL;

	xmlSetGenericErrorFunc(NULL, parser_errors_handler);
	
	switch(type) {
	case HTML:
		doc = htmlReadMemory(cnt, size, NULL, "utf-8", (HTML_PARSE_RECOVER | HTML_PARSE_NONET));
		break;
	case XML:
		doc = xmlReadMemory(cnt, size, NULL, "utf-8", (XML_PARSE_RECOVER | XML_PARSE_NONET));
		break;
	}
	xmlSetGenericErrorFunc(NULL, NULL);

	xmlNodePtr node = NULL;
	xmlXPathObjectPtr obj = NULL;
	xmlXPathContextPtr ctx = NULL;

	if(doc) {
		ctx = xmlXPathNewContext(doc);
		if(!ctx) {
			goto cleanup;
		}
		
		obj = xmlXPathEvalExpression((xmlChar*)pattern, ctx);
		if(!obj || !obj->nodesetval->nodeMax) {
			goto cleanup;
		}
		
		node = obj->nodesetval->nodeTab[nnum];
		if(node) {
			pcnt = (char*)xmlNodeGetContent(node);
		}
	}
	
cleanup:
	if(obj) {
		xmlXPathFreeObject(obj);
	}
	if(ctx) {
		xmlXPathFreeContext(ctx);
	}
	if(doc) {
		xmlFreeDoc(doc);
	}
	return pcnt;
}

static char*
retrieve_txt_content(const char *url, int size) {
	int res = -1;

    infobar_cnt = deadbeef->fopen(url);
    if(!infobar_cnt) {
    	trace("infobar: failed to open %s\n", url);
    	return NULL;
    }

    char *txt = calloc(size + 1, sizeof(char));
    if(txt && infobar_cnt) {
		res = deadbeef->fread(txt, 1, size, infobar_cnt);
		if(res <= 0) {
			trace("infobar: failed to retrieve a content from %s\n", url);
		}
	}
	if(infobar_cnt) {
		deadbeef->fclose(infobar_cnt);
		infobar_cnt = NULL;
	}
    return txt;
}

static int
retrieve_img_content(const char *url, const char *img) {
	infobar_cnt = deadbeef->fopen(url);
	if(!infobar_cnt) {
		trace("infobar: failed to open %s\n", url);
		return -1;
	}

	FILE *out_file = fopen(img, "wb+");
	if(!out_file) {
		trace("infobar: failed to open %s", img);
		if(infobar_cnt) {
			deadbeef->fclose(infobar_cnt);
			infobar_cnt = NULL;
		}
		return -1;
	}

	int len = 0;
	int err = 0;
	char tmp[4096] = {0};

	if(infobar_cnt) {
		while((len = deadbeef->fread(tmp, 1, sizeof(tmp), infobar_cnt)) > 0) {
			if(fwrite(tmp, 1, len, out_file) != len) {
				trace ("infobar: failed to write to %s\n", img);
				err = 1;
				break;
			}
		}
	}
	fclose(out_file);
	
	if(infobar_cnt) {
		deadbeef->fclose(infobar_cnt);
		infobar_cnt = NULL;
	}
	return err ? -1 : 0;
}

static void
retrieve_artist_bio(void) {
	trace("infobar: retrieve artist bio started\n");

	int res = -1;
	
	int cnt_len = 0;
	int bio_len = 0;
	int img_len = 0;

	char *cnt = NULL;
	char *bio = NULL;
	char *img = NULL;
	char *img_url = NULL;
	
	BioViewData *data = malloc(sizeof(BioViewData));
	if(!data) {
		goto cleanup;
	}
	
	deadbeef->mutex_lock(infobar_mutex);
	
	if(!artist_changed) {
		trace("infobar: artist hasn't changed\n");
		goto cleanup;
	}

	char cache_path[512] = {0};
	res = get_cache_path(cache_path, sizeof(cache_path), BIO);
	if(res == 0) {
		trace("infobar: failed to get bio cache dir\n");
		goto cleanup;
	}

	if(!is_exists(cache_path)) {
		res = is_dir(cache_path, 0755);
		if(res < 0) {
			trace("infobar: failed to create %s\n", cache_path);
			goto cleanup;
		}
	}

	char cache_file[512] = {0};
	res = snprintf(cache_file, sizeof(cache_file), "%s/%s", cache_path, artist);
	if(res == 0) {
		trace("infobar: failed to form a path to the bio cache file\n");
		goto cleanup;
	}
	
	char eartist[300] = {0};
	res = uri_encode(eartist, sizeof(eartist), artist, '+');
	if(res == -1) {
		trace("infobar: failed to encode %s\n", artist);
		goto cleanup;
	}

	char locale[5] = {0};
	deadbeef->conf_get_str(CONF_BIO_LOCALE, "en", locale, sizeof(locale));
	
	char track_url[512] = {0};
	res = snprintf(track_url, sizeof(track_url), "http://ws.audioscrobbler.com/2.0/?method=artist.getinfo&artist=%s&lang=%s&api_key=b25b959554ed76058ac220b7b2e0a026",
			eartist, locale);
	if(res == 0) {
		trace("infobar: failed to form a bio download url\n");
		goto cleanup;
	}

	if(!is_exists(cache_file) ||
		is_old_cache(cache_file, BIO)) {

		cnt = retrieve_txt_content(track_url, TXT_MAX);
		if(!cnt) {
			trace("infobar: failed to download %s\n", track_url);
			goto cleanup;
		}

		cnt_len = strlen(cnt);
		bio = parse_content(cnt, cnt_len, "/lfm/artist/bio/content", XML, 0);
		if(bio) {
			bio_len = strlen(bio);
			
			char *tmp = NULL;
			tmp = parse_content(bio, bio_len, "/html/body", HTML, 0);
			if(tmp) {
				free(bio);
				bio = tmp;
				bio_len = strlen(bio);
			}
			
			if(deadbeef->junk_detect_charset(bio)) {
				tmp = convert_to_utf8(bio, bio_len);
				if(tmp) {
					free(bio);
					bio = tmp;
					bio_len = strlen(bio);
				}
			}
	
			res = save_content(cache_file, bio, bio_len);
			if(res < 0) {
				trace("infobar: failed to save %s\n", cache_file);
				goto cleanup;
			}
		}
	} else {
		bio = load_content(cache_file);
		if(bio) {
			bio_len = strlen(bio);
		}
	}
	
	res = asprintf(&img, "%s/%s_img", cache_path, artist);
	if(res == -1) {
		trace("infobar: failed to form a path to the bio image file\n");
		goto cleanup;
	}

	if(!is_exists(img) || 
		is_old_cache(img, BIO)) {
		if(!cnt) {
			cnt = retrieve_txt_content(track_url, TXT_MAX);
			if(!cnt) {
				trace("infobar: failed to download %s\n", track_url);
				goto cleanup;
			}
		}

		cnt_len = strlen(cnt);
		img_url = parse_content(cnt, cnt_len, "//image[@size=\"extralarge\"]", XML, 0);
		if(img_url) {
			img_len = strlen(img_url);
		}	

		if(img_url && img_len > 0) {
			res = retrieve_img_content(img_url, img);
			if(res < 0) {
				trace("infobar: failed to download %s\n", img_url);
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
		data->img = img;
		data->len = bio_len;
	}
	
	if(cnt) {
		free(cnt);
	}
	if(img_url) {
		free(img_url);
	}
	
	if(artist_changed) {
		g_idle_add((GSourceFunc)update_bio_view, data);
	}
}

static gboolean
is_redirect(const char *buf) {
	if(!buf) {
		return FALSE;
	}
	
	if(strstr(buf, "#REDIRECT") ||
	   strstr(buf, "#redirect")) 
	{
		return TRUE;
	}
	return FALSE;
}
		
static int 
get_redirect_info(const char *buf, char *artist, int alen, char *title, int tlen) {
	char *bp = strrchr(buf, '[');
	char *mp = strchr(buf, ':');
	char *ep = strchr(buf, ']');
	
	int bi = bp - buf + 1;
	int mi = mp - buf + 1;
	int ei = ep - buf + 1;
	
	if((mi - bi) > alen ||
	   (ei - mi) > tlen)
	{
		return -1;
	}
	memcpy(artist, buf + bi, (mi - bi) - 1);
	memcpy(title, buf + mi, (ei - mi) - 1);
	return 0;
}

static int
get_new_lines_count(const char *buf) {
	int nlnum = 0;
	
	while(*buf) {
		if(*buf == '\n' ||
		   *buf == '\r') 
		{
			++nlnum;
		} else {
			break;
		}
		++buf;
	}
	return nlnum;
}

static char*
cleanup_new_lines(const char *buf, int len, int nlnum) {
	char *cld_buf = calloc(len + 1, sizeof(char));
	if(cld_buf) {
		memcpy(cld_buf, buf + nlnum, len - nlnum + 1);
	}
	return cld_buf;
}

static char*
lyrics_concat(const char *buf1, const char *buf2, const char *sep) {	
	int len1 = strlen(buf1);
	int len2 = strlen(buf2);
	int slen = strlen(sep);
	
	char *new_buf = calloc(len1 + len2 + slen + 1, sizeof(char));
	if(new_buf) {
		strncpy(new_buf, buf1, len1);
		strncat(new_buf, sep, slen);
		strncat(new_buf, buf2, len2);
	}
	return new_buf;
}

static char*
fetch_lyrics_from(const char *url, const char *artist, const char *title, const char *pattern, ContentType type, char space) {
	int res = -1;
	int len = 0;
	
	char eartist[300] = {0};
	char etitle[300] = {0};
	
	if(uri_encode(eartist, sizeof(eartist), artist, space) == -1 ||
	   uri_encode(etitle, sizeof(etitle), title, space) == -1)
	{
		trace("infobar: failed to encode %s or %s", artist, title);
		return NULL;
	}

	char track_url[512] = {0};
	res = snprintf(track_url, sizeof(track_url), url, eartist, etitle);
	if(res == 0) {
		trace("infobar: failed to form lyrics download url\n");
		return NULL;
	}

	char *cnt = retrieve_txt_content(track_url, TXT_MAX);
	if(!cnt) {
		trace("infobar: failed to download %s\n", track_url);
		return NULL;
	}
	len = strlen(cnt);
	
	char *lyr = parse_content(cnt, len, pattern, type, 0);
	if(lyr) {
		if(deadbeef->junk_detect_charset(lyr)) {
			len = strlen(lyr);
			
			char *tmp = convert_to_utf8(lyr, len);
			if(tmp) {
				free(lyr);
				lyr = tmp;
			}
		}
	}
	free(cnt);
	return lyr;
}

static void
retrieve_track_lyrics(void) {
	trace("infobar: retrieve track lyrics started\n");

	int res = -1;
	int len = 0;
	
	char *lyr = NULL;
		
	LyricsViewData *data = malloc(sizeof(LyricsViewData));
	if(!data) {
		goto cleanup;
	}
	
	deadbeef->mutex_lock(infobar_mutex);

	char cache_path[512] = {0};
	res = get_cache_path(cache_path, sizeof(cache_path), LYRICS);
	if(res == 0) {
		trace("infobar: failed to get lyrics cache path\n");
		goto cleanup;
	}

	if(!is_exists(cache_path)) {
		res = is_dir(cache_path, 0755);
		if(res < 0) {
			trace("infobar: failed to create %s\n", cache_path);
			goto cleanup;
		}
	}

	char cache_file[512] = {0};
	res = snprintf(cache_file, sizeof(cache_file), "%s/%s-%s", cache_path, artist, title);
	if(res == 0) {
		trace("infobar: failed to form a path to the lyrics cache file\n");
		goto cleanup;
	}
	
	if(!is_exists(cache_file) ||
		is_old_cache(cache_file, LYRICS)) {
			
		gboolean wikia = deadbeef->conf_get_int(CONF_LYRICSWIKIA_ENABLED, 1);
		if(wikia && !lyr && len == 0) {
			
			lyr = fetch_lyrics_from("http://lyrics.wikia.com/api.php?action=query&prop=revisions&rvprop=content&format=xml&titles=%s:%s",
					artist, title, "//rev", XML, '_');
			if(lyr) {
				len = strlen(lyr);
			}
			if(is_redirect(lyr) && len > 0) {		
				char rartist[100] = {0};
				char rtitle[100] = {0};
				
				res = get_redirect_info(lyr, rartist, sizeof(rartist), rtitle, sizeof(rtitle));
				if(res == 0) {						
					free(lyr);			
					lyr = fetch_lyrics_from("http://lyrics.wikia.com/api.php?action=query&prop=revisions&rvprop=content&format=xml&titles=%s:%s",
							rartist, rtitle, "//rev", XML, '_');
					if(lyr) {
						len = strlen(lyr);
					}	
				}
			}
			
			if(lyr && len > 0) {
				char *tmp1 = parse_content(lyr, len, "//lyrics", HTML, 0);
				if(tmp1) {
					char *tmp2 = parse_content(lyr, len, "//lyrics", HTML, 1);
					if(tmp2) {
						free(lyr);
						lyr = lyrics_concat(tmp1, tmp2, "\n\n***************\n\n");
						if(lyr) {
							len = strlen(lyr);
						}
						free(tmp1);
						free(tmp2);
					} else {
						free(lyr);
						lyr = tmp1;
						len = strlen(lyr);
					}
				}
			}
		}

		gboolean mania = deadbeef->conf_get_int(CONF_LYRICSMANIA_ENABLED, 1);
		if(mania && !lyr && len == 0) {
			lyr = fetch_lyrics_from("http://www.lyricsmania.com/%s_lyrics_%s.html",
					title, artist, "//*[@id=\"songlyrics_h\"]", HTML, '_');
			if(lyr) {
				len = strlen(lyr);
			}
		}
	
		gboolean time = deadbeef->conf_get_int(CONF_LYRICSTIME_ENABLED, 1);
		if(time && !lyr && len == 0) {
			lyr = fetch_lyrics_from("http://www.lyricstime.com/%s-%s-lyrics.html",
					artist, title, "//*[@id=\"songlyrics\"]", HTML, '-');
			if(lyr) {
				len = strlen(lyr);
			}
		}
	
		gboolean mega = deadbeef->conf_get_int(CONF_MEGALYRICS_ENABLED, 1);
		if(mega && !lyr && len == 0) {
			lyr = fetch_lyrics_from("http://megalyrics.ru/lyric/%s/%s.htm",
					artist, title, "//pre[@class=\"lyric\"]", HTML, '_');
			if(lyr) {
				len = strlen(lyr);
			}
		}
		
		if(lyr && len > 0) {
			int nlnum = get_new_lines_count(lyr);
			if(nlnum > 0) {
				char *tmp = cleanup_new_lines(lyr, len, nlnum);
				if(tmp) {
					free(lyr);
					lyr = tmp;
					len = strlen(lyr);
				}
			}
		}
	
		if(lyr && len > 0) {
			res = save_content(cache_file, lyr, len);
			if(res < 0) {
				trace("infobar: failed to save %s\n", cache_file);
				goto cleanup;
			}
		}
	} else {
		lyr = load_content(cache_file);
		if(lyr) {
			len = strlen(lyr);
		}
	}
	
cleanup:
	if(infobar_mutex) {
		deadbeef->mutex_unlock(infobar_mutex);
	}
	
	if(data) {
		data->txt = lyr;
		data->len = len;
	}
	g_idle_add((GSourceFunc)update_lyrics_view, data);
}

static int 
get_track_info(DB_playItem_t *track, char *artist, int alen, char *title, int tlen) {
	deadbeef->pl_lock();

	const char *cur_artist = deadbeef->pl_find_meta(track, "artist");
	const char *cur_title =  deadbeef->pl_find_meta(track, "title");
	
	if(!cur_artist || !cur_title) {
		deadbeef->pl_unlock();
		return -1;
	}
	
	strncpy(artist, cur_artist, alen);
	strncpy(title, cur_title, tlen);

	deadbeef->pl_unlock();
	return 0;
}

static void
infobar_songstarted(ddb_event_track_t *ev) {
	trace("infobar: infobar song started\n");

	int res = -1;
	
	if(!ev->track)
		return;
	
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
	artist_changed = res == 0 ? FALSE : TRUE;
	
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

	create_infobar_interface();
	create_infobar_menu_entry();

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

	if(bio_pixbuf) {
		g_object_unref(bio_pixbuf);
		bio_pixbuf = NULL;
	}

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
	"property \"Lyrics alignment type\" entry infobar.lyrics.alignment 1;"
	"property \"Lyrics cache update period (hr)\" entry infobar.lyrics.cache.period 0;"
	"property \"Biography cache update period (hr)\" entry infobar.bio.cache.period 24;"
	"property \"Default image height (px)\" entry infobar.bio.image.height 250;"
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
