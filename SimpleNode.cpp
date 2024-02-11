
#include <SimpleNode.h>

namespace node {

Logger config_log( "CONFIG" );

// static member definitions
Logger EmonCms::log( "EMON" );
Logger Mqtt::log( "MQTT" );
Logger Ntp::log( "NTP" );
Logger Uptime::log( "UPTIME" );
Logger Webserver::log( "WEB" );

Logger WiFi::log( "WIFI" );
std::vector< std::reference_wrapper< WifiObserver > > WiFi::observers;
WifiObserver::~WifiObserver( ) { }
  // pure virtual destructor implementation

Syslog* Logger::syslog;

} // node



#include <Schedule.h>
void defer_to_loop(const std::function<void(void)>& fn)
{
    (void)schedule_function(fn);
}
