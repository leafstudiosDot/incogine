//
//  transform.hpp
//  Incogine
//
//  Created by moritz on 11/27/22.
//
#pragma once

#include "component.hpp"

class Component;
class TransformComponent : public Component {
private:
    const string c_type = "transform";
    float c_location[2] = {0, 0};
    float c_height;
    float c_width;
public:
    TransformComponent(float width, float height, float *location);
    float& getLocation();
    float getSize();
    
    string getType() {
        return c_type;
    }
    
    void disableComponent();
    void enableComponent();
    bool enabled() {
        return false;
    }
    
    void Start(Entity* self) {
        
    }
};
