#include "Midi.h"
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
    uint8_t byte = (uint8_t)c;

    // realtime messages (clock, transport, active sensing) can arrive
    // interleaved inside other messages and must not disturb the parser state
    if(byte >= 0xf8)
    {
        if(byte == 0xf8 && OnSync != NULL)
        {
            OnSync();
        }
        else if(byte == 0xfa && OnStart != NULL)
        {
            OnStart();
        }
        else if(byte == 0xfb && OnContinue != NULL)
        {
            OnContinue();
        }
        else if(byte == 0xfc && OnStop != NULL)
        {
            OnStop();
        }
        return;
    }

    InputProcessor &in = inputProcessors[processor];
    if(byte & 0x80)
    {
        in.lastCommand = byte;
        in.dataByteCounter = 0;
        return;
    }

    // data byte with no status seen yet: nothing to attach it to
    if(in.dataByteCounter < 0)
    {
        return;
    }
    if(in.dataByteCounter < (int8_t)sizeof(in.dataBuffer))
    {
        in.dataBuffer[in.dataByteCounter++] = byte;
    }

    uint8_t command = ((uint8_t)in.lastCommand) & 0xf0;
    uint8_t channel = ((uint8_t)in.lastCommand) & 0x0f;
    if(in.dataByteCounter != 2)
    {
        return;
    }
    // after a complete message the counter resets but lastCommand stays,
    // so running status (one status byte, many data pairs) works
    switch(command)
    {
        case 0x80: // note off
            if(OnNoteOff != NULL)
            {
                OnNoteOff(channel, in.dataBuffer[0], in.dataBuffer[1]);
            }
            in.dataByteCounter = 0;
            break;
        case 0x90: // note on (velocity 0 is a note off)
            if(in.dataBuffer[1] == 0)
            {
                if(OnNoteOff != NULL)
                {
                    OnNoteOff(channel, in.dataBuffer[0], in.dataBuffer[1]);
                }
            }
            else if(OnNoteOn != NULL)
            {
                OnNoteOn(channel, in.dataBuffer[0], in.dataBuffer[1]);
            }
            in.dataByteCounter = 0;
            break;
        case 0xb0: // control change
            if(OnCCChanged != NULL)
            {
                // filter out repeated cc values (lastCCValue inits to 0xff,
                // which no 7 bit data byte matches, so the first one passes)
                if(lastCCValue[in.dataBuffer[0]] != in.dataBuffer[1])
                {
                    OnCCChanged(in.dataBuffer[0], in.dataBuffer[1]);
                    lastCCValue[in.dataBuffer[0]] = in.dataBuffer[1];
                }
            }
            in.dataByteCounter = 0;
            break;
        case 0xf0: // system common
            if((uint8_t)in.lastCommand == 0xf2)
            {
                if(OnPosition != NULL)
                {
                    OnPosition(in.dataBuffer[0] | (in.dataBuffer[1] << 7));
                }
                in.dataByteCounter = 0;
            }
            break;
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
