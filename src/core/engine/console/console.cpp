#include "console.h"

Console::CommandType Console::parseCommand(const std::string &command) const {
    static const std::unordered_map<std::string, CommandType> commandMap = {
        std::make_pair("quit", CMD_QUIT),
        //{"reset_engine", CMD_RESET}
    };

    auto it = commandMap.find(command);
    if (it != commandMap.end()) {
        return it->second;
    }
    return CMD_UNKNOWN;
}

void Console::Command(const std::string &command) {
    switch (parseCommand(command)) {
        case CMD_QUIT:
            (*engine)->Quit();
            break;

        /*case CMD_RESET:
            delete *engine;
            *engine = new Engine();
            std::cout << "Engine reset." << std::endl;
            break;*/

        case CMD_UNKNOWN:
        default:
            std::cout << "Unknown command: " << command << std::endl;
            break;
    }
}

void Console::loop() {
    std::string command;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, command);
        Command(command);
    }
}

void Console::start() {
    struct LoopFunctor {
        Console* console;
        void operator()() { console->loop(); }
    };

    LoopFunctor functor = {this};
    std::thread t(functor);
    t.detach();
}