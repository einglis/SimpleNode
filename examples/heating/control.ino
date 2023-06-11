
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


// off, stat_off, stat_on
// a given output has a time component and one or more stat components, one of which is always demanding.

class Channel : public PegBoard
{
public:
  Channel( int output_pin )
    : pin{ output_pin }
    , boost_secs{ 0 }
    { }

  void boost_for( int length )
  {
    boost_secs = length * 60;
  }

  int pin;
  int boost_secs;
};

static Channel chans[] =
{
  Channel( app::outputs::demand_hw_pin ),
  Channel( app::outputs::demand_ch1_pin ),
  Channel( app::outputs::demand_ch2_pin ),
  Channel( app::outputs::demand_ch3_pin ),
};
static const size_t num_chans = sizeof(chans) / sizeof(chans[0]);


Ticker crossings_ticker;
void crossings_fn( ) // called at 2Hz
{
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

  static bool even = false;
  if (even) // every 1 Hz.
    for (auto& c : chans)
      if (c.boost_secs > 0)
        c.boost_secs--;
  even = !even;
}

// ----------------------------------------------------------------------------

SwitchInput  stat_hw( [](){ return digitalRead( app::inputs::stat_hw_pin  ); } );
SwitchInput stat_ch1( [](){ return digitalRead( app::inputs::stat_ch1_pin ); } );
SwitchInput stat_ch2( [](){ return digitalRead( app::inputs::stat_ch2_pin ); } );

uint32_t stats = 1 << 3; // perma-on


inline uint32_t rr1 (uint32_t x) { return (x << 31) | (x >> 1); } // roll right one

Ticker t1;


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

//  mqtt.on(  "hw", [](auto, auto data) { if (bool x; str_on_off( data, x ))  hw_control( x ); } );
//  mqtt.on( "ch1", [](auto, auto data) { if (bool x; str_on_off( data, x )) ch1_control( x ); } );
//  mqtt.on( "ch2", [](auto, auto data) { if (bool x; str_on_off( data, x )) ch2_control( x ); } );
//  mqtt.on( "ch3", [](auto, auto data) { if (bool x; str_on_off( data, x )) ch3_control( x ); } );



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


  crossings_ticker.attach_scheduled( 0.5, crossings_fn );
    // note: the logic of crossings_fn will need to change if the interval is adjusted.

  t1.attach_scheduled(0.1, []()
  {
    static uint32_t f0 = 0x07070707;
    for (auto& c : chans)
    {
      unsigned int sense = c.current_sensitivity();
      if (c.boost_secs > 0)
        sense = ~0;

      if (sense & stats)
        digitalWrite( c.pin, HIGH );
      else if (sense)
        digitalWrite( c.pin, f0 & 1 );
      else
        digitalWrite( c.pin, LOW );
    }

    f0 = rr1( f0 );
  } );
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
  printf("c%d s%x %s now\n", channel, sensitivity, (on_n_off) ? "ON" : "OFF" );
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

  printf("c%d s%x %s at %02u:%02u %s\n", channel, sensitivity, (on_n_off) ? "ON" : "OFF", time / 60, time % 60, day_to_str(day) );
  Channel* c = id_to_channel( channel );
  if (!c) return;

  if (day == 0)
  {
    if (ntp.epoch_valid())
    {
      const long int curr_epoch = ntp.epoch_time();
      const int dd = ntp.epoch_day( curr_epoch );
      printf("today; %d\n", dd);

      if (on_n_off)
        c->on_peg( dd, time, sensitivity );
      else
        c->off_peg( dd, time, sensitivity );
    }
  }
  else if (day == 8)
  {
    printf("every day\n");
    for (int i = 0; i < 7; i++)
      if (on_n_off)
        c->on_peg( i, time, sensitivity );
      else
        c->off_peg( i, time, sensitivity );
  }
  else
  {
    printf("one day %d\n", day-1);

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
  printf("c%d boost %u\n", channel, time );
  Channel* c = id_to_channel( channel );
  if (!c) return;

  if (time >= 0)
    c->boost_for( time );
}

void cmd_delete( int channel, int time, int day )
{
  if (time >= 0)
    printf("c%d del %02u:%02u\n", channel, time / 60, time % 60 );
  else
    printf("c%d clear %s\n", channel, day_to_str(day) );
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