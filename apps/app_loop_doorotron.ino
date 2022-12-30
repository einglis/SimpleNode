
#include "inputs.h"

Logger app_log( "APP" );
Logger input_log( "INPUTS" );

enum { DOOR_A = 0, DOOR_B = 1, num_doors = 2 };
bool is_open[num_doors] = { false, false };
int pins[num_doors] = { BUTTON_A_PIN, BUTTON_B_PIN };

SwitchInput switch_a( [](){ return !digitalRead( SWITCH_A_PIN ); } );
SwitchInput switch_b( [](){ return !digitalRead( SWITCH_B_PIN ); } );
  // the switch inputs themselves are active high, but the logic from the door is active low.

void door_open_led( int index, bool on )
{
  static int state = 0;
  if (on) state |=  (1 << index);
  else    state &= ~(1 << index);
  digitalWrite( LED_1_PIN, state );
}

void bipping_led( int index, bool on )
{
  static int state = 0;
  if (on) state |=  (1 << index);
  else    state &= ~(1 << index);
  digitalWrite( LED_2_PIN, state );
}

void switch_event( int s, SwitchInput::Event f ) // called in SYS context
{
  if (s != DOOR_A and s != DOOR_B)
    return; // never expected

  schedule_function( [=]() {
    switch (f)
    {
      case SwitchInput::FlipOpen:
          input_log.debugf( "Switch %c flip open", s + 'A' );
          is_open[ s ] = true;
          door_open_led( s, true );
          mqtt.publish( (s == DOOR_A) ? "door A open" : "door B open" );
          break;
      case SwitchInput::FlipClose:
          input_log.debugf( "Switch %c flip close", s + 'A' );
          is_open[ s ] = false;
          door_open_led( s, false );
          mqtt.publish( (s == DOOR_A) ? "door A closed" : "door B closed" );
          break;
      default:
          break;
    }
  } );
}

Ticker bip_ticker;
unsigned int bips_requested[num_doors] = { };
  // it seems reasonable to ignore overflow; this is at least 65k bips

void bip_wrangler( )
{
  static unsigned int bips_performed[num_doors] = { };
  static int state[num_doors] = { };

  for (auto i = 0; i < num_doors; i++)
  {
    switch (state[i])
    {
      case 0: // idle
        if (bips_requested[i] > bips_performed[i])
        {
          digitalWrite( pins[i], HIGH );
          bipping_led( i, true );

          state[i] = 1; // bipping
        }
        break;

      case 1: // bipping
        digitalWrite( pins[i], LOW );
        bipping_led( i, false );

        state[i] = 2; // holdoff
        break;

      case 2: // hold off
      default:
        // don't want more than one bip queued, so fake up the counter
        // by advancing further than the one we just did if needs be.
        bips_performed[i] = max( bips_performed[i] + 1, bips_requested[i] - 1);

        state[i] = 0; // idle
        break;
    }
  }
}

void app_setup( )
{
  pinMode( LED_1_PIN, OUTPUT );
  pinMode( LED_2_PIN, OUTPUT );
  pinMode( BUTTON_A_PIN, OUTPUT );
  pinMode( BUTTON_B_PIN, OUTPUT );
  pinMode( SWITCH_A_PIN, INPUT );
  pinMode( SWITCH_B_PIN, INPUT );

  bip_ticker.attach_ms_scheduled( 500, bip_wrangler );

  switch_a.setup( [](SwitchInput::Event f, int ){ switch_event( DOOR_A, f ); } );
  switch_a.update_debounce_ms( 100 ); // potentially noisy inputs so be conservative
  switch_b.setup( [](SwitchInput::Event f, int ){ switch_event( DOOR_B, f ); } );
  switch_b.update_debounce_ms( 100 );

  mqtt.on( "doorA", [](auto, auto){ ++bips_requested[DOOR_A]; } );
  mqtt.on( "doorB", [](auto, auto){ ++bips_requested[DOOR_B]; } );
}

// ----------------------------------------------------------------------------
