#define _CRT_SECURE_NO_WARNINGS
#include "ConsoleManager.h"

int main() {
    ConsoleManager::initialize();
    ConsoleManager::getInstance()->run();
    ConsoleManager::destroy();
    return 0;
}