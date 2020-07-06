/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#include "wad.hpp"

#include <cstring>

#include <stdexcept>



size_t WAD::lumpidx(char const *name, size_t start) const
{
    for (size_t i = start; i < directory.size(); ++i)
    {
        if (strncasecmp(name, directory[i].name, 8) == 0)
        {
            return i;
        }
    }
    throw std::runtime_error(
        "Couldn't find a lump named '"
        + std::string(name)
        + "'");
}

DirEntry const &WAD::findlump(char const *name, size_t start) const
{
    return directory[lumpidx(name, start)];
}

