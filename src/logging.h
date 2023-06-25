#pragma once

#include <WiFiUdp.h>

#include "logging.h"

namespace node {

class Logger
{
public:
  Logger( const char* source )
    : src{ source }
    { }
  
  #define DEFINE_LOG_FNS( level, tag ) \
    void level( const __FlashStringHelper* msg ) { log( F(": " tag ": "), msg ); } \
    void level( const char* msg ) { log( F(": " tag ": "), msg ); } \
    void level##f( const char* fmt, ... ) \
    { \
      va_list args; \
      va_start(args, fmt); \
      vsnprintf( temp, sizeof(temp), fmt, args); \
      va_end(args); \
      level( temp ); \
    }

  DEFINE_LOG_FNS( debug,   "DEBUG" )
  DEFINE_LOG_FNS( info,    "INFO" )
  DEFINE_LOG_FNS( warning, "WARNING" )
  DEFINE_LOG_FNS( error,   "ERROR" )

  #undef DEFINE_LOG_FNS

private:
  const char* src;
  char temp[ 128 ]; // deliberately short-ish; could probably be static...

  void log( const __FlashStringHelper* level_etc, const char* msg )
  {
    // to save operations, print operations, level_etc should be of the form  ": level: " 
    Serial.print( src );
    Serial.print( level_etc );
    Serial.println( msg );

    syslog(1, src, msg);
  }
  void log( const __FlashStringHelper* level_etc, const __FlashStringHelper* msg )
  {
    Serial.print( src );
    Serial.print( level_etc );
    Serial.println( msg );

    syslog(1, src, "FlashStringHelper");
  }

  static WiFiUDP syslog_wifi;

  static void syslog(int level, const char *source, const char *message)
  {
    //if (WiFi.status() != WL_CONNECTED)
    //  return;

    // Syslog messages should apparently carry a protocol version, various
    // identifiers, a timestamp and so on.  But it works just fine with a
    // severity and a message, and really that's all we want or need.

    char header[] = { '<', (char)(level + '0'), '>', '\0' };
        // the syslog protocol allows 0..191, but we know we'll only need 0..9
        // (which implicitly means they'll all be 'kernel' messages).

    syslog_wifi.beginPacket(IPAddress(10,23,1,111), 514);
    syslog_wifi.write(header);
    syslog_wifi.write(WIFI_HOSTNAME);
    syslog_wifi.write(' ');
    syslog_wifi.write(source);
    syslog_wifi.write(": ");
    syslog_wifi.write(message);
    syslog_wifi.endPacket();
  }
};

WiFiUDP Logger::syslog_wifi;

} // node
