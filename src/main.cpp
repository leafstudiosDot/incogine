#include "core/engine/engine.h"
#include "core/platforms/platforms.h"

using namespace std;

int main() {
    cout << "Incogine by leastudiosDot" << endl;
    Engine* engine = Engine::Instance();
    engine->Init();

    printPlatform();

    while (engine->running()) {
        engine->Events();
        engine->Update();
        engine->Render();
    }

    engine->Cleanup();

    return 0;
}