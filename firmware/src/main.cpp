#include <Arduino.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "config.h"
#include "secrets.h"
#include "fonts/FreeSans9pt7b.h"
#include "fonts/FreeSans12pt7b.h"
#include "fonts/FreeSans18pt7b.h"

static const int PAD = 14;

static const uint16_t COL_BG = RGB565(10, 14, 24);
static const uint16_t COL_TEXT = RGB565(235, 240, 248);
static const uint16_t COL_NAME = RGB565(120, 180, 255);
static const uint16_t COL_DIM = RGB565(140, 155, 180);
static const uint16_t COL_TEMP = RGB565(255, 190, 90);
static const uint16_t COL_LINE = RGB565(45, 55, 75);
static const uint16_t COL_BORDER = RGB565(70, 85, 110);
static const uint16_t COL_SUN = RGB565(255, 200, 40);
static const uint16_t COL_MOON = RGB565(220, 225, 240);
static const uint16_t COL_CLOUD = RGB565(200, 208, 220);
static const uint16_t COL_DARK_CLOUD = RGB565(120, 130, 150);
static const uint16_t COL_RAIN = RGB565(80, 160, 255);
static const uint16_t COL_SNOW = RGB565(255, 255, 255);

Arduino_DataBus *bus = new Arduino_SWPAR8(
    PIN_RS, GFX_NOT_DEFINED, PIN_WR, GFX_NOT_DEFINED,
    PIN_D0, PIN_D1, PIN_D2, PIN_D3, PIN_D4, PIN_D5, PIN_D6, PIN_D7);

#if DISPLAY_ST7796
Arduino_GFX *gfx = new Arduino_ST7796(bus, PIN_RST, PORTRAIT_ROTATION);
#else
Arduino_GFX *gfx = new Arduino_ILI9486(bus, PIN_RST, PORTRAIT_ROTATION);
#endif

Arduino_Canvas *canvas = nullptr;
Preferences prefs;

struct Layout {
  int title, date, icon, temp;
};
static const Layout PORTRAIT_LAYOUT = {50, 76, 110, 124};
static const Layout LANDSCAPE_LAYOUT = {38, 58, 76, 90};
static const int ROWS_PERCENT_LANDSCAPE = 40;

static bool landscape = false;
static int scrW, scrH, rowW, rowStep;
static const Layout *layout = &PORTRAIT_LAYOUT;

struct Weather {
  bool valid = false;
  float temp = 0;
  int code = -1;
  bool isDay = true;
};

enum Icon { ICON_NONE, ICON_CLEAR, ICON_PARTLY, ICON_CLOUDY, ICON_FOG, ICON_RAIN, ICON_SNOW, ICON_THUNDER };

static Weather weather[3];
static unsigned long lastWeatherFetch = 0;
static bool weatherEverFetched = false;
static int lastMinute = -1;
static bool lastButton = true;

static Icon iconFor(int code) {
  switch (code) {
    case 0: case 1: return ICON_CLEAR;
    case 2: return ICON_PARTLY;
    case 3: return ICON_CLOUDY;
    case 45: case 48: return ICON_FOG;
    case 51: case 53: case 55: case 56: case 57:
    case 61: case 63: case 65: case 66: case 67:
    case 80: case 81: case 82: return ICON_RAIN;
    case 71: case 73: case 75: case 77: case 85: case 86: return ICON_SNOW;
    case 95: case 96: case 99: return ICON_THUNDER;
    default: return ICON_NONE;
  }
}

static const float ICON_SCALE = 0.75f;

static int sc(float v) {
  return lroundf(v * ICON_SCALE);
}

static void drawSun(int cx, int cy, float r) {
  for (int a = 0; a < 360; a += 45) {
    float rad = a * DEG_TO_RAD;
    int x0 = cx + cosf(rad) * sc(r + 4), y0 = cy + sinf(rad) * sc(r + 4);
    int x1 = cx + cosf(rad) * sc(r + 8), y1 = cy + sinf(rad) * sc(r + 8);
    canvas->drawLine(x0, y0, x1, y1, COL_SUN);
    canvas->drawLine(x0 + 1, y0, x1 + 1, y1, COL_SUN);
  }
  canvas->fillCircle(cx, cy, sc(r), COL_SUN);
}

static void drawMoon(int cx, int cy, float r) {
  canvas->fillCircle(cx, cy, sc(r), COL_MOON);
  canvas->fillCircle(cx + sc(r / 2 + 2), cy - sc(r / 3), sc(r - 1), COL_BG);
}

static void drawCloud(int cx, int cy, uint16_t colour) {
  canvas->fillCircle(cx - sc(10), cy + sc(2), sc(8), colour);
  canvas->fillCircle(cx + sc(1), cy - sc(5), sc(11), colour);
  canvas->fillCircle(cx + sc(11), cy + sc(2), sc(8), colour);
  canvas->fillRoundRect(cx - sc(18), cy + sc(1), sc(37), sc(10), sc(5), colour);
}

static void drawIcon(Icon icon, bool isDay, int cx, int cy) {
  switch (icon) {
    case ICON_CLEAR:
      if (isDay) drawSun(cx, cy, 10);
      else drawMoon(cx, cy, 13);
      break;
    case ICON_PARTLY:
      if (isDay) drawSun(cx + sc(8), cy - sc(8), 8);
      else drawMoon(cx + sc(8), cy - sc(8), 10);
      drawCloud(cx - sc(3), cy + sc(5), COL_CLOUD);
      break;
    case ICON_CLOUDY:
      drawCloud(cx, cy, COL_CLOUD);
      break;
    case ICON_FOG:
      for (int i = 0; i < 4; i++) {
        int w = sc((i % 2) ? 30 : 38);
        canvas->fillRoundRect(cx - w / 2, cy - sc(12) + i * sc(8), w, sc(4), sc(2), COL_CLOUD);
      }
      break;
    case ICON_RAIN:
      drawCloud(cx, cy - sc(6), COL_CLOUD);
      for (int i = -1; i <= 1; i++) {
        int x = cx + i * sc(10);
        canvas->drawLine(x + sc(2), cy + sc(9), x - sc(2), cy + sc(17), COL_RAIN);
        canvas->drawLine(x + sc(2) + 1, cy + sc(9), x - sc(2) + 1, cy + sc(17), COL_RAIN);
      }
      break;
    case ICON_SNOW:
      drawCloud(cx, cy - sc(6), COL_CLOUD);
      for (int i = -1; i <= 1; i++) {
        int x = cx + i * sc(10);
        canvas->fillCircle(x, cy + sc(12), 2, COL_SNOW);
        canvas->fillCircle(x + sc(5), cy + sc(18), 2, COL_SNOW);
      }
      break;
    case ICON_THUNDER:
      drawCloud(cx, cy - sc(6), COL_DARK_CLOUD);
      canvas->fillTriangle(cx + sc(2), cy + sc(4), cx - sc(6), cy + sc(14), cx + sc(1), cy + sc(14), COL_SUN);
      canvas->fillTriangle(cx - sc(1), cy + sc(12), cx + sc(6), cy + sc(12), cx - sc(3), cy + sc(22), COL_SUN);
      break;
    default:
      break;
  }
}

static void printAt(const GFXfont *font, const char *text, int x, int y, uint16_t colour, bool alignRight) {
  int16_t x1, y1;
  uint16_t w, h;
  canvas->setFont(font);
  canvas->setTextColor(colour);
  canvas->getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  canvas->setCursor(alignRight ? x - (int)w - x1 : x - x1, y);
  canvas->print(text);
}

static void showStatus(const char *line1, const char *line2) {
  gfx->fillScreen(COL_BG);
  gfx->setFont(&FreeSans12pt7b);
  gfx->setTextColor(COL_TEXT);
  gfx->setCursor(30, 140);
  gfx->print(line1);
  gfx->setFont(&FreeSans9pt7b);
  gfx->setTextColor(COL_DIM);
  gfx->setCursor(30, 175);
  gfx->print(line2);
}

static void applyOrientation() {
  gfx->setRotation(landscape ? LANDSCAPE_ROTATION : PORTRAIT_ROTATION);
  scrW = gfx->width();
  scrH = gfx->height();
  rowW = landscape ? scrW * ROWS_PERCENT_LANDSCAPE / 100 : scrW;
  rowStep = scrH / 3;
  layout = landscape ? &LANDSCAPE_LAYOUT : &PORTRAIT_LAYOUT;

  delete canvas;
  canvas = new Arduino_Canvas(rowW, scrH - 2 * rowStep, gfx);
  canvas->begin(GFX_SKIP_OUTPUT_BEGIN);
  gfx->fillScreen(COL_BG);
  if (rowW < scrW) {
    const int gap = 8;
    int x = rowW + gap;
    int w = scrW - x - gap;
    int h = (scrH - 3 * gap) / 2;
    gfx->drawRoundRect(x, gap, w, h, 6, COL_BORDER);
    gfx->drawRoundRect(x, 2 * gap + h, w, h, 6, COL_BORDER);
  }
}

static void localTimeFor(const Location &loc, time_t now, struct tm *out) {
  setenv("TZ", loc.posixTz, 1);
  tzset();
  localtime_r(&now, out);
}

static void drawRow(int i, time_t now) {
  const Location &loc = LOCATIONS[i];
  struct tm t;
  localTimeFor(loc, now, &t);

  canvas->fillScreen(COL_BG);
  if (i > 0) {
    canvas->drawFastHLine(PAD, 0, rowW - 2 * PAD, COL_LINE);
  }

  printAt(&FreeSans12pt7b, loc.name, PAD, layout->title, COL_NAME, false);

  char buf[32];
  strftime(buf, sizeof(buf), "%H:%M", &t);
  printAt(&FreeSans18pt7b, buf, rowW - PAD, layout->title, COL_TEXT, true);
  strftime(buf, sizeof(buf), "%a %d %b", &t);
  printAt(&FreeSans9pt7b, buf, rowW - PAD, layout->date, COL_DIM, true);

  const Weather &w = weather[i];
  if (w.valid) {
    drawIcon(iconFor(w.code), w.isDay, PAD + 16, layout->icon);

    snprintf(buf, sizeof(buf), "%d", (int)lroundf(w.temp));
    int16_t x1, y1;
    uint16_t tw, th;
    int tx = PAD + 38;
    int ty = layout->temp;
    canvas->setFont(&FreeSans12pt7b);
    canvas->getTextBounds(buf, 0, ty, &x1, &y1, &tw, &th);
    canvas->setTextColor(COL_TEMP);
    canvas->setCursor(tx - x1, ty);
    canvas->print(buf);
    int dx = tx + tw + 5;
    canvas->drawCircle(dx, y1 + 3, 2, COL_TEMP);
    canvas->setFont(&FreeSans9pt7b);
    canvas->setCursor(dx + 4, y1 + 13);
    canvas->print("C");
  } else {
    printAt(&FreeSans12pt7b, "--", PAD, layout->temp, COL_DIM, false);
  }

  int y = i * rowStep;
  int h = (i == 2) ? scrH - y : rowStep;
  gfx->draw16bitRGBBitmap(0, y, canvas->getFramebuffer(), rowW, h);
}

static void drawAll() {
  time_t now = time(nullptr);
  for (int i = 0; i < 3; i++) {
    drawRow(i, now);
  }
}

static bool fetchWeather() {
  String url = "https://api.open-meteo.com/v1/forecast?current=temperature_2m,weather_code,is_day&latitude=";
  for (int i = 0; i < 3; i++) {
    if (i) url += ",";
    url += String(LOCATIONS[i].lat, 4);
  }
  url += "&longitude=";
  for (int i = 0; i < 3; i++) {
    if (i) url += ",";
    url += String(LOCATIONS[i].lon, 4);
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);
  http.useHTTP10(true);
  if (!http.begin(client, url)) {
    Serial.println("Weather begin failed");
    return false;
  }
  int status = http.GET();
  if (status != HTTP_CODE_OK) {
    Serial.printf("Weather HTTP %d\n", status);
    http.end();
    return false;
  }

  JsonDocument filter;
  filter[0]["current"]["temperature_2m"] = true;
  filter[0]["current"]["weather_code"] = true;
  filter[0]["current"]["is_day"] = true;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    Serial.printf("Weather JSON: %s\n", err.c_str());
    return false;
  }

  JsonArray arr = doc.as<JsonArray>();
  for (int i = 0; i < 3 && i < (int)arr.size(); i++) {
    JsonObject cur = arr[i]["current"];
    if (cur["temperature_2m"].is<float>()) {
      weather[i].temp = cur["temperature_2m"].as<float>();
      weather[i].code = cur["weather_code"] | -1;
      weather[i].isDay = (cur["is_day"] | 1) == 1;
      weather[i].valid = true;
    }
  }
  return true;
}

static void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  prefs.begin("timebuddy");
  landscape = prefs.getBool("landscape", false);

  if (!gfx->begin()) {
    Serial.println("Display init failed");
  }
  gfx->invertDisplay(DISPLAY_INVERT);
  applyOrientation();

  showStatus("TimeBuddy", "Connecting to " WIFI_SSID "...");
  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    showStatus("No WiFi", "Check WIFI_SSID / WIFI_PASSWORD");
    while (WiFi.status() != WL_CONNECTED) {
      delay(5000);
      connectWiFi();
    }
  }

  showStatus("TimeBuddy", "Syncing time...");
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  while (time(nullptr) < 1700000000) {
    delay(250);
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  bool redraw = false;
  unsigned long interval = weatherEverFetched ? WEATHER_REFRESH_MS : WEATHER_RETRY_MS;
  if (lastWeatherFetch == 0 || millis() - lastWeatherFetch >= interval) {
    lastWeatherFetch = millis();
    if (WiFi.status() == WL_CONNECTED && fetchWeather()) {
      weatherEverFetched = true;
      redraw = true;
    }
  }

  time_t now = time(nullptr);
  int minute = (now / 60) % 60;
  if (minute != lastMinute) {
    lastMinute = minute;
    redraw = true;
  }

  bool button = digitalRead(PIN_BUTTON);
  if (!button && lastButton) {
    landscape = !landscape;
    prefs.putBool("landscape", landscape);
    applyOrientation();
    redraw = true;
  }
  lastButton = button;

  if (redraw) {
    drawAll();
  }

  delay(30);
}
