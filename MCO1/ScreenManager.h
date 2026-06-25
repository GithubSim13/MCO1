#pragma once
#include "Process.h"
#include <string>

typedef std::string String;

class ScreenManager {
public:
    static ScreenManager* getInstance();
    static void initialize();
    static void destroy();

    void openScreen(const String& processName);
    void reattachScreen(const String& processName);
    void listScreens();

private:
    ScreenManager() {}
    static ScreenManager* instance;

    void printProcessSMI(Process* process);
    String getCurrentTimestamp();
};