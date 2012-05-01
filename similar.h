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

#include <stdio.h>
#include <deadbeef/deadbeef.h>

#include "utils.h"

/* URL template to access similar artists on lastfm. */
#define SIM_URL_TEMPLATE "http://ws.audioscrobbler.com/2.0/?method=artist.getsimilar&artist=%s&limit=%d&api_key=e5199cf790d46ad475bdda700b0dd6fb"

/* XPath expressions. */
#define SIM_EXP "/lfm/similarartists/artist"

/* Creates an empty list of similar artists  with "Loading..." status. */
int new_sim_list(char ***list);

/* Frees list of similar artists */
void free_sim_list(char **ptr);

/* Fetches the list of similar artists from lastfm. */
int fetch_similar_artists(const char *artist, char ***artists);
