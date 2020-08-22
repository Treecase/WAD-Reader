/* See LICENSE file for copyright and license details.
 * program.hpp
 */

#ifndef _PROGRAM_H
#define _PROGRAM_H

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <glm/glm.hpp>

#include <system_error>
#include <string>
#include <vector>



class Shader
{
public:
    GLuint const id;

    Shader(GLenum shader_type, std::string path);
    ~Shader();
};



class Program
{
private:
    Program(Program const &) = delete;
    Program &operator=(Program const &) = delete;

public:
    GLuint const id;

    /* set this as the active shader */
    void use(void) const;

    /* set a uniform value in the shader */
    void set(std::string var, glm::mat4 const &value) const;
    void set(std::string var, glm::vec4 const &value) const;
    void set(std::string var, glm::vec3 const &value) const;
    void set(std::string var, glm::vec2 const &value) const;
    void set(std::string var, GLuint value) const;


    Program(std::initializer_list<Shader> shaders);
    ~Program();
};


#endif

