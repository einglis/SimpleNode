
#include "inputs.h"
void button_event( ButtonInput::Event e, int count ); // fwd declarations as Arduino
void switch_event( SwitchInput::Event e, int count ); // compiler workaround

Configuration& config = configuration; // gratuitous naming alias

Logger app_log( "APP" );

// ----------------------------------------------------------------------------

Logger input_log( "INPUTS" );

void button_event( ButtonInput::Event e, int count ) // called in SYS context
{
  schedule_function( [=]() {
    switch (e)
    {
      case ButtonInput::Press:
          input_log.infof( "Button press %d", count );
          break;
      case ButtonInput::HoldShort:
          input_log.infof( "Button hold short (%d)", count );
          break;
      case ButtonInput::HoldLong:
          input_log.infof( "Button hold long (%d)", count );
          break;
      case ButtonInput::Final:
          input_log.infof( "Button final (%d)", count );
          break;
      default:
          break; // most unexpected.
    }
  } );
}

void switch_event( SwitchInput::Event e, int count ) // called in SYS context
{
  schedule_function( [=]() {
    switch (e)
    {
      case SwitchInput::FlipOpen:
          input_log.infof( "Switch flip open (%d)", count );
          break;
      case SwitchInput::FlipClose:
          input_log.infof( "Switch flip close (%d)", count );
          break;
      case SwitchInput::Final:
          input_log.infof( "Switch final (%d)", count );
          break;
      default:
          break; // most unexpected.
    }
  } );
}

// ----------------------------------------------------------------------------

bool app_pixels_update( uint16_t num_pixels, std::function<void(uint16_t, uint32_t)> pixel )
{
  static int rate_limit = 0;
  rate_limit = (rate_limit + 1) % 10;
  if (rate_limit)
    return false; // no update

  static int pos = 0;
  for (auto i = 0; i < num_pixels; i++)
    pixel( i, (i == pos) ? 0x00080400 : 0x00000000 );
  pos = (pos + 1) % 6; // 6 is the true number of dev pixels
  return true;
}

// ----------------------------------------------------------------------------

ButtonInput db( [](){ return !digitalRead(BUTTON_PIN); /*active low*/ } );
SwitchInput ds( [](){ return !digitalRead(SWITCH_PIN); /*active low*/ } );

void app_setup( )
{
  //pinMode( PIXELS_PIN, OUTPUT ); // done for us
  pinMode( LED_1_PIN, OUTPUT );
  pinMode( LED_2_PIN, OUTPUT );

  pinMode( BUTTON_PIN, INPUT );
  pinMode( SWITCH_PIN, INPUT );

  db.setup( [](auto e, auto c){ button_event( e, c ); } );
  ds.setup( [](auto e, auto c){ switch_event( e, c ); } );

  mqtt.on( "", [](auto, auto){ app_log.infof( "mqtt message" ); } );


  if (!config.is_valid())
  {
    app_log.warning( "loaded configuration is not valid" );
    config.rw().counter = 0;
  }

  app_log.infof( "config counter is %d", config().counter );
  config.rw().counter++;
  config.save();
}
