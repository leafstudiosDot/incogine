#include <SDL/SDL.h>
#include <SDL_mixer.h>
#include "core/engine/engine.h"
#include <iostream>
#include <string>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

class Audio {
private:
    float width, height;
    const char* audioFilePath;
    Mix_Music* audiomix = nullptr;

public:
    Audio(const char* path);
    ~Audio();

	void play(int loop); // -1 Infinite loop, 0 play once, 1 play twice, etc.
};
