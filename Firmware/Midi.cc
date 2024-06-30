#include "midi.h"
#include "hardware/irq.h"
#include "hardware/dma.h"
#include <stdio.h>
#include <string.h>
#include "tusb.h"

#define UART_ID uart1
#define BAUD_RATE 31250
#define UART_TX_PIN 4
#define UART_RX_PIN 5

static int chars_rxed = 0;
uint8_t in_buf[128];
uint8_t buf_count = 0;
bool readBuf = false;
Midi* midi;

void midi_task(void)
{
  // The MIDI interface always creates input and output port/jack descriptors
  // regardless of these being used or not. Therefore incoming traffic should be read
  // (possibly just discarded) to avoid the sender blocking in IO
  uint8_t c[1];
  while ( tud_midi_available() )
  {
    tud_midi_stream_read(c, 1);
    midi->ProcessMessage(*c, 1);
  }
}


void on_uart_rx() {
    while (uart_is_readable(UART_ID)) {
        midi->ProcessMessage(uart_getc(UART_ID), 0);
    }
}

void Midi::ProcessMessage(char c, uint8_t processor)
{
    if(processor > 1) return;
    if((c & 0x80) > 0)
    {
        // clear the data buffer
        inputProcessors[processor].dataByteCounter = 0;
        inputProcessors[processor].lastCommand = c;
    }
    else if(inputProcessors[processor].dataByteCounter < 4)
    {
        inputProcessors[processor].dataBuffer[inputProcessors[processor].dataByteCounter++] = c&0x7f;
    }
    if(c == 0xf8 && OnSync != NULL)
    {
        OnSync();
    }
    if(c == 0xfa && OnStart != NULL)
    {
        OnStart();
    }
    if(c == 0xfb && OnContinue != NULL)
    {
        OnContinue();
    }
    if(c == 0xfc && OnStop != NULL)
    {
        OnStop();
    }
    if(inputProcessors[processor].lastCommand == 0xf2 && inputProcessors[processor].dataByteCounter == 2 && OnPosition != NULL) // song position pointer
    {
        OnPosition(inputProcessors[processor].dataBuffer[0] + inputProcessors[processor].dataBuffer[1]);
    }
    // not a system common message (transport / tick)
    if(inputProcessors[processor].lastCommand & (0xf0 == 0))
    {
        uint8_t channel = inputProcessors[processor].lastCommand & 0xf;
        // note on
        if(inputProcessors[processor].lastCommand & 0x90 > 0 && inputProcessors[processor].dataByteCounter == 2)
        {
            // actually a note off
            if(inputProcessors[processor].dataBuffer[1] == 0 && OnNoteOff != NULL)
            {
                OnNoteOff(channel, inputProcessors[processor].dataBuffer[0], inputProcessors[processor].dataBuffer[1]);
            }
            else if(OnNoteOn != NULL)
            {
                OnNoteOn(channel, inputProcessors[processor].dataBuffer[0], inputProcessors[processor].dataBuffer[1]);
            }
        }
        if((inputProcessors[processor].lastCommand & 0x80) > 0 && inputProcessors[processor].dataByteCounter == 2 && OnNoteOff != NULL)
        {
            OnNoteOff(channel, inputProcessors[processor].dataBuffer[0], inputProcessors[processor].dataBuffer[1]);
        }
        // control change
        if((inputProcessors[processor].lastCommand & 0xb0) > 0 && inputProcessors[processor].dataByteCounter == 2 && OnCCChanged != NULL)
        {
            // filter out repeated cc changes
            if(lastCCValue[inputProcessors[processor].dataBuffer[0]] == 0xff || lastCCValue[inputProcessors[processor].dataBuffer[0]] != inputProcessors[processor].dataBuffer[1])
            {
                OnCCChanged(inputProcessors[processor].dataBuffer[0], inputProcessors[processor].dataBuffer[1]);
                lastCCValue[inputProcessors[processor].dataBuffer[0]] = inputProcessors[processor].dataBuffer[1];
            }
        }
    }
}

void Midi::Init()
{
    midi = this;
    // clear out the last values so we immediately update on first get
    for(int i=0;i<128;i++)
    {
        lastCCValue[i] = 0xff;
    }
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
    TxIndex = 0;
    //
    DmaChannelTX = dma_claim_unused_channel(true);

    dma_channel_config txConfig = dma_channel_get_default_config(DmaChannelTX);
    channel_config_set_transfer_data_size(&txConfig, DMA_SIZE_8);
    channel_config_set_read_increment(&txConfig, true);
    channel_config_set_write_increment(&txConfig, false);
    channel_config_set_ring(&txConfig, false, MIDI_BUF_LENGTH_POW);
    channel_config_set_dreq(&txConfig, DREQ_UART1_TX);
    dma_channel_set_config(DmaChannelTX, &txConfig, false);
    dma_channel_set_write_addr(DmaChannelTX, &uart1_hw->dr, false);
    initialized = true;
}

uint16_t Midi::Write(const uint8_t* data, uint16_t length)
{
    tud_midi_stream_write(0, data, length);

    if (!initialized || length == 0) {
        return 0;
    }
    // copy the data into the buffer if possible
    uint8_t* start = &TxBuffer[TxIndex + MIDI_BUF_LENGTH*pingPong];
    // truncate copy amount to fit into buffer
    length = TxIndex + length > MIDI_BUF_LENGTH - 1
        ? MIDI_BUF_LENGTH - 1 - TxIndex
        : length;
    memcpy(start, data,
            length);
    TxIndex += length;
    // attempt to flush immediately
    Flush();
    return length;
}

void Midi::Flush()
{
    if(!initialized || dma_channel_is_busy(DmaChannelTX))
        return;
    // Size check
    if (TxIndex == 0)
    {
        return;
    }
    uint8_t* start = &TxBuffer[MIDI_BUF_LENGTH*pingPong];
    dma_channel_transfer_from_buffer_now(DmaChannelTX, start, TxIndex);
    pingPong = (pingPong+1)%2;
    TxIndex = 0;
}

void Midi::NoteOn(uint8_t channel, uint8_t pitch, uint8_t velocity)
{
    uint8_t send[] = {0x90+channel,pitch,velocity};
    Write(send, 3);
}

void Midi::NoteOff(uint8_t channel, uint8_t pitch)
{
    uint8_t send[] = {0x90+channel,pitch,0};
    Write(send, 3);
}

void Midi::StartSequence()
{
    uint8_t sendValue = 0xFA;
    Write(&sendValue, 1);
}
void Midi::StopSequence()
{
    uint8_t sendValue = 0xFC;
    Write(&sendValue, 1);
}
void Midi::TimingClock()
{
    uint8_t sendValue = 0xF8;
    Write(&sendValue, 1);
}
