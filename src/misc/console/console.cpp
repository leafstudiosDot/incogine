#include "console.h"

Console::Console() {

}

Console::~Console() {

}

void Console::Println(string strout) {
    cout << strout << endl;
    logs.push_front(strout);
}

void Console::PrintGUI(string strout, bool addtoLog) {
    if (addtoLog) {
        cout << strout << endl;
        logs.push_front(strout);
    } else {
        cout << strout << endl;
    }
}