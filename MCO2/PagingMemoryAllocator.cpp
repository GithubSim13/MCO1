#include "PagingMemoryAllocator.h"
#include <cmath>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace {

const char* HEX_DIGITS = "0123456789ABCDEF";

String bytesToHex(const char* data, size_t len) {
    String out;
    out.resize(len * 2);
    for (size_t i = 0; i < len; ++i) {
        unsigned char b = static_cast<unsigned char>(data[i]);
        out[i * 2]     = HEX_DIGITS[(b >> 4) & 0xF];
        out[i * 2 + 1] = HEX_DIGITS[b & 0xF];
    }
    return out;
}

int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0; // malformed input in the backing store: fail safe to 0 rather than throw
}

void hexToBytes(const String& hex, char* out, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        int hi = hexNibble(hex[i * 2]);
        int lo = hexNibble(hex[i * 2 + 1]);
        out[i] = static_cast<char>((hi << 4) | lo);
    }
}

} // namespace

PagingMemoryAllocator::PagingMemoryAllocator(size_t totalRamSize, size_t frameSize)
    : frameSize(frameSize) {
    this->memoryAllocatorType   = PAGING;
    this->maximumSize           = totalRamSize;
    this->currentAllocatedSize  = 0;
    this->numFrames             = totalRamSize / frameSize;
    this->freeFrames.resize(numFrames, true);
    this->physicalMemory.resize(numFrames * frameSize, 0);

    // Fresh backing store every run - stale slot indices wouldn't mean anything now.
    backingStoreFile.open("csopesy-backing-store.txt",
        std::ios::in | std::ios::out | std::ios::trunc | std::ios::binary);
    writeHeader();
}

PagingMemoryAllocator::~PagingMemoryAllocator() {
    if (backingStoreFile.is_open()) backingStoreFile.close();
}

void PagingMemoryAllocator::writeHeader() {
    std::ostringstream oss;
    oss << "CSOPESY BACKING STORE - frame size = " << frameSize << " bytes\n"
        << "Format: per evicted page, 1 metadata line + 1 hex data line.\n";
    String header = oss.str();
    if (header.size() > HEADER_SIZE) header.resize(HEADER_SIZE);
    else header.resize(HEADER_SIZE, ' ');
    header[HEADER_SIZE - 1] = '\n';
    backingStoreFile.seekp(0);
    backingStoreFile.write(header.data(), HEADER_SIZE);
    backingStoreFile.flush();
}

void PagingMemoryAllocator::writeSlot(long slot, const void* processPtr, size_t pageNum, const char* frameData) {
    std::ostringstream meta;
    meta << "owner=" << processPtr << " page=" << pageNum;
    String metaLine = meta.str();
    if (metaLine.size() > METADATA_WIDTH) metaLine.resize(METADATA_WIDTH);
    else metaLine.resize(METADATA_WIDTH, ' ');

    String dataLine = bytesToHex(frameData, frameSize);

    std::streamoff base = static_cast<std::streamoff>(HEADER_SIZE) +
                          static_cast<std::streamoff>(slot) * static_cast<std::streamoff>(slotRecordSize());

    backingStoreFile.seekp(base);
    backingStoreFile.write(metaLine.data(), METADATA_WIDTH);
    backingStoreFile.write("\n", 1);
    backingStoreFile.write(dataLine.data(), dataLine.size());
    backingStoreFile.write("\n", 1);
    backingStoreFile.flush();
}

void PagingMemoryAllocator::readSlot(long slot, char* outFrameData) {
    std::streamoff base = static_cast<std::streamoff>(HEADER_SIZE) +
                          static_cast<std::streamoff>(slot) * static_cast<std::streamoff>(slotRecordSize());
    std::streamoff dataOffset = base + static_cast<std::streamoff>(METADATA_WIDTH) + 1;

    String hexBuf(frameSize * 2, '0');
    backingStoreFile.clear(); // clear eof/fail bits from any prior read
    backingStoreFile.seekg(dataOffset);
    backingStoreFile.read(&hexBuf[0], hexBuf.size());
    hexToBytes(hexBuf, outFrameData, frameSize);
}

int PagingMemoryAllocator::findFreeFrame() {
    for (size_t i = 0; i < freeFrames.size(); ++i) {
        if (freeFrames[i]) return static_cast<int>(i);
    }
    return -1;
}

// Global FIFO victim selection across all processes' resident pages.
int PagingMemoryAllocator::evictVictimFrame() {
    while (!fifoPages.empty()) {
        auto victim = fifoPages.front();
        fifoPages.pop();

        auto it = processPageTables.find(victim.first);
        if (it == processPageTables.end()) continue; // owner already deallocated
        if (victim.second >= it->second.size()) continue; // stale page index
        PageTableEntry& pte = it->second[victim.second];
        if (!pte.valid) continue; // already evicted/replaced

        int frame = pte.frameNum;
        if (pte.dirty) {
            if (pte.backingSlot == -1) {
                if (!freeBackingSlots.empty()) {
                    pte.backingSlot = freeBackingSlots.back();
                    freeBackingSlots.pop_back();
                } else {
                    pte.backingSlot = backingStoreNextSlot++;
                }
            }
            writeSlot(pte.backingSlot, victim.first, victim.second, &physicalMemory[frame * frameSize]);
        }
        pte.valid    = false;
        pte.frameNum = -1;
        numPagedOut++;
        return frame;
    }
    return -1;
}

void* PagingMemoryAllocator::allocate(size_t size) {
    if (size == 0) return nullptr;

    std::lock_guard<std::mutex> lock(memoryMutex);

    size_t numPagesNeeded = static_cast<size_t>(std::ceil(static_cast<double>(size) / frameSize));

    std::vector<PageTableEntry> pageTable(numPagesNeeded); // all unmapped until first touch

    void* virtualPtr = reinterpret_cast<void*>(nextVirtualBase);
    nextVirtualBase += 0x10000; // never reuse a token, even after deallocate()

    processPageTables[virtualPtr] = std::move(pageTable);
    allocationSizes[virtualPtr]   = size;
    currentAllocatedSize         += numPagesNeeded * frameSize;

    return virtualPtr;
}

void PagingMemoryAllocator::deallocate(void* ptr) {
    std::lock_guard<std::mutex> lock(memoryMutex);

    auto it = processPageTables.find(ptr);
    if (it == processPageTables.end()) return;

    for (const auto& pte : it->second) {
        if (pte.valid && pte.frameNum != -1) {
            freeFrames[pte.frameNum] = true;
        }
        if (pte.backingSlot != -1) {
            freeBackingSlots.push_back(pte.backingSlot);
        }
    }

    currentAllocatedSize -= (it->second.size() * frameSize);
    processPageTables.erase(it);
    allocationSizes.erase(ptr);
    // fifoPages entries for this ptr are skipped lazily in evictVictimFrame().
}

PageTableEntry* PagingMemoryAllocator::ensureResidentLocked(void* ptr, size_t offset) {
    auto it = processPageTables.find(ptr);
    if (it == processPageTables.end()) return nullptr;

    size_t pageNum = offset / frameSize;
    if (pageNum >= it->second.size()) return nullptr; // out of this allocation's bounds

    PageTableEntry& pte = it->second[pageNum];
    if (pte.valid) return &pte;

    // Page fault: retry find-free-or-evict until a valid frame is obtained.
    int frame = -1;
    for (size_t attempts = 0; attempts <= numFrames + 1; ++attempts) {
        frame = findFreeFrame();
        if (frame == -1) frame = evictVictimFrame();
        if (frame != -1) break;
    }
    if (frame == -1) {
        return nullptr; // should be unreachable
    }

    if (pte.backingSlot != -1) {
        readSlot(pte.backingSlot, &physicalMemory[frame * frameSize]);
        numPagedIn++;
    } else {
        std::memset(&physicalMemory[frame * frameSize], 0, frameSize); // first touch: zero-filled
    }

    freeFrames[frame] = false;
    pte.frameNum = frame;
    pte.valid    = true;
    pte.dirty    = false;
    fifoPages.push({ ptr, pageNum });

    return &pte;
}

void PagingMemoryAllocator::accessMemory(void* ptr, size_t offset, bool isWrite) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    PageTableEntry* pte = ensureResidentLocked(ptr, offset);
    if (pte != nullptr && isWrite) pte->dirty = true;
}

bool PagingMemoryAllocator::isValidAddress(void* ptr, size_t address) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    auto it = allocationSizes.find(ptr);
    if (it == allocationSizes.end()) return false;
    return address < it->second;
}

bool PagingMemoryAllocator::readUint16(void* ptr, size_t address, uint16_t& outValue) {
    std::lock_guard<std::mutex> lock(memoryMutex);

    auto sizeIt = allocationSizes.find(ptr);
    if (sizeIt == allocationSizes.end()) return false;
    if (address + 1 >= sizeIt->second || address + 1 < address /* overflow guard */) return false;

    PageTableEntry* pteLo = ensureResidentLocked(ptr, address);
    if (pteLo == nullptr) return false;
    unsigned char lo = static_cast<unsigned char>(physicalMemory[pteLo->frameNum * frameSize + (address % frameSize)]);

    size_t addrHi = address + 1;
    PageTableEntry* pteHi = ensureResidentLocked(ptr, addrHi);
    if (pteHi == nullptr) return false;
    unsigned char hi = static_cast<unsigned char>(physicalMemory[pteHi->frameNum * frameSize + (addrHi % frameSize)]);

    outValue = static_cast<uint16_t>(lo | (hi << 8));
    return true;
}

bool PagingMemoryAllocator::writeUint16(void* ptr, size_t address, uint16_t value) {
    std::lock_guard<std::mutex> lock(memoryMutex);

    auto sizeIt = allocationSizes.find(ptr);
    if (sizeIt == allocationSizes.end()) return false;
    if (address + 1 >= sizeIt->second || address + 1 < address) return false;

    unsigned char lo = static_cast<unsigned char>(value & 0xFF);
    unsigned char hi = static_cast<unsigned char>((value >> 8) & 0xFF);

    PageTableEntry* pteLo = ensureResidentLocked(ptr, address);
    if (pteLo == nullptr) return false;
    physicalMemory[pteLo->frameNum * frameSize + (address % frameSize)] = static_cast<char>(lo);
    pteLo->dirty = true;

    size_t addrHi = address + 1;
    PageTableEntry* pteHi = ensureResidentLocked(ptr, addrHi);
    if (pteHi == nullptr) return false;
    physicalMemory[pteHi->frameNum * frameSize + (addrHi % frameSize)] = static_cast<char>(hi);
    pteHi->dirty = true;

    return true;
}

size_t PagingMemoryAllocator::getNumFreeFrames() const {
    std::lock_guard<std::mutex> lock(memoryMutex);
    size_t free = 0;
    for (bool f : freeFrames) if (f) free++;
    return free;
}

size_t PagingMemoryAllocator::getTotalMemory() const {
    return maximumSize;
}

size_t PagingMemoryAllocator::getUsedMemory() const {
    return (numFrames - getNumFreeFrames()) * frameSize;
}

String PagingMemoryAllocator::visualizeMemory() {
    std::lock_guard<std::mutex> lock(memoryMutex);

    std::ostringstream oss;
    oss << "Physical Frames Status (" << numFrames << " frames x " << frameSize << " bytes):\n";
    for (size_t i = 0; i < freeFrames.size(); ++i) {
        oss << "Frame " << i << ": " << (freeFrames[i] ? "[ Free ]" : "[ Used ]") << "\n";
    }
    return oss.str();
}
