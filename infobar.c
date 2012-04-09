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

#include "infobar.h"

static DB_misc_t plugin;

static uintptr_t ifb_mutex;
static uintptr_t ifb_cond;
static intptr_t ifb_tid;

static gboolean artist_changed = TRUE;
static gboolean infobar_stopped = TRUE;

static void
retrieve_artist_bio() {
    
    trace("infobar: retrieving artist's biography\n");

    BioViewData *view = calloc(1, sizeof(BioViewData));
    if (!view)
        return;

    char *cache_path = NULL;
    if (get_cache_path(&cache_path, BIO) == -1)
        goto update;
        
    if (!is_exists(cache_path)) {
        if (create_dir(cache_path, 0755) == -1) {
            free(cache_path);
            goto update;
        }
    }
    
    char *txt_cache = NULL;
    if (asprintf(&txt_cache, "%s/%s", cache_path, artist) == -1) {
        free(cache_path);
        goto update;
    }
    
    char *url = NULL;
    if (form_bio_url(artist, &url) == -1) {
        free(cache_path);
        free(txt_cache);
        goto update;
    }
    
    char *bio_txt = NULL;
    
    if (!is_exists(txt_cache) || is_old_cache(txt_cache, BIO)) {
        /* There is no cache for artist's biography or it's too old,
         * retrieving new one. */
        if (fetch_bio_txt(url, &bio_txt) == 0) {
            view->txt = bio_txt;
            view->len = strlen(bio_txt);
            /* Saving biography to reuse it later. */
            save_txt_file(txt_cache, bio_txt);
        }
        
    } else {
        /* We got a cached biography, just loading it. */
        if (load_txt_file(txt_cache, &bio_txt) == 0) {
            view->txt = bio_txt;
            view->len = strlen(bio_txt);
        }
    }
    free(txt_cache);
    
    char *img_cache = NULL;
    if (asprintf(&img_cache, "%s/%s_img", cache_path, artist) == -1) {
        free(cache_path);
        free(url);
        goto update;
    }
    free(cache_path);
    
    /* Retrieving artist's image if we don't have a cached one. */
    if (!is_exists(img_cache) || is_old_cache(img_cache, BIO))
        fetch_bio_image(url, img_cache);
        
    view->img = img_cache;
    free(url);

update:
    g_idle_add((GSourceFunc) update_bio_view, view);
}

static void
retrieve_track_lyrics(void) {
    
    trace("infobar: retrieving track lyrics\n");

    LyricsViewData *view = calloc(1, sizeof(LyricsViewData));
    if (!view)
        return;

    char *cache_path = NULL;
    if (get_cache_path(&cache_path, LYRICS) == -1)
        goto update;

    if (!is_exists(cache_path)) {
        if (create_dir(cache_path, 0755) == -1) {
            free(cache_path);
            goto update;
        }
    }

    char *txt_cache = NULL;
    if (asprintf(&txt_cache, "%s/%s-%s", cache_path, artist, title) == -1) {
        free(cache_path);
        goto update;
    }
    free(cache_path);
    
    char *lyr_txt = NULL;
    
    if (!is_exists(txt_cache) || is_old_cache(txt_cache, LYRICS)) {
        /* There is no cache for the current track or the previous cache 
         * is too old, so start retrieving new one. */
        if (deadbeef->conf_get_int(CONF_LYRICSWIKIA_ENABLED, 1) && !lyr_txt)
            fetch_lyrics_from_lyricswikia(artist, title, &lyr_txt);

        if (deadbeef->conf_get_int(CONF_LYRICSMANIA_ENABLED, 1) && !lyr_txt)
            fetch_lyrics_from_lyricsmania(artist, title, &lyr_txt);
    
        if (deadbeef->conf_get_int(CONF_LYRICSTIME_ENABLED, 1) && !lyr_txt)
            fetch_lyrics_from_lyricstime(artist, title, &lyr_txt);
    
        if (deadbeef->conf_get_int(CONF_MEGALYRICS_ENABLED, 1) && !lyr_txt)
            fetch_lyrics_from_megalyrics(artist, title, &lyr_txt);
    
        if (lyr_txt) {
            
            char *lyr_wo_nl = NULL;
            /* Some lyrics contains new line characters at the beginning of the 
             * file, so we gonna strip them. */
            if (del_nl(lyr_txt, &lyr_wo_nl) == 0) {
                free(lyr_txt);
                lyr_txt = lyr_wo_nl;
            }
            view->txt = lyr_txt;
            view->len = strlen(lyr_txt);
            /* Saving lyrics to reuse it later.*/
            save_txt_file(txt_cache, lyr_txt);
        }
        
    } else {
        /* We got a cache for the current track, so just load it. */
        if (load_txt_file(txt_cache, &lyr_txt) == 0) {
            view->txt = lyr_txt;
            view->len = strlen(lyr_txt);
        }
    }
    free(txt_cache);
    
update:
    g_idle_add((GSourceFunc) update_lyrics_view, view);
}

static void
infobar_songstarted(ddb_event_track_t *ev) {
    
    trace("infobar: infobar song started\n");
    
    DB_playItem_t *pl_track = deadbeef->streamer_get_playing_track();
    if (!pl_track)
        return;
        
    if (ev->track != pl_track) {
        deadbeef->pl_item_unref(pl_track);
        return;
    } 
    deadbeef->pl_item_unref(pl_track);
        
    /* Don't retrieve anything as infobar is not visible. */
    if (!deadbeef->conf_get_int(CONF_INFOBAR_VISIBLE, 0))
        return;
        
    /* Don't retrieve anything as lyrics and biography tabs are invisible. */
    if (!deadbeef->conf_get_int(CONF_LYRICS_ENABLED, 1) &&
        !deadbeef->conf_get_int(CONF_BIO_ENABLED, 1)) {
        return;
    }
    
    deadbeef->mutex_lock(ifb_mutex);
    
    if (artist) {
        free(artist);
        artist = NULL;
    }
    if (title) {
        free(title);
        title = NULL;
    }
    if (get_track_info(ev->track, &artist, &title) == -1) {
        deadbeef->mutex_unlock(ifb_mutex);
        return;
    }
    
    if (old_artist && old_title) {
        
        int acmp = strcmp(old_artist, artist);
        int tcmp = strcmp(old_title, title);
        
        /* Same artist and title as before. */
        if (acmp == 0 && tcmp == 0) {
            deadbeef->mutex_unlock(ifb_mutex);
            return;
        } 
        artist_changed = acmp != 0;
    }
    
    if (old_artist) {
        free(old_artist);
        old_artist = NULL;
    }
    if (old_title) {
        free(old_title);
        old_title = NULL;
    }
    if (update_track_info(artist, title, &old_artist, &old_title) == -1){
        deadbeef->mutex_unlock(ifb_mutex);
        return;
    }
    
    deadbeef->mutex_unlock(ifb_mutex);
    deadbeef->cond_signal(ifb_cond);
}

static void
infobar_thread(void *ctx) {
    
    for (;;) {
        
        trace("infobar: infobar thread started\n");
    
        if (infobar_stopped) {
            deadbeef->mutex_unlock(ifb_mutex);
            return;
        }
        
        deadbeef->cond_wait(ifb_cond, ifb_mutex);
        if (infobar_stopped) {
            deadbeef->mutex_unlock(ifb_mutex);
            return;
        }
        deadbeef->mutex_unlock(ifb_mutex);
        
        if (deadbeef->conf_get_int(CONF_LYRICS_ENABLED, 1)) {
            trace("infobar: retrieving song's lyrics...\n");
            retrieve_track_lyrics();
        }

        if (deadbeef->conf_get_int(CONF_BIO_ENABLED, 1)) {
            trace("infobar: retrieving artist's biography...\n");
            /* Don't retrieve biography again, if we still playing 
             * the same artist.*/
            if (artist_changed)
                retrieve_artist_bio();
        }
    }
}

static int
infobar_message(uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2) {
    
    switch (id) {
    case DB_EV_SONGSTARTED:
    {
        trace("infobar: recieved songstarted message\n");
        ddb_event_track_t* event = (ddb_event_track_t*) ctx;
        
        if (!event->track) 
            return 0;
            
        if (!is_stream(event->track))
            infobar_songstarted(event);
    }
        break;
    case DB_EV_TRACKINFOCHANGED:
    {
        trace("infobar: recieved trackinfochanged message\n");
        ddb_event_track_t* event = (ddb_event_track_t*) ctx;
        
        if (!event->track) 
            return 0;
            
        if (is_stream(event->track))
            infobar_songstarted(event);
    }
        break;
    case DB_EV_CONFIGCHANGED:
        g_idle_add((GSourceFunc) infobar_config_changed, NULL);
        break;
    }
    return 0;
}

static gboolean
infobar_init(void) {
    
    trace("infobar: initializing plug-in's ui\n");

    create_infobar_interface();
    attach_infobar_menu_entry();
    infobar_config_changed();
    
    return FALSE;
}

static int
infobar_connect(void) {
    
    trace("infobar: connecting the plug-in\n");

    ddb_gtkui_t* gtkui_plugin = (ddb_gtkui_t*) deadbeef->plug_get_for_id("gtkui");
    if (!gtkui_plugin) {
        return -1;
    }    
    init_ui_plugin(gtkui_plugin);
    g_idle_add((GSourceFunc)infobar_init, NULL);
    
    return 0;
}

static int
infobar_disconnect(void) {
    
    trace("infobar: disconnecting the plug-in\n");
    free_ui_plugin();
    return 0;
}

static int
infobar_start(void) {
    
    trace("infobar: starting the plug-in\n");
    infobar_stopped = FALSE;
    
    ifb_cond = deadbeef->cond_create();
    ifb_mutex = deadbeef->mutex_create_nonrecursive();
    ifb_tid = deadbeef->thread_start(infobar_thread, NULL);
    
    return 0;
}

static int
infobar_stop(void) {
    
    trace("infobar: stopping the plug-in\n");

    infobar_stopped = TRUE;
    free_bio_pixbuf();
    
    if (artist) 
        free(artist);
    
    if (title) 
        free(title);
        
    if (old_artist) 
        free(old_artist);
    
    if (old_title) 
        free(old_title);

    if (ifb_tid) {
        deadbeef->cond_signal(ifb_cond);
        deadbeef->thread_join(ifb_tid);
    }

    if (ifb_mutex) {
        deadbeef->mutex_unlock(ifb_mutex);
        deadbeef->mutex_free(ifb_mutex);
    }

    if (ifb_cond)
        deadbeef->cond_free(ifb_cond);
        
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
        "Copyright (C) 2011-2012 Dmitriy Simbiriatin <dmitriy.simbiriatin@gmail.com>\n"
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
    .plugin.website = "https://bitbucket.org/dsimbiriatin/deadbeef-infobar",
    .plugin.start = infobar_start,
    .plugin.stop = infobar_stop,
    .plugin.connect    = infobar_connect,
    .plugin.disconnect = infobar_disconnect,
    .plugin.configdialog = settings_dlg,
    .plugin.message = infobar_message,
};

DB_plugin_t *ddb_infobar_load(DB_functions_t *ddb) {
    
    deadbeef = ddb;
    return DB_PLUGIN(&plugin);
}
