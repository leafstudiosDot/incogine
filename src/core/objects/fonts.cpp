//
//  fonts.cpp
//  Incogine
//
//  Created by moritz on 10/20/22.
//

#include "fonts.hpp"

Fonts::~Fonts() {
    TTF_CloseFont(font);
}

void Fonts::RenderFont(const char* content, SDL_Rect *position, SDL_Color color) {
    initial = TTF_RenderText_Blended(font, content, color);
    
    w = nextpoweroftwo(initial->w);
    h = nextpoweroftwo(initial->h);
    
    intermediary = SDL_CreateRGBSurface(0, w, h, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    SDL_BlitSurface(initial, 0, intermediary, 0);
    
    // GL New Texture
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, 4, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, intermediary->pixels);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glColor3f(1.0f, 1.0f, 1.0f);
    
    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f);
            glVertex2f(position->x, position->y);
        glTexCoord2f(1.0f, 1.0f);
            glVertex2f(position->x, position->y);
        glTexCoord2f(1.0f, 0.0f);
            glVertex2f(position->x, position->y + h);
        glTexCoord2f(0.0f, 0.0f);
            glVertex2f(position->x, position->y + h);
    glEnd();
    glFinish();
    
    position->w = initial->w;
    position->h = initial->h;
    
    SDL_FreeSurface(initial);
    SDL_FreeSurface(intermediary);
    glDeleteTextures(1, &texture);
}

int Fonts::round(double x)
{
    return (int)(x + 0.5);
}

int Fonts::nextpoweroftwo(int x)
{
    double logbase2 = log(x) / log(2);
    return round(pow(2,ceil(logbase2)));
}
