
Logger app_log( "APP" );

// ----------------------------------------------------------------------------

#define POWER_PIN 14 // 14: dev, island

void power_on()  { digitalWrite(POWER_PIN, LOW);  } // active low
void power_off() { digitalWrite(POWER_PIN, HIGH); }

// ----------------------------------------------------------------------------

int pattern_phase_inc = 1;

// ----------------------------------------------------------------------------

static int power_fade_count = 0;
Ticker power_fade_ticker;

static int pwr_target_on = false;

void toggle_power( )
{
  new_power( !pwr_target_on );
}

void new_power( bool pwr )
{
  pwr_target_on = pwr;

  power_fade_ticker.attach_ms_scheduled( 5, []() {
    if (pwr_target_on)
    {
      if (power_fade_count == 0)
        power_on();

      if (power_fade_count < 255)
        power_fade_count++;
    }
    else
    {
      if (power_fade_count > 0)
        power_fade_count--;
      if (power_fade_count == 0)
        power_off();
    }

    //pixels.brightness( power_fade_count );
    pixels.brightness( Adafruit_NeoPixel::gamma8(power_fade_count) );

    if (power_fade_count == 0 || power_fade_count == 255)
      power_fade_ticker.detach();
  } );
}


static int curr_pattern = 0;
static int prev_pattern = curr_pattern;

#include "pixel_pattern.h"
PixelPattern::~PixelPattern() {} // pure virtual destructor.

static std::vector< PixelPattern* >pixel_patterns;

static int transition_count = 0;
Ticker transition_ticker;

void new_pattern( int next_pattern )
{
  if (next_pattern >= (int)pixel_patterns.size())
  {
    app_log.warningf( "unknown pattern %d", next_pattern );
    return;
  }

  transition_ticker.attach_ms_scheduled( 10, [next_pattern]() {
    if (transition_count)
      transition_count--;

    if (transition_count == 0)
    {
      if (next_pattern != curr_pattern)
      {
        app_log.infof("new pattern; %u -> %u", curr_pattern, next_pattern);
        pixel_patterns[next_pattern]->activate();
          // won't be called for the very first pattern; gloss over this for now.

        prev_pattern = curr_pattern;
        curr_pattern = next_pattern;
        transition_count = 255;
      }
      else
      {
        pixel_patterns[prev_pattern]->deactivate();
        transition_ticker.detach();
      }
    }
  } );
}

void cycle_pattern( )
{
  new_pattern( (curr_pattern + 1) % pixel_patterns.size() ); 
}


void app_mqtt_message( const char* data, int len )
{
  app_log.infof( "mqtt message of len %u: %s", len, data );

  if (0 == strncmp_P("std", data, len))
  {
    pattern_phase_inc = 1;
  }
  else if (0 == strncmp("fast", data, len))
  {
    pattern_phase_inc = 6;
  }
  else if (0 == strncmp("rev", data, len))
  {
    pattern_phase_inc = -2;
  }
  else if (0 == strncmp("on", data, len))
  {
    new_power(true);
  }
  else if (0 == strncmp("off", data, len))
  {
    new_power(false);
  }
  else if (0 == strncmp("toggle", data, len))
  {
    toggle_power();
  }
  else if (0 == strncmp("next", data, len))
  {
    cycle_pattern( );
  }
}






uint32_t mix( uint32_t a, uint32_t b, unsigned int amnt)
{
    uint32_t x = 0;
    for (int i = 0; i < 4; i++)
    {
        unsigned int aa = a & 0xff;
        unsigned int bb = b & 0xff;
        unsigned int xx = (aa * amnt + bb * (255-amnt)) >> 8;

        x = (x >> 8) | (xx << 24);
        a >>= 8;
        b >>= 8;
    }

    return x;
}



bool app_pixels_update( uint16_t num_pixels, std::function< void(uint16_t n, uint32_t c) >pixel )
{

  PixelPattern* pattern = pixel_patterns[ curr_pattern ];
  PixelPattern* pattern_outgoing = pixel_patterns[ prev_pattern ];

  if (transition_count > 0)
  {
      if (pattern && pattern_outgoing)
      {
          pattern->advance( pattern_phase_inc );
          pattern_outgoing->advance( pattern_phase_inc );
          for (auto i = 0; i < num_pixels; i++)
              pixel( i, mix(pattern_outgoing->pixel(i),pattern->pixel(i),transition_count) );
      }
  }
  else
  {
      if (pattern)
      {
          pattern->advance( pattern_phase_inc );
          for (auto i = 0; i < num_pixels; i++)
              pixel( i, pattern->pixel(i) );
      }
  }

  return true;
}

// ----------------------------------------------------------------------------

namespace {

#include "pixel_patterns/rainbows.h"
CycleRainbowPattern rainbow1;
MonoRainbowPattern rainbow2;

#include "pixel_patterns/colour_random.h"
ColourRandomPattern random_colours;

#include "pixel_patterns/snakes.h"
SnakesPattern snakes( NUMPIXELS );

#include "pixel_patterns/sparkle.h"
SparklePattern sparkle_white;
SparklePattern sparkle_red( 0x010000 );
SparklePattern sparkle_yellow( 0x010100 );

}

void app_setup( )
{
  pinMode( POWER_PIN, OUTPUT );
  power_off();

  pixel_patterns.push_back( &rainbow1 );
  pixel_patterns.push_back( &rainbow2 );
  pixel_patterns.push_back( &snakes );
  pixel_patterns.push_back( &random_colours );
  pixel_patterns.push_back( &sparkle_white );
  pixel_patterns.push_back( &sparkle_red );
  pixel_patterns.push_back( &sparkle_yellow );
}
