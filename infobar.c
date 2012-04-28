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

static void
retrieve_artist_bio(void *ctx) {
    
    trace("infobar: retrieving artist's biography\n");
    DB_playItem_t *track = (DB_playItem_t*) ctx;
    
    char *bio_txt = NULL, *artist = NULL, *img_cache = NULL;
    
    if (!is_track_changed(track)) {
        
        gdk_threads_enter();
        update_bio_view("Loading...", NULL);
        gdk_threads_leave();
    
        if (get_artist_info(track, &artist) == -1)
            goto update;
    
        char *txt_cache = NULL;
        if (create_bio_cache(artist, &txt_cache, &img_cache) == -1) {
            free(artist);
            goto update;
        }
    
        if (!is_exists(txt_cache) || is_old_cache(txt_cache, BIO)) {
            /* There is no cache for artist's biography or it's 
            * too old, retrieving new one. */
            if (fetch_bio_txt(artist, &bio_txt) == 0) {
                /* Saving biography to reuse it later. */
                save_txt_file(txt_cache, bio_txt);
            }
        } else {
            /* We got a cached biography, just loading it. */
            load_txt_file(txt_cache, &bio_txt);
        }
        free(txt_cache);
    
        /* Retrieving artist's image if we don't have a cached one. */
        if (!is_exists(img_cache) || is_old_cache(img_cache, BIO))
            fetch_bio_image(artist, img_cache);
        
        free(artist);
    }
    
update:
    if (!is_track_changed(track)) {
        gdk_threads_enter();
        update_bio_view(bio_txt, img_cache);
        gdk_threads_leave();
    }
    if (bio_txt) 
        free(bio_txt);
    
    if (img_cache) 
        free(img_cache);
}

static void
retrieve_track_lyrics(void *ctx) {
    
    trace("infobar: retrieving track lyrics\n");
    DB_playItem_t *track = (DB_playItem_t*) ctx;
    
    char *lyr_txt = NULL, *artist = NULL, *title = NULL;
    
    if (!is_track_changed(track)) {
        
        gdk_threads_enter();
        update_lyrics_view("Loading...", track);
        gdk_threads_leave();
        
        if (get_artist_and_title_info(track, &artist, &title) == -1)
            goto update;

        char *txt_cache = NULL;
        if (create_lyr_cache(artist, title, &txt_cache) == -1) {
            free(artist);
            free(title);
            goto update;
        }
    
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
            
            if (deadbeef->conf_get_int(CONF_LYRICS_SCRIPT_ENABLED, 0) && !lyr_txt)
                fetch_lyrics_from_script(artist, title, &lyr_txt);
    
            if (lyr_txt) {
                char *lyr_wo_nl = NULL;
                /* Some lyrics contains new line characters at the 
                * beginning of the file, so we gonna strip them. */
                if (del_nl(lyr_txt, &lyr_wo_nl) == 0) {
                    free(lyr_txt);
                    lyr_txt = lyr_wo_nl;
                }
                /* Saving lyrics to reuse it later.*/
                save_txt_file(txt_cache, lyr_txt);
            }
        } else {
            /* We got a cache for the current track, so just loading it. */
            load_txt_file(txt_cache, &lyr_txt);
        }
        free(txt_cache);
        free(artist);
        free(title);
    }
    
update:
    if (!is_track_changed(track)) {
        gdk_threads_enter();
        update_lyrics_view(lyr_txt, track);
        gdk_threads_leave();
    }
    if (lyr_txt) 
        free(lyr_txt);
}

static void
infobar_songstarted(ddb_event_track_t *ev) {
    
    trace("infobar: infobar song started\n");
    
    /* Don't retrieve anything as infobar is not visible. */
    if (!deadbeef->conf_get_int(CONF_INFOBAR_VISIBLE, 0))
        return;
        
    /* Don't retrieve anything as lyrics and biography tabs are invisible. */
    if (!deadbeef->conf_get_int(CONF_LYRICS_ENABLED, 1) &&
        !deadbeef->conf_get_int(CONF_BIO_ENABLED, 1)) {
        return;
    }
    if (deadbeef->conf_get_int(CONF_LYRICS_ENABLED, 1)) {
        intptr_t tid = deadbeef->thread_start(retrieve_track_lyrics, ev->track);
        deadbeef->thread_detach(tid);
    }
    if (deadbeef->conf_get_int(CONF_BIO_ENABLED, 1)) {
        intptr_t tid = deadbeef->thread_start(retrieve_artist_bio, ev->track);
        deadbeef->thread_detach(tid);
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
        gdk_threads_enter();
        infobar_config_changed();
        gdk_threads_leave();
        break;
    }
    return 0;
}

static int
infobar_connect(void) {
    
    trace("infobar: connecting the plug-in\n");
    
    gdk_threads_enter();
    int res = init_ui_plugin();
    gdk_threads_leave();
    
    return res;
}

static int
infobar_disconnect(void) {
    
    trace("infobar: disconnecting the plug-in\n");
    free_ui_plugin();
    return 0;
}

static const char settings_dlg[] =
    "property \"Enable lyrics\" checkbox infobar.lyrics.enabled 1;"
    "property \"Fetch from Lyricswikia\" checkbox infobar.lyrics.lyricswikia 1;"
    "property \"Fetch from Lyricsmania\" checkbox infobar.lyrics.lyricsmania 1;"
    "property \"Fetch from Lyricstime\" checkbox infobar.lyrics.lyricstime 1;"
    "property \"Fetch from Megalyrics\" checkbox infobar.lyrics.megalyrics 1;"
    "property \"Fetch from script\" checkbox infobar.lyrics.script 0;"
    "property \"Lyrics script path\" file infobar.lyrics.script.path \"\";"
    "property \"Lyrics alignment type\" select[3] infobar.lyrics.alignment 0 left center right;"
    "property \"Lyrics cache update period (hr)\" spinbtn[0,99,1] infobar.lyrics.cache.period 0;"
    "property \"Enable biography\" checkbox infobar.bio.enabled 1;"
    "property \"Biography locale\" entry infobar.bio.locale \"en\";"
    "property \"Biography cache update period (hr)\" spinbtn[0,99,1] infobar.bio.cache.period 24;"
;

static DB_misc_t plugin = {
    
    .plugin.api_vmajor = 1,
    .plugin.api_vminor = 0,
    .plugin.version_major = 1,
    .plugin.version_minor = 2,
    .plugin.type = DB_PLUGIN_MISC,
    .plugin.name = "Infobar",
    .plugin.descr = "Fetches and shows song's lyrics and artist's biography.\n\n"
                    "To change the biography's locale, set an appropriate ISO 639-2 locale code.\n"
                    "See http://en.wikipedia.org/wiki/List_of_ISO_639-2_codes for more infomation.\n\n"
                    "You can set cache update period to 0 if you don't want to update the cache at all.\n\n"
                    "To use the custom lyrics script, enable \"Fetch from script\" option and specify full\n"
                    "path to the script in \"Lyrics script path\" field.",
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
    .plugin.connect = infobar_connect,
    .plugin.disconnect = infobar_disconnect,
    .plugin.configdialog = settings_dlg,
    .plugin.message = infobar_message,
};

DB_plugin_t *ddb_infobar_load(DB_functions_t *ddb) {
    
    deadbeef = ddb;
    return DB_PLUGIN(&plugin);
}
