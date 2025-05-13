#include "sprite.h"

void Sprite::setColor(const Color& newColor) {
    color = newColor;
}

Color Sprite::getColor() const {
    return color;
}