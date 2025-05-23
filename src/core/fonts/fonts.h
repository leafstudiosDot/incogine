#include <iostream>
#include <string>
#include <GL/glew.h>
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
        TTF_Font* font;
		const unsigned char* fontData;
		unsigned int fontDataSize;
        bool fontLoaded;
        string utf8_text;
        SDL_Color color = {255, 255, 255};
        const char* text_content;
        GLuint textTexture;
        float fontWidth;
        float fontHeight;
        float fontScale = 1.0f;

        double basePointSize;
        double currentPointSize;

        void updateTexture();
    public:
        Font();
        ~Font();

        static bool Init();
        bool setFont(const unsigned char* data, unsigned int dataSize, double pointSize);
        void renderUI(float x, float y);
        void setColor(GLubyte r, GLubyte g, GLubyte b, GLubyte a = 255);
		FontSize getSize() const;
        void setTextContent(const std::string& content);
        void setFontScale(float scale);

        inline TTF_Font* GetFont() { return font; }
};

#endif