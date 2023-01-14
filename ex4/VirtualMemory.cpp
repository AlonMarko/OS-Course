//
// Created by idanorg on 21/05/2022.
//


#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <cmath>

#define ZERO 0

bool
treeTraversal(word_t frameIdx, uint64_t prevFrame, uint64_t *lastPageIdxPrevFrame, int depth, word_t *emptyFrameIdx,
              word_t *maxFrame,
              uint64_t *maxRoute,
              uint64_t *maxRoutePage, word_t *maxRouteFrame, word_t rootIdx,
              uint64_t rootFrame,
              uint64_t pageIdx);

void clearFrameContent(word_t frame);

uint64_t getOff(uint64_t virtualAddress);

word_t pageFault(word_t frame, uint64_t phyisAddr, uint64_t page);

uint64_t getPhysicalFromVirtual(uint64_t virtualAddress);

uint64_t distCalc(uint64_t frame, uint64_t idx);

uint64_t distCalc(uint64_t frame, uint64_t idx) {
    uint64_t absDist = frame > idx ? frame - idx : idx - frame;
    uint64_t finalDist = absDist > NUM_PAGES - absDist ? NUM_PAGES - absDist : absDist;
    return finalDist;
}


void VMinitialize() {
    for (uint64_t i = 0; i < PAGE_SIZE; i++) {
        PMwrite(i, 0);
    }
}

void clearFrameContent(word_t frame) {
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMwrite(frame * PAGE_SIZE + i, ZERO);
    }
}


int VMread(uint64_t virtualAddress, word_t *value) {
//    uint64_t line = virtualAddress & (uint64_t) (pow(2, OFFSET_WIDTH) - 1);
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) {
        return 0;
    }
    if (value == nullptr) {
        return 0;
    }
    uint64_t physical_address = getPhysicalFromVirtual(virtualAddress);
    if (physical_address == 0) {
        return 0;
    }
//    uint64_t addr = (physical_address * PAGE_SIZE) + line;
    PMread(physical_address, value);
    return 1;
}

int VMwrite(uint64_t virtualAddress, word_t value) {
//    uint64_t line = virtualAddress & (uint64_t) (pow(2, OFFSET_WIDTH) - 1);
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) {
        return 0;
    }
    uint64_t physical_address = getPhysicalFromVirtual(virtualAddress);
    if (physical_address == 0) {
        return 0;
    }
//    uint64_t addr = (physical_address * PAGE_SIZE) + line;
    PMwrite(physical_address, value);
    return 1;
}

uint64_t getOff(uint64_t virtualAddress) {
    return virtualAddress & ((uint64_t) (pow(2, OFFSET_WIDTH) - 1));
}

bool
treeTraversal(word_t frameIdx, uint64_t prevFrame, uint64_t *lastPageIdxPrevFrame, int depth, word_t *emptyFrameIdx,
              int *maxFrame, uint64_t *maxRoute,
              uint64_t *maxRoutePage, word_t *maxRouteFrame, word_t rootIdx,
              uint64_t rootFrame,
              uint64_t pageIdx) {
    if (frameIdx > *maxFrame) {
        *maxFrame = frameIdx;
    }
    if (depth == TABLES_DEPTH) {
        uint64_t dist = distCalc(rootFrame, pageIdx);
        if (dist > *maxRoute) {
            *maxRoute = dist;
            *maxRoutePage = pageIdx;
            *maxRouteFrame = frameIdx;
            *lastPageIdxPrevFrame = prevFrame;
        }
        return false;
    }
    uint64_t addr = frameIdx * PAGE_SIZE;
    word_t next = 0;
    word_t pageZero = 0;
    for (int i = 0; i < PAGE_SIZE; ++i) {
        PMread(addr + i, &next);
        if (next != ZERO) {
            if ((treeTraversal(next, addr + i, lastPageIdxPrevFrame, depth + 1, emptyFrameIdx, maxFrame, maxRoute,
                               maxRoutePage, maxRouteFrame, rootIdx, rootFrame,
                               ((pageIdx << OFFSET_WIDTH) + i))) && (next != rootIdx)) {
                PMwrite(addr + i, 0);
                *emptyFrameIdx = next;
            }
        } else {
            pageZero++;
        }
    }
    return PAGE_SIZE > pageZero ? false : true;
}


word_t pageFault(word_t frame, uint64_t phyisAddr, uint64_t page) {
    word_t evictFrameIdx = 0;
    word_t maxFrameIdx = 0;
    word_t maxRouteFrame = 0;
    uint64_t maxRoutePage = 0;
    uint64_t lastPageIdxPrevFrame = 0;
    uint64_t dist = 0;
    word_t idx;
    treeTraversal(ZERO, lastPageIdxPrevFrame, &lastPageIdxPrevFrame, ZERO, &evictFrameIdx, &maxFrameIdx, &dist,
                  &maxRoutePage, &maxRouteFrame, frame, page, ZERO);
    if (evictFrameIdx != ZERO) {
        idx = evictFrameIdx;
    } else if (maxFrameIdx < NUM_FRAMES - 1) {
        idx = maxFrameIdx + 1;
    } else {
        idx = maxRouteFrame;
        PMevict(maxRouteFrame, maxRoutePage);
        PMwrite(lastPageIdxPrevFrame, ZERO);
    }
    clearFrameContent(idx);
    PMwrite(phyisAddr, idx);
    return idx;
}

uint64_t getPhysicalFromVirtual(uint64_t virtualAddress) {
    word_t addr1 = 0;
    word_t addr2 = 0;
    int bit = TABLES_DEPTH * OFFSET_WIDTH;
    uint64_t frame = virtualAddress >> OFFSET_WIDTH;
    uint64_t currPhysicalAddress;
    while (bit > 0) {
        uint64_t offSet = getOff(virtualAddress >> bit);
        currPhysicalAddress = addr2 * PAGE_SIZE + offSet;
        PMread(currPhysicalAddress, &addr1);
        if (addr1 == 0) {
            addr1 = pageFault(addr2, currPhysicalAddress, frame);
            if (abs(bit - OFFSET_WIDTH) == 0) { // last bit - single restore.
                PMrestore(addr1, frame);
            }
        }
        bit -= OFFSET_WIDTH;
        addr2 = addr1;
    }
    uint64_t offSet = getOff(virtualAddress);
    return addr1 * PAGE_SIZE + offSet;
}