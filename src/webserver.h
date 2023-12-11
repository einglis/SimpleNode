#pragma once

#include <ESPAsyncWebServer.h>

#include "logging.h"
#include "wifi.h"

namespace node {

#define DEFAULT_WEBSERVER_PORT 80

namespace web {
  // static Strings to try and avoid heap fragmentation.
  static String text_type{ "text/plain" };
  static String html_type{ "text/html" };
}

// ----------------------------------------------------------------------------

class Webserver
  : public node::WifiObserver
{
public:
  Webserver()
    : server( DEFAULT_WEBSERVER_PORT/*lazy*/ )
    { }

  void begin( )
  {
    server.onNotFound( not_found_response );
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


  static void not_found_response( AsyncWebServerRequest* r )
  {
    static String message( "Not found" );
    r->send( 404, web::text_type, message );
  }
  static void server_error_response( AsyncWebServerRequest* r )
  {
    static String message( "Server error" );
    r->send( 500, web::text_type, message );
  }

  typedef std::array<char, 4096> buf_type;
  buf_type* get_buffer( ) { return &page_buf; }

private:
  AsyncWebServer server;
  std::vector< AsyncCallbackWebHandler > handlers;
    // raw ptrs rather than shared mostly to cut down on syntactic noise
    // but it's irrelevant anyway, since we never expect these to be deleted.
  buf_type page_buf;
    // It appears that the webserver can only serve one page at a time, so
    // this should be safe.  Worst case is we get some mangled pages, which
    // does not seem like a huge heartache.  At a push, we could use a
    // semaphore to track if it's in use and return a 500 Server Error or
    // something, but that seems overkill for now.

  static Logger log;
};

Logger Webserver::log( "WEB" );

} // node
