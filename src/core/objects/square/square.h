#include "../objects.h"
#include "../../components/sprite/sprite.h"
#include "../../engine/engine.h"

#ifndef SQUARE_H
#define SQUARE_H

class Square : public Object {
    private:
        Sprite* sprite;
		SDL_Renderer* renderer;
    public:
        Square(SDL_Renderer* _renderer);
        ~Square();

        void setColor(const Color& color);
        Color getColor() const;
        void Render();
};

#endif
