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
    if (!TTF_Init()) {
        cerr << "TTF_Init Error: " << SDL_GetError() << endl;
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

    font = TTF_OpenFontIO(SDL_IOFromConstMem(data, size), 1, fontSize);
    if (font == nullptr) {
        std::cerr << "TTF_OpenFont Error: " << SDL_GetError() << std::endl;
        fontLoaded = false;
    } else {
        fontLoaded = true;
    }
}

void Font::renderUI(float x, float y) {
    if (!fontLoaded || renderer == nullptr) {
        std::cerr << "Font or Renderer is not initialized in Font::renderUI" << std::endl;
        return;
    }

    surface = TTF_RenderText_Blended(font, text_content, 0, color);
    if (surface == nullptr) {
        cerr << "TTF_RenderText_Blended Error: " << SDL_GetError() << endl;
        return;
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == nullptr) {
        cerr << "SDL_CreateTextureFromSurface Error: " << SDL_GetError() << endl;
        return;
    }

    if (texture != nullptr) {
        SDL_FRect destRect = {x, y, surface->w, surface->h};
        SDL_RenderTexture(renderer, texture, nullptr, &destRect);

        SDL_DestroySurface(surface);
        SDL_DestroyTexture(texture);
    }
}

void Font::setColor(Uint8 newColorR, Uint8 newColorG, Uint8 newColorB, Uint8 newColorA) {
    color = { newColorR, newColorG, newColorB, newColorA };
}

FontSize Font::getFontWidth() {
	float textWidth = 0, textHeight = 0;
    //TTF_GetTextSize(font, text_content, &textWidth, &textHeight);
    fontWidth.width = textWidth;
    fontWidth.height = textHeight;
	return fontWidth;
}

void Font::setTextContent(const char* content) {
    utf8_text = content;
    text_content = utf8_text.c_str();
}

void Font::setTextContent(const wchar_t* content) {
    std::wstring wstr(content);

    utf8_text.clear();
    for (wchar_t wc : wstr) {
        encode_utf8_char(static_cast<uint32_t>(wc), utf8_text);
    }

    text_content = utf8_text.c_str();
}

void Font::encode_utf8_char(uint32_t wc, std::string& out) {
    if (wc <= 0x7F) {
        out.push_back(char(wc));
    }
    else if (wc <= 0x7FF) {
        out.push_back(char(0xC0 | ((wc >> 6) & 0x1F)));
        out.push_back(char(0x80 | (wc & 0x3F)));
    }
    else if (wc <= 0xFFFF) {
        out.push_back(char(0xE0 | ((wc >> 12) & 0x0F)));
        out.push_back(char(0x80 | ((wc >> 6) & 0x3F)));
        out.push_back(char(0x80 | (wc & 0x3F)));
    }
    else if (wc <= 0x10FFFF) {
        out.push_back(char(0xF0 | ((wc >> 18) & 0x07)));
        out.push_back(char(0x80 | ((wc >> 12) & 0x3F)));
        out.push_back(char(0x80 | ((wc >> 6) & 0x3F)));
        out.push_back(char(0x80 | (wc & 0x3F)));
    }
}