//
//  sprite.hpp
//  Incogine
//
//  Created by moritz on 1/17/23.
//
#pragma once

#include "component.hpp"

class Component;
class ColliderComponent : public Component {
private:
    string c_type = "sprite";
    bool c_enabled = false;
public:
    string getType() {
        return c_type;
    }
    
    char* spriteloc;
    
    void disableComponent() {
        c_enabled = false;
    }
    
    void enableComponent() {
        c_enabled = true;
    }
    
    bool enabled() {
        return c_enabled;
    }
    
    void Update() {
        
    }
    
    void Start() {
        
    }
};
