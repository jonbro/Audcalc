#define SAMPLES_PER_BLOCK 64
#define BLOCKS_PER_SEND 4
#define SAMPLES_PER_SEND SAMPLES_PER_BLOCK*BLOCKS_PER_SEND

// 6mb * 0x40000 (file position start)
// this includes the total space used up by other files (song data / sequence data)
// this is approximately 90 seconds sample time
#define MAX_RECORDED_SAMPLES_SIZE 6*1024*1024-0x40000