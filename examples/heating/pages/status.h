#pragma once

#include <ESPAsyncWebServer.h>
using node::web::text_type;

namespace webpages {

void handle_status( AsyncWebServerRequest* request, node::Webserver& web )
{
  auto buf = web.get_buffer();
  if (!buf)
    return web.server_error_response( request );

  const char *const bend = buf->data() + buf->size();
  char* bp = buf->data();

  if (bend-bp > 0) bp += snprintf( bp, bend-bp, "I am the status page" );

  //Serial.printf("Rendered status page in %u bytes\n", bp-buf->data()+1);
  request->send( 200, text_type, buf->data() );
}

// ----------------------------------------------------------------------------

void register_status( node::Webserver& web )
{
  web.add_handler( "/status", HTTP_GET,
    [&web]( AsyncWebServerRequest* r ){ handle_status( r, web ); }
  );
}

} // webpages
