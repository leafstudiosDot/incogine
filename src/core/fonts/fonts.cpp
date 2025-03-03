#include "fonts.h"
using namespace std;

Font::Font() {
    font = nullptr;
    fontLoaded = false;
    int fontSize = 24;
}

Font::~Font() {
    if (fontLoaded) {
        TTF_CloseFont(font);
        font = nullptr;
    }
}

void Font::Init(SDL_Renderer* renderer) {
    if (TTF_Init() == -1) {
        cerr << "TTF_Init Error: " << TTF_GetError() << endl;
        return;
    }

    this->renderer = renderer;
}

void Font::setFont(const char* _data, int _size) {
	data = _data;
	size = _size;
    setFontSize(fontSize);
}

void Font::setFontRaw(TTF_Font* font) {
    this->font = font;
    fontLoaded = true;
}

void Font::setFontSize(int newSize) {
    if (fontSize == newSize || data == nullptr) return;

    fontSize = newSize;
    if (fontLoaded) {
        TTF_CloseFont(font);
    }

    font = TTF_OpenFontRW(SDL_RWFromConstMem(data, size), 1, fontSize);
    if (font == nullptr) {
        std::cerr << "TTF_OpenFont Error: " << TTF_GetError() << std::endl;
        fontLoaded = false;
    } else {
        fontLoaded = true;
    }
}

void Font::renderUI(int x, int y) {
    if (!fontLoaded || renderer == nullptr) {
        std::cerr << "Font or Renderer is not initialized in Font::renderUI" << std::endl;
        return;
    }

    surface = TTF_RenderText_Blended(font, text_content, color);
    if (surface == nullptr) {
        cerr << "TTF_RenderText_Solid Error: " << TTF_GetError() << endl;
        return;
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == nullptr) {
        cerr << "SDL_CreateTextureFromSurface Error: " << SDL_GetError() << endl;
        return;
    }

    if (texture != nullptr) {
        SDL_Rect destRect = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, nullptr, &destRect);

        SDL_FreeSurface(surface);
        SDL_DestroyTexture(texture);
    }
}

void Font::setColor(Uint8 newColorR, Uint8 newColorG, Uint8 newColorB, Uint8 newColorA) {
    color = { newColorR, newColorG, newColorB, newColorA };
}

FontSize Font::getFontWidth() {
	int textWidth, textHeight;
    TTF_SizeText(font, text_content, &textWidth, &textHeight);
    fontWidth.width = textWidth;
    fontWidth.height = textHeight;
	return fontWidth;
}

void Font::setTextContent(const char* content) {
	text_content = const_cast<char*>(content);
}