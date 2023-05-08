
#include "pegboard.h"

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

class Channel : public PegBoard
{
public:
  Channel( int output_pin )
    : pin{ output_pin }
    { }
  int pin;
};

static Channel chans[] =
{
  Channel( app::outputs::demand_hw_pin ),
  Channel( app::outputs::demand_ch1_pin ),
  Channel( app::outputs::demand_ch2_pin ),
  Channel( app::outputs::demand_ch3_pin ),
};
static const size_t num_chans = sizeof(chans) / sizeof(chans[0]);

Ticker crossings_ticker;





void crossings_fn( )
{
  static long int epoch = 30000;//ntp.epoch_time() * 60;
  epoch+=60;

  int hh = ntp.epoch_hrs( epoch );
  int mm = ntp.epoch_mins( epoch );

  if ((mm % 5) == 0)
    app_log.debugf( "%d:%d", hh, mm );

  for (auto& c : chans)
    c.tick();

}

// ----------------------------------------------------------------------------

SwitchInput  stat_hw( [](){ return digitalRead( app::inputs::stat_hw_pin  ); } );
SwitchInput stat_ch1( [](){ return digitalRead( app::inputs::stat_ch1_pin ); } );
SwitchInput stat_ch2( [](){ return digitalRead( app::inputs::stat_ch2_pin ); } );

uint32_t stats = 0;


inline uint32_t rr1 (uint32_t x) { return (x << 31) | (x >> 1); } // roll right one

Ticker test_ticker;
Ticker t1;


void stat_update( node::SwitchInput::Event f, int index )
{
  if (f == SwitchInput::FlipClose)
    stats |= (1 << index);
  else if (f == SwitchInput::FlipOpen)
    stats &= ~(1 << index);
}

void app_setup( )
{
  pinMode( app::inputs::stat_hw_pin,  INPUT );
  pinMode( app::inputs::stat_ch1_pin, INPUT );
  pinMode( app::inputs::stat_ch2_pin, INPUT );

   stat_hw.begin( [](SwitchInput::Event f, int){ switch_event( f, "Hot Water Stat" ); stat_update(f,0); } );
  stat_ch1.begin( [](SwitchInput::Event f, int){ switch_event( f, "Heating 1 Stat" ); stat_update(f,1); } );
  stat_ch2.begin( [](SwitchInput::Event f, int){ switch_event( f, "Heating 2 Stat" ); stat_update(f,2); } );

   stat_hw.update_debounce_ms( 100 ); // thermostat inputs will probably be
  stat_ch1.update_debounce_ms( 100 ); // quite clean, but we can afford to
  stat_ch2.update_debounce_ms( 100 ); // slug the inputs at this end anyway.

  pinMode( app::outputs::demand_hw_pin,  OUTPUT );
  pinMode( app::outputs::demand_ch1_pin, OUTPUT );
  pinMode( app::outputs::demand_ch2_pin, OUTPUT );
  pinMode( app::outputs::demand_ch3_pin, OUTPUT );

//  mqtt.on(  "hw", [](auto, auto data) { if (bool x; str_on_off( data, x ))  hw_control( x ); } );
//  mqtt.on( "ch1", [](auto, auto data) { if (bool x; str_on_off( data, x )) ch1_control( x ); } );
//  mqtt.on( "ch2", [](auto, auto data) { if (bool x; str_on_off( data, x )) ch2_control( x ); } );
//  mqtt.on( "ch3", [](auto, auto data) { if (bool x; str_on_off( data, x )) ch3_control( x ); } );



  crossings_ticker.attach_scheduled( 0.1, crossings_fn );

  static uint32_t f0 = 0x07070707;



  t1.attach_scheduled(0.1, [](){

    for (auto& c : chans)
    {
      unsigned int sense = c.current_sensitivity();

      if (sense & stats)
        digitalWrite( c.pin, HIGH );
      else if (sense)
        digitalWrite( c.pin, f0 & 1 );
      else
        digitalWrite( c.pin, LOW );
    }

    f0 = rr1( f0 );
  } );
}

// ----------------------------------------------------------------------------

Channel* id_to_channel( int channel );
Channel* id_to_channel( int channel )
{
  switch (channel)
  {
    case 0: return &chans[0];
    case 1: return &chans[1];
    case 2: return &chans[2];
    case 3: return &chans[3];
    default:
      return nullptr;
  }
}

// ----------------------------------------------------------------------------

void cmd_now( int channel, unsigned int sensitivity, bool on_n_off )
{
  printf("c%d s%x %s now\n", channel, sensitivity, (on_n_off) ? "ON" : "OFF" );
  Channel* c = id_to_channel( channel );
  if (!c) return;

  if (on_n_off)
    c->on_now( sensitivity );
  else
    c->off_now( sensitivity );
}

void cmd_set( int channel, unsigned int sensitivity, bool on_n_off, int time )
{
  printf("c%d s%x %s at %02u:%02u\n", channel, sensitivity, (on_n_off) ? "ON" : "OFF", time / 60, time % 60 );
  Channel* c = id_to_channel( channel );
  if (!c) return;

  if (on_n_off)
    c->on_peg( time, sensitivity );
  else
    c->off_peg( time, sensitivity );
}

void cmd_boost( int channel, int time )
{
  printf("c%d boost %u\n", channel, time );
  Channel* c = id_to_channel( channel );
  if (!c) return;

  if (time > 0)
    c->boost_on( time );
  else
    c->boost_off( );
}

void cmd_delete( int channel, int time )
{
  printf("c%d del %02u:%02u\n", channel, time / 60, time % 60 );
  Channel* c = id_to_channel( channel );
  if (!c) return;

  c->remove_peg( time );
}
