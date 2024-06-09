#include "objects.h"

Object::Object(Position pos) : pos(pos) {}

Component& Object::getComponent(int index) {
    if (true) {
        return components[index];
    } else {
        throw std::runtime_error("Access to this component is not allowed.");
    }
}

void Object::addComponent(const Component& component) {
    components.push_back(component);
}