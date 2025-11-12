/*********************************************************************
 * ESP32-S3 Screenless Board Test: I2C Scanner + LC76G GPS
 * - I2C @ SDA=8, SCL=9
 * - GPS UART on RX=44 (from GPS TX), TX=43 (to GPS RX)
 * - Holds GPIO17 / GPIO42 HIGH (board-specific enables)
 * - Serial@115200: prints I2C devices and GPS status
 *
 * Controls (type in Serial Monitor):
 *   'i' = rescan I2C
 *   'b' = re-autodetect GPS baud
 *   'c' = GPS cold start
 *********************************************************************/

#include <Arduino.h>
#include <Wire.h>
#include <time.h>
#include <stdlib.h>  // setenv, unsetenv, getenv
#include <string.h>  // strdup, strncpy

// -------- Pins --------
#define I2C_SDA   8
#define I2C_SCL   9
#define HOLD1_PIN 17
#define HOLD2_PIN 42
#define GPS_RX    44   // ESP32-S3 pin receiving from GPS TX
#define GPS_TX    43   // ESP32-S3 pin transmitting to GPS RX

// -------- GPS UART --------
HardwareSerial GPS(1);
const uint32_t BAUDS[] = {9600, 38400, 115200};
uint32_t gpsBaud = 0;

// -------- Helpers: portable UTC mktime (toolchains without timegm) --------
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
void setTZ_Chicago() { setenv("TZ", "CST6CDT,M3.2.0/2,M11.1.0/2", 1); tzset(); }

// ================= I2C SCANNER =================
struct ScanResult { uint8_t addr; char label[24]; };
ScanResult found[16];
int foundN = 0;

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
static void printAddrBoth(uint8_t a7) {
  uint8_t wr=(a7<<1)|0, rd=(a7<<1)|1;
  Serial.printf("  - 7-bit 0x%02X | 8-bit W:0x%02X R:0x%02X  ", a7, wr, rd);
}
void identifyI2C(uint8_t addr, char out[24]) {
  uint8_t v;
  strcpy(out, "");
  if (addr == 0x15) { strcpy(out, "CST816S touch"); return; } // your panel

  // BNO055
  if (i2cRead8(addr, 0x00, v) && v == 0xA0) { strcpy(out, "BNO055"); return; }

  // BMP/BME family ID at 0xD0
  if (i2cRead8(addr, 0xD0, v)) {
    if (v == 0x58) { strcpy(out, "BME/BMP280 (0x58)"); return; }
    if (v == 0x60) { strcpy(out, "BME680/BMP388 (0x60)"); return; }
  }

  // MAX3010x (REG 0xFF = part ID, e.g., 0x15/0x11/0x33)
  if (addr == 0x57 && i2cRead8(addr, 0xFF, v)) {
    if (v == 0x15 || v == 0x11 || v == 0x33) { strcpy(out, "MAX3010x"); return; }
  }

  // 24xx EEPROM heuristic
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
      char label[24]; identifyI2C(a, label);
      printAddrBoth(a); Serial.println(label);
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

void probe_short(int SDA=8, int SCL=9) {
  pinMode(SDA, INPUT_PULLUP);
  pinMode(SCL, OUTPUT);
  Serial.println("Probing SDA<->SCL short:");
  for (int i=0;i<5;i++) {
    digitalWrite(SCL, LOW);  delayMicroseconds(50);
    int sdaL = digitalRead(SDA);
    digitalWrite(SCL, HIGH); delayMicroseconds(50);
    int sdaH = digitalRead(SDA);
    Serial.printf("  SCL L->H, SDA read: %d -> %d\n", sdaL, sdaH);
  }
  // If SDA flips in sync with SCL, there is a short/coupling.
}

// ================= GPS / NMEA =================
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
  // Enable RMC + GGA + GSV + ZDA at 1 Hz, and multi-constellation
  gpsSendNMEA("PMTK314,0,1,0,1,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0");
  gpsSendNMEA("PMTK220,1000");
  gpsSendNMEA("PMTK353,1,1,1,1,0"); // GPS+GLONASS+Galileo+BeiDou
}
void gpsColdStart() { gpsSendNMEA("PMTK104"); }

// State
struct GpsState {
  bool rmcValid = false;
  char utc[24] = "";
  char local[24] = "";
  char date[16] = "";
  int  fix = 0;
  int  satsUsed = 0;   // from GGA
  int  inViewGPS = 0;  // from GPGSV
  int  maxCN0 = -1;    // dB-Hz
} gps;

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
  time_t t_utc = mktime_utc(&tm_utc);

  strftime(gps.utc, sizeof(gps.utc), "%Y-%m-%d %H:%M:%S UTC", gmtime(&t_utc));
  setTZ_Chicago();
  struct tm *lt = localtime(&t_utc);
  strftime(gps.local, sizeof(gps.local), "%Y-%m-%d %I:%M:%S %p %Z", lt);
  snprintf(gps.date, sizeof(gps.date), "%04d-%02d-%02d", YY, MM, DD);
  gps.rmcValid = (stat=="A");
}
void handleGGA(const String &s) {
  String f[15]; int idx=0,k=0;
  for (int i=0;i<(int)s.length() && k<15; ++i) {
    if (s[i]==','||s[i]=='*'){ f[k++]=s.substring(idx,i); idx=i+1; }
  }
  if (k<8) return;
  gps.fix = f[6].length()? f[6].toInt():0;
  gps.satsUsed = f[7].length()? f[7].toInt():0;
}
void handleGSV(const String &s) {
  // $GxGSV,total,msg,sv_in_view, sat1,el,az,cn0, sat2,el,az,cn0, ...
  String f[28]; int idx=0,k=0;
  for (int i=0;i<(int)s.length() && k<28; ++i) {
    if (s[i]==','||s[i]=='*'){ f[k++]=s.substring(idx,i); idx=i+1; }
  }
  if (k<4) return;
  int sv_in_view = f[3].length()? f[3].toInt():0;
  if (s[1]=='G' && s[2]=='P') gps.inViewGPS = sv_in_view; // GPS page
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

// ================= Setup / Loop =================
void setup() {
  pinMode(HOLD1_PIN, OUTPUT); digitalWrite(HOLD1_PIN, HIGH);
  pinMode(HOLD2_PIN, OUTPUT); digitalWrite(HOLD2_PIN, HIGH);

  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println("\nESP32-S3 Screenless Test: I2C + GPS");
  Serial.printf("Holding GPIO%d=HIGH, GPIO%d=HIGH\n", HOLD1_PIN, HOLD2_PIN);

  Wire.begin(I2C_SDA, I2C_SCL, 100000);
  Wire.setTimeOut(50);

  scanI2C(100000, 300);

  gpsAutodetect();
  Serial.println("Commands: 'i' = scan I2C, 'b' = re-detect GPS baud, 'c' = GPS cold start\n");
}

void loop() {
  // User commands
  if (Serial.available()) {
    char c = Serial.read();
    if (c=='i' || c=='I') scanI2C(100000, 0);
    else if (c=='b' || c=='B') { gpsAutodetect(); }
    else if (c=='c' || c=='C') { gpsColdStart(); gps.maxCN0 = -1; }
    else if (c=='p' || c=='P') {probe_short();}
  }

  // GPS read/parse/print (raw + summaries)
  static String line;
  while (GPS.available()) {
    char ch = GPS.read();
    if (ch == '\r') continue;
    if (ch == '\n') {
      if (line.startsWith("$")) {
        bool ok = validNMEAChecksum(line);
        Serial.printf("%s %s\n", line.c_str(), ok ? "" : "(bad CS)");
        if (ok) {
          if (line.indexOf("RMC") == 3) { handleRMC(line); Serial.printf("[RMC] %s | %s\n", gps.utc, gps.local); }
          else if (line.indexOf("GGA") == 3) { handleGGA(line); Serial.printf("[GGA] fix=%d satsUsed=%d\n", gps.fix, gps.satsUsed); }
          else if (line.indexOf("GSV") == 3) { handleGSV(line); Serial.printf("[GSV] GPS in view=%d maxCN0=%d\n", gps.inViewGPS, gps.maxCN0); }
        }
      }
      line = "";
    } else {
      line += ch;
      if (line.length() > 160) line.remove(0, line.length()-160);
    }
  }

  delay(1);
}
