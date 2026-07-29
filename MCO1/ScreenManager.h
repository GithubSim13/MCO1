#pragma once
#include "Process.h"
#include <string>

typedef std::string String;

class ScreenManager {
public:
    static ScreenManager* getInstance();
    static void initialize();
    static void destroy();

    // screen -s / -c : open interactive view for a newly created process
    void openScreen(Process* process);

    // screen -r  : reattach to an existing process by name
    void reattachScreen(const String& processName);

    // screen -ls : list all running and finished processes
    void listScreens();

    // report-util: write csopesy-log.txt
    void reportUtil();

    // Full formatted timestamp, e.g. "(07/29/2026 02:15:30PM)" - used for logs.
    String getTimestamp();

    // Bare "HH:MM:SS" (24-hour) - used specifically for the memory access
    // violation message format required by the spec.
    String getTimeOnly();

private:
    ScreenManager() {}
    static ScreenManager* instance;

    void printProcessSMI(Process* process);
    void runScreenLoop(Process* process);
};
