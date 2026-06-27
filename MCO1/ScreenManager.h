#pragma once
#include "Process.h"
#include <string>

typedef std::string String;

class ScreenManager {
public:
    static ScreenManager* getInstance();
    static void initialize();
    static void destroy();

    // screen -s  : open interactive view for a newly created process
    void openScreen(Process* process);

    // screen -r  : reattach to an existing process by name
    void reattachScreen(const String& processName);

    // screen -ls : list all running and finished processes
    void listScreens();

    // report-util: write csopesy-log.txt
    void reportUtil();

    // Utility: get a formatted timestamp string (used by ConsoleManager too)
    String getTimestamp();

private:
    ScreenManager() {}
    static ScreenManager* instance;

    void printProcessSMI(Process* process);
    void runScreenLoop(Process* process);
};
