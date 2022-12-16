#pragma once

class DebouncedInput
{
public:
    DebouncedInput( std::function<int()> input_fn_, int debounce_ms )
    : state{ 0 }
    , count{ 0 }
    , max_count{ debounce_ms }
    , input_fn{ input_fn_ }
    { }
  virtual ~DebouncedInput( ) { }

  virtual void operator()( )
  {
    const int in = input_fn();
    if (in && count < max_count)
      ++count;
    else if (!in && count > 0)
      --count;

    if ((state == 0 && count == max_count) 
      || (state == 1 && count == 0))
    {
      state = !state;
      new_state();
    }
  }

protected:
  int state;
  virtual void new_state( ) = 0; 

private:
  int count;
  const int max_count;
  std::function<int()> input_fn;
};

// ----------------------------------------------------------------------------

class ButtonInput 
  : public DebouncedInput
{
public:
  ButtonInput( std::function<int()> input_fn )
    : DebouncedInput{ input_fn, debounce_ms }
    , count{ 0 }
    , timer{ 0 }
    { }

  enum Flags {
      Press     =  1 << 31,
      Final     =  1 << 30,
      HoldShort =  1 << 29,
      HoldLong  =  1 << 28,
        // with 28 spare bits for the count, a mask
        // seems a little unncessary...
  };

private:
  virtual void new_state( ) 
  { 
    if (state) // button up -> down
    {
      count++;
      event( Press );
    }

    timer = 0;
  }
  virtual void operator()( )
  {
    DebouncedInput::operator()( );

    if (timer < std::numeric_limits<decltype(timer)>::max()) timer++;
      // don't overflow in case nothing happens for a while

    if (state) // button is down
    {
      if (timer == short_hold_ms)
        event( HoldShort );
      else if (timer == long_hold_ms)
        event( HoldLong );
    }
    else // button is up
    {
      if (timer == recent_press_ms)
      {
        event( Final );
        count = 0;
      }
    }
  }

  void event( Flags f )
  {
    const uint32_t e = f | count;

    switch (f) // temporary debug
    {
        case Press:
            Serial.printf( "Button press %d\n", count );
            break;
        case HoldShort:
            Serial.printf( "Button hold short (%d)\n", count );
            break;
        case HoldLong:
            Serial.printf( "Button hold long (%d)\n", count );
            break;
        case Final:
            Serial.printf( "Button final (%d)\n", count );
            break;
        default:
            break; // most unexpected.
    }
  }

  enum {
    debounce_ms = 30,
    recent_press_ms = 300,
    short_hold_ms = 1500,
    long_hold_ms = 3000,
  };

  int count; // number of presses
  int timer; // hold_time when button is down; recent_time when up
};

// ----------------------------------------------------------------------------

class SwitchInput 
  : public DebouncedInput
{
public:
  SwitchInput( std::function<int()> input_fn, int max_flips )
    : DebouncedInput{ input_fn, debounce_ms }
    , count{ 0 }
    , count_max{ max_flips }
    , recent{ 0 }
    { }

private:
  virtual void new_state( )
  {
    count = (recent) ? std::min(count + 1, count_max) : 1;
    ;//log.debugf( "Switch flip %s (%d)", (state) ? "closed" : "open", count );
    recent = recent_window_ms;
  }
  virtual void operator()( )
  {
    DebouncedInput::operator()( );
    if (recent)
      if (--recent == 0)
        ;//log.debugf( "Switch final (%d)", count );
  }

  enum {
    debounce_ms = 50,
    recent_window_ms = 400,
  };

  int count;
  const int count_max;
  int recent;
};
