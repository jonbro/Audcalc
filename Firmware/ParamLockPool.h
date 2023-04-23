#pragma once
#include <stdio.h>
#include <string.h>
#include "ParamLockPoolInternal.pb.h"
#define LOCKCOUNT 16*256
#include <pb_encode.h>
#include <pb_decode.h>

struct ParamLock
{
    uint8_t step;
    uint8_t param;
    uint8_t value;
    uint16_t next;
};

bool ParamLockPoolInternal_encode_locks(pb_ostream_t *ostream, const pb_field_t *field, void * const *arg);
bool ParamLockPoolInternal_decode_locks(pb_istream_t *stream, const pb_field_iter_t *field, void **arg);

class ParamLockPool
{
    public:
        ParamLockPool();
        void Init();
        bool GetFreeParamLock(ParamLock **lock);
        bool IsFreeLock(ParamLock *searchLock);
        void ReturnLockToPool(ParamLock *lock);
        uint16_t GetLockPosition(ParamLock *lock);
        ParamLock* GetLock(uint16_t position);
        bool IsValidLock(ParamLock *lock);
        bool IsValidLock(uint16_t lockPosition);
        uint16_t FreeLockCount();

        void Serialize(pb_ostream_t *s);
        void Deserialize(pb_istream_t *s);

        static uint16_t InvalidLockPosition() { return LOCKCOUNT; }
    private:
        ParamLock locks[LOCKCOUNT];
        uint16_t freeLocks;
};

class ParamLockPoolTest
{
    public:
        void RunTest();
};