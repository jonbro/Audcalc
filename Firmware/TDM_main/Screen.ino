#include <DMAChannel.h>
#include <SPI.h>
#include <DmaSpi.h>
#define FRAMESIZE 6144//96*64
uint32_t srcHi[FRAMESIZE];
uint16_t src[FRAMESIZE];
DMAChannel dma(false);

int _cs = 10;
int _dc = 15;
int _reset = 14;
#define SPICLOCK 8000000
SPISettings ssettings(SPICLOCK, MSBFIRST, SPI_MODE0);

inline uint16_t Color(uint8_t r, uint8_t g, uint8_t b){
  return ((uint16_t)(r & 0xF8) << 8) |
         ((uint16_t)(g & 0xFC) << 3) |
                    (b         >> 3);
}

// Select one of these defines to set the pixel color order
#define SSD1331_COLORORDER_RGB

#if defined SSD1331_COLORORDER_RGB && defined SSD1331_COLORORDER_BGR
  #error "RGB and BGR can not both be defined for SSD1331_COLORODER."
#endif

// Timing Delays
#define SSD1331_DELAYS_HWFILL    (3)
#define SSD1331_DELAYS_HWLINE       (1)

// SSD1331 Commands
#define SSD1331_CMD_DRAWLINE    0x21
#define SSD1331_CMD_DRAWRECT    0x22
#define SSD1331_CMD_FILL      0x26
#define SSD1331_CMD_SETCOLUMN     0x15
#define SSD1331_CMD_SETROW        0x75
#define SSD1331_CMD_CONTRASTA     0x81
#define SSD1331_CMD_CONTRASTB     0x82
#define SSD1331_CMD_CONTRASTC   0x83
#define SSD1331_CMD_MASTERCURRENT   0x87
#define SSD1331_CMD_SETREMAP    0xA0
#define SSD1331_CMD_STARTLINE     0xA1
#define SSD1331_CMD_DISPLAYOFFSET   0xA2
#define SSD1331_CMD_NORMALDISPLAY   0xA4
#define SSD1331_CMD_DISPLAYALLON    0xA5
#define SSD1331_CMD_DISPLAYALLOFF   0xA6
#define SSD1331_CMD_INVERTDISPLAY   0xA7
#define SSD1331_CMD_SETMULTIPLEX    0xA8
#define SSD1331_CMD_SETMASTER     0xAD
#define SSD1331_CMD_DISPLAYOFF    0xAE
#define SSD1331_CMD_DISPLAYON       0xAF
#define SSD1331_CMD_POWERMODE     0xB0
#define SSD1331_CMD_PRECHARGE     0xB1
#define SSD1331_CMD_CLOCKDIV    0xB3
#define SSD1331_CMD_PRECHARGEA    0x8A
#define SSD1331_CMD_PRECHARGEB    0x8B
#define SSD1331_CMD_PRECHARGEC    0x8C
#define SSD1331_CMD_PRECHARGELEVEL  0xBB
#define SSD1331_CMD_VCOMH       0xBE

void sendCommand(uint8_t commandByte, uint8_t *dataBytes = NULL, uint8_t numDataBytes = 0) {
    //SPI.beginTransaction(ssettings);
    digitalWrite(_cs, LOW);
    digitalWrite(_dc, LOW);
    SPI.transfer(commandByte); // Send the command byte
    digitalWrite(_dc, HIGH);
    for (int i=0; i<numDataBytes; i++) {
      SPI.transfer(*dataBytes); // Send the data bytes
      dataBytes++;
    }
    digitalWrite(_cs, HIGH);
  //Serial.println("sending data");
    //SPI.endTransaction();
}

void displayBegin()
{

    // Initialization Sequence
    sendCommand(SSD1331_CMD_DISPLAYOFF);    // 0xAE
    sendCommand(SSD1331_CMD_SETREMAP);  // 0xA0
#if defined SSD1331_COLORORDER_RGB
    sendCommand(0x72);        // RGB Color
#else
    sendCommand(0x76);        // BGR Color
#endif
    sendCommand(SSD1331_CMD_STARTLINE);   // 0xA1
    sendCommand(0x0);
    sendCommand(SSD1331_CMD_DISPLAYOFFSET);   // 0xA2
    sendCommand(0x0);
    sendCommand(SSD1331_CMD_NORMALDISPLAY);   // 0xA4
    sendCommand(SSD1331_CMD_SETMULTIPLEX);  // 0xA8
    sendCommand(0x3F);        // 0x3F 1/64 duty
    sendCommand(SSD1331_CMD_SETMASTER);   // 0xAD
    sendCommand(0x8E);
    sendCommand(SSD1331_CMD_POWERMODE);   // 0xB0
    sendCommand(0x0B);
    sendCommand(SSD1331_CMD_PRECHARGE);   // 0xB1
    sendCommand(0x31);
    sendCommand(SSD1331_CMD_CLOCKDIV);    // 0xB3
    sendCommand(0xF0);  // 7:4 = Oscillator Frequency, 3:0 = CLK Div Ratio (A[3:0]+1 = 1..16)
    sendCommand(SSD1331_CMD_PRECHARGEA);    // 0x8A
    sendCommand(0x64);
    sendCommand(SSD1331_CMD_PRECHARGEB);    // 0x8B
    sendCommand(0x78);
    sendCommand(SSD1331_CMD_PRECHARGEC);    // 0x8C
    sendCommand(0x64);
    sendCommand(SSD1331_CMD_PRECHARGELEVEL);    // 0xBB
    sendCommand(0x3A);
    sendCommand(SSD1331_CMD_VCOMH);     // 0xBE
    sendCommand(0x3E);
    sendCommand(SSD1331_CMD_MASTERCURRENT);   // 0x87
    sendCommand(0x06);
    sendCommand(SSD1331_CMD_CONTRASTA);   // 0x81
    sendCommand(0x91);
    sendCommand(SSD1331_CMD_CONTRASTB);   // 0x82
    sendCommand(0x50);
    sendCommand(SSD1331_CMD_CONTRASTC);   // 0x83
    sendCommand(0x7D);
    sendCommand(SSD1331_CMD_DISPLAYON); //--turn on oled panel
}
void setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {

  uint8_t x1 = x;
  uint8_t y1 = y;
  if (x1 > 95) x1 = 95;
  if (y1 > 63) y1 = 63;

  uint8_t x2 = (x+w-1);
  uint8_t y2 = (y+h-1);
  if (x2 > 95) x2 = 95;
  if (y2 > 63) y2 = 63;

  if (x1 > x2) {
    uint8_t t = x2;
    x2 = x1;
    x1 = t;
  }
  if (y1 > y2) {
    uint8_t t = y2;
    y2 = y1;
    y1 = t;
  }

  sendCommand(0x15); // Column addr set
  sendCommand(x1);
  sendCommand(x2);

  sendCommand(0x75); // Column addr set
  sendCommand(y1);
  sendCommand(y2);
}

void writePixel(uint16_t w)
{
  SPI.transfer(w >> 8);
  SPI.transfer(w);
}
struct Particle
{
  float x=0;
  float y=0;
  float sx;
  float sy;
  int age =100;
  
  void init()
  {
    sx = random(100)/100.0f;
    sy = random(100)/100.0f;
    age = random(50, 100);
  }
  void update()
  {
    x+=sx;
    y+=sy;
    age--;
    if(x<0||x>=96)
      sx*=-1;
    if(y<0||y>=64)
      sy*=-1;
  }
};
Particle particles[100];

void ScreenSetup()
{
    for(int i=0;i<100;i++)
  {
    particles[i].init();
  }

  for (size_t i = 0; i < FRAMESIZE; i++)
  {
    src[i] = 0;
    srcHi[i] =0xffffff;
  }
    pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);

  pinMode(_reset, OUTPUT);
  digitalWrite(_reset, HIGH);
  delay(10);
  digitalWrite(_reset, LOW);
  delay(10);
  digitalWrite(_reset, HIGH);
  
  pinMode(_dc, OUTPUT);
  digitalWrite(_dc, HIGH); // Data mode
  
  SPI.begin();
  displayBegin();
  delay(799);
  setAddrWindow(0, 0, 96, 64);
  //writePixel();
  // should be ready ot just smash the values over
  digitalWrite(_cs, LOW);
  digitalWrite(_dc, HIGH);
  LPSPI4_CR &= ~LPSPI_CR_MEN;//disable LPSPI:
  LPSPI4_CFGR1 |= LPSPI_CFGR1_NOSTALL; //prevent stall from RX
  //LPSPI4_TCR = 15; // Framesize 16 Bits
  LPSPI4_FCR = 0; // Fifo Watermark
  LPSPI4_DER = LPSPI_DER_TDDE; //TX DMA Request Enable
  LPSPI4_CR |= LPSPI_CR_MEN; //enable LPSPI:
  dma.begin(true); // Allocate the DMA channel first
  dma.destination((uint8_t &)  LPSPI4_TDR);
  dma.sourceBuffer(src, sizeof(src));
  //dma.transferCount(100);
  dma.disableOnCompletion();
  //DMAPriorityOrder(AudioOutputI2S::dma, dma);
}
void dmaTx()
{
  dma.enable();
  //dma.triggerManual();
  dma.triggerAtHardwareEvent( DMAMUX_SOURCE_LPSPI4_TX );  // start
  dma.attachInterrupt(spidmaisr);
}
void spidmaisr(void)
{
  dma.disable();
}
bool dmaStarted = false;
void ScreenLoop()
{
  if(dmaStarted && !dma.complete())
  {
    return;
  }
  dmaStarted = true;
  dma.clearComplete();
  /**/
  for(int i=0;i<64*96;i++)
  {
    // fade src hi
    uint8_t r = (srcHi[i]>>16)&0xff;
    uint8_t g = (srcHi[i]>>8)&0xff;
    uint8_t b = (srcHi[i]&0xff);
    r = (uint8_t)(((float)g)*0.93f);
    g =r;
    b =r;
    srcHi[i] = ((uint32_t)r<<16)|((uint16_t)g<<8)|b;
  }
  for(int i=0;i<100;i++)
  {
    //particles[i].update();
    int x = random(96);
    int y = random(64);
    //srcHi[(int)particles[i].x+(int)particles[i].y*96] = 0xffffff;
    srcHi[x+y*96] = 0xffffff;
  }
  digitalWrite(_cs, LOW);
  digitalWrite(_dc, HIGH);
  for(int i=0;i<64*96;i++)
  {
    uint8_t r = (srcHi[i]>>16)&0xff;
    uint8_t g = (srcHi[i]>>8)&0xff;
    uint8_t b = (srcHi[i]&0xff);
    src[i] = Color(r,g,b);
    // swap the values so that the endianess is correct
    // this can probably be solved on the spi side
    //writePixel(src[i]);
    src[i] = ((src[i]&0xff)<<8)|(src[i]>>8);
  }
  // should be ready ot just smash the values over
  int startMils = micros();
  
  dmaTx();
  //Serial.println(micros()-startMils);
}
