// everything needs to rotate because I have the rows and columns messed up on the other side
void requestButtons()
{
  return;
  int x = 0;
  int y = 0;
  // just read them real quick, we can after we get through the loop
  char data[3];
  int count = 0;
  Wire.requestFrom(8, 3);    // request 25 bytes from slave device #8
  while (Wire.available()) {
    data[count++] = Wire.read();
  }
  for(int i=0;i<3;i++)
  {
    int bc = 0;
    char b = data[i];
    while(bc<8)
    {
      // unpack the state
      pressed[y][x++] = (b&(1<<bc)) > 0;
      bc++;
      if(x==5)
      {
        x=0;
        y++;
      }
    }

  }
}

int lastLed[5][5];
int currentLed[5][5];

void setLed(int x, int y, int b)
{
  return;
  /**/
  currentLed[x][y]=b;
//  AudioNoInterrupts();
//  AudioInterrupts();
}
void updateLed()
{
  return;
  for(int x=0;x<5;x++)
  {
    for(int y=0;y<5;y++)
    {
      if(lastLed[x][y]!=currentLed[x][y])
      {
        Wire.beginTransmission(8); // transmit to device #8
        Wire.write('l');
        Wire.write(y);
        Wire.write(x);
        Wire.write(currentLed[x][y]);
        Wire.endTransmission();  
      }
      lastLed[x][y]=currentLed[x][y];
    }
  }
}
