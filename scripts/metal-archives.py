#!/usr/bin/python
# -*- coding: utf-8 -*-

# Script to fetch lyrics from http://metal-archives.com provided by Psych218.

from sys import argv, exit
from urllib.request import urlopen
from urllib.parse import quote

base = "http://www.metal-archives.com"
artist = quote(argv[1].replace("_", "+"), safe="+%,.")
title = quote(argv[2].replace("_", "+"), safe="+%,.")

# Searching for the song
template = "{0}/search/ajax-advanced/searching/songs/?songTitle={2}&bandName={1}"
url = template.format(base, artist, title)
try:
    usock = urlopen(url)
    data = str(usock.read())
    usock.close()
except Exception as e:
    exit(1)

# Get ID of song
lyrics_id = data.split("lyricsLink_")[1].split("\\")[0]

# Fetch Lyrics
url = "{0}/release/ajax-view-lyrics/id/{1}".format(base, lyrics_id)
try:
    usock = urlopen(url)
    data = usock.read()
    usock.close()
except Exception as e:
    exit(1)

# Output Lyrics w/o html tags
content = data.decode()
lyrics = content.replace("\n","").replace("\t","").replace("\r","").replace("<br />","\n")

if lyrics == "<em>(lyrics not available)</em>":
    exit(1)
    
print(lyrics)
exit(0)
