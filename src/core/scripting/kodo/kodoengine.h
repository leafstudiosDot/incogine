// Internal engine-boundary helpers shared by the Kodo interpreter TUs.
// Not part of the public scripting API.

#ifndef KODOENGINE_H
#define KODOENGINE_H

#include <cmath>
#include <random>

#include "../../objects/objects.h"
#include "../../objects/square/square.h"
#include "../../components/sprite/sprite.h"
#include "kodovalue.h"

namespace Kodo {
namespace internal {

inline int colorByte(double c) {
    int v = (int)std::round(c * 255.0);
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

inline void applyEngineColor(Object* o, const ColorV& c) {
    if (!o) return;
    Color ec(colorByte(c.r), colorByte(c.g), colorByte(c.b), colorByte(c.a));
    if (auto* sq = dynamic_cast<Square*>(o)) {
        sq->setColor(ec);
        return;
    }
    if (Component* comp = o->getComponentByName("Sprite")) {
        if (auto* spr = dynamic_cast<Sprite*>(comp)) spr->setColor(ec);
    }
}

inline ColorV readEngineColor(Object* o) {
    ColorV c;
    if (!o) return c;
    Color ec(255, 255, 255, 255);
    if (auto* sq = dynamic_cast<Square*>(o)) {
        ec = sq->getColor();
    } else if (Component* comp = o->getComponentByName("Sprite")) {
        if (auto* spr = dynamic_cast<Sprite*>(comp)) ec = spr->getColor();
    }
    c.r = ec.r / 255.0;
    c.g = ec.g / 255.0;
    c.b = ec.b / 255.0;
    c.a = ec.a / 255.0;
    return c;
}

inline std::mt19937& rng() {
    static std::mt19937 r{std::random_device{}()};
    return r;
}

inline const Value* namedArg(const std::map<std::string, Value>& named, const std::string& key) {
    auto it = named.find(key);
    return it != named.end() ? &it->second : nullptr;
}

} // namespace internal
} // namespace Kodo

#endif
