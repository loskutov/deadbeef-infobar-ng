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

#include "infobar_bio.h"

int form_bio_url(const char *artist, char **url) {
    
    int alen = strlen(artist) * 3;
    
    char *eartist = calloc(alen + 1, sizeof(char));
    if (!eartist)
        return -1;
    
    if (uri_encode(eartist, alen, artist, '+') == -1) {
        free(eartist);
        return -1;
    }
    deadbeef->conf_lock();
    
    const char *locale = deadbeef->conf_get_str_fast(CONF_BIO_LOCALE, "en");
    
    if (asprintf(url, BIO_URL_TEMPLATE, eartist, locale) == -1) {
        deadbeef->conf_unlock();
        free(eartist);
        return -1;
    }
    deadbeef->conf_unlock();
    free(eartist);
    
    return 0;
}

int fetch_bio_txt(const char *url, char **txt) {
    
    char *raw_page = NULL;
    if (retrieve_txt_content(url, &raw_page) == -1)
        return -1;

    char *html_txt = NULL;
    if (parse_content(raw_page, "/lfm/artist/bio/content", &html_txt, XML, 0) == -1) {
        free(raw_page);
        return -1;
    }
    free(raw_page);
    
    char *bio_txt = NULL;
    if (parse_content(html_txt, "/html/body", &bio_txt, HTML, 0) == -1) {
        free(html_txt);
        return -1;
    }
    *txt = bio_txt;
    
    char *bio_utf8 = NULL;
    if(deadbeef->junk_detect_charset(bio_txt)) {
        if (convert_to_utf8(bio_txt, &bio_utf8) == 0) {
            free(bio_txt);
            *txt = bio_utf8;
        }
    }
    return 0;
}
