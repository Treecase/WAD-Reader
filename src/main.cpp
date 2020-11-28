/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#include "camera.hpp"
#include "mesh.hpp"
#include "program.hpp"
#include "readwad.hpp"
#include "texture.hpp"
#include "things.hpp"
#include "renderlevel.hpp"

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



static std::array<uint32_t, 128> CODE_PAGE_437
{
    /* 8 */
    0x00C7,
    0x00FC,
    0x00E9,
    0x00E2,
    0x00E4,
    0x00E0,
    0x00E5,
    0x00E7,
    0x00EA,
    0x00EB,
    0x00E8,
    0x00EF,
    0x00EE,
    0x00EC,
    0x00C4,
    0x00C5,
    /* 9 */
    0x00C9,
    0x00E6,
    0x00c6,
    0x00f4,
    0x00f6,
    0x00f2,
    0x00fb,
    0x00f9,
    0x00ff,
    0x00d6,
    0x00dc,
    0x00a2,
    0x00a3,
    0x00a5,
    0x20a7,
    0x0192,
    /* A */
    0x00e1,
    0x00ed,
    0x00f3,
    0x00fa,
    0x00f1,
    0x00d1,
    0x00aa,
    0x00ba,
    0x00bf,
    0x2310,
    0x00ac,
    0x00bd,
    0x00bc,
    0x00a1,
    0x00ab,
    0x00bb,
    /* B */
    0x2591,
    0x2592,
    0x2593,
    0x2502,
    0x2524,
    0x2561,
    0x2562,
    0x2556,
    0x2555,
    0x2563,
    0x2551,
    0x2557,
    0x255d,
    0x255c,
    0x255b,
    0x2510,
    /* C */
    0x2514,
    0x2534,
    0x252c,
    0x251c,
    0x2500,
    0x253c,
    0x255e,
    0x255f,
    0x255a,
    0x2554,
    0x2569,
    0x2566,
    0x2560,
    0x2550,
    0x256c,
    0x2567,
    /* D */
    0x2568,
    0x2564,
    0x2565,
    0x2559,
    0x2558,
    0x2552,
    0x2553,
    0x256b,
    0x256a,
    0x2518,
    0x250c,
    0x2588,
    0x2584,
    0x258c,
    0x2590,
    0x2580,
    /* E */
    0x03b1,
    0x00df,
    0x0393,
    0x03c0,
    0x03a3,
    0x03c3,
    0x00b5,
    0x03c4,
    0x03a6,
    0x0398,
    0x03a9,
    0x03b4,
    0x221e,
    0x03c6,
    0x03b5,
    0x2229,
    /* F */
    0x2261,
    0x00b1,
    0x2265,
    0x2264,
    0x2320,
    0x2321,
    0x00f7,
    0x2248,
    0x00b0,
    0x2219,
    0x00b7,
    0x221a,
    0x207f,
    0x00b2,
    0x25a0,
    0x00a0
};

char *CODEPAGE437(unsigned char ch)
{
    char *out = nullptr;
    if (ch <= 0x7F)
    {
        out = new char[2];
        out[0] = ch & 0x7F;
        out[1] = '\0';
    }
    else
    {
        auto codepoint = CODE_PAGE_437[ch & 0x7F];
        if (codepoint < 0x800)
        {
            out = new char[3];
            out[0] = 0b11000000 | ((codepoint >> 6) & 0x1f);
            out[1] = 0b10000000 | ((codepoint >> 0) & 0x3f);
            out[2] = '\0';
        }
        else if (codepoint < 0x10000)
        {
            out = new char[4];
            out[0] = 0b11100000 | ((codepoint >> 12) & 0x0f);
            out[1] = 0b10000000 | ((codepoint >>  6) & 0x3f);
            out[2] = 0b10000000 | ((codepoint >>  0) & 0x3f);
            out[3] = '\0';
        }
        else
        {
            out = new char[4];
            out[1] = 0b11100000 | ((codepoint >> 16) & 0x0f);
            out[1] = 0b10000000 | ((codepoint >> 12) & 0x3f);
            out[2] = 0b10000000 | ((codepoint >>  6) & 0x3f);
            out[3] = 0b10000000 | ((codepoint >>  0) & 0x3f);
            out[4] = '\0';
        }
    }
    return out;
}



enum Weapon
{
    Fist = 0,
    Chainsaw = 1,
    Pistol = 2,
    Shotgun = 3,
    Chaingun = 4,
    RocketLauncher = 5,
    PlasmaRifle = 6,
    BFG9000 = 7,
    SuperShotgun = 8
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

struct Controller
{
    double left, right;
    double forward, backward;
    double turn;
};

RenderLevel make_renderlevel(
    Level const &lvl,
    RenderGlobals &g,
    uint8_t include,
    uint8_t exclude);

enum class State
{
    TitleScreen,
    InLevel,
    Exit,
};

struct GameState
{
    RenderGlobals &rndr;
    Player &doomguy;
    WAD &wad;

    Controller ctrl;
    bool doom2;
    uint8_t difficulty;

    size_t level_idx;
    Level level;
    std::unique_ptr<RenderLevel> renderlevel;

    State state;
    bool menu_open;
    bool automap_open;

    Uint64 last_update;
    bool update_animation;

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
                    rndr.width / 2,
                    rndr.height / 2);
                break;
            case State::TitleScreen:
                automap_open = false;
                break;
            case State::Exit:
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
            case State::Exit:
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

    void setlevel(size_t idx)
    {
        char episode = '0' + ((idx / 10) % 10);
        char mission = '0' + (idx % 10);
        std::string name{};

        if (doom2)
        {
            name =\
                "MAP"
                + std::string{episode}
                + std::string{mission};
        }
        else
        {
            name =\
                "E"
                + std::string{(char)(episode + 1)}
                + "M"
                + std::string{mission};
        }
        level = readlevel(name, wad);
        renderlevel.reset(
            new RenderLevel(level, rndr, difficulty, MP_ONLY));
        level_idx = idx;

        /* set the camera position to player 1's spawn point */
        for (auto &thing : level.things)
        {
            if (thing.type == 1)
            {
                rndr.cam.pos.x = -thing.x;
                rndr.cam.pos.z = thing.y;
                rndr.cam.angle.x = thing.angle - 90;
                rndr.cam.angle.y = 0;
                break;
            }
        }
    }


    GameState(
            RenderGlobals &g,
            Player &doomguy,
            WAD &wad,
            uint8_t difficulty,
            State initial,
            bool menu_initial)
    :   rndr{g},
        doomguy{doomguy},
        wad{wad},

        ctrl{},
        doom2{false},
        difficulty{difficulty},

        level_idx{0},
        level{},
        renderlevel{nullptr},

        state{initial},
        menu_open{menu_initial},
        automap_open{false},

        last_update{0},
        update_animation{false},

        current_menuscreen{""}
    {
        try
        {
            readlevel("MAP01", wad);
            doom2 = true;
        }
        catch (std::out_of_range &e)
        {
            doom2 = false;
        }
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
uint16_t get_ssector(int16_t x, int16_t y, Level const &lvl);

void handle_event_TitleScreen(GameState &gs, SDL_Event e);
void handle_event_InLevel(GameState &gs, SDL_Event e);

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
    "SHT",  /* super shotgun */
};

static std::unique_ptr<Mesh> automap_cursor{nullptr};
static GLuint automap_cursor_vbo = 0;
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
            printf("patch: %s\n", argv[2 + i]);
            patchWAD(wad, f);
            fclose(f);
        }
    }
    readwad(wad);


    RenderGlobals g{};
    g.width  = 320;
    g.height = 240;


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
    automap_cursor->bind();
    std::vector<glm::vec4> colors{
        6,
        glm::vec4{1.0, 1.0, 1.0, 1.0}};
    glGenBuffers(1, &automap_cursor_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, automap_cursor_vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        6 * sizeof(glm::vec4),
        colors.data(),
        GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        4, GL_FLOAT,
        GL_FALSE,
        sizeof(glm::vec4),
        (void *)0);


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
    DirEntry dir = wad.findlump("COLORMAP");
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


    GameState gs{g, doomguy, wad, SKILL3, State::TitleScreen, false};

    /* load the first level */
    gs.setlevel(1);


    /* FPS timer */
    auto timer1hz = SDL_AddTimer(1000, callback1hz, nullptr);
    /* game update timer */
    auto timer35hz = SDL_AddTimer(1000 / 35, callback35hz, nullptr);

    float const speed = 256;
    float const turnspeed = 256;
    Uint64 const freq = SDL_GetPerformanceFrequency();

    SDL_Event e;
    while (gs.state != State::Exit)
    {
        while (SDL_PollEvent(&e) && gs.state != State::Exit)
        {
            /* events that don't depend on the
             * game state are handled here */
            switch (e.type)
            {
            case SDL_QUIT:
                gs.state = State::Exit;
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

            switch (gs.state)
            {
            case State::TitleScreen:
                handle_event_TitleScreen(gs, e);
                break;
            case State::InLevel:
                handle_event_InLevel(gs, e);
                break;
            case State::Exit:
                break;
            }
        }

        /* update the game */
        Uint64 now = SDL_GetPerformanceCounter();
        if (gs.state == State::InLevel && !gs.menu_open)
        {
            /* TODO: raycast for hitscan and rendering */

            float deltatime =\
                (now - gs.last_update) / (float)freq;

            g.cam.rotate(turnspeed * gs.ctrl.turn * deltatime, 0);

            double dx = gs.ctrl.right - gs.ctrl.left,
                   dz = gs.ctrl.forward - gs.ctrl.backward;
            if (dx != 0 || dz != 0)
            {
                g.cam.move(
                    speed
                    * deltatime
                    * glm::normalize(glm::vec3{dx, 0, dz}));
            }
            int ssector = -1;
            try
            {
                ssector =\
                    get_ssector(-g.cam.pos.x, g.cam.pos.z, gs.level);
            }
            catch (std::runtime_error &e)
            {
            }
            if (ssector != -1)
            {
                auto &seg =\
                    gs.level.segs[gs.level.ssectors[ssector].start];
                auto &ld = gs.level.linedefs[seg.linedef];
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
            case Weapon::SuperShotgun:
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
            if (doomguy.weapon == Weapon::SuperShotgun)
            {
                guidef[32].first = "STYSNUM" + std::string{'3'};
            }
        }
        gs.last_update = now;
        if (   gs.state == State::InLevel
            && !gs.menu_open
            && gs.update_animation)
        {
            gs.update_animation = false;

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
            for (auto &thing : gs.renderlevel->things)
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
          {
            /* draw the sky */
            auto &img = g.textures["sky1"];

            double aspect_w = img->width;
            double const w = img->width / aspect_w;

            glDisable(GL_DEPTH_TEST);
            guiprog.use();
            guiprog.set("palettes", 0);
            guiprog.set("palette_idx", 0);
            guiprog.set("colormap", 2);
            guiprog.set("colormap_idx", 0);
            guiprog.set("tex", 1);
            guiprog.set("position",
                glm::scale(glm::mat4{1},
                glm::vec3{w, 1, 1}));
            guiprog.set("texOffset", glm::vec2{
                (-g.cam.angle.x * (1024.0 / img->width)) / 360.0,
                0});

            img->bind();
            guiquad.bind();
            glDrawElements(
                GL_TRIANGLES,
                guiquad.size(),
                GL_UNSIGNED_INT,
                0);
            guiprog.set("texOffset", glm::vec2{0, 0});

            /* draw the first person view */
            glEnable(GL_DEPTH_TEST);
            render_level(*gs.renderlevel, g);
            /* draw the automap */
            glDisable(GL_DEPTH_TEST);
            if (gs.automap_open)
            {
                render_automap(*gs.renderlevel, g);
            }
            /* draw the HUD */
            glDisable(GL_DEPTH_TEST);
            render_hud(doomguy, wad, guiquad, guiprog, g);
          } break;

        case State::TitleScreen:
          {
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
          } break;

        case State::Exit:
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

    char vga_to_vt102[16] =
    {
        '0',
        '4',
        '2',
        '6',
        '1',
        '5',
        '3',
        '7',
        '0',
        '4',
        '2',
        '6',
        '1',
        '5',
        '3',
        '7',
    };

    /* TODO: switch from using raw ANSI escapes
     * to something more cross-platform */
    DirEntry exittext = wad.findlump("ENDOOM");
    auto textdata = exittext.data.get();
    for (size_t y = 0; y < 25; ++y)
    {
        for (size_t x = 0; x < 80; ++x)
        {
            unsigned char vga = textdata[(y * 160) + (x * 2) + 1];
            unsigned char ch = textdata[(y * 160) + (x * 2)];

            unsigned char fore = vga & 0x0f;
            unsigned char back = (vga >> 4) & 0x07;
            unsigned char blink = (vga >> 7) & 0x01;

            char *utf8 = CODEPAGE437(ch);
            printf("\x1b[3%c;4%c;%s;%sm%s",
                vga_to_vt102[fore],
                vga_to_vt102[back],
                blink? "5" : "25",
                fore > 7? "1" : "22",
                utf8);
            delete[] utf8;
        }
        putchar('\n');
    }
    printf("\x1b[0m");

    /* cleanup */
    SDL_RemoveTimer(timer1hz);
    SDL_RemoveTimer(timer35hz);

    glDeleteBuffers(1, &automap_cursor_vbo);

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

/* true iff (x,y) is on the right side of n's partition line */
bool _check_node_side(int16_t x, int16_t y, Node const &n)
{
    /* check the left/right bounding boxes
     * to see if we can decide quickly */
    bool right =\
        (  n.right_lower_x <= x && x <= n.right_upper_x
        && n.right_lower_y <= y && y <= n.right_upper_y);
    bool left =\
        (  n.left_lower_x <= x && x <= n.left_upper_x
        && n.left_lower_y <= y && y <= n.left_upper_y);

    /* if the point is inside both boxes, or neither,
     * do the expensive calculation */
    if ((left && right) || (!left && !right))
    {
        double part_angle = atan2((double)n.dy, (double)n.dx);
        double xy_angle = atan2(
            (double)y - (double)n.y,
            (double)x - (double)n.x);

        if (copysign(1, part_angle) != copysign(1, xy_angle))
        {
            part_angle = atan2(-(double)n.dy, -(double)n.dx);
            right = (xy_angle > part_angle);
        }
        else
        {
            right = (xy_angle < part_angle);
        }
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



void handle_event_TitleScreen(GameState &gs, SDL_Event e)
{
    if (gs.menu_open)
    {
        switch (e.type)
        {
        case SDL_MOUSEBUTTONDOWN:
            gs.transition(State::InLevel, false);
            break;
        case SDL_KEYUP:
            switch (e.key.keysym.sym)
            {
            case SDLK_RETURN:
                gs.transition(State::InLevel, false);
                break;
            }
            break;
        }
    }
    else
    {
        switch (e.type)
        {
        case SDL_MOUSEBUTTONDOWN:
            gs.transition(true);
            break;
        }
    }
}

void handle_event_InLevel(GameState &gs, SDL_Event e)
{
    if (gs.menu_open)
    {
        switch (e.type)
        {
        case SDL_MOUSEBUTTONDOWN:
            gs.transition(State::Exit);
            break;
        case SDL_KEYUP:
            switch (e.key.keysym.sym)
            {
            case SDLK_RETURN:
                gs.transition(State::Exit);
                break;
            }
            break;
        }
    }
    else
    {
        switch (e.type)
        {
        case SDL_USEREVENT:
            switch (e.user.code)
            {
            case 0:
                gs.update_animation = true;
                break;
            }
            break;

        case SDL_MOUSEMOTION:
            gs.rndr.cam.rotate(
                -e.motion.xrel / 10.0,
                -e.motion.yrel / 10.0);
            break;

        case SDL_MOUSEBUTTONDOWN:
            switch (gs.doomguy.weapon)
            {
            case Weapon::Fist:
            case Weapon::Chainsaw:
                break;
            case Weapon::Pistol:
            case Weapon::Chaingun:
                if (gs.doomguy.bullets > 0)
                {
                    gs.doomguy.bullets -= 1;
                }
                break;
            case Weapon::SuperShotgun:
            case Weapon::Shotgun:
                if (gs.doomguy.shells > 0)
                {
                    gs.doomguy.shells -= 1;
                }
                break;
            case Weapon::RocketLauncher:
                if (gs.doomguy.rockets > 0)
                {
                    gs.doomguy.rockets -= 1;
                }
                break;
            case Weapon::PlasmaRifle:
            case Weapon::BFG9000:
                if (gs.doomguy.cells > 0)
                {
                    gs.doomguy.cells -= 1;
                }
                break;
            }
            break;

        case SDL_MOUSEWHEEL:
            if (gs.doom2)
            {
                if (gs.doomguy.weapon == Weapon::SuperShotgun)
                {
                    if (e.wheel.y < 0)
                    {
                        gs.doomguy.weapon = Weapon::Chaingun;
                    }
                    else if (e.wheel.y > 0)
                    {
                        gs.doomguy.weapon = Weapon::Shotgun;
                    }
                    break;
                }
                else
                {
                    if (   e.wheel.y < 0
                        && gs.doomguy.weapon == Weapon::Shotgun)
                    {
                        gs.doomguy.weapon = Weapon::SuperShotgun;
                        break;
                    }
                    else if (e.wheel.y > 0
                        && gs.doomguy.weapon == Weapon::Chaingun)
                    {
                        gs.doomguy.weapon = Weapon::SuperShotgun;
                        break;
                    }
                }
            }
            gs.doomguy.weapon -= e.wheel.y;
            gs.doomguy.weapon %= 8;
            break;

        case SDL_KEYDOWN:
            switch (e.key.keysym.sym)
            {
            case SDLK_LEFT:
                gs.ctrl.turn = 1;
                break;
            case SDLK_RIGHT:
                gs.ctrl.turn = -1;
                break;
            case SDLK_d:
                gs.ctrl.right = 1.0;
                break;
            case SDLK_a:
                gs.ctrl.left = 1.0;
                break;
            case SDLK_w:
                gs.ctrl.forward = 1.0;
                break;
            case SDLK_s:
                gs.ctrl.backward = 1.0;
                break;
            case SDLK_1:
                if (gs.doomguy.weapon == Weapon::Chainsaw)
                {
                    gs.doomguy.weapon = Weapon::Fist;
                }
                else
                {
                    gs.doomguy.weapon = Weapon::Chainsaw;
                }
                break;
            case SDLK_2:
                gs.doomguy.weapon = Weapon::Pistol;
                break;
            case SDLK_3:
                if (gs.doom2)
                {
                    if (gs.doomguy.weapon == Weapon::SuperShotgun)
                    {
                        gs.doomguy.weapon = Weapon::Shotgun;
                    }
                    else
                    {
                        gs.doomguy.weapon = Weapon::SuperShotgun;
                    }
                }
                else
                {
                    gs.doomguy.weapon = Weapon::Shotgun;
                }
                break;
            case SDLK_4:
                gs.doomguy.weapon = Weapon::Chaingun;
                break;
            case SDLK_5:
                gs.doomguy.weapon = Weapon::RocketLauncher;
                break;
            case SDLK_6:
                gs.doomguy.weapon = Weapon::PlasmaRifle;
                break;
            case SDLK_7:
                gs.doomguy.weapon = Weapon::BFG9000;
                break;
            case SDLK_TAB:
                gs.automap_open = !gs.automap_open;
                break;
            }
            break;

        case SDL_KEYUP:
            switch (e.key.keysym.sym)
            {
            case SDLK_LEFT:
                gs.ctrl.turn = 0;
                break;
            case SDLK_RIGHT:
                gs.ctrl.turn = 0;
                break;
            case SDLK_d:
                gs.ctrl.right = 0;
                break;
            case SDLK_a:
                gs.ctrl.left = 0;
                break;
            case SDLK_w:
                gs.ctrl.forward = 0;
                break;
            case SDLK_s:
                gs.ctrl.backward = 0;
                break;

            case SDLK_SPACE:
                gs.setlevel(
                    gs.level_idx
                    + (gs.level_idx % 10 == 9)
                    + 1);
                break;
            }
            break;
        }
    }
}



void render_level(RenderLevel const &lvl, RenderGlobals const &g)
{
    /* draw the floors and ceilings */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g.palette_texture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g.colormap_texture);
    glActiveTexture(GL_TEXTURE1);

    g.program->use();
    g.program->set("camera", g.cam.matrix());
    g.program->set("projection", g.projection);
    g.program->set("palettes", 0);
    g.program->set("palette", g.palette_number);
    g.program->set("colormap", 2);
    g.program->set("tex", 1);

    for (auto &floor : lvl.floors)
    {
        g.program->set("colormap_idx",
            (255 - floor.lightlevel) / 8);
        if (floor.mesh != nullptr)
        {
            floor.tex->bind();
            floor.mesh->bind();
            glDrawElements(
                GL_TRIANGLES,
                floor.mesh->size(),
                GL_UNSIGNED_INT,
                0);
        }
    }
    for (auto &ceiling : lvl.ceilings)
    {
        g.program->set("colormap_idx",
            (255 - ceiling.lightlevel) / 8);
        if (ceiling.mesh != nullptr)
        {
            ceiling.tex->bind();
            ceiling.mesh->bind();
            glDrawElements(
                GL_TRIANGLES,
                ceiling.mesh->size(),
                GL_UNSIGNED_INT,
                0);
        }
    }


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

    bool right = !_check_node_side(-g.cam.pos.x, g.cam.pos.z, node);
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
    std::string sprname = hands[doomguy.weapon] + "GA0";
    if (doomguy.weapon == Weapon::SuperShotgun)
    {
        sprname = hands[doomguy.weapon] + "2A0";
    }
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

    lvl.automap->bind();
    glDrawElements(
        GL_LINES,
        lvl.automap->size(),
        GL_UNSIGNED_INT,
        0);

    /* draw the automap cursor */
    g.automap_program->set("transform", glm::mat4{1});
    g.automap_program->set("color", glm::vec4{1.0, 0.0, 0.0, 1.0});
    automap_cursor->bind();
    glDrawElements(
        GL_LINES,
        automap_cursor->size(),
        GL_UNSIGNED_INT,
        0);
}

