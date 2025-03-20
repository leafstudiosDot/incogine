#include "objects.h"

Object::Object(Position pos, Scale scale, Rotation rotation) : pos(pos), scale(scale), rotation(rotation) {

}

Object::~Object() {

}

Component& Object::getComponent(int index) {
    if (true) {
        return components[index];
    } else {
        throw std::runtime_error("Access to this component is not allowed.");
    }
}

// Component
void Object::addComponent(const Component& component) {
    components.push_back(component);
}

// Position
void Object::setPosition(const Position& newPos) { 
    pos = newPos; 
}

Position Object::getPosition() const { 
    return pos;
}

// Scale
void Object::setScale(const Scale& newScale) { 
    scale = newScale; 
}

Scale Object::getScale() const {
    return scale; 
}

// Rotation
void Object::setRotation(const Rotation& newRotation) { 
    rotation = newRotation; 
}

Rotation Object::getRotation() const { 
    return rotation; 
}