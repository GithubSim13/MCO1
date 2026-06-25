#include "IETThread.h"

void IETThread::start() {
    workerThread = std::thread(&IETThread::run, this);
}

void IETThread::join() {
    if (workerThread.joinable())
        workerThread.join();
}