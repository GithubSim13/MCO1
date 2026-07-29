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

// Demand-paging allocator.
//
// Virtual address space model: each call to allocate(size) hands back an
// opaque "base pointer" that identifies one process's memory block. Every
// subsequent access is expressed as (basePtr, offset) where offset is
// relative to the START of that block (0 = first byte the process owns) —
// this offset IS the "memory_address" the READ/WRITE instructions deal with.
// This is intentionally NOT a 1:1 mapping onto this program's real address
// space (per the spec) - basePtr values are just distinct tokens.
//
// Physical RAM is modeled as `physicalMemory`, divided into `numFrames`
// frames of `frameSize` bytes. A page fault (accessing a page whose
// PageTableEntry::valid is false) triggers, in order:
//   1. look for a free frame
//   2. if none, evict the oldest resident page system-wide (global FIFO)
//      across ALL processes - writing it to the backing store first if dirty
//   3. bring the requested page in (from the backing store if it was
//      evicted before, else zero-filled - "first touch")
// This all happens synchronously and is retried internally until it
// succeeds, so from the instruction's point of view a memory access always
// either (a) blocks until a valid frame is obtained, exactly as described in
// the spec's "restart the instruction" example, or (b) fails outright because
// the address itself is out of bounds (an access violation, not a fault).
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

    // --- Backing store: csopesy-backing-store.txt ---------------------------
    // A genuinely human-readable TEXT file. Each evicted page occupies one
    // fixed-size "slot": a metadata line (which process/page it belongs to,
    // purely for a human reading the file - never parsed back in) followed
    // by a hex-encoded data line (2 hex chars per byte). The file is opened
    // in binary mode purely so fixed byte offsets are exact on every
    // platform (text-mode CRLF translation on Windows would otherwise break
    // our slot-offset math) - the *contents* are still plain ASCII text,
    // openable in any editor at any time, per the spec.
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

    // Ensures the page containing `offset` (within `ptr`'s block) is resident
    // in a physical frame, running the full fault-handling loop described
    // above as many times as necessary. Caller MUST already hold memoryMutex.
    // Returns the resolved PageTableEntry for that page, or nullptr only if
    // `ptr`/`offset` are not a valid (allocated) location at all.
    PageTableEntry* ensureResidentLocked(void* ptr, size_t offset);
};
