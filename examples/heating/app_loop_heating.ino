
using node::SwitchInput;
  // still need fully qualified types in function signatures due to Arduino code mangling.

// ----------------------------------------------------------------------------

node::Logger app_log( "APP" );

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

Ticker test_ticker;

void app_setup( )
{
  pinMode( app::inputs::stat_hw_pin,  INPUT );
  pinMode( app::inputs::stat_ch1_pin, INPUT );
  pinMode( app::inputs::stat_ch2_pin, INPUT );

   stat_hw.begin( [](SwitchInput::Event f, int){ switch_event( f, "Hot Water Stat" ); } );
  stat_ch1.begin( [](SwitchInput::Event f, int){ switch_event( f, "Heating 1 Stat" ); } );
  stat_ch2.begin( [](SwitchInput::Event f, int){ switch_event( f, "Heating 2 Stat" ); } );

   stat_hw.update_debounce_ms( 100 ); // thermostat inputs will probably be
  stat_ch1.update_debounce_ms( 100 ); // quite clean, but we can afford to
  stat_ch2.update_debounce_ms( 100 ); // slug the inputs at this end anyway.

  pinMode( app::outputs::demand_hw_pin,  OUTPUT );
  pinMode( app::outputs::demand_ch1_pin, OUTPUT );
  pinMode( app::outputs::demand_ch2_pin, OUTPUT );
  pinMode( app::outputs::demand_ch3_pin, OUTPUT );

  test_ticker.attach_scheduled( 1, [](){
    static int c = 0;
    digitalWrite( app::outputs::demand_hw_pin,  c & 1 );
    digitalWrite( app::outputs::demand_ch1_pin, c & 2 );
    digitalWrite( app::outputs::demand_ch2_pin, c & 4 );
    digitalWrite( app::outputs::demand_ch3_pin, c & 8 );
    c++;
  } );

  crossings_ticker.attach_scheduled( 0.1, crossings_fn );
}

// ----------------------------------------------------------------------------
