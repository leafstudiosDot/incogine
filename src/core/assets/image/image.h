#include <SDL/SDL.h>
#include <SDL_image.h>
#include <iostream>
#include <string>
using namespace std;

class Image {
public:
    Image(SDL_Renderer* renderer);
    ~Image();

    void render(float x, float y);

private:
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    float width, height;

    void loadFromMemory(const unsigned char* data, unsigned int size);
};
