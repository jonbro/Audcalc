#include "ParamLockPool.h"
#include <cassert>


bool ParamLockPoolInternal_encode_locks(pb_ostream_t *ostream, const pb_field_t *field, void * const *arg)
{
    ParamLock* lockPool = *(ParamLock**)arg;

    // encode all locks
    for (int i = 0; i < LOCKCOUNT; i++)
    {
        if (!pb_encode_tag_for_field(ostream, field))
        {
            const char * error = PB_GET_ERROR(ostream);
            return false;
        }
        ParamLockPoolInternal_ParamLock lock = ParamLockPoolInternal_ParamLock_init_zero;

        lock.next = lockPool[i].next;
        lock.step = lockPool[i].step;
        lock.param = lockPool[i].param;
        lock.value = lockPool[i].value;
        lock.index = i;
        if (!pb_encode_submessage(ostream, ParamLockPoolInternal_ParamLock_fields, &lock))
        {
            const char * error = PB_GET_ERROR(ostream);
            printf("ParamLockPoolInternal_encode_locks error: %s", error);
            return false;
        }
    }
    return true;
}
int decodeCount = 0;
bool ParamLockPoolInternal_decode_locks(pb_istream_t *stream, const pb_field_iter_t *field, void **arg)
{
    ParamLock* lockPool = (*(ParamLock**)arg);
    ParamLockPoolInternal_ParamLock msgLock = ParamLockPoolInternal_ParamLock_init_zero;
    if (!pb_decode(stream, ParamLockPoolInternal_ParamLock_fields, &msgLock))
        return false;
    ParamLock *lock = lockPool+msgLock.index;
    lock->next = msgLock.next;
    lock->step = msgLock.step;
    lock->param = msgLock.param;
    lock->value = msgLock.value;
    decodeCount++;
    return true;
}

ParamLockPool::ParamLockPool()
{
    Init();
}
void ParamLockPool::Init()
{
    freeLocks = 0;
    for(int i=0;i<LOCKCOUNT;i++)
    {
        locks[i].next = i+1;
    }
}
void ParamLockPool::Serialize(pb_ostream_t *s)
{
    ParamLockPoolInternal lockPoolEncoder = ParamLockPoolInternal_init_zero;
    lockPoolEncoder.freeLocks = freeLocks;
    lockPoolEncoder.locks.funcs.encode = &ParamLockPoolInternal_encode_locks;
    lockPoolEncoder.locks.arg = locks;
    pb_encode_ex(s, ParamLockPoolInternal_fields, &lockPoolEncoder, PB_ENCODE_DELIMITED);
}

void ParamLockPool::Deserialize(pb_istream_t *s)
{
    ParamLockPoolInternal lockPoolDecoder = ParamLockPoolInternal_init_zero;
    lockPoolDecoder.locks.funcs.decode = &ParamLockPoolInternal_decode_locks;
    lockPoolDecoder.locks.arg = locks;
    pb_decode_ex(s, ParamLockPoolInternal_fields, &lockPoolDecoder, PB_ENCODE_DELIMITED);
    freeLocks = lockPoolDecoder.freeLocks;
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