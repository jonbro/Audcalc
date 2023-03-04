#pragma once
#include <stdio.h>
#include <string.h>

struct ParamLock
{
    uint8_t step;
    uint8_t param;
    uint8_t value;
    uint16_t next;
};

#define LOCKCOUNT 16*256
class ParamLockPool
{
    public:
        ParamLockPool();
        bool GetFreeParamLock(ParamLock **lock);
        bool IsFreeLock(ParamLock *searchLock);
        void ReturnLockToPool(ParamLock *lock);
        uint16_t GetLockPosition(ParamLock *lock);
        ParamLock* GetLock(uint16_t position);
        bool IsValidLock(ParamLock *lock);
        bool IsValidLock(uint16_t lockPosition);
        uint16_t FreeLockCount();
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