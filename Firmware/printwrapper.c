#include "printwrapper.h"
#include "pico/mutex.h"
#include "tusb.h"
#include "pico/stdio/driver.h"

static mutex_t stdio_usb_mutex;
#define PICO_STDIO_USB_STDOUT_TIMEOUT_US 500000
bool stdio_usb_connected(void) {
    return tud_ready();
}

static void stdio_usb_out_chars(const char *buf, int length) {
  for (uint32_t i = 0; i < length; i++) {
    tud_cdc_write_char(buf[i]);
  }
  tud_cdc_write_flush();
}

int stdio_usb_in_chars(char *buf, int length) {
}

stdio_driver_t stdio_usb = {
    .out_chars = stdio_usb_out_chars,
    .in_chars = stdio_usb_in_chars,
    .crlf_enabled = true
};

void usb_printf_init()
{
    stdio_set_driver_enabled(&stdio_usb, true);
}