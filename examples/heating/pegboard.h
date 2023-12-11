
#pragma once

#include <iostream>
#include <map>

extern node::Logger app_log;
// ----------------------------------------------------------------------------

typedef uint8_t senses_t;

std::ostream &str_sense( std::ostream &os, senses_t s )
{
  os << ((s & 1) ? 'w' : '-');
  s >>= 1;
  for (size_t i = 1; i < sizeof(senses_t) * 8; ++i, s >>= 1)
    os << (char)((s & 1) ? i+'0' : '-');
  return os;
}

// ----------------------------------------------------------------------------

class PegBoard
{
public:
  PegBoard( )
    : curr_time{ 0 }
    , curr_day{ 0 }
    , curr_pegs{ 0 }
    { }

  void on_now( senses_t s )
  {
    curr_pegs |= s;
  }
  void off_now( senses_t s )
  {
    curr_pegs &= ~s;
  }

  void on_peg( int day, int time, senses_t s )
  {
    if (day < 0 || day >= 7) return;

    auto it = pegs.find(time);
    if (it != pegs.end())
    {
      it->second.on[day] |= s;
    }
    else
    {
      peg p;
      memset((void*)&p, 0, sizeof(p));
      p.on[day] = s;
      pegs.insert( {time, p} );
    }
  }
  void off_peg( int day, int time, senses_t s )
  {
    if (day < 0 || day >= 7) return;

    auto it = pegs.find(time);
    if (it != pegs.end())
    {
      it->second.off[day] |= s;
    }
    else
    {
      peg p;
      memset((void*)&p, 0, sizeof(p));
      p.off[day] = s;
      pegs.insert( {time, p} );
    }
  }
  void remove_peg( int time )
  {
    auto it = pegs.find(time);
    if (it != pegs.end())
      pegs.erase(it);
  }

  void apply_peg( int day, int time )
  {
    auto it = pegs.find(time);
    if (it != pegs.end())
    {
      senses_t annihilate = it->second.on[day] & it->second.off[day];
      curr_pegs |=  (it->second.on[day]  & ~annihilate);
      curr_pegs &= ~(it->second.off[day] & ~annihilate);
    }
  }

  void tick( int day, int hour, int mins )
  {
    const int new_time = hour * 60 + mins;

    if (day != curr_day || new_time < curr_time)
    {
      app_log.infof("new day or reverse time  %d %02d:%02d -> %d %02d:%02d",
        curr_day, curr_time/60, curr_time%60, day, hour, mins );

      curr_time = 0;
      curr_day = day % 7; // just in case
      curr_pegs = 0;

      apply_peg( curr_day, curr_time ); // ie 00:00
    }

    while (curr_time < new_time)
    {
      curr_time++;
      apply_peg( curr_day, curr_time );
        // as pegs accumulate, we might have glitches in the control outputs, but they'll
        // be too brief to matter, and a large ffwd is not really expected ever anyway.
    }
  }

  senses_t current_sensitivity() const
  {
    return curr_pegs;
  }

  void report() const
  {
    printf("at %02u:%02u,  ", curr_time/60, curr_time%60);

    std::cout << "pegs: ";
    str_sense(std::cout, curr_pegs);
    std::cout << "  -->  ";
    str_sense(std::cout, current_sensitivity());

    std::cout << std::endl;
  }

  void save(const char *filename)
  {
    dump();

    Serial.println(filename);
    File file = LittleFS.open(filename, "w");
    if (!file)
    {
      Serial.println("Failed to open file for writing");
      // XXXEDD: report error by MQTT?
      return;
    }

    for (auto [time, p]: pegs)
    {
      uint8_t buf[16] = { 0 };
      uint8_t *bp = &buf[0];
      *bp++ = time / 60;
      *bp++ = time % 60;
      for (int i = 0; i < 7; i++)
      {
        *bp++ = p.on[i];
        *bp++ = p.off[i];
      }
      if (file.write(buf, 16))
        Serial.println("Record written");
      else
        Serial.println("Write failed");
    }
    file.close();
  }

  void load(const char *filename)
  {
    Serial.println(filename);
    File file = LittleFS.open(filename, "r");
    if (!file)
    {
      Serial.println("Failed to open file for reading");
      // XXXEDD: report error by MQTT?
      return;
    }
    while (file.available() >= 16)
    {
      uint8_t buf[16] = { 0 };
      file.read(buf, 16);

      int time = (int)buf[0] * 60 + buf[1];
      uint8_t *bp = &buf[2];

      peg p;
      memset((void*)&p, 0, sizeof(p));

      for (int i = 0; i < 7; i++)
      {
        p.on[i] = *bp++;
        p.off[i] = *bp++;
      }

      pegs.insert( {time, p} );
    }
    if (file.available() > 0)
      Serial.println("bytes remain in the file");
    file.close();

    dump();
  }


  void dump( std::ostream& os = std::cout ) const
  {
    for (auto [time, peg]: pegs)
    {
      os << " " << time/60 << ":" << time % 60;

      const char *days[] = {"mo","tu","we","th","fr","sa","su"};
      for (int i = 0; i < 7; i++)
      {
        os << " " << days[i] << ":";
        str_sense(os, peg.on[i]);
        os << "/";
        str_sense(os, peg.off[i]);
      }
      os  << std::endl;
    }
  }

  int pegboard_dump( char* buf, int buf_len )
  {
    const int max_sense = 8;
    const int day_stride = max_sense * 2 + 2 + 2 + 1;

    char sub[ day_stride*7 ];
    memset( sub, ' ', sizeof(sub)-1 );

    const char* days[] = { "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" };
    for (int i = 0; i < 7; i++)
      memcpy( sub + i*day_stride + (day_stride - strlen(days[i])) / 2, days[i], strlen(days[i]) );

    sub[ sizeof(sub)-1 ] = '\0';

    char* bp = buf;
    bp += sprintf( bp, "         %s\n", sub );

    for (auto [time, peg]: pegs)
    {
      memset( sub, ' ', sizeof(sub)-1 );

      for (int i = 0; i < 7; i++)
      {
        char* sp = sub + i*day_stride;

        *sp++ = '+';
        for (int j = 0; j < max_sense; j++)
          *sp++ = (peg.on[i] & (1 << j)) ? j+'0' : '_';
        sp++;

        *sp++ = '-';
        for (int j = 0; j < max_sense; j++)
          *sp++ = (peg.off[i] & (1 << j)) ? j+'0' : '_';
      }

      sub[ sizeof(sub)-1 ] = '\0';

      bp += sprintf( bp, " %02d:%02d : %s\n", time/60, time%60, sub );
    }

    (void)buf_len;
    return bp-buf;
  }


private:
  struct peg
  {
    senses_t on[7];
    senses_t off[7];
  };

  int curr_time;
  int curr_day;
  std::map<int, peg> pegs = { }; // peg( time )
  senses_t curr_pegs;
};