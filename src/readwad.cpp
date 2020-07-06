/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#include "readwad.hpp"
#include "wad.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <stdexcept>


/* FIXME: TEMP */
#define wad_DO_PATCH_PGMS
#define wad_DO_TEXTURE_PGMS



WAD readwad(FILE *wad)
{
    WAD out{};

    char id[5] = {0,0,0,0,0};
    fread(id, 1, 4, wad);

    out.iwad = !strcmp(id, "IWAD");
    if (!out.iwad && strcmp(id, "PWAD") != 0)
    {
        throw std::runtime_error(
            "WAD id string must be 'IWAD' or 'PWAD'! (got '"
            + std::string(id)
            + "')");
    }


    /* read the directory */
    uint32_t lump_count = 0,
             directory_pointer = 0;

    fread(&lump_count, 4, 1, wad);
    fread(&directory_pointer, 4, 1, wad);

    fseek(wad, directory_pointer, SEEK_SET);
    for (uint32_t i = 0; i < lump_count; ++i)
    {
        DirEntry entry{};

        fread(&entry.offset, 4, 1, wad);
        fread(&entry.size, 4, 1, wad);

        fread(&entry.name, 8, 1, wad);
        entry.name[8] = '\0';

        out.directory.push_back(entry);
    }

    return out;
}



Level readlevel(FILE *f, WAD const &wad)
{
    /* load PNAMES */
    auto dir = wad.findlump("PNAMES");
    fseek(f, dir.offset, SEEK_SET);

    uint32_t count = 0;
    fread(&count, 4, 1, f);

    std::vector<size_t> pnames{};
    for (size_t i = 0; i < count; ++i)
    {
        char name[9];
        name[8] = '\0';
        fread(name, 8, 1, f);
        pnames.push_back(wad.lumpidx(name));
    }


    /* load texture definitions */
    auto tds = readtexturedefs(f, wad, "TEXTURE1");
    for (auto &td : readtexturedefs(f, wad, "TEXTURE2"))
    {
        tds.push_back(td);
    }


    /* load the palette */
    dir = wad.findlump("PLAYPAL");
    fseek(f, dir.offset, SEEK_SET);
    Palette palette{};

    uint8_t pal8[256][3];
    fread(pal8, 3, 256, f);
    for (size_t i = 0; i < 256; ++i)
    {
        palette[i] =\
            pal8[i][0]
            | (pal8[i][1] <<  8)
            | (pal8[i][2] << 16)
            | 0xFF000000;
    }


    /* build the textures */
    std::vector<Texture> textures{};
    for (auto &td : tds)
    {
        textures.push_back(buildtexture(f, wad, pnames, td));
    }


    Level out{};
    auto lvlidx = wad.lumpidx("E1M1");

    /* read THINGS */
    dir = wad.findlump("THINGS", lvlidx);
    fseek(f, dir.offset, SEEK_SET);

    for (size_t i = 0; i < dir.size / 10; ++i)
    {
        Thing thing{};

        fread(&thing.x, 2, 1, f);
        fread(&thing.y, 2, 1, f);
        fread(&thing.angle, 2, 1, f);
        fread(&thing.type, 2, 1, f);
        fread(&thing.options, 2, 1, f);

        out.things.push_back(thing);
    }


    /* TODO: vertices, sidedefs */
    std::vector<Vertex> vertices;
    std::vector<Sidedef> sidedefs;


    /* read LINEDEFS */
    dir = wad.findlump("LINEDEFS", lvlidx);
    fseek(f, dir.offset, SEEK_SET);

    for (size_t i = 0; i < dir.size / 14; ++i)
    {
        Linedef ld{};

        uint16_t idx = 0;
        fread(&idx, 2, 1, f);
        ld.start = &vertices[idx];

        fread(&idx, 2, 1, f);
        ld.end = &vertices[idx];

        fread(&ld.flags, 2, 1, f);
        fread(&ld.types, 2, 1, f);
        fread(&ld.tag, 2, 1, f);

        fread(&idx, 2, 1, f);
        ld.right = &sidedefs[idx];

        fread(&idx, 2, 1, f);
        ld.left = (idx != 0xFFFF? &sidedefs[idx] : nullptr);

        /* TODO: do something with this */
    }

    return out;
}



std::vector<TextureDefinition> readtexturedefs(
    FILE *f,
    WAD const &wad,
    char const lumpname[9])
{
    auto dir = wad.findlump(lumpname);
    fseek(f, dir.offset, SEEK_SET);

    uint32_t count = 0;
    fread(&count, 4, 1, f);

    std::vector<TextureDefinition> tds{};
    for (size_t i = 0; i < count; ++i)
    {
        TextureDefinition td{};

        fseek(f, dir.offset + 4 + (i * 4), SEEK_SET);

        uint32_t ptr = 0;
        fread(&ptr, 4, 1, f);

        fseek(f, dir.offset + ptr, SEEK_SET);

        td.name[8] = '\0';
        uint16_t patchdef_count = 0;

        fread(td.name, 8, 1, f);
        fseek(f, 4L, SEEK_CUR);
        fread(&td.width, 2, 1, f);
        fread(&td.height, 2, 1, f);
        fseek(f, 4L, SEEK_CUR);
        fread(&patchdef_count, 2, 1, f);

        printf("%s:%dx%d,%d patches\n",
            td.name,
            td.width,
            td.height,
            patchdef_count);

        for (size_t j = 0; j < patchdef_count; ++j)
        {
            PatchDescriptor pd{};

            fread(&pd.x, 2, 1, f);
            fread(&pd.y, 2, 1, f);
            fread(&pd.pname_index, 2, 1, f);
            fseek(f, 4L, SEEK_CUR);

            td.patchdescs.push_back(pd);
        }
        tds.push_back(td);
    }
    return tds;
}



Picture loadpicture(
    FILE *f,
    WAD const &wad,
    std::vector<size_t> const &pnames,
    PatchDescriptor const &pd)
{
    auto patch = wad.directory[pnames[pd.pname_index]];
    fseek(f, patch.offset, SEEK_SET);

    Picture pic{};

    fread(&pic.width, 2, 1, f);
    fread(&pic.height, 2, 1, f);
    fread(&pic.left, 2, 1, f);
    fread(&pic.top, 2, 1, f);

    pic.data = std::vector<uint8_t>(pic.width * pic.height, 0);
    pic.opaque = std::vector<bool>(pic.width * pic.height, false);

    auto colptrs = std::vector<uint32_t>(pic.width, 0);
    for (size_t i = 0; i < pic.width; ++i)
    {
        fread(&colptrs[i], 4, 1, f);
    }

    for (size_t x = 0; x < pic.width; ++x)
    {
        fseek(f, patch.offset + colptrs[x], SEEK_SET);

        for (;;)
        {
            uint8_t row = 0,
                    length = 0;

            fread(&row, 1, 1, f);
            if (row == 255)
            {
                break;
            }
            fread(&length, 1, 1, f);
            fseek(f, 1L, SEEK_CUR);

            for (size_t i = 0; i < length; ++i)
            {
                uint8_t pix = 0;
                fread(&pix, 1, 1, f);

                size_t idx = ((row + i) * pic.width) + x;
                pic.data[idx] = pix;
                pic.opaque[idx] = true;
                
            }
            fseek(f, 1L, SEEK_CUR);
        }
    }

#ifdef wad_DO_PATCH_PGMS
    {
    printf("%s.pgm\n", patch.name);
    auto name = "patches/" + std::string(patch.name) + ".pgm";
    FILE *pgm = fopen(name.c_str(), "w");
    if (pgm != nullptr)
    {
        fprintf(pgm, "P5\n%d %d\n255\n", pic.width, pic.height);
        fwrite(pic.data.data(), 1, pic.width * pic.height, pgm);
        fclose(pgm);
    }
    else
    {
        fprintf(stderr, "Can't open '%s' (%s)\n",
            name.c_str(),
            strerror(errno));
    }
    }
#endif
    return pic;
}



Texture buildtexture(
    FILE *f,
    WAD const &wad,
    std::vector<size_t> const &pnames,
    TextureDefinition const &td)
{
    printf("build %s\n", td.name);

    Texture tex{};
    tex.width = td.width;
    tex.height = td.height;
    tex.data = std::vector<uint8_t>(tex.width * tex.height, 0);
    tex.opaque = std::vector<bool>(tex.width * tex.height, false);

    for (auto &pd : td.patchdescs)
    {
        auto pic = loadpicture(f, wad, pnames, pd);

        for (size_t y = 0; y < pic.height; ++y)
        {
            for (size_t x = 0; x < pic.width; ++x)
            {
                if (pd.y + y >= td.height || pd.x + x >= td.width)
                {
                    continue;
                }
                size_t picidx = (y * pic.width) + x;
                size_t texidx = ((pd.y + y) * td.width) + pd.x + x;
                if (pic.opaque[picidx])
                {
                    tex.data[texidx] = pic.data[picidx];
                    tex.opaque[texidx] = true;
                }
            }
        }
    }

#ifdef wad_DO_TEXTURE_PGMS
    {
    printf("%s.pgm\n", td.name);
    auto name = "patches/" + std::string(td.name) + ".pgm";
    FILE *pgm = fopen(name.c_str(), "w");
    if (pgm != nullptr)
    {
        fprintf(pgm, "P5\n%d %d\n255\n", td.width, td.height);
        fwrite(tex.data.data(), 1, td.width * td.height, pgm);
        fclose(pgm);
    }
    else
    {
        fprintf(stderr, "Can't open '%s' (%s)\n",
            name.c_str(),
            strerror(errno));
    }
    }
#endif

    return tex;
}
