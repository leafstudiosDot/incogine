#include "../components.h"
#include "../../objects/objects.h"

class Transform : public Component {
    private:
        string component_name = "Transform";
        Position pos = {0.0, 0.0, 0.0};
        Scale scale = {0.0, 0.0, 0.0};
        Rotation rotation = {0.0, 0.0, 0.0};
    public:
        void setPosition(Position p);
        void setScale(Scale s);
        void setRotation(Rotation r);
        
        Position getPosition() const;
        Scale getScale() const;
        Rotation getRotation() const;
};
