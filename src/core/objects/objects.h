#include <string>
#include <vector>
#include <memory>
#include <cstdint>
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
        uint64_t id;
        Object* parent = nullptr;
        std::vector<Object*> children;
        static std::vector<Object*> liveObjects;
        static uint64_t nextId;

    public:
        Object(const std::string& name, Position pos, Scale scale, Rotation rotation);
        virtual ~Object();

        // Name
        void setName(const std::string& newName);
        std::string getName() const;

        // Identity — unique per object, stable across renames. The runtime
        // auto-assigns ids; loaders (and Studio's scene round-trip) override
        // them with setId(), which pushes the generator past any loaded id
        // so later objects never collide. Used by C#/Kodo Object.find.
        uint64_t getId() const;
        void setId(uint64_t newId);
        static Object* FindById(uint64_t id);

        // Live-object registry (used by C#/Kodo Object.find).
        static Object* FindByName(const std::string& name);
        static std::vector<Object*> FindAllByName(const std::string& name);

        // Hierarchy — non-owning links for the scene tree. Lifetimes are
        // unchanged: whoever new'd an object still owns it. Destroying an
        // object detaches it from its parent and orphans its children
        // (their parent becomes null); there is no cascading delete.
        // Stored transforms stay in world space; hierarchy is organizational
        // (editors translate subtrees explicitly when moving a parent).
        // setParent/addChild return false (and change nothing) when the
        // link would create a cycle.
        bool setParent(Object* newParent);
        Object* getParent() const;
        const std::vector<Object*>& getChildren() const;
        size_t getChildCount() const;
        Object* getChild(size_t index) const;
        bool addChild(Object* child);
        void removeChild(Object* child);

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
