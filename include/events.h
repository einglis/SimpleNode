#pragma once

#include <vector>

namespace node {

template< typename T >
class Events
{
public:
  Events( size_t num_events )
    : len{ num_events }
    , wr_pos{ 0 }
    , fill{ 0 }
    , overflow{ false }
  {
    events.reserve( len );
  }

  void add_unguarded( T e )
  {
    events[ wr_pos ] = std::move( e );
    wr_pos = (wr_pos + 1) % len;

    if (fill < len)
      fill++;
    else
      overflow = true;
  }

  void add( T e )
  {
    noInterrupts();
    add_unguarded( e );
    interrupts();
  }

  bool remove_unguarded( T& e, bool& o )
  {
    if (fill == 0)
      return false;

    const size_t rd_pos = (wr_pos + len - fill) % len;
    e = std::move( events[ rd_pos ] );
    fill--;

    o = overflow;
    Serial.println(o);
    overflow = false;

    return true;
  }

  bool remove( T& e, bool& overflow )
  {
    if (fill == 0)
      return false;

    noInterrupts();
    const bool rc = remove_unguarded( e, overflow );
    interrupts();
    return rc;
  }

  bool remove( T& e )
  {
    bool dummy_overflow;
    return remove( e, dummy_overflow );
  }

private:
  const size_t len;
  std::vector<T> events;
  size_t wr_pos;
  size_t fill;
  bool overflow;
};

} // node
