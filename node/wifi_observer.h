#pragma once

namespace node {

struct WifiObserver
{
  virtual void wifi_up() { };
  virtual void wifi_down() { };
  virtual ~WifiObserver() = 0;
};

WifiObserver::~WifiObserver( ) { }
  // pure virtual destructor implementation

// ------------------------------------

class WifiObservers
{
public:
  static void add( WifiObserver* wo )
  {
    list.push_back( wo );
  }
  static void wifi_up( )
  {
    schedule_function( []() { // decouple schedulling, just in case.
      for ( auto wo : list )
        wo->wifi_up();
    } );
  }
  static void wifi_down( )
  {
    schedule_function( []() { // ditto
      for ( auto wo : list )
        wo->wifi_down();
    } );
  }

private:
  static std::vector< WifiObserver* > list; // declaration...
};

std::vector< WifiObserver* > WifiObservers::list; // ...definition

} // node