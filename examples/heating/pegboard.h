
#pragma once

#include <iostream>
#include <map>

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
    , curr_pegs{ 0 }
    , boost_until{ 0 }
    { }

  void on_now( senses_t s )
  {
    curr_pegs |= s;
  }
  void off_now( senses_t s )
  {
    curr_pegs &= ~s;
  }

  void on_peg( int time, senses_t s )
  {
    auto it = pegs.find(time);
    if (it != pegs.end())
      it->second.on |= s;
    else
      pegs.insert( {time, { s, 0 }} );
  }
  void off_peg( int time, senses_t s )
  {
    auto it = pegs.find(time);
    if (it != pegs.end())
      it->second.off |= s;
    else
      pegs.insert( {time, { 0, s }} );
  }
  void remove_peg( int time )
  {
    auto it = pegs.find(time);
    if (it != pegs.end())
      pegs.erase(it);
  }

  void boost_on( int length )
  {
    boost_until = curr_time + 1 + length;
  }
  void boost_off( )
  {
    boost_until = 0;
  }

  void midnight( )
  {
    curr_time = 0;
    curr_pegs = 0;
    boost_until = 0;
  }
  void tick( )
  {
    curr_time++;
    if (curr_time == 24*60)
      midnight();

    auto it = pegs.find(curr_time);
    if (it != pegs.end())
    {
      senses_t annihilate = it->second.on & it->second.off;
      curr_pegs |=  (it->second.on  & ~annihilate);
      curr_pegs &= ~(it->second.off & ~annihilate);
    }

    if (curr_time >= boost_until)
        boost_until = 0;
  }

  senses_t current_sensitivity() const
  {
    if (boost_until > 0)
      return ~0; // everything
    else
      return curr_pegs;
  }

  void report() const
  {
    printf("at %02u:%02u,  ", curr_time/60, curr_time%60);

    std::cout << "pegs: ";
    str_sense(std::cout, curr_pegs);
    if (boost_until > 0)
      std::cout << " + boost";

    std::cout << "  -->  ";
    str_sense(std::cout, current_sensitivity());

    std::cout << std::endl;
  }

  void save(const char *filename)
  {
    Serial.println(filename);
    File file = LittleFS.open(filename, "w");
    if (!file)
    {
      Serial.println("Failed to open file for writing");
      // XXXEDD: report error by MQTT?
      return;
    }
    for (auto [time, peg]: pegs)
    {
      uint8_t buf[4] = { 0 };
      buf[0] = time / 60;
      buf[1] = time % 60;
      buf[2] = peg.on;
      buf[3] = peg.off;

      if (file.write(buf, 4))
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
    while (file.available() >= 4)
    {
      uint8_t buf[4] = { 0 };
      file.read(buf, 4);
      int time = (int)buf[0] * 60 + buf[1];
      pegs.insert( {time, {buf[2], buf[3]}} );
    }
    if (file.available() > 0)
      Serial.println("bytes remain in the file");
  }


  void dump() const
  {
    for (auto [time, peg]: pegs)
    {
      std::cout << " " << time/60 << ":" << time % 60;
      std::cout << " on: ";
      str_sense(std::cout, peg.on);
      std::cout << " off: ";
      str_sense(std::cout, peg.off);
      std::cout  << std::endl;
    }
  }

private:
  struct peg
  {
    senses_t on;
    senses_t off;
  };

  int curr_time;
  std::map<int, peg> pegs = { }; // peg( time )
  senses_t curr_pegs;
  int boost_until;
};
