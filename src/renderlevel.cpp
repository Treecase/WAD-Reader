/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#include "renderlevel.hpp"

#include "things.hpp"
#include "wad.hpp"

#include <cmath>

#include <algorithm>
#include <deque>
#include <unordered_set>
#include <set>



std::string tolowercase(std::string const &str);
uint16_t get_ssector(int16_t x, int16_t y, Level const &lvl);

static bool intersect_line_point(
    glm::vec2 const &point,
    glm::vec2 const &start,
    glm::vec2 const &end);
static std::pair<double, double> intersect_line_line(
    glm::vec2 const &start1,
    glm::vec2 const &end1,
    glm::vec2 const &start2,
    glm::vec2 const &end2);
static bool point_in_triangle(
    glm::vec2 const &p0,
    glm::vec2 const &p1,
    glm::vec2 const &p2,
    glm::vec2 const &point);
static bool polygon_in_polygon(
    std::vector<Vertex> const &p1,
    std::vector<Vertex> const &p2);
static bool point_in_polygon(
    Vertex const &point,
    std::vector<Vertex> const &polygon);

static bool line_in_polygon(
    glm::vec2 const &line1,
    glm::vec2 const &line2,
    std::vector<Vertex> const &polygon);

static bool is_counterclockwise(std::vector<Vertex> const &vertices);
static double interior_angle(
    glm::vec2 const &A,
    glm::vec2 const &B,
    glm::vec2 const &C);



RenderLevel::RenderLevel(
    Level const &lvl,
    RenderGlobals &g,
    uint8_t include,
    uint8_t exclude)
{
    raw = &lvl;

    /* /+========================================================+\ */
    /* ||                         THINGS                         || */
    /* \+========================================================+/ */
    /* make RenderThings from things */
    for (auto &thing : lvl.things)
    {
        if ((thing.options & include) && !(thing.options & exclude))
        {
            things.push_back(RenderThing{});
            RenderThing &rt = things.back();
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
            rt.pos = glm::vec3{-thing.x, rt.sector->floor+5, thing.y};
        }
    }

    /* /+========================================================+\ */
    /* ||                         WALLS                          || */
    /* \+========================================================+/ */
    /* create Walls from the segs */
    /* TODO: animated walls */
    for (auto &seg : lvl.segs)
    {
        auto &ld = lvl.linedefs[seg.linedef];
        auto &side = seg.direction? ld.left : ld.right;
        auto &opp  = seg.direction? ld.right : ld.left;

        walls.emplace_back(nullptr, nullptr);

        /* middle */
        if (side->middle != "-")
        {
            bool twosided = ld.flags & TWOSIDED;
            bool unpegged = ld.flags & UNPEGGEDLOWER;

            int top = side->sector->ceiling;
            int bot = side->sector->floor;

            if (twosided)
            {
                if (side->sector->floor < opp->sector->floor)
                {
                    bot = opp->sector->floor;
                }
                if (side->sector->ceiling > opp->sector->ceiling)
                {
                    top = opp->sector->ceiling;
                }
            }

            std::string texname = tolowercase(side->middle);
            auto &tex = g.textures[texname];
            walls.back().middletex = tex.get();

            double const tw = tex->width,
                         th = tex->height;

            double len =\
                sqrt(
                    pow(seg.end->x - seg.start->x, 2)
                    + pow(seg.end->y - seg.start->y, 2))
                / tw;
            double hgt = abs(top - bot) / th;

            double sx = (seg.offset + side->x) / tw,
                   sy = (side->y / th) + (unpegged? -hgt : 0);
            double ex = sx + len,
                   ey = sy + hgt;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
            walls.back().middlemesh.reset(new Mesh{
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
            if (   side->sector->floor < opp->sector->floor
                && !(side->sector->floor_flat == "F_SKY1"
                    && opp->sector->floor_flat == "F_SKY1"))
            {
                int top = opp->sector->floor;
                int bot = side->sector->floor;

                std::string texname = tolowercase(side->lower);
                if (texname != "-")
                {
                    auto &tex = g.textures[texname];
                    walls.back().lowertex = tex.get();

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
                    walls.back().lowermesh.reset(new Mesh{
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
            if (side->sector->ceiling > opp->sector->ceiling
                && !(side->sector->ceiling_flat == "F_SKY1"
                    && opp->sector->ceiling_flat == "F_SKY1"))
            {
                int top = side->sector->ceiling;
                int bot = opp->sector->ceiling;

                std::string texname = tolowercase(side->upper);
                if (texname != "-")
                {
                    auto &tex = g.textures[texname];
                    walls.back().uppertex = tex.get();

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
                    walls.back().uppermesh.reset(new Mesh{
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

    /* /+========================================================+\ */
    /* ||                         FLATS                          || */
    /* \+========================================================+/ */
//    for (auto &sector : lvl.sectors)
    /* FIXME: TEMP */
    for (
        size_t sector_idx = 0;
        sector_idx < lvl.sectors.size();
        ++sector_idx)
    {
        auto &sector = lvl.sectors[sector_idx];

        std::vector<std::pair<Vertex, Vertex>> lines{};

        /* get all the linedefs associated with the sector */
        for (auto &linedef : lvl.linedefs)
        {
            if (linedef.right->sector == &sector)
            {
                lines.push_back({*linedef.start, *linedef.end});
            }
            else if (
                (linedef.flags & TWOSIDED)
                && linedef.left->sector == &sector)
            {
                lines.push_back({*linedef.end, *linedef.start});
            }
        }

        /* create lists of connected vertices in the sector */
        /* TODO: there can be shared vertices? */
        std::vector<std::vector<Vertex>> loops{};
        while (!lines.empty())
        {
            std::vector<Vertex> loop{};

            loop.push_back(lines.back().first);
            loop.push_back(lines.back().second);
            lines.pop_back();

            for (bool changed = true; changed;)
            {
                changed = false;
                for (auto it = lines.begin(); it != lines.end();)
                {
                    auto &last = loop.back();
                    if (   last.x == it->first.x
                        && last.y == it->first.y)
                    {
                        changed = true;
                        loop.push_back(it->second);
                        it = lines.erase(it);
                    }
                    else
                    {
                        it++;
                    }
                }
            }
            loops.push_back(loop);
        }

        /* make sure all the loops are clockwise */
        for (auto &loop : loops)
        {
            if (   loop.front().x == loop.back().x
                && loop.front().y == loop.back().y)
            {
                loop.pop_back();
            }
            if (is_counterclockwise(loop))
            {
                std::reverse(std::begin(loop), std::end(loop));
            }
        }


        /* FIXME: TEMP */
        printf("LOOPS\n");
        for (auto &loop : loops)
        {
            puts("LOOP");
            for (auto &point : loop)
            {
                printf("%d,%d\n", point.x, point.y);
            }
        }


        /* +------------------------------------------------------+ */
        /* |                 Triangulate the Sector               | */
        /* +------------------------------------------------------+ */
        /* generate a tree containing all the nested loops */
        struct TreeNode
        {
            std::unordered_set<size_t> parents;
            std::unordered_set<size_t> children;
        };
        std::vector<struct TreeNode> tree{loops.size()};

        for (size_t i = 0; i < loops.size(); ++i)
        {
            for (size_t j = 0; j < loops.size(); ++j)
            {
                if (   i != j
                    && polygon_in_polygon(loops[i], loops[j]))
                {
                    /* j contains i */
                    tree[i].parents.emplace(j);
                    tree[j].children.emplace(i);
                }
            }
        }

        /* modify the tree so each node has only 1 parent
         * (or 0 parents for the root) */
        for (size_t i = 0; i < tree.size(); ++i)
        {
            for (auto &parent : tree[i].parents)
            {
                for (auto &grandparent : tree[parent].parents)
                {
                    if (tree[i].parents.count(grandparent))
                    {
                        tree[i].parents.erase(grandparent);
                        tree[grandparent].children.erase(i);
                    }
                }
            }
        }

        /* get the parentless nodes (aka roots) */
        std::unordered_set<size_t> tree_roots{};
        for (size_t i = 0; i < tree.size(); ++i)
        {
            if (tree[i].parents.empty())
            {
                tree_roots.emplace(i);
            }
        }

        /* +------------------------------------------------------+ */
        /* |                  Simplify the loops                  | */
        /* +------------------------------------------------------+ */
        printf("#simplify no. %lu\n", sector_idx);
        std::vector<Vertex> simplified{};
        for (auto &root : tree_roots)
        {
            printf("#root: %lu\n", root);
            std::vector<Vertex> outer{};
            std::vector<std::vector<Vertex>> insides{};
            for (auto &vertex : loops[root])
            {
                outer.push_back(vertex);
            }
            for (auto &child : tree[root].children)
            {
                insides.push_back(loops[child]);
            }

            while (!insides.empty())
            {
                /* select the inner polygon with the highest x value to
                 * merge with the outer polygon */
                std::pair<int16_t, size_t> global_max_x = {
                    insides[0][0].x, 0};
                for (size_t i = 1; i < insides[0].size(); ++i)
                {
                    if (insides[0][i].x > global_max_x.first)
                    {
                        global_max_x = {insides[0][i].x, 0};
                    }
                }

                for (size_t i = 1; i < insides.size(); ++i)
                {
                    for (auto &vertex : insides[i])
                    {
                        auto max_x = vertex.x;
                        if (max_x > global_max_x.first)
                        {
                            global_max_x = {max_x, i};
                        }
                    }
                }

                auto &inner = insides[global_max_x.second];


                /* create a set containing all the reflex vertices in the
                 * outer polygon */
                std::unordered_set<size_t> reflex_vertices{};
                for (size_t i = 0; i < outer.size(); ++i)
                {
                    size_t i0 = (i == 0? outer.size() - 1 : i - 1),
                           i1 = i,
                           i2 = (i + 1) % outer.size();
                    auto theta = interior_angle(
                        glm::vec2{outer[i0].x, outer[i0].y},
                        glm::vec2{outer[i1].x, outer[i1].y},
                        glm::vec2{outer[i2].x, outer[i2].y});

                    if (theta >= M_PI)
                    {
                        reflex_vertices.emplace(i1);
                    }
                }


                /* find the vertex with maximum x in the inner polygon */
                glm::vec2 M{inner[0].x, inner[0].y};
                for (size_t i = 1; i < inner.size(); ++i)
                {
                    if (inner[i].x > M.x)
                    {
                        M = glm::vec2{inner[i].x, inner[i].y};
                    }
                }


                /* Cast the ray M+t[1, 0] into every line segment of
                 * outer. For each intersected line, choose the endpoint
                 * with the maximum x coordinate. We want to get the
                 * point with the highest x coord out of these
                 * endpoints. */
                std::pair<double, size_t> closest{INFINITY, -1};
                for (size_t i = 0; i < outer.size(); ++i)
                {
                    size_t im1 = (i == 0? outer.size() - 1 : i - 1);
                    glm::vec2 p0{outer[im1].x, outer[im1].y};
                    glm::vec2 p1{outer[i].x, outer[i].y};

                    auto tu = intersect_line_line(
                        M, M + glm::vec2{1, 0},
                        p0, p1);

                    auto t = tu.first;
                    auto u = tu.second;
                    if (!std::isnan(t)
                        && t >= 0
                        && (0 <= abs(u) && abs(u) <= 1))
                    {
                        size_t endpoint = (p1.x > p0.x? i : im1);
                        if (t < closest.first)
                        {
                            closest = {t, endpoint};
                        }
                    }
                }

                /* FIXME: TEMP
                 *  skip sectors where we couldn't find a P */
                if (closest.second == (size_t)-1)
                {
                    puts("#skip this loop, P is null");
                    insides.erase(insides.begin() + global_max_x.second);
                    continue;
                }
                auto t = closest.first;
                auto &P = outer.at(closest.second);
                auto I = M + glm::vec2{t, 0};


                Vertex *mutually_visible = nullptr;

                /* If I is a point on the outer polygon, then P is
                 * mutually visible */
                if (I.x == P.x && I.y == P.y)
                {
                    mutually_visible = &P;
                }
                else
                {
                    /* Search the reflex vertices of the outer polygon
                     * (excluding P). If they're all outside the triangle
                     * MIP, P is mutually visible */
                    std::unordered_set<size_t> contained{};
                    for (auto &idx : reflex_vertices)
                    {
                        auto &point = outer[idx];
                        if (   (point.x != P.x || point.y != P.y)
                            && point_in_triangle(
                                M, I, glm::vec2{P.x, P.y},
                                glm::vec2{point.x, point.y}))
                        {
                            contained.emplace(idx);
                        }
                    }
                    if (contained.empty())
                    {
                        mutually_visible = &P;
                    }
                    else
                    {
                        /* Search for the vertex inside MIP which has the
                         * smallest angle between it and the ray */
                        double smallest_angle = INFINITY;
                        for (auto &idx : contained)
                        {
                            glm::vec2 v{outer[idx].x, outer[idx].y};
                            auto p = v - M;
                            auto angle = atan2(p.y, p.x);
                            if (angle < 0)
                            {
                                angle = M_PI - angle;
                            }
                            if (angle < smallest_angle)
                            {
                                smallest_angle = angle;
                                mutually_visible = &outer[idx];
                            }
                        }
                    }
                }


                /* make sure the outer and inner polygons have opposite
                 * windings */
                if (is_counterclockwise(outer))
                {
                    std::reverse(std::begin(outer), std::end(outer));
                }
                if (!is_counterclockwise(inner))
                {
                    std::reverse(std::begin(inner), std::end(inner));
                }

                /* rotate the inner polygon so that the mutually visible
                 * vertex is the 1st in the list */
                while (inner[0].x != M.x || inner[0].y != M.y)
                {
                    std::rotate(
                        inner.begin(),
                        inner.begin() + 1,
                        inner.end());
                }


                /* merge the inner and outer polygons */
                std::vector<Vertex> newpoly{};
                for (auto &vertex : outer)
                {
                    newpoly.push_back(vertex);
                    if (vertex.x == mutually_visible->x
                        && vertex.y == mutually_visible->y)
                    {
                        newpoly.insert(
                            newpoly.end(),
                            inner.begin(),
                            inner.end());
                        newpoly.push_back(inner[0]);
                        newpoly.push_back(vertex);
                    }
                }


                /* make sure the resulting polygon is clockwise */
                if (is_counterclockwise(newpoly))
                {
                    std::reverse(std::begin(newpoly), std::end(newpoly));
                }

                outer = newpoly;
                insides.erase(insides.begin() + global_max_x.second);
            }
            for (auto &vertex : outer)
            {
                simplified.push_back(vertex);
            }
        }
        loops = {simplified};
        /* +------------------------------------------------------+ */
        /* |                END Simplify the loops                | */
        /* +------------------------------------------------------+ */

        /* triangulate via ear clipping */
        std::vector<std::array<glm::vec2, 3>> triangles{};
        auto &vertices = loops[0];

        bool changed = true;
        while (vertices.size() >= 4 && changed)
        {
            changed = false;
#if 0
            for (size_t i1 = 0; i1 < vertices.size(); ++i1)
            {
                size_t i0 = (i1 == 0? vertices.size() - 1 : i1 - 1);
                size_t i2 = (i1 + 1) % vertices.size();

                glm::vec2 p0{vertices[i0].x, vertices[i0].y},
                          p1{vertices[i1].x, vertices[i1].y},
                          p2{vertices[i2].x, vertices[i2].y};

                auto theta = interior_angle(p0, p1, p2);
                bool diagonal = line_in_polygon(p0, p2, vertices);
                if (abs(theta) < M_PI && diagonal)
                {
                    bool contains_vertex = false;
                    for (size_t i = 0; i < vertices.size(); ++i)
                    {
                        if (i != i0 && i != i1 && i != i2)
                        {
                            if (point_in_triangle(
                                    p0, p1, p2,
                                    glm::vec2{
                                        vertices[i].x,
                                        vertices[i].y}))
                            {
                                contains_vertex = true;
                                break;
                            }
                        }
                    }
                    if (!contains_vertex)
                    {
                        triangles.push_back({p0, p1, p2});
                        vertices.erase(vertices.begin() + i1);
                        changed = true;
                        break;
                    }
                }
            }
#else
            /* get all the reflex vertices */
            std::unordered_set<size_t> reflex_vertices{};
            for (size_t i = 0; i < vertices.size(); ++i)
            {
                size_t i0 = (i == 0? vertices.size() - 1 : i - 1),
                       i1 = i,
                       i2 = (i + 1) % vertices.size();

                auto theta = interior_angle(
                    glm::vec2{vertices[i0].x, vertices[i0].y},
                    glm::vec2{vertices[i1].x, vertices[i1].y},
                    glm::vec2{vertices[i2].x, vertices[i2].y});

                if (theta >= M_PI)
                {
                    reflex_vertices.emplace(i);
                }
            }

            for (size_t i1 = 0; i1 < vertices.size(); ++i1)
            {
                size_t i0 = (i1 == 0? vertices.size() - 1 : i1 - 1),
                       i2 = (i1 + 1) % vertices.size();

                glm::vec2 p0{vertices[i0].x, vertices[i0].y},
                          p1{vertices[i1].x, vertices[i1].y},
                          p2{vertices[i2].x, vertices[i2].y};

                /* skip reflex vertices and check that the diagonal
                 * is strictly contained by the polygon */
                if (reflex_vertices.count(i1) == 0
                    && line_in_polygon(p0, p2, vertices))
                {
                    /* skip triangles that contain other vertices */
                    bool inside = false;
                    for (auto &j : reflex_vertices)
                    {
                        if (j != i0 && j != i2)
                        {
                            if(point_in_triangle(
                                    p0, p1, p2,
                                    glm::vec2{
                                        vertices[j].x,
                                        vertices[j].y}))
                            {
                                inside = true;
                                break;
                            }
                        }
                    }

                    /* since the vertex has an acute interior angle
                     * and the associated triangle doesn't contain
                     * other vertices, add the triangle to the
                     * triangles list and remove the vertex from
                     * the loop. */
                    if (!inside)
                    {
                        triangles.push_back({p0, p1, p2});
                        vertices.erase(vertices.begin() + i1);
                        changed = true;
                        break;
                    }
                }
            }
#endif
        }
#if 0
        triangles.push_back(
            {   glm::vec2{vertices[0].x, vertices[0].y},
                glm::vec2{vertices[1].x, vertices[1].y},
                glm::vec2{vertices[2].x, vertices[2].y}});
#else
        if (vertices.size() > 3)
        {
            printf("#%lu failed: %lu verts remain!\n",
                sector_idx,
                vertices.size());
            printf("#%lu failed: %lu/%lu\n",
                sector_idx,
                triangles.size(),
                vertices.size() + triangles.size());
//            exit(1);
            continue;
        }
        triangles.push_back(
            {   glm::vec2{vertices[0].x, vertices[0].y},
                glm::vec2{vertices[1].x, vertices[1].y},
                glm::vec2{vertices[2].x, vertices[2].y}});
#endif


        /* +------------------------------------------------------+ */
        /* |                  Create the Meshes                   | */
        /* +------------------------------------------------------+ */
        std::vector<Mesh::Vertex> fverts{},
                                  cverts{};
        for (auto &tri : triangles)
        {
            auto &p0 = tri[0],
                  p1 = tri[1],
                  p2 = tri[2];

            GLfloat fl = sector.floor,
                    cl = sector.ceiling;

            fverts.push_back({-p0.x,fl,p0.y, p0.x/64.f,p0.y/64.f});
            fverts.push_back({-p1.x,fl,p1.y, p1.x/64.f,p1.y/64.f});
            fverts.push_back({-p2.x,fl,p2.y, p2.x/64.f,p2.y/64.f});

            cverts.push_back({-p0.x,cl,p0.y, p0.x/64.f,p0.y/64.f});
            cverts.push_back({-p1.x,cl,p1.y, p1.x/64.f,p1.y/64.f});
            cverts.push_back({-p2.x,cl,p2.y, p2.x/64.f,p2.y/64.f});
        }

        std::reverse(std::begin(cverts), std::end(cverts));

        auto floortex = g.flats[sector.floor_flat].get();
        auto ceiltex = g.flats[sector.ceiling_flat].get();

        if (sector.floor_flat != "F_SKY1")
        {
            floors.emplace_back(
                floortex,
                new Mesh{fverts},
                sector.lightlevel);
        }
        else
        {
            floors.emplace_back(floortex, nullptr);
        }
        if (sector.ceiling_flat != "F_SKY1")
        {
            ceilings.emplace_back(
                ceiltex,
                new Mesh{cverts},
                sector.lightlevel);
        }
        else
        {
            ceilings.emplace_back(ceiltex, nullptr);
        }
    }


    /* /+========================================================+\ */
    /* ||                        AUTOMAP                         || */
    /* \+========================================================+/ */
    /* create the automap mesh */
    std::vector<Mesh::Vertex> verts{};
    std::vector<glm::vec4> colors{};
    for (auto &ld : lvl.linedefs)
    {
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
        verts.push_back(
            {(GLfloat)-ld.start->x,(GLfloat)ld.start->y,0, 0,0});
        verts.push_back(
            {(GLfloat)-ld.end->x,(GLfloat)ld.end->y,0, 0,0});
        colors.push_back(color);
        colors.push_back(color);
    }
    automap.reset(new Mesh{verts});

    automap->bind();
    glGenBuffers(1, &automap_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, automap_vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        verts.size() * sizeof(glm::vec4),
        colors.data(),
        GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        4, GL_FLOAT,
        GL_FALSE,
        sizeof(glm::vec4),
        (void *)0);
}

RenderLevel::~RenderLevel()
{
    glDeleteBuffers(1, &automap_vbo);
}



/* /+============================================================+\ */
/* ||                            UTIL                            || */
/* \+============================================================+/ */
#define ISCLOSE(a, b)   (abs((a) - (b)) <= 0.0005)
static bool intersect_line_point(
    glm::vec2 const &point,
    glm::vec2 const &start,
    glm::vec2 const &end)
{
    double dx = end.x - start.x;
    double dy = end.y - start.y;
    if (dy == 0)
    {
        return point.y == start.y;
    }
    else if (dx != 0)
    {
        double m = dy / dx;
        double b = start.y - m * start.x;
        return ISCLOSE(point.y, m * point.x + b);
    }
    else
    {
        return point.x == start.x;
    }
}

static std::pair<double, double> intersect_line_line(
    glm::vec2 const &start1,
    glm::vec2 const &end1,
    glm::vec2 const &start2,
    glm::vec2 const &end2)
{
    auto denom = glm::determinant(
        glm::mat2{
            glm::vec2{start1.x - end1.x, start1.y - end1.y},
            glm::vec2{start2.x - end2.x, start2.y - end2.y}});

    /* there's a solution */
    if (denom != 0)
    {
        auto numer_t = glm::determinant(
            glm::mat2{
                glm::vec2{start1.x - start2.x, start1.y - start2.y},
                glm::vec2{start2.x - end2.x, start2.y - end2.y}});
        auto numer_u = glm::determinant(
            glm::mat2{
                glm::vec2{start1.x - end1.x, start1.y - end1.y},
                glm::vec2{start1.x - start2.x, start1.y - start2.y}});

        return {numer_t / denom, numer_u / denom};
    }
    /* parallel or coincident */
    else
    {
        auto dx1 = end1.x - start1.x;
        auto dx2 = end2.x - start2.x;

        /* vertical lines */
        if (dx1 == 0 || dx2 == 0)
        {
            /* coincident */
            if (start1.x == start2.x)
            {
                return {INFINITY, INFINITY};
            }
            else
            {
                return {NAN, NAN};
            }
        }
        else
        {
            auto m1 = (end1.y - start1.y) / dx1;
            auto m2 = (end2.y - start2.y) / dx2;
            auto b1 = start1.y - m1 * start1.x;
            auto b2 = start2.y - m2 * start2.x;

            /* coincident */
            if (ISCLOSE(b1, b2))
            {
                return {INFINITY, INFINITY};
            }
            else
            {
                return {NAN, NAN};
            }
        }
    }
}
#undef ISCLOSE

static double _sign(
    glm::vec2 const &p0,
    glm::vec2 const &p1,
    glm::vec2 const &p2)
{
    return (
          (p0.x - p2.x) * (p1.y - p2.y)
        - (p1.x - p2.x) * (p0.y - p2.y));
}

static bool point_in_triangle(
    glm::vec2 const &p0,
    glm::vec2 const &p1,
    glm::vec2 const &p2,
    glm::vec2 const &point)
{
    auto d1 = _sign(point, p0, p1);
    auto d2 = _sign(point, p1, p2);
    auto d3 = _sign(point, p2, p0);

    bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(neg && pos);
}


/* check if p1 is inside p2 */
static bool polygon_in_polygon(
    std::vector<Vertex> const &p1,
    std::vector<Vertex> const &p2)
{
    for (size_t i = 0; i < p1.size(); ++i)
    {
        auto ip1 = (i + 1) % p1.size();
        for (size_t j = 0; j < p2.size(); ++j)
        {
            auto jp1 = (j + 1) % p2.size();
            auto tu = intersect_line_line(
                    glm::vec2{p1[i].x  , p1[i].y  },
                    glm::vec2{p1[ip1].x, p1[ip1].y},
                    glm::vec2{p2[j].x  , p2[j].y  },
                    glm::vec2{p2[jp1].x, p2[jp1].y});
            auto t = tu.first;
            auto u = tu.second;
            if (0.0 <= t && t < 1.0 && 0.0 <= u && u <= 1.0)
            {
                return false;
            }
        }
    }
    return point_in_polygon(p1[0], p2);
}

static bool point_in_polygon(
    Vertex const &point,
    std::vector<Vertex> const &polygon)
{
    glm::vec2 ray_origin{point.x, point.y};
    glm::vec2 ray_delta{0, 1};
    size_t intersects = 0;
    for (size_t i = 0; i < polygon.size(); ++i)
    {
        auto ip1 = (i + 1) % polygon.size();
        auto tu = intersect_line_line(
            glm::vec2{polygon[i].x, polygon[i].y},
            glm::vec2{polygon[ip1].x, polygon[ip1].y},
            ray_origin,
            ray_origin + ray_delta);
        auto t = tu.first;
        auto u = tu.second;
        if (u >= 0.0 && 0.0 <= t && t < 1.0)
        {
            intersects++;
        }
    }
    return intersects % 2 != 0;
}


/* test if a line is contained by the polygon */
static bool line_in_polygon(
    glm::vec2 const &start,
    glm::vec2 const &end,
    std::vector<Vertex> const &polygon)
{
    auto translation = -start;
    auto translated = end - start;

    auto angle = atan2(translated.y, translated.x);
    glm::mat2 rotation{
        glm::vec2{ cos(-angle), sin(-angle)},
        glm::vec2{-sin(-angle), cos(-angle)}};

    std::vector<double> intersections{};
    for (size_t i = 0; i < polygon.size(); ++i)
    {
        size_t ip1 = (i + 1) % polygon.size();
        glm::vec2 p0o{polygon[i].x, polygon[i].y};
        glm::vec2 p1o{polygon[ip1].x, polygon[ip1].y};

        auto p0 = rotation * (p0o + translation);
        auto p1 = rotation * (p1o + translation);

        if ((p0.y >= 0) != (p1.y > 0))
        {
            auto dx = p1.x - p0.x;
            auto dy = p1.y - p0.y;

            auto x = p1.x;
            if (dx != 0)
            {
                auto m = dy / dx;
                if (m != 0)
                {
                    auto b = p0.y - m * p0.x;
                    x = -b / m;
                }
            }
            intersections.push_back(x);
        }
    }

    size_t count = 0;
    for (auto x : intersections)
    {
        count += (x < 0);
    }
    size_t countzero = 0;
    for (auto x : intersections)
    {
        countzero += (x == 0);
    }

    if (count == 0)
    {
        return countzero != 0;
    }
    else
    {
        if ((count + countzero) % 2 != 0)
        {
            return true;
        }
        else
        {
            return count % 2 != 0;
        }
    }
}


/* check if a polygon is wound counterclockwise */
static bool is_counterclockwise(std::vector<Vertex> const &polygon)
{
    int winding = 0;
    for (size_t i = 0; i < polygon.size(); ++i)
    {
        size_t ip1 = (i + 1) % polygon.size();
        winding += (
            (polygon[ip1].x - polygon[i].x)
            * (polygon[ip1].y + polygon[i].y));
    }
    return winding > 0;
}

/* get the interior angle */
static double interior_angle(
    glm::vec2 const &A,
    glm::vec2 const &B,
    glm::vec2 const &C)
{
    auto v1 = B - A;
    auto v2 = C - B;

    auto diagonal = v2 - v1;

    auto r3 = atan2(diagonal.y, diagonal.x);
    auto r1 = atan2(-v1.y, -v1.x) - r3;
    auto r2 = atan2( v2.y,  v2.x) - r3;

    if (r1 < 0)
    {
        r1 += 2 * M_PI;
    }
    if (r2 <= 0)
    {
        r2 += 2 * M_PI;
    }

    auto theta = acos(
        glm::dot(glm::normalize(-v1), glm::normalize(v2)));

    if (r2 < r1)
    {
        theta = (2 * M_PI) - theta;
    }
    return theta;
}

