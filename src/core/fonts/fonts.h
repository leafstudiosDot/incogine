#include <iostream>
#include <SDL_ttf.h>
using namespace std;

#ifndef FONTS_H
#define FONTS_H

class Font {
    private:
        TTF_Font* font;
        bool fontLoaded;
        SDL_Surface* surface;
        SDL_Texture* texture;
        char* text_content;
        SDL_Color color = {255, 255, 255};
        SDL_Renderer* renderer;
        int fontSize = 24;
    public:
        Font();
        ~Font();

        void Init(SDL_Renderer* renderer);
        void setFont(const char* data, int size);
        void setFontRaw(TTF_Font* font);
        void renderUI(const char* content, int x, int y, int modifiedFontSize = 24);
        SDL_Color setColor(SDL_Color newColor);

        inline TTF_Font* GetFont() { return font; }
};

#endif