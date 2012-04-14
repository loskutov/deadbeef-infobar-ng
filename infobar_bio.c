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

#include "infobar_bio.h"

/* Forms URL, which will be used to retrieve artists's biography and image. */
static int 
form_bio_url(const char *artist, char **url) {
    
    int alen = strlen(artist) * 4;
    
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

/* Fetches artist's biography from lastfm. */
int fetch_bio_txt(const char *artist, char **txt) {
    
    char *url = NULL;
    if (form_bio_url(artist, &url) == -1)
        return -1;
    
    char *raw_page = NULL;
    if (retrieve_txt_content(url, &raw_page) == -1) {
        free(url);
        return -1;
    }
    free(url);

    char *html_txt = NULL;
    if (parse_content(raw_page, BIO_TXT_XML_PATTERN, &html_txt, XML, 0) == -1) {
        free(raw_page);
        return -1;
    }
    free(raw_page);
    
    char *bio_txt = NULL;
    if (parse_content(html_txt, BIO_TXT_HTML_PATTERN, &bio_txt, HTML, 0) == -1) {
        free(html_txt);
        return -1;
    }
    *txt = bio_txt;
    
    /* Making sure, that retrieved text has UTF-8 encoding,
     * otherwise converting it. */
    char *bio_utf8 = NULL;
    if (deadbeef->junk_detect_charset(bio_txt)) {
        if (convert_to_utf8(bio_txt, &bio_utf8) == 0) {
            free(bio_txt);
            *txt = bio_utf8;
        }
    }
    return 0;
}

/* Fetches artist's image from lastfm. Retrieved image will
 * be saved to the specified path. */
int fetch_bio_image(const char *artist, const char *path) {
    
    char *url = NULL;
    if (form_bio_url(artist, &url) == -1)
        return -1;

    char *raw_page = NULL;
    if (retrieve_txt_content(url, &raw_page) == -1) {
        free(url);
        return -1;
    }
    free(url);
    
    char *img_url = NULL;
    if (parse_content(raw_page, BIO_IMG_PATTERN, &img_url, XML, 0) == -1) {
        free(raw_page);
        return -1;
    }
    free(raw_page);
    
    if (retrieve_img_content(img_url, path) == -1) {
        free(img_url);
        return -1;
    }
    free(img_url);
    return 0;
}
