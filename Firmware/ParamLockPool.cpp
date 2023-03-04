#include "ParamLockPool.h"
#include <cassert>

ParamLockPool::ParamLockPool()
{
    freeLocks = 0;
    for(int i=0;i<LOCKCOUNT;i++)
    {
        locks[i].next = i+1;
    }
}

bool ParamLockPool::GetFreeParamLock(ParamLock **lock)
{
    if(IsValidLock(freeLocks))
    {
        *lock = GetLock(freeLocks);
        freeLocks = (*lock)->next;
        return true;
    }
    return false;
}

uint16_t ParamLockPool::FreeLockCount()
{
    uint16_t count = 0;
    ParamLock *lock = GetLock(freeLocks);
    while(IsValidLock(lock))
    {
        count++;
        if(lock == GetLock(lock->next))
            return count;
        lock = GetLock(lock->next);
    }
    return count;
}

bool ParamLockPool::IsFreeLock(ParamLock *searchLock)
{
    ParamLock *lock = GetLock(freeLocks);
    while(IsValidLock(lock))
    {
        if(lock == searchLock)
            return true;
        lock = GetLock(lock->next);
    }
    return false;
}

void ParamLockPool::ReturnLockToPool(ParamLock *lock)
{
    if(!IsValidLock(freeLocks))
    {
        lock->next = LOCKCOUNT;
        freeLocks = GetLockPosition(lock);
        return;
    }
    lock->next = freeLocks;
    freeLocks = GetLockPosition(lock);
}

uint16_t ParamLockPool::GetLockPosition(ParamLock *lock)
{
    uint16_t res = lock-locks;
    if(res >= LOCKCOUNT)
        return LOCKCOUNT;
    return res;
}

ParamLock* ParamLockPool::GetLock(uint16_t position)
{
    assert(position < LOCKCOUNT);
    return locks+position;
}

bool ParamLockPool::IsValidLock(ParamLock *lock)
{
    return GetLockPosition(lock) < LOCKCOUNT;
}

bool ParamLockPool::IsValidLock(uint16_t lockPosition)
{
    return lockPosition < LOCKCOUNT;
}

void ParamLockPoolTest::RunTest()
{
    ParamLockPool lockPool = ParamLockPool();
    uint16_t patternLocks = ParamLockPool::InvalidLockPosition();
    ParamLock *lock;

    assert(lockPool.FreeLockCount() == 4096);
    // get a lock
    lockPool.GetFreeParamLock(&lock);
    assert(lockPool.FreeLockCount() == 4095);
    lockPool.ReturnLockToPool(lock);
    assert(lockPool.FreeLockCount() == 4096);
     
    // return it
    // attempt to exhaust the lockpool
    int lockpoolCount = 0;
    while(lockPool.GetFreeParamLock(&lock))
    {
        lock->param = 0;
        lock->step = lockpoolCount;
        lock->value = 0;
        lock->next = patternLocks;
        patternLocks = lockPool.GetLockPosition(lock);
        lockpoolCount++;
    }
    printf("final lockpool count: %i\n", lockpoolCount);
}