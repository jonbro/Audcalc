#pragma once
#include "pico/multicore.h"
#include "pico/util/queue.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int renderInstrument;
    uint8_t *sync_buffer;
    int16_t *workBuffer;
} queue_entry_t;

typedef struct
{
    bool renderInstrumentComplete;
} queue_entry_complete_t;

extern queue_t signal_queue;
extern queue_t complete_queue;
extern queue_t renderCompleteQueue;
#ifdef __cplusplus
}
#endif
