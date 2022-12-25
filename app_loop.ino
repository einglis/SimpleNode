
Logger app_log( "APP" );

// ----------------------------------------------------------------------------

#define POWER_PIN 14 // 14: dev, 5: island

void power_on() { digitalWrite(POWER_PIN, HIGH); }
void power_off() { digitalWrite(POWER_PIN, LOW); }

// ----------------------------------------------------------------------------

void app_setup( )
{
  pinMode( POWER_PIN, OUTPUT );
  power_off();
}

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
static int next_pattern = curr_pattern;
static int last_pattern = curr_pattern;

static int transition_count = 0;
Ticker transition_ticker;

void new_pattern( int pattern )
{
  next_pattern = pattern;

  transition_ticker.attach_ms_scheduled( 10, []() {
    if (transition_count)
      transition_count--;

    if (transition_count == 0)
    {
      if (next_pattern != curr_pattern)
      {
        app_log.infof("new pattern; %u -> %u", curr_pattern, next_pattern);
        last_pattern = curr_pattern;
        curr_pattern = next_pattern;
        transition_count = 255;
      }
      else
      {
        transition_ticker.detach();
      }
    }
  } );
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
    static int x = curr_pattern;
    x = 1 - x;
    new_pattern( x );
  }
}

class Pattern
{
  public:
    virtual void advance() = 0;
    virtual uint32_t pixel( unsigned int i ) = 0;
    virtual ~Pattern() = 0;
};
Pattern::~Pattern() {} // pure virtual destructor.

class RainbowPattern : public Pattern
{
  public:
    RainbowPattern() : j( 0 ) {}
    void advance() { j += pattern_phase_inc; }
    uint32_t pixel( unsigned int i ) { return Pixels::rgb_wheel((i+j) & 255); }
  private:
    unsigned int j;
};
RainbowPattern r1;

class RandomPattern : public Pattern
{
  public:
    void advance() { }
    uint32_t pixel( unsigned int ) { return Pixels::rgb_wheel(rand()); }
};
RandomPattern r2;


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

  Pattern* pattern = 0;

  switch (curr_pattern)
  {
      default: curr_pattern = 1; // fallthrough
      case 0: pattern = &r1; break;
      case 1: pattern = &r2; break;
  }

  if (transition_count > 0)
  {
      Pattern* pattern_outgoing = 0;
      switch (last_pattern)
      {
          default: last_pattern = 1; // fallthrough
          case 0: pattern_outgoing = &r1; break;
          case 1: pattern_outgoing = &r2; break;
      }

      if (pattern && pattern_outgoing)
      {
          pattern->advance();
          pattern_outgoing->advance();
          for (auto i = 0; i < num_pixels; i++)
              pixel( i, mix(pattern_outgoing->pixel(i),pattern->pixel(i),transition_count) );
      }

  }
  else
  {
      if (pattern)
      {
          pattern->advance();
          for (auto i = 0; i < num_pixels; i++)
              pixel( i, pattern->pixel(i) );
      }
  }

  return true;
}
