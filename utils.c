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

#include "utils.h"

static char *
escape_name(const char * s) {
    char * ans = strdup(s);
    if (!ans)
        return NULL;
    for (char * c = ans; *c; ++c) {
        if (*c == '/')
            *c = '_';
    }
    return ans;
}

/* Retrieves a path to the lyrics or biography cache directory. */
static int
get_cache_path(char **path, CacheType type) {

    int res = -1;
    const char *home_cache = getenv("XDG_CACHE_HOME");

    switch(type) {
    case LYRICS:
        res = asprintf(path, home_cache ? "%s/deadbeef/lyrics" : "%s/.cache/deadbeef/lyrics",
                home_cache ? home_cache : getenv("HOME"));
        break;
    case BIO:
        res = asprintf(path, home_cache ? "%s/deadbeef/bio" : "%s/.cache/deadbeef/bio",
                home_cache ? home_cache : getenv("HOME"));
        break;
    }
    return res;
}

/* Recursively creates directory for cache files. */
static int
create_dir(const char *dir, mode_t mode) {

    char *tmp = strdup(dir);
    char *slash = tmp;

    do {
        slash = strstr(slash + 1, "/");
        if (slash)
            *slash = 0;

        if (!is_exists(tmp)) {
            if (mkdir(tmp, mode) != 0) {
                free(tmp);
                return -1;
            }
        }
        if (slash)
            *slash = '/';

    } while(slash);

    free(tmp);
    return 0;
}

/* Encodes specified string. */
static int
uri_encode(char *out, size_t outl, const char *str, char space) {

    size_t l = outl;

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


/* Checks if specified file or directory is exists. */
gboolean is_exists(const char *obj) {

    struct stat st = {0};
    return stat(obj, &st) == 0;
}

/* Checks if the specified track item is a local track or a stream. */
gboolean is_stream(DB_playItem_t *track) {
    return deadbeef->pl_get_item_duration(track) <= 0.000000;
}

/* Checks if the current track item is differs from specified one. */
gboolean is_track_changed(DB_playItem_t *track) {

    DB_playItem_t *pl_track = deadbeef->streamer_get_playing_track();
    if (!pl_track)
        return FALSE;

    if (track == pl_track) {
        deadbeef->pl_item_unref(pl_track);
        return FALSE;
    }
    deadbeef->pl_item_unref(pl_track);
    return TRUE;
}

/* Common function to parse XML and HTML content using XPath expression. */
int parse_common(const char *content, const char *exp, ContentType type, char **psd) {

    xmlDocPtr doc = NULL;
    if (init_doc_obj(content, type, &doc) == -1)
        return -1;

    xmlXPathObjectPtr xpath = NULL;
    if (get_xpath_obj(doc, exp, &xpath) == -1) {
        xmlFreeDoc(doc);
        return -1;
    }
    xmlNodePtr node = xpath->nodesetval->nodeTab[0];
    *psd = (char*) xmlNodeGetContent(node);

    xmlXPathFreeObject(xpath);
    xmlFreeDoc(doc);
    return 0;
}

/* Initializes xmlDoc object depending on content type. */
int init_doc_obj(const char *content, ContentType type, xmlDocPtr *doc) {

    size_t len = strlen(content);

    switch(type) {
    case XML:
        *doc = xmlReadMemory(content, len, NULL, "utf-8", (XML_PARSE_RECOVER |
                   XML_PARSE_NONET | XML_PARSE_NOWARNING | XML_PARSE_NOERROR));
        break;
    case HTML:
        *doc = htmlReadMemory(content, len, NULL, "utf-8", (HTML_PARSE_RECOVER |
                  HTML_PARSE_NONET | HTML_PARSE_NOWARNING | HTML_PARSE_NOERROR));
    }
    return *doc ? 0 : -1;
}

/* Creates an instance of XPath object for specified expression. */
int get_xpath_obj(const xmlDocPtr doc, const char *exp, xmlXPathObjectPtr *obj) {

    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    if (!ctx)
        return -1;

    *obj = xmlXPathEvalExpression((xmlChar*) exp, ctx);
    if (!*obj || (*obj)->nodesetval->nodeNr == 0) {
        xmlXPathFreeContext(ctx);
        return -1;
    }
    xmlXPathFreeContext(ctx);
    return 0;
}

/* Retrieves text data from the specified URL.*/
int retrieve_txt_content(const char *url, char **content) {

    DB_FILE *stream = deadbeef->fopen(url);
    if (!stream)
        return -1;

    *content = calloc(MAX_TXT_SIZE + 1, sizeof(char));
    if (!*content) {
        deadbeef->fclose(stream);
        return -1;
    }

    if (deadbeef->fread(*content, 1, MAX_TXT_SIZE, stream) <= 0) {
        deadbeef->fclose(stream);
        free(*content);
        *content = NULL;
        return -1;
    }
    deadbeef->fclose(stream);
    return 0;
}

/* Retrieves image file from the specified URL and saves it locally. */
int retrieve_img_content(const char *url, const char *img) {

    DB_FILE *stream = deadbeef->fopen(url);
    if (!stream)
        return -1;

    FILE *out_file = fopen(img, "wb+");
    if (!out_file) {
        deadbeef->fclose(stream);
        return -1;
    }

    size_t len = 0;
    char temp[4096] = {0};

    while ((len = deadbeef->fread(temp, 1, sizeof(temp), stream)) > 0) {
        if (fwrite(temp, 1, len, out_file) != len) {
            deadbeef->fclose(stream);
            fclose(out_file);
            return -1;
        }
    }
    deadbeef->fclose(stream);
    fclose(out_file);
    return 0;
}

/* Loads content of the specified text file. */
int load_txt_file(const char *file, char **content) {

    FILE *in_file = fopen(file, "r");
    if (!in_file)
        return -1;

    if (fseek(in_file, 0, SEEK_END) != 0) {
        fclose(in_file);
        return -1;
    }

    size_t size = ftell(in_file);
    rewind(in_file);

    *content = calloc(size + 1, sizeof(char));
    if (!*content) {
        fclose(in_file);
        return -1;
    }

    if (fread(*content, 1, size, in_file) != size) {
        fclose(in_file);
        free(*content);
        *content = NULL;
        return -1;
    }
    fclose(in_file);
    return 0;
}

/* Saves specified content to the text file. */
int save_txt_file(const char *file, const char *content) {

    FILE *out_file = fopen(file, "w+");
    if (!out_file)
        return -1;

    size_t size = strlen(content);

    if (fwrite(content, 1, size, out_file) <= 0) {
        fclose(out_file);
        return -1;
    }
    fclose(out_file);
    return 0;
}

/* Executes external script and reads its output. */
int execute_script(const char *cmd, char **out) {

    FILE *script = popen(cmd, "r");
    if (!script)
        return -1;

    *out = calloc(MAX_TXT_SIZE + 1, sizeof(char));
    if (!*out) {
        pclose(script);
        return -1;
    }

    if (fread(*out, 1, MAX_TXT_SIZE, script) <= 0) {
        pclose(script);
        free(*out);
        *out = NULL;
        return -1;
    }
    pclose(script);
    return 0;
}

/* Deletes lyrics cache for specified track. */
int del_lyr_cache(const char *artist, const char *title) {

    char *cache_path = NULL;
    if (get_cache_path(&cache_path, LYRICS) == -1)
        return -1;

    char * txt_cache = NULL;
    char * escaped_artist = escape_name(artist);
    char * escaped_title = escape_name(title);
    if (!escaped_artist || !escaped_title ||
            asprintf(&txt_cache, "%s/%s-%s", cache_path, escaped_artist, escaped_title) == -1) {
        free(escaped_artist);
        free(escaped_title);
        free(cache_path);
        return -1;
    }
    free(escaped_artist);
    free(escaped_title);
    free(cache_path);

    if (remove(txt_cache) != 0) {
        free(txt_cache);
        return -1;
    }
    free(txt_cache);
    return 0;
}

/* Deletes biography cache for specified artist. */
int del_bio_cache(const char *artist) {

    char *cache_path = NULL;
    if (get_cache_path(&cache_path, BIO) == -1)
        return -1;

    char * txt_cache = NULL;
    char * escaped_artist = escape_name(artist);
    if (!escaped_artist ||
            asprintf(&txt_cache, "%s/%s", cache_path, escaped_artist) == -1) {
        free(escaped_artist);
        free(cache_path);
        return -1;
    }

    if (remove(txt_cache) != 0) {
        free(cache_path);
        free(txt_cache);
        return -1;
    }
    free(txt_cache);

    char *img_cache = NULL;
    if (asprintf(&img_cache, "%s/%s_img", cache_path, escaped_artist) == -1) {
        free(cache_path);
        return -1;
    }
    free(escaped_artist);

    if (remove(img_cache) != 0) {
        free(cache_path);
        free(img_cache);
        return -1;
    }
    free(cache_path);
    free(img_cache);
    return 0;
}

/* Creates lyrics cache file for the specified track. */
int create_lyr_cache(const char *artist, const char *title, char **txt_cache) {

    char *cache_path = NULL;
    if (get_cache_path(&cache_path, LYRICS) == -1)
        return -1;

    if (!is_exists(cache_path)) {
        if (create_dir(cache_path, 0755) == -1) {
            free(cache_path);
            return -1;
        }
    }

    char * escaped_artist = escape_name(artist);
    char * escaped_title = escape_name(title);
    if (!escaped_artist || !escaped_title ||
            asprintf(txt_cache, "%s/%s-%s", cache_path, escaped_artist, escaped_title) == -1) {
        free(escaped_artist);
        free(escaped_title);
        free(cache_path);
        return -1;
    }
    free(escaped_artist);
    free(escaped_title);
    free(cache_path);
    return 0;
}

/* Creates biography cache files for the specified artist. */
int create_bio_cache(const char *artist, char **txt_cache, char **img_cache) {

    char *cache_path = NULL;
    if (get_cache_path(&cache_path, BIO) == -1)
        return -1;

    if (!is_exists(cache_path)) {
        if (create_dir(cache_path, 0755) == -1) {
            free(cache_path);
            return -1;
        }
    }

    char * escaped_artist = escape_name(artist);
    if (!escaped_artist ||
            asprintf(txt_cache, "%s/%s", cache_path, escaped_artist) == -1) {
        free(escaped_artist);
        free(cache_path);
        return -1;
    }

    if (asprintf(img_cache, "%s/%s_img", cache_path, escaped_artist) == -1) {
        free(escaped_artist);
        free(cache_path);
        free(*txt_cache);
        return -1;
    }
    free(escaped_artist);
    free(cache_path);
    return 0;
}

/* Checks if the specified cache file is old. */
gboolean is_old_cache(const char *cache_file, CacheType type) {

    size_t uperiod = 0;
    time_t tm = time(NULL);

    struct stat st;
    if (stat(cache_file, &st) == 0) {

        switch (type) {
        case LYRICS:
            uperiod = deadbeef->conf_get_int(CONF_LYRICS_UPDATE_PERIOD, 0);
            break;
        case BIO:
            uperiod = deadbeef->conf_get_int(CONF_BIO_UPDATE_PERIOD, 24);
            break;
        }
        if (uperiod == 0)
            return FALSE;

        return (uperiod > 0 && tm - st.st_mtime > uperiod * 60 * 60);
    }
    return TRUE;
}

/* Encodes artist name. */
int encode_artist(const char *artist, char **eartist, const char space) {

    size_t ealen = strlen(artist) * 4;

    *eartist = calloc(ealen + 1, sizeof(char));
    if (!*eartist)
        return -1;

    if (uri_encode(*eartist, ealen, artist, space) == -1) {
        free(*eartist);
        return -1;
    }
    return 0;
}

/* Encodes artist name and song title. */
int encode_artist_and_title(const char *artist, const char *title, char **eartist, char **etitle) {

    if (encode_artist(artist, eartist, '_') == -1)
        return -1;

    size_t etlen = strlen(title) * 4;

    *etitle = calloc(etlen + 1, sizeof(char));
    if (!*etitle) {
        free(*eartist);
        return -1;
    }

    if (uri_encode(*etitle, etlen, title, '_') == -1) {
        free(*eartist);
        free(*etitle);
        return -1;
    }
    return 0;
}

/* Encodes artist name, song title and album name. */
int encode_full(const char *artist, const char *title, const char *album, char **eartist, char **etitle, char **ealbum) {

    if (encode_artist_and_title(artist, title, eartist, etitle) == -1)
        return -1;

    size_t ealen = strlen(album) * 4;

    *ealbum = calloc(ealen + 1, sizeof(char));
    if (!*ealbum) {
        free(*eartist);
        free(*etitle);
        return -1;
    }

    if (uri_encode(*ealbum, ealen, album, '_') == -1) {
        free(*eartist);
        free(*etitle);
        free(*ealbum);
        return -1;
    }
    return 0;
}

/* Converts specified string encoding to UTF-8. */
int convert_to_utf8(const char *str, char **str_utf8) {

    size_t len = strlen(str);
    const char *str_cs = deadbeef->junk_detect_charset(str);
    if (!str_cs)
        return -1;

    *str_utf8 = calloc(len * 4, sizeof(char));
    if (!*str_utf8)
        return -1;

    if (deadbeef->junk_iconv(str, len, *str_utf8, len * 4, str_cs, "utf-8") < 0) {
        free(*str_utf8);
        return -1;
    }
    return 0;
}

/* Deletes new lines at the beginning of specified text data. */
int del_nl(const char *txt, char **wo_nl) {

    size_t num = 0;

    for (; *txt; ++txt) {
        if (*txt == '\n' ||
            *txt == '\r')
        {
            ++num;
            continue;
        }
        break;
    }
    txt-=num;

    /*No new line characters found. */
    if (num == 0)
        return -1;

    size_t len = strlen(txt);

    *wo_nl = calloc(len - num + 1, sizeof(char));
    if (!*wo_nl)
        return -1;

    memcpy(*wo_nl, txt + num, len - num + 1);
    return 0;
}

/* Concatenates two lyrics texts into one, using simple separator to visually divide them. */
int concat_lyrics(const char *fst, const char *snd, char **lyr) {

    size_t fst_len = strlen(fst);
    size_t snd_len = strlen(snd);
    size_t sep_len = strlen(SEP);

    *lyr = calloc(fst_len + snd_len + sep_len + 1, sizeof(char));
    if (!*lyr)
        return -1;

    memcpy(*lyr, fst, fst_len + 1);
    memcpy(*lyr + fst_len, SEP, sep_len + 1);
    memcpy(*lyr + fst_len + sep_len, snd, snd_len + 1);
    return 0;
}

/* Replaces each substring of the specified string with the given replacement. */
int replace_all(const char *str, const char *orig, const char *with, char **repl) {

    size_t str_len = strlen(str);
    size_t orig_len = strlen(orig);
    size_t with_len = strlen(with);

    char *cur = NULL;
    size_t count = 0;

    /* Counting occurences of the substring. */
    const char *ins = str;
    while ((cur = strstr(ins, orig))) {
        ins = cur + orig_len;
        ++count;
    }
    /* No occurences found. */
    if (count == 0)
        return -1;

    size_t repl_len = str_len - (orig_len * count)
                              + (with_len * count);

    *repl = calloc(repl_len + 1, sizeof(char));
    if (!*repl)
        return -1;

    char *to = *repl;
    size_t copied = 0;

    /* Replacing substrings. */
    while ((cur = strstr(str, orig))) {

        int copy = cur - str;
        memcpy(to, str, copy);
        to+=copy; str = cur;

        memcpy(to, with, with_len + 1);
        to+=with_len; str+=orig_len;

        copied+=copy + with_len;
        /* We've already replaced all the substrings. */
        if (--count == 0) break;
    }
    memcpy(to, str, repl_len - copied);
    return 0;
}

/* Retrieves infomation about current artist. */
int get_artist_info(DB_playItem_t *track, char **artist) {

    deadbeef->pl_lock();

    const char *cur_artist = deadbeef->pl_find_meta(track, "artist");
    if (!cur_artist) {
        deadbeef->pl_unlock();
        return -1;
    }
    size_t alen = strlen(cur_artist);

    *artist = calloc(alen + 1, sizeof(char));
    if (!*artist) {
        deadbeef->pl_unlock();
        return -1;
    }
    memcpy(*artist, cur_artist, alen + 1);
    deadbeef->pl_unlock();
    return 0;
}

/* Retrieves infomation about current artist and title */
int get_artist_and_title_info(DB_playItem_t *track, char **artist, char **title) {

    if (get_artist_info(track, artist) == -1)
        return -1;

    deadbeef->pl_lock();

    const char *cur_title = deadbeef->pl_find_meta(track, "title");
    if (!cur_title) {
        deadbeef->pl_unlock();
        free(*artist);
        return -1;
    }
    size_t tlen = strlen(cur_title);

    *title = calloc(tlen + 1, sizeof(char));
    if (!*title) {
        deadbeef->pl_unlock();
        free(*artist);
        return -1;
    }
    memcpy(*title, cur_title, tlen + 1);
    deadbeef->pl_unlock();
    return 0;
}

/* Retrieves information about current artist, title and album. */
int get_full_track_info(DB_playItem_t *track, char **artist, char **title, char **album) {

    if (get_artist_and_title_info(track, artist, title) == -1)
        return -1;

    deadbeef->pl_lock();

    /* Album info is optional. Leaving empty string
       if there is no album info available. */
    const char *cur_album = deadbeef->pl_find_meta(track, "album");
    if (!cur_album) {
        deadbeef->pl_unlock();
        *album = calloc(1, sizeof(char));
        return 0;
    }
    size_t alen = strlen(cur_album);

    *album = calloc(alen + 1, sizeof(char));
    if (!*album) {
        deadbeef->pl_unlock();
        free(*artist);
        free(*title);
        return -1;
    }
    memcpy(*album, cur_album, alen + 1);
    deadbeef->pl_unlock();
    return 0;
}

/*Converts string to persentage representation */
int string_to_perc(const char* str, char *perc) {

    float num = 0.0;
    sscanf(str, "%f", &num);

    int res = sprintf(perc, "%.1f%%", num * 100);
    return  res < 0 ? -1 : 0;
}

/* Calculates new resolution to respectively resize image. */
void find_new_resolution(float ww, float wh, float aw, float ah, Res *res) {

    float w = 0, h = 0;
    float ratio = wh / ww;

    if (ww > wh) {
        w = ww > aw ? aw : ww;
        h = w * ratio;
    } else {
        h = wh > ah ? ah : wh;
        w = h / ratio;
    }

    if (w > aw) {
        w = aw;
        h = w * ratio;
    }
    if (h > ah) {
        h = ah;
        w = h / ratio;
    }
    res->width = w;
    res->height = h;
}
