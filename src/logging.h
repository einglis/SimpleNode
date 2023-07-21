#pragma once

#include <WiFiUdp.h>

#include "logging.h"

namespace node {

class Syslog
{
public:

  enum
  {
    severity_debug   = 7,
    severity_info    = 6,
    severity_warning = 4,
    severity_error   = 3,
  };

  Syslog(  )
    : have_ip{ false }
    , ip{ }
    , port{ }
    , log_level{ severity_debug }
  { }

  #define DEFAULT_SYSLOG_PORT 514
  void begin( const char* syslog_ip_as_string, int syslog_port = DEFAULT_SYSLOG_PORT )
  {
    have_ip = ip.fromString( syslog_ip_as_string );
      // too early to use DNS, and don't want to do a lookup (each time) once in the main loop
    port = syslog_port;
  }

  void set_level( int new_level )
  {
    log_level = new_level;
  }

  void syslog(int level, const char *source, const char *message)
  {
    if (!have_ip) return;
    if (level > log_level) return;

    // Syslog messages should apparently carry a protocol version, various
    // identifiers, a timestamp and so on.  But it works just fine with a
    // severity and a message, and really that's all we want or need.

    char header[] = { '<', (char)(level + '0'), '>', '\0' };
        // the syslog protocol allows 0..191, but we know we'll only need 0..9
        // (which implicitly means they'll all be 'kernel' messages).

    syslog_wifi.beginPacket( ip, port );
    syslog_wifi.write(header);
    syslog_wifi.write(WIFI_HOSTNAME);
    syslog_wifi.write(' ');
    syslog_wifi.write(source);
    syslog_wifi.write(": ");
    syslog_wifi.write(message);
    syslog_wifi.endPacket();
  }

  void syslog(int level, const char *source, const __FlashStringHelper *message)
  {
    if (!have_ip) return;
    if (level > log_level) return;

    char header[] = { '<', (char)(level + '0'), '>', '\0' };

    syslog_wifi.beginPacket( ip, port );
    syslog_wifi.write(header);
    syslog_wifi.write(WIFI_HOSTNAME);
    syslog_wifi.write(' ');
    syslog_wifi.write(source);
    syslog_wifi.write(": ");

    const char *message_ = (const char *)message;
    uint8_t m = pgm_read_byte(message_++);
    while (m)
    {
      syslog_wifi.write(m);
      m = pgm_read_byte(message_++);
    }
    syslog_wifi.endPacket();
  }

private:
  WiFiUDP syslog_wifi;
  bool have_ip;
  IPAddress ip;
  int port;
  int log_level;
};


class Logger
{
public:
  Logger( const char* source )
    : src{ source }
    { }

  static void use_syslog( Syslog& s )
  {
    syslog = &s;
  }

  #define DEFINE_LOG_FNS( level, tag, syslog ) \
    void level( const __FlashStringHelper* msg ) { log( F(": " tag ": "), msg, syslog ); } \
    void level( const char* msg ) { log( F(": " tag ": "), msg, syslog ); } \
    void level##f( const char* fmt, ... ) \
    { \
      va_list args; \
      va_start(args, fmt); \
      vsnprintf( temp, sizeof(temp), fmt, args); \
      va_end(args); \
      level( temp ); \
    }

  DEFINE_LOG_FNS( debug,   "DEBUG",   7 )
  DEFINE_LOG_FNS( info,    "INFO",    6 )
  DEFINE_LOG_FNS( warning, "WARNING", 4 )
  DEFINE_LOG_FNS( error,   "ERROR",   3 )

  #undef DEFINE_LOG_FNS

private:
  const char* src;
  char temp[ 128 ]; // deliberately short-ish; could probably be static...

  void log( const __FlashStringHelper* level_etc, const char* msg, int syslog_level )
  {
    // to save operations, print operations, level_etc should be of the form  ": level: "
    Serial.print( src );
    Serial.print( level_etc );
    Serial.println( msg );

    if (syslog)
      syslog->syslog( syslog_level, src, msg );
  }
  void log( const __FlashStringHelper* level_etc, const __FlashStringHelper* msg, int syslog_level )
  {
    Serial.print( src );
    Serial.print( level_etc );
    Serial.println( msg );

    if (syslog)
      syslog->syslog( syslog_level, src, msg );
  }

  static Syslog* syslog;
};

Syslog* Logger::syslog; // static member definition

} // node
