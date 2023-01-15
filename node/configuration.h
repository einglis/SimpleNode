#pragma once

#include "../logging.h"

namespace node {

#define CONFIG_MAGIC 0x20f4da53
union ConfigHeader
{
  struct {
    uint32_t magic;
    size_t length;
  };
  uint8_t padding[32]; // gentle future proof
};

// ------------------------------------

class Configuration
{
public:
  Configuration( const char* filename = CONFIG_FILENAME )
    : filename{ filename }
    , valid{ false }
  { }

  void setup()
  {
    if (!LittleFS.begin())
      log.error( "failed to mount file system" );

    (void)load( );
  }

  bool is_valid() const { return valid; }

  AppConfig& rw() { return app_config; }
  const AppConfig& ro() { return app_config; }
  const AppConfig& operator()() { return ro(); }

  bool save( )
  {
    File cf = LittleFS.open( filename, "w" ); // "wb" not allowed, apparently
    if (!cf)
    {
      log.error( "cannot open config for writing" );
      // does not necessarily invalidate the config
      return false;
    }

    bool save_success = false;

    ConfigHeader hdr;
    memset( &hdr, 0, sizeof(hdr) );
    hdr.magic = CONFIG_MAGIC;
    hdr.length = sizeof(app_config);

    const size_t write_rc = cf.write( (const uint8_t*)&hdr, sizeof(hdr) );
    if (write_rc == sizeof(hdr))
    {
      const size_t write_rc = cf.write( (const uint8_t*)&app_config, hdr.length );
      save_success = (write_rc == hdr.length);
      if (save_success)
        log.debugf( "successfully wrote %u bytes of config", write_rc );
      else
        log.errorf( "wrote %u bytes, not %u, of config", write_rc, hdr.length );
    }
    else
    {
      log.errorf( "wrote %u bytes, not %u, of config", write_rc, sizeof(hdr) );
    }

    cf.close();

    valid = save_success; // implicitly valid once saved
    return save_success;
  }

  bool load( )
  {
    File cf = LittleFS.open( filename, "r" );
    if (!cf)
    {
      log.error( "cannot open config for reading" );
      // does not necessarily invalidate the config
      return false;
    }

    bool load_success = false;

    ConfigHeader hdr;

    const size_t read_rc = cf.read( (uint8_t*)&hdr, sizeof(hdr) );
    if (read_rc == sizeof(hdr))
    {
      if (hdr.magic == CONFIG_MAGIC)
      {
        if (hdr.length != sizeof(app_config))
          log.warningf( "length %u does not match config size %u", hdr.length, sizeof(app_config) );
            // not necessarily terminal; eg if config has changed length between versions; caveat emptor.

        hdr.length = min( hdr.length, sizeof(app_config) );
          // just in case the config has got shorter, don't want to overflow the structure

        const size_t read_rc = cf.read( (uint8_t*)&app_config, hdr.length );
        load_success = (read_rc == hdr.length);
        if (load_success)
          log.debugf( "successfully read %u bytes of config", read_rc );
        else
          log.errorf( "read %u bytes, not %u, of config", read_rc, hdr.length );
      }
      else
      {
        log.warningf( "header magic %08x not expected %08x", hdr.magic, CONFIG_MAGIC );
      }
    }
    else
    {
      log.errorf( "read %u bytes, not %u, of hdr", read_rc, sizeof(hdr) );
    }

    cf.close();

    valid = load_success;
    return load_success;
  }

private:
  const char *filename;
  AppConfig app_config;
  bool valid;

  static Logger log;
};

// ------------------------------------

Logger Configuration::log( "CONFIG" );

} //node
