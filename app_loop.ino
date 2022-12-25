
Logger app_log( "APP" );

// ----------------------------------------------------------------------------

#define POWER_PIN 14 // 14: dev, 5: island

void power_on() { digitalWrite(POWER_PIN, HIGH); }
void power_off() { digitalWrite(POWER_PIN, LOW); }

// ----------------------------------------------------------------------------

void app_setup( )
{
  pinMode( POWER_PIN, OUTPUT );
  power_off();
}

// ----------------------------------------------------------------------------

int pattern_phase_inc = 1;

// ----------------------------------------------------------------------------

void app_mqtt_message( const char* data, int len )
{
  app_log.infof( "mqtt message of len %u: %s", len, data );

  if (0 == strncmp_P("std", data, len))
  {
    pattern_phase_inc = 1;
  }
  else if (0 == strncmp("fast", data, len))
  {
    pattern_phase_inc = 6;
  }
  else if (0 == strncmp("rev", data, len))
  {
    pattern_phase_inc = -2;
  }
  else if (0 == strncmp("on", data, len))
  {
    power_on();
  }
  else if (0 == strncmp("off", data, len))
  {
    power_off();
  }
}

bool app_pixels_update( uint16_t num_pixels, std::function< void(uint16_t n, uint32_t c) >pixel )
{
  static uint8_t pattern_phase = 0;
  pattern_phase += pattern_phase_inc; // happy to overflow and loop

  for (auto i = 0; i < num_pixels; i++)
    pixel( i, Pixels::Wheel( pattern_phase ) );

  return true;
}
