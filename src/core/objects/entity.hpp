//
//  entity.hpp
//  Incogine
//
//  Created by moritz on 11/27/22.
//

#ifndef entity_hpp
#define entity_hpp

#include "../core.hpp"

class Entity {
public:
    Entity();
    ~Entity();
    
    void Update();
    void Render();
    void Destroy();
};

#endif /* entity_hpp */
