#ifndef CONSOLE_H_
#define CONSOLE_H_

#include <iostream>
#include <string>
#include <deque>

using namespace std;
class Console {
public:
    Console();
    ~Console();

    void Println(string strout);
    void PrintGUI(string strout, bool addtoLog);
private:
    //int ShowLogMax = 255;
    deque<string> logs;
};

#endif
