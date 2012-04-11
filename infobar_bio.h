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

#ifndef INFOBAR_BIO_HEADER
#define INFOBAR_BIO_HEADER

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <deadbeef/deadbeef.h>

#include "infobar.h"
#include "utils.h"

#define BIO_URL_TEMPLATE "http://ws.audioscrobbler.com/2.0/?method=artist.getinfo&artist=%s&lang=%s&api_key=e5199cf790d46ad475bdda700b0dd6fb"

int fetch_bio_txt(const char *artist, char **txt);

int fetch_bio_image(const char *artist, const char *path);

#endif
