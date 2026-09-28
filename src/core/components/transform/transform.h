#include "../components.h"
#include "../../objects/objects.h"

#ifndef TRANSFORM_H
#define TRANSFORM_H

class Transform : public Component {
    private:
        string component_name = "Transform";
        Position pos = {0.0, 0.0, 0.0};
        Scale scale = {1.0, 1.0, 1.0};
        Rotation rotation = {0.0, 0.0, 0.0};
    public:
        Transform() : Component(nullptr) { setComponentName("Transform"); }

        void setPosition(Position p);
        void setScale(Scale s);
        void setRotation(Rotation r);

        Position getPosition() const;
        Scale getScale() const;
        Rotation getRotation() const;
};

#endif
