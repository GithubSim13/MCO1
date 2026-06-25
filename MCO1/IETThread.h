#pragma once
#include <thread>

class IETThread {
public:
    virtual void run() = 0;
    void start();
    void join();

private:
    std::thread workerThread;
};