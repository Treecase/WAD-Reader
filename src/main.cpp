/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#include "readwad.hpp"

#include <cstdlib>
#include <cerrno>
#include <cstring>



int main(int argc, char *argv[])
{
    if (argc <= 1)
    {
        fprintf(stderr, "No .WAD given!\n");
        exit(EXIT_FAILURE);
    }

    char const *filename = argv[1];

    FILE *wadfile = fopen(filename, "r");
    if (wadfile == nullptr)
    {
        fprintf(stderr, "Failed to open %s -- (%s)\n",
            filename,
            strerror(errno));
        exit(EXIT_FAILURE);
    }

    auto wad = readwad(wadfile);
    readlevel(wadfile, wad);

    fclose(wadfile);

    return EXIT_SUCCESS;
}

