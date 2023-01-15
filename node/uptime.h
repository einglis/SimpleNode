#pragma once

#include <Ticker.h>

#include "../logging.h"

namespace node {

class Uptime
{
public:
  Uptime( )
    : counter{ 0 }
  { }

  void setup( )
  {
    update_ticker.attach( 1, [this](){ counter++; } );
    report_ticker.attach( 6, [this](){ report(); } );
  }

  uint32_t secs( ) { return counter; }

private:
  uint32_t counter; // 32 bits is enough for 136 years of seconds
  Ticker update_ticker;
  Ticker report_ticker;

  void report( )
  {
    const uint32_t uptime_secs = secs();
    const uint32_t uptime_mins = uptime_secs / 60;
    const uint32_t uptime_hours = uptime_mins / 60;
    const uint32_t uptime_days = uptime_hours / 24;

    #define PLURAL(n) n, (n!=1)?"s":""
    if (uptime_days > 0)
      log.debugf( "%u day%s, ", PLURAL(uptime_days) );
    if (uptime_hours > 0)
      log.debugf( "%u hour%s", PLURAL(uptime_hours % 24) );
    else if (uptime_mins > 0)
      log.debugf( "%u minute%s", PLURAL(uptime_mins) );
    else
      log.debugf( "%u second%s", PLURAL(uptime_secs) );
    #undef PLURAL
  }

  static Logger log;
};

Logger Uptime::log( "UPTIME" );

} // node
