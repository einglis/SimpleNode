
node::Logger app_log( "APP" );

// ----------------------------------------------------------------------------

void power_on()  { digitalWrite(app::outputs::power_pin, HIGH); }
void power_off() { digitalWrite(app::outputs::power_pin, LOW); }

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
  configuration.rw().power = pwr;
  configuration.save();

  pwr_target_on = pwr;

  node::ticker_repeat( power_fade_ticker, 5, []()
  {
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
  configuration.rw().pattern = next_pattern;
  configuration.save();

  if (next_pattern >= (int)pixel_patterns.size())
  {
    app_log.warningf( "unknown pattern %d", next_pattern );
    return;
  }

  node::ticker_repeat( transition_ticker, 10/*ms*/, [next_pattern]() {
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

uint32_t fudge( int i, uint32_t c )
{
  // some replacement pixels are brighter than the old ones, so let's
  // mangle their brightness a bit to bring them more into line.

  if (i >= NUM_PIXELS - 4)
  {
    uint32_t r = (c & 0xff0000) >> 16;
    uint32_t g = (c & 0x00ff00) >>  8;
    uint32_t b = (c & 0x0000ff) >>  0;

    r *= 0.45;
    g *= 0.31;
    b *= 0.35;

    c = (r << 16) | (g << 8) | (b << 0);
  }

  return c;
}

namespace app {

bool pixels_update( uint16_t num_pixels, std::function<void(uint16_t, uint32_t)> pixel )
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
              pixel( i, fudge( i, mix(pattern_outgoing->pixel(i),pattern->pixel(i),transition_count) ) );
      }
  }
  else
  {
      if (pattern)
      {
          pattern->advance( pattern_phase_inc );
          for (auto i = 0; i < num_pixels; i++)
              pixel( i, fudge( i, pattern->pixel(i) ) );
      }
  }

  return true;
}

} // app

// ----------------------------------------------------------------------------

namespace {

#include "pixel_patterns/rainbows.h"
CycleRainbowPattern rainbow1;
MonoRainbowPattern rainbow2;

#include "pixel_patterns/colour_random.h"
ColourRandomPattern random_colours;

#include "pixel_patterns/snakes.h"
SnakesPattern snakes( NUM_PIXELS );

#include "pixel_patterns/sparkle.h"
SparklePattern sparkle_white;
SparklePattern sparkle_red( 0x010000 );
SparklePattern sparkle_yellow( 0x010100 );

}

void app_setup( )
{
  //pinMode( node::outputs::pixels_pin, OUTPUT ); // done for us
  pinMode( app::outputs::power_pin, OUTPUT );
  power_off();

  pixel_patterns.push_back( &rainbow1 );
  pixel_patterns.push_back( &rainbow2 );
  pixel_patterns.push_back( &snakes );
  pixel_patterns.push_back( &random_colours );
  pixel_patterns.push_back( &sparkle_white );
  pixel_patterns.push_back( &sparkle_red );
  pixel_patterns.push_back( &sparkle_yellow );
  pixels.brightness( 0 );

  mqtt.on( "std",  [](auto, auto){ pattern_phase_inc =  1; } );
  mqtt.on( "fast", [](auto, auto){ pattern_phase_inc =  6; } );
  mqtt.on( "rev",  [](auto, auto){ pattern_phase_inc = -1; } );
  mqtt.on( "next", [](auto, auto){ cycle_pattern(); } );

  mqtt.on( "on",     [](auto, auto){ new_power(true); } );
  mqtt.on( "off",    [](auto, auto){ new_power(false); } );
  mqtt.on( "toggle", [](auto, auto){ toggle_power(); } );

  //mqtt.on( "", [](auto, auto){ /*catchall*/ } );

  if (!configuration.load() || !configuration.is_valid())
    memset( &configuration.rw(), 0, sizeof(app::Config));

  configuration.rw().counter++;
  configuration.save();

  new_power(configuration.ro().power);
  curr_pattern = prev_pattern = configuration.ro().pattern;
}
