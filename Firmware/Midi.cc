#include "midi.h"
#include "hardware/irq.h"
#include <stdio.h>

#define UART_ID uart1
#define BAUD_RATE 31250
#define UART_TX_PIN 4
#define UART_RX_PIN 5

static int chars_rxed = 0;
uint8_t in_buf[128];
uint8_t buf_count = 0;
void on_uart_rx() {
    while (uart_is_readable(UART_ID)) {
        char c = uart_getc(UART_ID);
        in_buf[buf_count++] = c;
        // if((c>>7) == 1){
        //     printf("\n");
        // }
        // printf("0x%x ", c);
    }
}

void Midi::Init()
{
    // Set up our UART with the required speed.
    uart_init(UART_ID, BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
    irq_set_enabled(UART_IRQ, true);

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(UART_ID, true, false);
}
int Midi::GetNote()
{
    uint8_t cmdStart = 0;
    
    while((in_buf[cmdStart]>>4) != 0x9 && cmdStart<buf_count)
    {
        cmdStart++;
        while((in_buf[cmdStart]>>7) != 1 && cmdStart<buf_count)
        {
            cmdStart++;
        }
    }
    if(cmdStart>buf_count)
    {
        buf_count = 0;
        return -1;
    }
    if((in_buf[cmdStart]>>4) == 0x9)
    {
        buf_count = 0;
        return in_buf[cmdStart+1]&0x7f; // midi data bytes strip off the top bit
    }
    // note off
    buf_count = 0;
    return -1;
}
void Midi::NoteOn(int16_t pitch, uint8_t velocity)
{
    uart_putc_raw(UART_ID, 0x90); // note on / channel
    uart_putc_raw(UART_ID, pitch); // middle c
    uart_putc_raw(UART_ID, velocity); // velocity
}

void Midi::NoteOff(int16_t pitch)
{
    uart_putc_raw(UART_ID, 0x90); // note on / channel
    uart_putc_raw(UART_ID, pitch); // middle c
    uart_putc_raw(UART_ID, 0); // velocity
}

void Midi::StartSequence()
{
    uart_putc_raw(UART_ID, 0xFA);
}
void Midi::StopSequence()
{
    uart_putc_raw(UART_ID, 0xFC);
}
void Midi::TimingClock()
{
    uart_putc_raw(UART_ID, 0xF8);
}
