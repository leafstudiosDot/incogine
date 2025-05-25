#include "audio.h"

Audio::Audio(const char* path) {
	if (!path) {
		if (Engine::Instance(0, nullptr)->inDevMode()) {
			std::cerr << "Audio path is null in Audio::setAudio" << std::endl;
		}
		return;
	}

	fs::path fullPath = getExecutableDir() / "data" / "audio" / path;
	audioFilePath = fullPath.string();


	if (Engine::Instance(0, nullptr)->inDevMode()) {
		std::cout << "Loading audio from: " << fullPath << std::endl;
	}

	if (!fs::exists(fullPath)) {
		if (Engine::Instance(0, nullptr)->inDevMode()) {
			std::cerr << "Audio file does not exist: " << audioFilePath << std::endl;
		}
		return;
	}

	if (audiomix) {
		Mix_FreeMusic(audiomix);
		audiomix = nullptr;
	}

	audiomix = Mix_LoadMUS(audioFilePath.c_str());
	if (!audiomix) {
		SDL_Log("Mix_LoadMUS failed: %s", SDL_GetError());
	}
}

Audio::~Audio() {
	if (audiomix) {
		Mix_FreeMusic(audiomix);
		audiomix = nullptr;
	}

	if (Engine::Instance(0, nullptr)->inDevMode()) {
		std::cout << "Audio object destroyed." << std::endl;
	}
}

void Audio::play(int loop = -1) {
	// Placeholder for audio playback logic
	if (Engine::Instance(0, nullptr)->inDevMode()) {
		std::cout << "Audio playback started." << std::endl;
	}
	if (audiomix) {
		if (Mix_PlayMusic(audiomix, loop) < 0) {
			SDL_Log("Mix_PlayMusic error: %s", SDL_GetError());
		}
	}
}