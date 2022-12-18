
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

const char* PROGMEM form = 
  "<html><body>"
  "<form method='POST' action='post_demo'>"
  "<input name='fo' value='http://mqtt:80/api/arduino/simple_node.ino.generic.bin' id='filename'>"
  "<input type='submit'>"
  "</form>"
  "<form method='POST' enctype='multipart/form-data' action='post_demo'>"
  "<input name='ff' value='' id='filename' type='file'>"
  "<input type='submit'>"
  "</form>"
  "</body></html>"
;

void handle_update( AsyncWebServerRequest* request )
{
  request->send( 200, "text/html", form );
}

// ----------------------------------------------------------------------------

#define PARAM_MESSAGE "message"

void handle_get_demo( AsyncWebServerRequest* request )
{
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

}; // webpages

void register_web_pages( Webserver& web)
{
  web.add_handler( "/", HTTP_GET, webpages::handle_default );
  web.add_handler( "/update", HTTP_GET, webpages::handle_update );

  web.add_handler( "/get_demo", HTTP_GET, webpages::handle_get_demo );
  web.add_handler( "/post_demo", HTTP_POST, webpages::handle_post_demo );
}