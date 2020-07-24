/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#include "camera.hpp"
#include "mesh.hpp"
#include "program.hpp"
#include "readwad.hpp"
#include "texture.hpp"
#include "things.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <SDL2/SDL.h>

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>



struct RenderThing
{
    GLTexture *sprite;
    std::unique_ptr<Mesh> mesh;
    glm::vec3 pos;

    RenderThing(
        GLTexture *sprite,
        Mesh *mesh,
        glm::vec3 pos)
    :   sprite{sprite},
        mesh{mesh},
        pos{pos}
    {
    }
};

struct Wall
{
    GLTexture *middletex;
    std::unique_ptr<Mesh> middlemesh;

    GLTexture *middletex2;
    std::unique_ptr<Mesh> middlemesh2;

    GLTexture *uppertex;
    std::unique_ptr<Mesh> uppermesh;

    GLTexture *lowertex;
    std::unique_ptr<Mesh> lowermesh;

    Wall(
            GLTexture *middletex,
            Mesh *middlemesh,
            GLTexture *middletex2=nullptr,
            Mesh *middlemesh2=nullptr,
            GLTexture *uppertex=nullptr,
            Mesh *uppermesh=nullptr,
            GLTexture *lowertex=nullptr,
            Mesh *lowermesh=nullptr)
    :   middletex{middletex},
        middlemesh{middlemesh},
        middletex2{middletex2},
        middlemesh2{middlemesh2},
        uppertex{uppertex},
        uppermesh{uppermesh},
        lowertex{lowertex},
        lowermesh{lowermesh}
    {
    }
};

struct RenderFlat
{
    GLTexture *floortex;
    std::unique_ptr<Mesh> floormesh;
    GLTexture *ceilingtex;
    std::unique_ptr<Mesh> ceilingmesh;

    RenderFlat(
            GLTexture *floortex,
            Mesh *floormesh,
            GLTexture *ceilingtex,
            Mesh *ceilingmesh)
    :   floortex{floortex},
        floormesh{floormesh},
        ceilingtex{ceilingtex},
        ceilingmesh{ceilingmesh}
    {
    }
};

struct RenderGlobals
{
    int width, height;

    Camera cam;
    std::unique_ptr<Program> program;
    std::unique_ptr<Program> billboard_shader;
    glm::mat4 projection;

    GLuint palette_id;
    GLuint palette_number;

    std::unordered_map<
        std::string,
        std::unique_ptr<GLTexture>> textures, flats;
};

struct RenderLevel
{
    Level const *raw;

    std::vector<Wall> walls;
    std::vector<RenderFlat> flats;
    std::vector<RenderThing> things;
};



RenderLevel make_renderlevel(Level const &lvl, RenderGlobals &g);
uint16_t get_ssector(int16_t x, int16_t y, Level const &lvl);
void draw_level(RenderLevel const &lvl, RenderGlobals const &g);
void draw_node(
    uint16_t index,
    RenderLevel const &lvl,
    RenderGlobals const &g);
void draw_ssector(
    uint16_t index,
    RenderLevel const &lvl,
    RenderGlobals const &g);



std::string tolowercase(std::string const &str)
{
    std::string lower = str;
    std::transform(
        lower.begin(),
        lower.end(),
        lower.begin(),
        [](unsigned char c){ return std::tolower(c); });
    return lower;
}



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

    int episode = 1,
        mission = 1;

    auto wad = readwad(wadfile);
    Level level{};
    try
    {
        level = readlevel(
            (   "E"
                + std::to_string(episode)
                + "M"
                + std::to_string(mission)),
            wadfile,
            wad);
    }
    catch (std::runtime_error &e)
    {
        episode = 0;
        level = readlevel(
            "MAP" + std::to_string(episode) + std::to_string(mission),
            wadfile,
            wad);
    }


    RenderGlobals g{};
    g.width  = 320;
    g.height = 240;


    /* setup SDL stuff */
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    SDL_Window *win = SDL_CreateWindow(
        __func__,
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        g.width, g.height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext context = SDL_GL_CreateContext(win);

    SDL_SetRelativeMouseMode(SDL_TRUE);
    if (SDL_GL_SetSwapInterval(-1) == -1)
    {
        SDL_GL_SetSwapInterval(1);
    }


    /* init OpenGL stuff */
    glewInit();

    glViewport(0, 0, g.width, g.height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);


    /* setup the camera */
    g.cam = Camera{
        glm::vec3(0, 0, 0),
        glm::vec3(0, 0, 1),
        glm::vec3(0, 1, 0)};

    /* load projection matrix */
    double const fov = 60;
    g.projection = glm::perspective(
        glm::radians(fov),
        (double)g.width / (double)g.height,
        0.1, 10000.0);

    /* load the shaders */
    g.program.reset(new Program{
        Shader{GL_VERTEX_SHADER, "shaders/vertex.glvs"},
        Shader{GL_FRAGMENT_SHADER, "shaders/fragment.glfs"}});
    g.billboard_shader.reset(new Program{
        Shader{GL_VERTEX_SHADER, "shaders/billboard.glvs"},
        Shader{GL_FRAGMENT_SHADER, "shaders/fragment.glfs"}});


    /* set up the palette */
    g.palette_number = 0;
    auto palette = new uint8_t[14][256][3];
    for (size_t i = 0; i < 14; ++i)
    {
        auto &pal = level.wad->palettes[i];
        for (size_t j = 0; j < 256; ++j)
        {
            palette[i][j][0] = pal[(j * 3) + 0];
            palette[i][j][1] = pal[(j * 3) + 1];
            palette[i][j][2] = pal[(j * 3) + 2];
        }
    }
    glGenTextures(1, &g.palette_id);
    glBindTexture(GL_TEXTURE_2D, g.palette_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGB8,
        256, 14,
        0,
        GL_RGB,
        GL_UNSIGNED_BYTE,
        palette);
    delete[] palette;


    /* make GLTextures from the textures */
    for (auto &pair : level.wad->textures)
    {
        auto &name = pair.first;
        auto &tex = pair.second;

        auto imgdata = new uint32_t[tex.data.size()];
        for (size_t i = 0; i < tex.data.size(); ++i)
        {
            imgdata[i] =\
                ((tex.opaque[i]? 0xFF : 0) << 8)
                | tex.data[i];
        }
        g.textures.emplace(
            tolowercase(name),
            new GLTexture{tex.width, tex.height, imgdata});
        delete[] imgdata;
    }

    /* make GLTextures from the flats */
    for (auto &pair : level.wad->flats)
    {
        auto &name = pair.first;
        auto &flat = pair.second;

        auto *imgdata = new uint8_t[flat.size() * 2];
        for (size_t i = 0; i < flat.size(); ++i)
        {
            imgdata[(i * 2) + 0] = flat[i];
            imgdata[(i * 2) + 1] = 0xFF;
        }
        g.flats.emplace(
            name,
            new GLTexture{64, 64, imgdata});
        delete[] imgdata;
    }


    /* set the camera location to the player start */
    for (auto &thing : level.things)
    {
        if (thing.type == 1)
        {
            g.cam.pos.x = -thing.x;
            g.cam.pos.z = thing.y;
        }
    }


    auto renderlevel = make_renderlevel(level, g);

    glm::vec3 delta{0};
    SDL_Event e;
    bool paused = false;
    for (bool running = true; running; )
    {
        while (SDL_PollEvent(&e) && running)
        {
            switch (e.type)
            {
            case SDL_QUIT:
                running = false;
                break;

            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    g.width  = e.window.data1;
                    g.height = e.window.data2;

                    glViewport(0, 0, g.width, g.height);
                    g.projection = glm::perspective(
                        glm::radians(fov),
                        (double)g.width / (double)g.height,
                        0.1, 10000.0);
                }
                break;
            case SDL_KEYUP:
                if (e.key.keysym.sym == SDLK_ESCAPE)
                {
                    if (paused)
                    {
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                    }
                    else
                    {
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                    }
                    paused = !paused;
                    break;
                }
                break;
            }
            if (!paused)
            {
                switch (e.type)
                {
                case SDL_MOUSEMOTION:
                    g.cam.rotate(
                        -e.motion.xrel / 10.0,
                        -e.motion.yrel / 10.0);
                    break;

                case SDL_KEYDOWN:
                    switch (e.key.keysym.sym)
                    {
                    case SDLK_d:
                        delta.x = +10;
                        break;
                    case SDLK_a:
                        delta.x = -10;
                        break;
                    case SDLK_w:
                        delta.z = +10;
                        break;
                    case SDLK_s:
                        delta.z = -10;
                        break;
                    }
                    break;

                case SDL_KEYUP:
                    switch (e.key.keysym.sym)
                    {
                    case SDLK_a:
                    case SDLK_d:
                        delta.x = 0;
                        break;
                    case SDLK_w:
                    case SDLK_s:
                        delta.z = 0;
                        break;

                    case SDLK_SPACE:
                        mission += 1;
                        if (mission > 9)
                        {
                            mission = 1;
                            episode += 1;
                            if (episode > 3)
                            {
                                episode = 1;
                            }
                        }
                        for (auto &t : renderlevel.things)
                        {
                            delete t.sprite;
                        }
                        try
                        {
                            level = readlevel(
                                (   "E"
                                    + std::to_string(episode)
                                    + "M"
                                    + std::to_string(mission)),
                                wadfile,
                                wad);
                        }
                        catch (std::runtime_error &e)
                        {
                            level = readlevel(
                                (   "MAP"
                                    + std::to_string(episode)
                                    + std::to_string(mission)),
                                wadfile,
                                wad);
                        }
                        renderlevel = make_renderlevel(level, g);

                        for (auto &thing : level.things)
                        {
                            if (thing.type == 1)
                            {
                                g.cam.pos.x = -thing.x;
                                g.cam.pos.z = thing.y;
                                break;
                            }
                        }
                    }
                    break;
                }
            }
        }

        g.cam.move(delta);
        int ssector = -1;
        try
        {
            ssector = get_ssector(-g.cam.pos.x, g.cam.pos.z, level);
        }
        catch (std::runtime_error &e)
        {
        }
        if (ssector != -1)
        {
            auto &seg = level.segs[level.ssectors[ssector].start];
            auto &ld = level.linedefs[seg.linedef];
            g.cam.pos.y = (
                seg.direction?
                    ld.left
                    : ld.right)->sector->floor + 48;
        }

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        g.program->use();
        g.program->set("camera", g.cam.matrix());
        g.program->set("projection", g.projection);
        g.program->set("palettes", 0);
        g.program->set("palette", g.palette_number);
        g.program->set("tex", 1);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g.palette_id);
        glActiveTexture(GL_TEXTURE1);

        draw_level(renderlevel, g);

        SDL_GL_SwapWindow(win);
        SDL_Delay(1000 / 60);
    }


    /* cleanup */
    for (auto &t : renderlevel.things)
    {
        delete t.sprite;
    }
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(win);
    SDL_Quit();
    fclose(wadfile);

    return EXIT_SUCCESS;
}



RenderLevel make_renderlevel(Level const &lvl, RenderGlobals &g)
{
    RenderLevel out{};
    out.raw = &lvl;

    /* make RenderThings from things */
    for (auto &thing : lvl.things)
    {
        if (thing.options & SKILL12)
        {
            auto &data = thingdata[thing.type];
            Picture const *spr = nullptr;
            switch (data.frames)
            {
            case -1:
                /* no image */
                break;
            case 0:
                spr = &lvl.wad->sprites[data.sprite + "A1"];
                break;
            default:
                if (data.frames > 0)
                {
                    spr = &lvl.wad->sprites[data.sprite + "A0"];
                }
                else
                {
                    spr = &lvl.wad->sprites[
                        data.sprite
                        + std::string(
                            1, 'A' + (-data.frames - 2))
                        + "0"];
                }
                break;
            }

            /* get the thing's y position
             * (ie. the floor height of the sector it's inside) */
            int ssector = -1;
            try
            {
                ssector = get_ssector(thing.x, thing.y, lvl);
            }
            catch (std::runtime_error &e)
            {
            }

            double y = 0;
            if (ssector != -1)
            {
                auto &seg = lvl.segs[lvl.ssectors[ssector].start];
                auto &ld = lvl.linedefs[seg.linedef];
                y = (
                    seg.direction?
                        ld.left
                        : ld.right)->sector->floor;
            }

            /* make the picture data */
            if (spr != nullptr)
            {
                auto imgdata = new uint32_t[spr->data.size()];
                for (size_t i = 0; i < spr->data.size(); ++i)
                {
                    imgdata[i] =\
                        ((spr->opaque[i]? 0xFF : 0) << 8)
                        | spr->data[i];
                }

                /* TODO: figure out the left/top offsets */
                GLfloat w = spr->width,
                        h = spr->height;
                out.things.emplace_back(
                    new GLTexture{spr->width, spr->height, imgdata},
                    new Mesh{{
                        {0, 0, 0,  0, 1},
                        {w, 0, 0,  1, 1},
                        {w, h, 0,  1, 0},
                        {0, h, 0,  0, 0}},
                        {0,1,2, 2,3,0}},
                    glm::vec3(-thing.x, y, thing.y));
                delete[] imgdata;
            }
            else
            {
                out.things.emplace_back(
                    nullptr,
                    nullptr,
                    glm::vec3(-thing.x, y, thing.y));
            }
        }
    }

    /* create Walls from the linedefs */
    for (auto &ld : lvl.linedefs)
    {
        out.walls.emplace_back(nullptr, nullptr);

        if (ld.flags & TWOSIDED)
        {
            /* middle (right side) */
            if (ld.right->middle != "-")
            {
                int top = ld.right->sector->ceiling;
                int bot = ld.right->sector->floor;

                double len = sqrt(
                    pow(ld.end->x - ld.start->x, 2)
                    + pow(ld.end->y - ld.start->y, 2));
                double hgt = abs(top - bot);

                out.walls.emplace_back(nullptr, nullptr);

                std::string texname = tolowercase(ld.right->middle);
                if (texname != "-")
                {
                    out.walls.back().middletex =\
                        g.textures[texname].get();
                    len /= g.textures[texname]->width;
                    hgt /= g.textures[texname]->height;

                    bool unpegged = ld.flags & UNPEGGEDLOWER;
                    double sx = 0,
                           sy = unpegged? -hgt : 0;
                    double ex = len,
                           ey = unpegged? 0 : hgt;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
                    out.walls.back().middlemesh.reset(new Mesh{
                        {
                            {-ld.start->x, bot, ld.start->y,  sx, ey},
                            {-ld.end->x  , bot, ld.end->y  ,  ex, ey},
                            {-ld.end->x  , top, ld.end->y  ,  ex, sy},
                            {-ld.start->x, top, ld.start->y,  sx, sy},
                        },
                        {
                            0, 1, 2,
                            2, 3, 0
                        }});
                }
#pragma GCC diagnostic pop
            }
            /* middle (left side) */
            if (ld.left->middle != "-")
            {
                int top = ld.right->sector->ceiling;
                int bot = ld.right->sector->floor;

                double len = sqrt(
                    pow(ld.end->x - ld.start->x, 2)
                    + pow(ld.end->y - ld.start->y, 2));
                double hgt = abs(top - bot);

                out.walls.emplace_back(nullptr, nullptr);

                std::string texname = tolowercase(ld.right->middle);
                if (texname != "-")
                {
                    out.walls.back().middletex =\
                        g.textures[texname].get();
                    len /= g.textures[texname]->width;
                    hgt /= g.textures[texname]->height;

                    bool unpegged = ld.flags & UNPEGGEDLOWER;
                    double sx = 0,
                           sy = unpegged? -hgt : 0;
                    double ex = len,
                           ey = unpegged? 0 : hgt;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
                    out.walls.back().middlemesh.reset(new Mesh{
                        {
                            {-ld.start->x, bot, ld.start->y,  sx, ey},
                            {-ld.end->x  , bot, ld.end->y  ,  ex, ey},
                            {-ld.end->x  , top, ld.end->y  ,  ex, sy},
                            {-ld.start->x, top, ld.start->y,  sx, sy},
                        },
                        {
                            2, 1, 0,
                            0, 3, 2
                        }});
                }
#pragma GCC diagnostic pop
            }
            /* lower section */
            if (ld.right->sector->floor != ld.left->sector->floor)
            {
                bool right_is_top =\
                    (   ld.right->sector->floor
                        > ld.left->sector->floor);

                int top =\
                    (right_is_top?
                        ld.right->sector->floor
                        : ld.left->sector->floor);
                int bot =\
                    (right_is_top?
                        ld.left->sector->floor
                        : ld.right->sector->floor);

                std::string texname = tolowercase(
                    (right_is_top?
                        ld.left
                        : ld.right)->lower);
                if (texname != "-")
                {
                    auto const tex = g.textures[texname].get();
                    out.walls.back().lowertex = tex;
                    double len =\
                        sqrt(
                            pow(ld.end->x - ld.start->x, 2)
                            + pow(ld.end->y - ld.start->y, 2))
                        / (double)tex->width;
                    double hgt = abs(top - bot) / (double)tex->height;

                    bool unpegged = ld.flags & UNPEGGEDLOWER;
                    double sx = 0,
                           sy = 0;
                    double ex = len,
                           ey = hgt;

                    if (unpegged)
                    {
                        double offset =\
                            (   glm::max(
                                    ld.right->sector->ceiling,
                                    ld.left->sector->ceiling)
                                - top)
                            / (double)tex->height;
                        sy += offset;
                        ey += offset;
                    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
                    out.walls.back().lowermesh.reset(new Mesh{
                        {
                            {-ld.start->x, bot, ld.start->y,  sx, ey},
                            {-ld.end->x,   bot, ld.end->y,    ex, ey},
                            {-ld.end->x,   top, ld.end->y,    ex, sy},
                            {-ld.start->x, top, ld.start->y,  sx, sy},
                        },
                        {
                            0, 1, 2,
                            2, 3, 0,
                        }});
                }
#pragma GCC diagnostic pop
            }
            /* upper section */
            if (ld.right->sector->ceiling != ld.left->sector->ceiling)
            {
                bool right_is_top =\
                    (   ld.right->sector->ceiling
                        > ld.left->sector->ceiling);

                int top =\
                    (right_is_top?
                        ld.right->sector->ceiling
                        : ld.left->sector->ceiling);
                int bot =\
                    (right_is_top?
                        ld.left->sector->ceiling
                        : ld.right->sector->ceiling);

                std::string texname = tolowercase(
                    (right_is_top?
                        ld.right
                        : ld.left)->upper);
                if (texname != "-")
                {
                    auto tex = g.textures[texname].get();
                    out.walls.back().uppertex = tex;
                    double len =\
                        sqrt(
                            pow(ld.end->x - ld.start->x, 2)
                            + pow(ld.end->y - ld.start->y, 2))
                        / (double)tex->width;
                    double hgt = abs(top - bot) / (double)tex->height;

                    bool unpegged = ld.flags & UNPEGGEDUPPER;
                    double sx = 0,
                           sy = unpegged? 0 : -hgt;
                    double ex = len,
                           ey = unpegged? hgt : 0;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
                    out.walls.back().uppermesh.reset(new Mesh{
                        {
                            {-ld.start->x, bot, ld.start->y,  sx, ey},
                            {-ld.end->x,   bot, ld.end->y,    ex, ey},
                            {-ld.end->x,   top, ld.end->y,    ex, sy},
                            {-ld.start->x, top, ld.start->y,  sx, sy},
                        },
                        {
                            0, 1, 2,
                            2, 3, 0,
                        }});
                }
#pragma GCC diagnostic pop
            }
        }
        else
        {
            int top = ld.right->sector->ceiling;
            int bot = ld.right->sector->floor;

            double len = sqrt(
                pow(ld.end->x - ld.start->x, 2)
                + pow(ld.end->y - ld.start->y, 2));
            double hgt = abs(top - bot);

            std::string texname = tolowercase(ld.right->middle);
            out.walls.back().middletex = g.textures[texname].get();
            len /= g.textures[texname]->width;
            hgt /= g.textures[texname]->height;

            bool unpegged = ld.flags & UNPEGGEDLOWER;
            double sx = 0,
                   sy = unpegged? -hgt : 0;
            double ex = len,
                   ey = unpegged? 0 : hgt;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
            out.walls.back().middlemesh.reset(new Mesh{
                {
                    {-ld.start->x, bot, ld.start->y,  sx, ey},
                    {-ld.end->x  , bot, ld.end->y  ,  ex, ey},
                    {-ld.end->x  , top, ld.end->y  ,  ex, sy},
                    {-ld.start->x, top, ld.start->y,  sx, sy},
                },
                {
                    0, 1, 2,
                    2, 3, 0
                }});
#pragma GCC diagnostic pop
        }
    }

#if 0
    /* create the floors/ceilings from ssectors */
    for (auto &ssector : lvl.ssectors)
    {
        std::vector<size_t> ordered{};
        std::unordered_set<size_t> indices{};
        for (size_t i = 0; i < ssector.count; ++i)
        {
            indices.emplace(ssector.start + i);
        }

        size_t i = *indices.begin();
        ordered.push_back(i);
        indices.erase(i);
        for (;;)
        {
            bool found = false;

            auto &ref_seg = lvl.segs[i];
            auto &ref_ld = lvl.linedefs[ref_seg.linedef];

            for (auto j : indices)
            {
                auto &seg = lvl.segs[j];
                auto &ld = lvl.linedefs[seg.linedef];

                int16_t ref_x = ref_ld.end->x,
                        ref_y = ref_ld.end->y;
                if (ref_seg.direction)
                {
                    ref_x = ref_ld.start->x;
                    ref_y = ref_ld.start->y;
                }
                int16_t x = ld.start->x,
                        y = ld.start->y;
                if (seg.direction)
                {
                    x = ld.end->x;
                    y = ld.end->y;
                }

                if (x == ref_x && y == ref_y)
                {
                    i = j;
                    ordered.push_back(j);
                    indices.erase(j);
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                break;
            }
        }


        std::vector<Mesh::Vertex> floor_verts{},
                                  ceiling_verts{};

        for (size_t i : ordered)
        {
            auto &seg = lvl.segs[i];
            auto &ld = lvl.linedefs[seg.linedef];

            if (seg.direction == 1)
            {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
                floor_verts.push_back({
                    -ld.end->x,
                    ld.left->sector->floor,
                    ld.end->y,
                    ld.end->x / 64.0,
                    ld.end->y / 64.0});
                ceiling_verts.push_back({
                    -ld.start->x,
                    ld.left->sector->ceiling,
                    ld.start->y,
                    ld.start->x / 64.0,
                    ld.start->y / 64.0});
            }
            else
            {
                floor_verts.push_back({
                    -ld.start->x,
                    ld.right->sector->floor,
                    ld.start->y,
                    ld.start->x / 64.0,
                    ld.start->y / 64.0});
                ceiling_verts.push_back({
                    -ld.end->x,
                    ld.right->sector->ceiling,
                    ld.end->y,
                    ld.end->x / 64.0,
                    ld.end->y / 64.0});
#pragma GCC diagnostic pop
            }
        }

        auto &ld = lvl.linedefs[lvl.segs[ssector.start].linedef];

        /* reverse the floor polygon so the 'front' face points up */
        std::reverse(std::begin(floor_verts), std::end(floor_verts));

        out.flats.emplace_back(
            /* floor */
            g.flats[ld.right->sector->floor_flat].get(),
            new Mesh{floor_verts},
            /* ceiling */
            g.flats[ld.right->sector->ceiling_flat].get(),
            new Mesh{ceiling_verts});
    }
#endif

    return out;
}


void draw_level(RenderLevel const &lvl, RenderGlobals const &g)
{
    /* draw the walls */
    draw_node(lvl.raw->nodes.size() - 1, lvl, g);

#if 0
    /* draw the flats */
    for (auto &flat : lvl.flats)
    {
        /* floor */
        glActiveTexture(GL_TEXTURE1);
        flat.floortex->bind();
        g.program->set("tex", 1);
        g.program->set("xoffset", 0);
        g.program->set("yoffset", 0);

        flat.floormesh->bind();
        glDrawElements(
            GL_TRIANGLE_FAN,
            flat.floormesh->size(),
            GL_UNSIGNED_INT,
            0);

        /* ceiling */
        flat.ceilingtex->bind();
        g.program->set("tex", 1);
        g.program->set("xoffset", 0);
        g.program->set("yoffset", 0);

        flat.ceilingmesh->bind();
        glDrawElements(
            GL_TRIANGLE_FAN,
            flat.ceilingmesh->size(),
            GL_UNSIGNED_INT,
            0);
    }
#endif

    /* draw the things */
    g.billboard_shader->use();
    g.billboard_shader->set("camera", g.cam.matrix());
    g.billboard_shader->set("projection", g.projection);
    g.billboard_shader->set("palettes", 0);
    g.billboard_shader->set("palette", g.palette_number);
    g.billboard_shader->set("tex", 1);
    g.billboard_shader->set("xoffset", 0);
    g.billboard_shader->set("yoffset", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g.palette_id);
    glActiveTexture(GL_TEXTURE1);
    for (auto &t : lvl.things)
    {
        if (t.sprite != nullptr)
        {
            t.sprite->bind();
            g.billboard_shader->set("model",
                glm::translate(glm::mat4(1), t.pos));

            t.mesh->bind();
            glDrawElements(
                GL_TRIANGLES,
                t.mesh->size(),
                GL_UNSIGNED_INT,
                0);
        }
    }


}

void draw_node(
    uint16_t index,
    RenderLevel const &lvl,
    RenderGlobals const &g)
{
    Node const &node = lvl.raw->nodes[index];
    if (node.right & 0x8000)
    {
        draw_ssector(node.right & 0x7FFF, lvl, g);
    }
    else
    {
        draw_node(node.right, lvl, g);
    }
    if (node.left & 0x8000)
    {
        draw_ssector(node.left & 0x7FFF, lvl, g);
    }
    else
    {
        draw_node(node.left, lvl, g);
    }
}

void draw_ssector(
    uint16_t index,
    RenderLevel const &lvl,
    RenderGlobals const &g)
{
    SSector const &ssector = lvl.raw->ssectors[index];

    for (size_t i = 0; i < ssector.count; ++i)
    {
        auto &seg = lvl.raw->segs[ssector.start + i];
        auto &wall = lvl.walls[seg.linedef];

        if (wall.uppermesh != nullptr)
        {
            g.program->set("xoffset",
                lvl.raw->linedefs[seg.linedef].right->x);
            g.program->set("yoffset",
                lvl.raw->linedefs[seg.linedef].right->y);

            if (wall.uppertex != nullptr)
            {
                wall.uppertex->bind();
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            wall.uppermesh->bind();
            glDrawElements(
                GL_TRIANGLES,
                wall.uppermesh->size(),
                GL_UNSIGNED_INT,
                0);
        }
        if (wall.middlemesh != nullptr)
        {
            if (wall.middletex != nullptr)
            {
                wall.middletex->bind();
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            wall.middlemesh->bind();
            glDrawElements(
                GL_TRIANGLES,
                wall.middlemesh->size(),
                GL_UNSIGNED_INT,
                0);
        }
        if (wall.middlemesh2 != nullptr)
        {
            if (wall.middletex2 != nullptr)
            {
                wall.middletex2->bind();
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            wall.middlemesh2->bind();
            glDrawElements(
                GL_TRIANGLES,
                wall.middlemesh2->size(),
                GL_UNSIGNED_INT,
                0);
        }
        if (wall.lowermesh != nullptr)
        {
            if (wall.lowertex != nullptr)
            {
                wall.lowertex->bind();
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            wall.lowermesh->bind();
            glDrawElements(
                GL_TRIANGLES,
                wall.lowermesh->size(),
                GL_UNSIGNED_INT,
                0);
        }
    }
}



uint16_t _get_ssector_interior(
    int16_t x,
    int16_t y,
    Level const &lvl,
    uint16_t node)
{
    auto &n = lvl.nodes[node];
    uint16_t number = 0;

    double part_angle = atan2((double)n.dy, (double)n.dx);
    double xy_angle = atan2(
        (double)y - (double)n.y,
        (double)x - (double)n.x);

    bool right = false;
    if (copysign(1, part_angle) != copysign(1, xy_angle))
    {
        part_angle = atan2(-(double)n.dy, -(double)n.dx);
        right = (xy_angle > part_angle);
    }
    else
    {
        right = (xy_angle < part_angle);
    }

    int16_t lower_x = right? n.right_lower_x : n.left_lower_x,
            upper_x = right? n.right_upper_x : n.left_upper_x,
            lower_y = right? n.right_lower_y : n.left_lower_y,
            upper_y = right? n.right_upper_y : n.left_upper_y;

    if (lower_x <= x && x <= upper_x && lower_y <= y && y <= upper_y)
    {
        number = right? n.right : n.left;
    }
    else
    {
        throw std::runtime_error("out of bounds");
    }

    if (number & 0x8000)
    {
        return number & 0x7FFF;
    }
    else
    {
        return _get_ssector_interior(x, y, lvl, number);
    }
}

uint16_t get_ssector(int16_t x, int16_t y, Level const &lvl)
{
    return _get_ssector_interior(x, y, lvl, lvl.nodes.size() - 1);
}

