//
//  fonts.cpp
//  Incogine
//
//  Created by moritz on 10/20/22.
//

#include "fonts.hpp"
#ifndef INCOGINE_FONTS_H
#define INCOGINE_FONTS_H

Fonts::Fonts(const char* loc) {
    if(!(fontassigned = TTF_OpenFont(loc, 100))) {
        printf("Error loading font: %s", TTF_GetError());
    }
}

Fonts::~Fonts() {
    TTF_CloseFont(fontassigned);
}

GLuint stringTex = 0;

GLuint Fonts::TextToTexture(SDL_Color color, const char* text)
{
    SDL_Surface* msg = TTF_RenderText_Blended(fontassigned, text, color);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, msg->w, msg->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, msg->pixels);

    SDL_FreeSurface(msg);
    return tex;
}

void Fonts::RenderFont(const char* content, float x, float y, float z, SDL_Color color, GLfloat objWidth, GLfloat objHeight) {
    stringTex = TextToTexture(color, content);

    glColor3ub( 255, 255, 255 );
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, stringTex);
    glTranslated(x, y, z);
    glBegin(GL_QUADS);
        glTexCoord2f( 0.0f, 0.0f ); glVertex2f( (objWidth*(-1)), objHeight );
        glTexCoord2f( 1.0f, 0.0f ); glVertex2f( objWidth, objHeight );
        glTexCoord2f( 1.0f, 1.0f ); glVertex2f( objWidth, (objHeight*(-1)) );
        glTexCoord2f( 0.0f, 1.0f ); glVertex2f( (objWidth*(-1)), (objHeight*(-1)) );
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glDeleteTextures(1, &stringTex);
}

void Fonts::RenderFontHUD(const char *content, float x, float y, float z, SDL_Color color, GLfloat objWidth, GLfloat objHeight, GLfloat rotateangle) {
    stringTex = TextToTexture(color, content);

    glColor3ub( 255, 255, 255 );
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, stringTex);
    glTranslated(x, y, z);
    glRotatef(rotateangle, 0, 0, 1);
    glBegin(GL_QUADS);
        glTexCoord2f( 0.0f, 0.0f ); glVertex2f( (objWidth*(-1)), objHeight );
        glTexCoord2f( 1.0f, 0.0f ); glVertex2f( objWidth, objHeight );
        glTexCoord2f( 1.0f, 1.0f ); glVertex2f( objWidth, (objHeight*(-1)) );
        glTexCoord2f( 0.0f, 1.0f ); glVertex2f( (objWidth*(-1)), (objHeight*(-1)) );
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glDeleteTextures(1, &stringTex);
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

#endif
