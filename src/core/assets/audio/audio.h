#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include "core/engine/engine.h"
#include "../assetmanager.h"
#include <iostream>

using namespace std;

struct MIX_Audio;
struct MIX_Track;

class Audio {
private:
    float width, height;
    MIX_Audio* audioData = nullptr;
    MIX_Track* track = nullptr;
public:
    Audio(const char* path);
    ~Audio();

	void play(int loop); // -1 Infinite loop, 0 play once, 1 play twice, etc.
	void stop();
};