#include "../objects.h"

#ifndef CUBE_H
#define CUBE_H

class Cube : public Object {
    private:
        Position pos;

    public:
        Cube() : Object(Position(0, 0, 0)), pos(0, 0, 0) {}
};

#endif
