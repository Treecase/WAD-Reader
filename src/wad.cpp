/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#include "wad.hpp"

#include <cstring>

#include <stdexcept>



void DirEntry::read(void *ptr, size_t byte_count)
{
    if (idx >= size)
    {
        throw std::out_of_range{
            "DirEntry(\""
            + std::string{name}
            + "\")::read -- "
            + std::to_string(idx)
            + "/"
            + std::to_string(size)};
    }
    memcpy(ptr, data.get() + idx, byte_count);
    idx += byte_count;
}

void DirEntry::seek(ssize_t offset, int whence)
{
    switch (whence)
    {
    case SEEK_SET:
        idx = offset;
        break;
    case SEEK_CUR:
        idx += offset;
        break;
    case SEEK_END:
        idx = (size - 1) - offset;
        break;
    default:
        throw std::runtime_error{"bad 'whence' value"};
        break;
    }
}



size_t WAD::lumpidx(std::string name, size_t start) const
{
    for (size_t i = start; i < directory.size(); ++i)
    {
        if (strncasecmp(name.c_str(), directory[i].name, 8) == 0)
        {
            return i;
        }
    }
    throw std::out_of_range{
        "Couldn't find a lump named '" + name + "'"};
}

DirEntry &WAD::findlump(std::string name, size_t start)
{
    return directory[lumpidx(name, start)];
}

