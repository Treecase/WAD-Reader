/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#ifndef _READWAD_H
#define _READWAD_H

#include "wad.hpp"

#include <cstdio>


/* read a .WAD file */
WAD readwad(FILE *wad);

/* read an 'ExMy' lump */
Level readlevel(std::string level, FILE *f, WAD &wad);

/* read a 'TEXTUREx' lump */
std::vector<TextureDefinition> readtexturedefs(
    FILE *f,
    WAD const &wad,
    char const lumpname[9]);

/* load a picture */
Picture loadpicture(FILE *f, DirEntry const &lump);

/* flatten patches into a single texture */
Texture buildtexture(
    FILE *f,
    WAD const &wad,
    TextureDefinition const &td);


#endif

