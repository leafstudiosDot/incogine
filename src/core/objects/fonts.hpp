//
//  fonts.hpp
//  Incogine
//
//  Created by moritz on 10/20/22.
//

#ifndef fonts_hpp
#define fonts_hpp

#include "../core.hpp"

class Fonts {
public:
    Fonts(const char* loc);
    ~Fonts();
    
    void RenderFont(const char* content, float x, float y, float z, SDL_Color color, GLfloat objWidth, GLfloat objHeight);
    void RenderFontHUD(const char* content, float x, float y, float z, SDL_Color color, GLfloat objWidth, GLfloat objHeight, GLfloat rotateangle);

private:
    int round(double x);
    int nextpoweroftwo(int x);
    TTF_Font *fontassigned;
};

#endif /* fonts_hpp */
