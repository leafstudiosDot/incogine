#include "fonts.h"
#include "../assets/assetmanager.h"
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

bool Font::setFontFile(const char* path, double pointSize) {
    SDL_IOStream* io = AssetManager::Instance().Open(path);
    if (!io) {
        std::cerr << "setFontFile: asset not found: " << path << std::endl;
        return false;
    }

    if (fontLoaded && font) {
        TTF_CloseFont(font);
        fontLoaded = false;
    }

    font = TTF_OpenFontIO(io, 1, pointSize);
    if (!font) {
        SDL_CloseIO(io);
        std::cerr << "TTF_OpenFontIO Error: " << SDL_GetError() << std::endl;
        return false;
    }

    fontData = nullptr;
    fontDataSize = 0;
    basePointSize = pointSize;
    currentPointSize = pointSize;

    fontLoaded = true;
    return true;
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

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    QuadRenderer::DrawQuad(x, y, fontWidth, fontHeight,
                           color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f,
                           textTexture);

    glDisable(GL_BLEND);
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

