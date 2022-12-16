#pragma once

namespace {

static Logger log( "INPUTS" ); // super lazy

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

class ButtonInput 
  : public DebouncedInput
{
public:
  ButtonInput( std::function<int()> input_fn, int max_presses )
    : DebouncedInput{ input_fn, debounce_ms }
    , count{ 0 }
    , count_max{ max_presses }
    , recent{ 0 }
    , hold_time{ 0 }
    { }

private:
  virtual void new_state( ) 
  { 
    if (state) // button up -> down
    {
      count = (recent) ? std::min(count + 1, count_max) : 1;
      log.debugf( "Button press %d", count );
    }
    else // button down -> up
    {
      recent = recent_window_ms;
    }
  }
  virtual void operator()( )
  {
    DebouncedInput::operator()( );

    if (state) // button is down
    {
      ++hold_time;
      if (hold_time == short_hold_ms)
        log.debugf( "Button hold short (%d)", count );
      else if (hold_time == long_hold_ms)
        log.debugf( "Button hold long (%d)", count );
    }
    else // button is up
    {
      hold_time = 0;
      if (recent)
        if (--recent == 0)
          log.debugf( "Button final (%d)", count );
    }
  }

  enum {
    debounce_ms = 30,
    recent_window_ms = 300, 
    short_hold_ms = 2000,
    long_hold_ms = 5000,
  };

  int count;
  const int count_max;
  int recent;
  int hold_time;
};

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
    log.debugf( "Switch flip %s (%d)", (state) ? "closed" : "open", count );
    recent = recent_window_ms;
  }
  virtual void operator()( )
  {
    DebouncedInput::operator()( );
    if (recent)
      if (--recent == 0)
        log.debugf( "Switch final (%d)", count );
  }

  enum {
    debounce_ms = 50,
    recent_window_ms = 400,
  };

  int count;
  const int count_max;
  int recent;
};

} // anon