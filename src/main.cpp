#include "core/engine/engine.h"
#include "core/platforms/platforms.h"

using namespace std;

int main() {
    cout << "Incogine by leastudiosDot v" << INCOGINE_VERSION << "\n";
    cout << "Running " << PROJECT_NAME << " v" << PROJECT_VERSION << " by " << PROJECT_AUTHOR << "\n";
    cout << "\n" << PROJECT_DESCRIPTION << "\n";
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