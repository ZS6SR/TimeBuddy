#pragma once

// 0 = ILI9486, 1 = ST7796. Switch if the screen stays white or shows garbage.
#define DISPLAY_ST7796 0

// Portrait: 4 or 6. Landscape: 1 or 3. Switch if the picture is upside down.
#define PORTRAIT_ROTATION 4
#define LANDSCAPE_ROTATION 1

// Switch if colours look inverted (black background shows white).
#define DISPLAY_INVERT false

#define PIN_D0 0
#define PIN_D1 1
#define PIN_D2 2
#define PIN_D3 3
#define PIN_D4 4
#define PIN_D5 5
#define PIN_D6 6
#define PIN_D7 7
#define PIN_WR 10
#define PIN_RS 20
#define PIN_RST 21

// BOOT button: switches between portrait and landscape.
#define PIN_BUTTON 9

#define WEATHER_REFRESH_MS (10UL * 60UL * 1000UL)
#define WEATHER_RETRY_MS (30UL * 1000UL)

#define RACE_REFRESH_MS (6UL * 60UL * 60UL * 1000UL)
#define RACE_RETRY_MS (5UL * 60UL * 1000UL)

// Race times are shown in this location's time zone (0 = top, 1 = middle, 2 = bottom).
#define RACE_TIME_LOCATION 1

// Setup page is at http://timebuddy.local (or the IP shown at startup).
#define WEB_HOSTNAME "timebuddy"

// If the saved Wi-Fi can't be joined within this time, TimeBuddy starts its own
// open Wi-Fi network with the setup page, and restarts to try again after
// SETUP_MODE_TIMEOUT_MS with no one using the page (e.g. after a power cut).
#define WIFI_CONNECT_TIMEOUT_MS (30UL * 1000UL)
#define SETUP_AP_SSID "TimeBuddy-Setup"
#define SETUP_MODE_TIMEOUT_MS (5UL * 60UL * 1000UL)

// Default locations, used until changed on the setup page.

struct Location {
  const char *name;
  const char *country;
  const char *posixTz;
  float lat;
  float lon;
};

#include "locations.h"
