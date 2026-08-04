#pragma once
#include <string>
#include <cstddef>
#include <cstdint>

typedef std::string String;

// Memory-management contract every allocator strategy (flat, paging, ...) must satisfy.
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

    // Forces page residency without an associated value (legacy MO1-style call).
    virtual void accessMemory(void* ptr, size_t offset, bool isWrite) = 0;

    virtual String visualizeMemory() = 0;

    // Byte-addressable, page-fault-aware value access; false = out of bounds (caller treats as a violation).
    virtual bool readUint16(void* ptr, size_t address, uint16_t& outValue) = 0;
    virtual bool writeUint16(void* ptr, size_t address, uint16_t value) = 0;

    // Pure bounds check, no residency side effects.
    virtual bool isValidAddress(void* ptr, size_t address) = 0;

    // Accounting, used by vmstat / process-smi.
    virtual size_t getTotalMemory() const = 0;
    virtual size_t getUsedMemory() const = 0;
    virtual size_t getFreeMemory() const { return getTotalMemory() - getUsedMemory(); }

    // Paging-specific counters; default to 0 for non-paging allocators.
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
