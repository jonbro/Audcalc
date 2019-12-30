#ifndef matrix_h_
#define matrix_h_

class Matrix
{
  public:
    void setup();
    void update();
    void setLed(int x, int y, int value);
    bool readButton(int x, int y);
  private:
    void writeReg(int reg, int val);
    int loopCount = 0;
    int brightnessLevels = 16;
    int brightness[5][5];
    bool pressed[5][5];
};

#endif
