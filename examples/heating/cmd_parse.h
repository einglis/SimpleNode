
#pragma once

void cmd_now( int channel, unsigned int sensitivity, bool on_n_off );
void cmd_set( int channel, unsigned int sensitivity, bool on_n_off, int time );
void cmd_boost( int channel, int time );
void cmd_delete( int channel, int time );

bool parse_cmd( const char *str );
  // will call one of the above functions and return true, or return false.
