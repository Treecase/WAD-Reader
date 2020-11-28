/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#include "camera.hpp"



Camera::Camera()
:   _forward{0, 0, 0},
    pos{0, 0, 0},
    up{0, 0, 0},
    angle{0, 0}
{
}

Camera::Camera(glm::vec3 pos, glm::vec3 forward, glm::vec3 up)
:   _forward{glm::normalize(forward)},
    pos{pos},
    up{glm::normalize(up)}
{
}

void Camera::move(double dx, double dy, double dz)
{
    move(glm::vec3{dx, dy, dz});
}

void Camera::move(glm::vec3 vector)
{
    auto f = forward();
    pos += vector.x * glm::normalize(glm::cross(forward(), up));
    pos += vector.y * up;
    pos += vector.z * glm::normalize(glm::vec3{f.x, 0, f.z});
}

void Camera::rotate(double horizontal, double vertical)
{
    angle.x = fmod(angle.x + horizontal, 360.0);
    angle.y = fmod(angle.y + vertical, 360.0);
}

glm::mat4 Camera::matrix(void) const
{
    return glm::lookAt(pos, pos + forward(), up);
}

glm::vec3 Camera::forward(void) const
{
    auto mat = glm::rotate(
        glm::rotate(
            glm::mat4{1},
            (float)glm::radians(angle.x),
            glm::vec3{0, 1, 0}),
        (float)glm::radians(angle.y),
        glm::cross(_forward, up));

    return glm::vec3{mat * glm::vec4{_forward, 1}};
}

