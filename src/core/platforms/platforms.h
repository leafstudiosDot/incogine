#include "../engine/engine.h"
using namespace std;

void printPlatform() {
    #if defined(_WIN32) || defined(_WIN64)
        cout << "Platform - Microsoft Windows" << endl;
    #elif defined(__APPLE__) && defined(__MACH__)
        #include <TargetConditionals.h>
        #if TARGET_OS_MAC
            cout << "Platform - Apple macOS" << endl;
        #elif TARGET_OS_IPHONE
            cout << "Platform - Apple iOS" << endl;
        #endif
    #elif defined(__linux__)
        cout << "Platform - Linux" << endl;
    #elif defined(__ANDROID__)
        cout << "Platform - Android" << endl;
    #elif defined(__ORBIS__)
        cout << "Platform - Sony PlayStation 4" << endl;
    #elif defined(__PROSPERO__)
        cout << "Platform - Sony PlayStation 5" << endl;
    #elif defined(_DURANGO)
        cout << "Platform - Microsoft Xbox One" << endl;
    #elif defined(__XBOXONE__)
        cout << "Platform - Microsoft Xbox Series X/S" << endl;
    #elif defined(__NX__)
        cout << "Platform - Nintendo Switch" << endl;
    #elif defined(__Mira__)
        cout << "Platform - leafstudiosDot Firoma Mira" << endl;
    #else
        cout << "Unknown platform" << endl;
    #endif
}