/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#ifndef _RENDERLEVEL_H
#define _RENDERLEVEL_H

#include "camera.hpp"
#include "mesh.hpp"
#include "program.hpp"
#include "texture.hpp"
#include "wad.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>


struct RenderThing
{
    struct SpriteDef
    {
        GLTexture *tex;
        bool flipx;
        glm::vec2 offset;
    };

    bool angled;
    bool cleanloop;
    bool reverse_anim;
    int framecount;
    int frame_idx;

    std::unordered_map<std::string, SpriteDef> sprites;

    Sector *sector;
    glm::vec3 pos;
    double angle;
};

struct Wall
{
    GLTexture *middletex;
    std::unique_ptr<Mesh> middlemesh;

    GLTexture *uppertex;
    std::unique_ptr<Mesh> uppermesh;

    GLTexture *lowertex;
    std::unique_ptr<Mesh> lowermesh;

    Wall(
            GLTexture *middletex,
            Mesh *middlemesh,
            GLTexture *uppertex=nullptr,
            Mesh *uppermesh=nullptr,
            GLTexture *lowertex=nullptr,
            Mesh *lowermesh=nullptr)
    :   middletex{middletex},
        middlemesh{middlemesh},
        uppertex{uppertex},
        uppermesh{uppermesh},
        lowertex{lowertex},
        lowermesh{lowermesh}
    {
    }
};

struct RenderFlat
{
    GLTexture *tex;
    Mesh *mesh;
    uint16_t lightlevel;

    RenderFlat(GLTexture *tex, Mesh *mesh, uint16_t lightlevel)
    :   tex{tex},
        mesh{mesh},
        lightlevel{lightlevel}
    {
    }

    RenderFlat(GLTexture *tex, nullptr_t const &blank)
    :   tex{tex},
        mesh{blank}
    {
    }
};

struct RenderGlobals
{
    int width, height;

    Camera cam;
    std::unique_ptr<Program> program;
    std::unique_ptr<Program> billboard_shader;
    std::unique_ptr<Program> automap_program;
    glm::mat4 projection;

    GLuint palette_texture;
    GLuint palette_number;

    GLuint colormap_texture;

    std::unordered_map<
        std::string,
        std::unique_ptr<GLTexture>> textures,
                                    flats,
                                    sprites,
                                    menu_images,
                                    gui_images;
};

class RenderLevel
{
public:
    Level const *raw;

    std::vector<Wall> walls;
    std::vector<RenderThing> things;
    std::vector<RenderFlat> floors;
    std::vector<RenderFlat> ceilings;

    std::unique_ptr<Mesh> automap;
    GLuint automap_vbo;

    RenderLevel(
        Level const &lvl,
        RenderGlobals &g,
        uint8_t include,
        uint8_t exclude);
    ~RenderLevel();

private:
    RenderLevel(RenderLevel const &) = delete;
    RenderLevel &operator=(RenderLevel const &) = delete;
};


#endif

