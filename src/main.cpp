/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#include "camera.hpp"
#include "mesh.hpp"
#include "program.hpp"
#include "readwad.hpp"
#include "texture.hpp"

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
    glm::mat4 projection;

    std::unordered_map<
        std::string,
        std::unique_ptr<GLTexture>> textures, flats;
};

struct RenderLevel
{
    Level const *raw;

    std::vector<Wall> walls;
    std::vector<RenderFlat> flats;
};




RenderLevel make_renderlevel(Level const &lvl, RenderGlobals &g);
void draw_level(RenderLevel const &lvl, RenderGlobals const &g);
void draw_node(
    size_t index,
    RenderLevel const &lvl,
    RenderGlobals const &g);
void draw_ssector(
    size_t index,
    RenderLevel const &lvl,
    RenderGlobals const &g);
Uint32 callback60hz(Uint32, void *);



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
    auto level = readlevel(
        "E" + std::to_string(episode) + "M" + std::to_string(mission),
        wadfile,
        wad);


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
    g.projection = glm::perspective(
        glm::radians(45.0),
        (double)g.width / (double)g.height,
        0.1, 10000.0);

    /* load the shader */
    g.program.reset(new Program{
        Shader{GL_VERTEX_SHADER, "shaders/vertex.glvs"},
        Shader{GL_FRAGMENT_SHADER, "shaders/fragment.glfs"}});


    /* make GLTextures from the textures */
    for (auto &pair : level.wad->textures)
    {
        auto &name = pair.first;
        auto &tex = pair.second;

        auto *imgdata = new uint32_t[tex.data.size()];
        for (size_t i = 0; i < tex.data.size(); ++i)
        {
            imgdata[i] = level.wad->palette[tex.data[i]];
            if (!tex.opaque[i])
            {
                imgdata[i] &= 0x00FFFFFF;
            }
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

        auto *imgdata = new uint32_t[flat.size()];
        for (size_t i = 0; i < flat.size(); ++i)
        {
            imgdata[i] = level.wad->palette[flat[i]];
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

    glm::vec3 delta(0);
    SDL_Event e;
    for (bool running = true; running; )
    {
        while (SDL_PollEvent(&e) && running)
        {
            switch (e.type)
            {
            case SDL_QUIT:
                running = false;
                break;

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
                case SDLK_z:
                    delta.y = +10;
                    break;
                case SDLK_x:
                    delta.y = -10;
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
                case SDLK_z:
                case SDLK_x:
                    delta.y = 0;
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
                    level = readlevel(
                            (   "E"
                                + std::to_string(episode)
                                + "M"
                                + std::to_string(mission)),
                            wadfile,
                            wad);
                    renderlevel = make_renderlevel(level, g);

                    for (auto &thing : level.things)
                    {
                        if (thing.type == 1)
                        {
                            g.cam.pos.x = -thing.x;
                            g.cam.pos.z = thing.y;
                        }
                    }
                }
                break;


            case SDL_WINDOWEVENT:
                switch (e.window.event)
                {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    g.width  = e.window.data1;
                    g.height = e.window.data2;

                    glViewport(0, 0, g.width, g.height);
                    g.projection = glm::perspective(
                        glm::radians(45.0),
                        (double)g.width / (double)g.height,
                        0.1, 10000.0);
                    break;
                }
                break;
            }
        }

        g.cam.move(delta);

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        draw_level(renderlevel, g);

        SDL_GL_SwapWindow(win);
        SDL_Delay(1000 / 60);
    }


    /* cleanup */
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
                        2, 1, 0,
                        0, 3, 2
                    }});
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

                std::string texname =\
                    (right_is_top? ld.left->lower : ld.right->lower);
                if (texname == "-")
                {
                    /* TODO: deal with this properly */
                    texname = "aastinky";
                }
                texname = tolowercase(texname);

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

                std::string texname =\
                    (right_is_top? ld.right->upper : ld.left->upper);
                if (texname == "-")
                {
                    /* TODO: deal with this properly */
                    texname = "aastinky";
                }
                texname = tolowercase(texname);

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

            if (ld.right->middle != "-")
            {
                std::string texname = tolowercase(ld.right->middle);
                out.walls.back().middletex = g.textures[texname].get();
                len /= g.textures[texname]->width;
                hgt /= g.textures[texname]->height;
            }

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

    return out;
}


void draw_level(RenderLevel const &lvl, RenderGlobals const &g)
{
    g.program->use();
    g.program->set("camera", g.cam.matrix());
    g.program->set("projection", g.projection);

    /* draw the walls */
    draw_node(lvl.raw->nodes.size() - 1, lvl, g);


    /* draw the flats */
    for (auto &flat : lvl.flats)
    {
        /* floor */
        glBindTexture(GL_TEXTURE_2D, flat.floortex->id());
        glActiveTexture(GL_TEXTURE0);
        g.program->set("tex", 0);
        g.program->set("xoffset", 0);
        g.program->set("yoffset", 0);

        flat.floormesh->bind();
        glDrawElements(
            GL_TRIANGLE_FAN,
            flat.floormesh->size(),
            GL_UNSIGNED_INT,
            0);

        /* ceiling */
        glBindTexture(GL_TEXTURE_2D, flat.ceilingtex->id());
        glActiveTexture(GL_TEXTURE0);
        g.program->set("tex", 0);
        g.program->set("xoffset", 0);
        g.program->set("yoffset", 0);

        flat.ceilingmesh->bind();
        glDrawElements(
            GL_TRIANGLE_FAN,
            flat.ceilingmesh->size(),
            GL_UNSIGNED_INT,
            0);
    }
}

void draw_node(
    size_t index,
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
    size_t index,
    RenderLevel const &lvl,
    RenderGlobals const &g)
{
    SSector const &ssector = lvl.raw->ssectors[index];

    for (size_t i = 0; i < ssector.count; ++i)
    {
        auto &seg = lvl.raw->segs.at(ssector.start + i);
        auto &wall = lvl.walls.at(seg.linedef);

        if (wall.uppermesh != nullptr)
        {
            if (wall.uppertex != nullptr)
            {
                glBindTexture(GL_TEXTURE_2D, wall.uppertex->id());
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }
            glActiveTexture(GL_TEXTURE0);
            g.program->set("tex", 0);
            g.program->set("xoffset",
                lvl.raw->linedefs[seg.linedef].right->x);
            g.program->set("yoffset",
                lvl.raw->linedefs[seg.linedef].right->y);

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
                glBindTexture(GL_TEXTURE_2D, wall.middletex->id());
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }
            glActiveTexture(GL_TEXTURE0);
            g.program->set("tex", 0);
            g.program->set("xoffset",
                lvl.raw->linedefs[seg.linedef].right->x);
            g.program->set("yoffset",
                lvl.raw->linedefs[seg.linedef].right->y);

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
                glBindTexture(GL_TEXTURE_2D, wall.middletex2->id());
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }
            glActiveTexture(GL_TEXTURE0);
            g.program->set("tex", 0);
            g.program->set("xoffset",
                lvl.raw->linedefs[seg.linedef].right->x);
            g.program->set("yoffset",
                lvl.raw->linedefs[seg.linedef].right->y);

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
                glBindTexture(GL_TEXTURE_2D, wall.lowertex->id());
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }
            glActiveTexture(GL_TEXTURE0);
            g.program->set("tex", 0);
            g.program->set("xoffset",
                lvl.raw->linedefs[seg.linedef].right->x);
            g.program->set("yoffset",
                lvl.raw->linedefs[seg.linedef].right->y);

            wall.lowermesh->bind();
            glDrawElements(
                GL_TRIANGLES,
                wall.lowermesh->size(),
                GL_UNSIGNED_INT,
                0);
        }
    }
}

