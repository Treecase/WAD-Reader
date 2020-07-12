/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#include "camera.hpp"



Camera::Camera()
:   _forward(0, 0, 0),
    pos(0, 0, 0),
    up(0, 0, 0)
{
}

Camera::Camera(glm::vec3 pos, glm::vec3 forward, glm::vec3 up)
:   _forward(glm::normalize(forward)),
    pos(pos),
    up(glm::normalize(up))
{
}

void Camera::move(double dx, double dy, double dz)
{
    move(glm::vec3(dx, dy, dz));
}

void Camera::move(glm::vec3 vector)
{
    pos += vector.x * glm::normalize(glm::cross(_forward, up));
    pos += vector.y * up;
    pos += vector.z * _forward;
}

void Camera::rotate(double horizontal, double vertical)
{
    auto hmat = glm::rotate(
        glm::mat4(1),
        (float)glm::radians(horizontal),
        glm::vec3(0, 1, 0));
    auto vmat = glm::rotate(
        glm::mat4(1),
        (float)glm::radians(vertical),
        glm::normalize(glm::cross(_forward, up)));

    _forward = glm::vec3(vmat * hmat * glm::vec4(_forward, 1));
}

glm::mat4 Camera::matrix(void) const
{
    return glm::lookAt(pos, pos + _forward, up);
}

glm::vec3 Camera::forward(void) const
{
    return _forward;
}

