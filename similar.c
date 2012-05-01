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
parse_similar(const char *content, char ***artists) {
    
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
    
    *artists = calloc(nodeSet->nodeNr, sizeof(char*));
    if (!*artists) {
        xmlXPathFreeObject(xpath);
        xmlFreeDoc(doc);
        return -1;
    }
    
    for (int i = 0; i < nodeSet->nodeNr; ++i) {
        /* Looping through the "artist" nodes. */
        xmlNodePtr node = nodeSet->nodeTab[i];
        xmlNodePtr child = node->children;
        
        for (; child; child = child->next){
            /* Looking for "name" tag. */
            if (child->type == XML_ELEMENT_NODE && 
                xmlStrcasecmp(child->name, (xmlChar*) "name") == 0) 
            {
                *(*artists + i) = (char*) xmlNodeGetContent(child);
                break;
            }
        }
    }
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

/* Creates an empty list of similar artists with "Loading..." status. */
int new_sim_list(char ***list) {
    
    *list = calloc(1, sizeof(char*));
    if (!*list)
        return -1;
    
    *(*list) = "Loading...";
    return 0;
}

/* Frees list of similar artists */
void free_sim_list(char **list) {
    
    int elem = 0;
    for(; *list; ++list) 
        ++elem;
    
    list-=elem;
    for (int i = 0; i < elem; ++i)
        if (list[i]) free(list[i]);
    free(list);
}

/* Fetches the list of similar artists from lastfm. */
int fetch_similar_artists(const char *artist, char ***artists) {
    
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
    
    if (parse_similar(raw_page, artists) == -1) {
        free(raw_page);
        return -1;
    }
    free(raw_page);
    return 0;
}
