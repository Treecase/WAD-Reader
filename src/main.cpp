/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#include "readwad.hpp"

#include <SDL2/SDL.h>

#include <cstdlib>
#include <cerrno>
#include <cstring>



void draw_level(Level const &lvl);
void draw_node(
    size_t index,
    Level const &lvl,
    SDL_Renderer *ren);
void draw_ssector(
    size_t index,
    Level const &lvl,
    SDL_Renderer *ren);

Uint32 callback60hz(Uint32, void *);



static int camx = 0,
           camy = -3000;
static double zoom = 1;

static int width  = 320,
           height = 240;

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
    /* set the camera location to the player start */
    for (auto &thing : lvl.things)
    {
        if (thing.type == 1)
        {
            camx = -thing.x;
            camy = thing.y;
        }
    }


    /* setup SDL stuff */
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    SDL_Window *win = SDL_CreateWindow(
        __func__,
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width, height,
        SDL_WINDOW_RESIZABLE);
    SDL_Renderer *ren = SDL_CreateRenderer(
        win,
        -1,
        SDL_RENDERER_ACCELERATED);

    auto timer60hz = SDL_AddTimer(1000 / 60, callback60hz, nullptr);


    bool running = true,
         doframe = false,
         redraw = false;
    SDL_Event e;
    while (SDL_WaitEvent(&e) && running)
    {
        switch (e.type)
        {
        case SDL_QUIT:
            running = false;
            break;

        case SDL_USEREVENT:
            doframe = true;
            break;

        case SDL_MOUSEMOTION:
            if (e.motion.state & SDL_BUTTON_LMASK)
            {
                camx += e.motion.xrel * zoom;
                camy += e.motion.yrel * zoom;
                redraw = true;
            }
            break;

        case SDL_MOUSEWHEEL:
            if (e.wheel.y > 0)
            {
                if (zoom >= 2)
                {
                    zoom /= 2;
                    redraw = true;
                }
            }
            else if (e.wheel.y < 0)
            {
                zoom *= 2;
                redraw = true;
            }
            break;

        case SDL_WINDOWEVENT:
            switch (e.window.event)
            {
            case SDL_WINDOWEVENT_EXPOSED:
                doframe = true;
                redraw = true;
                break;
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                width  = e.window.data1;
                height = e.window.data2;
                doframe = true;
                redraw = true;
                break;
            }
            break;
        }

        if (redraw && doframe)
        {
            doframe = false;
            redraw = false;

            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);

            draw_node(lvl.nodes.size() - 1, lvl, ren);

            SDL_SetRenderDrawColor(ren, 255, 0, 0, 255);
            SDL_Rect r;
            r.x = (width  / 2) - 1;
            r.y = (height / 2) - 1;
            r.w = 2;
            r.h = 2;
            SDL_RenderFillRect(ren, &r);

            SDL_RenderPresent(ren);
        }
    }

    SDL_RemoveTimer(timer60hz);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
}

void draw_node(
    size_t index,
    Level const &lvl,
    SDL_Renderer *ren)
{
    Node const &node = lvl.nodes[index];
    if (node.right & 0x8000)
    {
        draw_ssector(
            node.right & 0x7FFF,
            lvl,
            ren);
    }
    else
    {
        draw_node(node.right, lvl, ren);
    }
    if (node.left & 0x8000)
    {
        draw_ssector(
            node.left & 0x7FFF,
            lvl,
            ren);
    }
    else
    {
        draw_node(node.left, lvl, ren);
    }
}

void draw_ssector(
    size_t index,
    Level const &lvl,
    SDL_Renderer *ren)
{
    SSector const &ssector = lvl.ssectors[index];

    SDL_SetRenderDrawColor(ren,
        ssector_colors[index][0],
        ssector_colors[index][1],
        ssector_colors[index][2],
        255);

    for (size_t i = 0; i < ssector.count; ++i)
    {
        auto &seg = lvl.segs[ssector.start + i];

        int sx = (( seg.start->x + camx) / zoom) + (width / 2),
            sy = ((-seg.start->y + camy) / zoom) + (height / 2);
        int ex = (( seg.end->x   + camx) / zoom) + (width / 2),
            ey = ((-seg.end->y   + camy) / zoom) + (height / 2);

        SDL_RenderDrawLine(ren, sx, sy, ex, ey);
    }
}

Uint32 callback60hz(Uint32 interval, void *param)
{
    SDL_Event e;
    e.type = SDL_USEREVENT;
    SDL_PushEvent(&e);
    return interval;
}

