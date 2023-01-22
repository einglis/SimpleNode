
node::Logger app_log( "APP" );

// ----------------------------------------------------------------------------

bool power_state = false;
void set_power_state( bool new_state )
{
  power_state = new_state;
  digitalWrite( app::outputs::relay_pin, power_state );
}

void toggle_power_state()
{
  set_power_state( !power_state );
}

// ----------------------------------------------------------------------------

void button_event( node::ButtonInput::Event e, int count ) // called in SYS context
{
  // do this immediately for a timely response...
  if (e == node::ButtonInput::Press && count == 1)
    toggle_power_state(); 

  // ...reporting can be done in the next loop
  schedule_function( [=]() {

    const char* event_name = nullptr;
    switch (e)
    {
      case node::ButtonInput::Press:     event_name = "press"; break;
      case node::ButtonInput::HoldShort: event_name = "short"; break;
      case node::ButtonInput::HoldLong:  event_name = "long";  break;
      case node::ButtonInput::Final:     event_name = "final"; break;
      default: break; // unexpected
    }

    if (event_name)
    {
      char buf[32];
      sprintf( buf, "%s %d", event_name, count );
      mqtt.publish( buf );
    }
  } );
}

// ----------------------------------------------------------------------------

node::ButtonInput button( [](){ return !digitalRead( app::inputs::button_pin_n ); } ); // active low

void app_setup( )
{
  pinMode( app::inputs::button_pin_n, INPUT );
  button.begin( [](auto e, auto c){ button_event( e, c ); } );

  pinMode( app::outputs::led_pin_n, OUTPUT );
  pinMode( app::outputs::relay_pin, OUTPUT );
  set_power_state( false );

  mqtt.on( "on",     [](auto, auto){ set_power_state( true ); } );
  mqtt.on( "off",    [](auto, auto){ set_power_state( false ); } );
  mqtt.on( "toggle", [](auto, auto){ toggle_power_state(); } );
}

// ----------------------------------------------------------------------------
