#include "audio.h"

namespace {
	MIX_Mixer* gMixer = nullptr;

	MIX_Mixer* getMixer() {
		if (!gMixer) {
			if (!MIX_Init()) {
				SDL_Log("MIX_Init failed: %s", SDL_GetError());
				return nullptr;
			}

			if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0) {
				if (!SDL_Init(SDL_INIT_AUDIO)) {
					SDL_Log("SDL audio init failed: %s", SDL_GetError());
					return nullptr;
				}
			}

			gMixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
			if (!gMixer) {
				SDL_Log("MIX_CreateMixerDevice failed: %s", SDL_GetError());
			}
		}
		return gMixer;
	}
}

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

	if (audioData) {
		MIX_DestroyAudio(audioData);
		audioData = nullptr;
	}

	MIX_Mixer* mixer = getMixer();
	if (!mixer) {
		return;
	}

	audioData = MIX_LoadAudio(mixer, audioFilePath.c_str(), true);
	if (!audioData) {
		SDL_Log("MIX_LoadAudio failed: %s", SDL_GetError());
	}
}

Audio::~Audio() {
	if (audioData) {
		MIX_DestroyAudio(audioData);
		audioData = nullptr;
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
	if (audioData) {
		if (loop > 0 && Engine::Instance(0, nullptr)->inDevMode()) {
			std::cerr << "Loop count is ignored with current SDL3_mixer playback path." << std::endl;
		}

		MIX_Mixer* mixer = getMixer();
		if (!mixer) {
			return;
		}

		if (!MIX_PlayAudio(mixer, audioData)) {
			SDL_Log("MIX_PlayAudio error: %s", SDL_GetError());
		}
	}
}