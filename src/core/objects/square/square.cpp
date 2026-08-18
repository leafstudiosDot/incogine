#include "square.h"

Square::Square() : Object("Square", Position(0, 0, 0), Scale(1, 1, 1), Rotation(0, 0, 0)) {
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
    auto c = sprite->getColor();
    auto win = Engine::Instance(0, nullptr)->GetWindowSize();

    float x = static_cast<float>(getPosition().x);
    float y = static_cast<float>(getPosition().y);
    float w = static_cast<float>(getScale().x * win.width);
    float h = static_cast<float>(getScale().y * win.height);

    glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    QuadRenderer::DrawQuad(x, y, w, h,
                           c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);

    glDisable(GL_BLEND);

    glEnable(GL_DEPTH_TEST);
}