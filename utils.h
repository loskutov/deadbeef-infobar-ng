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

#ifndef UTILS_HEADER
#define UTILS_HEADER

#include <stdio.h>
#include <stdlib.h>

#include "infobar.h"
#include "types.h"

gboolean is_exists(const char *obj);

gboolean is_redirect(const char *str);

gboolean is_old_cache(const char *cache_file, CacheType type);

int create_dir(const char *dir, mode_t mode);

int load_txt_file(const char *file, char **content);

int save_txt_file(const char *file, const char *content);

int convert_to_utf8(const char *str, char **str_utf8);

int get_redirect_info(const char *str, char **artist, char **title);

int get_cache_path(char **path, ContentType type);

int uri_encode(char *out, int outl, const char *str, char space);

void find_new_resolution(float ww, float wh, float aw, float ah, Res *res);

#endif
