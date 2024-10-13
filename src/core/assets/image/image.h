#include <SDL2/SDL.h>
#include <SDL_image.h>
#include <iostream>
#include <string>
using namespace std;

class Image {
public:
    Image(SDL_Renderer* renderer);
    ~Image();

    void render(int x, int y);

private:
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    int width, height;

    void loadFromMemory(const unsigned char* data, unsigned int size);
};
