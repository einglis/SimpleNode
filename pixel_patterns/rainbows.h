#pragma once

#include "../pixel_pattern.h"

class CycleRainbowPattern : public PixelPattern
{
public:
  CycleRainbowPattern( ) : j{ 0 } { }

  virtual void advance( int inc )
  {
    j += inc;
  }
  virtual uint32_t pixel( unsigned int i )
  {
    return rgb_wheel( (i+j) & 255 );
  }

private:
  unsigned int j;
};

class MonoRainbowPattern : public PixelPattern
{
public:
  MonoRainbowPattern() : j( 0 ) { }

  virtual void advance( int inc )
  {
    j += inc;
  }
  virtual uint32_t pixel( unsigned int )
  {
     return rgb_wheel( j & 255 );
  }
private:
  unsigned int j;
};
