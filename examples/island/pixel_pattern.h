#pragma once

#include "Adafruit_NeoPixel.h"

class PixelPattern
{
public:
  virtual void advance( int inc = 1 ) = 0;
  virtual uint32_t pixel( unsigned int i ) = 0;
  virtual void activate() { }
  virtual void deactivate() { }
  virtual ~PixelPattern() = 0;

protected:
  static uint32_t colour( uint8_t r, uint8_t g, uint8_t b )
  {
    return Adafruit_NeoPixel::Color(r, g, b);
  }
  static uint8_t gamma( uint8_t x ) 
  { 
    return Adafruit_NeoPixel::gamma8(x); 
  }

  static uint32_t rgb_wheel( uint8_t p )
  {
    if (p < 85)
    {
      return Adafruit_NeoPixel::Color(p * 3, 255 - p * 3, 0);
    }
    else if(p < 170)
    {
      p -= 85;
      return Adafruit_NeoPixel::Color(255 - p * 3, 0, p * 3);
    }
    else
    {
      p -= 170;
      return Adafruit_NeoPixel::Color(0, p * 3, 255 - p * 3);
    }
  }
};
