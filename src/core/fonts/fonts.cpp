#include "fonts.h"
using namespace std;

Font::Font() {
    font = nullptr;
    fontLoaded = false;
}

Font::~Font() {
    if (fontLoaded) {
        TTF_CloseFont(font);
    }
    if (surface != nullptr) {
        SDL_FreeSurface(surface);
    }
    if (texture != nullptr) {
        SDL_DestroyTexture(texture);
    }
}

void Font::Init(SDL_Renderer* renderer) {
    if (TTF_Init() == -1) {
        cerr << "TTF_Init Error: " << TTF_GetError() << endl;
        return;
    }

    this->renderer = renderer;
}

void Font::setFont(const char* data, int size) {
    font = TTF_OpenFontRW(SDL_RWFromConstMem(data, size), 1, fontSize);
    if (font == nullptr) {
        cerr << "TTF_OpenFont Error: " << TTF_GetError() << endl;
        return;
    }

    fontLoaded = true;
}

void Font::setFontRaw(TTF_Font* font) {
    this->font = font;
    fontLoaded = true;
}

void Font::renderUI(const char* content, int x, int y, int modifiedFontSize) {
    if (!fontLoaded || renderer == nullptr) {
        std::cerr << "Font or Renderer is not initialized in Font::renderUI" << std::endl;
        return;
    }

    text_content = const_cast<char*>(content);
    fontSize = modifiedFontSize;
    surface = TTF_RenderText_Solid(font, text_content, color);
    if (surface == nullptr) {
        cerr << "TTF_RenderText_Solid Error: " << TTF_GetError() << endl;
        return;
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == nullptr) {
        cerr << "SDL_CreateTextureFromSurface Error: " << SDL_GetError() << endl;
        return;
    }

    SDL_Rect destRect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, nullptr, &destRect);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}