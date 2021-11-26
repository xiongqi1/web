/*

RSS 0.91, 0.92 and 2.0

Retrieve RSS feeds.

--------------------------------------------------------------------------------
gSOAP XML Web services tools
Copyright (C) 2001-2004, Robert van Engelen, Genivia, Inc. All Rights Reserved.
This software is released under one of the following two licenses:
GPL or Genivia's license for commercial use.
--------------------------------------------------------------------------------
GPL license.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

Author contact information:
engelen@genivia.com / engelen@acm.org
--------------------------------------------------------------------------------
A commercial use license is available from Genivia, Inc., contact@genivia.com
--------------------------------------------------------------------------------
*/

//gsoap dc schema namespace: http://purl.org/dc/elements/1.1/

typedef char *XML;	// mixed content

struct channel
{ char *title;
  char *link;
  char *language;
  char *copyright;
  XML description;	// description may contain XHTML that we want to preserve
  struct image *image;
  int __size;
  struct item *item;
  time_t *dc__date;	// RSS 2.0 dc schema element
};

struct item
{ char *title;
  char *link;
  XML description;	// description may contain XHTML that we want to preserve
  char *pubDate;
  time_t *dc__date;	// RSS 2.0 dc schema element
};

struct image
{ char *title;
  char *url;
  char *link;
  int width  0:1 = 0;	// optional, default value = 0
  int height 0:1 = 0;	// optional, default value = 0
  XML description;	// description may contain XHTML that we want to preserve
};

struct rss
{ @char *version = "2.0";
  struct channel channel;
};
