
#include "app_config.h"
#include "common.h"

#include <SimpleNode.h>

#include "pegboard.h"

// ----------------------------------------------------------------------------

static uint32_t sense_pattern = 0xFF00FF00; // when sensitive to something but no active stats
static uint32_t boost_pattern = 0x33333333; // when on boost but no relevant stats

inline uint32_t rr1 (uint32_t x) { return (x << 31) | (x >> 1); } // roll right one

// ----------------------------------------------------------------------------

class Channel : public PegBoard
{
public:
  Channel( const char* name, int output_pin, int led_pin, senses_t max_sensitivity )
    : name{ name }
    , pin{ output_pin }
    , led{ led_pin }
    , curr_state{ Inactive }
    , max_sensitivity{ max_sensitivity }
    , boost_secs{ 0 }
    { }

  void boost_for( int length ) { boost_secs = length * 60; }
  void second_tick( ) { if (boost_secs > 0) boost_secs--; }

  enum channel_state { Inactive, Active, Sensitive, Boost, BoostSensitive };
  channel_state state() { return curr_state; }

  const char* const name;

  void refresh( const uint32_t stats )
  {
    const uint32_t sense = (boost_secs > 0) ? max_sensitivity : current_sensitivity();

    if (sense & stats) // a sensitive stat is active
    {
      digitalWrite( pin, HIGH );
      digitalWrite( led, HIGH );
      curr_state = (boost_secs > 0) ? Boost : Active;
    }
    else if (boost_secs > 0) // we're on boost (and implicitly no relevant stat is active)
    {
      digitalWrite( pin, LOW );
      digitalWrite( led, boost_pattern & 1 );
      curr_state = BoostSensitive;
    }
    else if (sense) // we're sensitive to something (and implicitly no relevant stat is active)
    {
      digitalWrite( pin, LOW );
      digitalWrite( led, sense_pattern & 1 );
      curr_state = Sensitive;
    }
    else // off
    {
      digitalWrite( pin, LOW );
      digitalWrite( led, LOW );
      curr_state = Inactive;
    }
  }

private:
  const int pin;
  const int led;
  channel_state curr_state;
  senses_t max_sensitivity;
  int boost_secs;

  friend int lazy_dump( char* buf, int buf_len );
};

static Channel chans[] =
{
  Channel( "Hot Water",  app::outputs::demand_hw_pin, app::outputs::demand_hw_led,   1 ), // sensitive to only HW stat
  Channel( "Downstairs", app::outputs::demand_ch1_pin, app::outputs::demand_ch1_led, ~0 ), // sensitive to everything
  Channel( "Upstairs",   app::outputs::demand_ch2_pin, app::outputs::demand_ch2_led, ~0 ),
  Channel( "Bathrooms",  app::outputs::demand_ch3_pin, app::outputs::demand_ch3_led, ~0 ),
};

// ----------------------------------------------------------------------------

class Stat
{
public:
  Stat( const char* name, char id, std::function<int()> input_fn )
    : name{ name }
    , id{ id }
    , bit{ 1 << num_stats }
    , demand{ input_fn }
    {
      ++num_stats;
    }

  const char* const name;
  const char id;
  const int bit;
  std::function<int()> demand;

public:
  static int num_stats;
};

static Stat stats[] =
{
  Stat( "Hot Water", 'w', [](){ return digitalRead( app::inputs::stat_hw_pin ); } ),
  Stat( "Heating Main", '1', [](){ return digitalRead( app::inputs::stat_ch1_pin ); } ),
  Stat( "Heating Aux", '2', [](){ return digitalRead( app::inputs::stat_ch2_pin ); } ),
  Stat( "Always On", 'a', [](){ return true; } ),
};
int Stat::num_stats = 0;

// ----------------------------------------------------------------------------

class SystemState
{
public:
  SystemState( )
    : curr_stats{ 0 }
    , state_string{ 0 }
    { }

  uint32_t update_stats( )
  {
    uint32_t new_stats = 0;
    for (auto& s : stats)
      if (s.demand())
        new_stats |= s.bit;

    if (const uint32_t stats_delta = new_stats ^ curr_stats)
    {
      for (auto& s : stats)
        if (stats_delta & s.bit)
          app_log.infof( "%s %s", s.name, (new_stats & s.bit) ? "DEMAND" : "OFF" );
    }

    return curr_stats = new_stats;
  }

  void report_changes( )
  {
      char buf[32]; // need 6 + 6 + 6 + 2 + 1 = 21
      char* bp = &buf[0];

      *bp++ = 'S';
      for (auto& s : stats)
        *bp++ = s.demand() ? s.id : '-';
      *bp++ = ' ';

      *bp++ = 'C';
      for (auto& c : chans)
      {
         auto state = c.state();
         switch (state)
         {
            case Channel::Active: *bp++ = 'A'; break;
            case Channel::Sensitive: *bp++ = 'a'; break;
            case Channel::Boost: *bp++ = 'B'; break;
            case Channel::BoostSensitive: *bp++ = 'b'; break;
            case Channel::Inactive:
            default: *bp++ = '-'; break;
         }
      }
       *bp++ = ' ';

      *bp++ = 'V';
      *bp++ = '-'; // close, opening, open, closing
      *bp++ = '-';
      *bp++ = '-';
      *bp++ = '-';
      *bp++ = ' ';

      *bp++ = 'D';
      *bp++ = '-'; // off, waiting, on, overrun
      *bp++ = '\0';

      // XXXEDD: todo, every now and again also; and rate limit

      if (strcmp( buf, state_string ))
      {
        strcpy( state_string, buf );
        Serial.print( buf );
        mqtt.publish( "status", buf );
      }
  }

private:
    uint32_t curr_stats;
    char state_string[32];
};

SystemState system_state;


int lazy_dump( char* buf, int buf_len )
{
  // char* bp = buf;
  // for (auto& chan: chans)
  // {
  //   bp += sprintf( bp, "<h2>%s ", chan.name );

  //   auto state = chan.state();
  //   switch (state)
  //   {
  //     case Channel::Active:
  //       bp += sprintf( bp, " -- ACTIVE" );
  //       break;
  //     case Channel::Boost:
  //       bp += sprintf( bp, " -- On boost" );
  //       break;
  //     case Channel::BoostSensitive:
  //       bp += sprintf( bp, " -- Boost sensitive" );
  //       break;
  //     case Channel::Sensitive:
  //       bp += sprintf( bp, " -- Sensitive" );
  //       break;
  //     case Channel::Inactive:
  //       bp += sprintf( bp, " -- OFF" );
  //       break;
  //     default:
  //       bp += sprintf( bp, " -- unknown state %u", (unsigned int)state );
  //       break;
  //   }

  //   bp += sprintf( bp, "<b>Curr stats: </b><samp>" );
  //   for (int j = 0; j < 8; j++)
  //     *bp++ = (curr_stats & (1 << j)) ? j+'0' : '_';

  //   bp += sprintf( bp, "</samp>, <b>True sense: </b><samp>" );
  //   for (int j = 0; j < 8; j++)
  //     *bp++ = (chan.current_sensitivity() & (1 << j)) ? j+'0' : '_';

  //   bp += sprintf( bp, "</samp>, <b>Boost sense: </b><samp>" );
  //   for (int j = 0; j < 8; j++)
  //     *bp++ = (chan.max_sensitivity & (1 << j)) ? j+'0' : '_';
  //   bp += sprintf( bp, "</samp>" );
  //   if (chan.boost_secs > 0)
  //     bp += sprintf( bp, " (active for %d seconds)", chan.boost_secs);
  //   else
  //     bp += sprintf( bp, " (inactive)");

  //   bp += sprintf( bp, "<br>\n" );

  //   bp += sprintf( bp, "<b>Schedule:</b>\n<pre>" );
  //   bp += chan.pegboard_dump( bp, buf_len );
  //   bp += sprintf( bp, "\n</pre>\n" );
  //   bp += sprintf( bp, "<hr>\n" );
  // }
  // return bp-buf;
  return 0;
}

// ----------------------------------------------------------------------------

node::Ticker channel_tick_ticker;
const int channel_tick_interval_ms = 500; // ie 2Hz
void channel_tick_fn( )
{
  static bool even = false;
  even = !even;

  if (even) // ie every 1 Hz.
    for (auto& c : chans)
      c.second_tick();


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
      c.minute_tick(dd, hh, mm);
  }
  prev_epoch = curr_epoch;
}

// ------------------------------------



node::Ticker demand_check_ticker; // XXXEDD: IsrTicker?
const int demand_check_interval_ms = 99; // 10Hz ish, but not critical
void demand_check_fn( )
{
  uint32_t new_stats = system_state.update_stats( );
    // by simply reading the raw inputs at a modest pace, we avoid any faff of
    // debouncing; if any creeps through, the valve processing has a sufficiently
    // slow response for it not to matter anyway.

  for (auto& c : chans)
    c.refresh( new_stats );

  system_state.report_changes( );

  // cycle the LED blinky patterns
  sense_pattern = rr1( sense_pattern );
  boost_pattern = rr1( boost_pattern );
};

// ------------------------------------

node::Ticker external_report_ticker;
const int external_report_interval_ms = 5.02 * 1000; // fraction to try and de-synchronize with others
void external_report_fn( )
{
  // static bool even = false;
  // even = !even;

  // if (even)
  // {
  //   uint32_t my_stats = curr_stats;
  //   uint32_t my_channels = 0;

  //   for (auto& c : chans)
  //   {
  //     my_channels <<= 8;

  //     unsigned int sense = c.current_sensitivity();
  //     if (c.boost_secs > 0)
  //       my_channels |= 4;

  //     if (sense & my_stats)
  //       my_channels |= 3; // bits 1 and 2
  //     else if (sense)
  //       my_channels |= 1;
  //   }

  //   emoncms.thing( my_stats, my_channels );
  // }
  // else
  // {
  //     char buf[32]; // need 20
  //     char* bp = &buf[0];

  //     *bp++ = 'S';
  //     *bp++ = (curr_stats & 1) ? 'W' : '-';
  //     *bp++ = (curr_stats & 2) ? '1' : '-';
  //     *bp++ = (curr_stats & 4) ? '2' : '-';
  //     *bp++ = ' ';

  //     *bp++ = 'C';
  //     for (auto& c : chans)
  //     {
  //       const uint32_t sense = c.current_sensitivity();
  //       const uint32_t boost_sense = (c.boost_secs > 0) ? c.max_sensitivity : 0;

  //       if (curr_stats & sense) *bp++ = 'O'; // on
  //       else if (curr_stats & boost_sense) *bp++ = 'B'; // boosted
  //       else if (boost_sense) *bp++ = 'b'; // could be boosted
  //       else if (sense) *bp++ = 'o'; // could be on
  //       else *bp++ = '-'; // off
  //     }
  //     *bp++ = ' ';

  //     *bp++ = 'V';
  //     *bp++ = '-'; // close, opening, open, closing
  //     *bp++ = '-';
  //     *bp++ = '-';
  //     *bp++ = '-';
  //     *bp++ = ' ';

  //     *bp++ = 'D';
  //     *bp++ = '-'; // off, waiting, on, overrun
  //     *bp++ = '\0';

  //     mqtt.publish( "status", buf );
  // }
}

// ----------------------------------------------------------------------------

void app_setup( )
{
  pinMode( app::inputs::stat_hw_pin,  INPUT );
  pinMode( app::inputs::stat_ch1_pin, INPUT );
  pinMode( app::inputs::stat_ch2_pin, INPUT );

  pinMode( app::outputs::demand_hw_pin,  OUTPUT );
  pinMode( app::outputs::demand_ch1_pin, OUTPUT );
  pinMode( app::outputs::demand_ch2_pin, OUTPUT );
  pinMode( app::outputs::demand_ch3_pin, OUTPUT );

  pinMode( app::outputs::demand_hw_led,  OUTPUT );
  pinMode( app::outputs::demand_ch1_led, OUTPUT );
  pinMode( app::outputs::demand_ch2_led, OUTPUT );
  pinMode( app::outputs::demand_ch3_led, OUTPUT );

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

#if ESP8266
  void listDir(const char * dirname);
  listDir("/");
#endif

  char file[] = "/fishX.bin";

  for (int i = 0; i <= 3; i++)
  {
    file[4] = i + '0';
    chans[i].load(file);
  }

  channel_tick_ticker.repeat( channel_tick_interval_ms, channel_tick_fn );
    // Follows NTP and ticks the channels accordingly

  demand_check_ticker.repeat( demand_check_interval_ms, demand_check_fn );
    // Compares channel sensitivity to current stat inputs

  external_report_ticker.repeat( external_report_interval_ms, external_report_fn );
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
  if (day < 0) day = 8; // unset => all.

  if (time >= 0)
    Serial.printf("c%d del %02u:%02u, %s\n", channel, time / 60, time % 60, day_to_str(day) );
  else
    Serial.printf("c%d clear %s\n", channel, day_to_str(day) );
  Channel* c = id_to_channel( channel );
  if (!c) return;

  if (day == 0)
  {
    if (ntp.epoch_valid())
    {
      const long int curr_epoch = ntp.epoch_time();
      const int dd = (ntp.epoch_day( curr_epoch ) + 6) % 7; // ntp has 0 == sun, but we want 0 == mon.
      printf("today; %d\n", dd);

      if (time >= 0)
        c->remove_peg( dd, time );
      else
        c->remove_pegs( dd );
    }
  }
  else if (day == 8)
  {
    Serial.printf("every day\n");
    for (int i = 0; i < 7; i++)
      if (time >= 0)
        c->remove_peg( i, time );
      else
        c->remove_pegs( i );
  }
  else
  {
    Serial.printf("one day %d\n", day-1);

    if (time >= 0)
      c->remove_peg( day-1, time );
    else
      c->remove_pegs( day-1 );
  }

  char file[] = "/fishX.bin";
  file[4] = channel + '0';
  c->save(file);
}


#if ESP8266

void listDir(const char * dirname) {
  Serial.printf("Listing directory: %s\n", dirname);

  Dir root = LittleFS.oppenDir(dirname);

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

#endif
