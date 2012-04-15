#!/bin/bash

# Infobar plugin for DeaDBeeF music player
# Copyright (C) 2011-2012 Dmitriy Simbiriatin <dmitriy.simbiriatin@gmail.com>

# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

# This an example lyrics script which fetches lyrics from http://lyrics.com.

# Encoded artist name.
ARTIST=$1

# Encoded song title.
TITLE=$2

# By default, plugin encodes spaces using '_' character, you can use sed to
# replace them with the character you need.

# Lyrics.com uses '-' to encode the spaces, so we gonna replace default '_' 
# characters with them.
ARTIST=`echo $ARTIST | sed 's/_/-/g'`

# Same as for artist name.
TITLE=`echo $TITLE | sed 's/_/-/g'`

# XPath expression to find the lyrics on the page.
EXP="//div[@id=\"lyric_space\"]"

# URL template.
URL_TEMP="http://www.lyrics.com/$TITLE-lyrics-$ARTIST.html"

# I'm using xml_grep utility from "xml twig" package here (you can easily find 
# this package in your favorite distro) to parse html page, and "w3m" to form
# pretty-looking lyrics text. 
xml_grep -html $EXP $URL_TEMP | w3m -dump -T text/html;

exit 0;
