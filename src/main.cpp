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
    GLTexture *tex;
    Mesh *mesh;

    RenderFlat(GLTexture *tex, Mesh *mesh)
    :   tex{tex},
        mesh{mesh}
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
        std::unique_ptr<GLTexture>> textures,
                                    flats,
                                    sprites,
                                    menu_images,
                                    gui_images;
};

struct RenderLevel
{
    Level const *raw;

    std::vector<Wall> walls;
    std::vector<RenderThing> things;
    std::vector<RenderFlat> floors;
    std::vector<RenderFlat> ceilings;
};

enum Weapon
{
    Fist = 0,
    Chainsaw = 1,
    Pistol = 2,
    Shotgun = 3,
    Chaingun = 4,
    RocketLauncher = 5,
    PlasmaRifle = 6,
    BFG9000 = 7
};

struct Player
{
    int bullets, max_bullets;
    int shells, max_shells;
    int rockets, max_rockets;
    int cells, max_cells;
    int health, armor;
    unsigned int weapon;
};

enum class State
{
    TitleScreen,
    InLevel,
};

struct GameState
{
    RenderGlobals &render_globals;

    State state;
    bool menu_open;

    std::string current_menuscreen;


    void transition(State newstate, bool newmenu)
    {
        state = newstate;
        menu_open = newmenu;
        if (menu_open)
        {
            current_menuscreen = "paused";
            SDL_SetRelativeMouseMode(SDL_FALSE);
            switch (state)
            {
            case State::InLevel:
                SDL_WarpMouseInWindow(
                    NULL,
                    render_globals.width / 2,
                    render_globals.height / 2);
                break;
            case State::TitleScreen:
                break;
            }
        }
        else
        {
            current_menuscreen = "";
            switch (state)
            {
            case State::InLevel:
                SDL_SetRelativeMouseMode(SDL_TRUE);
                break;
            case State::TitleScreen:
                SDL_SetRelativeMouseMode(SDL_FALSE);
                break;
            }
        }
    }
    void transition(State newstate)
    {
        transition(newstate, menu_open);
    }
    void transition(bool newmenu)
    {
        transition(state, newmenu);
    }


    GameState(RenderGlobals &g, State initial, bool menu_initial)
    :   render_globals{g},
        state{initial},
        menu_open{menu_initial}
    {
        transition(initial, menu_initial);
    }
};



GLTexture *picture2gltexture(Picture const &pic);
RenderLevel make_renderlevel(Level const &lvl, RenderGlobals &g);
uint16_t get_ssector(int16_t x, int16_t y, Level const &lvl);

void render_level(RenderLevel const &lvl, RenderGlobals const &g);
void render_node(
    uint16_t index,
    RenderLevel const &lvl,
    RenderGlobals const &g);
void render_ssector(
    uint16_t index,
    RenderLevel const &lvl,
    RenderGlobals const &g);
void render_hud(
    Player const &doomguy,
    WAD &wad,
    Mesh const &guiquad,
    Program const &guiprog,
    RenderGlobals &g);
void render_menu(
    Mesh const &guiquad,
    Program const &guiprog,
    GameState const &gs,
    RenderGlobals &g);



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



static std::vector<std::string> const menu_lump_names
{
    "M_DOOM",
    "M_RDTHIS",
    "M_OPTION",
    "M_QUITG",
    "M_NGAME",
    "M_SKULL1",
    "M_SKULL2",
    "M_THERMO",
    "M_THERMR",
    "M_THERMM",
    "M_THERML",
    "M_ENDGAM",
    "M_PAUSE",
    "M_MESSG",
    "M_MSGON",
    "M_MSGOFF",
    "M_EPISOD",
    "M_EPI1",
    "M_EPI2",
    "M_EPI3",
    "M_HURT",
    "M_JKILL",
    "M_ROUGH",
    "M_SKILL",
    "M_NEWG",
    "M_ULTRA",
    "M_NMARE",
    "M_SVOL",
    "M_OPTTTL",
    "M_SAVEG",
    "M_LOADG",
    "M_DISP",
    "M_MSENS",
    "M_GDHIGH",
    "M_GDLOW",
    "M_DETAIL",
    "M_DISOPT",
    "M_SCRNSZ",
    "M_SGTTL",
    "M_LGTTL",
    "M_SFXVOL",
    "M_MUSVOL",
    "M_LSLEFT",
    "M_LSCNTR",
    "M_LSRGHT"
};

static std::vector<std::string> const gui_lump_names
{
    "AMMNUM0",
    "AMMNUM1",
    "AMMNUM2",
    "AMMNUM3",
    "AMMNUM4",
    "AMMNUM5",
    "AMMNUM6",
    "AMMNUM7",
    "AMMNUM8",
    "AMMNUM9",
    "BRDR_TL",
    "BRDR_T",
    "BRDR_TR",
    "BRDR_L",
    "BRDR_R",
    "BRDR_BL",
    "BRDR_B",
    "BRDR_BR",
    "STBAR",
    "STGNUM0",
    "STGNUM1",
    "STGNUM2",
    "STGNUM3",
    "STGNUM4",
    "STGNUM5",
    "STGNUM6",
    "STGNUM7",
    "STGNUM8",
    "STGNUM9",
    "STTMINUS",
    "STTNUM0",
    "STTNUM1",
    "STTNUM2",
    "STTNUM3",
    "STTNUM4",
    "STTNUM5",
    "STTNUM6",
    "STTNUM7",
    "STTNUM8",
    "STTNUM9",
    "STTPRCNT",
    "STYSNUM0",
    "STYSNUM1",
    "STYSNUM2",
    "STYSNUM3",
    "STYSNUM4",
    "STYSNUM5",
    "STYSNUM6",
    "STYSNUM7",
    "STYSNUM8",
    "STYSNUM9",
    "STKEYS0",
    "STKEYS1",
    "STKEYS2",
    "STKEYS3",
    "STKEYS4",
    "STKEYS5",
    "STDISK",
    "STCDROM",
    "STARMS",
    "STCFN033",
    "STCFN034",
    "STCFN035",
    "STCFN036",
    "STCFN037",
    "STCFN038",
    "STCFN039",
    "STCFN040",
    "STCFN041",
    "STCFN042",
    "STCFN043",
    "STCFN044",
    "STCFN045",
    "STCFN046",
    "STCFN047",
    "STCFN048",
    "STCFN049",
    "STCFN050",
    "STCFN051",
    "STCFN052",
    "STCFN053",
    "STCFN054",
    "STCFN055",
    "STCFN056",
    "STCFN057",
    "STCFN058",
    "STCFN059",
    "STCFN060",
    "STCFN061",
    "STCFN062",
    "STCFN063",
    "STCFN064",
    "STCFN065",
    "STCFN066",
    "STCFN067",
    "STCFN068",
    "STCFN069",
    "STCFN070",
    "STCFN071",
    "STCFN072",
    "STCFN073",
    "STCFN074",
    "STCFN075",
    "STCFN076",
    "STCFN077",
    "STCFN078",
    "STCFN079",
    "STCFN080",
    "STCFN081",
    "STCFN082",
    "STCFN083",
    "STCFN084",
    "STCFN085",
    "STCFN086",
    "STCFN087",
    "STCFN088",
    "STCFN089",
    "STCFN090",
    "STCFN091",
    "STCFN092",
    "STCFN093",
    "STCFN094",
    "STCFN095",
    "STCFN121",
    "STFB1",
    "STFB0",
    "STFB2",
    "STFB3",
    "STPB1",
    "STPB0",
    "STPB2",
    "STPB3",
    "STFST01",
    "STFST00",
    "STFST02",
    "STFTL00",
    "STFTR00",
    "STFOUCH0",
    "STFEVL0",
    "STFKILL0",
    "STFST11",
    "STFST10",
    "STFST12",
    "STFTL10",
    "STFTR10",
    "STFOUCH1",
    "STFEVL1",
    "STFKILL1",
    "STFST21",
    "STFST20",
    "STFST22",
    "STFTL20",
    "STFTR20",
    "STFOUCH2",
    "STFEVL2",
    "STFKILL2",
    "STFST31",
    "STFST30",
    "STFST32",
    "STFTL30",
    "STFTR30",
    "STFOUCH3",
    "STFEVL3",
    "STFKILL3",
    "STFST41",
    "STFST40",
    "STFST42",
    "STFTL40",
    "STFTR40",
    "STFOUCH4",
    "STFEVL4",
    "STFKILL4",
    "STFGOD0",
    "STFDEAD0",
};

static std::vector<std::string> const fullscreen_lump_names
{
    "HELP1",
    "HELP2",
    "TITLEPIC",
    "CREDIT",
    "VICTORY2",
    "PFUB1",
    "PFUB2",
};

static std::unordered_map<
    std::string,
    std::vector<std::pair<std::string, glm::vec2>>> menuscreens
{
    {   "", {}},
    {   "paused",
        {
            {"M_DOOM"  , glm::vec2{0, 50}},
            {"M_NGAME" , glm::vec2{0, 105}},
            {"M_OPTION", glm::vec2{0, 122}},
            {"M_LOADG" , glm::vec2{0, 139}},
            {"M_SAVEG" , glm::vec2{0, 156}},
            {"M_QUITG" , glm::vec2{0, 174}},
        }
    },
};

static std::vector<std::pair<std::string, glm::vec2>> guidef
{
    /* status bar */
    {"STBAR"   , glm::vec2{   0, 224}},
    /* bullets */
    {"STYSNUM0", glm::vec2{ 117, 216}},
    {"STYSNUM5", glm::vec2{ 121, 216}},
    {"STYSNUM0", glm::vec2{ 125, 216}},
    {"STYSNUM2", glm::vec2{ 143, 216}},
    {"STYSNUM0", glm::vec2{ 147, 216}},
    {"STYSNUM0", glm::vec2{ 151, 216}},
    /* shells */
    {"STYSNUM0", glm::vec2{ 117, 222}},
    {"STYSNUM0", glm::vec2{ 121, 222}},
    {"STYSNUM0", glm::vec2{ 125, 222}},
    {"STYSNUM0", glm::vec2{ 143, 222}},
    {"STYSNUM5", glm::vec2{ 147, 222}},
    {"STYSNUM0", glm::vec2{ 151, 222}},
    /* rockets */
    {"STYSNUM0", glm::vec2{ 117, 228}},
    {"STYSNUM0", glm::vec2{ 121, 228}},
    {"STYSNUM0", glm::vec2{ 125, 228}},
    {"STYSNUM0", glm::vec2{ 143, 228}},
    {"STYSNUM5", glm::vec2{ 147, 228}},
    {"STYSNUM0", glm::vec2{ 151, 228}},
    /* cells */
    {"STYSNUM0", glm::vec2{ 117, 234}},
    {"STYSNUM0", glm::vec2{ 121, 234}},
    {"STYSNUM0", glm::vec2{ 125, 234}},
    {"STYSNUM3", glm::vec2{ 143, 234}},
    {"STYSNUM0", glm::vec2{ 147, 234}},
    {"STYSNUM0", glm::vec2{ 151, 234}},
    /* armor */
    {"STTPRCNT", glm::vec2{  68, 219}},
    {"STTNUM0" , glm::vec2{  26, 219}},
    {"STTNUM0" , glm::vec2{  40, 219}},
    {"STTNUM0" , glm::vec2{  54, 219}},
    /* face */
    {"STFST01" , glm::vec2{   0, 225}},
    /* arms panel */
    {"STARMS"  , glm::vec2{- 36, 224}},
    {"STYSNUM2", glm::vec2{- 47, 215}},
    {"STGNUM3" , glm::vec2{- 35, 215}},
    {"STGNUM4" , glm::vec2{- 23, 215}},
    {"STGNUM5" , glm::vec2{- 47, 225}},
    {"STGNUM6" , glm::vec2{- 35, 225}},
    {"STGNUM7" , glm::vec2{- 23, 225}},
    /* health */
    {"STTPRCNT", glm::vec2{- 63, 219}},
    {"STTNUM1" , glm::vec2{-105, 219}},
    {"STTNUM0" , glm::vec2{- 91, 219}},
    {"STTNUM0" , glm::vec2{- 77, 219}},
    /* current ammo */
    {"STTNUM6" , glm::vec2{-151, 219}},
    {"STTNUM6" , glm::vec2{-137, 219}},
    {"STTNUM6" , glm::vec2{-123, 219}},
};

/* 1st-person weapon sprites */
static std::vector<std::string> const hands
{
    "PUN",  /* fists */
    "SAW",  /* chainsaw */
    "PIS",  /* pistol */
    "SHT",  /* shotgun */
    "CHG",  /* chaingun */
    "MIS",  /* rocket launcher */
    "PLS",  /* plasma rifle */
    "BFG",  /* BFG 9000 */
};



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
            wad);
    }
    catch (std::out_of_range &e)
    {
        episode = 0;
        level = readlevel(
            "MAP" + std::to_string(episode) + std::to_string(mission),
            wad);
    }


    RenderGlobals g{};
    g.width  = 320;
    g.height = 240;

    Player doomguy{};
    doomguy.bullets = 50;
    doomguy.shells  = 0;
    doomguy.rockets = 0;
    doomguy.cells   = 0;
    doomguy.max_bullets = 200;
    doomguy.max_shells  = 50;
    doomguy.max_rockets = 50;
    doomguy.max_cells   = 300;
    doomguy.health = 100;
    doomguy.armor  = 0;
    doomguy.weapon = Weapon::Pistol;


    /* setup SDL stuff */
    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    SDL_Window *win = SDL_CreateWindow(
        __func__,
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        g.width, g.height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (win == nullptr)
    {
        fprintf(stderr, "failed to create window -- %s\n",
            SDL_GetError());
        exit(EXIT_FAILURE);
    }
    SDL_GLContext context = SDL_GL_CreateContext(win);
    if (context == nullptr)
    {
        fprintf(stderr, "failed to create context -- %s\n",
            SDL_GetError());
        exit(EXIT_FAILURE);
    }

    if (SDL_GL_SetSwapInterval(-1) == -1)
    {
        puts("VSync");
        SDL_GL_SetSwapInterval(1);
    }
    else
    {
        puts("Adaptive VSync");
    }

    int versionmajor = 0,
        versionminor = 0;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &versionmajor);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &versionminor);
    printf("OpenGL v%d.%d\n", versionmajor, versionminor);


    /* init OpenGL stuff */
    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        fprintf(stderr, "glewInit() -- %s\n",
            glewGetErrorString(err));
        exit(EXIT_FAILURE);
    }

    glViewport(0, 0, g.width, g.height);
    glClearColor(0, 0, 0, 1);
    glClearStencil(0);
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


    /* screen quad + shader */
    Mesh screenquad{
        {   {-1, -1, 0,  0, 0},
            {-1,  1, 0,  0, 1},
            { 1, -1, 0,  1, 0},
            { 1,  1, 0,  1, 1}},
        {2, 1, 0,  2, 3, 1}};
    Program screenprog{
        Shader{GL_VERTEX_SHADER, "shaders/screen.glvs"},
        Shader{GL_FRAGMENT_SHADER, "shaders/screen.glfs"}};


    /* GUI quad + shader */
    Mesh guiquad{
        {   {-1, -1, 0,  0, 1},
            {-1,  1, 0,  0, 0},
            { 1, -1, 0,  1, 1},
            { 1,  1, 0,  1, 0}},
        {2, 1, 0,  2, 3, 1}};
    Program guiprog{
        Shader{GL_VERTEX_SHADER, "shaders/gui.glvs"},
        Shader{GL_FRAGMENT_SHADER, "shaders/fragment.glfs"}};

    /* set up the screen framebuffer */
    GLuint screenframebuffer = 0;
    GLuint screentexture = 0;
    GLuint screendepthstencil = 0;
    glGenFramebuffers(1, &screenframebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, screenframebuffer);

    glGenTextures(1, &screentexture);
    glBindTexture(GL_TEXTURE_2D, screentexture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGB,
        g.width, g.height,
        0,
        GL_RGB,
        GL_UNSIGNED_BYTE,
        nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        screentexture,
        0);

    glGenRenderbuffers(1, &screendepthstencil);
    glBindRenderbuffer(GL_RENDERBUFFER, screendepthstencil);
    glRenderbufferStorage(
        GL_RENDERBUFFER,
        GL_DEPTH24_STENCIL8,
        g.width, g.height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER,
        GL_DEPTH_STENCIL_ATTACHMENT,
        GL_RENDERBUFFER,
        screendepthstencil);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER)
        != GL_FRAMEBUFFER_COMPLETE)
    {
        fputs("Failed to create screenframebuffer\n", stderr);
        exit(EXIT_FAILURE);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);


    /* set up the palette */
    g.palette_number = 0;
    auto palette = new uint8_t[14][256][3];
    for (size_t i = 0; i < 14; ++i)
    {
        auto &pal = wad.palettes[i];
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
    for (auto &pair : wad.textures)
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
    for (auto &pair : wad.flats)
    {
        auto &name = pair.first;
        auto &flat = pair.second;

        auto imgdata = new uint32_t[flat.size()];
        for (size_t i = 0; i < flat.size(); ++i)
        {
            imgdata[i] = 0xFF00 | flat[i];
        }
        g.flats.emplace(
            name,
            new GLTexture{64, 64, imgdata});
        delete[] imgdata;
    }

    /* make GLTextures from the sprites */
    for (auto &pair : wad.sprites)
    {
        auto &name = pair.first;
        auto &sprite = pair.second;
        g.sprites.emplace(name, picture2gltexture(sprite));
    }

    /* load the GUI pictures */
    for (auto &name : gui_lump_names)
    {
        auto picture = loadpicture(wad.findlump(name));
        g.gui_images.emplace(name, picture2gltexture(picture));
    }

    /* load the menu pictures */
    for (auto &name : menu_lump_names)
    {
        auto picture = loadpicture(wad.findlump(name));
        g.menu_images.emplace(name, picture2gltexture(picture));
    }

    /* load the fullscreen pictures */
    for (auto &name : fullscreen_lump_names)
    {
        try
        {
            g.menu_images.emplace(
                name,
                picture2gltexture(loadpicture(wad.findlump(name))));
        }
        catch (std::out_of_range &e)
        {
        }
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


    GameState gs{g, State::TitleScreen, false};

    /* set up the level's render data */
    auto renderlevel = make_renderlevel(level, g);

    glm::vec3 delta{0};
    SDL_Event e;
    for (bool running = true; running; )
    {
        while (SDL_PollEvent(&e) && running)
        {
            /* events that don't depend on the
             * game state are handled here */
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

                    /* realloc the screen framebuffer */
                    glDeleteRenderbuffers(1, &screendepthstencil);
                    glDeleteTextures(1, &screentexture);
                    glDeleteFramebuffers(1, &screenframebuffer);

                    glGenFramebuffers(1, &screenframebuffer);
                    glBindFramebuffer(
                        GL_FRAMEBUFFER,
                        screenframebuffer);

                    glGenTextures(1, &screentexture);
                    glBindTexture(GL_TEXTURE_2D, screentexture);
                    glTexImage2D(
                        GL_TEXTURE_2D,
                        0,
                        GL_RGB,
                        g.width, g.height,
                        0,
                        GL_RGB,
                        GL_UNSIGNED_BYTE,
                        nullptr);
                    glTexParameteri(
                        GL_TEXTURE_2D,
                        GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR);
                    glTexParameteri(
                        GL_TEXTURE_2D,
                        GL_TEXTURE_MAG_FILTER,
                        GL_LINEAR);
                    glFramebufferTexture2D(
                        GL_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0,
                        GL_TEXTURE_2D,
                        screentexture,
                        0);

                    glGenRenderbuffers(1, &screendepthstencil);
                    glBindRenderbuffer(
                        GL_RENDERBUFFER,
                        screendepthstencil);
                    glRenderbufferStorage(
                        GL_RENDERBUFFER,
                        GL_DEPTH24_STENCIL8,
                        g.width, g.height);
                    glBindRenderbuffer(GL_RENDERBUFFER, 0);
                    glFramebufferRenderbuffer(
                        GL_FRAMEBUFFER,
                        GL_DEPTH_STENCIL_ATTACHMENT,
                        GL_RENDERBUFFER,
                        screendepthstencil);

                    if (   glCheckFramebufferStatus(GL_FRAMEBUFFER)
                        != GL_FRAMEBUFFER_COMPLETE)
                    {
                        fprintf(stderr,
                            "Failed to realloc screenframebuffer"
                            " (%dx%d)\n",
                            g.width, g.height);
                        exit(EXIT_FAILURE);
                    }
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                }
                break;
            case SDL_KEYUP:
                if (e.key.keysym.sym == SDLK_ESCAPE)
                {
                    gs.transition(!gs.menu_open);
                }
                break;
            }
            /* === END GAME STATE INDEPENDENT EVENT HANDLING === */
            if (gs.menu_open)
            {
                /* these actions are taken if the menu is open*/
                switch (e.type)
                {
                case SDL_MOUSEBUTTONDOWN:
                    switch (gs.state)
                    {
                    case State::TitleScreen:
                        gs.transition(State::InLevel, false);
                        break;
                    default:
                        running = false;
                        break;
                    }
                    break;
                }
                /* === END MENU EVENT HANDLING === */
            }
            else
            {
                /* these actions are taken if the menu is NOT open */
                switch (gs.state)
                {
                case State::InLevel:
                    switch (e.type)
                    {
                    case SDL_MOUSEMOTION:
                        g.cam.rotate(
                            -e.motion.xrel / 10.0,
                            -e.motion.yrel / 10.0);
                        break;

                    case SDL_MOUSEBUTTONDOWN:
                        switch (doomguy.weapon)
                        {
                        case Weapon::Fist:
                        case Weapon::Chainsaw:
                            break;
                        case Weapon::Pistol:
                        case Weapon::Chaingun:
                            if (doomguy.bullets > 0)
                            {
                                doomguy.bullets -= 1;
                            }
                            break;
                        case Weapon::Shotgun:
                            if (doomguy.shells > 0)
                            {
                                doomguy.shells -= 1;
                            }
                            break;
                        case Weapon::RocketLauncher:
                            if (doomguy.rockets > 0)
                            {
                                doomguy.rockets -= 1;
                            }
                            break;
                        case Weapon::PlasmaRifle:
                        case Weapon::BFG9000:
                            if (doomguy.cells > 0)
                            {
                                doomguy.cells -= 1;
                            }
                            break;
                        }
                        break;

                    case SDL_MOUSEWHEEL:
                        doomguy.weapon -= e.wheel.y;
                        doomguy.weapon %= 8;
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
                        case SDLK_1:
                            if (doomguy.weapon == Weapon::Chainsaw)
                            {
                                doomguy.weapon = Weapon::Fist;
                            }
                            else
                            {
                                doomguy.weapon = Weapon::Chainsaw;
                            }
                            break;
                        case SDLK_2:
                            doomguy.weapon = Weapon::Pistol;
                            break;
                        case SDLK_3:
                            doomguy.weapon = Weapon::Shotgun;
                            break;
                        case SDLK_4:
                            doomguy.weapon = Weapon::Chaingun;
                            break;
                        case SDLK_5:
                            doomguy.weapon = Weapon::RocketLauncher;
                            break;
                        case SDLK_6:
                            doomguy.weapon = Weapon::PlasmaRifle;
                            break;
                        case SDLK_7:
                            doomguy.weapon = Weapon::BFG9000;
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
                            try
                            {
                                level = readlevel(
                                    (   "E"
                                        + std::to_string(episode)
                                        + "M"
                                        + std::to_string(mission)),
                                    wad);
                            }
                            catch (std::out_of_range &e)
                            {
                                level = readlevel(
                                    (   "MAP"
                                        + std::to_string(episode)
                                        + std::to_string(mission)),
                                    wad);
                            }
                            renderlevel =\
                                make_renderlevel(level, g);

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
                    break;
                /* === END InLevel EVENT HANDLING === */
                case State::TitleScreen:
                    switch (e.type)
                    {
                    case SDL_MOUSEBUTTONDOWN:
                        gs.transition(true);
                        break;
                    }
                    break;
                /* === END TitleScreen EVENT HANDLING === */
                }
            }
        }

        if (gs.state == State::InLevel)
        {
            /* update the game state */
            g.cam.move(delta);
            int ssector = -1;
            try
            {
                ssector =\
                    get_ssector(-g.cam.pos.x, g.cam.pos.z, level);
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

            /* update the GUI numbers */
            int ammo = 666;
            switch (doomguy.weapon)
            {
            case Weapon::Fist:
            case Weapon::Chainsaw:
                break;
            case Weapon::Pistol:
            case Weapon::Chaingun:
                ammo = doomguy.bullets;
                break;
            case Weapon::Shotgun:
                ammo = doomguy.shells;
                break;
            case Weapon::RocketLauncher:
                ammo = doomguy.rockets;
                break;
            case Weapon::PlasmaRifle:
            case Weapon::BFG9000:
                ammo = doomguy.cells;
                break;
            }
            std::vector<std::string> imgs{
                "STYSNUM", "STYSNUM", "STYSNUM", "STYSNUM",
                "STYSNUM", "STYSNUM", "STYSNUM", "STYSNUM",
                "STTNUM", "STTNUM", "STTNUM"};
            std::vector<int> values{
                doomguy.bullets,
                doomguy.max_bullets,
                doomguy.shells,
                doomguy.max_shells,
                doomguy.rockets,
                doomguy.max_rockets,
                doomguy.cells,
                doomguy.max_cells,
                doomguy.armor,
                doomguy.health,
                ammo};
            std::vector<int> offsets{1,4,7,10,13,16,19,22,26,38,41};
            for (size_t j = 0; j < values.size(); ++j)
            {
                char digits[3];
                for (size_t i = 0; i < 3; ++i)
                {
                    int digit = (int)(
                            values[j]
                            / pow(10, 2 - i)
                        ) % 10;
                    digits[i] = digit;
                    if (digit == 0)
                    {
                        if (i == 0 || (i == 1 && digits[i-1] == 0))
                        {
                            guidef[offsets[j]+i].first = "";
                            continue;
                        }
                    }
                    guidef[offsets[j]+i].first =\
                        imgs[j]
                        + std::string{(char)('0' + digit)};
                }
            }

            /* face */
            guidef[29].first =\
                "STFST"
                + std::string{
                    (char)('4' - glm::min(100, doomguy.health) / 25)}
                + "1";

            /* arms panel */
            for (size_t i = 2; i <= 7; ++i)
            {
                guidef[31 + (i - 2)].first =\
                    (doomguy.weapon == i? "STYSNUM" : "STGNUM")
                    + std::string{(char)('0' + i)};
            }
        }


        /* render the scene into the framebuffer */
        glBindFramebuffer(GL_FRAMEBUFFER, screenframebuffer);
            glClear(
                GL_COLOR_BUFFER_BIT
                | GL_DEPTH_BUFFER_BIT
                | GL_STENCIL_BUFFER_BIT);
        switch (gs.state)
        {
        case State::InLevel:
            /* draw the first person view */
            glEnable(GL_DEPTH_TEST);
            render_level(renderlevel, g);
            /* draw the HUD */
            glDisable(GL_DEPTH_TEST);
            render_hud(doomguy, wad, guiquad, guiprog, g);
            break;

        case State::TitleScreen:
            glDisable(GL_DEPTH_TEST);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, g.palette_id);
            glActiveTexture(GL_TEXTURE1);

            auto &img = g.menu_images["TITLEPIC"];

            double const aspect_h = img->height;
            double const aspect_w =\
                (g.width / (double)g.height) * aspect_h;

            double const w = img->width / aspect_w;
            double const h = img->height / aspect_h;

            guiprog.use();
            guiprog.set("palettes", 0);
            guiprog.set("palette", 0);
            guiprog.set("tex", 1);
            guiprog.set("xoffset", 0);
            guiprog.set("yoffset", 0);
            guiprog.set("position",
                glm::scale(glm::mat4{1}, glm::vec3{w, h, 1}));

            img->bind();
            guiquad.bind();
            glDrawElements(
                GL_TRIANGLES,
                guiquad.size(),
                GL_UNSIGNED_INT,
                0);
            break;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        /* draw the framebuffer to the screen */
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, screentexture);

        screenprog.use();
        screenprog.set("camera", glm::mat4(1));
        screenprog.set("projection", glm::mat4(1));
        screenprog.set("screen", (GLuint)0);
        screenquad.bind();
        glDrawElements(
            GL_TRIANGLES,
            screenquad.size(),
            GL_UNSIGNED_INT,
            0);

        /* overlay the menu */
        render_menu(guiquad, guiprog, gs, g);


        SDL_GL_SwapWindow(win);
        SDL_Delay(1000 / 60);
    }

    auto &exittext = wad.findlump("ENDOOM");
    for (size_t y = 0; y < 25; ++y)
    {
        for (size_t x = 0; x < 80; ++x)
        {
            putchar(exittext.data.get()[(y * 160) + (x * 2)]);
        }
        putchar('\n');
    }


    /* cleanup */
    glDeleteRenderbuffers(1, &screendepthstencil);
    glDeleteTextures(1, &screentexture);
    glDeleteFramebuffers(1, &screenframebuffer);

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(win);
    SDL_Quit();
    fclose(wadfile);

    return EXIT_SUCCESS;
}



GLTexture *picture2gltexture(Picture const &p)
{
    auto data = new uint32_t[p.data.size()];
    for (size_t i = 0; i < p.data.size(); ++i)
    {
        data[i] = ((p.opaque[i]? 0xFF : 0) << 8) | p.data[i];
    }
    auto gltexture = new GLTexture{p.width, p.height, data};
    delete[] data;
    return gltexture;
}

RenderLevel make_renderlevel(Level const &lvl, RenderGlobals &g)
{
    RenderLevel out{};
    out.raw = &lvl;

    /* make RenderThings from things */
    for (auto &thing : lvl.things)
    {
        /* TODO: set thing options filter externally */
        if (thing.options & SKILL3 && !(thing.options & MP_ONLY))
        {
            auto &data = thingdata[thing.type];
            std::string sprname = "";
            switch (data.frames)
            {
            case -1:
                /* no image */
                break;
            case 0:
                sprname = data.sprite + "A1";
                break;
            default:
                if (data.frames > 0)
                {
                    sprname = data.sprite + "A0";
                }
                else
                {
                    sprname =\
                        data.sprite
                        + std::string{
                            1,
                            (char)('A' + (-data.frames - 2))}
                        + "0";
                }
                break;
            }
            sprname = sprname;

            /* get the thing's y position
             * (ie. the floor height of the sector it's inside) */
            int ssector = -1;
            double y = 0;
            try
            {
                ssector = get_ssector(thing.x, thing.y, lvl);
            }
            catch (std::runtime_error &e)
            {
            }
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
            if (sprname != "")
            {
                auto &spr = lvl.wad->sprites[sprname];

                /* TODO: figure out the top offset */
                GLfloat w = spr.width,
                        h = spr.height;
                out.things.emplace_back(
                    g.sprites[sprname].get(),
                    new Mesh{{
                        {(GLfloat)-spr.left+0, 0, 0,  0, 1},
                        {(GLfloat)-spr.left+w, 0, 0,  1, 1},
                        {(GLfloat)-spr.left+w, h, 0,  1, 0},
                        {(GLfloat)-spr.left+0, h, 0,  0, 0}},
                        {0,1,2, 2,3,0}},
                    glm::vec3(-thing.x, y, thing.y));
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

    std::unordered_map<
        SSector const *,
        std::vector<Node const *>> ssnodes{};
    for (auto &nd : lvl.nodes)
    {
        if (nd.right & 0x8000)
        {
            ssnodes[&lvl.ssectors[nd.right & 0x7FFF]].push_back(&nd);
        }
        if (nd.left & 0x8000)
        {
            ssnodes[&lvl.ssectors[nd.left & 0x7FFF]].push_back(&nd);
        }
    }

    return out;
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



void render_level(RenderLevel const &lvl, RenderGlobals const &g)
{
    /* draw the walls */
    render_node(lvl.raw->nodes.size() - 1, lvl, g);


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

void render_node(
    uint16_t index,
    RenderLevel const &lvl,
    RenderGlobals const &g)
{
    Node const &node = lvl.raw->nodes[index];
    if (node.right & 0x8000)
    {
        render_ssector(node.right & 0x7FFF, lvl, g);
    }
    else
    {
        render_node(node.right, lvl, g);
    }
    if (node.left & 0x8000)
    {
        render_ssector(node.left & 0x7FFF, lvl, g);
    }
    else
    {
        render_node(node.left, lvl, g);
    }
}

void render_ssector(
    uint16_t index,
    RenderLevel const &lvl,
    RenderGlobals const &g)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g.palette_id);
    glActiveTexture(GL_TEXTURE1);

    g.program->use();
    g.program->set("camera", g.cam.matrix());
    g.program->set("projection", g.projection);
    g.program->set("palettes", 0);
    g.program->set("palette", g.palette_number);
    g.program->set("tex", 1);

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

void render_hud(
    Player const &doomguy,
    WAD &wad,
    Mesh const &guiquad,
    Program const &guiprog,
    RenderGlobals &g)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g.palette_id);
    glActiveTexture(GL_TEXTURE1);

    guiprog.use();
    guiprog.set("palettes", 0);
    guiprog.set("palette", 0);
    guiprog.set("tex", 1);
    guiprog.set("xoffset", 0);
    guiprog.set("yoffset", 0);
    guiquad.bind();

    double const aspect_h = 240.0;
    double const aspect_w = (g.width / (double)g.height) * aspect_h;

    /* weapon sprite */
    std::string const sprname = hands[doomguy.weapon] + "GA0";
    auto &img = g.sprites[sprname];
    auto &spr = wad.sprites[sprname];

    double const w = img->width / aspect_w;
    double const h = img->height / aspect_h;

    glm::vec2 const offset = glm::vec2{
        (((-spr.left) + (spr.width / 2)) / 160.0) - 1,
        ((((-spr.top) + (spr.height / 2)) / 83.5) * -1) + 1};

    guiprog.set("position",
        glm::scale(
            glm::translate(
                glm::mat4{1},
                glm::vec3{offset, 0}),
            glm::vec3{w, h, 1}));

    img->bind();
    glDrawElements(
        GL_TRIANGLES,
        guiquad.size(),
        GL_UNSIGNED_INT,
        0);

    /* HUD overlay */
    for (auto &imgpair : guidef)
    {
        if (imgpair.first == "")
        {
            continue;
        }
        auto &img = g.gui_images[imgpair.first];
        glm::vec2 offset = imgpair.second;

        double const w = img->width / aspect_w;
        double const h = img->height / aspect_h;

        offset.x /= aspect_w / 2;
        offset.y = ((offset.y / 120.0) * -1) + 1;

        guiprog.set("position",
            glm::scale(
                glm::translate(
                    glm::mat4{1},
                    glm::vec3{offset, 0}),
                glm::vec3{w, h, 1}));

        img->bind();
        glDrawElements(
            GL_TRIANGLES,
            guiquad.size(),
            GL_UNSIGNED_INT,
            0);
    }
}

void render_menu(
    Mesh const &guiquad,
    Program const &guiprog,
    GameState const &gs,
    RenderGlobals &g)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g.palette_id);
    glActiveTexture(GL_TEXTURE1);

    guiprog.use();
    guiprog.set("palettes", 0);
    guiprog.set("palette", 0);
    guiprog.set("tex", 1);
    guiprog.set("xoffset", 0);
    guiprog.set("yoffset", 0);
    guiquad.bind();

    double const aspect_h = 240.0;
    double const aspect_w = (g.width / (double)g.height) * aspect_h;

    for (auto &imgpair : menuscreens[gs.current_menuscreen])
    {
        auto &img = g.menu_images[imgpair.first];
        glm::vec2 offset{
            imgpair.second.x / 160.0,
            ((imgpair.second.y / 120.0) * -1) + 1};

        double const w = img->width / aspect_w;
        double const h = img->height / aspect_h;

        guiprog.set("position",
            glm::scale(
                glm::translate(
                    glm::mat4{1},
                    glm::vec3{offset, 0}),
                glm::vec3{w, h, 1}));

        img->bind();
        glDrawElements(
            GL_TRIANGLES,
            guiquad.size(),
            GL_UNSIGNED_INT,
            0);
    }
}

