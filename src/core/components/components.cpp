#include "components.h"
#include "../objects/objects.h"

Component::Component(Object* parent) : linkedobj(parent) {}

void Component::setComponentName(string name) {
    component_name = name;
}

string Component::getComponentName() {
    return component_name;
}