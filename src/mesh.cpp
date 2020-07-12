/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#include "mesh.hpp"



GLsizei Mesh::size(void) const
{
    return _indices.size();
}

void Mesh::bind(void) const
{
    glBindVertexArray(_vao);
}



Mesh::Mesh(
  std::vector<Mesh::Vertex> vertices,
  std::vector<GLuint> indices)
:   _vao{0},
    _vbo{0},
    _ebo{0},
    _vertices{vertices},
    _indices{indices}
{
    if (_indices.empty())
    {
        for (size_t i = 0; i < _vertices.size(); ++i)
        {
            _indices.push_back(i);
        }
    }

    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);
    glGenBuffers(1, &_ebo);

    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);

    glBufferData(
        GL_ARRAY_BUFFER,
        _vertices.size() * sizeof(Mesh::Vertex),
        &_vertices[0],
        GL_STATIC_DRAW);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        _indices.size() * sizeof(GLuint),
        &_indices[0],
        GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Mesh::Vertex),
        (void *)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Mesh::Vertex),
        (void *)(sizeof(GLfloat) * 3));
}

Mesh::Mesh(std::initializer_list<Mesh::Vertex> vertices)
:   Mesh(std::vector<Mesh::Vertex>{vertices})
{
}

Mesh::~Mesh()
{
    glDeleteBuffers(1, &_ebo);
    glDeleteBuffers(1, &_vbo);
    glDeleteVertexArrays(1, &_vao);
}

