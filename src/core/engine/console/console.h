#include "../engine.h"
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <mutex>

#ifndef CONSOLE_H
#define CONSOLE_H

class Console {
    public:
        Console(Engine** engine) : engine(engine) {}

        void start();
        void loop();
        void Command(const std::string& command);

    private:
        Engine** engine;

        enum CommandType {
            CMD_QUIT,
            //CMD_RESET,
            CMD_UNKNOWN
        };

        enum CommandType parseCommand(const std::string &command) const;
};

#endif