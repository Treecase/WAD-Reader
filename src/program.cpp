/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#include "program.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <stdexcept>
#include <string>



Shader::Shader(GLenum shader_type, std::string path)
:   id{glCreateShader(shader_type)}
{
    FILE *f = fopen(path.c_str(), "r");
    if (f == nullptr)
    {
        throw std::system_error{
            errno,
            std::generic_category(),
            "fopen"};
    }

    fseek(f, 0L, SEEK_END);
    long len = ftell(f);
    fseek(f, 0L, SEEK_SET);

    char *src = new char[len + 1];
    src[len] = '\0';
    fread(src, 1, len, f);
    fclose(f);

    glShaderSource(id, 1, &src, nullptr);

    int success = 0;
    glCompileShader(id);
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[512] = {0};
        glGetShaderInfoLog(id, sizeof(log), nullptr, log);
        delete[] src;
        throw std::runtime_error(
            "glCompileShader("
            + path
            + ") -- "
            + std::string(log, sizeof(log)));
    }
    delete[] src;
}

Shader::~Shader()
{
    glDeleteShader(id);
}





void Program::use(void) const
{
    glUseProgram(id);
}

void Program::set(std::string var, glm::mat4 const &value) const
{
    glUniformMatrix4fv(
        glGetUniformLocation(id, var.c_str()),
        1,
        GL_FALSE,
        glm::value_ptr(value));
}

void Program::set(std::string var, glm::vec4 const &value) const
{
    glUniform4fv(
        glGetUniformLocation(id, var.c_str()),
        1,
        glm::value_ptr(value));
}

void Program::set(std::string var, glm::vec3 const &value) const
{
    glUniform3fv(
        glGetUniformLocation(id, var.c_str()),
        1,
        glm::value_ptr(value));
}

void Program::set(std::string var, glm::vec2 const &value) const
{
    glUniform2fv(
        glGetUniformLocation(id, var.c_str()),
        1,
        glm::value_ptr(value));
}

void Program::set(std::string var, GLuint value) const
{
    glUniform1i(glGetUniformLocation(id, var.c_str()), value);
}



Program::Program(std::initializer_list<Shader> shaders)
:   id{glCreateProgram()}
{
    for (auto &shader : shaders)
    {
        glAttachShader(id, shader.id);
    }

    glLinkProgram(id);

    int success = 0;
    glGetProgramiv(id, GL_LINK_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetShaderInfoLog(id, sizeof(log), nullptr, log);
        throw std::runtime_error(
            "glLinkProgram -- "
            + std::string(log, sizeof(log)));
    }
}

Program::~Program()
{
    glDeleteProgram(id);
}

