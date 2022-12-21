//
//  entity.hpp
//  Incogine
//
//  Created by moritz on 11/27/22.
//

#ifndef entity_hpp
#define entity_hpp

#include "../core.hpp"
#include "components/component.hpp"

class Component;
class Entity {
public:
    Entity();
    ~Entity();
    
    vector<Component*> components;
    
    void Start();
    void Update();
    void Render();
    void Destroy();
};

#endif /* entity_hpp */
