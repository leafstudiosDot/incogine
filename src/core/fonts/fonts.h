#include <iostream>
#include <SDL_ttf.h>
using namespace std;

#ifndef FONTS_H
#define FONTS_H

struct FontSize {
	float width;
	float height;
};

class Font {
    private:
        const char* data;
        int size;
        TTF_Font* font;
        bool fontLoaded;
        SDL_Surface* surface;
        SDL_Texture* texture;
        char* text_content;
        SDL_Color color = {255, 255, 255};
        SDL_Renderer* renderer;
        int fontSize;
        FontSize fontWidth;
    public:
        Font();
        ~Font();

        void Init(SDL_Renderer* renderer);
        void setFont(const char* data, int size);
        void setFontRaw(TTF_Font* font);
        void renderUI(float x, float y);
        void setColor(Uint8 newColorR, Uint8 newColorG, Uint8 newColorB, Uint8 newColorA);
		void setFontSize(int newSize);
		void setTextContent(const char* content);
		FontSize getFontWidth();

        inline TTF_Font* GetFont() { return font; }
};

#endif