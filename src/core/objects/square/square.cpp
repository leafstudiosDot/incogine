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
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    auto& win = Engine::Instance(0, nullptr)->GetWindowSize();
    glOrtho(0, win.width, win.height, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto c = sprite->getColor();
    glColor4ub(c.r, c.g, c.b, c.a);

    float x = static_cast<float>(getPosition().x);
    float y = static_cast<float>(getPosition().y);
    float w = static_cast<float>(getScale().x * win.width);
    float h = static_cast<float>(getScale().y * win.height);

    glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x, y + h);
    glEnd();

    glDisable(GL_BLEND);

	glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}