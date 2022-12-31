#include "ParamLockPool.h"

ParamLock ParamLockPool::nullLock = {0};

ParamLockPool::ParamLockPool()
{
    freeLocks = locks;
    for(int i=0;i<LOCKCOUNT;i++)
    {
        locks[i].next = i+1;
    }
}

bool ParamLockPool::GetFreeParamLock(ParamLock **lock)
{
    if(freeLocks != &nullLock)
    {
        *lock = freeLocks;
        freeLocks = GetLock(freeLocks->next);
        return true;
    }
    return false;
}

void ParamLockPool::FreeLock(ParamLock *lock)
{
    lock->next = GetLockPosition(freeLocks);
    freeLocks = lock;
}

uint16_t ParamLockPool::GetLockPosition(ParamLock *lock)
{
    if(lock==&nullLock)
        return LOCKCOUNT; // this is effectively returning the nulllock
    return lock-locks;
}

ParamLock* ParamLockPool::GetLock(uint16_t position)
{
    if(position >= LOCKCOUNT)
        return &nullLock;
    return locks+position;
}

ParamLock* ParamLockPool::NullLock()
{
    return &nullLock;
}
bool ParamLockPool::validLock(ParamLock *lock)
{
    return GetLockPosition(lock) < LOCKCOUNT;
}

void ParamLockPoolTest::RunTest()
{
    ParamLockPool lockPool = ParamLockPool();
    ParamLock *patternLocks = ParamLockPool::NullLock();
    ParamLock *lock = ParamLockPool::NullLock();
    // attempt to exhaust the lockpool
    int lockpoolCount = 0;
    while(lockPool.GetFreeParamLock(&lock))
    {
        lock->param = 0;
        lock->step = lockpoolCount;
        lock->value = 0;
        lock->next = lockPool.GetLockPosition(patternLocks);
        patternLocks = lock;
        lockpoolCount++;
    }
    printf("final lockpool count: %i\n", lockpoolCount);
}