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
			std::cerr << "Audio path is null in Audio::Audio" << std::endl;
		}
		return;
	}

	// Audio is resolved through the AssetManager: disk first (assets/ next
	// to the executable, APK assets on Android, preloaded FS on Web),
	// embedded fallback when built with ICG_EMBED_ASSETS=ON.
	SDL_IOStream* io = AssetManager::Instance().Open(path);
	if (!io) {
		if (Engine::Instance(0, nullptr)->inDevMode()) {
			std::cerr << "Audio asset not found: " << path << std::endl;
		}
		return;
	}

	MIX_Mixer* mixer = getMixer();
	if (!mixer) {
		SDL_CloseIO(io);
		return;
	}

	audioData = MIX_LoadAudio_IO(mixer, io, true, true);
	if (!audioData) {
		SDL_Log("MIX_LoadAudio_IO failed: %s", SDL_GetError());
		return;
	}

	track = MIX_CreateTrack(mixer);
	if (!track) {
		SDL_Log("MIX_CreateTrack failed: %s", SDL_GetError());
		return;
	}

	if (!MIX_SetTrackAudio(track, audioData)) {
		SDL_Log("MIX_SetTrackAudio failed: %s", SDL_GetError());
	}
}

Audio::~Audio() {
	stop();

	if (track) {
		MIX_DestroyTrack(track);
		track = nullptr;
	}

	if (audioData) {
		MIX_DestroyAudio(audioData);
		audioData = nullptr;
	}

	if (Engine::Instance(0, nullptr)->inDevMode()) {
		std::cout << "Audio object destroyed." << std::endl;
	}
}

void Audio::play(int loop) {
	if (Engine::Instance(0, nullptr)->inDevMode()) {
		std::cout << "Audio playback started." << std::endl;
	}
	if (audioData && track) {
		SDL_PropertiesID options = SDL_CreateProperties();
		if (options) {
			SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER, static_cast<Sint64>(loop));
		}

		if (!MIX_PlayTrack(track, options)) {
			SDL_Log("MIX_PlayTrack error: %s", SDL_GetError());
		}

		if (options) {
			SDL_DestroyProperties(options);
		}
	}
}

void Audio::stop() {
	if (track) {
		if (!MIX_StopTrack(track, 0)) {
			SDL_Log("MIX_StopTrack error: %s", SDL_GetError());
		}
	}
}