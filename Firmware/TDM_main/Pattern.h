#ifndef pattern_h_
#define pattern_h_

class Pattern
{
public:
  Pattern() : triggers() {};
  // 16 channels, 16 steps, 1 note playing on that step (will eventually need a parameter lock, but this should be enough)
  uint8_t triggers[16][16];
};

#endif // pattern_h_
