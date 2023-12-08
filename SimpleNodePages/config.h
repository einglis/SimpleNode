#pragma once

#include <ESPAsyncWebServer.h>

namespace webpages {

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

// ------------------------------------

void handle_raw_bytes( AsyncWebServerRequest* request, const uint8 *data, size_t data_len )
{
  auto hexeroo = std::make_shared<Hexorator<16>>( data, data_len );
  AsyncWebServerResponse *response = request->beginChunkedResponse("text/plain",
    [hexeroo]( uint8_t *buf, size_t buf_len, size_t ) { return hexeroo->render_next( (char *)buf, buf_len ); }
  );
  request->send(response);
}

// ----------------------------------------------------------------------------

void register_config( node::Webserver &web, const uint8 *data, size_t data_len )
{
  web.add_handler( "/config", HTTP_GET,
    [=]( AsyncWebServerRequest *r ){ handle_raw_bytes( r, data, data_len ); }
  );
}

} // webpages
