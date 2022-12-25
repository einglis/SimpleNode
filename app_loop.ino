
Logger app_log( "APP" );

void app_mqtt_message( const char* data, int len )
{
  app_log.infof( "mqtt message of len %u: %s", len, data );
}


bool app_pixels_update( uint16_t num_pixels, std::function< void(uint16_t n, uint32_t c) >pixel )
{
  static uint8_t pattern_phase = 0;
  pattern_phase++; // happy to overflow and loop

  for (auto i = 0; i < num_pixels; i++)
    pixel( i, Pixels::Wheel( pattern_phase ) );

  return true;
}
