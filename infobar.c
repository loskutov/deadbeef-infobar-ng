/*
    Infobar plugin for DeaDBeeF music player
    Copyright (C) 2015 Ignat Loskutov <ignat.loskutov@gmail.com>
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

DB_functions_t *deadbeef;

ddb_gtkui_t *gtkui_plugin;

static int
similar_view_loading(void) {
    static const SimilarInfoList list = {
            .size = 1,
            .data = { { .name = "Loading..." } },
    };
    update_similar_view(&list);
    return G_SOURCE_REMOVE;
}

static int
lyrics_view_loading(DB_playItem_t *track) {
    update_lyrics_view("Loading...", track);
    return G_SOURCE_REMOVE;
}

static int
bio_view_loading(void) {
    update_bio_view("Loading...", NULL);
    return G_SOURCE_REMOVE;
}

static int
display_similar_artists(SimilarInfoList *similar) {
    update_similar_view(similar);
    free_sim_list(similar);
    return G_SOURCE_REMOVE;
}

typedef struct {
    char *text;
    void *data;
} Pair;

static int
display_bio(Pair *txt_img) {
    char *bio_txt = txt_img->text;
    char *img_cache = txt_img->data;
    update_bio_view(bio_txt, img_cache);
    free(bio_txt);
    free(img_cache);
    free(txt_img);
    return G_SOURCE_REMOVE;
}

static int
display_lyrics(Pair *txt_track) {
    char *lyr_txt = txt_track->text;
    DB_playItem_t *track = txt_track->data;
    update_lyrics_view(lyr_txt, track);
    free(lyr_txt);
    free(txt_track);
    return G_SOURCE_REMOVE;
}

static void
retrieve_similar_artists(void *ctx) {

    trace("infobar-ng: retrieving similar artists\n");
    DB_playItem_t *track = (DB_playItem_t*) ctx;

    char *artist = NULL;
    SimilarInfoList *similar = NULL;

    if (!is_track_changed(track)) {
        g_idle_add(&similar_view_loading, NULL);

        if (get_artist_info(track, &artist) == -1)
            goto update;

        if (fetch_similar_artists(artist, &similar) == -1) {
            free(artist);
            goto update;
        }
        free(artist);
    }

update:
    if (!is_track_changed(track)) {
        g_idle_add(&display_similar_artists, similar);
    } else {
        free_sim_list(similar);
    }
}

static void
retrieve_artist_bio(void *ctx) {
    trace("infobar-ng: retrieving artist's biography\n");
    DB_playItem_t *track = (DB_playItem_t*) ctx;

    char *bio_txt = NULL, *artist = NULL, *img_cache = NULL;
    Pair *pair = NULL;

    if (!is_track_changed(track)) {

        g_idle_add(&bio_view_loading, NULL);

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

                /* Making sure, that retrieved text has UTF-8 encoding,
                 * otherwise converting it. */
                char *bio_utf8 = NULL;
                if (deadbeef->junk_detect_charset(bio_txt)) {
                    if (convert_to_utf8(bio_txt, &bio_utf8) == 0) {
                        free(bio_txt);
                        bio_txt = bio_utf8;
                    }
                }
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
    if (!is_track_changed(track) && (pair = malloc(sizeof(*pair)))) {
        *pair = (Pair) { .text = bio_txt, .data = img_cache };
        g_idle_add(&display_bio, pair);
    } else {
        free(bio_txt);
        free(img_cache);
    }
}

static void
retrieve_track_lyrics(void *ctx) {

    trace("infobar-ng: retrieving track lyrics\n");
    DB_playItem_t *track = (DB_playItem_t*) ctx;

    char *lyr_txt = NULL, *artist = NULL, *title = NULL, *album = NULL;
    Pair *pair = NULL;

    if (!is_track_changed(track)) {

        g_idle_add(&lyrics_view_loading, track);

        if (get_full_track_info(track, &artist, &title, &album) == -1)
            goto update;

        char *txt_cache = NULL;
        if (create_lyr_cache(artist, title, &txt_cache) == -1) {
            free(artist);
            free(title);
            free(album);
            goto update;
        }

        if (!is_exists(txt_cache) || is_old_cache(txt_cache, LYRICS)) {
            /* There is no cache for the current track or the previous cache
             * is too old, so start retrieving new one. */
            if (deadbeef->conf_get_int(CONF_LYRICWIKI_ENABLED, 1) && !lyr_txt)
                fetch_lyrics_from_lyricwiki(artist, title, &lyr_txt);

            if (deadbeef->conf_get_int(CONF_LYRICSMANIA_ENABLED, 1) && !lyr_txt)
                fetch_lyrics_from_lyricsmania(artist, title, &lyr_txt);

            if (deadbeef->conf_get_int(CONF_LYRICSTIME_ENABLED, 1) && !lyr_txt)
                fetch_lyrics_from_lyricstime(artist, title, &lyr_txt);

            if (deadbeef->conf_get_int(CONF_MEGALYRICS_ENABLED, 1) && !lyr_txt)
                fetch_lyrics_from_megalyrics(artist, title, &lyr_txt);

            if (deadbeef->conf_get_int(CONF_LYRICS_SCRIPT_ENABLED, 0) && !lyr_txt) {

                deadbeef->pl_lock();
                char *fname = strdup(deadbeef->pl_find_meta_raw(track, ":URI"));
                deadbeef->pl_unlock();

                fetch_lyrics_from_script(artist, title, album, fname, &lyr_txt);

                free(fname);
            }

            if (lyr_txt) {
                char *lyr_wo_nl = NULL;
                /* Some lyrics contains new line characters at the
                 * beginning of the text, so we gonna strip them. */
                if (del_nl(lyr_txt, &lyr_wo_nl) == 0) {
                    free(lyr_txt);
                    lyr_txt = lyr_wo_nl;
                }

                /* Making sure, that retrieved text has UTF-8 encoding,
                 * otherwise converting it. */
                char *lyr_utf8 = NULL;
                if (deadbeef->junk_detect_charset(lyr_txt)) {
                    if (convert_to_utf8(lyr_txt, &lyr_utf8) == 0) {
                        free(lyr_txt);
                        lyr_txt = lyr_utf8;
                    }
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
        free(album);
    }

update:
    if (!is_track_changed(track) && (pair = malloc(sizeof(*pair)))) {
        *pair = (Pair) { .text = lyr_txt, .data = track };
        g_idle_add(&display_lyrics, pair);
    } else {
        free(lyr_txt);
    }
}

static void
infobar_songstarted(ddb_event_track_t *ev) {

    trace("infobar-ng: infobar-ng song started\n");

    /* Don't retrieve anything as all tabs are invisible. */
    if (!deadbeef->conf_get_int(CONF_LYRICS_ENABLED, 1) &&
        !deadbeef->conf_get_int(CONF_BIO_ENABLED, 1) &&
        !deadbeef->conf_get_int(CONF_SIM_ENABLED, 1)) {
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
    if (deadbeef->conf_get_int(CONF_SIM_ENABLED, 1)) {
        intptr_t tid = deadbeef->thread_start(retrieve_similar_artists, ev->track);
        deadbeef->thread_detach(tid);
    }
}

static int
infobar_message(struct ddb_gtkui_widget_s *w, uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2) {

    switch (id) {
    case DB_EV_SONGSTARTED:
    {
        trace("infobar-ng: recieved songstarted message\n");
        ddb_event_track_t* event = (ddb_event_track_t*) ctx;

        if (!event->track)
            return 0;

        if (!is_stream(event->track))
            infobar_songstarted(event);
    }
        break;
    case DB_EV_TRACKINFOCHANGED:
    {
        trace("infobar-ng: recieved trackinfochanged message\n");
        ddb_event_track_t* event = (ddb_event_track_t*) ctx;

        if (!event->track)
            return 0;

        if (!is_stream(event->track))
            infobar_songstarted(event);
    }
        break;
    case DB_EV_CONFIGCHANGED:
        g_idle_add(&infobar_config_changed, NULL);
        break;
    }
    return 0;
}

static ddb_gtkui_widget_t
*w_infobar_create (void) {
    ddb_gtkui_widget_t *widget = malloc(sizeof(ddb_gtkui_widget_t));
    memset(widget, 0, sizeof(ddb_gtkui_widget_t));

    create_infobar();
    widget->widget = infobar;
    widget->init = infobar_init;
    widget->destroy = infobar_destroy;
    widget->message = infobar_message;

    gtkui_plugin->w_override_signals(widget->widget, widget);
    return widget;
}

static int
infobar_connect(void) {

    trace("infobar-ng: connecting the plug-in\n");
    gtkui_plugin = (ddb_gtkui_t *)deadbeef->plug_get_for_id(DDB_GTKUI_PLUGIN_ID);
    if (!gtkui_plugin) {
        fprintf(stderr, "infobar-ng: can't find gtkui plugin\n");
        return -1;
    }
    gtkui_plugin->w_reg_widget(WIDGET_LABEL, 0, w_infobar_create, WIDGET_ID, NULL);
    return 0;
}

static int
infobar_disconnect(void) {

    trace("infobar-ng: disconnecting the plug-in\n");
    if (gtkui_plugin) {
        gtkui_plugin->w_unreg_widget(WIDGET_ID);
    }
    return 0;
}

static const char settings_dlg[] =
    "property \"Enable lyrics\" checkbox infobar.lyrics.enabled 1;"
    "property \"Fetch from LyricWiki\" checkbox infobar.lyrics.lyricwiki 1;"
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
    "property \"Enable similar artists\" checkbox infobar.similar.enabled 1;"
    "property \"Max number of similar artists\" spinbtn[0,99,1] infobar.similar.max.artists 10;"
;

static DB_misc_t plugin = {

    .plugin.api_vmajor = 1,
    .plugin.api_vminor = 5,
    .plugin.version_major = 1,
    .plugin.version_minor = 4,
    .plugin.type = DB_PLUGIN_MISC,
    .plugin.name = "Infobar-ng",
    #if GTK_CHECK_VERSION(3, 0, 0)
    .plugin.id = "infobar-gtk3",
    #else
    .plugin.id = "infobar-gtk2",
    #endif
    .plugin.descr = "Infobar-ng plugin for DeadBeeF audio player.\nFetches and shows:\n"
                    "- song's lyrics;\n- artist's biography;\n- list of similar artists.\n\n"
                    "To change the biography's locale, set an appropriate\nISO 639-2 locale code.\n"
                    "See http://en.wikipedia.org/wiki/List_of_ISO_639-2_codes\nfor more infomation.\n\n"
                    "You can set cache update period to 0 if you don't want to update\nthe cache at all.",
    .plugin.copyright =
        "Copyright (C) 2015 Ignat Loskutov <ignat.loskutov@gmail.com>\n"
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
    .plugin.website = "https://bitbucket.org/IgnatLoskutov/deadbeef-infobar-ng",
    .plugin.connect = infobar_connect,
    .plugin.disconnect = infobar_disconnect,
    .plugin.configdialog = settings_dlg
};

#if GTK_CHECK_VERSION(3, 0, 0)
DB_plugin_t *ddb_infobar_gtk3_load(DB_functions_t *ddb) {
#else
DB_plugin_t *ddb_infobar_gtk2_load(DB_functions_t *ddb) {
#endif
    deadbeef = ddb;
    return DB_PLUGIN(&plugin);
}
