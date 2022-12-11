#pragma ONCE

class Logger
{
public:
  Logger( const char* source )
    : src{ source }
    { }
  
  #define DEFINE_LOG_FNS( level, tag ) \
    void level( const __FlashStringHelper* msg ) { log( F(": " tag ": "), msg ); } \
    void level( const char* msg ) { log( F(": " tag ": "), msg ); } \
    void level##f( const char* fmt, ... ) \
    { \
      va_list args; \
      va_start(args, fmt); \
      vsnprintf( temp, sizeof(temp), fmt, args); \
      va_end(args); \
      level( temp ); \
    }

  DEFINE_LOG_FNS( debug,   "DEBUG" )
  DEFINE_LOG_FNS( info,    "INFO" )
  DEFINE_LOG_FNS( warning, "WARNING" )
  DEFINE_LOG_FNS( error,   "ERROR" )

  #undef DEFINE_LOG_FNS

private:
  const char* src;
  char temp[ 128 ]; // deliberately short-ish; could probably be static...

  void log( const __FlashStringHelper* level_etc, const char* msg )
  {
    // to save operations, print operations, level_etc should be of the form  ": level: " 
    Serial.print( src );
    Serial.print( level_etc );
    Serial.println( msg );
  }
  void log( const __FlashStringHelper* level_etc, const __FlashStringHelper* msg )
  {
    Serial.print( src );
    Serial.print( level_etc );
    Serial.println( msg );
  }
};
