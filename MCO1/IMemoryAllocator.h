#pragma once
#include <string>
#include <cstddef>
#include <cstdint>

typedef std::string String;

// IMemoryAllocator is the memory-management contract every allocator strategy
// (flat, paging, ...) must satisfy. MO2 extends the MO1 interface with
// byte-addressable uint16 read/write (used by the READ/WRITE instructions and
// by the symbol table) and with accounting getters used by vmstat/process-smi.
class IMemoryAllocator
{
public:
    enum MemoryAllocatorType
    {
        FLAT_MEMORY_ALLOCATOR,
        PAGING,
    };

    virtual ~IMemoryAllocator() = default;

    virtual void* allocate(size_t size) = 0;
    virtual void deallocate(void* ptr) = 0;

    // Legacy MO1-style "touch this page" call - still supported for callers
    // that only want to force residency (and page-fault handling) without an
    // associated value; the value-carrying paths below are preferred.
    virtual void accessMemory(void* ptr, size_t offset, bool isWrite) = 0;

    virtual String visualizeMemory() = 0;

    // --- MO2: byte-addressable, page-fault-aware value access -------------
    // `address` is an offset within the process's OWN allocation (i.e. what
    // the process instructions call "memory_address" — 0 is the start of
    // that process's block, not physical RAM address 0).
    //
    // Both return false if [address, address+1] falls outside the bounds of
    // this allocation - the caller (Process::readMemory/writeMemory) treats a
    // false return as a memory access violation and shuts the process down.
    // On success, residency (and therefore any necessary page-fault
    // handling / eviction / backing-store I/O) has already been resolved
    // before the function returns - the caller never has to retry.
    virtual bool readUint16(void* ptr, size_t address, uint16_t& outValue) = 0;
    virtual bool writeUint16(void* ptr, size_t address, uint16_t value) = 0;

    // Pure bounds check, no residency side effects. Exposed so callers can
    // pre-validate an address (e.g. before deciding how to log an error)
    // without forcing a page fault.
    virtual bool isValidAddress(void* ptr, size_t address) = 0;

    // --- Accounting, used by vmstat / process-smi --------------------------
    virtual size_t getTotalMemory() const = 0;
    virtual size_t getUsedMemory() const = 0;
    virtual size_t getFreeMemory() const { return getTotalMemory() - getUsedMemory(); }

    // Paging-specific counters. Default to 0 so a future non-paging allocator
    // doesn't need to implement paging bookkeeping it has no concept of.
    virtual long getNumPagedIn() const { return 0; }
    virtual long getNumPagedOut() const { return 0; }

protected:
    MemoryAllocatorType memoryAllocatorType;

    struct MemoryBlock {
        size_t start;
        size_t size;

        bool operator<(const MemoryBlock& other) const {
            return start < other.start;
        }
    };

    size_t maximumSize = 0;
    size_t currentAllocatedSize = 0;
};
