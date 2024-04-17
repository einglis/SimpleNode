#pragma once

#include <Ticker.h>
#include "logging.h"

namespace node {

class Ntp
{
public:
  Ntp( )
    : is_valid{ false }
  { }

  void begin( int report_interval_secs = 0 )
  {
    #ifdef ESP32
    configTime(0, 0, NTP_HOST);  // 0, 0 because we will use TZ in the next line
    setenv("TZ", NTP_TIMEZONE, 1/*overwrite*/);
    tzset();
    #elif ESP8266
    configTime(NTP_TIMEZONE, NTP_HOST);
    #endif

    if (report_interval_secs)
      report_ticker.repeat( report_interval_secs * 1000/*ms*/, [this]() { log_time( now() ); } );
  }

  bool time_valid( )
  {
    if (!is_valid)
    {
      struct tm tm = decode( now() );
      if (tm.tm_year > 2020)
      {
        // it is possible to register a callback for when an NTP update is recevied but
        // a) I had confusing hangs with that and b) the ESP32 will retain its time over
        // a restart (not reset) so it might be immediately valid, even without an update.
        log.info("Time has become plausibly valid");
        log_time( now() );
        is_valid = true;
      }
    }
    return is_valid;
  }

  static time_t now( )
  {
    return time(nullptr);
  }

  static struct tm decode( time_t t )
  {
      struct tm tm;
      localtime_r(&t, &tm);
      tm.tm_year += 1900;
      tm.tm_mon += 1;
      tm.tm_wday = (tm.tm_wday + 6) % 7;  // convert 0 == Sunday to 0 == Monday   ... my ball, my rules!
      return tm;
  }

private:
  static void log_time( time_t time )
  {
    const char* wdays[] = { "Mon", "Tues", "Wed", "Thurs", "Fri", "Sat", "Sun" };
    struct tm tm = decode( time );

    char buf[128];
    char* bp = &buf[0];
    bp += sprintf(bp, "%d-%02d-%02d", tm.tm_year, tm.tm_mon, tm.tm_mday); // date YYYY-MM-DD
    bp += sprintf(bp, ", %02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec); // time: HH:MM:SS
    bp += sprintf(bp, " (%s%s)", wdays[tm.tm_wday], tm.tm_isdst?", DST":"");

    log.info(buf);
  }

  bool is_valid;
  node::Ticker report_ticker;

  static Logger log;
};

} //node
