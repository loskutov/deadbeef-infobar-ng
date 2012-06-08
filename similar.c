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

#include "similar.h"

/* Parses XML from lastfm and forms list of similar artists. */
static int
parse_similar(const char *content, SimilarInfo **similar, int *size) {
    
    xmlDocPtr doc = NULL;
    if (init_doc_obj(content, XML, &doc) == -1)
        return -1;
    
    xmlXPathObjectPtr xpath = NULL;
    if (get_xpath_obj(doc, SIM_EXP, &xpath) == -1) {
        xmlFreeDoc(doc);
        return -1;
    }
    xmlNodeSetPtr nodeSet = xpath->nodesetval;
    if (nodeSet->nodeNr == 0) {
        xmlXPathFreeObject(xpath);
        xmlFreeDoc(doc);
        return -1;
    }
    
    *similar = calloc(nodeSet->nodeNr, sizeof(SimilarInfo));
    if (!*similar) {
        xmlXPathFreeObject(xpath);
        xmlFreeDoc(doc);
        return -1;
    }
    
    for (int i = 0; i < nodeSet->nodeNr; ++i) {
        xmlNodePtr root = nodeSet->nodeTab[i];
        
        xmlNodePtr child = root->children;
        for (; child; child = child->next) {
           
            if (child->type == XML_ELEMENT_NODE) {
                
                if (xmlStrcasecmp(child->name, (xmlChar*) "name") == 0) {
                    (*similar)[i].name = (char*) xmlNodeGetContent(child);
                }
                
                if (xmlStrcasecmp(child->name, (xmlChar*) "match") == 0) {
                    (*similar)[i].match = (char*) xmlNodeGetContent(child);
                }
            } 
        }
    }
    *size = nodeSet->nodeNr;
    xmlXPathFreeObject(xpath);
    xmlFreeDoc(doc);
    return 0;
}


/* Forms an URL, which is used to retrieve the list of similar artists. */
static int
form_similar_url(const char *artist, char **url, int limit) {
    
    char *eartist = NULL;
    if (encode_artist(artist, &eartist, '+') == -1)
        return -1;
    
    if (asprintf(url, SIM_URL_TEMPLATE, eartist, limit) == -1) {
        free(eartist);
        return -1;
    }
    free(eartist);
    return 0;
} 

/* Frees list of similar artists */
void free_sim_list(SimilarInfo *similar, int size) {
    
    for (int i = 0; i < size; ++i) {
        if (similar[i].name) 
            free(similar[i].name);

        if (similar[i].match) 
            free(similar[i].match);
    }
    free(similar);
}

/* Fetches the list of similar artists from lastfm. */
int fetch_similar_artists(const char *artist, SimilarInfo **similar, int *size) {
    
    int limit = deadbeef->conf_get_int(CONF_SIM_MAX_ARTISTS, 10);
    
    char *url = NULL;
    if (form_similar_url(artist, &url, limit) == -1)
        return -1;
    
    char *raw_page = NULL;
    if (retrieve_txt_content(url, &raw_page) == -1) {
        free(url);
        return -1;
    }
    free(url);
    
    if (parse_similar(raw_page, similar, size) == -1) {
        free(raw_page);
        return -1;
    }
    free(raw_page);
    return 0;
}
