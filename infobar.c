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

#include "infobar.h"

//#define trace(fmt,...)

static DB_misc_t plugin;

#define TXT_MAX 100000

DB_FILE *infobar_cnt;

gboolean artist_changed = TRUE;

static uintptr_t infobar_mutex;
static uintptr_t infobar_cond;
static intptr_t infobar_tid;
static gboolean infobar_stopped;

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
	attach_infobar_menu_entry();

	infobar_config_changed();
	return FALSE;
}

static int
infobar_connect(void) {
	trace("infobar: connecting infobar plugin\n");

	ddb_gtkui_t* gtkui_plugin = (ddb_gtkui_t*) deadbeef->plug_get_for_id("gtkui");
	if(!gtkui_plugin) {
		return -1;
    }    
    init_ui_plugin(gtkui_plugin);
    
	g_idle_add((GSourceFunc)infobar_init, NULL);
	return 0;
}

static int
infobar_disconnect(void) {
	trace("infobar: disconnecting infobar plugin\n");
	free_ui_plugin();
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

	free_bio_pixbuf();

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
	"property \"Default image height (px)\" entry infobar.bio.image.height 200;"
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
