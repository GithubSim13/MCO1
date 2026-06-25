#pragma once
#include <string>

typedef std::string String;

class ConsoleManager {
public:
    static ConsoleManager* getInstance();
    static void initialize();
    static void destroy();

    void run();

private:
    ConsoleManager() {}
    static ConsoleManager* instance;

    bool isInitialized = false;
    void printHeader();
    void handleCommand(const String& command);
};