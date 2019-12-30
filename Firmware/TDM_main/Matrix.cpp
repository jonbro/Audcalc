
#include <Wire.h>
#include "Matrix.h"

#define DATA_B 0x00
#define DATA_A 0x01
#define DIR_A 0x03
#define DIR_B 0x02
#define PULLDOWN_A 0x07
#define PULLDOWN_B 0x06

int brightnessLevels = 16;

void Matrix::setup()
{
  Wire.begin();
  for (int x = 0; x < 5; x++)
  {
    for (int y = 0; y < 5; y++)
    {
      this->brightness[x][y] = 0;
    }
  }
  this->brightnessLevels = 16;
  // initialize all our registers
  // directions
  writeReg(DIR_B, 0b11111100);
  writeReg(DIR_A, 0);
  // pulldowns
  writeReg(PULLDOWN_B, 0b01111100);

}
void Matrix::update()
{
  for (int y = 0; y < 5; y++)
  {
    // sink the columns that need to be sank for this row
    int xReg = 0;
    for (int x = 0; x < 5; x++)
    {
      if (this->loopCount >= this->brightness[x][y])
      {
        xReg |= (1 << x);
      }
    }
    Wire.beginTransmission(0x20);
    Wire.write(DATA_B);
    Wire.write(y >= 3 ? 1 << (y - 3) : 0x0);
    Wire.write(((1 << (y + 5) | 0x1f & xReg)) & 0xff);
    Wire.endTransmission(true);
    if (this->loopCount == 1)
    {
      Wire.beginTransmission(0x20);
      Wire.write(DATA_B);
      Wire.endTransmission();
      Wire.requestFrom(0x20, 1);
      while (Wire.available()) { // slave may send less than requested
        int c = Wire.read(); // receive a byte as character
        // read the buttons pressed for this row
        for (int x = 0; x < 5; x++)
        {
          this->pressed[x][y] = (1 << (x + 2)) & (c & 0x7c);
        }
      }
    }
    else
    {
      //delayMicroseconds(1);
    }
    // this kills ghosting on the bottom two rows at the cost of a bit of overhead :/
    Wire.beginTransmission(0x20);
    Wire.write(DATA_A);
    Wire.write(0x1f);
    Wire.endTransmission();
  }
  this->loopCount--;
  if (this->loopCount <= 0)
  {
    this->loopCount = this->brightnessLevels;
    /*
    for (int x = 0; x < 5; x++)
    {
      for (int y = 0; y < 5; y++)
      {
        brightness[x][y] = max(0, brightness[x][y] - 1);
      }
    }
    if (random(20) > 8)
    {
      brightness[random(5)][random(5)] = brightnessLevels;
    }
    */
  }
}
void Matrix::setLed(int x, int y, int value)
{
  this->brightness[x][y] = value;
}
bool Matrix::readButton(int x, int y)
{
  return this->pressed[x][y];
}

void Matrix::writeReg(int reg, int val)
{
  Wire.beginTransmission(0x20);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission(true);
}
