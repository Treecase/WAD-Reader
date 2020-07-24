/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#include "texture.hpp"

#include <stdexcept>



GLTexture::GLTexture(size_t width, size_t height, void const *data)
:   _id{0},
    width{width},
    height{height}
{
    glGenTextures(1, &_id);
    glBindTexture(GL_TEXTURE_2D, _id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    /* copy the image data to the texture */
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8UI,
        width, height,
        0,
        GL_RGBA_INTEGER,
        GL_UNSIGNED_BYTE,
        data);
}

GLTexture::~GLTexture()
{
    glDeleteTextures(1, &_id);
}

void GLTexture::bind(void) const
{
    glBindTexture(GL_TEXTURE_2D, _id);
}

GLuint GLTexture::id(void) const
{
    return _id;
}

