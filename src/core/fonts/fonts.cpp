#include "fonts.h"
using namespace std;

Font::Font() : font(nullptr), fontLoaded(false) {}

Font::~Font() {
    if (textTexture) {
        glDeleteTextures(1, &textTexture);
    }

    if (fontLoaded && font) {
        TTF_CloseFont(font);
    }
}

bool Font::Init() {
    if (!TTF_Init()) {
        cerr << "TTF_Init Error: " << SDL_GetError() << endl;
        return false;
    }
    glEnable(GL_TEXTURE_2D);
    return true;
}

bool Font::setFont(const unsigned char* data, unsigned int dataSize, int pointSize) {
    if (fontLoaded && font) {
        TTF_CloseFont(font);
        fontLoaded = false;
    }
    SDL_IOStream* rw = SDL_IOFromConstMem(data, dataSize);
    if (!rw) {
        std::cerr << "SDL_IOStream from memory failed: " << SDL_GetError() << std::endl;
        return false;
    }
    font = TTF_OpenFontIO(rw, 1, pointSize);
    if (!font) {
        SDL_CloseIO(rw);
        std::cerr << "TTF_OpenFontIO Error: " << SDL_GetError() << std::endl;
        return false;
    }
    fontLoaded = true;
    return true;
}

void Font::renderUI(float x, float y) {
    if (!textTexture) {
		cout << "Font::renderUI Error: No texture available" << endl;
        return;
    };

    // Save matrices
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    GLint vp[4]; glGetIntegerv(GL_VIEWPORT, vp);
    glOrtho(0, vp[2], vp[3], 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    // Draw quad
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textTexture);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4ub(color.r, color.g, color.b, color.a);
    float x2 = x + fontWidth;
    float y2 = y + fontHeight;
    glBegin(GL_QUADS);
      glTexCoord2f(0,0); glVertex2f(x, y);
      glTexCoord2f(1,0); glVertex2f(x2, y);
      glTexCoord2f(1,1); glVertex2f(x2, y2);
      glTexCoord2f(0,1); glVertex2f(x, y2);
    glEnd();
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
    glEnable(GL_DEPTH_TEST);
    // Restore matrices
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void Font::setColor(GLubyte r, GLubyte g, GLubyte b, GLubyte a) {
    color = { r, g, b, a };
    if (!utf8_text.empty()) updateTexture();
}

void Font::updateTexture() {
    if (!fontLoaded || utf8_text.empty()) return;
    // Free old texture
    if (textTexture) {
        glDeleteTextures(1, &textTexture);
        textTexture = 0;
    }
    // Render text to SDL surface
    SDL_Surface* surf = TTF_RenderText_Blended(font, utf8_text.c_str(), utf8_text.length(), color);
    if (!surf) {
        std::cerr << "TTF_RenderText_Blended Error: " << SDL_GetError() << std::endl;
        return;
    }
    fontWidth = surf->w;
    fontHeight = surf->h;
    // Upload to OpenGL texture
    glGenTextures(1, &textTexture);
    glBindTexture(GL_TEXTURE_2D, textTexture);

    // Ensure proper row alignment for non-4-byte widths
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surf->w, surf->h, 0, GL_BGRA, GL_UNSIGNED_BYTE, surf->pixels);
    // Clamp to edge to avoid bleeding
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Restore default alignment
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    glBindTexture(GL_TEXTURE_2D, 0);
    SDL_DestroySurface(surf);
}

void Font::setTextContent(const std::string& content) {
    utf8_text = content;
    updateTexture();
}

FontSize Font::getSize() const {
    return { float(fontWidth), float(fontHeight) };
}