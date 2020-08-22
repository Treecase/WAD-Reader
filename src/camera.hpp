/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 */

#ifndef _CAMERA_H
#define _CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


class Camera
{
private:
    glm::vec3 _forward;

public:
    glm::vec3 pos, up;
    glm::vec2 angle;

    /* move the camera relative to its forward/up vectors:
     * ie. if vector.x > 0, move right; if < 0, move left */
    void move(double dx, double dy, double dz);
    void move(glm::vec3 vector);

    /* rotate the camera left/right and up/down */
    void rotate(double horizontal, double vertical);

    /* get the camera transform matrix */
    glm::mat4 matrix(void) const;

    /* get the camera's forward vector */
    glm::vec3 forward(void) const;


    Camera();
    Camera(glm::vec3 pos, glm::vec3 forward, glm::vec3 up);
};


#endif

