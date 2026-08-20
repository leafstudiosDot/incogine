#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <cmath>
#include "../components/components.h"
#include "core/engine/engine.h"

#ifndef OBJECTS_H
#define OBJECTS_H

class Component;
class ScriptComponent;

struct Position {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    Position() : x(0.0), y(0.0), z(0.0) {}
    Position(double x, double y, double z) : x(x), y(y), z(z) {}

    double distanceTo(const Position& other) const {
        return sqrt((x - other.x) * (x - other.x) + (y - other.y) * (y - other.y) + (z - other.z) * (z - other.z));
    }

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

    void scaleBy(double factor) {
        x *= factor;
        y *= factor;
        z *= factor;
    }

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

    void reset() {
        x = 0.0;
        y = 0.0;
        z = 0.0;
    }

    bool operator==(const Rotation& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const Rotation& other) const {
        return !(*this == other);
    }
};

struct Color {
    int r{255};
    int g{255};
    int b{255};
    int a{255};

    Color(int r, int g, int b, int a) : r(r), g(g), b(b), a(a) {}

    bool operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }

    bool operator!=(const Color& other) const {
        return !(*this == other);
    }
};

class Object {
    private:
        string name;
        std::vector<std::unique_ptr<Component>> components;
        Position pos;
        Scale scale;
        Rotation rotation;

    public:
        Object(const std::string& name, Position pos, Scale scale, Rotation rotation);
        ~Object();

        // Name
        void setName(const std::string& newName);
        std::string getName() const;

        // Component
        Component* getComponent(int index);
        int getComponentCount() const;
        void addComponent(std::unique_ptr<Component> component);
        Component* getComponentByName(const std::string& name);

        // Script helpers
        void startScripts();
        void updateScripts();
        void destroyScripts();

        // Transform Manipulation
        void setPosition(const Position& newPos);
        Position getPosition() const;

        void setScale(const Scale& newScale);
        Scale getScale() const;

        void setRotation(const Rotation& newRotation);
        Rotation getRotation() const;

        void Render();
};

#endif
