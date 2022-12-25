#pragma once

#include "../pixel_pattern.h"

class RainbowPattern : public PixelPattern
{
public:
  RainbowPattern( ) : j{ 0 } { }

  virtual void advance( int inc )
  { 
    j += inc;
  }
  virtual uint32_t pixel( unsigned int i ) 
  { 
    return rgb_wheel((i+j) & 255); 
  }

private:
  unsigned int j;
};
