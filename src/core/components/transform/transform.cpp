#include "transform.h"

void Transform::setPosition(Position p) {
    pos = p;
}

void Transform::setScale(Scale s) {
    scale = s;
}

void Transform::setRotation(Rotation r) {
    rotation = r;
}

Position Transform::getPosition() const {
    return pos;
}

Scale Transform::getScale() const {
    return scale;
}

Rotation Transform::getRotation() const {
    return rotation;
}
