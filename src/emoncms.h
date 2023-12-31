#pragma once

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include "logging.h"
#include "wifi.h"

namespace node {

#define DEFAULT_EMONCMS_PORT 80

// ----------------------------------------------------------------------------

class EmonCms
{
public:
  EmonCms( )
    { }

  void begin( const char* emoncms_host, const char* api_key )
  {
    (void)emoncms_host;
    (void)api_key;
  }

  void thing( uint32_t stats, uint32_t channels )
  {

    // http.begin(my_wifi, "http://" EMONCMS_HOST, DEFAULT_EMONCMS_PORT );

    // int httpCode = http.sendRequest("GET", "/input/post?" "apikey=" EMONCMS_API_KEY "&node=123" "&csv=1,2,3");

    //bool begin_rc = http.begin(my_wifi, "http://" EMONCMS_HOST "/input/post?" "apikey=" EMONCMS_API_KEY "&node=123" "&csv=1,2,3" );

#if 0
    bool begin_rc = http.begin(my_wifi, EMONCMS_HOST, DEFAULT_EMONCMS_PORT, "/input/post?" "apikey=" EMONCMS_API_KEY "&node=123" "&csv=1,2,3", false /*not https*/ );
    int httpCode = http.GET();
#else

    bool begin_rc = http.begin(my_wifi, EMONCMS_HOST, DEFAULT_EMONCMS_PORT, "/input/post", false /*not https*/ );
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
//    http.setReuse(false); // no keepalive

    //String post = "node=123" "&apikey=" EMONCMS_API_KEY "&csv=1,2,,,5";
    String post = "node=boiler_ctrl" "&apikey=" EMONCMS_API_KEY "&fulljson={" \
      "\"stats\":";

    post += stats;
    post += ",\"channels\":";
    post += channels;
    post += "}";


    int httpCode = http.POST( post );

    String payload = http.getString();
          Serial.println(payload);

#endif

    log.infof("http: %d %d -", begin_rc, httpCode);
    log.infof("%s",payload.c_str());

    http.end();

//     Serial.print("[HTTP] POST...\n");
//     // start connection and send HTTP header and body
//     int httpCode = http.POST("{\"hello\":\"world\"}");



// static void my_callback (byte status, word off, word len) {
//   int lsize =   get_reply_data(off);

//   if (strcmp(line_buf,"ok")==0)
//   {
//     Serial.println("OK recieved"); ethernet_requests = 0; ethernet_error = 0;
//   }

//         str.reset();
//         str.print(basedir); str.print("/input/post?");
//         str.print("apikey="); str.print(apikey);
//         str.print("&node=");  str.print(node_id);
//         str.print("&csv=");
//         for (byte i=0; i<n; i+=2)
//         {
//           int num = ((unsigned char)rf12_data[i+1] << 8 | (unsigned char)rf12_data[i]);
//           if (i) str.print(",");
//           str.print(num);
//         }
//         if (node_id==12)
//         {
//           const int ch = (rf12_data[0] & 0xa0) >> 5;
//           str.print(","); str.print(ch);
//           const int hw = (rf12_data[0] & 0x50) >> 4;
//           str.print(","); str.print(hw);
//         }

//         str.print("\0");  //  End of json string



// const char website[] PROGMEM = "emoncms.shorlandoaks.com";
//     ether.browseUrl(PSTR("") ,str.buf, website, my_callback);

  }

private:
  WiFiClient my_wifi;
  HTTPClient http; // not sure if we should dynamically allocate this each time?

  static Logger log;
};

} // node
