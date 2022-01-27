#include "midi.h"

#define UART_ID uart1
#define BAUD_RATE 31250
#define UART_TX_PIN 4
#define UART_RX_PIN 5

void Midi::Init()
{
    // Set up our UART with the required speed.
    uart_init(UART_ID, BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
}

void Midi::NoteOn(int16_t pitch)
{
    uart_putc_raw(UART_ID, 0x90); // note on / channel
    uart_putc_raw(UART_ID, pitch); // middle c
    uart_putc_raw(UART_ID, 0x40); // velocity
}

void Midi::NoteOff(int16_t pitch)
{
    uart_putc_raw(UART_ID, 0x90); // note on / channel
    uart_putc_raw(UART_ID, pitch); // middle c
    uart_putc_raw(UART_ID, 0); // velocity
}