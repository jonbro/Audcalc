#include "diagnostics.h"
extern "C" {
    #include "ssd1306.h"
    #include "filesystem.h"
}

bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

void __not_in_flash_func(core2)()
{
    // this needs to be called here, because the multicore launch relies on
    // the fifo being functional
    sleep_ms(20);
    multicore_lockout_victim_init();
}


void Diagnostics::run()
{
    /*
    - general diagnostics plan -
    ============================

    - determine the pullup states on the i2c
    - enable i2c, do i2c scan to see if the codec and screen are there
        - if codec or screen aren't there, then we are going to have issues,
        - enable the ws2812 and read out the error there
        - do a fast flash of the debug indicator
    - enable the buttons
    - provide the following set of diagnostics, behind each button
        - flash memory check
        - flash memory full clear
        - 

    */
    memset(color, 0, 25 * sizeof(uint32_t));
    
    // pullups check
    bool sdaPullup = false;
    bool sclPullup = false;
    hardware_check_i2c_pullups(&sdaPullup, &sclPullup);
    
    // init the rest of the hardware
    hardware_init();
    hardware_disable_power_latch();

    // check to see if the screen and codec are there
    uint8_t rxdata;
    bool foundScreen = i2c_dma_read(hardware_get_i2c(), 0x3C, &rxdata, 1) >= 0;
    bool foundCodec = i2c_dma_read(hardware_get_i2c(), 0x18, &rxdata, 1) >= 0;

    ws2812_init();
    if(!foundScreen)
    {
        // bail on the rest of the diagnostics, because we don't want to do a screen free version
        while(true)
        {
            color[5] = foundScreen?urgb_u32(0, 50, 200):urgb_u32(200, 50, 0);
            color[6] = foundCodec?urgb_u32(0, 50, 200):urgb_u32(200, 50, 0);
            ws2812_setColors(color+5);
            ws2812_trigger();
        }
    }
    {
        // if the user isn't holding the powerkey, 
        // or if holding power & esc then immediately shutdown
        if(!hardware_get_key_state(0,0) || hardware_get_key_state(3, 0))
        {
            // hardware_shutdown();
        }
        // if the user is holding the record key, then reboot in usb mode
        if(hardware_get_key_state(4, 4))
        {
            hardware_reboot_usb();
        }
    }
    ssd1306_t* disp = GetDisplay();
    // setup screen
    disp->external_vcc=false;
    ssd1306_init(disp, 128, 32, 0x3C, I2C_PORT);
    ssd1306_poweroff(disp);
    sleep_ms(3);
    ssd1306_poweron(disp);
    ssd1306_clear(disp);
    ssd1306_contrast(disp, 0x7f); // lower brightness / power requirement
    ssd1306_t *p = disp;
    ssd1306_set_string_color(p, false);
    sprintf(str, "i2c ok: %i %i %i %i", sdaPullup, sclPullup, foundScreen, foundCodec);
    ssd1306_draw_string_gfxfont(p, 3, 12, str, true, 1, 1, &m6x118pt7b);
    ssd1306_show(disp);

    sleep_ms(1500);

    multicore_launch_core1(core2);
    ssd1306_clear(disp);
    sprintf(str, "core2 launch ok");
    ssd1306_draw_string_gfxfont(p, 3, 12, str, true, 1, 1, &m6x118pt7b);
    ssd1306_show(disp);
    sleep_ms(1000);


    while(true)
    {
        memset(color, 0, 25 * sizeof(uint32_t));

        color[0+1*5] = urgb_u32(0, 200, 250);
        color[1+1*5] = urgb_u32(0, 200, 250);

        ssd1306_clear(disp);
        sprintf(str, "lit tests");
        ssd1306_draw_string_gfxfont(p, 3, 12, str, true, 1, 1, &m6x118pt7b);
        ssd1306_show(disp);
        hardware_get_all_key_state(&keyState);
        
        // act on keychanges
        for (size_t i = 0; i < 25; i++)
        {
            uint32_t s = keyState & (1ul<<i);
            int x=i/5;
            int y=i%5;
            if((keyState & (1ul<<i)) != (lastKeyState & (1ul<<i)))
            {
                if(x==0 && y==1)
                {
                    flashQuickClear();
                }
                if(x==1 && y==1)
                {
                    buttonTest();
                }
            }
        }
        lastKeyState = keyState;

        ws2812_setColors(color+5);
        ws2812_trigger();
    }
}
void Diagnostics::buttonTest()
{
    ssd1306_t* disp = GetDisplay();

    bool buttonsPressed[25] = {0};
    memset(&buttonsPressed, 0, sizeof(bool));
    while(true)
    {
        ssd1306_clear(disp);
        sprintf(str, "press all buttons");
        ssd1306_draw_string_gfxfont(disp, 3, 12, str, true, 1, 1, &m6x118pt7b);
        sprintf(str, "%i %i %i %i %i", buttonsPressed[0], buttonsPressed[1], buttonsPressed[2], buttonsPressed[3], buttonsPressed[4]);
        ssd1306_draw_string_gfxfont(disp, 3, 12+17, str, true, 1, 1, &m6x118pt7b);
        ssd1306_show(disp);
        hardware_get_all_key_state(&keyState);
        bool allButtonsPressed = true;
        for (size_t i = 0; i < 25; i++)
        {
            allButtonsPressed &= buttonsPressed[i];
        }
        if(allButtonsPressed)
            return;
        // act on keychanges
        for (size_t i = 0; i < 25; i++)
        {
            uint32_t s = keyState & (1ul<<i);
            int x=i/5;
            int y=i%5;

            if(buttonsPressed[x+y*5])
            {
                color[x+y*5] = urgb_u32(200, 50, 50);
            }
            else
            {
                color[x+y*5] = urgb_u32(0,0,0);
            }
            if((keyState & (1ul<<i)) && !(lastKeyState & (1ul<<i)))
            {
                buttonsPressed[x+y*5] = true;
            }
        }
        lastKeyState = keyState;

        ws2812_setColors(color+5);
        ws2812_trigger();
    }
}
void Diagnostics::flashQuickClear()
{
    char str[64];
    ssd1306_t* disp = GetDisplay();
    ssd1306_clear(disp);
    sprintf(str, "clearing filesystem");
    ssd1306_draw_string_gfxfont(disp, 3, 12, str, true, 1, 1, &m6x118pt7b);
    ffs_filesystemClearState state = ffs_filesystemClearState_init;
    uint8_t count = 0;
    while(DeleteNonEmpty(&state) && !state.error)
    {
        if(count++ > 16)
        {
            ssd1306_clear(disp);
            sprintf(str, "clearing filesystem");
            ssd1306_draw_string_gfxfont(disp, 3, 12, str, true, 1, 1, &m6x118pt7b);
            sprintf(str, "%x", state.offset);
            ssd1306_draw_string_gfxfont(disp, 3, 12+17, str, true, 1, 1, &m6x118pt7b);
            ssd1306_show(disp);
            sleep_us(100);
            count = 0;
        }
    }
    if(!state.error)
    {
        ssd1306_clear(disp);
        sprintf(str, "clear ok");
        ssd1306_draw_string_gfxfont(disp, 3, 12, str, true, 1, 1, &m6x118pt7b);
        ssd1306_show(disp);
        sleep_ms(1000);
    }
    else
    {
        ssd1306_clear(disp);
        sprintf(str, "clear failed:");
        ssd1306_draw_string_gfxfont(disp, 3, 12, str, true, 1, 1, &m6x118pt7b);
        sprintf(str, "addr: %x", state.offset-0x1000);
        ssd1306_draw_string_gfxfont(disp, 3, 12+17, str, true, 1, 1, &m6x118pt7b);
        ssd1306_show(disp);
        sleep_ms(3000);
    }
}