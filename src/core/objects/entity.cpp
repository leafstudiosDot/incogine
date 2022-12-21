//
//  entity.cpp
//  Incogine
//
//  Created by moritz on 11/27/22.
//

#include "entity.hpp"

Entity::Entity() {
    
}

Entity::~Entity() {
    
}

void Entity::Start() {
    for (Component* component : components) {
        component->Start();
    }
}

void Entity::Update() {
    for (Component* component : components) {
        component->Update();
    }
}

void Entity::Render() {
    
}

void Entity::Destroy() {
    
}
