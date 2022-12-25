
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

const uint8_t gamma_2_6[256] =
{
    0,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   2,   2,
    2,   2,   2,   2,   3,   3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,
    4,   4,   5,   5,   5,   5,   5,   6,   6,   6,   6,   7,   7,   7,   7,   8,
    8,   8,   9,   9,   9,   9,  10,  10,  10,  11,  11,  12,  12,  12,  13,  13,
   13,  14,  14,  15,  15,  16,  16,  17,  17,  17,  18,  18,  19,  19,  20,  20,
   21,  22,  22,  23,  23,  24,  24,  25,  26,  26,  27,  28,  28,  29,  30,  30,
   31,  32,  32,  33,  34,  35,  35,  36,  37,  38,  38,  39,  40,  41,  42,  42,
   43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  52,  53,  54,  55,  56,  57,
   58,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  71,  72,  73,  74,  75,
   77,  78,  79,  80,  82,  83,  84,  86,  87,  88,  90,  91,  92,  94,  95,  96,
   98,  99, 101, 102, 104, 105, 107, 108, 110, 111, 113, 114, 116, 118, 119, 121,
  122, 124, 126, 127, 129, 131, 133, 134, 136, 138, 140, 141, 143, 145, 147, 149,
  151, 152, 154, 156, 158, 160, 162, 164, 166, 168, 170, 172, 174, 176, 178, 180,
  182, 184, 187, 189, 191, 193, 195, 197, 200, 202, 204, 206, 209, 211, 213, 216,
  218, 220, 223, 225, 227, 230, 232, 235, 237, 240, 242, 245, 247, 250, 252, 255,
};

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
    pixels.brightness( gamma_2_6[power_fade_count] );

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
