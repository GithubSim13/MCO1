#include "PagingMemoryAllocator.h"
#include <cmath>
#include <cstring>

PagingMemoryAllocator::PagingMemoryAllocator(size_t totalRamSize, size_t frameSize)
    : frameSize(frameSize) {
    this->memoryAllocatorType = PAGING;
    this->maximumSize = totalRamSize;
    this->currentAllocatedSize = 0;
    this->numFrames = totalRamSize / frameSize;
    this->freeFrames.resize(numFrames, true); // All physical frames are free initially

    // Initialize Backing Store implementation directly within Memory Manager
    backingStoreFile.open("backing_store.bin", std::ios::in | std::ios::out | std::ios::trunc | std::ios::binary);
}

PagingMemoryAllocator::~PagingMemoryAllocator() {
    if (backingStoreFile.is_open()) backingStoreFile.close();
}

int PagingMemoryAllocator::findFreeFrame() {
    for (size_t i = 0; i < freeFrames.size(); ++i) {
        if (freeFrames[i]) return static_cast<int>(i);
    }
    return -1; // RAM is full, eviction needed
}

void* PagingMemoryAllocator::allocate(size_t size) {
    if (size == 0 || currentAllocatedSize + size > maximumSize) {
        return nullptr; // Out of virtual/physical limits
    }

    size_t numPagesNeeded = std::ceil((double)size / frameSize);
    std::vector<PageTableEntry> pageTable(numPagesNeeded);

    for (size_t i = 0; i < numPagesNeeded; ++i) {
        int freeFrame = findFreeFrame();

        if (freeFrame != -1) {
            // Physical frame available
            freeFrames[freeFrame] = false;
            pageTable[i].frameNum = freeFrame;
            pageTable[i].valid = true;
            numPagedIn++; // Tally page load into physical frame
        } else {
            // Physical RAM full: Reserve space in backing store (demand paging / swap allocation)
            pageTable[i].frameNum = -1;
            pageTable[i].valid = false;
            pageTable[i].backingStoreOffset = backingStoreNextOffset;
            backingStoreNextOffset += frameSize;
            numPagedOut++; // Tally page assigned/paged out to backing store
        }
    }

    // Generate simulated virtual base address pointer using size and count
    void* virtualPtr = reinterpret_cast<void*>(0x100000 + processPageTables.size() * 0x10000);
    processPageTables[virtualPtr] = pageTable;
    allocationSizes[virtualPtr] = size;
    currentAllocatedSize += numPagesNeeded * frameSize;

    return virtualPtr;
}

void PagingMemoryAllocator::deallocate(void* ptr) {
    auto it = processPageTables.find(ptr);
    if (it == processPageTables.end()) return; // Invalid pointer

    const auto& pageTable = it->second;
    for (const auto& pte : pageTable) {
        if (pte.valid && pte.frameNum != -1) {
            freeFrames[pte.frameNum] = true; // Free physical frame
        }
    }

    currentAllocatedSize -= (pageTable.size() * frameSize);
    processPageTables.erase(it);
    allocationSizes.erase(ptr);
}

String PagingMemoryAllocator::visualizeMemory() {
    String viz = "Physical Frames Status:\n";
    for (size_t i = 0; i < freeFrames.size(); ++i) {
        viz += "Frame " + std::to_string(i) + ": " + (freeFrames[i] ? "[ Free ]" : "[ Used ]") + "\n";
    }
    return viz;
}