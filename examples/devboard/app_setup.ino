
using node::ButtonInput;
using node::SwitchInput;
  // still need fully qualified types in function
  // signatures due to Arduino code mangling.

// ----------------------------------------------------------------------------

node::Logger app_log( "APP" );
node::Logger input_log( "INPUTS" );

void button_event( node::ButtonInput::Event e, int count ) // called in SYS context
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

void switch_event( node::SwitchInput::Event e, int count ) // called in SYS context
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

namespace app {

bool pixels_update( uint16_t num_pixels, std::function<void(uint16_t, uint32_t)> pixel_fn )
{
  static int rate_limit = 0;
  rate_limit = (rate_limit + 1) % 10;
  if (rate_limit)
    return false; // no update

  static int pos = 0;
  for (auto i = 0; i < num_pixels; i++)
    pixel_fn( i, (i == pos) ? 0x00080400 : 0x00000000 );
  pos = (pos + 1) % 6; // 6 is the true number of dev pixels
  return true;
}

} // app

// ----------------------------------------------------------------------------

auto& config = configuration; // gratuitous naming alias

ButtonInput db( [](){ return !digitalRead( app::inputs::button_pin_n ); } );
SwitchInput ds( [](){ return !digitalRead( app::inputs::switch_pin_n ); } );

// ------------------------------------

void app_setup( )
{
  //pinMode( node::outputs::pixels_pin, OUTPUT ); // done for us
  pinMode( app::outputs::led_1_pin, OUTPUT );
  pinMode( app::outputs::led_2_pin, OUTPUT );

  pinMode( app::inputs::button_pin_n, INPUT );
  pinMode( app::inputs::switch_pin_n, INPUT );

  db.begin( [](auto e, auto c){ button_event( e, c ); } );
  ds.begin( [](auto e, auto c){ switch_event( e, c ); } );

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
