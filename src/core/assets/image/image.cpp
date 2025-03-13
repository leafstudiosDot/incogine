#include "image.h"

Image::Image(SDL_Renderer* renderer) : renderer(renderer), texture(nullptr) {
    //loadFromMemory(image_data, image_size);
}

Image::~Image() {
    if (texture) {
        SDL_DestroyTexture(texture);
    }
}

void Image::render(float x, float y) {
    if (!texture) return;

    SDL_FRect dstRect = {x, y, width, height};
    SDL_RenderTexture(renderer, texture, nullptr, &dstRect);
}

void Image::loadFromMemory(const unsigned char* data, unsigned int size) {
    SDL_IOStream* rw = SDL_IOFromConstMem(data, size);
    if (!rw) {
        throw std::runtime_error("Failed to create SDL_IOStream");
    }

    SDL_Surface* surface = IMG_Load_IO(rw, 1);
    if (!surface) {
        SDL_CloseIO(rw);
        throw std::runtime_error("Failed to load image from memory: " + std::string(SDL_GetError()));
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_DestroySurface(surface);
        throw std::runtime_error("Failed to create texture from surface: " + std::string(SDL_GetError()));
    }

    width = surface->w;
    height = surface->h;
    SDL_DestroySurface(surface);
}
