
#include "inputs.h"

Logger app_log( "APP" );

void switch_event( SwitchInput::Event f, const char* name );
void switch_event( SwitchInput::Event f, const char* name ) // called in SYS context
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

SwitchInput  stat_hw( [](){ return digitalRead( STAT_HW_PIN  ); } );
SwitchInput stat_ch1( [](){ return digitalRead( STAT_CH1_PIN ); } );
SwitchInput stat_ch2( [](){ return digitalRead( STAT_CH2_PIN ); } );

Ticker test_ticker;

void app_setup( )
{
  pinMode( STAT_HW_PIN,  INPUT );
  pinMode( STAT_CH1_PIN, INPUT );
  pinMode( STAT_CH2_PIN, INPUT );

   stat_hw.setup( [](SwitchInput::Event f, int){ switch_event( f, "Hot Water Stat" ); } );
  stat_ch1.setup( [](SwitchInput::Event f, int){ switch_event( f, "Heating 1 Stat" ); } );
  stat_ch2.setup( [](SwitchInput::Event f, int){ switch_event( f, "Heating 2 Stat" ); } );

   stat_hw.update_debounce_ms( 100 ); // thermostat inputs will probably be
  stat_ch1.update_debounce_ms( 100 ); // quite clean, but we can afford to
  stat_ch2.update_debounce_ms( 100 ); // slug the inputs at this end anyway.

  pinMode( DEMAND_HW_PIN,  OUTPUT );
  pinMode( DEMAND_CH1_PIN, OUTPUT );
  pinMode( DEMAND_CH2_PIN, OUTPUT );
  pinMode( DEMAND_CH3_PIN, OUTPUT );

  test_ticker.attach_scheduled( 1, [](){
    static int c = 0;
    digitalWrite( DEMAND_HW_PIN,  c & 1 );
    digitalWrite( DEMAND_CH1_PIN, c & 2 );
    digitalWrite( DEMAND_CH2_PIN, c & 4 );
    digitalWrite( DEMAND_CH3_PIN, c & 8 );
    c++;
  } );
}

// ----------------------------------------------------------------------------
