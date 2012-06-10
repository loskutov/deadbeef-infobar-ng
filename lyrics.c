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

/* Forms an URL, which is used to retrieve lyrics for specified track. */
static int
form_lyr_url(const char *artist, const char* title, const char* templ, gboolean rev, char **url) {
    
    char *eartist = NULL, *etitle = NULL;
    if (encode_artist_and_title(artist, title, &eartist, &etitle) == -1)
        return -1;
    
    if (asprintf(url, templ, rev ? etitle : eartist, 
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
                const char *script, const char* templ, char **cmd) {
    
    char *eartist = NULL, *etitle = NULL, *ealbum = NULL;
    if (encode_full(artist, title, album, &eartist, &etitle, &ealbum) == -1)
        return -1;
    
    if (asprintf(cmd, templ, script, eartist, etitle, ealbum) == -1) {
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

/* Formats lyrics fetched from "http://megalyrics.ru". */
static int
format_megalyrics(const char *lyr, char **fmd) {
    
    /* Removing <pre> and <h2> tags from the beginning. */
    char *wo_bpre = NULL;
    if (replace_all(lyr, ML_LYR_BEG, "", &wo_bpre) == -1)
        return -1;
    
    *fmd = wo_bpre;
    
    /* Removing </pre> tag from the end. */
    char *wo_epre = NULL;
    if (replace_all(wo_bpre, ML_LYR_END, "", &wo_epre) == -1) {
        free(wo_bpre);
        return -1;
    }
    free(wo_bpre);
    *fmd = wo_epre;
    
    /* Replacing <br/> tags with new line characters. */
    char *wo_br = NULL;
    if (replace_all(wo_epre, "<br/>", "\n", &wo_br) == -1) {
        free(wo_epre);
        return -1;
    }
    free(wo_epre);
    *fmd = wo_br;
    return 0;
}

/* Parses lyrics from XML and HTML pages. */
static int
parse_common(const char *content, const char *exp, ContentType type, char **psd) {
    
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

/* Parses lyrics fetched from "http://megalyrics.ru". */
static int
parse_megalyrics(const char *content, char **psd) {
    
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
    
    if (nb->size == 0) {
        xmlBufferFree(nb);
        return -1;
    }
    
    *psd = calloc(nb->size + 1, sizeof(char));
    if (!*psd) {
        xmlBufferFree(nb);
        return -1;
    }
    memcpy(*psd, nb->content, nb->size + 1);
    xmlBufferFree(nb);
    return 0;
}

/* Performs 2nd step of parsing lyrics from "http://lyricswikia.com". */
static int
parse_lyricswikia(const char *content, char **psd) {
    
    xmlDocPtr doc = NULL;
    if (init_doc_obj(content, HTML, &doc) == -1)
        return -1;
    
    xmlXPathObjectPtr xpath = NULL;
    if (get_xpath_obj(doc, LW_HTML_EXP, &xpath) == -1) {
        xmlFreeDoc(doc);
        return -1;
    }
    xmlNodePtr fstNode = xpath->nodesetval->nodeTab[0];
    char *fst = (char*) xmlNodeGetContent(fstNode);
    if (!fst) {
        xmlXPathFreeObject(xpath);
        xmlFreeDoc(doc);
        return -1;
    }
    *psd = fst;
    
    /* Some tracks on lyricswikia have multiply lyrics,
       so we gonna check this. */
    if (xpath->nodesetval->nodeNr > 1) {
        
        xmlNodePtr sndNode = xpath->nodesetval->nodeTab[1];
        char *snd = (char*) xmlNodeGetContent(sndNode);
        if (snd) {
            /* We got multiply lyrics, concatenating them into one. */
            char *multi = NULL;
            if (concat_lyrics(fst, snd, &multi) == 0) {
                free(fst);
                *psd = multi;
            }
            free(snd);
        }
    }
    xmlXPathFreeObject(xpath);
    xmlFreeDoc(doc);
    return 0;
}

/* Performs 1st step of fetching and parsing lyrics from "http://lyricswikia.com". */
static int
fetch_xml_from_lyricswikia(const char *artist, const char *title, char **xml) {
    
    char *url = NULL;
    if (form_lyr_url(artist, title, LW_URL_TEMP, FALSE, &url) == -1)
        return -1;
    
    char *raw_page = NULL;
    if (retrieve_txt_content(url, &raw_page) == -1) {
        free(url);
        return -1;
    }
    free(url);
    
    char *psd = NULL;
    if (parse_common(raw_page, LW_XML_EXP, XML, &psd) == -1) {
        free(raw_page);
        return -1;
    }
    free(raw_page);
    *xml = psd;
    return 0;
}

/* Fetches lyrics from "http://lyricsmania.com". */
int fetch_lyrics_from_lyricsmania(const char *artist, const char *title, char **lyr) {
    
    char *url = NULL;
    if (form_lyr_url(artist, title, LM_URL_TEMP, TRUE, &url) == -1)
        return -1;
    
    char *raw_page = NULL;
    if (retrieve_txt_content(url, &raw_page) == -1) {
        free(url);
        return -1;
    }
    free(url);
    
    char *psd = NULL;
    if (parse_common(raw_page, LM_EXP, HTML, &psd) == -1) {
        free(raw_page);
        return -1;
    }
    free(raw_page);
    *lyr = psd;
    return 0;
}

/* Fetches lyrics from "http://lyricstime.com". */
int fetch_lyrics_from_lyricstime(const char *artist, const char *title, char **lyr) {
    
    char *url = NULL;
    if (form_lyr_url(artist, title, LT_URL_TEMP, FALSE, &url) == -1)
        return -1;
    
    char *raw_page = NULL;
    if (retrieve_txt_content(url, &raw_page) == -1) {
        free(url);
        return -1;
    }
    free(url);
    
    char *psd = NULL;
    if (parse_common(raw_page, LT_EXP, HTML, &psd) == -1) {
        free(raw_page);
        return -1;
    }
    free(raw_page);
    *lyr = psd;
    return 0;
}

/* Fetches lyrics from "http://megalyrics.ru". */
int fetch_lyrics_from_megalyrics(const char *artist, const char *title, char **lyr) {
    
    char *url = NULL;
    if (form_lyr_url(artist, title, ML_URL_TEMP, FALSE, &url) == -1)
        return -1;
    
    char *raw_page = NULL;
    if (retrieve_txt_content(url, &raw_page) == -1) {
        free(url);
        return -1;
    }
    free(url);
    
    char *psd = NULL;
    if (parse_megalyrics(raw_page, &psd) == -1) {
        free(raw_page);
        return -1;
    }
    free(raw_page);
    *lyr = psd;
    
    char *fmd = NULL;
    if (format_megalyrics(psd, &fmd) == 0) {
        free(psd);
        *lyr = fmd;
    }
    return 0;
}

/* Fetches lyrics from "http://lyrics.wikia.com". */
int fetch_lyrics_from_lyricswikia(const char *artist, const char *title, char **lyr) {
    
    char *xml = NULL;
    if (fetch_xml_from_lyricswikia(artist, title, &xml) == -1)
        return -1;
    
    /* Checking if we got a redirect. Read more about redirects 
     * here: "http://lyrics.wikia.com/Help:Redirect". */
    if (is_redirect(xml)) {
        
        char *rartist = NULL, *rtitle = NULL;
        if (get_redirect_info(xml, &rartist, &rtitle) == 0) {
            
            free(xml);
            /* Retrieving lyrics again, using correct artist name and song title. */
            if (fetch_xml_from_lyricswikia(rartist, rtitle, &xml) == -1) {
                free(rartist);
                free(rtitle);
                return -1;
            }
            free(rartist);
            free(rtitle);
        }
    }
    char *psd = NULL;
    if (parse_lyricswikia(xml, &psd) == -1) {
        free(xml);
        return -1;
    }
    free(xml);
    *lyr = psd;
    return 0;
}

/* Fetches lyrics, using external script. */
int fetch_lyrics_from_script(const char *artist, const char *title, const char *album, char **lyr) {

    deadbeef->conf_lock();
    const char *path = deadbeef->conf_get_str_fast(CONF_LYRICS_SCRIPT_PATH, "");
    
    char *cmd = NULL;
    if (form_script_cmd(artist, title, album, path, SR_CMD_TEMP, &cmd) == -1) {
        deadbeef->conf_unlock();
        return -1;
    }
    deadbeef->conf_unlock();
    
    if (execute_script(cmd, lyr) == -1) {
        free(cmd);
        return -1;
    }
    free(cmd);
    return 0;
}
