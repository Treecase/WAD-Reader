/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#ifndef _TEXTURE_H
#define _TEXTURE_H

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>



/* IMPORTANT:
 *  Textures MUST NOT be initialized before the GL context is created.
 *  This means they CANNOT be declared at global scope except as
 *  pointers!
 */
class GLTexture
{
private:
    GLuint _id;


    /* no copying allowed! */
    GLTexture &operator=(GLTexture const &other) = delete;
    GLTexture(GLTexture const &other) = delete;

public:

    size_t const width, height;

    /* bind the texture */
    void bind(void) const;

    /* get the Texture's ID */
    GLuint id(void) const;


    /* NOTE: the required format is 'color, alpha, unused, unused',
     * each being 8-bits unsigned
     * (note that the shader only considers alpha as on or off) */
    GLTexture(size_t width, size_t height, void const *data);
    ~GLTexture();
};


#endif

