#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include "../assetmanager.h"
#include <iostream>
#include <string>
using namespace std;

class Image {
public:
    Image(SDL_Renderer* renderer);
    ~Image();

    // Loads the image from an asset path (disk first, embedded fallback).
    // Returns false when the asset cannot be found or decoded.
    bool load(const char* path);

    void render(float x, float y);

private:
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    float width, height;

    void loadFromMemory(const unsigned char* data, unsigned int size);
};