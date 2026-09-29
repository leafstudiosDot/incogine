#include "objects.h"
#include "../components/script/scriptcomponent.h"
#include <algorithm>

Object::Object(const std::string& name, Position pos, Scale scale, Rotation rotation)
    : name(name), pos(pos), scale(scale), rotation(rotation), id(nextId++) {
    liveObjects.push_back(this);
}

Object::~Object() {
    destroyScripts();
    if (parent) {
        auto& siblings = parent->children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
        parent = nullptr;
    }
    for (Object* child : children) {
        if (child) {
            child->parent = nullptr;
        }
    }
    children.clear();
    auto it = std::find(liveObjects.begin(), liveObjects.end(), this);
    if (it != liveObjects.end()) liveObjects.erase(it);
}

std::vector<Object*> Object::liveObjects;
uint64_t Object::nextId = 1;

Object* Object::FindByName(const std::string& name) {
    for (Object* o : liveObjects) {
        if (o && o->name == name) return o;
    }
    return nullptr;
}

std::vector<Object*> Object::FindAllByName(const std::string& name) {
    std::vector<Object*> out;
    for (Object* o : liveObjects) {
        if (o && o->name == name) out.push_back(o);
    }
    return out;
}

void Object::setName(const std::string& newName) {
    name = newName;
}

std::string Object::getName() const {
    return name;
}

uint64_t Object::getId() const {
    return id;
}

void Object::setId(uint64_t newId) {
    id = newId;
    if (newId != UINT64_MAX && newId >= nextId) {
        nextId = newId + 1;
    }
}

Object* Object::FindById(uint64_t id) {
    for (Object* o : liveObjects) {
        if (o && o->id == id) return o;
    }
    return nullptr;
}

bool Object::setParent(Object* newParent) {
    if (newParent == parent) {
        return true;
    }
    if (newParent) {
        for (Object* p = newParent; p; p = p->parent) {
            if (p == this) {
                return false; // would create a cycle
            }
        }
    }
    if (parent) {
        auto& siblings = parent->children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
    }
    parent = newParent;
    if (parent) {
        parent->children.push_back(this);
    }
    return true;
}

Object* Object::getParent() const {
    return parent;
}

const std::vector<Object*>& Object::getChildren() const {
    return children;
}

size_t Object::getChildCount() const {
    return children.size();
}

Object* Object::getChild(size_t index) const {
    if (index < children.size()) {
        return children[index];
    }
    return nullptr;
}

bool Object::addChild(Object* child) {
    if (!child) {
        return false;
    }
    return child->setParent(this);
}

void Object::removeChild(Object* child) {
    if (child && child->parent == this) {
        child->setParent(nullptr);
    }
}

Component* Object::getComponent(int index) {
    if (index >= 0 && index < static_cast<int>(components.size())) {
        return components[index].get();
    }
    return nullptr;
}

int Object::getComponentCount() const {
    return static_cast<int>(components.size());
}

void Object::addComponent(std::unique_ptr<Component> component) {
    components.push_back(std::move(component));
}

Component* Object::getComponentByName(const std::string& name) {
    for (auto& comp : components) {
        if (comp->getComponentName() == name) {
            return comp.get();
        }
    }
    return nullptr;
}

void Object::startScripts() {
    for (auto& comp : components) {
        ScriptComponent* script = dynamic_cast<ScriptComponent*>(comp.get());
        if (script) {
            script->Start();
        }
    }
}

void Object::updateScripts() {
    for (auto& comp : components) {
        ScriptComponent* script = dynamic_cast<ScriptComponent*>(comp.get());
        if (script) {
            script->Update();
        }
    }
}

void Object::destroyScripts() {
    for (auto& comp : components) {
        ScriptComponent* script = dynamic_cast<ScriptComponent*>(comp.get());
        if (script) {
            script->OnDestroy();
        }
    }
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
}
