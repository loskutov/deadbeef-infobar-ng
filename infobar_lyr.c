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

#include "infobar_lyr.h"

int form_lyr_url(const char *artist, const char* title, const char* template, char **url) {
    
    int alen = strlen(artist) * 3;
    int tlen = strlen(title) * 3;
    
    char *eartist = calloc(alen + 1, sizeof(char));
    if (!eartist)
        return -1;
    
    char *etitle = calloc(tlen + 1, sizeof(char));
    if (!etitle) {
        free(eartist);
        return -1;
    }
    
    if (uri_encode(eartist, alen, artist, '_') == -1 ||
        uri_encode(etitle, tlen, title, '_') == -1) 
    {
        free(eartist);
        free(etitle);
        return -1;
    }

    if (asprintf(url, template, eartist, etitle) == -1) {
        free(eartist);
        free(etitle);
        return -1;
    }
    free(eartist);
    free(etitle);
    return 0;
}
