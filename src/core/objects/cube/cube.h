#include "../objects.h"

#ifndef CUBE_H
#define CUBE_H

class Cube : public Object {
    private:

    public:
        Cube() : Object(Position(0, 0, 0), Scale(1, 1, 1), Rotation(0, 0, 0)) {}
        ~Cube();
};

#endif
