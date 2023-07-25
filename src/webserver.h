#pragma once

#include <ESPAsyncWebServer.h>

#include "logging.h"
#include "wifi.h"

namespace node {

class Webserver
  : public node::WifiObserver
{
public:
  Webserver()
    : server( WEBSERVER_PORT/*lazy*/ )
    { }

  void begin( )
  {
    server.onNotFound( [](auto request) { request->send( 404, "text/plain", "Not found" ); } );
    wifi_observer_register( *this );
  }

  void add_handler( const char* uri, WebRequestMethod method, ArRequestHandlerFunction fn, ArUploadHandlerFunction upload = nullptr )
  {
    handlers.push_back( server.on( uri, method, fn, upload ) );
  }

  virtual void wifi_down() // WifiObserver
  {
    log.info( F("stopping") );
    server.end();
  }

  virtual void wifi_got_ip( IPAddress ) // WifiObserver
  {
    log.info( F("starting") );
    server.begin();
  }

private:
  AsyncWebServer server;
  std::vector< AsyncCallbackWebHandler > handlers;
    // raw ptrs rather than shared mostly to cut down on syntactic noise
    // but it's irrelevant anyway, since we never expect these to be deleted.

  static Logger log;
};

Logger Webserver::log( "WEB" );

} // node
