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
    string audioFilePath;
    Mix_Music* audiomix = nullptr;

    inline fs::path getExecutableDir() {
        const char* base = SDL_GetBasePath();
        if (!base) {
            return fs::current_path();
        }

        fs::path exeDir = fs::path(base).parent_path();

        SDL_free(const_cast<char*>(base));
        return exeDir;
    }
public:
    Audio(const char* path);
    ~Audio();

	void play(int loop); // -1 Infinite loop, 0 play once, 1 play twice, etc.
};
