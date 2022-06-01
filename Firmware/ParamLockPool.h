#pragma once
#include <stdio.h>

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
        ParamLockPool()
        {
            freeLocks = locks;
            for(int i=0;i<LOCKCOUNT;i++)
            {
                locks[i].next = i+2;
                if(i==LOCKCOUNT-1)
                {
                    locks[i].next = 0;
                }
            }
        }
        bool GetParamLock(ParamLock **lock);
        void ReturnLock(ParamLock *lock);
        uint16_t GetLockPosition(ParamLock *lock); // returns the array position from the position - saves us 2 bytes from storing everything as pointers (4 bytes)
        ParamLock* GetLock(uint16_t position);
        static ParamLock* NullLock()
        {
            return &nullLock;
        }
    private:
        ParamLock locks[LOCKCOUNT];
        ParamLock *freeLocks;
        static ParamLock nullLock;
};