/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#ifndef _MESH_H
#define _MESH_H

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <glm/glm.hpp>

#include <vector>


class Mesh
{
public:

    struct Vertex
    {
        GLfloat x, y, z;
        GLfloat s, t;
    };


    /* get the number of vertices */
    GLsizei size(void) const;

    /* bind the mesh's VAO */
    void bind(void) const;


    Mesh(
        std::vector<Mesh::Vertex> vertices,
        std::vector<GLuint> indices={});
    Mesh(std::initializer_list<Mesh::Vertex> vertices);
    ~Mesh();


private:
    GLuint _vao, _vbo, _ebo;
    std::vector<Mesh::Vertex> _vertices;
    std::vector<GLuint> _indices;

    Mesh const &operator=(Mesh const &other) = delete;
    Mesh(Mesh const &other) = delete;
};


#endif

