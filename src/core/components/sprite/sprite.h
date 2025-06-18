#include "../components.h"
#include "../../objects/objects.h"

#ifndef SPRITE_H
#define SPRITE_H

class Sprite : public Component {
    private:
        string component_name = "Sprite";
        Color color = {255, 255, 255, 255};
    public:
        Sprite() : Component(nullptr) {}
        void setColor(const Color& newColor);
        Color getColor() const;
};

#endif