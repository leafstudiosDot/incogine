#include "image.h"

Image::Image(SDL_Renderer* renderer) : renderer(renderer), texture(nullptr) {
    //loadFromMemory(image_data, image_size);
}

Image::~Image() {
    if (texture) {
        SDL_DestroyTexture(texture);
    }
}

void Image::render(int x, int y) {
    if (!texture) return;

    SDL_Rect dstRect = {x, y, width, height};
    SDL_RenderCopy(renderer, texture, nullptr, &dstRect);
}

void Image::loadFromMemory(const unsigned char* data, unsigned int size) {
    SDL_RWops* rw = SDL_RWFromConstMem(data, size);
    if (!rw) {
        throw std::runtime_error("Failed to create SDL_RWops");
    }

    SDL_Surface* surface = IMG_Load_RW(rw, 1);
    if (!surface) {
        SDL_RWclose(rw);
        throw std::runtime_error("Failed to load image from memory: " + std::string(IMG_GetError()));
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        throw std::runtime_error("Failed to create texture from surface: " + std::string(SDL_GetError()));
    }

    width = surface->w;
    height = surface->h;
    SDL_FreeSurface(surface);
}
