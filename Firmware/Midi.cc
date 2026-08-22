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

// how many data bytes a status byte expects before its message is complete,
// -1 for sysex which runs until the end of exclusive status byte arrives
static int8_t ExpectedDataBytes(uint8_t status)
{
    if(status < 0xf0)
    {
        switch(status & 0xf0)
        {
            case 0xc0: // program change
            case 0xd0: // channel pressure
                return 1;
            default: // note off / on, poly pressure, control change, pitch bend
                return 2;
        }
    }
    switch(status)
    {
        case 0xf0: return -1; // sysex
        case 0xf1: return 1;  // mtc quarter frame
        case 0xf2: return 2;  // song position pointer
        case 0xf3: return 1;  // song select
        default: return 0;    // tune request, end of sysex, undefined
    }
}

void Midi::ProcessMessage(char c, uint8_t processor)
{
    if(processor > 1) return;
    uint8_t byte = (uint8_t)c;

    // realtime messages (clock, transport, active sensing) can arrive
    // interleaved inside other messages. Don't mess with the parser, jsut return
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
        // messages that carry no data bytes (tune request, end of sysex) are
        // complete on arrival, and they cancel any running status
        if(ExpectedDataBytes(byte) == 0)
        {
            in.lastCommand = 0;
            in.dataByteCounter = -1;
            return;
        }
        in.lastCommand = byte;
        in.dataByteCounter = 0;
        return;
    }

    // data byte with no associated status, drop
    if(in.dataByteCounter < 0)
    {
        return;
    }
    // don't overflow the databuffer (in the case of stuff like sysex messages)
    if(in.dataByteCounter < (int8_t)sizeof(in.dataBuffer))
    {
        in.dataBuffer[in.dataByteCounter++] = byte;
    }

    uint8_t status = (uint8_t)in.lastCommand;
    int8_t expected = ExpectedDataBytes(status);
    // sysex data is discarded, we just ignore it until we get the end status message
    // this also returns if we haven't parsed all the expected data messages yet
    if(expected < 0 || in.dataByteCounter != expected)
    {
        return;
    }
    // after a complete channel message the counter resets but lastCommand
    // stays, so running status (one status byte, many data pairs) works
    in.dataByteCounter = 0;

    uint8_t command = status & 0xf0;
    uint8_t channel = status & 0x0f;
    switch(command)
    {
        case 0x80: // note off
            if(OnNoteOff != NULL)
            {
                OnNoteOff(channel, in.dataBuffer[0], in.dataBuffer[1]);
            }
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
            break;
        case 0xb0: // control change
            if(OnCCChanged != NULL)
            {
                // filter out repeated cc values, tracked per input so the same
                // value arriving on din and usb isn't swallowed as a repeat
                // (lastCCValue inits to 0xff, which no 7 bit data byte
                // matches, so the first value on each input always passes)
                if(lastCCValue[processor][in.dataBuffer[0]] != in.dataBuffer[1])
                {
                    OnCCChanged(in.dataBuffer[0], in.dataBuffer[1]);
                    lastCCValue[processor][in.dataBuffer[0]] = in.dataBuffer[1];
                }
            }
            break;
        case 0xc0: // program change
        case 0xd0: // channel pressure
        case 0xa0: // poly key pressure
        case 0xe0: // pitch bend
            // nothing listens for these yet, but they still have to be
            // consumed so their data bytes don't pair up with the next message
            break;
        case 0xf0: // system common, running status does not apply
            if(status == 0xf2 && OnPosition != NULL)
            {
                OnPosition(in.dataBuffer[0] | (in.dataBuffer[1] << 7));
            }
            in.lastCommand = 0;
            in.dataByteCounter = -1;
            break;
    }
}

void Midi::Init()
{
    midi = this;
    // clear out the last values so we immediately update on first get
    for(int p=0;p<2;p++)
    {
        for(int i=0;i<128;i++)
        {
            lastCCValue[p][i] = 0xff;
        }
    }
    ResetSentCCState();
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
    // if this message doesn't fit in the remaining buffer, then drop it
    // avoids sending malformed messages.
    if (TxIndex + length > MIDI_BUF_LENGTH) {
        return 0;
    }
    uint8_t* start = &TxBuffer[TxIndex + MIDI_BUF_LENGTH*pingPong];
    memcpy(start, data, length);
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

bool Midi::NoteOff(uint8_t channel, uint8_t pitch)
{
    uint8_t send[] = {(uint8_t)(0x90+channel),pitch,0};
    return Write(send, 3) == 3;
}

void Midi::ResetSentCCState()
{
    memset(lastSentCCValue, 0xff, sizeof(lastSentCCValue));
    memset(lastSentProgram, 0xff, sizeof(lastSentProgram));
}

bool Midi::ProgramChange(uint8_t channel, uint8_t program)
{
    channel &= 0x0f;
    program &= 0x7f;
    if(lastSentProgram[channel] == program)
        return true;
    // two byte message: status plus the program, no controller number
    uint8_t send[] = {(uint8_t)(0xC0+channel), program};
    if(Write(send, 2) != 2)
        return false;
    lastSentProgram[channel] = program;
    return true;
}

bool Midi::ControlChange(uint8_t channel, uint8_t cc, uint8_t value)
{
    channel &= 0x0f;
    cc &= 0x7f;
    value &= 0x7f;
    // the receiver already holds this value on this channel, whoever put it there
    if(lastSentCCValue[channel][cc] == value)
        return true;
    uint8_t send[] = {(uint8_t)(0xB0+channel), cc, value};
    // only mark if the send succeeds
    if(Write(send, 3) != 3)
        return false;
    lastSentCCValue[channel][cc] = value;
    return true;
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
