#include <string>
#include <vector>
#include "../components/components.h"

#ifndef OBJECTS_H
#define OBJECTS_H

class Component;

struct Position {
    double x;
    double y;
    double z;
    
    Position(double x, double y, double z) : x(x), y(y), z(z) {}
};

class Object {
    private:
        std::vector<Component> components;
        Position pos;

    public:
        Object(Position pos);

        Component& getComponent(int index);

        void addComponent(const Component& component);
};

#endif