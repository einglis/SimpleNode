#pragma once

//#include <ESPAsyncWebServer.h>
using node::web::html_type;
using node::web::text_type;

int lazy_dump( char* buf, int buf_len );

namespace webpages {

void handle_status( AsyncWebServerRequest* request, node::Webserver& web )
{
  auto buf = web.get_buffer();
  if (!buf)
    return web.server_error_response( request );

  const char *const bend = buf->data() + buf->size();
  char* bp = buf->data();

  bp += sprintf( bp, "<html><body>\n" );
  bp += lazy_dump( bp, bend-bp );
  bp += sprintf( bp, "</body></html>\n" );

  Serial.printf("Rendered status page in %u bytes\n", bp-buf->data()+1);
  request->send( 200, html_type, buf->data() );
}

// ----------------------------------------------------------------------------

void register_status( node::Webserver& web )
{
  web.add_handler( "/status", HTTP_GET,
    [&web]( AsyncWebServerRequest* r ){ handle_status( r, web ); }
  );
}

} // webpages
