#include <SDL/SDL.h>
#include <SDL_mixer.h>
#include "core/engine/engine.h"
#include <iostream>
#include <string>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

struct MIX_Audio;
struct MIX_Track;

class Audio {
private:
    float width, height;
    string audioFilePath;
    MIX_Audio* audioData = nullptr;
    MIX_Track* track = nullptr;

    inline fs::path getExecutableDir() {
        const char* base = SDL_GetBasePath();
        if (!base || !*base) {
            return fs::current_path();
        }

        try {
            return fs::u8path(base).parent_path();
        } catch (const fs::filesystem_error&) {
            return fs::current_path();
        }
    }
public:
    Audio(const char* path);
    ~Audio();

	void play(int loop); // -1 Infinite loop, 0 play once, 1 play twice, etc.
	void stop();
};
