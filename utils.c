/*
    Infobar plugin for DeaDBeeF music player
    Copyright (C) 2012 Dmitriy Simbiriatin <dmitriy.simbiriatin@gmail.com>

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

int load_content(const char *file, char **content) {
    
    FILE *in_file = fopen(file, "r");
    if(!in_file) 
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
        
        if (outl <= 1) return -1;

        if (!(
            (*str >= '0' && *str <= '9') ||
            (*str >= 'a' && *str <= 'z') ||
            (*str >= 'A' && *str <= 'Z') ||
            (*str == ' ') ||
            (*str == '\'') ||
            (*str == '/')
        ))
        {
            if (outl <= 3) return -1;

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
        
    memcpy(artist, str + bi, (mi - bi) - 1);
    memcpy(title, str + mi, (ei - mi) - 1);
    
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
