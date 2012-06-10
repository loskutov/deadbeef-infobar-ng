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

#include "lyrics.h"

/* Formats lyrics fetched from "http://megalyrics.ru". */
static int
format_megalyrics(const char *txt, char **fmd) {
    
    /* Removing <pre> and <h2> tags from the beginning. */
    char *wo_bpre = NULL;
    if (replace_all(txt, ML_LYR_BEG, "", &wo_bpre) == -1)
        return -1;
    
    *fmd = wo_bpre;
    
    /* Removing </pre> tag from the end. */
    char *wo_epre = NULL;
    if (replace_all(*fmd, ML_LYR_END, "", &wo_epre) == -1) {
        free(*fmd);
        return -1;
    }
    free(*fmd);
    *fmd = wo_epre;
    
    /* Replacing <br/> tags with new line characters. */
    char *wo_br = NULL;
    if (replace_all(*fmd, "<br/>", "\n", &wo_br) == -1) {
        free(*fmd);
        return -1;
    }
    free(*fmd);
    *fmd = wo_br;
    return 0;
}

/* Parses lyrics fetched from "http://megalyrics.ru". */
static int
parse_megalyrics(const char *content, char **parsed) {
    
    xmlDocPtr doc = NULL;
    if (init_doc_obj(content, HTML, &doc) == -1)
        return -1;
    
    xmlXPathObjectPtr xpath = NULL;
    if (get_xpath_obj(doc, ML_EXP, &xpath) == -1) {
        xmlFreeDoc(doc);
        return -1;
    }
    xmlNodePtr node = xpath->nodesetval->nodeTab[0];
    xmlBufferPtr nb = xmlBufferCreate();
    xmlNodeDump(nb, doc, node, 0, 1);
    
    xmlXPathFreeObject(xpath);
    xmlFreeDoc(doc);
    
    *parsed = calloc(nb->size + 1, sizeof(char));
    if (!*parsed) {
        xmlBufferFree(nb);
        return -1;
    }
    memcpy(*parsed, nb->content, nb->size + 1);
    xmlBufferFree(nb);
    return 0;
}

static int
parse_common(const char *content, const char *exp, char **parsed) {
    
    xmlDocPtr doc = NULL;
    if (init_doc_obj(content, HTML, &doc) == -1)
        return -1;
    
    xmlXPathObjectPtr xpath = NULL;
    if (get_xpath_obj(doc, exp, &xpath) == -1) {
        xmlFreeDoc(doc);
        return -1;
    }
    xmlNodePtr node = xpath->nodesetval->nodeTab[0];
    *parsed = (char*) xmlNodeGetContent(node);
    
    xmlXPathFreeObject(xpath);
    xmlFreeDoc(doc);
    return 0;
}

/* Forms an URL, which is used to retrieve lyrics for specified track. */
static int
form_lyr_url(const char *artist, const char* title, const char* template, gboolean rev, char **url) {
    
    char *eartist = NULL, *etitle = NULL;
    
    if (encode_artist_and_title(artist, title, &eartist, &etitle) == -1)
        return -1;
    
    if (asprintf(url, template, rev ? etitle : eartist, 
                                rev ? eartist : etitle) == -1) 
    {
        free(eartist);
        free(etitle);
        return -1;
    }
    free(eartist);
    free(etitle);
    return 0;
}

/* Forms command string, which is used to execute external lyrics fetch script. */
static int
form_script_cmd(const char *artist, const char* title, const char *album, 
                const char *script, const char* template, char **cmd) {
    
    char *eartist = NULL, *etitle = NULL, *ealbum = NULL;
    
    if (encode_full(artist, title, album, &eartist, &etitle, &ealbum) == -1)
        return -1;
    
    if (asprintf(cmd, template, script, eartist, etitle, ealbum) == -1) {
        free(eartist);
        free(etitle);
        free(ealbum);
        return -1;
    }
    free(eartist);
    free(etitle);
    free(ealbum);
    return 0;
}

/* Fetches lyrics from specified URL and parses it. */
static int
fetch_lyrics(const char *url, const char *exp, ContentType type, char **txt) {
    
    char *raw_page = NULL;
    if (retrieve_txt_content(url, &raw_page) == -1)
        return -1;
    
    char *lyr_txt = NULL;
    if (parse_content(raw_page, exp, &lyr_txt, type, 0) == -1) {
        free(raw_page);
        return -1;
    }
    free(raw_page);
    *txt = lyr_txt;
    
    /* Making sure, that retrieved text has UTF-8 encoding,
     * otherwise converting it. */
    char *lyr_utf8 = NULL;
    if (deadbeef->junk_detect_charset(lyr_txt)) {
        if (convert_to_utf8(lyr_txt, &lyr_utf8) == 0) {
            free(lyr_txt);
            *txt = lyr_utf8;
        }
    }
    return 0;
}

/* Fetches lyrics from "http://lyricsmania.com". */
int fetch_lyrics_from_lyricsmania(const char *artist, const char *title, char **txt) {
    
    char *url = NULL;
    if (form_lyr_url(artist, title, LM_URL_TEMP, TRUE, &url) == -1)
        return -1;
    
    char *raw_page = NULL;
    if (retrieve_txt_content(url, &raw_page) == -1) {
        free(url);
        return -1;
    }
    free(url);
    
    if (parse_common(raw_page, LM_EXP, txt) == -1) {
        free(raw_page);
        return -1;
    }
    free(raw_page);
    
    /* Making sure, that retrieved text has UTF-8 encoding,
     * otherwise converting it. */
    char *lyr_utf8 = NULL;
    if (deadbeef->junk_detect_charset(*txt)) {
        if (convert_to_utf8(*txt, &lyr_utf8) == 0) {
            free(*txt);
            *txt = lyr_utf8;
        }
    }
    return 0;
}

/* Fetches lyrics from "http://lyricstime.com". */
int fetch_lyrics_from_lyricstime(const char *artist, const char *title, char **txt) {
    
    char *url = NULL;
    if (form_lyr_url(artist, title, LT_URL_TEMP, FALSE, &url) == -1)
        return -1;
    
    char *raw_page = NULL;
    if (retrieve_txt_content(url, &raw_page) == -1) {
        free(url);
        return -1;
    }
    free(url);
    
    if (parse_common(raw_page, LT_EXP, txt) == -1) {
        free(raw_page);
        return -1;
    }
    free(raw_page);
    
    /* Making sure, that retrieved text has UTF-8 encoding,
     * otherwise converting it. */
    char *lyr_utf8 = NULL;
    if (deadbeef->junk_detect_charset(*txt)) {
        if (convert_to_utf8(*txt, &lyr_utf8) == 0) {
            free(*txt);
            *txt = lyr_utf8;
        }
    }
    return 0;
}

/* Fetches lyrics from "http://megalyrics.ru". */
int fetch_lyrics_from_megalyrics(const char *artist, const char *title, char **txt) {
    
    char *url = NULL;
    if (form_lyr_url(artist, title, ML_URL_TEMP, FALSE, &url) == -1)
        return -1;
    
    char *raw_page = NULL;
    if (retrieve_txt_content(url, &raw_page) == -1) {
        free(url);
        return -1;
    }
    free(url);
    
    if (parse_megalyrics(raw_page, txt) == -1) {
        free(raw_page);
        return -1;
    }
    free(raw_page);
    
    char *fmd_lyr = NULL;
    if (format_megalyrics(*txt, &fmd_lyr) != -1) {
        free(*txt);
        *txt = fmd_lyr;
    }
    
    /* Making sure, that retrieved text has UTF-8 encoding,
     * otherwise converting it. */
    char *lyr_utf8 = NULL;
    if (deadbeef->junk_detect_charset(*txt)) {
        if (convert_to_utf8(*txt, &lyr_utf8) == 0) {
            free(*txt);
            *txt = lyr_utf8;
        }
    }
    return 0;
}

/* Fetches lyrics from "http://lyrics.wikia.com". */
int fetch_lyrics_from_lyricswikia(const char *artist, const char *title, char **txt) {
    
    char *url = NULL;
    if (form_lyr_url(artist, title, LW_URL_TEMP, FALSE, &url) == -1)
        return -1;

    char *raw_page = NULL;
    if (fetch_lyrics(url, LW_XML_EXP, XML, &raw_page) == -1) {
        free(url);
        return -1;
    }
    free(url);
    
    /* Checking if we got a redirect. Read more about redirects 
     * here: "http://lyrics.wikia.com/Help:Redirect". */
    if (is_redirect(raw_page)) {
        
        char *rartist = NULL;
        char *rtitle = NULL;
        
        if (get_redirect_info(raw_page, &rartist, &rtitle) == 0) {
            free(raw_page);
            
            /* Retrieving lyrics again, using correct artist name and title. */
            url = NULL;
            if (form_lyr_url(rartist, rtitle, LW_URL_TEMP, FALSE, &url) == -1) {
                free(rartist);
                free(rtitle);
                return -1;
            }
            free(rartist);
            free(rtitle);
            
            raw_page = NULL;
            if (fetch_lyrics(url, LW_XML_EXP, XML, &raw_page) == -1) {
                free(url);
                return -1;
            }
            free(url);
        }
    }
    char *fst_lyr_txt = NULL;
    char *snd_lyr_txt = NULL;

    if (parse_content(raw_page, LW_HTML_EXP, &fst_lyr_txt, HTML, 0) == -1) {
        free(raw_page);
        return -1;
    }
    *txt = fst_lyr_txt;
    
    /* Some tracks on lyrics wikia have multiply lyrics, so we gonna
     * check this. */
    char *multi_lyr = NULL;
    if (parse_content(raw_page, LW_HTML_EXP, &snd_lyr_txt, HTML, 1) == 0) {
        /* We got multiply lyrics, concatenating them into one. */
        if (concat_lyrics(fst_lyr_txt, snd_lyr_txt, &multi_lyr) == 0) {
            free(fst_lyr_txt);
            *txt = multi_lyr;
        }
        free(snd_lyr_txt);
    }
    free(raw_page);
    return 0;
}

/* Fetches lyrics, using external bash script. */
int fetch_lyrics_from_script(const char *artist, const char *title, const char *album, char **txt) {

    deadbeef->conf_lock();
    const char *path = deadbeef->conf_get_str_fast(CONF_LYRICS_SCRIPT_PATH, "");
    
    char *cmd = NULL;
    if (form_script_cmd(artist, title, album, path, SR_CMD_TEMP, &cmd) == -1) {
        deadbeef->conf_unlock();
        return -1;
    }
    deadbeef->conf_unlock();
    
    if (execute_script(cmd, txt) == -1) {
        free(cmd);
        return -1;
    }
    free(cmd);
    
    /* Making sure, that retrieved text has UTF-8 encoding,
     * otherwise converting it. */
    char *txt_utf8 = NULL;
    if (deadbeef->junk_detect_charset(*txt)) {
        if (convert_to_utf8(*txt, &txt_utf8) == 0) {
            free(*txt);
            *txt = txt_utf8;
        }
    }
    return 0;
}
