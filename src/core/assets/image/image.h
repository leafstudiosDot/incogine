#include "../../engine/engine.h"

#include <SDL_image.h>
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
