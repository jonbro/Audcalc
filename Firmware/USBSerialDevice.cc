#include "USBSerialDevice.h"

void USBSerialDevice::Init()
{
    tud_init(BOARD_TUD_RHPORT);
}
void USBSerialDevice::Update()
{
    tud_task();
    // connected() check for DTR bit
    // Most but not all terminal client set this when making connection
    // if ( tud_cdc_connected() )
    {
        // connected and there are data available
        if ( tud_cdc_available() )
        {
            // read data
            char buf[64];
            uint32_t count = tud_cdc_read(buf, sizeof(buf));
            (void) count;
            for(int i=0;i<count;i++)
            {
                if(buf[i] == '/')
                {
                    // do a string compare to see what command we've got
                    char *subStr = buf+i;
                    if(strcmp("/snds", subStr) == 0)
                    {
                        needsSongData = true;
                    }
                    else if(strcmp("/rcvs", subStr) == 0)
                    {
                        prepareRecv = true;
                    }
                }
            }
        }
    }
}

bool USBSerialDevice::NeedsSongData()
{
    return needsSongData;
}
void USBSerialDevice::SendData(const uint8_t* data, size_t count)
{
    // for(int i=0;i<count;i++)
    // {
    //     printf("%02x ", data[i]);
    //     lineBreak++;
    //     if(lineBreak>15)
    //     {
    //         printf("\n");
    //         lineBreak = 0;
    //     }
    // }
    tud_task();
    tud_cdc_write(data, count);
    tud_cdc_write_flush();
}
bool USBSerialDevice::GetData(uint8_t* data, size_t count)
{
    // do a timeout 
    int maxReads = 1000;
    while(count > 0 && maxReads > 0)
    {
        tud_task();
        if (tud_cdc_available())
        {
            uint32_t readCount = tud_cdc_read(data, count);
            count -= readCount;
            // printf("%02x ", data[0]);
            // lineBreak++;
            // if(lineBreak>15)
            // {
            //     printf("\n");
            //     lineBreak = 0;
            // }
        }
        maxReads--;
        sleep_us(2);

    }
    return maxReads>0 || count == 0;
}


// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
  (void) itf;
  (void) rts;

  // TODO set some indicator
  if ( dtr )
  {
    // Terminal connected
  }else
  {
    // Terminal disconnected
  }
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
  (void) itf;
}

// Invoked when device is mounted
void tud_mount_cb(void)
{
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
}