#include <thread>
#include "core/engine/engine.h"
#include "core/engine/console/console.h"
#include "core/platforms/platforms.h"

using namespace std;

int main(int argc, char* argv[]) {
    Engine* engine = Engine::Instance(argc, argv);
    Console console(&engine);

    console.start();

    cout << "Incogine by leastudiosDot v" << INCOGINE_VERSION << "\n";
    cout << "Running " << PROJECT_NAME << " v" << PROJECT_VERSION << " by " << PROJECT_AUTHOR << "\n";
    cout << "\n" << PROJECT_DESCRIPTION << "\n\n";

    if (engine->inDevMode()) {
        cout << "--RUNNING IN DEVELOPMENT MODE--\n";
    }

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