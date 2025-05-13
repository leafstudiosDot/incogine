#include "square.h"

Square::Square(SDL_Renderer* _renderer) : Object("Square", Position(0, 0, 0), Scale(1, 1, 1), Rotation(0, 0, 0)), renderer(_renderer) {
    sprite = new Sprite(); // Initialize the Sprite component
    sprite->setColor({255, 255, 255, 255}); // Default color: white
    addComponent(*sprite); // Add the Sprite component to the object
}

Square::~Square() {
    delete sprite;
}

void Square::setColor(const Color& color) {
    sprite->setColor(color);
}

Color Square::getColor() const {
    return sprite->getColor();
}

void Square::Render() {
    if (!renderer) { 
		cout << "[Object:" << Object::getName() << "] Renderer is not set for this object" << endl;
        return;
    };

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, sprite->getColor().r, sprite->getColor().g, sprite->getColor().b, sprite->getColor().a);

    // Get position and scale (assuming your Position/Scale types have x/y)
    int x = static_cast<float>(getPosition().x);
    int y = static_cast<float>(getPosition().y);
    int w = static_cast<float>(getScale().x * 100);
    int h = static_cast<float>(getScale().y * 100);

    SDL_FRect rect = { x, y, w, h };
    SDL_RenderFillRect(renderer, &rect);
}