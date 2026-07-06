// =================================================================
//  ESP32 Smart Weather Station — Firmware v2
//  Hardware : ESP32 + ST7735 TFT (128×160) + SSD1306 OLED (128×64)
//             + DHT11 + BMP180
//  Themes   : Nord | Cyberpunk | Coder | Gruvbox  (web + TFT in sync)
//  API      : /api/data  /api/settings  /api/notes  /api/pomodoro
//             /api/status  /api/page  /api/history
// =================================================================

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_BMP085.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <time.h>
#include <math.h>

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>

// =================================================================
// ---- Pin Definitions ----
// =================================================================
#define OLED_SDA    21
#define OLED_SCL    22
#define OLED_RESET  -1
#define OLED_ADDR   0x3C

#define TFT_CS      15
#define TFT_DC      16
#define TFT_RST     17
#define TFT_MOSI    13
#define TFT_SCLK    14

#define DHT_PIN     4
#define DHT_TYPE    DHT11

// =================================================================
// ---- WiFi & NTP ----
// =================================================================
const char* WIFI_SSID     = "Airtel_a204";
const char* WIFI_PASSWORD = "rahulkhanki";

const long  gmtOffset_sec      = 19800L;  // UTC+5:30
const int   daylightOffset_sec = 0;

// =================================================================
// ---- Nord Base Palette (RGB565 constants kept as aliases) ----
// =================================================================
#define NORD0   0x29A8   // #2E3440
#define NORD1   0x3A0A   // #3B4252
#define NORD2   0x426B   // #434C5E
#define NORD3   0x4AAD   // #4C566A
#define NORD4   0xDEFD   // #D8DEE9
#define NORD6   0xEF7E   // #ECEFF4
#define NORD7   0x8DF7   // #8FBCBB
#define NORD8   0x8E1A   // #88C0D0 Frost Cyan
#define NORD9   0x8518   // #81A1C1
#define NORD10  0x5C15   // #5E81AC
#define NORD11  0xBB0D   // #BF616A Red
#define NORD12  0xD42E   // #D08770 Orange
#define NORD13  0xEE51   // #EBCB8B Yellow
#define NORD14  0xA5F1   // #A3BE8C Green
#define NORD15  0xB475   // #B48EAD Purple

// =================================================================
// ---- Multi-Theme Palette System ----
// Semantic field names map to Nord's role hierarchy:
//   bg=NORD0 card=NORD1 raised=NORD2 border=NORD3
//   muted=NORD4 text=NORD6 teal=NORD7 accent=NORD8
//   blue=NORD9 dblue=NORD10 red=NORD11 orange=NORD12
//   yellow=NORD13 green=NORD14 purple=NORD15
// =================================================================
struct TFTPalette {
  uint16_t bg, card, raised, border, muted, text;
  uint16_t teal, accent, blue, dblue;
  uint16_t red, orange, yellow, green, purple;
};

// ---- Theme 0: Nord (cool arctic) ----
static const TFTPalette PALETTE_NORD = {
  .bg=0x29A8,.card=0x3A0A,.raised=0x426B,.border=0x4AAD,.muted=0xDEFD,.text=0xEF7E,
  .teal=0x8DF7,.accent=0x8E1A,.blue=0x8518,.dblue=0x5C15,
  .red=0xBB0D,.orange=0xD42E,.yellow=0xEE51,.green=0xA5F1,.purple=0xB475
};

// ---- Theme 1: Cyberpunk (neon on dark navy) ----
// bg=#0D0D1A card=#1A1A2E raised=#16213E border=#550088
// accent=#FF00FF cyan=#00FFFF green=#00FF41 purple=#9D00FF
static const TFTPalette PALETTE_CYBERPUNK = {
  .bg=0x0863,.card=0x18C5,.raised=0x1107,.border=0x4011,.muted=0x32AC,.text=0xFFFF,
  .teal=0x07FF,.accent=0xF81F,.blue=0x07FF,.dblue=0x05FF,
  .red=0xF80A,.orange=0xFEA0,.yellow=0xFFE0,.green=0x07E8,.purple=0x981F
};

// ---- Theme 2: Coder / Terminal (green-on-black) ----
// bg≈#0A0A0A card=#0F1117 accent=#00FF41
static const TFTPalette PALETTE_CODER = {
  .bg=0x0841,.card=0x0882,.raised=0x00C0,.border=0x02A0,.muted=0x05C6,.text=0x07E8,
  .teal=0x07F5,.accent=0x07E8,.blue=0x37E6,.dblue=0x0400,
  .red=0xF986,.orange=0xFD80,.yellow=0xFFE0,.green=0x07E8,.purple=0x07F5
};

// ---- Theme 3: Gruvbox (warm retro amber) ----
// bg=#1D2021 card=#282828 fg=#EBDBB2 yellow=#D79921
static const TFTPalette PALETTE_GRUVBOX = {
  .bg=0x1904,.card=0x2945,.raised=0x39C6,.border=0x5248,.muted=0x940E,.text=0xEED6,
  .teal=0x6CED,.accent=0xD4C4,.blue=0x4431,.dblue=0x2A49,
  .red=0xC923,.orange=0xD2E1,.yellow=0xFDE5,.green=0x9CA3,.purple=0xB310
};

extern int tftThemeIndex;

const TFTPalette& getPalette() {
  switch (tftThemeIndex) {
    case 1: return PALETTE_CYBERPUNK;
    case 2: return PALETTE_CODER;
    case 3: return PALETTE_GRUVBOX;
    default: return PALETTE_NORD;
  }
}

// =================================================================
// ---- Display Objects ----
// =================================================================
Adafruit_SSD1306 oled(128, 64, &Wire, OLED_RESET);
Adafruit_ST7735  tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
Adafruit_BMP085  bmp;
DHT              dht(DHT_PIN, DHT_TYPE);
WebServer        server(80);

// =================================================================
// ---- OLED Bitmaps (16×16 px) ----
// =================================================================
const unsigned char bmp_thermometer[] PROGMEM = {
  0x01,0x80,0x02,0x40,0x02,0x40,0x02,0x40,0x02,0x40,0x02,0x40,0x02,0x40,0x02,0x40,
  0x03,0xc0,0x07,0xe0,0x0f,0xf0,0x0f,0xf0,0x0f,0xf0,0x07,0xe0,0x03,0xc0,0x00,0x00
};
const unsigned char bmp_droplet[] PROGMEM = {
  0x01,0x80,0x01,0x80,0x03,0xc0,0x03,0xc0,0x07,0xe0,0x07,0xe0,0x0f,0xf0,0x1f,0xf8,
  0x1f,0xf8,0x3f,0xfc,0x3f,0xfc,0x3f,0xfc,0x1f,0xf8,0x0f,0xf0,0x03,0xc0,0x00,0x00
};
const unsigned char bmp_cloud[] PROGMEM = {
  0x00,0x00,0x03,0xc0,0x0f,0xf0,0x1e,0x78,0x38,0x1c,0x70,0x0e,0xf0,0x0f,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f,0xfe,0x3f,0xfc,0x00,0x00,0x00,0x00
};

// =================================================================
// ---- Sensor State ----
// =================================================================
struct SensorState {
  float tempDHT=NAN, humDHT=NAN, tempBME=NAN, pressBME=NAN;
  unsigned long lastUpdate=0;
  float tempHigh=-100.0f, tempLow=100.0f;
  int   lastDay=-1;
};
SensorState state;

// ---- History Ring Buffer ----
#define HISTORY_SIZE 100
float    tftHistTemp[HISTORY_SIZE];
uint32_t tftHistHeap[HISTORY_SIZE];
int8_t   tftHistRssi[HISTORY_SIZE];
uint8_t  histCount=0, histHead=0;

// =================================================================
// ---- Settings — forward declarations needed by getPalette() ----
// =================================================================
int  oledMode           = 0;
int  tftThemeIndex      = 0;  // 0=Nord 1=Cyberpunk 2=Coder 3=Gruvbox
bool tftCarouselEnabled = true;
int  tftCarouselSpeed   = 8000;
bool pageEnabled[10]    = {true,true,true,true,true,false,false,false,false,false};
int  notesFontSize      = 1;
int  todoFontSize       = 1;

// Backward-compat helper
uint16_t getThemeColor() { return getPalette().accent; }

// =================================================================
// ---- Productivity Data ----
// =================================================================
String tftNotes    = "";
String tftTodos[5];
int    todoCount   = 0;

bool          pomodoroActive  = false;
unsigned long pomodoroEndTime = 0;

int           tftPage        = 0;
unsigned long lastPageChange = 0;
float         animAngle      = 0.0f;

unsigned long lastLogTime   = 0;
const unsigned long LOG_INTERVAL = 10UL * 60UL * 1000UL;

// =================================================================
// ---- Init Helpers ----
// =================================================================
void initDisplays() {
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR))
    Serial.println(F("[OLED] Init failed"));
  oled.clearDisplay(); oled.setTextColor(SSD1306_WHITE);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(2);
  tft.fillScreen(NORD0);
}

void initSensors() {
  dht.begin();
  if (!bmp.begin()) Serial.println(F("[BMP] Init failed"));
}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print(F("[WiFi] Connecting"));
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-t0 < 15000UL) { delay(500); Serial.print('.'); }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("[WiFi] ")); Serial.println(WiFi.localIP());
    if (MDNS.begin("esp2display")) Serial.println(F("[mDNS] esp2display.local"));
  } else {
    Serial.println(F("[WiFi] Timeout — offline"));
  }
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
}

void initFS() {
  if (!LittleFS.begin(true)) { Serial.println(F("[FS] Mount failed")); return; }
  tftHistHeap[histHead] = ESP.getFreeHeap();
  tftHistRssi[histHead] = (int8_t)WiFi.RSSI();
  if (!LittleFS.exists("/history.csv")) {
    File f = LittleFS.open("/history.csv","w");
    if (f) { f.println(F("timestamp,dhtTemp,dhtHum,bmpTemp,bmpPress")); f.close(); }
  }
  if (LittleFS.exists("/config.json")) {
    File f = LittleFS.open("/config.json","r");
    StaticJsonDocument<512> doc;
    if (!deserializeJson(doc, f)) {
      oledMode           = doc["oledMode"]    | oledMode;
      tftThemeIndex      = doc["tftTheme"]    | tftThemeIndex;
      tftCarouselEnabled = doc["tftCarousel"] | tftCarouselEnabled;
      tftCarouselSpeed   = doc["tftSpeed"]    | tftCarouselSpeed;
      notesFontSize      = constrain((int)(doc["notesFontSize"]|1),1,2);
      todoFontSize       = constrain((int)(doc["todoFontSize"] |1),1,2);
      if (doc.containsKey("pages") && doc["pages"].is<JsonArray>()) {
        JsonArray arr = doc["pages"].as<JsonArray>();
        for (int i=0;i<10;i++) pageEnabled[i] = (i<(int)arr.size()) ? arr[i].as<bool>() : false;
      }
    }
    f.close();
  }
  if (LittleFS.exists("/notes.json")) {
    File f = LittleFS.open("/notes.json","r");
    StaticJsonDocument<1024> doc;
    if (!deserializeJson(doc, f)) {
      tftNotes = doc["notes"] | "";
      todoCount = 0;
      for (JsonVariant v : doc["todos"].as<JsonArray>())
        if (todoCount < 5) tftTodos[todoCount++] = v.as<String>();
    }
    f.close();
  }
}

void saveSettingsFS() {
  File f = LittleFS.open("/config.json","w");
  if (!f) { Serial.println(F("[FS] config.json write fail")); return; }
  StaticJsonDocument<512> doc;
  doc["oledMode"]=oledMode; doc["tftTheme"]=tftThemeIndex;
  doc["tftCarousel"]=tftCarouselEnabled; doc["tftSpeed"]=tftCarouselSpeed;
  doc["notesFontSize"]=notesFontSize; doc["todoFontSize"]=todoFontSize;
  JsonArray pages = doc.createNestedArray("pages");
  for (int i=0;i<10;i++) pages.add(pageEnabled[i]);
  serializeJson(doc, f); f.close();
}

void logData() {
  struct tm ti;
  if (!getLocalTime(&ti)) return;
  File f = LittleFS.open("/history.csv","a");
  if (!f) return;
  char ts[24]; strftime(ts,sizeof(ts),"%Y-%m-%d %H:%M:%S",&ti);
  f.printf("%s,%.2f,%.2f,%.2f,%.2f\n",ts,state.tempDHT,state.humDHT,state.tempBME,state.pressBME);
  f.close();
  tftHistTemp[histHead]=state.tempBME;
  tftHistHeap[histHead]=ESP.getFreeHeap();
  tftHistRssi[histHead]=(int8_t)WiFi.RSSI();
  histHead=(histHead+1)%HISTORY_SIZE;
  if (histCount<HISTORY_SIZE) histCount++;
}

// =================================================================
// ---- Sensor Reading ----
// =================================================================
void refreshSensors() {
  float t=dht.readTemperature(), h=dht.readHumidity();
  if (!isnan(t)&&!isnan(h)) { state.tempDHT=t; state.humDHT=h; }
  float bt=bmp.readTemperature(); int32_t p=bmp.readPressure();
  if (!isnan(bt) && p>0) {
    state.tempBME=bt; state.pressBME=(float)p;
    struct tm ti;
    if (getLocalTime(&ti)) {
      if (state.lastDay!=-1 && ti.tm_mday!=state.lastDay) { state.tempHigh=bt; state.tempLow=bt; }
      state.lastDay=ti.tm_mday;
    }
    if (bt>state.tempHigh) state.tempHigh=bt;
    if (bt<state.tempLow)  state.tempLow=bt;
  }
  state.lastUpdate=millis();
}

// =================================================================
// ---- API Handlers ----
// =================================================================
void handleDataJson() {
  StaticJsonDocument<256> doc;
  doc["tempDHT"]=state.tempDHT; doc["humDHT"]=state.humDHT;
  doc["tempBME"]=state.tempBME; doc["pressBME"]=state.pressBME;
  doc["uptime"]=millis()/1000; doc["freeHeap"]=ESP.getFreeHeap();
  doc["rssi"]=WiFi.RSSI();
  String j; serializeJson(doc,j); server.send(200,F("application/json"),j);
}

void handleHistoryCSV() {
  File f=LittleFS.open("/history.csv","r");
  if (!f) { server.send(500,F("text/plain"),F("Not found")); return; }
  server.streamFile(f,F("text/csv")); f.close();
}

void handleGetSettings() {
  StaticJsonDocument<512> doc;
  doc["oledMode"]=oledMode; doc["tftTheme"]=tftThemeIndex;
  doc["tftCarousel"]=tftCarouselEnabled; doc["tftSpeed"]=tftCarouselSpeed;
  doc["notesFontSize"]=notesFontSize; doc["todoFontSize"]=todoFontSize;
  JsonArray pages=doc.createNestedArray("pages");
  for (int i=0;i<10;i++) pages.add(pageEnabled[i]);
  String j; serializeJson(doc,j); server.send(200,F("application/json"),j);
}

void handlePostSettings() {
  if (!server.hasArg("plain")) { server.send(400,F("application/json"),F("{\"status\":\"error\"}")); return; }
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc,server.arg("plain"))) { server.send(400,F("application/json"),F("{\"status\":\"error\"}")); return; }
  oledMode           = doc["oledMode"]    | oledMode;
  tftThemeIndex      = constrain((int)(doc["tftTheme"]|tftThemeIndex),0,3);
  tftCarouselEnabled = doc["tftCarousel"] | tftCarouselEnabled;
  tftCarouselSpeed   = doc["tftSpeed"]    | tftCarouselSpeed;
  notesFontSize      = constrain((int)(doc["notesFontSize"]|notesFontSize),1,2);
  todoFontSize       = constrain((int)(doc["todoFontSize"] |todoFontSize ),1,2);
  if (doc.containsKey("pages") && doc["pages"].is<JsonArray>()) {
    JsonArray arr=doc["pages"].as<JsonArray>();
    for (int i=0;i<10;i++) pageEnabled[i]=(i<(int)arr.size())?arr[i].as<bool>():pageEnabled[i];
  }
  saveSettingsFS();
  server.send(200,F("application/json"),F("{\"status\":\"success\"}"));
}

void handleGetNotes() {
  StaticJsonDocument<1024> doc;
  doc["notes"]=tftNotes;
  JsonArray arr=doc.createNestedArray("todos");
  for (int i=0;i<todoCount;i++) arr.add(tftTodos[i]);
  String j; serializeJson(doc,j); server.send(200,F("application/json"),j);
}

void handlePostNotes() {
  if (!server.hasArg("plain")) { server.send(400,F("application/json"),F("{\"status\":\"error\"}")); return; }
  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc,server.arg("plain"))) { server.send(400,F("application/json"),F("{\"status\":\"error\"}")); return; }
  tftNotes=doc["notes"].as<String>(); todoCount=0;
  for (JsonVariant v : doc["todos"].as<JsonArray>())
    if (todoCount<5) tftTodos[todoCount++]=v.as<String>();
  File f=LittleFS.open("/notes.json","w");
  if (f) { serializeJson(doc,f); f.close(); }
  server.send(200,F("application/json"),F("{\"status\":\"success\"}"));
}

void handlePostPomodoro() {
  if (!server.hasArg("plain")) { server.send(400,F("application/json"),F("{\"status\":\"error\"}")); return; }
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc,server.arg("plain"))) { server.send(400,F("application/json"),F("{\"status\":\"error\"}")); return; }
  String action=doc["action"]|"";
  if (action=="start") {
    pomodoroActive=true;
    int mins=doc["duration"]|25;
    pomodoroEndTime=millis()+(unsigned long)(mins*60000UL);
    if (pageEnabled[8]) { tftPage=8; lastPageChange=millis(); }
  } else if (action=="stop") {
    pomodoroActive=false;
  }
  server.send(200,F("application/json"),F("{\"status\":\"success\"}"));
}

// ---- NEW: Current status (page, pomodoro, theme, rssi) ----
void handleGetStatus() {
  StaticJsonDocument<128> doc;
  doc["page"]          = tftPage;
  doc["pomodoroActive"]= pomodoroActive;
  long rem = pomodoroActive ? max(0L,(long)pomodoroEndTime-(long)millis()) : 0L;
  doc["pomodoroRemSec"]= (int)(rem/1000);
  doc["theme"]         = tftThemeIndex;
  doc["rssi"]          = WiFi.RSSI();
  String j; serializeJson(doc,j); server.send(200,F("application/json"),j);
}

// ---- NEW: Jump to a specific TFT page from web ----
void handlePostPage() {
  if (!server.hasArg("plain")) { server.send(400,F("application/json"),F("{\"status\":\"error\"}")); return; }
  StaticJsonDocument<64> doc;
  if (deserializeJson(doc,server.arg("plain"))) { server.send(400,F("application/json"),F("{\"status\":\"error\"}")); return; }
  int pg=doc["page"]|-1;
  if (pg>=0 && pg<10) { tftPage=pg; lastPageChange=millis(); }
  server.send(200,F("application/json"),F("{\"status\":\"success\"}"));
}

// =================================================================
// ---- OLED Rendering ----
// =================================================================
void renderOLED() {
  oled.clearDisplay();
  struct tm ti; bool hasTime=getLocalTime(&ti);
  if (oledMode==1) {
    if (hasTime) {
      oled.setFont(); oled.setTextSize(1); oled.setCursor(0,0);
      oled.printf("%02d/%02d/%02d", ti.tm_mday, ti.tm_mon+1, ti.tm_year%100);
      String ip = WiFi.localIP().toString();
      oled.setCursor(128 - (ip.length()*6), 0); oled.print(ip);
      oled.drawLine(0, 10, 128, 10, SSD1306_WHITE);
      oled.setFont(&FreeSans18pt7b); oled.setCursor(2,45);
      oled.printf("%02d:%02d",ti.tm_hour,ti.tm_min);
      oled.setFont(); oled.setCursor(102,18); oled.printf("%02d",ti.tm_sec);
    } else { oled.setFont(); oled.setCursor(0,20); oled.print(F("Waiting NTP...")); }
  } else if (oledMode==2) {
    oled.setFont(); oled.setTextSize(1);
    oled.setCursor(0,0);  oled.print(F("--- SYS INFO ---"));
    oled.setCursor(0,15); oled.print(F("IP: ")); oled.print(WiFi.localIP());
    oled.setCursor(0,28); oled.printf("Heap: %d KB",ESP.getFreeHeap()/1024);
    oled.setCursor(0,41); oled.printf("RSSI: %d dBm",WiFi.RSSI());
    uint32_t up=millis()/1000;
    oled.setCursor(0,54); oled.printf("Up: %dh %dm",up/3600,(up%3600)/60);
  } else {
    oled.setFont(); oled.setTextSize(1);
    oled.setCursor(0,0);
    if (hasTime) {
      String ip=WiFi.localIP().toString();
      oled.printf("%02d:%02d:%02d .%s",ti.tm_hour,ti.tm_min,ti.tm_sec,
        ip.substring(ip.lastIndexOf('.')+1).c_str());
    } else { oled.print(F("Wait NTP")); }
    oled.drawLine(0,10,128,10,SSD1306_WHITE);
    oled.setCursor(15,14); oled.print(F("IN"));
    oled.drawLine(0,23,60,23,SSD1306_WHITE);
    oled.drawBitmap(0,27,bmp_thermometer,16,16,SSD1306_WHITE);
    oled.setCursor(18,31); oled.printf("%.1f C",state.tempDHT);
    oled.drawBitmap(0,47,bmp_droplet,16,16,SSD1306_WHITE);
    oled.setCursor(18,51); oled.printf("%.0f %%",state.humDHT);
    oled.drawLine(64,12,64,64,SSD1306_WHITE);
    oled.setCursor(85,14); oled.print(F("OUT"));
    oled.drawLine(68,23,128,23,SSD1306_WHITE);
    oled.drawBitmap(68,27,bmp_thermometer,16,16,SSD1306_WHITE);
    oled.setCursor(86,31); oled.printf("%.1f C",state.tempBME);
    oled.drawBitmap(68,47,bmp_cloud,16,16,SSD1306_WHITE);
    oled.setCursor(86,51); oled.printf("%.0f hP",state.pressBME/100.0f);
  }
  oled.display();
}

// =================================================================
// ---- TFT Graphics Primitives ----
// =================================================================
void drawArc(int cx,int cy,int r,int thick,float pct,uint16_t col) {
  float end=(pct/100.0f)*360.0f;
  for (int t=0;t<thick;t++) {
    int rad=r-t;
    for (float a=0.0f;a<end;a+=2.0f) {
      float rd=(a-90.0f)*(float)M_PI/180.0f;
      tft.drawPixel(cx+(int)(rad*cosf(rd)),cy+(int)(rad*sinf(rd)),col);
    }
  }
}
inline void drawArcTrack(int cx,int cy,int r,int thick,uint16_t col) { drawArc(cx,cy,r,thick,100.0f,col); }

void drawAnalogDial(int cx,int cy,int r,float temp,uint16_t needleCol,const TFTPalette& P) {
  for (int d=-135;d<=135;d+=27) {
    float rd=(d-90.0f)*(float)M_PI/180.0f;
    tft.drawLine(cx+(int)(r*cosf(rd)),cy+(int)(r*sinf(rd)),cx+(int)((r-4)*cosf(rd)),cy+(int)((r-4)*sinf(rd)),P.border);
  }
  for (int d=-135;d<=135;d+=54) {
    float rd=(d-90.0f)*(float)M_PI/180.0f;
    tft.drawLine(cx+(int)(r*cosf(rd)),cy+(int)(r*sinf(rd)),cx+(int)((r-8)*cosf(rd)),cy+(int)((r-8)*sinf(rd)),P.text);
  }
  float angle=constrain(-135.0f+((temp+10.0f)/60.0f)*270.0f,-135.0f,135.0f);
  float rd=(angle-90.0f)*(float)M_PI/180.0f;
  int ex=cx+(int)((r-10)*cosf(rd)),ey=cy+(int)((r-10)*sinf(rd));
  tft.drawLine(cx,cy,ex,ey,needleCol);
  tft.drawLine(cx-1,cy,ex,ey,needleCol);
  tft.drawLine(cx+1,cy,ex,ey,needleCol);
  tft.fillCircle(cx,cy,4,needleCol);
  tft.fillCircle(cx,cy,2,P.bg);
}

void drawAnimatedSun(int cx,int cy,int r,float off,uint16_t col) {
  tft.fillCircle(cx,cy,r,col);
  tft.fillCircle(cx,cy,r-3,NORD13);
  for (int i=0;i<360;i+=45) {
    float rd=(i+off)*(float)M_PI/180.0f;
    tft.drawLine(cx+(int)((r+2)*cosf(rd)),cy+(int)((r+2)*sinf(rd)),
                 cx+(int)((r+7)*cosf(rd)),cy+(int)((r+7)*sinf(rd)),col);
  }
}

// =================================================================
// ---- TFT Renderer — Multi-Theme ----
// =================================================================
void renderTFT(bool forceClear) {
  const TFTPalette& P = getPalette();

  if (forceClear) tft.fillScreen(P.bg);

  struct tm ti; const bool hasTime=getLocalTime(&ti);

  // ---- Global top bar ----
  tft.fillRect(0,0,tft.width(),15,P.card);
  tft.drawFastHLine(0,15,tft.width(),P.border);
  tft.setFont(); tft.setTextSize(1);

  tft.setTextColor(P.accent,P.card);
  tft.setCursor(3,4);
  if (hasTime) {
    char d[10]; strftime(d,10,"%a %e",&ti); tft.print(d);
  } else { tft.print(F("--")); }

  int activeDot=0,curIdx=0;
  for (int i=0;i<10;i++) { if (pageEnabled[i]) { activeDot++; if(i==tftPage) curIdx=activeDot; } }
  if (activeDot>1) {
    char ps[8]; snprintf(ps,8,"%d/%d",curIdx,activeDot);
    int px=(tft.width()/2)-((int)(strlen(ps)*6)/2);
    tft.setTextColor(P.muted,P.card); tft.setCursor(px,4); tft.print(ps);
  }
  if (hasTime) {
    char ts[8]; snprintf(ts,8,"%02d:%02d",ti.tm_hour,ti.tm_min);
    tft.setTextColor(P.text,P.card);
    tft.setCursor(tft.width()-(int)(strlen(ts)*6)-3,4); tft.print(ts);
  }

  const int TOP=17;

  switch (tftPage) {

    // ==============================================================
    case 0: { // Dashboard
    // ==============================================================
      if (forceClear) {
        tft.setFont(); tft.setTextSize(1); tft.setTextColor(P.border,P.bg);
        tft.setCursor(14,TOP+90); tft.print(F("HUMIDITY"));
        tft.setCursor(80,TOP+90); tft.print(F("PRESS"));
      }
      tft.fillRect(28,TOP,73,72,P.bg);
      animAngle+=12.0f; if (animAngle>=360.0f) animAngle-=360.0f;
      float ph=state.pressBME/100.0f;
      if (ph>1015.0f) {
        drawAnimatedSun(64,TOP+35,11,animAngle,P.orange);
      } else if (ph>1000.0f) {
        tft.fillCircle(52,TOP+40,9,P.blue); tft.fillCircle(64,TOP+32,13,P.blue);
        tft.fillCircle(76,TOP+40,9,P.blue); tft.fillRect(52,TOP+36,25,13,P.blue);
        tft.fillCircle(64,TOP+32,10,P.accent); tft.fillRect(61,TOP+32,8,8,P.accent);
      } else {
        tft.fillCircle(52,TOP+37,9,P.dblue); tft.fillCircle(64,TOP+29,13,P.dblue);
        tft.fillCircle(76,TOP+37,9,P.dblue); tft.fillRect(52,TOP+33,25,13,P.dblue);
        int ro=(int)(animAngle/36)%7;
        for (int dd=0;dd<3;dd++) { int dx=50+dd*12; tft.drawLine(dx,TOP+50+ro,dx-2,TOP+58+ro,P.accent); }
      }
      tft.fillRect(0,TOP+72,128,22,P.bg);
      char tb[10]; snprintf(tb,10,"%.1f",state.tempBME);
      tft.setFont(&FreeSans18pt7b); tft.setTextColor(P.yellow,P.bg);
      int16_t bx,by; uint16_t bw,bh;
      tft.getTextBounds(tb,0,0,&bx,&by,&bw,&bh);
      int tx=(128-(int)bw)/2; tft.setCursor(tx,TOP+90); tft.print(tb);
      tft.drawCircle(tx+(int)bw+5,TOP+73,3,P.yellow); tft.drawCircle(tx+(int)bw+5,TOP+73,2,P.yellow);
      // Humidity pill
      tft.fillRoundRect(3,TOP+96,58,26,6,P.card); tft.drawRoundRect(3,TOP+96,58,26,6,P.blue);
      char hb[8]; snprintf(hb,8,"%d%%",(int)state.humDHT);
      tft.getTextBounds(hb,0,0,&bx,&by,&bw,&bh);
      tft.setTextColor(P.accent,P.card); tft.setFont(&FreeSans12pt7b);
      tft.setCursor(32-(int)(bw/2),TOP+115); tft.print(hb);
      // Pressure pill
      tft.fillRoundRect(67,TOP+96,58,26,6,P.card); tft.drawRoundRect(67,TOP+96,58,26,6,P.blue);
      char pb[8]; snprintf(pb,8,"%d",(int)(state.pressBME/100.0f));
      tft.getTextBounds(pb,0,0,&bx,&by,&bw,&bh);
      tft.setTextColor(P.green,P.card); tft.setCursor(96-(int)(bw/2),TOP+115); tft.print(pb);
      tft.setFont();
      break;
    }

    // ==============================================================
    case 1: { // Analog Temp Dial
    // ==============================================================
      int cx=tft.width()/2,cy=97,r=44;
      if (forceClear) {
        tft.setFont(&FreeSans9pt7b); tft.setTextColor(P.accent,P.bg);
        tft.setCursor(16,TOP+16); tft.print(F("TEMPERATURE"));
        tft.drawCircle(cx,cy,r+3,P.raised); tft.drawCircle(cx,cy,r+4,P.border);
      }
      tft.fillCircle(cx,cy,r+2,P.bg);
      drawAnalogDial(cx,cy,r,state.tempBME,P.red,P);
      char vb[10]; snprintf(vb,10,"%.1f",state.tempBME);
      tft.setFont(&FreeSansBold12pt7b);
      int16_t bx,by; uint16_t bw,bh;
      tft.getTextBounds(vb,0,0,&bx,&by,&bw,&bh);
      tft.setTextColor(P.yellow,P.bg); tft.setCursor(cx-(int)(bw/2)-4,cy+28); tft.print(vb);
      tft.setFont(); tft.setTextColor(P.text,P.bg); tft.setCursor(cx+(int)(bw/2)-2,cy+18); tft.print(F("\xF8" "C"));
      tft.setTextSize(1); tft.setTextColor(P.border,P.bg);
      tft.setCursor(cx-r+1,cy+r-8); tft.print(F("-10"));
      tft.setCursor(cx+r-14,cy+r-8); tft.print(F("50"));
      tft.fillRoundRect(10,TOP+3,108,13,4,P.card); tft.setTextColor(P.orange,P.card);
      tft.setCursor(14,TOP+7); tft.printf("H:%.1f",state.tempHigh==-100.0f?state.tempBME:state.tempHigh);
      tft.setTextColor(P.blue,P.card); tft.setCursor(76,TOP+7);
      tft.printf("L:%.1f",state.tempLow==100.0f?state.tempBME:state.tempLow);
      break;
    }

    // ==============================================================
    case 2: { // Atmosphere Arc Rings
    // ==============================================================
      if (forceClear) {
        tft.setFont(&FreeSans9pt7b); tft.setTextColor(P.accent,P.bg);
        tft.setCursor(22,TOP+16); tft.print(F("ATMOSPHERE"));
      }
      int cx=tft.width()/2,cy=94;
      tft.fillRect(5,TOP+20,tft.width()-10,118,P.bg);
      drawArcTrack(cx,cy,46,7,P.raised);
      drawArc(cx,cy,46,7,constrain(state.humDHT,0.0f,100.0f),P.accent);
      float ph=state.pressBME/100.0f;
      float pp=constrain(((ph-900.0f)/200.0f)*100.0f,0.0f,100.0f);
      drawArcTrack(cx,cy,34,7,P.raised);
      drawArc(cx,cy,34,7,pp,P.dblue);
      tft.setFont(); tft.setTextSize(1);
      tft.setTextColor(P.accent,P.bg); tft.setCursor(cx-14,cy-12); tft.printf("%.0f%%",state.humDHT);
      tft.drawFastHLine(cx-12,cy,24,P.border);
      tft.setTextColor(P.dblue,P.bg); tft.setCursor(cx-17,cy+5); tft.printf("%.0f",ph);
      tft.setTextColor(P.muted,P.bg);
      tft.setCursor(6,cy+55); tft.print(F("HUM"));
      tft.setCursor(98,cy+55); tft.print(F("hPa"));
      tft.fillRoundRect(10,cy+62,108,14,4,P.card); tft.setTextColor(P.accent,P.card);
      tft.setCursor(14,cy+67); tft.printf("Hum %.0f%%  Press %.0f",state.humDHT,ph);
      break;
    }

    // ==============================================================
    case 3: { // Notes
    // ==============================================================
      if (forceClear) {
        tft.fillRoundRect(2,TOP,tft.width()-4,20,5,P.dblue);
        tft.setFont(&FreeSans9pt7b); tft.setTextColor(P.text,P.dblue);
        tft.setCursor(8,TOP+14); tft.print(F("NOTES"));
        tft.fillRect(tft.width()-18,TOP+4,12,2,P.accent);
        tft.fillRect(tft.width()-18,TOP+8,12,2,P.accent);
        tft.fillRect(tft.width()-18,TOP+12,8,2,P.accent);
      }
      tft.fillRoundRect(2,TOP+22,tft.width()-4,tft.height()-TOP-24,5,P.card);
      tft.drawRoundRect(2,TOP+22,tft.width()-4,tft.height()-TOP-24,5,P.border);
      tft.setFont(); tft.setTextSize(notesFontSize); tft.setTextWrap(false);
      if (tftNotes.length()==0) {
        tft.setTextColor(P.border,P.card); tft.setCursor(10,TOP+32); tft.print(F("No notes yet."));
      } else {
        tft.setTextColor(P.text,P.card);
        int cx2=8,cy2=TOP+30; tft.setCursor(cx2,cy2);
        for (size_t i=0;i<tftNotes.length();i++) {
          char c=tftNotes[i];
          if (c=='\n'||cx2+(6*notesFontSize)>tft.width()-8) {
            cy2+=8*notesFontSize+2; cx2=8; tft.setCursor(cx2,cy2); if (c=='\n') continue;
          }
          tft.print(c); cx2+=6*notesFontSize;
        }
      }
      tft.setTextSize(1); tft.setFont();
      break;
    }

    // ==============================================================
    case 4: { // Tasks / To-Do
    // ==============================================================
      if (forceClear) {
        tft.fillRoundRect(2,TOP,tft.width()-4,20,5,P.purple);
        tft.setFont(&FreeSans9pt7b); tft.setTextColor(P.text,P.purple);
        tft.setCursor(8,TOP+14); tft.print(F("TASKS"));
        tft.setFont(); char bg[5]; snprintf(bg,5,"%d/5",todoCount);
        tft.setTextColor(P.text,P.purple); tft.setCursor(tft.width()-26,TOP+6); tft.print(bg);
      }
      tft.fillRect(2,TOP+22,tft.width()-4,tft.height()-TOP-24,P.bg);
      tft.setFont(); tft.setTextSize(todoFontSize);
      if (todoCount==0) {
        tft.setTextColor(P.border,P.bg); tft.setCursor(10,TOP+32); tft.print(F("All done! :)"));
      } else {
        int y=TOP+26,lh=(7*todoFontSize)+9;
        for (int i=0;i<todoCount;i++) {
          tft.drawRoundRect(5,y,6*todoFontSize,6*todoFontSize,1,P.accent);
          tft.setTextColor(P.text,P.bg); tft.setCursor(8+(8*todoFontSize),y+1);
          tft.print(tftTodos[i].substring(0,(todoFontSize==1)?18:9));
          y+=lh;
          if (i<todoCount-1) tft.drawFastHLine(4,y-3,tft.width()-8,P.raised);
        }
      }
      tft.setTextSize(1); tft.setFont();
      break;
    }

    // ==============================================================
    case 5: { // Barometric Forecast
    // ==============================================================
      if (forceClear) {
        tft.fillRoundRect(2,TOP,tft.width()-4,20,5,P.blue);
        tft.setFont(&FreeSans9pt7b); tft.setTextColor(P.text,P.blue);
        tft.setCursor(8,TOP+14); tft.print(F("FORECAST"));
      }
      tft.fillRect(2,TOP+22,tft.width()-4,tft.height()-TOP-24,P.bg);
      float ph=state.pressBME/100.0f;
      const char* ws; uint16_t wc;
      if (ph<1000.0f) { ws="RAINY"; wc=P.accent; }
      else if (ph>1020.0f) { ws="CLEAR"; wc=P.yellow; }
      else { ws="FAIR"; wc=P.green; }
      tft.setFont(&FreeSansBold12pt7b); tft.setTextColor(wc,P.bg);
      int16_t bx,by; uint16_t bw,bh;
      tft.getTextBounds(ws,0,0,&bx,&by,&bw,&bh);
      tft.setCursor((tft.width()-(int)bw)/2,TOP+52); tft.print(ws);
      tft.drawBitmap((tft.width()-16)/2,TOP+56,(ph>1020.0f)?bmp_thermometer:bmp_cloud,16,16,wc);
      tft.setFont(); tft.setTextSize(1);
      tft.fillRoundRect(5,TOP+78,56,18,5,P.card); tft.setTextColor(P.orange,P.card);
      tft.setCursor(10,TOP+83); tft.printf("H: %.1fC",state.tempHigh==-100.0f?state.tempBME:state.tempHigh);
      tft.fillRoundRect(67,TOP+78,56,18,5,P.card); tft.setTextColor(P.accent,P.card);
      tft.setCursor(72,TOP+83); tft.printf("L: %.1fC",state.tempLow==100.0f?state.tempBME:state.tempLow);
      tft.fillRoundRect(8,TOP+102,tft.width()-16,16,8,P.raised);
      float pf=constrain((ph-900.0f)/200.0f,0.0f,1.0f);
      int bw2=(int)((tft.width()-20)*pf);
      if (bw2>0) tft.fillRoundRect(9,TOP+103,bw2,14,7,P.dblue);
      tft.setTextColor(P.text,P.bg); tft.setCursor(12,TOP+122); tft.printf("%.1f hPa",ph);
      tft.setFont();
      break;
    }

    // ==============================================================
    case 6: { // Smart Clock
    // ==============================================================
      if (forceClear) {
        tft.drawRoundRect(2,2,tft.width()-4,tft.height()-4,10,P.border);
        tft.drawRoundRect(3,3,tft.width()-6,tft.height()-6,9,P.raised);
      }
      tft.fillRoundRect(4,16,tft.width()-8,tft.height()-20,7,P.bg);
      if (hasTime) {
        char ws[16]; snprintf(ws,16,"WiFi %d dB",WiFi.RSSI());
        int sw=(int)(strlen(ws)*6);
        tft.fillRoundRect((tft.width()-sw-10)/2,TOP+2,sw+10,13,6,P.card);
        tft.setFont(); tft.setTextColor(P.accent,P.card);
        tft.setCursor((tft.width()-sw)/2,TOP+5); tft.print(ws);
        char ts[8]; snprintf(ts,8,"%02d:%02d",ti.tm_hour,ti.tm_min);
        tft.setFont(&FreeSans18pt7b); tft.setTextColor(P.text,P.bg);
        int16_t bx,by; uint16_t bw,bh;
        tft.getTextBounds(ts,0,0,&bx,&by,&bw,&bh);
        tft.setCursor((tft.width()-(int)bw)/2,TOP+58); tft.print(ts);
        tft.setFont();
        tft.drawFastHLine(10,TOP+62,tft.width()-20,P.raised);
        int sf=(tft.width()-20)*ti.tm_sec/59;
        if (sf>0) tft.drawFastHLine(10,TOP+62,sf,P.accent);
        tft.fillCircle(10+sf,TOP+62,2,P.accent);
        char ds[20]; strftime(ds,20,"%A, %b %d",&ti);
        tft.setFont(&FreeSans9pt7b); tft.setTextColor(P.teal,P.bg);
        tft.getTextBounds(ds,0,0,&bx,&by,&bw,&bh);
        tft.setCursor((tft.width()-(int)bw)/2,TOP+80); tft.print(ds);
        tft.setFont();
        tft.fillRoundRect(10,TOP+88,50,24,10,P.card); tft.setTextColor(P.orange,P.card);
        tft.setCursor(16,TOP+96); tft.printf("%.1f\xF8" "C",state.tempBME);
        tft.fillRoundRect(68,TOP+88,50,24,10,P.card); tft.setTextColor(P.accent,P.card);
        tft.setCursor(74,TOP+96); tft.printf("%.0f%%",state.humDHT);
      }
      tft.setFont();
      break;
    }

    // ==============================================================
    case 7: { // System Terminal
    // ==============================================================
      if (forceClear) {
        tft.fillRoundRect(2,TOP,tft.width()-4,tft.height()-TOP-2,4,P.bg);
        tft.drawRoundRect(2,TOP,tft.width()-4,tft.height()-TOP-2,4,P.border);
        tft.fillRoundRect(2,TOP,tft.width()-4,16,4,P.card);
        tft.drawFastHLine(2,TOP+16,tft.width()-4,P.border);
        tft.fillCircle(13,TOP+8,4,P.red);
        tft.fillCircle(25,TOP+8,4,P.yellow);
        tft.fillCircle(37,TOP+8,4,P.green);
        tft.setFont(); tft.setTextColor(P.muted,P.card);
        tft.setCursor(50,TOP+4); tft.print(F("esp32-sys"));
      }
      tft.fillRect(4,TOP+18,tft.width()-8,tft.height()-TOP-22,P.bg);
      tft.setFont(); tft.setTextSize(1);
      int ty=TOP+22;
      tft.setTextColor(P.green,P.bg); tft.setCursor(4,ty); tft.print(F("root@esp32:~$")); ty+=12;
      tft.setTextColor(P.accent,P.bg);
      tft.setCursor(4,ty); tft.printf("ip:   %s",WiFi.localIP().toString().c_str()); ty+=10;
      tft.setCursor(4,ty); tft.printf("rssi: %d dBm",WiFi.RSSI()); ty+=10;
      uint32_t up=millis()/1000;
      tft.setCursor(4,ty); tft.printf("up:   %02dh%02dm%02ds",up/3600,(up%3600)/60,up%60); ty+=10;
      tft.setCursor(4,ty); tft.printf("heap: %.1f KB",ESP.getFreeHeap()/1024.0f); ty+=10;
      tft.setTextColor(P.blue,P.bg); tft.setCursor(4,ty); tft.printf("mac:  %s",WiFi.macAddress().c_str()); ty+=12;
      tft.setTextColor(P.yellow,P.bg); tft.setCursor(4,ty);
      tft.print((millis()/500)%2==0?F("> _"):F(">  "));
      break;
    }

    // ==============================================================
    case 8: { // Pomodoro Timer
    // ==============================================================
      if (forceClear) {
        tft.setFont(&FreeSans9pt7b); tft.setTextColor(P.accent,P.bg);
        tft.setCursor(22,TOP+16); tft.print(F("POMODORO"));
      }
      tft.fillRect(4,TOP+18,tft.width()-8,tft.height()-TOP-22,P.bg);
      int cx=tft.width()/2,cy=92;
      if (!pomodoroActive) {
        drawArcTrack(cx,cy,38,9,P.raised);
        tft.setFont(&FreeSans9pt7b); tft.setTextColor(P.accent,P.bg);
        int16_t bx,by; uint16_t bw,bh;
        tft.getTextBounds("READY",0,0,&bx,&by,&bw,&bh);
        tft.setCursor(cx-(int)(bw/2),cy+6); tft.print(F("READY"));
        tft.setFont(); tft.setTextColor(P.border,P.bg);
        tft.setCursor(14,cy+28); tft.print(F("Start via web UI"));
      } else {
        long rem=(long)pomodoroEndTime-(long)millis();
        if (rem<=0) {
          pomodoroActive=false;
          drawArc(cx,cy,38,9,100.0f,P.green);
          tft.setFont(&FreeSans9pt7b); tft.setTextColor(P.green,P.bg);
          int16_t bx,by; uint16_t bw,bh;
          tft.getTextBounds("DONE!",0,0,&bx,&by,&bw,&bh);
          tft.setCursor(cx-(int)(bw/2),cy+6); tft.print(F("DONE!"));
        } else {
          int mins=(int)(rem/60000),secs=(int)((rem%60000)/1000);
          float pct=(float)rem/(25.0f*60000.0f)*100.0f;
          drawArcTrack(cx,cy,38,9,P.raised);
          uint16_t ac=(pct>50.0f)?P.red:(pct>20.0f?P.orange:P.green);
          drawArc(cx,cy,38,9,pct,ac);
          tft.setFont(); tft.setTextSize(2); tft.setTextColor(P.text,P.bg);
          tft.setCursor(cx-22,cy-8); tft.printf("%02d:%02d",mins,secs);
          tft.setTextSize(1); tft.setTextColor(P.muted,P.bg);
          tft.setCursor(cx-26,cy+20); tft.print(F("remaining"));
        }
      }
      tft.setFont();
      break;
    }

    // ==============================================================
    case 9: { // Hardware Health Graph
    // ==============================================================
      if (forceClear) {
        tft.fillRoundRect(2,TOP,tft.width()-4,20,4,P.card);
        tft.setFont(&FreeSans9pt7b); tft.setTextColor(P.accent,P.card);
        tft.setCursor(8,TOP+14); tft.print(F("HW HEALTH"));
      }
      tft.fillRect(2,TOP+22,tft.width()-4,tft.height()-TOP-24,P.bg);
      tft.setFont(); tft.setTextSize(1);
      tft.fillRoundRect(4,TOP+25,60,16,4,P.card); tft.setTextColor(P.accent,P.card);
      tft.setCursor(8,TOP+29); tft.printf("%.0f KB",ESP.getFreeHeap()/1024.0f);
      tft.fillRoundRect(68,TOP+25,55,16,4,P.card); tft.setTextColor(P.teal,P.card);
      tft.setCursor(72,TOP+29); tft.printf("%d dBm",WiFi.RSSI());
      const int gY=TOP+46,gH=60,gW=tft.width()-16;
      tft.fillRoundRect(8,gY,gW,gH,3,P.card); tft.drawRoundRect(8,gY,gW,gH,3,P.border);
      if (histCount>1) {
        int mx=min((int)histCount,gW-2);
        for (int i=0;i<mx-1;i++) {
          int idx=(histHead-mx+i+HISTORY_SIZE)%HISTORY_SIZE;
          int nxt=(idx+1)%HISTORY_SIZE;
          float h1=(float)tftHistHeap[idx],h2=(float)tftHistHeap[nxt];
          int y1=gY+gH-2-(int)(((h1-100000.0f)/200000.0f)*(gH-4));
          int y2=gY+gH-2-(int)(((h2-100000.0f)/200000.0f)*(gH-4));
          y1=constrain(y1,gY+1,gY+gH-2); y2=constrain(y2,gY+1,gY+gH-2);
          tft.drawLine(9+i,y1,10+i,y2,P.accent);
        }
      } else {
        tft.setTextColor(P.border,P.card); tft.setCursor(14,gY+gH/2-4);
        tft.print(F("Collecting data..."));
      }
      tft.setTextColor(P.border,P.bg); tft.setCursor(10,gY+gH+3);
      tft.print(F("Heap over time (100 pts)"));
      break;
    }
  } // end switch
}

// =================================================================
// ---- Setup ----
// =================================================================
void setup() {
  Serial.begin(115200); delay(500);
  initDisplays(); initFS(); initSensors(); initWiFi();

  server.serveStatic("/",          LittleFS, "/index.html");
  server.serveStatic("/style.css", LittleFS, "/style.css");
  server.serveStatic("/app.js",    LittleFS, "/app.js");

  server.on("/api/data",     HTTP_GET,  handleDataJson);
  server.on("/api/history",  HTTP_GET,  handleHistoryCSV);
  server.on("/api/settings", HTTP_GET,  handleGetSettings);
  server.on("/api/settings", HTTP_POST, handlePostSettings);
  server.on("/api/notes",    HTTP_GET,  handleGetNotes);
  server.on("/api/notes",    HTTP_POST, handlePostNotes);
  server.on("/api/pomodoro", HTTP_POST, handlePostPomodoro);
  server.on("/api/status",   HTTP_GET,  handleGetStatus);   // NEW
  server.on("/api/page",     HTTP_POST, handlePostPage);    // NEW

  server.begin(); Serial.println(F("[HTTP] Server ready"));
  refreshSensors(); logData(); lastLogTime=millis();
}

// =================================================================
// ---- Loop ----
// =================================================================
void loop() {
  server.handleClient();

  static unsigned long lastWifiCheck=0;
  if (millis()-lastWifiCheck>30000UL) {
    lastWifiCheck=millis();
    if (WiFi.status()!=WL_CONNECTED) {
      Serial.println(F("[WiFi] Reconnecting..."));
      WiFi.disconnect(); WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
    }
  }

  bool pageChanged=false;
  if (tftCarouselEnabled) {
    if (millis()-lastPageChange>(unsigned long)tftCarouselSpeed) {
      lastPageChange=millis();
      int en=0; for (int i=0;i<10;i++) if(pageEnabled[i]) en++;
      if (en>0) {
        int orig=tftPage;
        do { tftPage=(tftPage+1)%10; } while (!pageEnabled[tftPage]&&tftPage!=orig);
        if (tftPage!=orig) pageChanged=true;
      }
    }
  } else {
    if (!pageEnabled[tftPage]) {
      for (int i=0;i<10;i++) { if(pageEnabled[i]) { tftPage=i; pageChanged=true; break; } }
    }
  }

  static unsigned long lastRefresh=0;
  if (millis()-lastRefresh>2000UL||pageChanged) {
    lastRefresh=millis();
    refreshSensors(); renderOLED(); renderTFT(pageChanged);
  }
  if (millis()-lastLogTime>LOG_INTERVAL) { lastLogTime=millis(); logData(); }
}
