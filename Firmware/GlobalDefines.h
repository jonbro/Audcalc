// codec sample rate, matched by the audio lookup tables in audio/resources
#define AUDIO_SAMPLE_RATE 32000
#define SAMPLES_PER_BLOCK 64
#define BLOCKS_PER_SEND 4
#define SAMPLES_PER_SEND SAMPLES_PER_BLOCK*BLOCKS_PER_SEND

// 6mb * 0x40000 (file position start)
// this includes the total space used up by other files (song data / sequence data)
// this is approximately 90 seconds sample time
#define MAX_RECORDED_SAMPLES_SIZE 6*1024*1024-0x40000

// where the reverb sits when the unit boots, and what songs saved before these
// controls existed fall back to. measured on a model of the tank: ~4.5s low /
// ~2.6s high RT60, which is a long-ish hall rather than the ~15s low RT60 the
// fixed 0.87 feedback used to give
#define VERB_DEFAULT_FEEDBACK 150
#define VERB_DEFAULT_DAMPING 128
#define VERB_DEFAULT_MODULATION 43
