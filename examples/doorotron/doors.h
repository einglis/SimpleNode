
void door_open_led( int index, bool on )
{
  static int state = 0;
  if (on) state |=  (1 << index);
  else    state &= ~(1 << index);
  digitalWrite( app::outputs::led_1_pin, state );
}

void bipping_led( int index, bool on )
{
  static int state = 0;
  if (on) state |=  (1 << index);
  else    state &= ~(1 << index);
  digitalWrite( app::outputs::led_2_pin, state );
}

// ----------------------------------------------------------------------------

class Door
{
public:
  Door( const char *name, int switch_pin, int button_pin )
    : name{ name }
    , switch_pin{ switch_pin }
    , button_pin{ button_pin }
    , switch_in{ [switch_pin](){ return digitalRead( switch_pin ); } }
    , is_open{ false }
    , bips_requested{ 0 }
    , bips_performed{ 0 }
    , bip_state{ 0 }
    , index{ next_index++ }
  {
    pinMode( button_pin, OUTPUT );
    pinMode( switch_pin, INPUT );
  }

  void begin( )
  {
    switch_in.begin( [this](node::SwitchInput::Event f, int){ switch_event( f ); } );
    switch_in.update_debounce_ms( 100 ); // potentially noisy inputs so be conservative

    bip_ticker.attach_ms_scheduled( 500, [this](){ bip_wrangler(); } );
    mqtt.on( name, [this](auto, auto){ bips_requested++; } );
  }

private:
  void switch_event( node::SwitchInput::Event f ) // called in SYS context
  {
    schedule_function( [this, f]() {
      char buf[32];

      switch (f)
      {
        case node::SwitchInput::FlipOpen:
            is_open = true;
            door_open_led( index, true );
            sprintf( buf, "%s open", name );
            app_log.debugf( buf );
            mqtt.publish( buf );
            break;

        case node::SwitchInput::FlipClose:
            is_open = false;
            door_open_led( index, false );
            sprintf( buf, "%s closed", name );
            app_log.debugf( buf );
            mqtt.publish( buf );
            break;

        default:
            break;
      }
    } );
  }

  void bip_wrangler( )
  {
    switch (bip_state)
    {
      case 0: // idle
        if (bips_requested > bips_performed)
        {
          digitalWrite( button_pin, HIGH );
          bipping_led( index, true );

          bip_state = 1; // bipping
        }
        break;

      case 1: // bipping
        digitalWrite( button_pin, LOW );
        bipping_led( index, false );

        bip_state = 2; // holdoff
        break;

      case 2: // hold off
      default:
        // don't want more than one bip queued, so fake up the counter
        // by advancing further than the one we just did if needs be.
        bips_performed = max( bips_performed + 1, bips_requested - 1);

        bip_state = 0; // idle
        break;
    }
  }

  const char* name;
  const int switch_pin;
  const int button_pin;
  node::SwitchInput switch_in;
  bool is_open;

  Ticker bip_ticker;
  unsigned int bips_requested;
  unsigned int bips_performed;
    // we will ignore overflow; this is at least 65k bips
  int bip_state;

  int index;
  static int next_index;
};

int Door::next_index = 0;

// ----------------------------------------------------------------------------
