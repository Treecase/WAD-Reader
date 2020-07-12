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
//#define wad_DO_PATCH_PGMS
//#define wad_DO_TEXTURE_PGMS



WAD readwad(FILE *f)
{
    WAD wad{};

    char id[5] = {0};
    fread(id, 1, 4, f);

    wad.iwad = !strcmp(id, "IWAD");
    if (!wad.iwad && strcmp(id, "PWAD") != 0)
    {
        throw std::runtime_error(
            "WAD id string must be 'IWAD' or 'PWAD'! (got '"
            + std::string(id)
            + "')");
    }


    /* read the directory */
    uint32_t lump_count = 0,
             directory_pointer = 0;

    fread(&lump_count, 4, 1, f);
    fread(&directory_pointer, 4, 1, f);

    fseek(f, directory_pointer, SEEK_SET);
    for (uint32_t i = 0; i < lump_count; ++i)
    {
        DirEntry entry{};
        entry.name[8] = '\0';

        fread(&entry.offset, 4, 1, f);
        fread(&entry.size, 4, 1, f);
        fread(&entry.name, 8, 1, f);

        wad.directory.push_back(entry);
    }

    /* load PNAMES */
    auto dir = wad.findlump("PNAMES");
    fseek(f, dir.offset, SEEK_SET);

    uint32_t count = 0;
    fread(&count, 4, 1, f);

    for (size_t i = 0; i < count; ++i)
    {
        char name[9] = {0};
        fread(name, 8, 1, f);
        wad.pnames.push_back(wad.lumpidx(name));
    }


    /* load the palette */
    dir = wad.findlump("PLAYPAL");
    fseek(f, dir.offset, SEEK_SET);

    uint8_t pal8[256][3];
    fread(pal8, 3, 256, f);
    for (size_t i = 0; i < 256; ++i)
    {
        wad.palette[i] =\
            pal8[i][0]
            | (pal8[i][1] <<  8)
            | (pal8[i][2] << 16)
            | 0xFF000000;
    }


    /* load textures */
    auto tds = readtexturedefs(f, wad, "TEXTURE1");
    for (auto &td : readtexturedefs(f, wad, "TEXTURE2"))
    {
        tds.push_back(td);
    }

    for (auto &td : tds)
    {
        wad.textures[std::string(td.name)] =\
            buildtexture(f, wad, td);
    }


    /* load the flats */
    size_t f_start = wad.lumpidx("F_START"),
           f_end = wad.lumpidx("F_END");
    for (size_t i = f_start + 1; i < f_end; ++i)
    {
        auto &lump = wad.directory[i];
        fseek(f, lump.offset, SEEK_SET);

        wad.flats[wad.directory[i].name] = Flat{};
        fread(wad.flats[wad.directory[i].name].data(), 1, 4096, f);
    }



    return wad;
}



Level readlevel(char const *level, FILE *f, WAD &wad)
{
    Level out{};
    out.wad = &wad;
    auto const lvlidx = wad.lumpidx(level);

    /* read THINGS */
    auto dir = wad.findlump("THINGS", lvlidx);
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


    /* read VERTEXES */
    dir = wad.findlump("VERTEXES", lvlidx);
    fseek(f, dir.offset, SEEK_SET);
    for (size_t i = 0; i < dir.size / 4; ++i)
    {
        Vertex vertex{};
        fread(&vertex.x, 2, 1, f);
        fread(&vertex.y, 2, 1, f);
        out.vertices.push_back(vertex);
    }


    /* read flats */
    size_t begin = wad.lumpidx("F_START", lvlidx),
           end = wad.lumpidx("F_END", lvlidx);
    fseek(f, wad.directory[begin].offset, SEEK_SET);
    for (size_t i = begin; i < end; i++)
    {
        std::string name = wad.directory[i].name;
        fread(out.flats[name].data(), 1, 4096, f);
    }


    /* read SECTORS */
    dir = wad.findlump("SECTORS", lvlidx);
    fseek(f, dir.offset, SEEK_SET);
    for (size_t i = 0; i < dir.size / 26; ++i)
    {
        Sector sec{};
        char floorname[9],
             ceilingname[9];
        floorname[8] = ceilingname[8] = '\0';

        fread(&sec.floor, 2, 1, f);
        fread(&sec.ceiling, 2, 1, f);

        fread(&floorname, 1, 8, f);
        fread(&ceilingname, 1, 8, f);

        fread(&sec.lightlevel, 2, 1, f);
        fread(&sec.special, 2, 1, f);
        fread(&sec.tag, 2, 1, f);

        sec.floor_flat = floorname;
        sec.ceiling_flat = ceilingname;

        out.sectors.push_back(sec);
    }


    /* read SIDEDEFS */
    dir = wad.findlump("SIDEDEFS", lvlidx);
    fseek(f, dir.offset, SEEK_SET);

    for (size_t i = 0; i < dir.size / 30; ++i)
    {
        Sidedef sd{};
        uint16_t sector_idx = 0;
        char up[9] = {0},
             low[9] = {0},
             mid[9] = {0};

        fread(&sd.x, 2, 1, f);
        fread(&sd.y, 2, 1, f);
        fread(&up, 1, 8, f);
        fread(&low, 1, 8, f);
        fread(&mid, 1, 8, f);
        fread(&sector_idx, 2, 1, f);

        sd.upper = std::string{up},
        sd.lower = std::string{low},
        sd.middle = std::string{mid};

        sd.sector = &out.sectors[sector_idx];

        out.sidedefs.push_back(sd);
    }


    /* read LINEDEFS */
    dir = wad.findlump("LINEDEFS", lvlidx);
    fseek(f, dir.offset, SEEK_SET);

    for (size_t i = 0; i < dir.size / 14; ++i)
    {
        Linedef ld{};

        uint16_t idx = 0;
        fread(&idx, 2, 1, f);
        ld.start = &out.vertices[idx];

        fread(&idx, 2, 1, f);
        ld.end = &out.vertices[idx];

        fread(&ld.flags, 2, 1, f);
        fread(&ld.types, 2, 1, f);
        fread(&ld.tag, 2, 1, f);

        fread(&idx, 2, 1, f);
        ld.right = &out.sidedefs[idx];

        fread(&idx, 2, 1, f);
        ld.left = (idx != 0xFFFF? &out.sidedefs[idx] : nullptr);

        out.linedefs.push_back(ld);
    }


    /* read SEGS */
    dir = wad.findlump("SEGS", lvlidx);
    fseek(f, dir.offset, SEEK_SET);

    for (size_t i = 0; i < dir.size / 12; ++i)
    {
        Seg seg{};

        uint16_t startvert = 0,
                 endvert = 0,
                 linedef = 0;

        fread(&startvert, 2, 1, f);
        fread(&endvert, 2, 1, f);
        fread(&seg.angle, 2, 1, f);
        fread(&seg.linedef, 2, 1, f);
        fread(&seg.direction, 2, 1, f);
        fread(&seg.offset, 2, 1, f);

        seg.start = &out.vertices[startvert];
        seg.end = &out.vertices[endvert];

        out.segs.push_back(seg);
    }


    /* read SSECTORS */
    dir = wad.findlump("SSECTORS", lvlidx);
    fseek(f, dir.offset, SEEK_SET);

    for (size_t i = 0; i < dir.size / 4; ++i)
    {
        SSector ssector{};
        fread(&ssector.count, 2, 1, f);
        fread(&ssector.start, 2, 1, f);
        out.ssectors.push_back(ssector);
    }


    /* read NODES */
    dir = wad.findlump("NODES", lvlidx);
    fseek(f, dir.offset, SEEK_SET);

    for (size_t i = 0; i < dir.size / 28; ++i)
    {
        Node node{};

        fread(&node.x, 2, 1, f);
        fread(&node.y, 2, 1, f);
        fread(&node.dx, 2, 1, f);
        fread(&node.dy, 2, 1, f);

        fread(&node.right_upper_y, 2, 1, f);
        fread(&node.right_lower_y, 2, 1, f);
        fread(&node.right_lower_x, 2, 1, f);
        fread(&node.right_upper_x, 2, 1, f);

        fread(&node.left_upper_y, 2, 1, f);
        fread(&node.left_lower_y, 2, 1, f);
        fread(&node.left_lower_x, 2, 1, f);
        fread(&node.left_upper_x, 2, 1, f);

        fread(&node.right, 2, 1, f);
        fread(&node.left, 2, 1, f);

        out.nodes.push_back(node);
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

    /* number of texturedefs in the lump */
    uint32_t count = 0;
    fread(&count, 4, 1, f);

    std::vector<TextureDefinition> tds{};
    for (size_t i = 0; i < count; ++i)
    {
        TextureDefinition td{};

        /* get the pointer to the actual texturedef */
        uint32_t ptr = 0;
        fseek(f, dir.offset + 4 + (i * 4), SEEK_SET);
        fread(&ptr, 4, 1, f);

        /* read the texturedef data */
        fseek(f, dir.offset + ptr, SEEK_SET);

        uint16_t patchdef_count = 0;

        td.name[8] = '\0';
        fread(td.name, 8, 1, f);
        fseek(f, 4L, SEEK_CUR);
        fread(&td.width, 2, 1, f);
        fread(&td.height, 2, 1, f);
        fseek(f, 4L, SEEK_CUR);
        fread(&patchdef_count, 2, 1, f);

#ifdef wad_DO_PATCH_PGMS
        printf("%s:%dx%d,%d patches\n",
            td.name,
            td.width,
            td.height,
            patchdef_count);
#endif

        /* read all the texturedef's patchdefs */
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
    PatchDescriptor const &pd)
{
    auto patch = wad.directory[wad.pnames[pd.pname_index]];
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
    TextureDefinition const &td)
{
#ifdef wad_DO_TEXTURE_PGMS
    printf("build %s\n", td.name);
#endif

    Texture tex{};
    tex.width = td.width;
    tex.height = td.height;
    tex.data = std::vector<uint8_t>(tex.width * tex.height, 0);
    tex.opaque = std::vector<bool>(tex.width * tex.height, false);

    for (auto &pd : td.patchdescs)
    {
        auto pic = loadpicture(f, wad, pd);

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

