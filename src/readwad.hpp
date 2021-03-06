/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#ifndef _READWAD_H
#define _READWAD_H

#include "wad.hpp"

#include <cstdio>


/* load a .WAD file from disk */
WAD loadIWAD(FILE *f);
void patchWAD(WAD &wad, FILE *f);

/* read a .WAD file */
void readwad(WAD &wad);

/* read an 'ExMy' lump */
Level readlevel(std::string level, WAD &wad);

/* read a 'TEXTUREx' lump */
std::vector<TextureDefinition> readtexturedefs(
    WAD &wad,
    char const lumpname[9]);

/* load a picture */
Picture loadpicture(DirEntry &lump);

/* flatten patches into a single texture */
Texture buildtexture(WAD &wad, TextureDefinition const &td);


#endif

