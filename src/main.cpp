#include "core/engine/engine.h"

using namespace std;

int main() {
    Engine* engine = Engine::Instance();
    engine->Init();

    while (engine->running()) {
        engine->Events();
        engine->Update();
        engine->Render();
    }

    engine->Cleanup();

    return 0;
}