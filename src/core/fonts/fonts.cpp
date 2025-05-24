#include "fonts.h"
using namespace std;

Font::Font() : font(nullptr), fontLoaded(false), textTexture(0) {}

Font::~Font() {
    if (textTexture) {
        glDeleteTextures(1, &textTexture);
    }

    if (fontLoaded && font) {
        TTF_CloseFont(font);
    }
}

bool Font::setFont(const unsigned char* data, unsigned int dataSize, double pointSize) {
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

    fontData = data;
    fontDataSize = dataSize;
    basePointSize = pointSize;
    currentPointSize = pointSize;

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
    glDisable(GL_TEXTURE_2D);
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

    if (textTexture) {
        glDeleteTextures(1, &textTexture);
        textTexture = 0;
    }

    // Render text to SDL surface (UTF-8 blended)
    SDL_Color sdl_color = { color.r, color.g, color.b, color.a };
    SDL_Surface* surf = TTF_RenderText_Blended(font, utf8_text.c_str(), 0, sdl_color);
    if (!surf) {
        std::cerr << "TTF_RenderText_Blended Error: " << SDL_GetError() << std::endl;
        return;
    }

    fontWidth = surf->w;
    fontHeight = surf->h;

    // Generate and bind OpenGL texture
    glGenTextures(1, &textTexture);
    glBindTexture(GL_TEXTURE_2D, textTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    int bytesPerPixel = SDL_BYTESPERPIXEL(surf->format);
    int rowLength     = surf->pitch / bytesPerPixel;
    glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);

    GLenum format = GL_RGBA;
    GLenum type   = GL_UNSIGNED_BYTE;
    GLenum uploadFormat = GL_RGBA;

    switch (surf->format) {
        case SDL_PIXELFORMAT_ARGB8888:
            uploadFormat = GL_BGRA;
            break;
        case SDL_PIXELFORMAT_RGBA8888:
            uploadFormat = GL_RGBA;
            break;
        case SDL_PIXELFORMAT_ABGR8888:
            uploadFormat = GL_RGBA;
            break;
        case SDL_PIXELFORMAT_BGRA8888:
            uploadFormat = GL_BGRA;
            break;
        default:
            std::cerr << "Unsupported pixel format: " << SDL_GetPixelFormatName(surf->format) << std::endl;
            SDL_DestroySurface(surf);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glBindTexture(GL_TEXTURE_2D, 0);
            return;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surf->w, surf->h, 0,
                 uploadFormat, type, surf->pixels);

    // Texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
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

void Font::setFontScale(float scale) {
    fontScale = scale;
    double newPointSize = basePointSize * scale;
    if (abs(newPointSize - currentPointSize) > 0.1) {
        currentPointSize = newPointSize;
        if (fontLoaded && font) {
            TTF_CloseFont(font);
            fontLoaded = false;
        }

        font = TTF_OpenFontIO(SDL_IOFromConstMem(fontData, fontDataSize), 1, currentPointSize);
        if (font) {
            fontLoaded = true;
            updateTexture();
        }
    }
}

