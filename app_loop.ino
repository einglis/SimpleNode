
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



static int curr_pattern = 1;
static int next_pattern = 1;
static int last_pattern = 1;

static int transition_count = 0;

Ticker trans_ticker;

void tfn( )
{
  if (transition_count == 0)
  {
    if (next_pattern != curr_pattern)
    {
      Serial.printf("new pattern; %u -> %u\n", curr_pattern, next_pattern);
      transition_count = 254;
      last_pattern = curr_pattern;
      curr_pattern = next_pattern;
    }
  }
  else
  {
    transition_count -= 1;
  }

  if (transition_count == 0)
    trans_ticker.detach();
}

void new_pattern( int pattern )
{
  next_pattern = pattern;

  trans_ticker.attach_ms_scheduled( 10, tfn );
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
    power_on();
  }
  else if (0 == strncmp("off", data, len))
  {
    power_off();
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
    uint32_t pixel( unsigned int i ) { return Pixels::Wheel((i+j) & 255); }
  private:
    unsigned int j;
};
RainbowPattern r1;

class RandomPattern : public Pattern
{
  public:
    RandomPattern() : j( 0 ) {}
    void advance() { j += pattern_phase_inc; }
    uint32_t pixel( unsigned int i ) { return rand(); }
  private:
    unsigned int j;
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
