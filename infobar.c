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

gboolean artist_changed = TRUE;

static uintptr_t infobar_mutex;
static uintptr_t infobar_cond;
static intptr_t infobar_tid;
static gboolean infobar_stopped;

static void
retrieve_artist_bio(void) {
    trace("infobar: retrieve artist bio started\n");

    int res = -1;

    char *cnt = NULL;
    char *bio = NULL;
    char *img = NULL;
    
    char *track_url = NULL;
 
    BioViewData *data = malloc(sizeof(BioViewData));
    if(!data) {
        goto cleanup;
    }
    
    deadbeef->mutex_lock(infobar_mutex);
    
    if(!artist_changed) {
        trace("infobar: artist hasn't changed\n");
        goto cleanup;
    }

    char *cache_path = NULL;
    if (get_cache_path(&cache_path, BIO) == -1) {
        trace("infobar: failed to get bio cache dir\n");
        goto cleanup;
    }

    if(!is_exists(cache_path)) {
        res = create_dir(cache_path, 0755);
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
    
    if (form_bio_url(artist, &track_url) == -1) {
        goto cleanup;
    }

    if(!is_exists(cache_file) || is_old_cache(cache_file, BIO)) {
        
        if (fetch_bio_txt(track_url, &bio) == -1)
            goto cleanup;
    
        if (save_txt_file(cache_file, bio) < 0) {
            trace("infobar: failed to save %s\n", cache_file);
            goto cleanup;
        }
    } else {
        if (load_txt_file(cache_file, &bio) == 0) {
        }
    }
    
    res = asprintf(&img, "%s/%s_img", cache_path, artist);
    if(res == -1) {
        trace("infobar: failed to form a path to the bio image file\n");
        goto cleanup;
    }

    if (!is_exists(img) || is_old_cache(img, BIO)) {
        
        if (!cnt) {
            if (retrieve_txt_content(track_url, &cnt) != 0) {
                trace("infobar: failed to download %s\n", track_url);
                goto cleanup;
            }
        }
    
        char *img_url = NULL;
        if (parse_content(cnt, "//image[@size=\"extralarge\"]", &img_url, XML, 0) == 0) {
            if (retrieve_img_content(img_url, img) < 0) {
                free(img_url);
                goto cleanup;
            }
        }
        free(img_url);
    }

cleanup:
    if(infobar_mutex) {
        deadbeef->mutex_unlock(infobar_mutex);
    }
    
    if(data) {
        data->txt = bio;
        data->img = img;
        data->len = bio ? strlen(bio) : 0;
    }
    
    if (track_url) {
        free(track_url);
    }
    
    if(cnt) {
        free(cnt);
    }
    
    if(artist_changed) {
        g_idle_add((GSourceFunc)update_bio_view, data);
    }
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

    char *cnt = NULL;
    if (retrieve_txt_content(track_url, &cnt) != 0) {
        trace("infobar: failed to download %s\n", track_url);
        return NULL;
    }
    
    char *lyr = NULL;
    if (parse_content(cnt, pattern, &lyr, type, 0) == 0) {
        if(deadbeef->junk_detect_charset(lyr)) {
            
            char *tmp = NULL;
            if (convert_to_utf8(lyr, &tmp) == 0) {
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

    char *cache_path = NULL;
    if (get_cache_path(&cache_path, LYRICS) == -1) {
        trace("infobar: failed to get lyrics cache path\n");
        goto cleanup;
    }

    if(!is_exists(cache_path)) {
        res = create_dir(cache_path, 0755);
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
                char *rartist = NULL;
                char *rtitle = NULL;
                
                res = get_redirect_info(lyr, &rartist, &rtitle);
                if(res == 0) {                        
                    free(lyr);            
                    lyr = fetch_lyrics_from("http://lyrics.wikia.com/api.php?action=query&prop=revisions&rvprop=content&format=xml&titles=%s:%s",
                            rartist, rtitle, "//rev", XML, '_');
                    if(lyr) {
                        len = strlen(lyr);
                    }    
                }
                free(rartist);
                free(rtitle);
            }
            
            if(lyr && len > 0) {
                char *tmp1 = NULL;
                if (parse_content(lyr, "//lyrics", &tmp1, HTML, 0) == 0) {
                    char *tmp2 = NULL;
                    if (parse_content(lyr, "//lyrics", &tmp2, HTML, 1) == 0) {
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
            res = save_txt_file(cache_file, lyr);
            if(res < 0) {
                trace("infobar: failed to save %s\n", cache_file);
                goto cleanup;
            }
        }
    } else {
        if (load_txt_file(cache_file, &lyr) == 0) {
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
        
    if (artist) free(artist);
    if (title) free(title);
    
    if (get_track_info(ev->track, &artist, &title) == -1) {
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
    .plugin.connect    = infobar_connect,
    .plugin.disconnect = infobar_disconnect,
    .plugin.configdialog = settings_dlg,
    .plugin.message = infobar_message,
};

DB_plugin_t *ddb_infobar_load (DB_functions_t *ddb) {
    deadbeef = ddb;
    return DB_PLUGIN(&plugin);
}
