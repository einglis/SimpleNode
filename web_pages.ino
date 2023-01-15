
namespace webpages {

void handle_default( AsyncWebServerRequest* request )
{
String message;
message += "Build ";
message += Version;
message += "\n";
message += "Uptime ";
message += uptime.secs();
message += " seconds\n";

request->send(200, "text/plain", message);
}

// ----------------------------------------------------------------------------

const char form[] PROGMEM =
  "<html><body>"
  //"<form method='POST' action='update'>"
  //"<input name='fo' value='http://mqtt:80/api/arduino/simple_node.ino.generic.bin' id='filename'>"
  //"<input type='submit'>"
  //"</form>"
  "<form method='POST' enctype='multipart/form-data' action='update'>"
  "<input name='ff' value='' id='filename' type='file'>"
  "<input type='submit'>"
  "</form>"
  "</body></html>"
;

void handle_update( AsyncWebServerRequest* request )
{
  request->send_P( 200, "text/html", form );
}

const char reset_form[] PROGMEM =
  "<html><body>"
  "Success"
  "<form method='POST' action='reboot'>"
  "<input type='submit' value='Reboot'>"
  "</form>"
  "</body></html>"
;

// ----------------------------------------------------------------------------

#define PARAM_MESSAGE "message"

void handle_get_demo( AsyncWebServerRequest* request )
{
  int params = request->params();
  for(int i=0;i<params;i++){
    AsyncWebParameter* p = request->getParam(i);
    if(p->isFile()){ //p->isPost() is also true
      Serial.printf("FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
    } else if(p->isPost()){
      Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    } else {
      Serial.printf("GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
    }
  }

  // Send a GET request to <IP>/get?message=<message>
  String message;
  if (request->hasParam(PARAM_MESSAGE))
    message = request->getParam(PARAM_MESSAGE)->value();
  else
    message = "No message sent";

  request->send(200, "text/plain", "Hello, GET: " + message);
}

void handle_post_demo( AsyncWebServerRequest* request )
{
  // Send a POST request to <IP>/post with a form field message set to <message>
  String message;
  if (request->hasParam(PARAM_MESSAGE, true/*is_post*/))
    message = request->getParam(PARAM_MESSAGE, true)->value();
  else
    message = "No message sent";

  request->send(200, "text/plain", "Hello, POST: " + message);
}

#undef PARAM_MESSAGE

// ----------------------------------------------------------------------------

#ifdef NODE_HAS_WEB_UPDATE

node::Logger update_log( "UPDATE" );

void handle_update_done( AsyncWebServerRequest* request )
{
  AsyncWebServerResponse *response = request->beginResponse(200, "text/html", Update.hasError() ? "failed" : reset_form);
  response->addHeader("Connection", "close");
  request->send(response);
}

void handle_update_progress( AsyncWebServerRequest* request, String filename, size_t index, uint8_t *data, size_t len, bool final )
{
  // this is knowingly very fragile; copied from an example to
  // avoid getting hung up on this rather than the actual project
  (void)request;

  if (index == 0)
  {
      update_log.infof("start: %s", filename.c_str());
      Update.runAsync(true);
      if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000))
      {
        update_log.error( F("failed to start") );
        Update.printError(Serial);
      }
  }

  update_log.debugf("data; index %u, len %u", index, len );
  if (!Update.hasError())
  {
    if (Update.write(data, len) != len)
    {
      update_log.error( F("failed to write") );
      Update.printError(Serial);
    }
  }

  if (final)
  {
    if (Update.end(true))
    {
      update_log.info( F("success") );
    }
    else
    {
      update_log.error( F("failed to complete") );
      Update.printError(Serial);
    }
  }
}

void handle_reboot( AsyncWebServerRequest* request )
{
  request->send(200, "text/plain", "Rebooting..." );
  schedule_function( [](){ delay(500); ESP.restart(); } );
}

#endif

// ----------------------------------------------------------------------------

#ifdef NODE_HAS_CONFIG

template< size_t max_row_len >
class Hexorator
{
public:
  Hexorator( const uint8_t* data, size_t data_len )
    : data{ data }
    , data_len{ data_len }
    , addr{ 0 }
  { }

  size_t render_next( char* buf, size_t buf_len )
  {
    if (buf_len < max_buf_len) // this would be quite unexpected...
      return 0; // ...so just bail

    if (!data || !data_len)
      return 0; // no more to do anyway

    char* bp = buf;

    const size_t row_len = min(data_len, max_row_len);
    const uint8_t* dp;
    size_t col;

    // offset
    bp += sprintf( bp, "0x%04x   ", addr ); // addr + 3 spaces between sections

    // hex
    for (col = 0, dp = data; col < row_len && col < (max_row_len+1) / 2; ++col, ++dp)
        bp += sprintf( bp, "%02x ", *dp );
    bp += sprintf( bp, " " ); // 1 space halfway
    for ( ; col < row_len && col < max_row_len; ++col, ++dp)
        bp += sprintf( bp, "%02x ", *dp );
    for ( ; col < max_row_len; ++col)
        bp += sprintf( bp, "   " ); // spaces (if it's an incomplete row); we can tolerate the inefficiency

    bp += sprintf( bp, "  " ); // 2 spaces between sections

    // ascii
    for (col = 0, dp = data; col < row_len && col < (max_row_len+1) / 2; ++col, ++dp)
        bp += sprintf( bp, "%c", safe_ascii(*dp) );
    bp += sprintf( bp, " " ); // 1 space halfway
    for ( ; col < row_len && col < max_row_len; ++col, ++dp)
        bp += sprintf( bp, "%c", safe_ascii(*dp) );
    // no need to complete an incomplete row.

    *bp++ = '\n';
      // We don't actually need the termination here, so overwrite with a handy
      // linefeed.  Still need the increment though, to make the length correct

    data_len -= row_len;
    data += row_len;
    addr += row_len;

    return bp-buf;
  }

private:
  char safe_ascii( uint8_t c ) { return (' ' <= c && c <= '~') ? c : '.'; };

  static constexpr size_t max_buf_len = 6 + 3 * max_row_len + 1 * max_row_len + 7 + 1;
    // row is 0xaddr hex decode
    // addr is 6 bytes, hex is 3 per byte (incl. space), decode is 1 per byte (no escaping)
    // and there are 3 + 1 + 2 + 1 spaces/padding, with 1 for termination

  const uint8_t* data;
  size_t data_len;
  uint16_t addr;
};

void handle_config( AsyncWebServerRequest* request )
{
  auto hexeroo = std::make_shared<Hexorator<16>>( (const uint8_t*)&configuration(), sizeof(AppConfig) );
  AsyncWebServerResponse *response = request->beginChunkedResponse("text/plain",
    [hexeroo]( uint8_t *buf, size_t buf_len, size_t ) { return hexeroo->render_next( (char *)buf, buf_len ); }
  );
  request->send(response);
}

#endif

}; // webpages

#if defined(NODE_HAS_WEB_UPDATE) || defined(NODE_HAS_OTA_UPDATE)
#include "update_helpers.h"
static MyUpdaterHashClass muh;
static MyUpdaterVerifyClass muv;
#endif

void register_web_pages( node::Webserver& web)
{
  web.add_handler( "/", HTTP_GET, webpages::handle_default );

  #if defined(NODE_HAS_WEB_UPDATE) || defined(NODE_HAS_OTA_UPDATE)
  Update.installSignature( &muh, &muv );
  #endif

  #ifdef NODE_HAS_WEB_UPDATE
  web.add_handler( "/update", HTTP_GET, webpages::handle_update );
  web.add_handler( "/update", HTTP_POST, webpages::handle_update_done, webpages::handle_update_progress );
  web.add_handler( "/reboot", HTTP_POST, webpages::handle_reboot );
  #endif

  #ifdef NODE_HAS_CONFIG
  web.add_handler( "/config", HTTP_GET, webpages::handle_config );
  #endif

  web.add_handler( "/get_demo", HTTP_GET, webpages::handle_get_demo );
  web.add_handler( "/post_demo", HTTP_POST, webpages::handle_post_demo );
}
