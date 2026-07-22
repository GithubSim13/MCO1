#pragma once
#include "IMemoryAllocator.h"
#include <vector>
#include <unordered_map>
#include <iostream>
#include <fstream>

struct PageTableEntry {
    int frameNum = -1;      // Physical frame number (-1 if unmapped/swapped out)
    bool valid = false;     // Is page loaded in physical RAM?
    bool dirty = false;     // Has page been modified?
    int backingStoreOffset = -1; // Offset/location in backing store file
};

class PagingMemoryAllocator : public IMemoryAllocator {
public:
    PagingMemoryAllocator(size_t totalRamSize, size_t frameSize);
    ~PagingMemoryAllocator() override;

    void* allocate(size_t size) override;
    void deallocate(void* ptr) override;
    String visualizeMemory() override;

    // Tracker getters
    int getNumPagedIn() const { return numPagedIn; }
    int getNumPagedOut() const { return numPagedOut; }

private:
    size_t frameSize;
    size_t numFrames;

    std::vector<bool> freeFrames;
    std::unordered_map<void*, std::vector<PageTableEntry>> processPageTables;
    std::unordered_map<void*, size_t> allocationSizes;

    // Backing Store simulation
    std::fstream backingStoreFile;
    int backingStoreNextOffset = 0;

    // Page Fault / Paging Counters
    int numPagedIn = 0;
    int numPagedOut = 0;

    int findFreeFrame();
};