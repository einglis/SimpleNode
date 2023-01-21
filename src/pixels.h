#pragma once

#include <Adafruit_NeoPixel.h>
#include <Ticker.h>

namespace node {

class Pixels
{
public:
  typedef std::function< bool( uint16_t num_pixels, std::function< void(uint16_t n, uint32_t c) > pixel_fn ) > update_fn_t;
    // a function called to update any/all pixels via multiple calls to the 'pixel_fn' callback.
    // returns true if any pixel needs an update; false if no pixels need to be updated.

  Pixels( int pin, int num_pixels, update_fn_t update_fn )
    : pixels( num_pixels, pin, NEO_GRB + NEO_KHZ800 )
    , work_phase{ 0 }
    , update_fn{ update_fn }
    , need_update{ false }
    { }

  void begin()
  {
    //pinMode( pin, OUTPUT );
      // the Adafruit_NeoPixel constructor will have done this for us

    pixels.begin(); // This initializes the NeoPixel library.

    ticker.attach_ms( 50, [this](){ update(); } );
  }

  void brightness( uint8_t b )
  {
    pixels.setBrightness( b );
  }

private:
  Adafruit_NeoPixel pixels;
  Ticker ticker;
  int work_phase;
  update_fn_t update_fn;
  bool need_update;

  void update( )
  {
    // called in SYS context, so be quick

    if (work_phase == 0)
    {
      need_update = update_fn( pixels.numPixels(),
        [this](uint16_t n, uint32_t c){ pixels.setPixelColor( n, c ); } );
    }
    else
    {
      if (need_update)
        pixels.show();
    }

    work_phase = !work_phase;
  }
};

} // node
