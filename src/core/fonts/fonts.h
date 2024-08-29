#include <iostream>
#include <SDL_ttf.h>
using namespace std;

class Font {
    private:
        TTF_Font* font;
        bool fontLoaded;
        SDL_Surface* surface;
        SDL_Texture* texture;
        char* text_content;
        SDL_Color color = {255, 255, 255};
        SDL_Renderer* renderer;
    public:
        Font();
        ~Font();

        void Init(SDL_Renderer* renderer);
        void setFont(char* data, int size);
        void setFontRaw(TTF_Font* font);
        void renderUI(int x, int y);

        inline TTF_Font* GetFont() { return font; }
};