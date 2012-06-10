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

#include "biography.h"

/* Forms an URL, which is used to retrieve artists's biography and image. */
static int 
form_bio_url(const char *artist, char **url) {
    
    char *eartist = NULL;
    if (encode_artist(artist, &eartist, '+') == -1)
        return -1;
    
    deadbeef->conf_lock();
    const char *locale = deadbeef->conf_get_str_fast(CONF_BIO_LOCALE, "en");
    
    if (asprintf(url, BIO_URL_TEMP, eartist, locale) == -1) {
        deadbeef->conf_unlock();
        free(eartist);
        return -1;
    }
    deadbeef->conf_unlock();
    free(eartist);
    return 0;
}

/* Fetches artist's biography from lastfm. */
int fetch_bio_txt(const char *artist, char **bio) {
    
    char *url = NULL;
    if (form_bio_url(artist, &url) == -1)
        return -1;
    
    char *raw_page = NULL;
    if (retrieve_txt_content(url, &raw_page) == -1) {
        free(url);
        return -1;
    }
    free(url);

    char *xml = NULL;
    if (parse_common(raw_page, BIO_TXT_XML_EXP, XML, &xml) == -1) {
        free(raw_page);
        return -1;
    }
    free(raw_page);
    
    char *html = NULL;
    if (parse_common(xml, BIO_TXT_HTML_EXP, HTML, &html) == -1) {
        free(xml);
        return -1;
    }
    free(xml);
    *bio = html;
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
    if (parse_common(raw_page, BIO_IMG_EXP, XML, &img_url) == -1) {
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
