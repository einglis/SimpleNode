
#include "app_config.h"
#include <SimpleNode.h>

#include "Boiler.h"
#include "Channel.h"
#include "Controller.h"

using valves::Boiler;
using valves::Channel;
using valves::Controller;

class ChannelEx : public Channel
{
public:
  ChannelEx( int in_pin )
    : Channel( input_fn, output_fn, this )
    , in_pin{ in_pin }
  { }
  const int in_pin;
private:
  static int input_fn( void* context ) { return digitalRead( ((ChannelEx*)context)->in_pin ); }
  static void output_fn( void*, int ) { }
};

class BoilerEx : public Boiler
{
public:
  BoilerEx( )
    : Boiler( output_fn, this )
  { }
private:
  static void output_fn( void*, int ) { }
};

#ifdef PRE_PLUMB_VALVES
ChannelEx hw{ app::outputs::demand_hw_pin }; // real controller outputs are valve follower inputs
ChannelEx ch1{ app::outputs::demand_ch1_pin };

Channel *channels[] = { &hw, &ch1 };
const size_t num_channels = sizeof(channels)/sizeof(channels[0]);

BoilerEx boiler{ };
Controller control{ channels, num_channels, nullptr /*no default overrun*/, (Boiler*)&boiler };

#else
ChannelEx hw{ app::outputs::demand_hw_pin }; // real controller outputs are valve follower inputs
ChannelEx ch1{ app::outputs::demand_ch1_pin };
ChannelEx ch2{ app::outputs::demand_ch2_pin };
ChannelEx ch3{ app::outputs::demand_ch3_pin };

Channel *channels[] = { &hw, &ch1, &ch2, &ch3 };
const size_t num_channels = sizeof(channels)/sizeof(channels[0]);

BoilerEx boiler{ };
Controller control{ channels, num_channels, (Channel*)&ch3 /*overrun*/, (Boiler*)&boiler };
#endif

// ------------------------------------

node::Ticker valve_follower_ticker;
void valve_follower_begin( )
{
  boiler.overrun_time( 5 * 60 * 1000L ); // five minutes in ms
  boiler.underrun_time( Channel::valve_close_time_ms + 1000L );
    // to match the actual valve controller.

  valve_follower_ticker.repeat( 1/*ms*/, []() {
    for (auto chan : channels)
      chan->ms_poll();
    control.ms_poll();
    boiler.ms_poll();
  } );
}

size_t valve_follower_get_valves( char *bp )
{
  for (auto chan : channels)
  {
    switch (chan->state())
    {
      case Channel::Closed: *bp++ = '-'; break;
      case Channel::Opening: *bp++ = 'o'; break;
      case Channel::Open: *bp++ = 'O'; break;
      case Channel::Closing: *bp++ = 'c'; break;
      defatult: *bp++ = '?'; break;
    }
  }
  return num_channels;
}

size_t valve_follower_get_boiler( char *bp )
{
  switch (boiler.state())
  {
    case Boiler::Idle: *bp++ = '-'; break;
    case Boiler::Underrun: *bp++ = 'u'; break;
    case Boiler::Demand: *bp++ = 'D'; break;
    case Boiler::Overrun: *bp++ = 'o'; break;
    default: *bp++ = '?'; break;
  }
  return 1;
}
