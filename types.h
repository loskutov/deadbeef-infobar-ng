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
    HTML = 1,
    XML = 2,
} ContentType;

typedef enum {
    LYRICS = 1,
    BIO = 2,
} CacheType;

typedef struct {
    float width;
    float height;
} Res;

typedef struct {
    char *name;
    char *match;
} SimilarInfo;

#endif
