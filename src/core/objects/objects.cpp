#include "objects.h"

Object::Object(const std::string& name, Position pos, Scale scale, Rotation rotation) : name(name), pos(pos), scale(scale), rotation(rotation), renderer(nullptr) {

}

Object::~Object() {

}

void Object::setName(const std::string& newName) {
    name = newName;
}

std::string Object::getName() const {
    return name;
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

void Object::Render() {
    if (!renderer) {
        /*SDL_Rect rect;
        rect.x = static_cast<int>(pos.x);
        rect.y = static_cast<int>(pos.y);
        rect.w = static_cast<int>(scale.x);
        rect.h = static_cast<int>(scale.y);

        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // Example: Red color
        SDL_RenderFillRect(renderer, &rect);*/

        // If rendnerer is not set, we can use the default renderer
        cout << "Renderer is not set for this object" << endl;
    }
}