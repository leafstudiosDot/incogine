//
//  fonts.hpp
//  Incogine
//
//  Created by moritz on 10/20/22.
//

#ifndef fonts_hpp
#define fonts_hpp

#include "../core.hpp"

char fontFile[] = "./font.ttf";

class Fonts {
public:
    ~Fonts();
    
    void RenderFont(const char* content, SDL_Rect *position, SDL_Color color);

private:
    int round(double x);
    int nextpoweroftwo(int x);
    int w, h;
    GLuint texture;
    SDL_Surface *initial;
    SDL_Surface *intermediary;
    TTF_Font* font;
};

#endif /* fonts_hpp */
