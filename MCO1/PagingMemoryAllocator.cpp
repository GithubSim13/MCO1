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
    this->physicalMemory.resize(numFrames * frameSize, 0);

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

// FIFO victim selection
int PagingMemoryAllocator::evictVictimFrame() {
    while (!fifoPages.empty()) {
        auto victim = fifoPages.front();
        fifoPages.pop();

        auto it = processPageTables.find(victim.first);
        if (it == processPageTables.end()) continue; // Owner already deallocated, stale entry
        PageTableEntry& pte = it->second[victim.second];
        if (!pte.valid) continue;

        int frame = pte.frameNum;
        if (pte.dirty) {
            // skip write if clean
            if (pte.backingStoreOffset == -1) {
                if (!freeBackingOffsets.empty()) {
                    pte.backingStoreOffset = freeBackingOffsets.back();
                    freeBackingOffsets.pop_back();
                } else {
                    pte.backingStoreOffset = backingStoreNextOffset;
                    backingStoreNextOffset += static_cast<int>(frameSize);
                }
            }
            backingStoreFile.seekp(pte.backingStoreOffset);
            backingStoreFile.write(&physicalMemory[frame * frameSize], frameSize);
            backingStoreFile.flush();
        }
        pte.valid = false;
        pte.frameNum = -1;
        numPagedOut++; // Tally real eviction
        return frame;
    }
    return -1;
}

void* PagingMemoryAllocator::allocate(size_t size) {
    if (size == 0) return nullptr;

    std::lock_guard<std::mutex> lock(memoryMutex);

    size_t numPagesNeeded = static_cast<size_t>(std::ceil((double)size / frameSize));

    // unmapped until first access
    std::vector<PageTableEntry> pageTable(numPagesNeeded);

    // never reuse freed pointers
    void* virtualPtr = reinterpret_cast<void*>(nextVirtualBase);
    nextVirtualBase += 0x10000;
    processPageTables[virtualPtr] = pageTable;
    allocationSizes[virtualPtr] = size;
    currentAllocatedSize += numPagesNeeded * frameSize;

    return virtualPtr;
}

void PagingMemoryAllocator::deallocate(void* ptr) {
    std::lock_guard<std::mutex> lock(memoryMutex);

    auto it = processPageTables.find(ptr);
    if (it == processPageTables.end()) return; // Invalid pointer

    const auto& pageTable = it->second;
    for (const auto& pte : pageTable) {
        if (pte.valid && pte.frameNum != -1) {
            freeFrames[pte.frameNum] = true; // Free physical frame
        }
        if (pte.backingStoreOffset != -1) {
            freeBackingOffsets.push_back(pte.backingStoreOffset); // Reclaim backing store slot
        }
    }

    currentAllocatedSize -= (pageTable.size() * frameSize);
    processPageTables.erase(it);
    allocationSizes.erase(ptr);
}

void PagingMemoryAllocator::accessMemory(void* ptr, size_t offset, bool isWrite) {
    std::lock_guard<std::mutex> lock(memoryMutex);

    auto it = processPageTables.find(ptr);
    if (it == processPageTables.end()) return;

    size_t pageNum = offset / frameSize;
    if (pageNum >= it->second.size()) return; // Access past the allocation
    PageTableEntry& pte = it->second[pageNum];

    if (!pte.valid) {
        // grab free frame or evict oldest
        int frame = findFreeFrame();
        if (frame == -1) frame = evictVictimFrame();
        if (frame == -1) return;

        if (pte.backingStoreOffset != -1) {
            // read back from backing store
            backingStoreFile.seekg(pte.backingStoreOffset);
            backingStoreFile.read(&physicalMemory[frame * frameSize], frameSize);
            numPagedIn++; // Tally page brought back from backing store
        } else {
            // first touch, zero-filled
            std::memset(&physicalMemory[frame * frameSize], 0, frameSize);
        }

        freeFrames[frame] = false;
        pte.frameNum = frame;
        pte.valid = true;
        pte.dirty = false;
        fifoPages.push({ ptr, pageNum });
    }

    if (isWrite) pte.dirty = true;
}

String PagingMemoryAllocator::visualizeMemory() {
    std::lock_guard<std::mutex> lock(memoryMutex);

    String viz = "Physical Frames Status:\n";
    for (size_t i = 0; i < freeFrames.size(); ++i) {
        viz += "Frame " + std::to_string(i) + ": " + (freeFrames[i] ? "[ Free ]" : "[ Used ]") + "\n";
    }
    return viz;
}
