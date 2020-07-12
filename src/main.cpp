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

#include <memory>



struct Wall
{
    GLTexture *texture;
    std::unique_ptr<Mesh> mesh;

    GLTexture *texture2;
    std::unique_ptr<Mesh> mesh2;

    Wall(
            GLTexture *t1,
            Mesh *m1,
            GLTexture *t2=nullptr,
            Mesh *m2=nullptr)
    :   texture{t1},
        mesh{m1},
        texture2{t2},
        mesh2{m2}
    {
    }
};



void draw_level(Level const &lvl);
void draw_node(size_t index, Level const &lvl);
void draw_ssector(size_t index, Level const &lvl);

Uint32 callback60hz(Uint32, void *);



static Camera cam;
static std::unique_ptr<Program> program;
static glm::mat4 projection;

static std::unordered_map<
    std::string,
    std::unique_ptr<GLTexture>> textures{};
static std::unordered_map<
    std::string,
    std::unique_ptr<GLTexture>> flat_texs{};

static std::vector<Wall> walls{};
static std::vector<Wall> flats{};

static int width  = 640,
           height = 480;

static std::vector<std::array<uint8_t, 3>> ssector_colors;




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
    auto level = readlevel("E1M1", wadfile, wad);

    fclose(wadfile);

    for (size_t i = 0; i < level.ssectors.size(); ++i)
    {
        ssector_colors.push_back(
            std::array<uint8_t, 3>{
                (uint8_t)rand(),
                (uint8_t)rand(),
                (uint8_t)rand()});
    }


    draw_level(level);

    return EXIT_SUCCESS;
}



void draw_level(Level const &lvl)
{
    cam = Camera{
        glm::vec3(0, 0, 0),
        glm::vec3(0, 0, 1),
        glm::vec3(0, 1, 0)};

    /* set the camera location to the player start */
    for (auto &thing : lvl.things)
    {
        if (thing.type == 1)
        {
            cam.pos.x = -thing.x;
            cam.pos.z = thing.y;
        }
    }

    /* setup SDL stuff */
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    SDL_Window *win = SDL_CreateWindow(
        __func__,
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext context = SDL_GL_CreateContext(win);

    SDL_SetRelativeMouseMode(SDL_TRUE);
    if (SDL_GL_SetSwapInterval(-1) == -1)
    {
        SDL_GL_SetSwapInterval(1);
    }


    glewInit();

    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);


    /* make GLTextures from the textures */
    for (auto &pair : lvl.wad->textures)
    {
        auto &name = pair.first;
        auto &tex = pair.second;

        auto *imgdata = new uint32_t[tex.data.size()];
        for (size_t i = 0; i < tex.data.size(); ++i)
        {
            imgdata[i] = lvl.wad->palette[tex.data[i]];
            if (!tex.opaque[i])
            {
                imgdata[i] &= 0x00FFFFFF;
            }
        }
        textures.emplace(
            name,
            new GLTexture{tex.width, tex.height, imgdata});
        delete[] imgdata;
    }

    /* make GLTextures from the flats */
    for (auto &pair : lvl.wad->flats)
    {
        auto &name = pair.first;
        auto &flat = pair.second;

        auto *imgdata = new uint32_t[flat.size()];
        for (size_t i = 0; i < flat.size(); ++i)
        {
            imgdata[i] = lvl.wad->palette[flat[i]];
        }
        flat_texs.emplace(
            name,
            new GLTexture{64, 64, imgdata});
        delete[] imgdata;
    }

    /* create Walls from the linedefs */
    for (auto &ld : lvl.linedefs)
    {
        if (ld.left != nullptr)
        {
            walls.emplace_back(nullptr, nullptr);

            if (ld.right->sector->floor != ld.left->sector->floor)
            {
                int top = glm::max(
                    ld.right->sector->floor,
                    ld.left->sector->floor);
                int bot = glm::min(
                    ld.right->sector->floor,
                    ld.left->sector->floor);

                std::string texname =\
                    (ld.right->lower == "-"?
                        ld.left->lower
                        : ld.right->lower);

                auto tex = textures[texname].get();
                walls.back().texture = tex;
                double len =\
                    sqrt(
                        pow(ld.end->x - ld.start->x, 2)
                        + pow(ld.end->y - ld.start->y, 2))
                    / (double)tex->width;
                double hgt = abs(top - bot) / (double)tex->height;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
                walls.back().mesh.reset(new Mesh{
                    {
                        {-ld.start->x, bot, ld.start->y,  0.0, hgt},
                        {-ld.end->x,   bot, ld.end->y,    len, hgt},
                        {-ld.end->x,   top, ld.end->y,    len, 0.0},
                        {-ld.start->x, top, ld.start->y,  0.0, 0.0},
                    },
                    {
                        0, 1, 2,
                        2, 3, 0,
                    }});
#pragma GCC diagnostic pop
            }
            if (ld.right->sector->ceiling != ld.left->sector->ceiling)
            {
                int top = glm::max(
                    ld.right->sector->ceiling,
                    ld.left->sector->ceiling);
                int bot = glm::min(
                    ld.right->sector->ceiling,
                    ld.left->sector->ceiling);

                std::string texname =\
                    (ld.right->upper == "-"?
                        ld.left->upper
                        : ld.right->upper);

                auto tex = textures[texname].get();
                walls.back().texture2 = tex;
                double len =\
                    sqrt(
                        pow(ld.end->x - ld.start->x, 2)
                        + pow(ld.end->y - ld.start->y, 2))
                    / (double)tex->width;
                double hgt = abs(top - bot) / (double)tex->height;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
                walls.back().mesh2.reset(new Mesh{
                    {
                        {-ld.start->x, bot, ld.start->y,  0.0, hgt},
                        {-ld.end->x,   bot, ld.end->y,    len, hgt},
                        {-ld.end->x,   top, ld.end->y,    len, 0.0},
                        {-ld.start->x, top, ld.start->y,  0.0, 0.0},
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
            auto bot = ld.right->sector->floor;
            auto top = ld.right->sector->ceiling;

            double length = sqrt(
                pow(ld.end->x - ld.start->x, 2)
                + pow(ld.end->y - ld.start->y, 2));
            double height = abs(top - bot);

            walls.emplace_back(nullptr, nullptr);

            if (ld.right->middle != "-")
            {
                walls.back().texture =\
                    textures[ld.right->middle].get();
                length /= textures[ld.right->middle]->width;
                height /= textures[ld.right->middle]->height;
            }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
            walls.back().mesh.reset(new Mesh{
                {
                    {-ld.start->x, bot, ld.start->y,  0.0   , height},
                    {-ld.end->x  , bot, ld.end->y  ,  length, height},
                    {-ld.end->x  , top, ld.end->y  ,  length, 0.0},
                    {-ld.start->x, top, ld.start->y,  0.0   , 0.0},
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
        std::vector<Mesh::Vertex> floor_verts{},
                                  ceiling_verts{};

        /* TODO: these vertices need to be ordered! */
        for (size_t i = 0; i < ssector.count; ++i)
        {
            auto &seg = lvl.segs[ssector.start + i];
            auto &linedef = lvl.linedefs[seg.linedef];

            Mesh::Vertex floor_v{},
                         ceiling_v;

            floor_v.x = linedef.start->x;
            floor_v.y = linedef.right->sector->floor;
            floor_v.z = linedef.start->y;
            floor_v.s = linedef.start->x;
            floor_v.t = linedef.start->y;

            ceiling_v.x = linedef.start->x;
            ceiling_v.y = linedef.right->sector->ceiling;
            ceiling_v.z = linedef.start->y;
            ceiling_v.s = linedef.start->x;
            ceiling_v.t = linedef.start->y;

            floor_verts.push_back(floor_v);
            ceiling_verts.push_back(ceiling_v);
        }

        auto &seg = lvl.segs[ssector.start];

        flats.emplace_back(
            /* floor */
            flat_texs.at(
                lvl.linedefs[
                    seg.linedef].right->sector->floor_flat).get(),
            new Mesh{floor_verts},
            /* ceiling */
            flat_texs.at(
                lvl.linedefs[
                    seg.linedef].right->sector->ceiling_flat).get(),
            new Mesh{ceiling_verts});
    }

    /* make the shaders */
    program.reset(new Program{
        Shader{GL_VERTEX_SHADER, "shaders/vertex.glvs"},
        Shader{GL_FRAGMENT_SHADER, "shaders/fragment.glfs"}});

    projection = glm::perspective(
        glm::radians(45.0),
        (double)width / (double)height,
        0.1, 10000.0);



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
                cam.rotate(
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
                }
                break;


            case SDL_WINDOWEVENT:
                switch (e.window.event)
                {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    width  = e.window.data1;
                    height = e.window.data2;

                    glViewport(0, 0, width, height);
                    projection = glm::perspective(
                        glm::radians(45.0),
                        (double)width / (double)height,
                        0.1, 10000.0);
                    break;
                }
                break;
            }
        }

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        cam.move(delta);

        program->use();
        program->set("camera", cam.matrix());
        program->set("projection", projection);
        draw_node(lvl.nodes.size() - 1, lvl);

        SDL_GL_SwapWindow(win);
        SDL_Delay(1000 / 60);
    }

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(win);
    SDL_Quit();
}

void draw_node(size_t index, Level const &lvl)
{
    Node const &node = lvl.nodes[index];
    if (node.right & 0x8000)
    {
        draw_ssector(node.right & 0x7FFF, lvl);
    }
    else
    {
        draw_node(node.right, lvl);
    }
    if (node.left & 0x8000)
    {
        draw_ssector(node.left & 0x7FFF, lvl);
    }
    else
    {
        draw_node(node.left, lvl);
    }
}

void draw_ssector(size_t index, Level const &lvl)
{
    SSector const &ssector = lvl.ssectors[index];

    for (size_t i = 0; i < ssector.count; ++i)
    {
        auto &seg = lvl.segs[ssector.start + i];
        auto &wall = walls[seg.linedef];

        if (wall.mesh != nullptr)
        {
            glBindTexture(GL_TEXTURE_2D, wall.texture->id());
            glActiveTexture(GL_TEXTURE0);
            program->set("tex", 0);

            wall.mesh->bind();
            glDrawElements(
                GL_TRIANGLES,
                wall.mesh->size(),
                GL_UNSIGNED_INT,
                0);
        }
        if (wall.mesh2 != nullptr)
        {
            glBindTexture(GL_TEXTURE_2D, wall.texture2->id());
            glActiveTexture(GL_TEXTURE0);
            program->set("tex", 0);

            wall.mesh2->bind();
            glDrawElements(
                GL_TRIANGLES,
                wall.mesh2->size(),
                GL_UNSIGNED_INT,
                0);
        }
    }

#if 0
    auto &flat = flats[index];

    glBindTexture(GL_TEXTURE_2D, flat.texture->id());
    glActiveTexture(GL_TEXTURE0);
    program->set("tex", 0);

    flat.mesh->bind();
    glDrawElements(
        GL_TRIANGLES,
        flat.mesh->size(),
        GL_UNSIGNED_INT,
        0);

    glBindTexture(GL_TEXTURE_2D, flat.texture2->id());
    glActiveTexture(GL_TEXTURE0);
    program->set("tex", 0);

    flat.mesh2->bind();
    glDrawElements(
        GL_TRIANGLES,
        flat.mesh2->size(),
        GL_UNSIGNED_INT,
        0);
#endif
}

