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

struct RenderLevel
{
    Level const *raw;

    std::vector<Wall> walls;
    std::vector<RenderThing> things;
    std::vector<RenderFlat> floors;
    std::vector<RenderFlat> ceilings;
    std::vector<std::unique_ptr<Mesh>> automap;
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

    std::vector<size_t> glance;
    size_t glance_idx;
    size_t glancecounter;
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
    bool automap_open;
    Uint64 last_update;

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
                automap_open = false;
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
                last_update = SDL_GetPerformanceCounter();
                break;
            case State::TitleScreen:
                SDL_SetRelativeMouseMode(SDL_FALSE);
                automap_open = false;
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



static unsigned long long frames_cumulative = 0;
static size_t seconds_count = 0;
static size_t frames_per_second = 0;
Uint32 callback1hz(Uint32 interval, void *param)
{
    printf("%lufps\n", frames_per_second);
    frames_cumulative += frames_per_second;
    seconds_count++;
    frames_per_second = 0;
    return interval;
}

Uint32 callback35hz(Uint32 interval, void *param)
{
    static size_t updatecount = 0;
    updatecount++;
    if (updatecount >= 7)
    {
        SDL_Event e;
        e.type = SDL_USEREVENT;
        e.user.code = 0;
        SDL_PushEvent(&e);
        updatecount = 0;
    }
    return interval;
}

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
void render_automap(RenderLevel const &lvl, RenderGlobals const &g);



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

static std::unique_ptr<Mesh> automap_cursor{nullptr};
static std::unique_ptr<Mesh> thingquad{nullptr};



int main(int argc, char *argv[])
{
    if (argc <= 1)
    {
        fprintf(stderr, "No .WAD given!\n");
        exit(EXIT_FAILURE);
    }

    FILE *wadfile = fopen(argv[1], "r");
    if (wadfile == nullptr)
    {
        fprintf(stderr, "Failed to open %s -- (%s)\n",
            argv[1],
            strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* read the IWAD */
    auto wad = loadIWAD(wadfile);
    fclose(wadfile);

    /* read PWADs and patch the IWAD */
    if (argc >= 3)
    {
        for (int i = 0; i < argc - 2; ++i)
        {
            FILE *f = fopen(argv[2 + i], "r");
            if (f == nullptr)
            {
                fprintf(stderr, "Failed to open %s -- (%s)\n",
                    argv[2 + i],
                    strerror(errno));
                exit(EXIT_FAILURE);
            }
            patchWAD(wad, f);
            fclose(f);
        }
    }
    readwad(wad);

    /* load the first level */
    int episode = 1,
        mission = 1;

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

    doomguy.glance = {1,0,1,2};
    doomguy.glance_idx = 0;
    doomguy.glancecounter = 0;


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
        glm::vec3{0, 0, 0},
        glm::vec3{0, 0, 1},
        glm::vec3{0, 1, 0}};

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
    g.automap_program.reset(new Program{
        Shader{GL_VERTEX_SHADER, "shaders/2d-vertex.glvs"},
        Shader{GL_FRAGMENT_SHADER, "shaders/color.glfs"}});


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

    /* mesh for the automap cursor */
    automap_cursor.reset(
        new Mesh{
            { 0.00,+0.020,0, 0,0},
            { 0.00,-0.020,0, 0,0},
            { 0.00,+0.020,0, 0,0},
            {-0.01,-0.005,0, 0,0},
            { 0.00,+0.020,0, 0,0},
            { 0.01,-0.005,0, 0,0}});

    /* quad used for rendering Things */
    thingquad.reset(
        new Mesh{
            {
                {-0.5,0,0, 0,1},
                {-0.5,1,0, 0,0},
                { 0.5,0,0, 1,1},
                { 0.5,1,0, 1,0},
            },
            {0,2,1, 1,2,3}});

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
    glGenTextures(1, &g.palette_texture);
    glBindTexture(GL_TEXTURE_2D, g.palette_texture);
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


    /* set up the colormap */
    auto colormap = new uint8_t[256 * 34];
    auto &dir = wad.findlump("COLORMAP");
    dir.seek(0, SEEK_SET);
    dir.read(colormap, 256 * 34);

    glGenTextures(1, &g.colormap_texture);
    glBindTexture(GL_TEXTURE_2D, g.colormap_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_R8UI,
        256, 34,
        0,
        GL_RED_INTEGER,
        GL_UNSIGNED_BYTE,
        colormap);
    delete[] colormap;


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

    /* FPS timer */
    auto timer1hz = SDL_AddTimer(1000, callback1hz, nullptr);
    /* game update timer */
    auto timer35hz = SDL_AddTimer(1000 / 35, callback35hz, nullptr);

    float const speed = 256;
    bool delta[4] = {false,false,false,false};
    bool animation_update = false;
    Uint64 const freq = SDL_GetPerformanceFrequency();

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
                    case SDL_USEREVENT:
                        switch (e.user.code)
                        {
                        case 0:
                            animation_update = true;
                            break;
                        }
                        break;

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
                            delta[0] = true;
                            break;
                        case SDLK_a:
                            delta[1] = true;
                            break;
                        case SDLK_w:
                            delta[2] = true;
                            break;
                        case SDLK_s:
                            delta[3] = true;
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
                        case SDLK_TAB:
                            gs.automap_open = !gs.automap_open;
                            break;
                        }
                        break;

                    case SDL_KEYUP:
                        switch (e.key.keysym.sym)
                        {
                        case SDLK_d:
                            delta[0] = false;
                            break;
                        case SDLK_a:
                            delta[1] = false;
                            break;
                        case SDLK_w:
                            delta[2] = false;
                            break;
                        case SDLK_s:
                            delta[3] = false;
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

        /* update the game */
        Uint64 now = SDL_GetPerformanceCounter();
        if (gs.state == State::InLevel && !gs.menu_open)
        {
            double dx = delta[0] - delta[1],
                   dz = delta[2] - delta[3];
            if (dx != 0 || dz != 0)
            {
                float deltatime = (now - gs.last_update) / (float)freq;

                g.cam.move(
                    speed
                    * deltatime
                    * glm::normalize(glm::vec3{dx, 0, dz}));
            }
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

            /* arms panel */
            for (size_t i = 2; i <= 7; ++i)
            {
                guidef[31 + (i - 2)].first =\
                    (doomguy.weapon == i? "STYSNUM" : "STGNUM")
                    + std::string{(char)('0' + i)};
            }
        }
        gs.last_update = now;
        if (gs.state == State::InLevel && !gs.menu_open && animation_update)
        {
            animation_update = false;

            if (++doomguy.glancecounter >= 10)
            {
                doomguy.glancecounter = 0;
                doomguy.glance_idx++;
                doomguy.glance_idx %= doomguy.glance.size();
            }

            /* HUD Doomguy face */
            /* TODO: animation */
            guidef[29].first =\
                "STFST"
                + std::string{
                    (char)(
                        '4'
                        - glm::min(100, doomguy.health) / 25)}
                + std::string{
                    (char)('0' + doomguy.glance[doomguy.glance_idx])};

            /* animate the Things */
            for (auto &thing : renderlevel.things)
            {
                if (   thing.framecount != -1
                    && !thing.sprites.empty())
                {
                    if (thing.cleanloop)
                    {
                        if (thing.reverse_anim)
                        {
                            thing.frame_idx--;
                            if (thing.frame_idx < 0)
                            {
                                thing.frame_idx = 1;
                                thing.reverse_anim =\
                                    false;
                            }
                        }
                        else
                        {
                            thing.frame_idx++;
                            if (   thing.frame_idx
                                >= thing.framecount)
                            {
                                thing.frame_idx -= 2;
                                thing.reverse_anim =\
                                    true;
                            }
                        }
                    }
                    else
                    {
                        thing.frame_idx++;
                        thing.frame_idx %=\
                            thing.framecount;
                    }
                }
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
            /* draw the automap */
            glDisable(GL_DEPTH_TEST);
            if (gs.automap_open)
            {
                render_automap(renderlevel, g);
            }
            /* draw the HUD */
            glDisable(GL_DEPTH_TEST);
            render_hud(doomguy, wad, guiquad, guiprog, g);
            break;

        case State::TitleScreen:
            glDisable(GL_DEPTH_TEST);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, g.palette_texture);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, g.colormap_texture);
            glActiveTexture(GL_TEXTURE1);

            auto &img = g.menu_images["TITLEPIC"];

            double aspect_w = img->width;
            double aspect_h = (g.height / (double)g.width) * aspect_w;
            if (  g.height    / (double)g.width
                < img->height / (double)img->width)
            {
                aspect_h = img->height;
                aspect_w = (g.width / (double)g.height) * aspect_h;
            }

            double const w = img->width / aspect_w;
            double const h = img->height / aspect_h;

            guiprog.use();
            guiprog.set("palettes", 0);
            guiprog.set("palette_idx", 0);
            guiprog.set("colormap", 2);
            guiprog.set("colormap_idx", 0);
            guiprog.set("tex", 1);
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
        screenprog.set("camera", glm::mat4{1});
        screenprog.set("projection", glm::mat4{1});
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
        frames_per_second++;
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
    SDL_RemoveTimer(timer1hz);
    SDL_RemoveTimer(timer35hz);

    glDeleteRenderbuffers(1, &screendepthstencil);
    glDeleteTextures(1, &screentexture);
    glDeleteFramebuffers(1, &screenframebuffer);

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(win);
    SDL_Quit();

    printf("Average FPS: %g\n",
        (double)frames_cumulative / (double)seconds_count);

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
            out.things.push_back(RenderThing{});
            RenderThing &rt = out.things.back();
            rt.angle = thing.angle;

            auto &data = thingdata[thing.type];
            std::unordered_map<
                std::string,
                std::pair<std::string, bool>> sprindices{};
            switch (data.frames)
            {
            /* no image */
            case -1:
                break;
            /* has angled views */
            case 0:
              {
                auto sprlumps = lvl.wad->findall(data.sprite);
                for (char rot = '1'; rot <= '8'; ++rot)
                {
                    auto idx = std::string{'A'} + std::string{rot};
                    for (auto &lump : sprlumps)
                    {
                        auto sprname = std::string{lump.name};
                        if (sprname[4] == 'A' && sprname[5] == rot)
                        {
                            sprindices[idx] = {sprname, true};
                            break;
                        }
                        else if (  sprname[6] == 'A'
                                && sprname[7] == rot)
                        {
                            sprindices[idx] = {sprname, false};
                            break;
                        }
                    }
                }
                rt.angled = true;
                rt.cleanloop = false;
                /* TODO: this is set per-thing? */
                rt.framecount = 1;
              } break;
            /* no angled views */
            default:
                if (data.frames > 0)
                {
                    for (int i = 0; i < data.frames; ++i)
                    {
                        std::string frame{(char)('A' + i)};
                        sprindices[frame + "0"] =\
                            {data.sprite + frame + "0", false};
                        rt.angled = false;
                        rt.cleanloop = data.cleanloop;
                        rt.framecount = data.frames;
                    }
                }
                else
                {
                    std::string frame{
                        (char)('A' - (data.frames + 2))};
                    sprindices[frame + "0"] =\
                        {data.sprite + frame + "0", false};
                    rt.angled = false;
                    rt.cleanloop = false;
                    rt.framecount = -1;
                    rt.frame_idx = -(data.frames + 2);
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
            if (ssector != -1)
            {
                auto &seg = lvl.segs[lvl.ssectors[ssector].start];
                auto &ld = lvl.linedefs[seg.linedef];
                rt.sector =\
                    (seg.direction? ld.left : ld.right)->sector;
            }

            /* set the thing's mesh and position */
            for (auto &p1 : sprindices)
            {
                auto &idx = p1.first;
                auto &p2 = p1.second;
                auto &sprname = p2.first;
                bool flipx = p2.second;

                auto &spr = lvl.wad->sprites[sprname];

                rt.sprites[idx].tex = g.sprites[sprname].get();
                rt.sprites[idx].flipx = flipx;
                rt.sprites[idx].offset.x = spr.left;
                rt.sprites[idx].offset.y = spr.top;
            }
            rt.pos = glm::vec3{-thing.x, rt.sector->floor, thing.y};
        }
    }

    /* create Walls from the segs */
    /* TODO: animated walls */
    for (auto &seg : lvl.segs)
    {
        auto &ld = lvl.linedefs[seg.linedef];
        auto &side = seg.direction? ld.left : ld.right;
        auto &opp  = seg.direction? ld.right : ld.left;

        out.walls.emplace_back(nullptr, nullptr);

        /* middle */
        if (!(ld.flags & TWOSIDED)
            || side->middle != "-")
        {
            int top = side->sector->ceiling;
            int bot = side->sector->floor;

            std::string texname = tolowercase(side->middle);
            auto &tex = g.textures[texname];
            out.walls.back().middletex = tex.get();

            double const tw = tex->width,
                         th = tex->height;

            double len =\
                sqrt(
                    pow(seg.end->x - seg.start->x, 2)
                    + pow(seg.end->y - seg.start->y, 2))
                / tw;
            double hgt = abs(top - bot) / th;

            bool unpegged = ld.flags & UNPEGGEDLOWER;
            double sx = (seg.offset + side->x) / tw,
                   sy = (side->y / th) + (unpegged? -hgt : 0);
            double ex = sx + len,
                   ey = sy + hgt;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
            out.walls.back().middlemesh.reset(new Mesh{
                {
                    {-seg.start->x,bot,seg.start->y,  sx,ey},
                    {-seg.end->x  ,bot,seg.end->y  ,  ex,ey},
                    {-seg.end->x  ,top,seg.end->y  ,  ex,sy},
                    {-seg.start->x,top,seg.start->y,  sx,sy},
                },
                {0,1,2, 2,3,0}});
#pragma GCC diagnostic pop
        }
        if (ld.flags & TWOSIDED)
        {
            /* lower section */
            if (side->sector->floor < opp->sector->floor)
            {
                int top = opp->sector->floor;
                int bot = side->sector->floor;

                std::string texname = tolowercase(side->lower);
                if (texname != "-")
                {
                    auto &tex = g.textures[texname];
                    out.walls.back().lowertex = tex.get();

                    double const tw = tex->width,
                                 th = tex->height;

                    double len =\
                        sqrt(
                            pow(seg.end->x - seg.start->x, 2)
                            + pow(seg.end->y - seg.start->y, 2))
                        / tw;
                    double hgt = abs(top - bot) / th;

                    bool unpegged = ld.flags & UNPEGGEDLOWER;
                    double sx = (seg.offset + side->x) / tw,
                           sy = side->y / th;
                    double ex = sx + len,
                           ey = sy + hgt;

                    if (unpegged)
                    {
                        double offset =\
                            (   glm::max(
                                    side->sector->ceiling,
                                    opp->sector->ceiling)
                                - top)
                            / th;
                        sy += offset;
                        ey += offset;
                    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
                    out.walls.back().lowermesh.reset(new Mesh{
                        {
                            {-seg.start->x,bot,seg.start->y, sx,ey},
                            {-seg.end->x  ,bot,seg.end->y  , ex,ey},
                            {-seg.end->x  ,top,seg.end->y  , ex,sy},
                            {-seg.start->x,top,seg.start->y, sx,sy},
                        },
                        {0,1,2, 2,3,0}});
                }
#pragma GCC diagnostic pop
            }
            /* upper section */
            if (side->sector->ceiling > opp->sector->ceiling)
            {
                int top = side->sector->ceiling;
                int bot = opp->sector->ceiling;

                std::string texname = tolowercase(side->upper);
                if (texname != "-")
                {
                    auto &tex = g.textures[texname];
                    out.walls.back().uppertex = tex.get();

                    double const tw = tex->width,
                                 th = tex->height;

                    double len =\
                        sqrt(
                            pow(seg.end->x - seg.start->x, 2)
                            + pow(seg.end->y - seg.start->y, 2))
                        / tw;
                    double hgt = abs(top - bot) / th;

                    bool unpegged = ld.flags & UNPEGGEDUPPER;
                    double sx = (seg.offset + side->x) / tw,
                           sy = (side->y / th) + (unpegged? 0 : -hgt);
                    double ex = sx + len,
                           ey = sy + hgt;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
                    out.walls.back().uppermesh.reset(new Mesh{
                        {
                            {-seg.start->x,bot,seg.start->y, sx,ey},
                            {-seg.end->x  ,bot,seg.end->y  , ex,ey},
                            {-seg.end->x  ,top,seg.end->y  , ex,sy},
                            {-seg.start->x,top,seg.start->y, sx,sy},
                        },
                        {0,1,2, 2,3,0}});
                }
#pragma GCC diagnostic pop
            }
        }
    }

    /* create the automap lines */
    for (auto &ld : lvl.linedefs)
    {
        out.automap.emplace_back(
            new Mesh{
                {(GLfloat)-ld.start->x,(GLfloat)ld.start->y,0, 0,0},
                {(GLfloat)-ld.end->x  ,(GLfloat)ld.end->y  ,0, 0,0}});
    }

    return out;
}

bool _check_node_side(int16_t x, int16_t y, Node const &n)
{
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
    return right;
}

uint16_t _get_ssector_interior(
    int16_t x,
    int16_t y,
    Level const &lvl,
    uint16_t node)
{
    auto &n = lvl.nodes[node];

    bool right = _check_node_side(x, y, n);
    uint16_t number = right? n.right : n.left;

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
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g.palette_texture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g.colormap_texture);
    glActiveTexture(GL_TEXTURE1);

    g.billboard_shader->use();
    g.billboard_shader->set("camera", g.cam.matrix());
    g.billboard_shader->set("projection", g.projection);
    g.billboard_shader->set("palettes", 0);
    g.billboard_shader->set("palette", g.palette_number);
    g.billboard_shader->set("colormap", 2);
    g.billboard_shader->set("tex", 1);

    thingquad->bind();

    for (auto &t : lvl.things)
    {
        if (!t.sprites.empty())
        {
            g.billboard_shader->set("colormap_idx",
                (255 - t.sector->lightlevel) / 8);

            std::string sprname = "";

            if (t.angled)
            {
                char frame = 'A' + t.frame_idx;
                char angle = '1';

                double a =\
                    glm::degrees(
                        atan2(
                            t.pos.z - g.cam.pos.z,
                            t.pos.x - g.cam.pos.x));
                if (a < 0)
                {
                    a = 360.0 + a;
                }
                int ang = fmod(a + t.angle + 22.5, 360.0) / 45;
                angle = '1' + (char)ang;

                sprname = std::string{frame} + std::string{angle};
            }
            else
            {
                char frame = 'A' + t.frame_idx;
                sprname = std::string{frame} + "0";
            }

            auto &spr = t.sprites.at(sprname);
            auto scale =\
                glm::scale(
                    glm::mat4{1},
                    glm::vec3{spr.tex->width, spr.tex->height, 1});

            g.billboard_shader->set("position",
                glm::translate(
                    glm::mat4{1},
                    glm::vec3{
                        t.pos.x - (spr.tex->width - (spr.offset.x*2)),
                        t.pos.y - (spr.tex->height - spr.offset.y),
                        t.pos.z}));
            g.billboard_shader->set("scale", scale);
            g.billboard_shader->set("flipx", spr.flipx);

            spr.tex->bind();
            glDrawElements(
                GL_TRIANGLES,
                thingquad->size(),
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

    /* TODO: the node side check drops the framerate a bit,
     *  it might be faster than always doing the right side first
     *  if I precompute it each frame? */
    //bool right = _check_node_side(g.cam.pos.x, g.cam.pos.z, node);
    bool right = true;
    uint16_t first  = right? node.right : node.left;
    uint16_t second = right? node.left  : node.right;

    if (first & 0x8000)
    {
        render_ssector(first & 0x7FFF, lvl, g);
    }
    else
    {
        render_node(first, lvl, g);
    }
    if (second & 0x8000)
    {
        render_ssector(second & 0x7FFF, lvl, g);
    }
    else
    {
        render_node(second, lvl, g);
    }
}

void render_ssector(
    uint16_t index,
    RenderLevel const &lvl,
    RenderGlobals const &g)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g.palette_texture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g.colormap_texture);
    glActiveTexture(GL_TEXTURE1);

    auto &ssector = lvl.raw->ssectors[index];
    auto &seg = lvl.raw->segs[ssector.start];
    auto &ld = lvl.raw->linedefs[seg.linedef];
    auto &side = seg.direction? ld.left : ld.right;

    g.program->use();
    g.program->set("camera", g.cam.matrix());
    g.program->set("projection", g.projection);
    g.program->set("palettes", 0);
    g.program->set("palette", g.palette_number);
    g.program->set("colormap", 2);
    g.program->set("colormap_idx",
        (255 - side->sector->lightlevel) / 8);
    g.program->set("tex", 1);

    for (size_t i = 0; i < ssector.count; ++i)
    {
        auto &wall = lvl.walls[ssector.start + i];

        if (wall.uppermesh != nullptr)
        {
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
    glBindTexture(GL_TEXTURE_2D, g.palette_texture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g.colormap_texture);
    glActiveTexture(GL_TEXTURE1);

    guiprog.use();
    guiprog.set("palettes", 0);
    guiprog.set("palette_idx", 0);
    guiprog.set("colormap", 2);
    guiprog.set("colormap_idx", 0);
    guiprog.set("tex", 1);
    guiquad.bind();

    double const aspect_h = 240.0;
    double const aspect_w = (g.width / (double)g.height) * aspect_h;

    /* weapon sprite */
    /* TODO: animations */
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
    glBindTexture(GL_TEXTURE_2D, g.palette_texture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g.colormap_texture);
    glActiveTexture(GL_TEXTURE1);

    guiprog.use();
    guiprog.set("palettes", 0);
    guiprog.set("palette_idx", 0);
    guiprog.set("colormap", 2);
    guiprog.set("colormap_idx", 0);
    guiprog.set("tex", 1);
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

void render_automap(RenderLevel const &lvl, RenderGlobals const &g)
{
    g.automap_program->use();
    g.automap_program->set("transform",
        glm::translate(
            glm::rotate(
                glm::scale(
                    glm::mat4{1},
                    glm::vec3{-1.0 / g.width, 1.0 / g.height, 1}),
                glm::radians(g.cam.angle.x),
                glm::vec3{0, 0, 1}),
            glm::vec3{-g.cam.pos.x, -g.cam.pos.z, 0}));

    for (size_t i = 0; i < lvl.automap.size(); ++i)
    {
        auto &mesh = lvl.automap[i];
        auto &ld = lvl.raw->linedefs[i];

        glm::vec4 color{0.0, 1.0, 0.0, 1.0};
        if (ld.flags & TWOSIDED)
        {
            if (!(ld.flags & SECRET))
            {
                color.x = 0;
                color.y = 0.5;
                color.z = 0;
            }
            else
            {
                color.x = 1.0;
                color.y = 1.0;
                color.z = 0;
            }
        }
        if (ld.flags & UNMAPPED)
        {
                color.w = 0.0;
        }
        if (ld.flags & PREMAPPED)
        {
                color.x = 0.0;
                color.y = 1.0;
                color.z = 1.0;
        }
        g.automap_program->set("color", color);

        mesh->bind();
        glDrawElements(
            GL_LINES,
            mesh->size(),
            GL_UNSIGNED_INT,
            0);
    }

    g.automap_program->set("transform", glm::mat4{1});
    g.automap_program->set("color", glm::vec4{1.0, 0.0, 0.0, 1.0});
    automap_cursor->bind();
    glDrawElements(
        GL_LINES,
        automap_cursor->size(),
        GL_UNSIGNED_INT,
        0);
}

