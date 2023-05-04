#pragma once
#include "tusb.h"

class USBSerialDevice
{
public:
    void Init();
    bool NeedsSongData();
    bool PrepareReceiveData() { return prepareRecv; }
    void Update();
    void SendData(const uint8_t* data, size_t count);
    bool GetData(uint8_t* data, size_t count);
    void SignalSongDataComplete()
    {
        needsSongData = false;
    }
    void SignalSongReceiveComplete()
    {
        prepareRecv = false;
    }
    void ResetLineBreak() { lineBreak = 0;}
private:
    int lineBreak;
    char buf[64];
    int  bufOffset = 0;
    bool needsSongData = false;
    bool prepareRecv = false;
    char commandBuffer[64];
};