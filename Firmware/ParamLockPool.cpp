#include "ParamLockPool.h"

ParamLock ParamLockPool::nullLock = {0};

bool ParamLockPool::GetParamLock(ParamLock **lock)
{
    if(freeLocks != &nullLock)
    {
        *lock = freeLocks;
        freeLocks = GetLock(freeLocks->next);
        return true;
    }
    return false;
}

void ParamLockPool::ReturnLock(ParamLock *lock)
{
    lock->next = GetLockPosition(freeLocks);
    freeLocks = lock;
}

uint16_t ParamLockPool::GetLockPosition(ParamLock *lock)
{
    if(lock==&nullLock)
        return 0;
    // we are going to 1 index here, so that we can use the zero position to represent no pointer
    return lock-locks+1;
}

ParamLock* ParamLockPool::GetLock(uint16_t position)
{
    if(position == 0 || position-1 >= LOCKCOUNT)
        return &nullLock;
    return locks+(position-1);
}
