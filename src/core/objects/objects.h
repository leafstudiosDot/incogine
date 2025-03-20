#include <string>
#include <vector>
#include <stdexcept>
#include <cmath>
#include "../components/components.h"

#ifndef OBJECTS_H
#define OBJECTS_H

class Component;

struct Position {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    Position(double x, double y, double z) : x(x), y(y), z(z) {}

	// Calculates distance to another position
    double distanceTo(const Position& other) const {
        return sqrt((x - other.x) * (x - other.x) + (y - other.y) * (y - other.y) + (z - other.z) * (z - other.z));
    }

    // Equality operators
    bool operator==(const Position& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const Position& other) const {
        return !(*this == other);
    }
};

struct Scale {
    double x{1.0};
    double y{1.0};
    double z{1.0};

    Scale(double x, double y, double z) : x(x), y(y), z(z) {}

    // Scale by?
    void scaleBy(double factor) {
        x *= factor;
        y *= factor;
        z *= factor;
    }

    // Equality operators
    bool operator==(const Scale& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const Scale& other) const {
        return !(*this == other);
    }
};

struct Rotation {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    Rotation(double x, double y, double z) : x(x), y(y), z(z) {}

    // Reset rotation
    void reset() {
        x = 0.0;
        y = 0.0;
        z = 0.0;
    }

    // Equality operators
    bool operator==(const Rotation& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const Rotation& other) const {
        return !(*this == other);
    }
};

class Object {
    private:
        std::vector<Component> components;
        Position pos;
        Scale scale;
        Rotation rotation;

    public:
        Object(Position pos, Scale scale, Rotation rotation);
        ~Object();

        Component& getComponent(int index);

        void addComponent(const Component& component);

        // Transform Manipulation
        void setPosition(const Position& newPos);
        Position getPosition() const;

        void setScale(const Scale& newScale);
        Scale getScale() const;

        void setRotation(const Rotation& newRotation);
        Rotation getRotation() const;
};

#endif