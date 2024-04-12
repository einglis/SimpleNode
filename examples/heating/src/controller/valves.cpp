
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
  ChannelEx( int in_pin, const char* tag )
    : Channel( input_fn, output_fn, this )
    , in_pin{ in_pin }
    , tag{ tag }
  { }
  const int in_pin;
  const char* tag;
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


ChannelEx hw{ app::outputs::demand_hw_pin,  "HW"  }; // real controller outputs are valve follower inputs
ChannelEx ch1{ app::outputs::demand_ch1_pin, "CH1" };
ChannelEx ch2{ app::outputs::demand_ch2_pin, "CH2" };
ChannelEx ch3{ app::outputs::demand_ch3_pin, "CH3" };

Channel *channels[] = { &hw, &ch1, &ch2, &ch3 };
const size_t num_channels = sizeof(channels)/sizeof(channels[0]);

BoilerEx boiler{ };
Controller control( channels, num_channels, (Channel*)&ch1 /*overrun*/, (Boiler*)&boiler );


node::Ticker valve_follower_ticker;

void valve_follower_poll( )
{
  // update all the state
  for (auto chan : channels)
    chan->ms_poll();
  control.ms_poll();
  boiler.ms_poll();


  // report the results over serial too
  char buf[128];
  auto expand_bits = []( char *buf, unsigned int x ) ->size_t
  {
    buf[0] = (x & 1) ? 'w' : '-';
    for (size_t i = 1; i < num_channels; ++i)
      buf[i] = (x & (1<<i)) ? (i+'0') : '-';
    return num_channels;
  };

  {
    static unsigned int prev_demand{ 0 };
    auto curr_demand = control.demand();
    if (curr_demand != prev_demand)
    {
      char *bp = &buf[0];
      bp += sprintf( bp, "Demand: " );
      bp += expand_bits( bp, prev_demand );
      bp += sprintf( bp, " --> " );
      bp += expand_bits( bp, curr_demand );
      *bp = '\0';
      Serial.println(buf);
    }
    prev_demand = curr_demand;
  }

  {
    static unsigned int prev_open{ 0 };
    auto curr_open = control.open();
    if (curr_open != prev_open)
    {
      char *bp = &buf[0];
      bp += sprintf( bp, "  Open: " );
      bp += expand_bits( bp, prev_open );
      bp += sprintf( bp, " --> " );
      bp += expand_bits( bp, curr_open );
      *bp = '\0';
      Serial.println(buf);
    }
    prev_open = curr_open;
  }

  {
    static auto prev_state = Controller::Idle;
    auto curr_state = control.state();
    if (curr_state != prev_state)
    {
      switch (curr_state)
      {
        case Controller::Idle: Serial.println("  Ctrl: idle"); break;
        case Controller::Demand: Serial.println("  Ctrl: demanding"); break;
        case Controller::Cool: Serial.println("  Ctrl: cooling"); break;
        default: Serial.println("  Ctrl: UKNOWN STATE"); break;
      }
    }
    prev_state = curr_state;
  }

  {
    static auto prev_state = Boiler::Idle;
    auto curr_state = boiler.state();
    if (curr_state != prev_state)
    {
      switch (curr_state)
      {
        case Boiler::Idle: Serial.println("Boiler: idle"); break;
        case Boiler::Underrun: Serial.println("Boiler: underrun"); break;
        case Boiler::Demand: Serial.println("Boiler: demanding"); break;
        case Boiler::Overrun: Serial.println("Boiler: overrun"); break;
        default: Serial.println("Boiler: UNKNOWN STATE"); break;
      }
    }
    prev_state = curr_state;
  }

}

void valve_follower_begin( )
{
  valve_follower_ticker.repeat( 1/*ms*/, valve_follower_poll );
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
