/*********************************************************************
 * ESP32-S3 Board Test: GC9A01 + CST816S + I2C Scanner + LC76G GPS
 * - Display: GC9A01 1.28" round, SPI (Arduino_GFX_Library)
 * - Touch:   CST816S, I2C @ SDA=8 SCL=9
 * - I2C:     Scanner + common chip-ID recognizer
 * - GPS:     LC76G on UART (RX=44, TX=43), UTC + local (America/Chicago),
 *            RMC/GGA/GSV parsing, Cold Start button
 * - Pins 17 and 42 forced HIGH (GPS enables per your design)
 *
 * UI pages:
 *   I2C page:  [Scan I2C]  [GPS Page]
 *   GPS page:  [Cold Start] [I2C Page]
 *
 * Requires: Arduino_GFX_Library by moononournation
 *********************************************************************/

#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <time.h>
#include <stdlib.h>   // setenv, unsetenv, getenv
#include <string.h>   // strdup, strncpy

// ========= USER PINS / CONST ===========
// --- Display (VSPI) ---
#define TFT_SCLK 12
#define TFT_MOSI 11
#define TFT_CS   10
#define TFT_DC   39
#define TFT_RST  40
#define TFT_BL    7   // set -1 if BL is tied to 3v3

// --- Touch (I2C) ---
#define TP_SDA   8
#define TP_SCL   9
#define TP_INT   41
#define TP_RST   25

// --- Always HIGH pins (GPS enables) ---
#define HOLD1_PIN 17
#define HOLD2_PIN 42

// --- GPS UART pins (ESP32-S3 IO matrix example) ---
#define GPS_RX 44   // ESP32-S3 pin that receives from GPS TX
#define GPS_TX 43   // ESP32-S3 pin that transmits to GPS RX

// ========= DISPLAY / TOUCH SETUP =========
const uint16_t W = 240, H = 240;
const uint8_t ROT = 1; // your working rotation

Arduino_DataBus *bus = new Arduino_ESP32SPI(
  TFT_DC /*DC*/, TFT_CS /*CS*/, TFT_SCLK /*SCK*/, TFT_MOSI /*MOSI*/, -1 /*MISO*/);
Arduino_GC9A01 *gfx = new Arduino_GC9A01(bus, TFT_RST, true /*IPS*/, W, H);

// ========= CST816S basics =========
static const uint8_t CST816S_ADDR = 0x15;
static const uint8_t REG_GESTURE  = 0x01;

// ========= GPS UART =========
HardwareSerial GPS(1);
uint32_t gpsBaud = 0;
const uint32_t BAUDS[] = {9600, 38400, 115200};

// ========= UI STATE =========
enum Page { PAGE_I2C = 0, PAGE_GPS = 1 };
Page page = PAGE_I2C;

// ========= UTILS =========
static bool i2cRead(uint8_t addr, uint8_t reg, uint8_t *buf, size_t n) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)addr, (int)n) != (int)n) return false;
  for (size_t i = 0; i < n; ++i) buf[i] = Wire.read();
  return true;
}
static bool i2cRead8(uint8_t addr, uint8_t reg, uint8_t &val) {
  return i2cRead(addr, reg, &val, 1);
}
static bool i2cWrite8(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}
static void printAddrBoth(uint8_t a7) {
  uint8_t wr=(a7<<1)|0, rd=(a7<<1)|1;
  Serial.printf("7-bit 0x%02X | 8-bit W:0x%02X R:0x%02X", a7, wr, rd);
}

// Portable UTC mktime replacement for toolchains without timegm()
time_t mktime_utc(struct tm *tm_utc) {
  char *old = getenv("TZ");
  char *backup = old ? strdup(old) : nullptr;

  setenv("TZ", "UTC", 1);
  tzset();
  time_t t = mktime(tm_utc);

  if (backup) { setenv("TZ", backup, 1); free(backup); }
  else { unsetenv("TZ"); }
  tzset();
  return t;
}

// ========= TOUCH =========
volatile bool touch_irq = false;
void IRAM_ATTR touch_isr() { touch_irq = true; }

bool cst816s_read6(uint8_t buf[6]) {
  Wire.beginTransmission(CST816S_ADDR);
  Wire.write(REG_GESTURE);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)CST816S_ADDR, 6) != 6) return false;
  for (int i=0;i<6;++i) buf[i] = Wire.read();
  return true;
}
bool cst816s_get_point(uint16_t &x, uint16_t &y, uint8_t &gesture, uint8_t &fingers) {
  uint8_t d[6];
  if (!cst816s_read6(d)) return false;
  gesture = d[0];
  fingers = d[1];
  x = ((uint16_t)(d[2] & 0x0F) << 8) | d[3];
  y = ((uint16_t)(d[4] & 0x0F) << 8) | d[5];
  return true;
}
static inline void mapTouchToScreen(uint16_t rx, uint16_t ry, uint16_t &sx, uint16_t &sy) {
  switch (ROT) {
    case 0: sx = rx;            sy = ry;            break;
    case 1: sx = ry;            sy = (W - 1) - rx;  break;
    case 2: sx = (W - 1) - rx;  sy = (H - 1) - ry;  break;
    case 3: sx = (H - 1) - ry;  sy = rx;            break;
  }
}

// ========= I2C SCANNER + ID =========
struct ScanResult { uint8_t addr; char label[24]; };
ScanResult found[16]; int foundN = 0;

void identifyI2C(uint8_t addr, char out[24]) {
  uint8_t v;
  strcpy(out, "");
  if (addr == CST816S_ADDR) { strcpy(out, "CST816S touch"); return; }

  // BNO055
  if (i2cRead8(addr, 0x00, v) && v == 0xA0) { strcpy(out, "BNO055"); return; }

  // BMP/BME family
  if (i2cRead8(addr, 0xD0, v)) {
    if (v == 0x58) { strcpy(out, "BME/BMP280 (0x58)"); return; }
    if (v == 0x60) { strcpy(out, "BME680/BMP388 (0x60)"); return; }
  }

  // MAX3010x (part id at 0xFF: common 0x15/0x11/0x33)
  if (addr == 0x57 && i2cRead8(addr, 0xFF, v)) {
    if (v == 0x15 || v == 0x11 || v == 0x33) { strcpy(out, "MAX3010x"); return; }
  }

  // Heuristic: 24xx EEPROM read @0x00 works
  Wire.beginTransmission(addr); Wire.write((uint8_t)0x00);
  if (Wire.endTransmission(false)==0 && Wire.requestFrom((int)addr, 4)==4) {
    strcpy(out, "EEPROM?");
    while (Wire.available()) (void)Wire.read();
    return;
  }

  strcpy(out, "unknown");
}

void scanI2C(uint32_t hz=100000, uint16_t settle_ms=200) {
  delay(settle_ms);
  Wire.setClock(hz);
  Serial.println("\n===== I2C Scan =====");
  foundN = 0;
  for (uint8_t a=1; a<127; ++a) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission()==0) {
      char label[24]; identifyI2C(a,label);
      Serial.print("  - "); printAddrBoth(a); Serial.printf("  %s\n", label);
      if (foundN < (int)(sizeof(found)/sizeof(found[0]))) {
        found[foundN].addr = a;
        size_t cap = sizeof(found[0].label) - 1;
        strncpy(found[foundN].label, label, cap);
        found[foundN].label[cap] = '\0';
        foundN++;
      }
    }
  }
  Serial.printf("Total: %d device(s)\n", foundN);
}

// ========= GPS / NMEA =========
bool validNMEAChecksum(const String &line) {
  int star = line.indexOf('*'); if (star < 0) return false;
  uint8_t sum = 0; for (int i=1;i<star;++i) sum ^= (uint8_t)line[i];
  if (star+2 >= (int)line.length()) return false;
  auto hexv=[](char c)->int{ if(c>='0'&&c<='9')return c-'0'; if(c>='A'&&c<='F')return 10+c-'A'; if(c>='a'&&c<='f')return 10+c-'a'; return -1; };
  int v = (hexv(line[star+1])<<4)|hexv(line[star+2]);
  return v>=0 && (uint8_t)v==sum;
}
void gpsSendNMEA(const char *payload) {
  uint8_t cs=0; for (const char*p=payload;*p;++p) cs^=(uint8_t)*p;
  char buf[16]; snprintf(buf,sizeof(buf),"*%02X\r\n",cs);
  GPS.print("$"); GPS.print(payload); GPS.print(buf);
}
void gpsMakeChatty() {
  gpsSendNMEA("PMTK314,0,1,0,1,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0"); // RMC,GGA,GSV,ZDA @ 1Hz
  gpsSendNMEA("PMTK220,1000");
  gpsSendNMEA("PMTK353,1,1,1,1,0"); // GPS+GLO+GAL+BDU
}
void gpsColdStart() { gpsSendNMEA("PMTK104"); }

struct GpsState {
  bool rmcValid = false;
  char utc[24] = "";
  char local[24] = "";
  char date[16] = "";
  int  fix = 0;
  int  satsUsed = 0; // from GGA
  int  inViewGPS = 0;
  int  maxCN0 = -1;
} gps;

void setTZ_Chicago() { setenv("TZ", "CST6CDT,M3.2.0/2,M11.1.0/2", 1); tzset(); }

// parse RMC time/date -> fill gps.utc/local/date
void handleRMC(const String &s) {
  String f[12]; int idx=0,k=0;
  for (int i=0;i<(int)s.length() && k<12; ++i) {
    if (s[i]==','||s[i]=='*'){ f[k++]=s.substring(idx,i); idx=i+1; }
  }
  if (k<10) return;
  String t=f[1], d=f[9], stat=f[2];
  if (t.length()<6 || d.length()!=6) return;
  int hh=t.substring(0,2).toInt();
  int mm=t.substring(2,4).toInt();
  int ss=t.substring(4,6).toInt();
  int DD=d.substring(0,2).toInt();
  int MM=d.substring(2,4).toInt();
  int YY=d.substring(4,6).toInt()+2000;

  struct tm tm_utc = {};
  tm_utc.tm_year = YY-1900; tm_utc.tm_mon=MM-1; tm_utc.tm_mday=DD;
  tm_utc.tm_hour=hh; tm_utc.tm_min=mm; tm_utc.tm_sec=ss;
  time_t t_utc = mktime_utc(&tm_utc);   // portable UTC mktime

  strftime(gps.utc, sizeof(gps.utc), "%H:%M:%S UTC", gmtime(&t_utc));
  setTZ_Chicago();
  struct tm *lt = localtime(&t_utc);
  strftime(gps.local, sizeof(gps.local), "%I:%M:%S %p %Z", lt);
  snprintf(gps.date, sizeof(gps.date), "%04d-%02d-%02d", YY, MM, DD);
  gps.rmcValid = (stat=="A");
}

// parse GGA for fix/satsUsed
void handleGGA(const String &s) {
  String f[15]; int idx=0,k=0;
  for (int i=0;i<(int)s.length() && k<15; ++i) {
    if (s[i]==','||s[i]=='*'){ f[k++]=s.substring(idx,i); idx=i+1; }
  }
  if (k<8) return;
  gps.fix = f[6].length()? f[6].toInt():0;
  gps.satsUsed = f[7].length()? f[7].toInt():0;
}

// parse GSV summary for GPS (we’ll track max CN0 and sv_in_view)
void handleGSV(const String &s) {
  // $GxGSV,total,msg,sv_in_view, sat1,el,az,cn0, sat2,el,az,cn0, ...
  String f[28]; int idx=0,k=0;
  for (int i=0;i<(int)s.length() && k<28; ++i) {
    if (s[i]==','||s[i]=='*'){ f[k++]=s.substring(idx,i); idx=i+1; }
  }
  if (k<4) return;
  int sv_in_view = f[3].length()? f[3].toInt():0;
  if (s[1]=='G' && s[2]=='P') gps.inViewGPS = sv_in_view;

  for (int j=7; j<k; j+=4) {
    if (f[j].length()) {
      int cn0 = f[j].toInt();
      if (cn0 > gps.maxCN0) gps.maxCN0 = cn0;
    }
  }
}

bool gpsAutodetect() {
  String line; gpsBaud = 0;
  for (uint8_t i=0;i<sizeof(BAUDS)/sizeof(BAUDS[0]);++i) {
    GPS.end(); GPS.begin(BAUDS[i], SERIAL_8N1, GPS_RX, GPS_TX);
    delay(50);
    uint32_t t0=millis(); bool ok=false;
    while (millis()-t0 < 1200) {
      while (GPS.available()) {
        char c=GPS.read(); if (c=='\r') continue;
        if (c=='\n') {
          if (line.startsWith("$") && validNMEAChecksum(line)) { ok=true; break; }
          line = "";
        } else {
          line += c; if (line.length()>120) line.remove(0, line.length()-120);
        }
      }
      if (ok) break; delay(1);
    }
    if (ok) { gpsBaud = BAUDS[i]; break; }
  }
  if (gpsBaud) {
    Serial.printf("GPS detected @ %lu baud\n", (unsigned long)gpsBaud);
    gpsMakeChatty();
    return true;
  } else {
    Serial.println("GPS not detected at common bauds.");
    return false;
  }
}

// ========= SIMPLE UI =========
void drawHeader(const char* title) {
  gfx->fillScreen(BLACK);
  gfx->fillCircle(W/2, H/2, (W/2)-2, 0x0841); // subtle ring
  gfx->fillRect(0,0,W,24,0x2104); // dark header
  gfx->setCursor(8,6); gfx->setTextColor(WHITE,0x2104); gfx->setTextSize(1);
  gfx->print(title);
}
void drawButton(int x,int y,int w,int h, const char* label, uint16_t col=0x07E0) {
  gfx->fillRoundRect(x,y,w,h,8,col);
  gfx->drawRoundRect(x,y,w,h,8,BLACK);
  gfx->setCursor(x+8,y+ (h/2-4)); gfx->setTextColor(BLACK,col); gfx->setTextSize(1);
  gfx->print(label);
}
bool tapIn(int16_t tx,int16_t ty,int x,int y,int w,int h){ return tx>=x && tx<x+w && ty>=y && ty<y+h; }

void renderI2C() {
  drawHeader("I2C Scanner");
  int y=30;
  gfx->setTextColor(WHITE,BLACK); gfx->setTextSize(1);
  if (foundN==0) { gfx->setCursor(10,y); gfx->print("No devices. Tap Scan."); }
  for (int i=0;i<foundN && y<190;i++) {
    gfx->setCursor(10,y);
    char line[64];
    snprintf(line,sizeof(line),"0x%02X  %s", found[i].addr, found[i].label);
    gfx->print(line);
    y+=14;
  }
  drawButton(18,200,90,28,"Scan I2C");
  drawButton(132,200,90,28,"GPS Page",0xFD20);
}

void renderGPS() {
  drawHeader("GPS Status");
  gfx->setTextColor(WHITE,BLACK); gfx->setTextSize(1);
  int y=34;
  gfx->setCursor(10,y);   gfx->printf("UTC:   %s", gps.utc[0]?gps.utc:"--:--:--"); y+=14;
  gfx->setCursor(10,y);   gfx->printf("Local: %s", gps.local[0]?gps.local:"--:--:--"); y+=14;
  gfx->setCursor(10,y);   gfx->printf("Date:  %s", gps.date[0]?gps.date:"--------"); y+=14;
  gfx->setCursor(10,y);   gfx->printf("Fix:   %d   SatsUsed: %d", gps.fix, gps.satsUsed); y+=14;
  gfx->setCursor(10,y);   gfx->printf("GPS in view: %d", gps.inViewGPS); y+=14;
  gfx->setCursor(10,y);   gfx->printf("Max C/N0: %d dB-Hz", gps.maxCN0>=0?gps.maxCN0:0);

  drawButton(12,200,100,28,"Cold Start");
  drawButton(128,200,100,28,"I2C Page",0x07FF);
}

// ========= SETUP =========
void setup() {
  pinMode(HOLD1_PIN, OUTPUT); digitalWrite(HOLD1_PIN, HIGH);
  pinMode(HOLD2_PIN, OUTPUT); digitalWrite(HOLD2_PIN, HIGH);

  if (TFT_BL >= 0) { pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, HIGH); }

  Serial.begin(115200);
  delay(50);

  // Display
  if (!gfx->begin(40000000)) { Serial.println("GC9A01 init failed"); while(1) delay(1000); }
  gfx->setRotation(ROT);

  // Touch I2C
  Wire.begin(TP_SDA, TP_SCL, 100000);
  Wire.setTimeOut(50);
  pinMode(TP_RST, OUTPUT); digitalWrite(TP_RST, LOW); delay(5); digitalWrite(TP_RST, HIGH); delay(50);
  pinMode(TP_INT, INPUT_PULLUP); attachInterrupt(digitalPinToInterrupt(TP_INT), touch_isr, FALLING);

  // First I2C scan
  scanI2C(100000, 500);

  // GPS
  gpsAutodetect();

  // UI
  renderI2C();
  Serial.println("UI ready. Tap buttons to test.");
}

// ========= LOOP =========
void loop() {
  // Touch handling: tap-to-button
  static bool down=false;
  uint16_t rx,ry; uint8_t g,n;
  if (touch_irq) { touch_irq = false; }
  static uint32_t lt=0;
  if (millis()-lt > 30) { lt=millis();
    if (cst816s_get_point(rx,ry,g,n)) {
      uint16_t sx,sy; mapTouchToScreen(rx,ry,sx,sy);
      if (!down && n>0) { down=true; }
      else if (down && n==0) { // release -> treat as tap
        down=false;
        if (page==PAGE_I2C) {
          if (tapIn(sx,sy,18,200,90,28)) { scanI2C(100000,0); renderI2C(); }
          else if (tapIn(sx,sy,132,200,90,28)) { page=PAGE_GPS; renderGPS(); }
        } else {
          if (tapIn(sx,sy,12,200,100,28)) { gpsColdStart(); gps.maxCN0=-1; }
          else if (tapIn(sx,sy,128,200,100,28)) { page=PAGE_I2C; renderI2C(); }
        }
      }
    }
  }

  // GPS reading
  static String line;
  while (GPS.available()) {
    char c=GPS.read();
    if (c=='\r') continue;
    if (c=='\n') {
      if (line.startsWith("$") && validNMEAChecksum(line)) {
        if (line.indexOf("RMC")==3) handleRMC(line);
        else if (line.indexOf("GGA")==3) handleGGA(line);
        else if (line.indexOf("GSV")==3) handleGSV(line);
      }
      line="";
    } else {
      line += c; if (line.length()>120) line.remove(0, line.length()-120);
    }
  }

  // Periodically refresh GPS page text
  static uint32_t lastGPSDraw=0;
  if (page==PAGE_GPS && millis()-lastGPSDraw>500) {
    lastGPSDraw = millis();
    renderGPS();
  }

  delay(1);
}
