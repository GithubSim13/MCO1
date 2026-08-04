#pragma once
#include "IMemoryAllocator.h"
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <string>
#include <fstream>

struct PageTableEntry {
    int  frameNum = -1;   // Physical frame number (-1 if unmapped/swapped out)
    bool valid    = false; // Is page loaded in physical RAM right now?
    bool dirty    = false; // Has page been modified since it was last loaded?
    long backingSlot = -1; // Slot index in the backing store file (-1 = never evicted)
};

// Demand-paging allocator: allocate() hands back an opaque base pointer; page faults resolve via free-frame-or-FIFO-evict, synchronously, until valid.
class PagingMemoryAllocator : public IMemoryAllocator {
public:
    PagingMemoryAllocator(size_t totalRamSize, size_t frameSize);
    ~PagingMemoryAllocator() override;

    void* allocate(size_t size) override;
    void deallocate(void* ptr) override;
    void accessMemory(void* ptr, size_t offset, bool isWrite) override;
    String visualizeMemory() override;

    bool readUint16(void* ptr, size_t address, uint16_t& outValue) override;
    bool writeUint16(void* ptr, size_t address, uint16_t value) override;
    bool isValidAddress(void* ptr, size_t address) override;

    size_t getTotalMemory() const override;
    size_t getUsedMemory() const override;

    long getNumPagedIn() const override  { return numPagedIn; }
    long getNumPagedOut() const override { return numPagedOut; }

    size_t getNumFrames() const { return numFrames; }
    size_t getFrameSize() const { return frameSize; }
    size_t getNumFreeFrames() const;

private:
    size_t frameSize;
    size_t numFrames;

    std::vector<bool> freeFrames;
    std::vector<char> physicalMemory; // Actual frame contents, frameSize bytes per frame
    std::unordered_map<void*, std::vector<PageTableEntry>> processPageTables;
    std::unordered_map<void*, size_t> allocationSizes;

    // Global FIFO order of resident (ptr, pageNum) pairs, for victim selection.
    std::queue<std::pair<void*, size_t>> fifoPages;

    // csopesy-backing-store.txt: human-readable, fixed-width slots (metadata + hex data).
    std::fstream backingStoreFile;
    static const size_t METADATA_WIDTH = 48; // fixed width of the metadata line, sans '\n'
    static const size_t HEADER_SIZE = 128;   // fixed width of the one-time file header
    long backingStoreNextSlot = 0;
    std::vector<long> freeBackingSlots; // reclaimed slots, reused before growing the file

    size_t slotRecordSize() const { return METADATA_WIDTH + 1 + (frameSize * 2) + 1; }
    void writeHeader();
    void writeSlot(long slot, const void* processPtr, size_t pageNum, const char* frameData);
    void readSlot(long slot, char* outFrameData);

    // Page Fault / Paging Counters (cumulative, never reset - vmstat reports these directly)
    long numPagedIn = 0;
    long numPagedOut = 0;

    size_t nextVirtualBase = 0x10000; // arbitrary distinct token generator for basePtr

    mutable std::mutex memoryMutex;

    int findFreeFrame();
    int evictVictimFrame();

    // Ensures the page at (ptr, offset) is resident; caller must hold memoryMutex.
    PageTableEntry* ensureResidentLocked(void* ptr, size_t offset);
};
