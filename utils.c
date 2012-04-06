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

gboolean is_exists(const char *obj) {
    
    struct stat st = {0};
    return stat(obj, &st) == 0;
}

int get_cache_path(char *cache_path, int len, ContentType type) {
    
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
