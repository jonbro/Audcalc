#include <stdio.h>

// bitmask for tracking free blocks
uint32_t freeblocks_bitmask[32];

// holds information about the file for each sample
struct SampleFile
{
    // lets simplify for now, we might just be able to get away with an array (until this proves to be memory inefficient)
    // the issue is if we record an entire sample into a single spot, we will really need a way to point to the block offsets more cheaply in ram
    // or potentially use flash, but I don't want to think about the difficulties there.
    uint32_t sampleBlocks[32];
};