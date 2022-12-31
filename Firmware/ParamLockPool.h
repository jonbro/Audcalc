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
        void ReturnLock(ParamLock *lock);
        uint16_t GetLockPosition(ParamLock *lock);
        ParamLock* GetLock(uint16_t position);
        static ParamLock* NullLock();
        bool validLock(ParamLock *lock);
    private:
        ParamLock locks[LOCKCOUNT];
        ParamLock *freeLocks;
        static ParamLock nullLock;
};

class ParamLockPoolTest
{
    public:
        void RunTest();
};