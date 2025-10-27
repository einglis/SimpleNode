#pragma once

#include "logging.h"
#include "schedule.h"

namespace node {

class Uptime
{
public:
  Uptime( )
    : counter{ 0 }
  { }

  void begin( )
  {
    update_ticker.repeat( 1000/*ms*/, [this](){ counter++; } );
    report_ticker.repeat( 6000/*ms*/, [this](){ report(); } );
  }

  uint32_t secs( ) { return counter; }

  int friendly( char* buf, int buf_len ) // realistically, 32 bytes should be enough
  {
    const uint32_t uptime_secs  = secs();
    const uint32_t uptime_mins  = uptime_secs  / 60;
    const uint32_t uptime_hours = uptime_mins  / 60;
    const uint32_t uptime_days  = uptime_hours / 24;

    int buf_used = 0;

    #define PLURAL(n) n, (n!=1)?"s":""

    if (uptime_days > 0)
      buf_used = snprintf(buf, buf_len, "%u day%s, %u hour%s",
        PLURAL(uptime_days), PLURAL(uptime_hours % 24) );
    else if (uptime_hours > 0)
      buf_used = snprintf(buf, buf_len, "%u hour%s", PLURAL(uptime_hours % 24) );
    else if (uptime_mins > 0)
      buf_used = snprintf(buf, buf_len, "%u minute%s", PLURAL(uptime_mins) );
    else
      buf_used = snprintf(buf, buf_len, "%u second%s", PLURAL(uptime_secs) );

    #undef PLURAL

    return buf_used;
  }

private:
  uint32_t counter; // 32 bits is enough for 136 years of seconds
  node::Ticker update_ticker;
  node::Ticker report_ticker;

  void report( )
  {
    char buf[32] = { 0 };
    (void)friendly( buf, sizeof(buf) );
    log.debugf( "%s", buf );
  }

  static Logger log;
};

} // node
