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

static DB_misc_t plugin;

static uintptr_t infobar_mutex;
static uintptr_t infobar_cond;
static intptr_t infobar_tid;

static gboolean artist_changed = TRUE;
static gboolean infobar_stopped = TRUE;

static void
retrieve_artist_bio(BioViewData **view) {
    
    trace("infobar: started retrieving of artist's biography\n");

    char *cache_path = NULL;
    if (get_cache_path(&cache_path, BIO) == -1)
        return;
        
    if (!is_exists(cache_path)) {
        if (create_dir(cache_path, 0755) == -1) {
            free(cache_path);
            return;
        }
    }
    
    char *txt_cache = NULL;
    if (asprintf(&txt_cache, "%s/%s", cache_path, artist) == -1) {
        free(cache_path);
        return;
    }
    
    char *url = NULL;
    if (form_bio_url(artist, &url) == -1) {
        free(cache_path);
        free(txt_cache);
        return;
    }
    
    *view = calloc(1, sizeof(BioViewData));
    if (!*view) {
        free(cache_path);
        free(txt_cache);
        free(url);
        return;
    }
    
    char *bio_txt = NULL;
    
    if (!is_exists(txt_cache) || is_old_cache(txt_cache, BIO)) {
        
        if (fetch_bio_txt(url, &bio_txt) == 0) {
            (*view)->txt = bio_txt;
            (*view)->len = strlen(bio_txt);
            save_txt_file(txt_cache, bio_txt);
        }
    } else {
        
        if (load_txt_file(txt_cache, &bio_txt) == 0) {
            (*view)->txt = bio_txt;
            (*view)->len = strlen(bio_txt);
        }
    }
    free(txt_cache);
    
    char *img_cache = NULL;
    if (asprintf(&img_cache, "%s/%s_img", cache_path, artist) == -1) {
        free(cache_path);
        free(url);
        return;
    }
    free(cache_path);
    
    if (!is_exists(img_cache) || is_old_cache(img_cache, BIO))
        fetch_bio_image(url, img_cache);
        
    (*view)->img = img_cache;
    free(url);
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

static void
retrieve_track_lyrics(LyricsViewData **view) {
    
    trace("infobar: retrieve track lyrics started\n");

    char *cache_path = NULL;
    if (get_cache_path(&cache_path, LYRICS) == -1)
        return;

    if (!is_exists(cache_path)) {
        if (create_dir(cache_path, 0755) == -1) {
            free(cache_path);
            return;
        }
    }

    char *txt_cache = NULL;
    if (asprintf(&txt_cache, "%s/%s-%s", cache_path, artist, title) == -1) {
        free(cache_path);
        return;
    }
    free(cache_path);
    
    *view = calloc(1, sizeof(LyricsViewData));
    if (!*view) {
        free(txt_cache);
        return;
    }
    
    char *lyr_txt = NULL;
    
    if (!is_exists(txt_cache) || is_old_cache(txt_cache, LYRICS)) {
            
        if (deadbeef->conf_get_int(CONF_LYRICSWIKIA_ENABLED, 1) && !lyr_txt)
            fetch_lyrics_from_lyricswikia(artist, title, &lyr_txt);

        if (deadbeef->conf_get_int(CONF_LYRICSMANIA_ENABLED, 1) && !lyr_txt)
            fetch_lyrics_from_lyricsmania(artist, title, &lyr_txt);
    
        if (deadbeef->conf_get_int(CONF_LYRICSTIME_ENABLED, 1) && !lyr_txt)
            fetch_lyrics_from_lyricstime(artist, title, &lyr_txt);
    
        if (deadbeef->conf_get_int(CONF_MEGALYRICS_ENABLED, 1) && !lyr_txt)
            fetch_lyrics_from_megalyrics(artist, title, &lyr_txt);
        
        /*if(lyr && len > 0) {
            int nlnum = get_new_lines_count(lyr);
            if(nlnum > 0) {
                char *tmp = cleanup_new_lines(lyr, len, nlnum);
                if(tmp) {
                    free(lyr);
                    lyr = tmp;
                    len = strlen(lyr);
                }
            }
        }*/
    
        if (lyr_txt) {
            (*view)->txt = lyr_txt;
            (*view)->len = strlen(lyr_txt);
            save_txt_file(txt_cache, lyr_txt);
        }
    } else {
        if (load_txt_file(txt_cache, &lyr_txt) == 0) {
            (*view)->txt = lyr_txt;
            (*view)->len = strlen(lyr_txt);
        }
    }
    free(txt_cache);
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
        
    if (artist) {
        free(artist);
        artist = NULL;
    }
    if (title) {
        free(title);
        title = NULL;
    }
    
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
            
            deadbeef->mutex_lock(infobar_mutex);
            
            LyricsViewData *view = NULL;
            retrieve_track_lyrics(&view);
            g_idle_add((GSourceFunc) update_lyrics_view, view);

            deadbeef->mutex_unlock(infobar_mutex);
        }

        if(deadbeef->conf_get_int(CONF_BIO_ENABLED, 1)) {
            trace("infobar: retrieving artist's bio...\n");
            
            deadbeef->mutex_lock(infobar_mutex);
            
            if (artist_changed) {
                BioViewData *view = NULL;
                retrieve_artist_bio(&view);
                g_idle_add((GSourceFunc) update_bio_view, view);
            }
            deadbeef->mutex_unlock(infobar_mutex);
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
