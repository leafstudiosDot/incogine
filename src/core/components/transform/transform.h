#include "../components.h"
#include "../../objects/objects.h"

class Transform : public Component {
    private:
        string component_name = "Transform";
        Position pos = {0.0, 0.0, 0.0};
    public:
        void setPosition(Position p);
};
