//
//  collider.hpp
//  Incogine
//
//  Created by moritz on 11/27/22.
//
#pragma once

#include "component.hpp"

class Component;
class ColliderComponent : public Component {
private:
    string c_type = "collider";
    bool c_enabled = false;
public:
    string getType() {
        return c_type;
    }
    
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
