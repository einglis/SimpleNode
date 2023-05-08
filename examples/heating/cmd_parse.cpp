
#include <cstddef>
#include <cstring>

#include "cmd_parse.h"

enum tok_t
{
  tok_none = 0,
  tok_channel,
  tok_sensitivity,
  tok_boost,
  tok_boost_time,
  tok_on,
  tok_off,
  tok_now,
  tok_at,
  tok_peg_time,
  tok_delete,
  tok_other,
};

struct token_t
{
  enum tok_t type;
  union
  {
    int channel;
    unsigned int sense;
    int boost_time;
    int peg_time;
  };
};

// ----------------------------------------------------------------------------

static bool parse_tok_channel( const char *sp, size_t len, int &channel )
{
  if (len != 2)
    return false;

  if (sp[1] == 'w' || sp[1] == 'W')
    channel = 0;
  else if (sp[1] >= '0' && sp[1] <= '3') // magic; 3 == max channel
    channel = sp[1] - '0';
  else
    return false;

  return true;
}

static bool parse_tok_sensitivity( const char *sp, size_t len, unsigned int &sense )
{
  if (len < 2)
    return false;

  for (size_t i = 1; i < len; i++)
  {
    if (sp[i] == 'w' || sp[i] == 'W')
      sense |= (1 << 0);
    else if (sp[i] >= '0' && sp[i] <= '7') // magic; 7 == max sensitivity
      sense |= (1 << (sp[i] - '0'));
    else
      return false;
  }

  return true;
}

static bool parse_int( const char *sp, size_t len, int &x )
{
  for (size_t i = 0; i < len; i++)
  {
    if (sp[i] >= '0' && sp[i] <= '9')
      x = 10 * x + sp[i] - '0';
    else
      return false;
  }

  return true;
}

static bool parse_tok_boost_time( const char *sp, size_t len, int &boost_time )
{
  if (len < 1)
    return false;

  if (!parse_int(&sp[0], len, boost_time))
    return false;

  if (boost_time > 60)
    return false;

  return true;
}

static bool parse_tok_peg_time( const char *sp, size_t len, int &peg_time )
{
  if (len != 5)
    return false;

  if (sp[2] != ':')
    return false;

  int hh = 0;
  int mm = 0;

  if (!parse_int(&sp[0], 2, hh) || !parse_int(&sp[3], 2, mm))
    return false;

  if (hh > 23 || mm > 60)
    return false;

  peg_time = hh * 60 + mm;
  return true;
}

// ------------------------------------

static token_t tokenise( const char *sp, size_t len )
{
  token_t t = { tok_none, 0 };
  if (!sp || !len)
    return t;

  if (sp[0] == 'c' || sp[0] == 'C')
  {
    if (parse_tok_channel( sp, len, t.channel ))
      t.type = tok_channel;
  }
  else if (sp[0] == 's' || sp[0] == 'S')
  {
    if (parse_tok_sensitivity( sp, len, t.sense ))
      t.type = tok_sensitivity;
  }
  else if (sp[0] >= '0' && sp[0] < '9')
  {
    if (parse_tok_boost_time( sp, len, t.boost_time ))
      t.type = tok_boost_time;
    else if (parse_tok_peg_time( sp, len, t.peg_time ))
      t.type = tok_peg_time;
  }
  else if (0 == strncasecmp(sp, "boost", len)) t.type = tok_boost;
  else if (0 == strncasecmp(sp, "on",    len)) t.type = tok_on;
  else if (0 == strncasecmp(sp, "off",   len)) t.type = tok_off;
  else if (0 == strncasecmp(sp, "now",   len)) t.type = tok_now;
  else if (0 == strncasecmp(sp, "del",   len)) t.type = tok_delete;
  else if (0 == strncasecmp(sp, "at",    len)) t.type = tok_at;
  else if (0 == strncmp(    sp, "//",    len)) t.type = tok_none; // just treat a comment as the end of the string.

  else
    t.type = tok_other;

  return t;
}

static token_t next_token( const char *&sp )
{
  while (*sp && *sp == ' ') { sp++; } // skip whitespace

  const char *start = sp;
  while (*sp && *sp != ' ') { sp++; } // collect characters

  return tokenise( start, sp - start );
}

// ----------------------------------------------------------------------------

bool parse_cmd( const char *str )
{
  //const char *str_orig = str;

  enum { none, is_on, is_off, is_del, is_boost } type = none;
  int channel = 0;
  unsigned int sensitivity = 0;
  int time = -1;

  enum { idle, error, have_chan, have_del, have_sense, have_boost, have_on_off, have_at, done } state = idle;
  while (state != error)
  {
    token_t t = next_token( str );

    auto next_state = error;
    switch (state)
    {
      case idle:
        if (t.type == tok_channel) { next_state = have_chan; channel = t.channel; }
        break;

      case have_chan:
        if      (t.type == tok_sensitivity) { next_state = have_sense; sensitivity = t.sense; }
        else if (t.type == tok_boost)       { next_state = have_boost; type = is_boost; }
        else if (t.type == tok_delete)      { next_state = have_del;   type = is_del;   }
        break;

      case have_sense:
        if      (t.type == tok_on)  { next_state = have_on_off; type = is_on;  }
        else if (t.type == tok_off) { next_state = have_on_off; type = is_off; }
        break;

      case have_boost:
        if      (t.type == tok_boost_time) { next_state = done; time = t.boost_time; }
        else if (t.type == tok_off)        { next_state = done; time = 0; }
        break;

      case have_on_off:
        if      (t.type == tok_now)        next_state = done;
        else if (t.type == tok_at)         next_state = have_at;
        else if (t.type == tok_peg_time) { next_state = done; time = t.peg_time; }
        else if (t.type == tok_none)       next_state = done;
        break;

      case have_at:
      case have_del:
        if (t.type == tok_peg_time) { next_state = done; time = t.peg_time; }
        break;

      case done:
        if (t.type == tok_none) next_state = done;
          // ensures there are no spurious tokens afterwards
        break;

      default:
        break;
    }

    state = next_state;

    if (t.type == tok_none)
      break;
  }

  if (state == done)
  {
    switch (type)
    {
      case is_on: 
      case is_off:
        if (time < 0)
          cmd_now( channel, sensitivity, (type == is_on) );
        else
          cmd_set( channel, sensitivity, (type == is_on), time );
        break;

      case is_boost:
        cmd_boost( channel, time );
        break;

      case is_del:
        cmd_delete( channel, time );
        break;

      default:
        //printf("parsing failed at pos %lu\n", str - str_orig);
        return false;
    }
  }

  return state == done;
}

// ----------------------------------------------------------------------------

#if 0 // tests

#include <cstdio>

void cmd_now( int channel, unsigned int sensitivity, bool on_n_off ) {}
void cmd_set( int channel, unsigned int sensitivity, bool on_n_off, int time ) {}
void cmd_boost( int channel, int time ) {}
void cmd_delete( int channel, int time ) {}

static void test( const char *str )
{
  const bool rc = parse_cmd(str);
  printf("decode \"%s\" -- %s\n", str, (rc) ? "success" : "failure");
}

int main( int argc, char *argv[] )
{
  test( "c1 s2 now" );
  test( "c1 s2 on" );
  test( "c1 s2 on // fish" );
  test( "c1 s2 off now" );
  test( "c1 s2 off now fish" );
  test( "c2 s123 boost   15" );
  test( "c2 s123 boost" );
  test( "c0 s0 on at 12:34" );
  test( "c3 del 12:34" );
  test( "c3 del 12:34 // stuff and things and junk" );
  test( "s3 c3 del 12:34" );
  test( "c0 s6 on at 02:34" );
  test( "c0 sw on at 02:34" );
  test( "c0 s1 on at 02:34" );
  test( "c0 s4 on at 02:34" );
  test( "c0 s4 off at 02:34" );
  test( "c0 s123 off at 02:34" );
  test( "c0 s4 off at 02:35" );
  test( "c3 del 12:34" );
  test( "c0 s1 on at 02:33" );
  test( "c0 s14 on at 01:34" );

  return 0; // success
}

#endif
