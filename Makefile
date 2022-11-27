release-mac:
	g++ -g -Wall ./src/main.cpp ./src/core/core.cpp -I ./src -I ./src/*.cpp -I./reqs/include -L./reqs/lib -F/Library/Frameworks -framework SDL2 -lSDL2_image -lSDL2_ttf -ldl -framework OpenGL -march=x86-64 -o ./build/main

release-web:
	emcc src/core/core.cpp src/game/game.cpp src/main.cpp -Isrc/*.cpp -Ireqs/include -s WASM=1 -s USE_SDL=2 -s USE_WEBGL2=1 -s LEGACY_GL_EMULATION=1 -s USE_SDL_IMAGE=2 -s SDL2_IMAGE_FORMATS='["png"]' -s USE_SDL_TTF=2 --embed-file src/core/objects/def_font.ttf@/assets/fonts/def_font.ttf -o build/web/index.html
