/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#include "readwad.hpp"
#include "things.hpp"
#include "wad.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <stdexcept>


/* FIXME: TEMP */
//#define wad_DO_PATCH_PGMS
//#define wad_DO_TEXTURE_PGMS



WAD loadIWAD(FILE *f)
{
    WAD wad{};

    char id[5] = {0};
    fread(id, 1, 4, f);

    if (strcmp(id, "IWAD") != 0)
    {
        throw std::runtime_error(
            "Bad IWAD id '"
            + std::string{id}
            + "')");
    }

    /* read the directory */
    uint32_t lump_count = 0,
             directory_pointer = 0;

    fread(&lump_count, 4, 1, f);
    fread(&directory_pointer, 4, 1, f);

    fseek(f, directory_pointer, SEEK_SET);
    for (uint32_t i = 0; i <= lump_count; ++i)
    {
        DirEntry entry{};
        uint32_t offset = 0;

        fread(&offset, 4, 1, f);
        fread(&entry.size, 4, 1, f);
        fread(&entry.name, 8, 1, f);
        entry.name[8] = '\0';

        auto data = new uint8_t[entry.size];
        fseek(f, offset, SEEK_SET);
        fread(data, 1, entry.size, f);
        fseek(f, directory_pointer + (16 * i), SEEK_SET);
        entry.data.reset(data);

        wad.directory.push_back(entry);
    }
    return wad;
}

void patchWAD(WAD &wad, FILE *f)
{
    char id[5] = {0};
    fread(id, 1, 4, f);

    if (strcmp(id, "PWAD") != 0)
    {
        throw std::runtime_error(
            "Bad PWAD id '"
            + std::string{id}
            + "')");
    }

    /* read the directory */
    uint32_t lump_count = 0,
             directory_pointer = 0;

    fread(&lump_count, 4, 1, f);
    fread(&directory_pointer, 4, 1, f);

    size_t level = 0;

    fseek(f, directory_pointer, SEEK_SET);
    for (uint32_t i = 0; i <= lump_count; ++i)
    {
        DirEntry entry{};
        uint32_t offset = 0;

        fread(&offset, 4, 1, f);
        fread(&entry.size, 4, 1, f);
        fread(&entry.name, 8, 1, f);
        entry.name[8] = '\0';

        auto data = new uint8_t[entry.size];
        fseek(f, offset, SEEK_SET);
        fread(data, 1, entry.size, f);
        fseek(f, directory_pointer + (16 * i), SEEK_SET);
        entry.data.reset(data);

        /* if this lump is a level marker,
         * set the directory search offset to here */
        if (   (   entry.name[4] == '\0'
                && entry.name[0] == 'E'
                && entry.name[2] == 'M'
                && isdigit(entry.name[1])
                && isdigit(entry.name[3]))
            || (   entry.name[5] == '\0'
                && strncmp("MAP", entry.name, 3) == 0
                && isdigit(entry.name[3])
                && isdigit(entry.name[4])))
        {
            try
            {
                level = wad.lumpidx(entry.name);
            }
            catch (std::out_of_range &e)
            {
                wad.directory.push_back(entry);
                level = wad.directory.size() - 1;
            }
        }
        else
        {
            try
            {
                /* if this lump is a level lump, add it to the level */
                if (   strcmp(entry.name, "THINGS") == 0
                    || strcmp(entry.name, "LINEDEFS") == 0
                    || strcmp(entry.name, "SIDEDEFS") == 0
                    || strcmp(entry.name, "VERTEXES") == 0
                    || strcmp(entry.name, "SEGS") == 0
                    || strcmp(entry.name, "SSECTORS") == 0
                    || strcmp(entry.name, "NODES") == 0
                    || strcmp(entry.name, "SECTORS") == 0
                    || strcmp(entry.name, "REJECT") == 0
                    || strcmp(entry.name, "BLOCKMAP") == 0)
                {
                    wad.directory[wad.lumpidx(entry.name, level)] =\
                        entry;
                }
                else
                {
                    wad.directory[wad.lumpidx(entry.name)] = entry;
                }
            }
            catch (std::out_of_range &e)
            {
                wad.directory.push_back(entry);
            }
        }
    }
}

void readwad(WAD &wad)
{
    /* load PNAMES */
    DirEntry dir = wad.findlump("PNAMES");
    dir.seek(0, SEEK_SET);

    uint32_t count = 0;
    dir.read(&count, 4);

    for (size_t i = 0; i < count; ++i)
    {
        char name[9];
        name[8] = '\0';
        dir.read(name, 8);
        wad.pnames.push_back(wad.lumpidx(name));
    }


    /* load the palette */
    dir = wad.findlump("PLAYPAL");
    dir.seek(0, SEEK_SET);
    for (size_t i = 0; i < wad.palettes.size(); ++i)
    {
        dir.read(&wad.palettes[i], 768);
    }


    /* load textures */
    auto tds = readtexturedefs(wad, "TEXTURE1");
    try
    {
        for (auto &td : readtexturedefs(wad, "TEXTURE2"))
        {
            tds.push_back(td);
        }
    }
    catch (std::out_of_range &e)
    {
    }
    for (auto &td : tds)
    {
        wad.textures[td.name] = buildtexture(wad, td);
    }


    /* load the flats */
    size_t f_start = wad.lumpidx("F_START"),
           f_end = wad.lumpidx("F_END");
    for (size_t i = f_start + 1; i < f_end; ++i)
    {
        DirEntry &lump = wad.directory[i];
        if (   strcmp(lump.name, "F1_START") == 0
            || strcmp(lump.name, "F1_END") == 0
            || strcmp(lump.name, "F2_START") == 0
            || strcmp(lump.name, "F2_END") == 0
            || strcmp(lump.name, "F3_START") == 0
            || strcmp(lump.name, "F3_END") == 0)
        {
            continue;
        }
        lump.seek(0, SEEK_SET);
        wad.flats[lump.name] = Flat{};
        lump.read(wad.flats[lump.name].data(), 4096);
    }


    /* load the sprites */
    size_t s_start = wad.lumpidx("S_START"),
           s_end = wad.lumpidx("S_END");
    for (size_t i = s_start + 1; i < s_end; ++i)
    {
        DirEntry &lump = wad.directory[i];
        wad.sprites[lump.name] = loadpicture(lump);
    }
}



Level readlevel(std::string level, WAD &wad)
{
    Level out{};
    out.wad = &wad;
    auto const lvlidx = wad.lumpidx(level.c_str());

    /* read THINGS */
    DirEntry dir = wad.findlump("THINGS", lvlidx);
    dir.seek(0, SEEK_SET);

    for (size_t i = 0; i < dir.size / 10; ++i)
    {
        Thing thing{};

        dir.read(&thing.x, 2);
        dir.read(&thing.y, 2);
        dir.read(&thing.angle, 2);
        dir.read(&thing.type, 2);
        dir.read(&thing.options, 2);

        out.things.push_back(thing);
    }


    /* read VERTEXES */
    dir = wad.findlump("VERTEXES", lvlidx);
    dir.seek(0, SEEK_SET);
    for (size_t i = 0; i < dir.size / 4; ++i)
    {
        Vertex vertex{};
        dir.read(&vertex.x, 2);
        dir.read(&vertex.y, 2);
        out.vertices.push_back(vertex);
    }


    /* read SECTORS */
    dir = wad.findlump("SECTORS", lvlidx);
    dir.seek(0, SEEK_SET);
    for (size_t i = 0; i < dir.size / 26; ++i)
    {
        Sector sec{};
        char floorname[9],
             ceilingname[9];
        floorname[8] = ceilingname[8] = '\0';

        dir.read(&sec.floor, 2);
        dir.read(&sec.ceiling, 2);

        dir.read(&floorname, 8);
        dir.read(&ceilingname, 8);

        dir.read(&sec.lightlevel, 2);
        dir.read(&sec.special, 2);
        dir.read(&sec.tag, 2);

        sec.floor_flat = floorname;
        sec.ceiling_flat = ceilingname;

        out.sectors.push_back(sec);
    }


    /* read SIDEDEFS */
    dir = wad.findlump("SIDEDEFS", lvlidx);
    dir.seek(0, SEEK_SET);

    for (size_t i = 0; i < dir.size / 30; ++i)
    {
        Sidedef sd{};
        uint16_t sector_idx = 0;
        char up[9],
             low[9],
             mid[9];
        up[8] = low[8] = mid[8] = '\0';

        dir.read(&sd.x, 2);
        dir.read(&sd.y, 2);
        dir.read(&up, 8);
        dir.read(&low, 8);
        dir.read(&mid, 8);
        dir.read(&sector_idx, 2);

        sd.upper = std::string{up},
        sd.lower = std::string{low},
        sd.middle = std::string{mid};

        sd.sector = &out.sectors[sector_idx];

        out.sidedefs.push_back(sd);
    }


    /* read LINEDEFS */
    dir = wad.findlump("LINEDEFS", lvlidx);
    dir.seek(0, SEEK_SET);

    for (size_t i = 0; i < dir.size / 14; ++i)
    {
        Linedef ld{};
        uint16_t idx = 0;

        dir.read(&idx, 2);
        ld.start = &out.vertices[idx];

        dir.read(&idx, 2);
        ld.end = &out.vertices[idx];

        dir.read(&ld.flags, 2);
        dir.read(&ld.types, 2);
        dir.read(&ld.tag, 2);

        dir.read(&idx, 2);
        ld.right = &out.sidedefs[idx];

        dir.read(&idx, 2);
        ld.left = (idx != 0xFFFF? &out.sidedefs[idx] : nullptr);

        out.linedefs.push_back(ld);
    }


    /* read SEGS */
    dir = wad.findlump("SEGS", lvlidx);
    dir.seek(0, SEEK_SET);

    for (size_t i = 0; i < dir.size / 12; ++i)
    {
        Seg seg{};
        uint16_t startvert = 0,
                 endvert = 0;

        dir.read(&startvert, 2);
        dir.read(&endvert, 2);
        dir.read(&seg.angle, 2);
        dir.read(&seg.linedef, 2);
        dir.read(&seg.direction, 2);
        dir.read(&seg.offset, 2);

        seg.start = &out.vertices[startvert];
        seg.end = &out.vertices[endvert];

        out.segs.push_back(seg);
    }


    /* read SSECTORS */
    dir = wad.findlump("SSECTORS", lvlidx);
    dir.seek(0, SEEK_SET);

    for (size_t i = 0; i < dir.size / 4; ++i)
    {
        SSector ssector{};
        dir.read(&ssector.count, 2);
        dir.read(&ssector.start, 2);
        out.ssectors.push_back(ssector);
    }


    /* read NODES */
    dir = wad.findlump("NODES", lvlidx);
    dir.seek(0, SEEK_SET);

    for (size_t i = 0; i < dir.size / 28; ++i)
    {
        Node node{};

        dir.read(&node.x, 2);
        dir.read(&node.y, 2);
        dir.read(&node.dx, 2);
        dir.read(&node.dy, 2);

        dir.read(&node.right_upper_y, 2);
        dir.read(&node.right_lower_y, 2);
        dir.read(&node.right_lower_x, 2);
        dir.read(&node.right_upper_x, 2);

        dir.read(&node.left_upper_y, 2);
        dir.read(&node.left_lower_y, 2);
        dir.read(&node.left_lower_x, 2);
        dir.read(&node.left_upper_x, 2);

        dir.read(&node.right, 2);
        dir.read(&node.left, 2);

        out.nodes.push_back(node);
    }

    return out;
}



std::vector<TextureDefinition> readtexturedefs(
    WAD &wad,
    char const lumpname[9])
{
    DirEntry dir = wad.findlump(lumpname);
    dir.seek(0, SEEK_SET);

    /* number of texturedefs in the lump */
    uint32_t count = 0;
    dir.read(&count, 4);

    std::vector<TextureDefinition> tds{};
    for (size_t i = 0; i < count; ++i)
    {
        TextureDefinition td{};

        /* get the pointer to the actual texturedef */
        uint32_t ptr = 0;
        dir.seek(4 + (i * 4), SEEK_SET);
        dir.read(&ptr, 4);

        /* read the texturedef data */
        dir.seek(ptr, SEEK_SET);

        uint16_t patchdef_count = 0;

        td.name[8] = '\0';
        dir.read(td.name, 8);
        dir.seek(4L, SEEK_CUR);
        dir.read(&td.width, 2);
        dir.read(&td.height, 2);
        dir.seek(4L, SEEK_CUR);
        dir.read(&patchdef_count, 2);

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

            dir.read(&pd.x, 2);
            dir.read(&pd.y, 2);
            dir.read(&pd.pname_index, 2);
            dir.seek(4, SEEK_CUR);

            td.patchdescs.push_back(pd);
        }
        tds.push_back(td);
    }
    return tds;
}



Picture loadpicture(DirEntry &lump)
{
    lump.seek(0, SEEK_SET);

    Picture pic{};

    lump.read(&pic.width, 2);
    lump.read(&pic.height, 2);
    lump.read(&pic.left, 2);
    lump.read(&pic.top, 2);

    pic.data = std::vector<uint8_t>(pic.width * pic.height, 0);
    pic.opaque = std::vector<bool>(pic.width * pic.height, false);

    auto colptrs = std::vector<uint32_t>(pic.width, 0);
    for (size_t i = 0; i < pic.width; ++i)
    {
        lump.read(&colptrs[i], 4);
    }

    for (size_t x = 0; x < pic.width; ++x)
    {
        lump.seek(colptrs[x], SEEK_SET);

        for (;;)
        {
            uint8_t row = 0,
                    length = 0;

            lump.read(&row, 1);
            if (row == 255)
            {
                break;
            }
            lump.read(&length, 1);
            lump.seek(1, SEEK_CUR);

            for (size_t i = 0; i < length; ++i)
            {
                uint8_t pix = 0;
                lump.read(&pix, 1);

                size_t idx = ((row + i) * pic.width) + x;
                pic.data[idx] = pix;
                pic.opaque[idx] = true;
                
            }
            lump.seek(1, SEEK_CUR);
        }
    }

#ifdef wad_DO_PATCH_PGMS
    {
    printf("%s.pgm\n", lump.name);
    auto name = "patches/" + std::string(lump.name) + ".pgm";
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



Texture buildtexture(WAD &wad, TextureDefinition const &td)
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
        auto pic = loadpicture(
            wad.directory[wad.pnames[pd.pname_index]]);

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

