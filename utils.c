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

gboolean is_redirect(const char *str) {
    
    if (!str) return FALSE;
        
    return strstr(str, "#REDIRECT") || 
           strstr(str, "#redirect");
}

gboolean is_exists(const char *obj) {
    
    struct stat st = {0};
    return stat(obj, &st) == 0;
}

gboolean is_stream(DB_playItem_t *track) {
    return deadbeef->pl_get_item_duration(track) <= 0.000000;
}

static void
parser_errors_handler(void *ctx, const char *msg, ...) {}

int parse_content(const char *content, const char *pattern, char **parsed, ContentType type, int num) {
    
    xmlDocPtr doc = NULL;
    xmlSetGenericErrorFunc(NULL, parser_errors_handler);

    int size = strlen(content);

    switch (type) {
    case HTML:
        doc = htmlReadMemory(content, size, NULL, "utf-8", 
            (HTML_PARSE_RECOVER | HTML_PARSE_NONET));
        break;
    case XML:
        doc = xmlReadMemory(content, size, NULL, "utf-8", 
            (XML_PARSE_RECOVER | XML_PARSE_NONET));
        break;
    }
    xmlSetGenericErrorFunc(NULL, NULL);
        
    if (!doc) 
        return -1;
        
    int res = 0;
    
    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    if (!ctx) {
        res = -1;
        goto cleanup;
    }
        
    xmlXPathObjectPtr obj = xmlXPathEvalExpression((xmlChar*) pattern, ctx);
    if (!obj || !obj->nodesetval->nodeMax) {
        res = -1;
        goto cleanup;
    }
    
    xmlNodePtr node = obj->nodesetval->nodeTab[num];
    if (!node) {
        res = -1;
        goto cleanup;
    }
    *parsed = (char*) xmlNodeGetContent(node);

cleanup:
    if (obj) xmlXPathFreeObject(obj);
    if (ctx) xmlXPathFreeContext(ctx);
    if (doc) xmlFreeDoc(doc);
    return res;
}

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
        return -1;
    }
    deadbeef->fclose(stream);
    return 0;
}

int retrieve_img_content(const char *url, const char *img) {
    
    DB_FILE *stream = deadbeef->fopen(url);
    if (!stream)
        return -1;

    FILE *out_file = fopen(img, "wb+");
    if (!out_file) {
        deadbeef->fclose(stream);
        return -1;
    }

    int len = 0;
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

int load_txt_file(const char *file, char **content) {
    
    FILE *in_file = fopen(file, "r");
    if (!in_file) 
        return -1;

    if (fseek(in_file, 0, SEEK_END) != 0) {
        fclose(in_file);
        return -1;
    }
    
    int size = ftell(in_file);
    rewind(in_file);

    *content = calloc(size + 1, sizeof(char));
    if (!*content) {
        fclose(in_file);
        return -1;
    }
    
    if (fread(*content, 1, size, in_file) != size) {
        fclose(in_file);
        free(*content);
        return -1;
    }
    fclose(in_file);
    return 0;
}

int save_txt_file(const char *file, const char *content) {
    
    FILE *out_file = fopen(file, "w+");
    if (!out_file)
        return -1;
        
    int size = strlen(content);
    
    if (fwrite(content, 1, size, out_file) <= 0) {
        fclose(out_file);
        return -1;
    }
    fclose(out_file);
    return 0;
}

int del_lyr_cache(const char *artist, const char *title) {
    
    char *cache_path = NULL;
    if (get_cache_path(&cache_path, LYRICS) == -1)
        return -1;
    
    char *txt_cache = NULL;
    if (asprintf(&txt_cache, "%s/%s-%s", cache_path, artist, title) == -1) {
        free(cache_path);
        return -1;
    }
    free(cache_path);
    
    if (remove(txt_cache) != 0) {
        free(txt_cache);
        return -1;
    }
    free(txt_cache);
    return 0;
}

int del_bio_cache(const char *artist) {
    
    char *cache_path = NULL;
    if (get_cache_path(&cache_path, BIO) == -1)
        return -1;
    
    char *txt_cache = NULL;
    if (asprintf(&txt_cache, "%s/%s", cache_path, artist) == -1) {
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
    if (asprintf(&img_cache, "%s/%s_img", cache_path, artist) == -1) {
        free(cache_path);
        return -1;
    }
    
    if (remove(img_cache) != 0) {
        free(cache_path);
        free(img_cache);
        return -1;
    }
    free(cache_path);
    free(img_cache);
    return 0;
}

int get_cache_path(char **path, ContentType type) {
    
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

gboolean is_old_cache(const char *cache_file, CacheType type) {
    
    int uperiod = 0;
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

int create_dir(const char *dir, mode_t mode) {
    
    struct stat st;
    
    char *tmp = strdup(dir);
    char *slash = tmp;
    
    do {
        slash = strstr(slash + 1, "/");
        if (slash) *slash = 0;
            
        if (stat(tmp, &st) == -1 ) {
            if (mkdir(tmp, mode) != 0) {
                free(tmp);
                return -1;
            }
        }
        if (slash) *slash = '/';
        
    } while(slash);

    free(tmp);
    return 0;
}

int uri_encode(char *out, int outl, const char *str, char space) {
    
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

int convert_to_utf8(const char *str, char **str_utf8) {
    
    int len = strlen(str);

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

int del_nl(const char *txt, char **txt_wo_nl) {
    
    int num = 0;
    int len = strlen(txt);
    
    for (int i = 0; i < len; ++i) {
        if (txt[i] == '\n')
            ++num;
        else
            break;
    }
    if (num == 0) 
        return -1;
    
    *txt_wo_nl = calloc(len - num + 1, sizeof(char));
    if (!*txt_wo_nl)
        return -1;
    
    memcpy(*txt_wo_nl, txt + num, len - num + 1);
    return 0;
}

int concat_lyrics(const char *fst_lyr, const char *snd_lyr, char **lyr) {
    
    const char *sep = "\n**************\n";
    
    int fst_len = strlen(fst_lyr);
    int snd_len = strlen(snd_lyr);
    int sep_len = strlen(sep);
    
    *lyr = calloc(fst_len + snd_len + sep_len + 1, sizeof(char));
    if (!*lyr) 
        return -1;
    
    memcpy(*lyr, fst_lyr, fst_len + 1);
    memcpy(*lyr + fst_len, sep, sep_len + 1);
    memcpy(*lyr + fst_len + sep_len, snd_lyr, snd_len + 1);

    return 0;
}

int get_redirect_info(const char *str, char **artist, char **title) {
    
    char *bp = strchr(str, '[');
    char *mp = strchr(str, ':');
    char *ep = strchr(str, ']');
    
    int bi = bp - str + 1;
    int mi = mp - str + 1;
    int ei = ep - str + 1;
    
    *artist = calloc((mi - bi) + 1, sizeof(char));
    if (!*artist)
        return -1;
        
    *title = calloc((ei - mi) + 1, sizeof(char));
    if (!*title) {
        free(*artist);
        return -1;
    }
        
    memcpy(*artist, str + bi, (mi - bi) - 1);
    memcpy(*title, str + mi, (ei - mi) - 1);
    
    return 0;
}

int get_track_info(DB_playItem_t *track, char **artist, char **title) {
    
    deadbeef->pl_lock();

    const char *cur_artist = deadbeef->pl_find_meta(track, "artist");
    const char *cur_title =  deadbeef->pl_find_meta(track, "title");

    if (!cur_artist || !cur_title) {
        deadbeef->pl_unlock();
        return -1;
    }
    
    int alen = strlen(cur_artist);
    
    *artist = calloc(alen + 1, sizeof(char));
    if (!*artist) {
        deadbeef->pl_unlock();
        return -1;
    } 
    
    int tlen = strlen(cur_title);
    
    *title = calloc(tlen + 1, sizeof(char));
    if (!*title) {
        deadbeef->pl_unlock();
        free(*artist);
        return -1;
    }
    memcpy(*artist, cur_artist, alen + 1);
    memcpy(*title, cur_title, tlen + 1);

    deadbeef->pl_unlock();
    return 0;
}

int update_track_info(const char *artist, const char * title, char **old_artist, char **old_title) {
    
    int alen = strlen(artist);
    int tlen = strlen(title);
    
    *old_artist = calloc(alen + 1, sizeof(char));
    if (!*old_artist) 
        return -1;
        
    *old_title = calloc(tlen + 1, sizeof(char));
    if (!*old_title) {
        free(*old_artist);
        return -1;
    }
    
    memcpy(*old_artist, artist, alen + 1);
    memcpy(*old_title, title, tlen + 1);
    
    return 0;
}

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
