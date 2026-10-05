#include "image.h"

Image::Image(SDL_Renderer* renderer) : renderer(renderer), texture(nullptr) {
}

Image::~Image() {
    if (texture) {
        SDL_DestroyTexture(texture);
    }
}

bool Image::load(const char* path) {
    SDL_IOStream* io = AssetManager::Instance().Open(path);
    if (!io) {
        std::cerr << "Image::load: asset not found: " << path << std::endl;
        return false;
    }

    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }

    SDL_Surface* surface = IMG_Load_IO(io, 1);
    if (!surface) {
        SDL_CloseIO(io);
        std::cerr << "IMG_Load_IO failed for " << path << ": " << SDL_GetError() << std::endl;
        return false;
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_DestroySurface(surface);
        std::cerr << "SDL_CreateTextureFromSurface failed: " << SDL_GetError() << std::endl;
        return false;
    }

    width = surface->w;
    height = surface->h;
    SDL_DestroySurface(surface);
    return true;
}

void Image::render(float x, float y) {
    if (!texture) return;

    SDL_FRect dstRect = {x, y, width, height};
    SDL_RenderTexture(renderer, texture, nullptr, &dstRect);
}
