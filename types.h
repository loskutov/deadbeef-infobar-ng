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

#ifndef TYPES_HEADER
#define TYPES_HEADER

/* Custom types. */
typedef enum {
    HTML = 0,
    XML = 1,
} ContentType;

typedef enum {
    LYRICS = 0,
    BIO = 1,
} CacheType;

typedef enum {
    NAME = 0,
    MATCH = 1,
    URL = 2,
} SimColumnName;

typedef struct {
    float width;
    float height;
} Res;

typedef struct {
    char *name;
    char *match;
    char *url;
} SimilarInfo;

#endif
