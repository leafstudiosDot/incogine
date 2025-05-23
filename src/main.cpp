#include "core/engine/engine.h"
#include "core/engine/version.h"
#include "core/platforms/platforms.h"

using namespace std;

int main(int argc, char* argv[]) {
    Engine* engine = Engine::Instance(argc, argv);

    cout << "Incogine by leastudiosDot v" << VERSION_STRING << "\n";
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