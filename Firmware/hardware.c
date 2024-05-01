#include "hardware.h"
#include "ssd1306.h"

#define LINE_IN_DETECT 24
#define HEADPHONE_DETECT 16
#define row_pin_base 11
#define col_pin_base 6
#define BLINK_PIN_LED 25
#define USB_POWER_SENSE 29
#define SUBSYSTEM_RESET_PIN 21

// I2C defines
#define I2C_SDA 2
#define I2C_SCL 3

static bool last_amp_state;
static uint8_t battery_level;

i2c_dma_t* i2c_dma;

void hardware_init()
{
    // give all the caps some time to warm up
    sleep_ms(100);
    // gpio_init(BLINK_PIN_LED);
    // gpio_set_dir(BLINK_PIN_LED, GPIO_OUT);
    // gpio_put(BLINK_PIN_LED, true);
    set_sys_clock_khz(200000, true);

    adc_init();
    adc_gpio_init(26);

    i2c_init(I2C_PORT, 400*1000);

    gpio_init(23);
    gpio_set_dir(23, GPIO_IN);
    gpio_set_pulls(23, true, false);
}
void hardware_check_i2c_pullups(bool *scl, bool *sda)
{
    gpio_set_function(I2C_SDA, GPIO_FUNC_NULL);
    gpio_set_function(I2C_SCL, GPIO_FUNC_NULL);
    gpio_set_pulls(I2C_SDA, false, false);
    gpio_set_pulls(I2C_SCL, false, false);

    gpio_set_dir(I2C_SDA, GPIO_IN);
    gpio_set_dir(I2C_SCL, GPIO_IN);
    *scl = gpio_get(I2C_SCL);
    *sda = gpio_get(I2C_SDA);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    i2c_init(I2C_PORT, 400*1000);

}
void hardware_reboot_usb()
{
    reset_usb_boot(1<<BLINK_PIN_LED, 0);
}

i2c_dma_t* hardware_get_i2c()
{
    return i2c_dma;
}

bool hardware_get_key_state(uint8_t x, uint8_t y)
{
    if(x==0&&y==0)
        return !gpio_get(23);
    gpio_put_masked(0x7c0, 1<<(col_pin_base+x));
    sleep_us(2);
    return gpio_get(row_pin_base+y);
}

void hardware_get_all_key_state(uint32_t *keyState)
{
    gpio_put(col_pin_base, true);
    // read keys
    for (size_t i = 0; i < 5; i++)
    {
        // the mask here are the gpio pins for the colums
        gpio_put_masked(0x7c0, 1<<(col_pin_base+i));
        sleep_us(1);
        for (size_t j = 0; j < 5; j++)
        {
            int index = (i*5+j);
            bool keyVal = index==0?!gpio_get(23):gpio_get(j+row_pin_base);
            if(keyVal)
            {
                *keyState |= 1ul << index;
            }
            else
            {
                *keyState &= ~(1ul << index);
            }
        }
    }
}

uint8_t hardware_get_battery_level()
{
    return battery_level;
}

void hardware_update_battery_level()
{
    adc_select_input(2);
    battery_level = adc_read()>>4;
}
float hardware_get_battery_level_float()
{
    const float conversion_factor = 2.5f / (1 << 8) * 3.1f;
    return battery_level * conversion_factor;
}
bool hardware_has_usb_power()
{
    bool res = gpio_get(USB_POWER_SENSE);
    // gpio_put(BLINK_PIN_LED, res);
    return res;
}

void hardware_shutdown()
{
    // shutdown all subsystems
    gpio_put(SUBSYSTEM_RESET_PIN, 0);

    // clear led colors
    uint32_t color[20] = {0};
    memset(color, 0, 20 * sizeof(uint32_t));
    ws2812_setColors(color);
    ws2812_trigger();

    // wait for the powerkey to go low, then shutdown
    while(hardware_get_key_state(0,0))
    {
        // do nothing
    }
    watchdog_enable(1,true);
    while(1);
}
bool hardware_disable_power_latch()
{
    gpio_set_pulls(23, false, false);
}

static bool last_mic_state = false;

void hardware_set_mic(bool mic_state)
{
    if(last_mic_state != mic_state)
    {
        last_mic_state = mic_state;
        driver_set_mic(last_mic_state);
    }
}
bool hardware_line_in_detected()
{
    return !gpio_get(LINE_IN_DETECT);
}
bool hardware_headphone_detected()
{
    return !gpio_get(HEADPHONE_DETECT);
}
