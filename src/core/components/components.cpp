#include "components.h"
#include "../objects/objects.h"

Component::Component(Object* parent) : linkedobj(parent) {}

Component::~Component() {}

void Component::setComponentName(string name) {
    component_name = name;
}

string Component::getComponentName() {
    return component_name;
}

Object* Component::getLinkedObject() const {
    return linkedobj;
}

void Component::Start() {}

void Component::Update() {}

void Component::OnDestroy() {}
