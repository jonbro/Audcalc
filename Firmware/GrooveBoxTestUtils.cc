#include "GrooveBox.h"
#include <string.h>

int GrooveBox::GetLostLockCount()
{
    if(patterns[0].lockPool.FreeLockCount()<10)
    {
        printf("running low on locks\n");
    }
    int lostLockCount = 0;
    for(int l=0;l<16*256;l++)
    {
        ParamLock *searchingForLock = patterns[0].lockPool.GetLock(l);
        bool foundLock = false;
        for(int i=0;i<16;i++)
        {
            for(int p=0;p<16;p++)
            {
                int lockCount = 0;
                ParamLock *lock = patterns[0].lockPool.GetLock(patterns[i].GetVoiceData()->locksForPattern[p]);
                while(patterns[0].lockPool.IsValidLock(lock))
                {
                    if(lock == searchingForLock){
                        foundLock = true;
                        break;
                    }
                    if(lock == patterns[0].lockPool.GetLock(lock->next))
                    {
                        break;
                    }
                    lock = patterns[0].lockPool.GetLock(lock->next);
                }
            }
        }
        if(!foundLock)
        {
            // before we attempt to free this lock, confirm its not on the freelock list
            if(!patterns[0].lockPool.IsFreeLock(searchingForLock))
            {
                lostLockCount++;
            }
        }
    }
    return lostLockCount;
}