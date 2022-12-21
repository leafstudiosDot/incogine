//
//  component.hpp
//  Incogine
//
//  Created by moritz on 11/27/22.
//
#pragma once
#ifndef component_hpp
#define component_hpp

#include "../../core.hpp"
#include "../entity.hpp"

class Component {
public:
    virtual string getType() = 0;
    virtual void disableComponent() = 0;
    virtual void enableComponent() = 0;
    virtual bool enabled() = 0;
    virtual void Update() = 0;
    virtual void Start() = 0;
};

#endif /* component_hpp */
