#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <time.h>

// ================= Hardware =================
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW   // try GENERIC_HW / PAROLA_HW / ICSTATION_HW if display is garbled
#define MAX_DEVICES   4
#define CS_PIN        2                     // D4 (GPIO2); DIN = D7, CLK = D5 (hardware SPI)

// ================= Time / DST =================
#define USE_DST_SWITCH 0    // 0 = automatic EU DST, 1 = manual hardware switch
#define DST_PIN        4    // D2 (GPIO4); switch to GND = summer time (UTC+3)

// ================= Brightness (0 = dimmest, 15 = brightest) =================
const uint8_t BRIGHT_DAY   = 2;     // while the sun is up
const uint8_t BRIGHT_NIGHT = 0;     // after dark
const float   LAT = 54.687f;        // Vilnius
const float   LON = 25.283f;
const int     DIM_EARLY_MIN = 30;   // dim this many minutes before sunset / stay dim after sunrise

// ================= Globals =================
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
const int WIDTH = MAX_DEVICES * 8;  // 32 columns

// 4x7 digits (one row per byte, 4 bits, MSB = left pixel)
const uint8_t BIG[10][7] = {
  {0b0110,0b1001,0b1001,0b1001,0b1001,0b1001,0b0110}, // 0
  {0b0010,0b0110,0b0010,0b0010,0b0010,0b0010,0b0111}, // 1
  {0b0110,0b1001,0b0001,0b0010,0b0100,0b1000,0b1111}, // 2
  {0b1110,0b0001,0b0001,0b0110,0b0001,0b0001,0b1110}, // 3
  {0b1001,0b1001,0b1001,0b1111,0b0001,0b0001,0b0001}, // 4
  {0b1111,0b1000,0b1110,0b0001,0b0001,0b1001,0b0110}, // 5
  {0b0110,0b1000,0b1000,0b1110,0b1001,0b1001,0b0110}, // 6
  {0b1111,0b0001,0b0010,0b0010,0b0100,0b0100,0b0100}, // 7
  {0b0110,0b1001,0b1001,0b0110,0b1001,0b1001,0b0110}, // 8
  {0b0110,0b1001,0b1001,0b0111,0b0001,0b1001,0b0110}, // 9
};

// 3x5 digits for seconds
const uint8_t SMALL[10][5] = {
  {0b111,0b101,0b101,0b101,0b111}, // 0
  {0b010,0b110,0b010,0b010,0b111}, // 1
  {0b111,0b001,0b111,0b100,0b111}, // 2
  {0b111,0b001,0b111,0b001,0b111}, // 3
  {0b101,0b101,0b111,0b001,0b001}, // 4
  {0b111,0b100,0b111,0b001,0b111}, // 5
  {0b111,0b100,0b111,0b101,0b111}, // 6
  {0b111,0b001,0b001,0b001,0b001}, // 7
  {0b111,0b101,0b111,0b101,0b111}, // 8
  {0b111,0b101,0b111,0b001,0b111}  // 9
};

// ================= Drawing =================
// x = 0 is the left edge of the display
void px(int x, int y, bool on = true) {
  if (x < 0 || x >= WIDTH || y < 0 || y > 7) return;
  mx.setPoint(y, WIDTH - 1 - x, on);
}

void drawBig(int x, int digit) {
  for (int r = 0; r < 7; r++)
    for (int c = 0; c < 4; c++)
      if (BIG[digit][r] & (1 << (3 - c))) px(x + c, r);
}

void drawSmall(int x, int digit) {
  for (int r = 0; r < 5; r++)
    for (int c = 0; c < 3; c++)
      if (SMALL[digit][r] & (1 << (2 - c))) px(x + c, r + 2);  // bottom-aligned
}

// ================= Sunrise / sunset =================
bool isDark(time_t now) {
  tm u;
  gmtime_r(&now, &u);

  float g = 2.0f * PI / 365.0f * (u.tm_yday + (u.tm_hour - 12) / 24.0f);
  float eqtime = 229.18f * (0.000075f + 0.001868f * cos(g) - 0.032077f * sin(g)
                 - 0.014615f * cos(2 * g) - 0.040849f * sin(2 * g));
  float decl = 0.006918f - 0.399912f * cos(g) + 0.070257f * sin(g)
               - 0.006758f * cos(2 * g) + 0.000907f * sin(2 * g)
               - 0.002697f * cos(3 * g) + 0.00148f * sin(3 * g);

  float latR = LAT * PI / 180.0f;
  float cosH = cos(90.833f * PI / 180.0f) / (cos(latR) * cos(decl)) - tan(latR) * tan(decl);
  if (cosH > 1.0f)  return true;    // polar night
  if (cosH < -1.0f) return false;   // midnight sun
  float ha = acos(cosH) * 180.0f / PI;

  float riseMin = 720.0f - 4.0f * (LON + ha) - eqtime;   // minutes after 00:00 UTC
  float setMin  = 720.0f - 4.0f * (LON - ha) - eqtime;
  float nowMin  = u.tm_hour * 60 + u.tm_min;

  return nowMin < riseMin + DIM_EARLY_MIN || nowMin > setMin - DIM_EARLY_MIN;
}

void drawClock(const tm &t, bool colon) {
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  mx.clear();

  int x = 1;
  drawBig(x,      t.tm_hour / 10);
  drawBig(x + 5,  t.tm_hour % 10);
  if (colon) { px(x + 10, 2); px(x + 10, 4); }
  drawBig(x + 12, t.tm_min / 10);
  drawBig(x + 17, t.tm_min % 10);
  drawSmall(x + 23, t.tm_sec / 10);
  drawSmall(x + 27, t.tm_sec % 10);

  static int lastMin = -1;
  static bool dark = false;
  if (t.tm_min != lastMin) {            // recalculate once per minute
    lastMin = t.tm_min;
    dark = isDark(time(nullptr));
  }
  mx.control(MD_MAX72XX::INTENSITY, dark ? BRIGHT_NIGHT : BRIGHT_DAY);

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}

void showLetter(char c) {               // boot status: W = WiFi, N = NTP
  mx.clear();
  mx.setChar(WIDTH - 1, c);
}

// ================= Timezone =================
void applyTimezone() {
#if USE_DST_SWITCH
  bool dstOn = (digitalRead(DST_PIN) == LOW);
  setenv("TZ", dstOn ? "EEST-3" : "EET-2", 1);          // fixed UTC+3 or UTC+2
#else
  setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);      // automatic EU rules (Vilnius)
#endif
  tzset();
}

// ================= Arduino =================
void setup() {
  Serial.begin(115200);
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, BRIGHT_DAY);
  mx.clear();
  showLetter('W');

#if USE_DST_SWITCH
  pinMode(DST_PIN, INPUT_PULLUP);
#endif

  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  if (!wm.autoConnect("MatrixClock-Setup")) ESP.restart();

  showLetter('N');
  configTime(0, 0, "pool.ntp.org", "time.google.com");  // NTP in UTC
  applyTimezone();                                      // must come after configTime
  while (time(nullptr) < 1700000000) { delay(200); yield(); }
  mx.clear();
}

void loop() {
#if USE_DST_SWITCH
  static int lastSw = -1;
  int sw = digitalRead(DST_PIN);
  if (sw != lastSw) { lastSw = sw; applyTimezone(); }
#endif

  static int lastSec = -1;
  time_t now = time(nullptr);
  tm t;
  localtime_r(&now, &t);

  if (t.tm_sec != lastSec) {            // redraw only when the second changes
    lastSec = t.tm_sec;
    drawClock(t, t.tm_sec % 2 == 0);    // colon blinks every second
  }

  if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
  delay(20);
}