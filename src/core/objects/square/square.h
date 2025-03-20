#include "../objects.h"

#ifndef SQUARE_H
#define SQUARE_H

class Square : public Object {
    private:

    public:
        Square() : Object(Position(0, 0, 0), Scale(1, 1, 1), Rotation(0, 0, 0)) {}
        ~Square();
};

#endif
