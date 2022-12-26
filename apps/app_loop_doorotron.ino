
#include "inputs.h"

Logger app_log( "APP" );
Logger input_log( "INPUTS" );

SwitchInput switch_a( [](){ return digitalRead(SWITCH_A_PIN); /*active low*/ } );
SwitchInput switch_b( [](){ return digitalRead(SWITCH_B_PIN); /*active low*/ } );

void switch_event( char s, SwitchInput::Event f ) // called in SYS context
{
  schedule_function( [=]() {
    switch (f)
    {
      case SwitchInput::FlipOpen:
          input_log.infof( "Switch %c flip open", s );
          break;
      case SwitchInput::FlipClose:
          input_log.infof( "Switch %c flip close", s );
          break;
      default:
          break;
    }
  } );
}

void bip( int pin, bool active_high = true )
{
  Ticker* t = new Ticker; // ugh!  but only a test function
  digitalWrite( pin, active_high );
  t->once_ms( 500, [=]() {
    digitalWrite( pin, !active_high );
    delete t;
  } );
}

void app_setup( )
{
  pinMode( LED_1_PIN, OUTPUT );
  pinMode( LED_2_PIN, OUTPUT );
  pinMode( BUTTON_A_PIN, OUTPUT );
  pinMode( BUTTON_B_PIN, OUTPUT );
  pinMode( SWITCH_A_PIN, INPUT );
  pinMode( SWITCH_B_PIN, INPUT );

  switch_a.setup( [](SwitchInput::Event f, int ){ switch_event( 'A', f ); } );
  switch_a.update_debounce_ms( 100 ); // potentially noisy inputs so be conservative
  switch_b.setup( [](SwitchInput::Event f, int ){ switch_event( 'B', f ); } );
  switch_b.update_debounce_ms( 100 );

  mqtt.on( "led1",  [](auto, auto){ bip( LED_1_PIN ); } );
  mqtt.on( "led2",  [](auto, auto){ bip( LED_2_PIN ); } );
  mqtt.on( "doorA", [](auto, auto){ bip( BUTTON_A_PIN ); } );
  mqtt.on( "doorB", [](auto, auto){ bip( BUTTON_B_PIN ); } );
}

// ----------------------------------------------------------------------------
