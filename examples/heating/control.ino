
#include "pegboard.h"

using node::SwitchInput;
  // still need fully qualified types in function signatures due to Arduino code mangling.

// ----------------------------------------------------------------------------

void switch_event( node::SwitchInput::Event f, const char* name ) // called in SYS context
{
  schedule_function( [=]() {
    switch (f)
    {
      case SwitchInput::FlipOpen:
          app_log.infof( "%s OFF", name );
          break;

      case SwitchInput::FlipClose:
          app_log.infof( "%s DEMAND", name );
          break;

      default:
          break;
    }
  } );
}


// ----------------------------------------------------------------------------

inline uint32_t rr1 (uint32_t x) { return (x << 31) | (x >> 1); } // roll right one


// off, stat_off, stat_on
// a given output has a time component and one or more stat components, one of which is always demanding.

class Channel : public PegBoard
{
public:
  Channel( const char* name, int output_pin, unsigned int max_sensitivity )
    : name{ name }
    , pin{ output_pin }
    , max_sensitivity{ max_sensitivity }
    , boost_secs{ 0 }
    { }

  void boost_for( int length )
  {
    boost_secs = length * 60;
  }

  const char* name;
  int pin;
  unsigned int max_sensitivity;
  int boost_secs;
};

static Channel chans[] =
{
  Channel( "Hot Water",  app::outputs::demand_hw_pin,   1 ), // sensitive to only HW stat
  Channel( "Downstairs", app::outputs::demand_ch1_pin, ~0 ), // sensitive to everything
  Channel( "Upstairs",   app::outputs::demand_ch2_pin, ~0 ),
  Channel( "Bathrooms",  app::outputs::demand_ch3_pin, ~0 ),
};
static const size_t num_chans = sizeof(chans) / sizeof(chans[0]);


uint32_t stats = 1 << 3; // perma-on

int lazy_dump( char* buf, int buf_len )
{
  char* bp = buf;
  for (auto& chan: chans)
  {
    bp += sprintf( bp, "<h2>%s ", chan.name );

   senses_t sense = chan.current_sensitivity();
    if (chan.boost_secs)
      sense = chan.max_sensitivity;

    if (sense & stats)
      bp += sprintf( bp, " -- ACTIVE" );
    else if (sense)
      bp += sprintf( bp, " -- sensitive" );
    else
      bp += sprintf( bp, " -- OFF" );

    bp += sprintf( bp, "</h2>\n" );

    bp += sprintf( bp, "<b>Curr stats: </b><samp>" );
    for (int j = 0; j < 8; j++)
      *bp++ = (stats & (1 << j)) ? j+'0' : '_';

    bp += sprintf( bp, "</samp>, <b>True sense: </b><samp>" );
    for (int j = 0; j < 8; j++)
      *bp++ = (chan.current_sensitivity() & (1 << j)) ? j+'0' : '_';

    bp += sprintf( bp, "</samp>, <b>Boost sense: </b><samp>" );
    for (int j = 0; j < 8; j++)
      *bp++ = (chan.max_sensitivity & (1 << j)) ? j+'0' : '_';
    bp += sprintf( bp, "</samp>" );
    if (chan.boost_secs > 0)
      bp += sprintf( bp, " (active for %d seconds)", chan.boost_secs);
    else
      bp += sprintf( bp, " (inactive)");

    bp += sprintf( bp, "<br>\n" );

    bp += sprintf( bp, "<b>Schedule:</b>\n<pre>" );
    bp += chan.pegboard_dump( bp, buf_len );
    bp += sprintf( bp, "\n</pre>\n" );
    bp += sprintf( bp, "<hr>\n" );
  }
  return bp-buf;
}

// ----------------------------------------------------------------------------

Ticker channel_tick_ticker;
const auto channel_tick_interval = 0.5; // ie 2Hz
void channel_tick_fn( )
{
  static bool even = false;
  even = !even;

  if (even) // ie every 1 Hz.
    for (auto& c : chans)
      if (c.boost_secs > 0)
        c.boost_secs--;


  if (!ntp.epoch_valid())
    return;

  static long int prev_epoch = ntp.epoch_time();
  long int curr_epoch = ntp.epoch_time();

  if ((curr_epoch != prev_epoch) && (ntp.epoch_secs(curr_epoch) == 0)) // we'll never be more than a minute away from the correct tick
  {
    const int dd = ntp.epoch_day( curr_epoch );
    const int hh = ntp.epoch_hrs( curr_epoch );
    const int mm = ntp.epoch_mins( curr_epoch );

    app_log.debugf( "%d - %d:%d", dd, hh, mm );

    for (auto& c : chans)
      c.tick(dd, hh, mm);
  }
  prev_epoch = curr_epoch;
}

// ------------------------------------

// XXXEDD: todo: disable all outputs while doing upgrades

Ticker demand_check_ticker;
const auto demand_check_interval = 0.1; // 10Hz, but not critical
void demand_check_fn( )
{
  static uint32_t blink = 0x00050005;
  static uint32_t blink2 = 0x1b1b1b1b;
  for (auto& c : chans)
  {
    unsigned int sense = c.current_sensitivity();
    if (c.boost_secs > 0)
      sense = c.max_sensitivity; // be sensitive to everything it can be

    if (sense & stats)
      digitalWrite( c.pin, HIGH );
    else if (c.boost_secs > 0)
      digitalWrite( c.pin, blink2 & 1 );
    else if (sense)
      digitalWrite( c.pin, blink & 1 );
    else
      digitalWrite( c.pin, LOW );
  }

  blink = rr1( blink );
  blink2 = rr1( blink2 );
};

// ------------------------------------

Ticker external_report_ticker;
const auto external_report_interval = 5.02; // fraction to try and de-synchronize with others
void external_report_fn( )
{
  static bool even = false;
  even = !even;

  uint32_t my_stats = stats;
  uint32_t my_channels = 0;

  for (auto& c : chans)
  {
    my_channels <<= 8;

    unsigned int sense = c.current_sensitivity();
    if (c.boost_secs > 0)
      my_channels |= 4;

    if (sense & my_stats)
      my_channels |= 3; // bits 1 and 2
    else if (sense)
      my_channels |= 1;
  }

  if (even)
    emoncms.thing( my_stats, my_channels );
}

// ----------------------------------------------------------------------------

SwitchInput  stat_hw( [](){ return digitalRead( app::inputs::stat_hw_pin  ); } );
SwitchInput stat_ch1( [](){ return digitalRead( app::inputs::stat_ch1_pin ); } );
SwitchInput stat_ch2( [](){ return digitalRead( app::inputs::stat_ch2_pin ); } );

void stat_update( node::SwitchInput::Event f, int index )
{
  if (f == SwitchInput::FlipClose)
    stats |= (1 << index);
  else if (f == SwitchInput::FlipOpen)
    stats &= ~(1 << index);
}

void app_setup( )
{
  pinMode( app::inputs::stat_hw_pin,  INPUT );
  pinMode( app::inputs::stat_ch1_pin, INPUT );
  pinMode( app::inputs::stat_ch2_pin, INPUT );

   stat_hw.begin( [](SwitchInput::Event f, int){ switch_event( f, "Hot Water Stat" ); stat_update(f,0); } );
  stat_ch1.begin( [](SwitchInput::Event f, int){ switch_event( f, "Heating 1 Stat" ); stat_update(f,1); } );
  stat_ch2.begin( [](SwitchInput::Event f, int){ switch_event( f, "Heating 2 Stat" ); stat_update(f,2); } );

   stat_hw.update_debounce_ms( 100 ); // thermostat inputs will probably be
  stat_ch1.update_debounce_ms( 100 ); // quite clean, but we can afford to
  stat_ch2.update_debounce_ms( 100 ); // slug the inputs at this end anyway.

  pinMode( app::outputs::demand_hw_pin,  OUTPUT );
  pinMode( app::outputs::demand_ch1_pin, OUTPUT );
  pinMode( app::outputs::demand_ch2_pin, OUTPUT );
  pinMode( app::outputs::demand_ch3_pin, OUTPUT );

  mqtt.on(  "", [](auto, auto data) { parse_cmd(data); } );

//  mqtt.on( "emon",     [](auto, auto){ emoncms.thing(); } );



  Serial.println("Mount LittleFS");
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    Serial.println("Formatting LittleFS filesystem");
    LittleFS.format();

    if (!LittleFS.begin())
      Serial.println("LittleFS mount failed again");
  }

  listDir("/");


  char file[] = "/fishX.bin";

  for (int i = 0; i <= 3; i++)
  {
    file[4] = i + '0';
    chans[i].load(file);
  }


  channel_tick_ticker.attach_scheduled( channel_tick_interval, channel_tick_fn );
    // Follows NTP and ticks the channels accordingly

  demand_check_ticker.attach_scheduled( demand_check_interval, demand_check_fn );
    // Compares channel sensitivity to current stat inputs

  external_report_ticker.attach_scheduled( external_report_interval, external_report_fn );
    // Reports to external entities every now and again

}

// ----------------------------------------------------------------------------

Channel* id_to_channel( int channel );
Channel* id_to_channel( int channel )
{
  switch (channel)
  {
    case 0: return &chans[0];
    case 1: return &chans[1];
    case 2: return &chans[2];
    case 3: return &chans[3];
    default:
      return nullptr;
  }
}

// ----------------------------------------------------------------------------

static const char *day_to_str(int day)
{
  const char *days[] = { "today", "mon", "tue", "wed", "thu", "fri", "sat", "sun", "all" };
  if (day < 0 || day > (int)(sizeof(days)/sizeof(days[0])))
    return "error";
  return days[day];
}

void cmd_now( int channel, unsigned int sensitivity, bool on_n_off )
{
  Serial.printf("c%d s%x %s now\n", channel, sensitivity, (on_n_off) ? "ON" : "OFF" );
  Channel* c = id_to_channel( channel );
  if (!c) return;

  if (on_n_off)
    c->on_now( sensitivity );
  else
    c->off_now( sensitivity );
}

void cmd_set( int channel, unsigned int sensitivity, bool on_n_off, int time, int day )
{
  if (day < 0) day = 0; // unset => today.

  Serial.printf("c%d s%x %s at %02u:%02u %s\n", channel, sensitivity, (on_n_off) ? "ON" : "OFF", time / 60, time % 60, day_to_str(day) );
  Channel* c = id_to_channel( channel );
  if (!c) return;

  if (day == 0)
  {
    if (ntp.epoch_valid())
    {
      const long int curr_epoch = ntp.epoch_time();
      const int dd = (ntp.epoch_day( curr_epoch ) + 6) % 7; // ntp has 0 == sun, but we want 0 == mon.
      printf("today; %d\n", dd);

      if (on_n_off)
        c->on_peg( dd, time, sensitivity );
      else
        c->off_peg( dd, time, sensitivity );
    }
  }
  else if (day == 8)
  {
    Serial.printf("every day\n");
    for (int i = 0; i < 7; i++)
      if (on_n_off)
        c->on_peg( i, time, sensitivity );
      else
        c->off_peg( i, time, sensitivity );
  }
  else
  {
    Serial.printf("one day %d\n", day-1);

    if (on_n_off)
      c->on_peg( day-1, time, sensitivity );
    else
      c->off_peg( day-1, time, sensitivity );
  }

  char file[] = "/fishX.bin";
  file[4] = channel + '0';
  c->save(file);
}

void cmd_boost( int channel, int time )
{
  Serial.printf("c%d boost %u\n", channel, time );
  Channel* c = id_to_channel( channel );
  if (!c) return;

  if (time >= 0)
    c->boost_for( time );
}

void cmd_delete( int channel, int time, int day )
{
  if (time >= 0)
    Serial.printf("c%d del %02u:%02u\n", channel, time / 60, time % 60 );
  else
    Serial.printf("c%d clear %s\n", channel, day_to_str(day) );
  Channel* c = id_to_channel( channel );
  if (!c) return;

  c->remove_peg( time );

  char file[] = "/fishX.bin";
  file[4] = channel + '0';
  c->save(file);
}


void listDir(const char * dirname) {
  Serial.printf("Listing directory: %s\n", dirname);

  Dir root = LittleFS.openDir(dirname);

  while (root.next()) {
    File file = root.openFile("r");
    Serial.print("  FILE: ");
    Serial.print(root.fileName());
    Serial.print("  SIZE: ");
    Serial.print(file.size());
    time_t cr = file.getCreationTime();
    time_t lw = file.getLastWrite();
    file.close();
    struct tm * tmstruct = localtime(&cr);
    Serial.printf("    CREATION: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
    tmstruct = localtime(&lw);
    Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
  }
}