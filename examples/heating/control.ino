
using node::SwitchInput;
  // still need fully qualified types in function signatures due to Arduino code mangling.

// ----------------------------------------------------------------------------

void switch_event( node::SwitchInput::Event f, const char* name ) // called in SYS context
{
  schedule_function( [=]() {
    switch (f)
    {
      case SwitchInput::FlipOpen:
          app_log.infof( "%s OFF", name );
          break;

      case SwitchInput::FlipClose:
          app_log.infof( "%s DEMAND", name );
          break;

      default:
          break;
    }
  } );
}

// ----------------------------------------------------------------------------


// off, stat_off, stat_on
// a given output has a time component and one or more stat components, one of which is always demanding.
struct channel
{
  int pin;
  bool enabled; // time portion
  bool active; // stat portion
  unsigned int sensitivity;
};

static struct channel chans[] =
{
  { app::outputs::demand_hw_pin,  true,  false, 1 << 0 },  // HW, sensitive to HW stat
  { app::outputs::demand_ch1_pin, true,  false, 1 << 1 },  // CH1, sensitive to CH1 stat
  { app::outputs::demand_ch2_pin, false, false, 1 << 2 },  // CH2, sensitive to CH2 stat
  { app::outputs::demand_ch3_pin, true,  false,    6   },  // CH3, sensitive to CH1,CH2 stats
};
static const size_t num_chans = sizeof(chans) / sizeof(chans[0]);



Ticker crossings_ticker;

struct peg
{
  int hh;
  int mm;
  bool on;
};

std::vector<peg> pegs = {
  { 10, 00, true },
  { 10, 30, false },
  { 12, 32, true },
  { 12, 32, true },
  { 12, 33, true, },
  { 14, 56, false }
};

std::vector<peg>::iterator it = pegs.end();
bool pegs_changed = true;

int next_hhmm = 1000;

void crossings_fn( )
{
  static long int epoch = 30000;//ntp.epoch_time() * 60;
  epoch+=60;

  int hh = ntp.epoch_hrs( epoch );
  int mm = ntp.epoch_mins( epoch );

  app_log.debugf( "%d:%d", hh, mm );


  if ((rand() & 0xff) == 0)
  {
    Serial.println("\nPegs change\n");
    pegs_changed = true;
  }

  if (pegs_changed)
  {
    it = pegs.begin();
    while (it != pegs.end() && (it->hh < hh || (it->hh == hh && it->mm < mm)))
    {
        Serial.printf("skipping %d:%d %s\n", it->hh, it->mm, it->on ? "ON" : "OFF" );
        it++;
    }
    pegs_changed = false;
  }

  while (it != pegs.end() && it->hh == hh && it->mm == mm)
  {
      Serial.printf("Heating %s\n", it->on ? "ON" : "OFF" );
      //digitalWrite( app::outputs::demand_ch2_pin, it->on );
      chans[2].enabled = it->on;
      it++;
  }
  if (it == pegs.end())
  {
    Serial.println("ready for a new day");
    it = pegs.begin();
  }

}

// ----------------------------------------------------------------------------

SwitchInput  stat_hw( [](){ return digitalRead( app::inputs::stat_hw_pin  ); } );
SwitchInput stat_ch1( [](){ return digitalRead( app::inputs::stat_ch1_pin ); } );
SwitchInput stat_ch2( [](){ return digitalRead( app::inputs::stat_ch2_pin ); } );

inline uint32_t rr1 (uint32_t x) { return (x << 31) | (x >> 1); } // roll right one

Ticker test_ticker;
Ticker t1;


void update( node::SwitchInput::Event f, unsigned int mask )
{
  for (size_t i = 0; i < num_chans; i++)
  {
    if (chans[i].sensitivity & mask)
    {
      if (f == SwitchInput::FlipClose)
        chans[i].active = true;
      else if (f == SwitchInput::FlipOpen)
        chans[i].active = false;
    }
  }
}

void app_setup( )
{
  pinMode( app::inputs::stat_hw_pin,  INPUT );
  pinMode( app::inputs::stat_ch1_pin, INPUT );
  pinMode( app::inputs::stat_ch2_pin, INPUT );

   stat_hw.begin( [](SwitchInput::Event f, int){ switch_event( f, "Hot Water Stat" ); update(f,1); } );
  stat_ch1.begin( [](SwitchInput::Event f, int){ switch_event( f, "Heating 1 Stat" ); update(f,2); } );
  stat_ch2.begin( [](SwitchInput::Event f, int){ switch_event( f, "Heating 2 Stat" ); update(f,4); } );

   stat_hw.update_debounce_ms( 100 ); // thermostat inputs will probably be
  stat_ch1.update_debounce_ms( 100 ); // quite clean, but we can afford to
  stat_ch2.update_debounce_ms( 100 ); // slug the inputs at this end anyway.

  pinMode( app::outputs::demand_hw_pin,  OUTPUT );
  pinMode( app::outputs::demand_ch1_pin, OUTPUT );
  pinMode( app::outputs::demand_ch2_pin, OUTPUT );
  pinMode( app::outputs::demand_ch3_pin, OUTPUT );

  crossings_ticker.attach_scheduled( 0.1, crossings_fn );

  static uint32_t f0 = 0x00000007;



  t1.attach_scheduled(0.1, [](){

    for (size_t i = 0; i < num_chans; i++)
    {
      Serial.printf("chan %u - %u %u\n", i, chans[i].enabled, chans[i].active );
      if (!chans[i].enabled)
        digitalWrite( chans[i].pin, LOW );
      else if (!chans[i].active)
        digitalWrite( chans[i].pin, f0 & 1 );
      else
        digitalWrite( chans[i].pin, HIGH );
    }

    f0 = rr1( f0 );
  } );
}

// ----------------------------------------------------------------------------
