/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#include "wad.hpp"

#include <cstring>

#include <stdexcept>



size_t WAD::lumpidx(std::string name, size_t start) const
{
    for (size_t i = start; i < directory.size(); ++i)
    {
        if (strncasecmp(name.c_str(), directory[i].name, 8) == 0)
        {
            return i;
        }
    }
    throw std::runtime_error(
        "Couldn't find a lump named '" + name + "'");
}

DirEntry const &WAD::findlump(std::string name, size_t start) const
{
    return directory[lumpidx(name, start)];
}

