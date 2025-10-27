
#include <SimpleNode.h>

extern node::Mqtt mqtt;


int hex(char x)
{
  if ('0' <= x && x <= '9') return x - '0';
  else if ('A' <= x && x <= 'F') return x - 'A' + 10;
  else if ('a' <= x && x <= 'f') return x - 'a' + 10;
  return 0;
}

static int rssi = -99;
static int snr = 0;

void parse_payload( const char* msg_, int len )
{
    if (len != 14)
      return (void)Serial.println( "wrong length" );

    uint8_t msg[7];
    for (int i = 0; i < len; i+=2)
      msg[i/2] = hex(msg_[i])*16 + hex(msg_[i+1]);

    if (msg[0] != 0x38 || msg[1] != 0x05)
      return (void)Serial.println( "wrong header" );

    const int id = msg[2] << 8 | msg[3];
    const int seq = msg[4];
    const int state = msg[5];
    const int charge = msg[6];

    Serial.print( "ID: " );
    Serial.print( id, HEX );
    Serial.print( ", sequence: " );
    Serial.print( seq );
    Serial.print( ", lock: " );
    Serial.print( (state) ? "locked" : "unlocked" );
    Serial.print( ", charge: " );
    Serial.print( 100 * charge / 255 );
    Serial.println( "%" );

    char buf[128];
    char*bp = &buf[0];

    bp += sprintf( bp, "{\"msg\":" );
    bp += sprintf( bp,   "{\"state\":\"%u\",\"batt\":\"%u\"}", state, 100*charge/255 );
    bp += sprintf( bp, ",\"meta\":" );
    bp += sprintf( bp,   "{\"seq\":\"%u\",\"rssi\":\"%d\",\"snr\":\"%d\"}", seq, rssi, snr );
    bp += sprintf( bp, "}" );

    Serial.println( &buf[0] );
    //Serial.println( bp - &buf[0] );

    char topic[16];
    sprintf(topic, "%04x/status", id);
    mqtt.publish(topic, &buf[0]);

    mqtt.publish("3805/status", &buf[0]); // testing
    mqtt.publish("1234/status", &buf[0]); // testing


    rssi = -99;
    snr = 0;
}

void parse_csv( const char* msg, void(*fn)(const char* tag, int value) )
{
  // ( <tag>:<value>, )* <tag>:<value>
  char tag[32]; // more than long enough for anything we're interested in.
  char value[32]; // likewise

  while (msg && *msg)
  {
    const char* mp = msg;
    while (*mp && *mp == ' ') // skip spaces
      ++mp;
    if (!*mp)
      return; // unexpected end

    msg = mp;
    while (*mp && *mp != ':')
      ++mp;
    if (!*mp)
      return; // unexpected end

    strncpy( tag, msg, min( (int)(mp-msg), (int)sizeof(tag)-1 ) );
    tag[ min( (int)(mp-msg), (int)sizeof(tag)-1 ) ] = '\0';
    //Serial.print( "tag: " );
    //Serial.println( tag );

    msg = ++mp; // skip colon
    while (*mp && *mp != ',')
      ++mp;

    strncpy( value, msg, min( (int)(mp-msg), (int)sizeof(value)-1 ) );
    value[ min( (int)(mp-msg), (int)sizeof(value)-1 ) ] = '\0';
    //Serial.print( "value: " );
    //Serial.println( value );

    if (fn)
      fn(tag, atoi(value));

    if (!*mp)
      return; // _expected_ end

    msg = ++mp; // skip comma
  }
}

void handle_meta_tags( const char* tag, int value)
{
    if (0 == strcmp(tag, "RSSI"))
      rssi = value;
    else if (0 == strcmp(tag, "SNR"))
      snr = value;
}

void parse_message( const char* msg )
{
  // RX "..."
  // LEN:x, RSSI:-y, SNR:z
  // RXLRPKT
  // RFCFG F:...

  if (0 == strncmp(msg, "RX \"", 4))
  {
    const char* mp = msg+4;
    while (*mp != '\0' && *mp != '"')
      mp++;
    parse_payload( msg+4, mp-msg-4 );
  }
  else if (0 == strncmp(msg, "LEN:", 4))
  {
    parse_csv( msg, handle_meta_tags );
  }
}
