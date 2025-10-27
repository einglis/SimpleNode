#include <HardwareSerial.h>
#include <SimpleNode.h>

extern node::Logger app_log;
void parse_message( const char* msg ); // lazy

namespace {

HardwareSerial mySerial(2); // RX: 16, TX: 17

const static int rx_buf_len = 128;  // long enough for anything we're interested in.
char rx_buf[rx_buf_len] = { 0 };
int rx_buf_used = 0;

void handle_input( const char* rx )
{
  if (!rx)
    return; // bad data

  if (0 == strncmp(rx, "+AT: ", 5))
  {
    Serial.print( "Received info: \"" ); // don't write "stATus"!
    Serial.print( rx+5 );
    Serial.println( "\"" );
  }
  else if (0 == strncmp(rx, "+TEST: ", 7))
  {
    Serial.print( "Received: \"" );
    Serial.print( rx+7 );
    Serial.println( "\"" );
    parse_message( rx+7 );
  }
}


Ticker config_ticker;
void schedule_config_refresh();

static void send_config()
{
  app_log.info("sending modem config");

  mySerial.println("AT");
  delay(200);
  mySerial.println("AT+MODE=TEST");
  delay(200);
  mySerial.println("AT+TEST=RFCFG,868,7,125,8,16,14,ON,OFF,OFF");
  delay(200);
  mySerial.println("AT+TEST=?");
  delay(200);
  mySerial.println("AT+TEST= RXLRPKT");

  schedule_config_refresh();
}

void schedule_config_refresh()
{
  config_ticker.once( 30 * 60 /*seconds*/, send_config );
}

} // anon

// ----------------------------------------------------------------------------

void modem_begin( )
{
  mySerial.begin(9600, SERIAL_8N1);
  config_ticker.once( 2/*seconds*/, send_config );
}

void modem_loop( )
{
  while(mySerial.available())
  {
      int ch = mySerial.read();
      yield();

      if (ch < 0)
        continue; // not expected; no data available

      if (ch == '\r')
      {
          rx_buf[ min(rx_buf_len-1, rx_buf_used) ] = '\0';
          handle_input( rx_buf );
          rx_buf_used = 0;
      }
      else if (ch != '\n')
      {
        if (rx_buf_used < rx_buf_len - 1)
          rx_buf[ rx_buf_used++ ] = ch;
      }
  }
}
