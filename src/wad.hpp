/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#ifndef _WAD_H
#define _WAD_H

#include <cstdint>

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>



/* pixel format: xxBBGGRR, xx=unused */
typedef uint8_t Palette[256 * 3];



/* 64x64 indexed color */
typedef std::array<uint8_t, 4096> Flat;



/* see [4-10] */
typedef char *Reject;



struct PatchDescriptor
{
    int16_t x, y;
    uint16_t pname_index;
};

struct TextureDefinition
{
    char name[9];
    uint16_t width, height;
    std::vector<PatchDescriptor> patchdescs;
};



struct Texture
{
    size_t width, height;
    std::vector<uint8_t> data;
    std::vector<bool> opaque;
};



struct Thing
{
    int16_t x, y;
    /* 0 = east, 90 = north */
    int16_t angle;
    /* see ThingTypeData */
    uint16_t type;
    /* see ThingOptions */
    uint16_t options;
};

/* see [4-2-1] and [4-2-2] */
struct ThingTypeData
{
    int radius,
        height,
        mass,
        health,
        speed;
    char sprite[4];
    int animation_frame_count;
    bool cleanloop, /* animation goes 1,2,3,2,1,2,...
                     * rather than 1,2,3,1,2,3,... */
         hurtable,
         monster,   /* counts towards kill % */
         solid,
         hanging,
         pickup,
         artifact;  /* counts toward item % */
};

/* see [4-2-3] */
enum ThingOptions
{
    /* appears on skill levels 1 and 2 */
    SKILL12 = 1 << 0,
    /* appears on skill level 3 */
    SKILL3  = 1 << 1,
    /* appears on skill levels 4 and 5 */
    SKILL45 = 1 << 2,
    /* monster isn't activated by hearing sounds */
    DEAF    = 1 << 3,
    /* only appears in multiplayer */
    MP_ONLY = 1 << 4
};



struct Vertex
{
    int16_t x, y;
};



struct Sector
{
    /* floor/ceiling heights */
    int16_t floor, ceiling;
    std::string floor_flat, ceiling_flat;
    /* 00=black, FF=white
     * (this number is divided by 8 ie. 0 through 7 are the
     * same, 8 through 15 are the same, etc.) */
    uint16_t lightlevel;
    /* see [4-9-1] */
    uint16_t special;
    /* see LINEDEF */
    uint16_t tag;
};



struct Sidedef
{
    /* how many pixels horizontal/vertically
     * to move before pasting the texture */
    int16_t x, y;
    /* the upper, lower, and middle texture names */
    std::string upper, lower, middle;
    /* SECTOR index of the SECTOR this SIDEDEF helps surround */
    Sector *sector;
};



struct Linedef
{
    /* start/end VERTEX indices */
    Vertex *start, *end;
    /* see LinedefFlags */
    uint16_t flags;
    /* see [4-3-2] */
    uint16_t types;
    /* LINEDEFS and SECTORS with matching tags are tied together */
    uint16_t tag;
    /* left/right SIDEDEFs
     * (all LINEDEFs MUST have a right side)
     * (see [4-3] for how to decide) */
    Sidedef *right, *left;
};

/* see [4-3-1] */
enum LinedefFlags
{
    /* monsters/players can't go through this line */
    IMPASSABLE      = 1 << 0,
    /* monsters can't go through */
    BLOCKMONSTERS   = 1 << 1,
    /* can have no texture, shots can travel through this line,
     * and monsters can see through it */
    TWOSIDED        = 1 << 2,
    /* upper texture is drawn top-down instead of bottom-up */
    UNPEGGEDUPPER   = 1 << 3,
    /* lower/middle textures are drawn bottom-up instead of top-down */
    UNPEGGEDLOWER   = 1 << 4,
    /* appears solid on automap */
    SECRET          = 1 << 5,
    /* sound can't pass through */
    BLOCKSOUND      = 1 << 6,
    /* doesn't appear on automap */
    UNMAPPED        = 1 << 7,
    /* appears on automap even if not seen yet */
    PREMAPPED       = 1 << 8
};



struct Seg
{
    Vertex *start, *end;
    /* 0000=east, 4000=north, 8000=west, C000=south
     * see [4-6] for more details */
    uint16_t angle;
    uint16_t linedef;
    /* 0 if the SEG goes the same, or 1 if in the
     * opposite direction of the attached LINEDEF */
    uint16_t direction;
    /* distance along the LINEDEF to the start of the SEG
     * if 'direction' is 0, this is from the LINEDEF's start
     * VERTEX to the SEG's start VERTEX, else from the end
     * VERTEX of the LINEDEF to the SEG's start VERTEX */
    int16_t offset;
};



/* see [4-7] */
struct SSector
{
    uint16_t count;
    uint16_t start;
};



/* see [4-8] */
struct Node
{
    int16_t x, y;
    int16_t dx, dy;
    int16_t right_upper_y, right_lower_y;
    int16_t right_lower_x, right_upper_x;
    int16_t left_upper_y, left_lower_y;
    int16_t left_lower_x, left_upper_x;
    /* if bit 15 is set, the rest of the number
     * is an SSECTOR, otherwise it's a NODE */
    uint16_t right, left;
};



/* see [4-11] */
struct BlockMap
{
    /* WAD header */
    int16_t x, y;
    int16_t column, rows;

    /* pointers to the blocklists
     * (measured in int16s, NOT bytes!)
     * (starting from start of BLOCKMAP LUMP!) */
    uint16_t pointers;
};



struct BlockMapEntry
{
    /* note that the first number will be 0000,
     * and the last will be FFFF */
    uint16_t *linedefs;
};



/* see [5-1] */
struct Picture
{
    uint16_t width, height;
    /* number of pixels to the left/above the
     * origin to start drawing the picture
     * (left should = floor(width / 2) to be centered)
     * if these are negative, they are absolute coordinates
     * from the top left of the screen, and width/height are
     * automatically scaled if the window is less than fullscreen
     */
    int16_t left, top;

    std::vector<uint8_t> data;
    std::vector<bool> opaque;
};



struct Level
{
    class WAD /*const*/ *wad;

    std::vector<Thing> things;
    std::vector<Linedef> linedefs;
    std::vector<Sidedef> sidedefs;
    std::vector<Vertex> vertices;
    std::vector<Seg> segs;
    std::vector<SSector> ssectors;
    std::vector<Node> nodes;
    std::vector<Sector> sectors;
    Reject reject;
    BlockMap blockmap;
};



struct DirEntry
{
private:
    ssize_t idx = 0;

public:
    uint32_t size;
    char name[9];
    std::shared_ptr<uint8_t[]> data;

    void read(void *ptr, size_t byte_count);
    void seek(ssize_t offset, int whence);
};

class WAD
{
public:
    bool iwad;
    std::vector<DirEntry> directory;

    std::vector<size_t> pnames;
    std::array<Palette, 14> palettes;
    std::unordered_map<std::string, Texture> textures;
    std::unordered_map<std::string, Flat> flats;
    std::unordered_map<std::string, Picture> sprites;

    /* get the lump's index in the WAD's directory */
    size_t lumpidx(std::string name, size_t start=0) const;

    /* get the lump itself */
    DirEntry &findlump(std::string name, size_t start=0);
};


#endif

