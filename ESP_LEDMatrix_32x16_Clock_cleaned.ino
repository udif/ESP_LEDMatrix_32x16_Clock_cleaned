/*
 * (c)20016-18 Pawel A. Hernik
 * 
  ESP-01 pinout from top:

  GND    GP2 GP0 RX/GP3
  TX/GP1 CH  RST VCC

  MAX7219 5 wires to ESP-1 from pin side:
  Re Br Or Ye
  Gr -- -- --
  capacitor between VCC and GND
  resistor 1k between CH and VCC

  USB to Serial programming
  ESP-1 from pin side:
  FF(GP0) to GND, RR(CH_PD) to GND before upload
  Gr FF -- Bl
  Wh -- RR Vi

  ------------------------
  NodeMCU 1.0/D1 mini pinout:

  D8 - DataIn
  D7 - LOAD/CS
  D6 - CLK

*/

#include <ESP8266WiFi.h>

#define NUM_MAX 8
#define LINE_WIDTH 32
#define ROTATE  90

// for ESP-01 module
//#define DIN_PIN 2 // D4
//#define CS_PIN  3 // D9/RX
//#define CLK_PIN 0 // D3

// for NodeMCU 1.0/D1 mini
#define DIN_PIN 15  // D8
#define CS_PIN  13  // D7
#define CLK_PIN 12  // D6

#define DBG
#ifdef DBG
#define DEBUG(x) x
#else
#define DEBUG(x)
#endif

#include "max7219.h"
#include "fonts.h"
#include <time.h>

// =======================================================================
// Your config below!
// =======================================================================
const char* ssid     = "x";     // SSID of local network
const char* password = "x";   // Password on network
long utcOffset = 1;                    // UTC for Warsaw,Poland
// =======================================================================

time_t localEpoch = 0;
time_t dst_from_Epoch = 0;
time_t dst_until_Epoch = 0;
int dst_offset = 0;
long localMillisAtUpdate = 0;
String date;
String buf="";
struct tm *_tm;
String line;
char dst = 0;

int xPos=0, yPos=0;
int clockOnly = 0;

void setup() 
{
  buf.reserve(500);
  Serial.begin(115200);
  initMAX7219();
  sendCmdAll(CMD_SHUTDOWN, 1);
  sendCmdAll(CMD_INTENSITY, 0);
  clr();
  xPos=0;
#ifndef DBG
  DEBUG(Serial.print("Connecting to WiFi ");)
  WiFi.begin(ssid, password);
  printString("CONNECT..", font3x7);
  refreshAll();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); DEBUG(Serial.print("."));
  }
  clr();
  xPos=0;
  DEBUG(Serial.println(""); Serial.print("MyIP: "); Serial.println(WiFi.localIP());)
  printString((WiFi.localIP().toString()).c_str(), font3x7);
#endif
  refreshAll();
  getTime();
}

// =======================================================================

unsigned int curTime,updTime=0;
int dots,mode;

void loop()
{
  curTime = millis();
  if(curTime-updTime>600000) {
    updTime = curTime;
    getTime();  // update time every 600s=10m
  }
  dots = (curTime % 1000)<500;     // blink 2 times/sec
  mode = (curTime % 60000)/20000;  // change mode every 20s
  updateTime();
  if(mode==0) drawTime0(_tm); else
  if(mode==1) drawTime1(_tm); else drawTime2(_tm);
  refreshAll();
  delay(100);
}

// =======================================================================

char* monthNames[] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
char txt[30];

void drawTime0(struct tm *t)
{
  clr();
  yPos = 0;
  xPos = (t->tm_hour>9) ? 0 : 2;
  sprintf(txt,"%d", t->tm_hour);
  printString(txt, digits7x16);
  if(dots) printCharX(':', digits5x16rn, xPos);
  xPos+=(t->tm_hour>=22 || t->tm_hour==20)?1:2;
  sprintf(txt,"%02d",t->tm_min);
  printString(txt, digits7x16);
}

void drawTime1(struct tm *t)
{
  clr();
  yPos = 0;
  xPos = (t->tm_hour>9) ? 0 : 3;
  sprintf(txt,"%d",t->tm_hour);
  printString(txt, digits5x16rn);
  if(dots) printCharX(':', digits5x16rn, xPos);
  xPos+=(t->tm_hour>=22 || t->tm_hour==20)?1:2;
  sprintf(txt,"%02d",t->tm_min);
  printString(txt, digits5x16rn);
  sprintf(txt,"%02d",t->tm_sec);
  printString(txt, font3x7);
}

void drawTime2(struct tm *t)
{
  clr();
  yPos = 0;
  xPos = (t->tm_hour>9) ? 0 : 2;
  sprintf(txt,"%d", t->tm_hour);
  printString(txt, digits5x8rn);
  if(dots) printCharX(':', digits5x8rn, xPos);
  xPos+=(t->tm_hour>=22 || t->tm_hour==20)?1:2;
  sprintf(txt,"%02d",t->tm_min);
  printString(txt, digits5x8rn);
  sprintf(txt,"%02d",t->tm_sec);
  printString(txt, digits3x5);
  yPos = 1;
  xPos = 1;
  sprintf(txt,"%d&%s&%d",t->tm_mday,monthNames[t->tm_mon-1],t->tm_year-1900);
  printString(txt, font3x7);
  for(int i=0;i<LINE_WIDTH;i++) scr[LINE_WIDTH+i]<<=1;
}

// =======================================================================

int charWidth(char c, const uint8_t *font)
{
  int fwd = pgm_read_byte(font);
  int fht = pgm_read_byte(font+1);
  int offs = pgm_read_byte(font+2);
  int last = pgm_read_byte(font+3);
  if(c<offs || c>last) return 0;
  c -= offs;
  int len = pgm_read_byte(font+4);
  return pgm_read_byte(font + 5 + c * len);
}

// =======================================================================

int stringWidth(const char *s, const uint8_t *font)
{
  int wd=0;
  while(*s) wd += 1+charWidth(*s++, font);
  return wd-1;
}

// =======================================================================

int stringWidth(String str, const uint8_t *font)
{
  return stringWidth(str.c_str(), font);
}

// =======================================================================

int printCharX(char ch, const uint8_t *font, int x)
{
  int fwd = pgm_read_byte(font);
  int fht = pgm_read_byte(font+1);
  int offs = pgm_read_byte(font+2);
  int last = pgm_read_byte(font+3);
  if(ch<offs || ch>last) return 0;
  ch -= offs;
  int fht8 = (fht+7)/8;
  font+=4+ch*(fht8*fwd+1);
  int j,i,w = pgm_read_byte(font);
  for(j = 0; j < fht8; j++) {
    for(i = 0; i < w; i++) scr[x+LINE_WIDTH*(j+yPos)+i] = pgm_read_byte(font+1+fht8*i+j);
    if(x+i<LINE_WIDTH) scr[x+LINE_WIDTH*(j+yPos)+i]=0;
  }
  return w;
}

// =======================================================================

void printChar(unsigned char c, const uint8_t *font)
{
  if(xPos>NUM_MAX*8) return;
  int w = printCharX(c, font, xPos);
  xPos+=w+1;
}

// =======================================================================

void printString(const char *s, const uint8_t *font)
{
  while(*s) printChar(*s++, font);
  //refreshAll();
}

void printString(String str, const uint8_t *font)
{
  printString(str.c_str(), font);
}

// =======================================================================
time_t get_str_time(int s, struct tm *t)
{
  t->tm_mday = line.substring(s+8, s+10).toInt();
  t->tm_mon = line.substring(s+5, s+7).toInt();
  t->tm_year = line.substring(s, s+4).toInt() - 1900;
  t->tm_hour = line.substring(s+11, s+13).toInt();
  t->tm_min = line.substring(s+14, s+16).toInt();
  t->tm_sec = line.substring(s+17, s+19).toInt();
  t->tm_yday = 0;
  t->tm_wday = 0;
  t->tm_isdst = 0;
    DEBUG(Serial.println(String(t->tm_hour) + ":" + String(t->tm_min) + ":" + String(t->tm_sec) + "   Date: " + t->tm_mday+"." + t->tm_mon + "." + t->tm_year);)
  return mktime(t);
}

void getTime()
{
  WiFiClient client;
  DEBUG(Serial.print("connecting to www.worldtimeapi.org ...");)
  updateTime();
#ifndef DBG
  if(!client.connect("www.worldtimeapi.org", 80)) {
    DEBUG(Serial.println("connection failed");)
    return;
  }
  client.print(String("GET /api/ip HTTP/1.1\r\n" \
               "Host: www.worldtimeapi.org\r\n" \
               "Connection: close\r\n\r\n"));

  int repeatCounter = 10;
  while (!client.available() && repeatCounter--) {
    delay(200); DEBUG(Serial.println("y."));
  }

    client.setNoDelay(false);
  int dateFound = 0;
  while(client.available() && !dateFound) {
    Serial.flush();
    line = client.readStringUntil(',');
#else
  int dateFound = 0;
  line = "{\"week_number\":28,\"utc_offset\":\"+03:00\",\"utc_datetime\":\"2019-07-08T20:01:44.996234+00:00\",\"unixtime\":1562616104,\"timezone\":\"Asia/Jerusalem\",\"raw_offset\":7200,\"dst_until\":\"2019-07-08T23:02:00+00:00\",\"dst_offset\":3600,\"dst_from\":\"2019-07-08T22:01:00+00:00\",\"dst\":false,\"day_of_year\":189,\"day_of_week\":1,\"datetime\":\"2019-07-08T22:00:44.996234+03:00\",\"client_ip\":\"84.229.8.19\",\"abbreviation\":\"IDT\"}";
  while(!dateFound) {
    Serial.flush();
    line = line.substring(line.indexOf(',')+1);
#endif
    DEBUG(Serial.println(line));
    // Date: Thu, 19 Nov 2015 20:25:40 GMT
    if(line.startsWith("\"dst_until\"")) {
      dst_until_Epoch = get_str_time(13, _tm);
      DEBUG(Serial.println(dst_until_Epoch);)
      continue;
    }
    if(line.startsWith("\"dst_from\"")) {
      dst_from_Epoch = get_str_time(12, _tm);
      continue;
    }
    if(line.startsWith("\"dst_offset\"")) {
      dst_offset = line.substring(13, 17).toInt();
      DEBUG(Serial.println(String(dst_offset)));
      continue;
    }
    if(line.startsWith("\"dst\"")) {
      dst = (line[6] == 't') ? 1 : 0;
      DEBUG(Serial.println(dst ? "true" : "false");)
      continue;
    }
    if(line.startsWith("\"datetime\"")) {
      localMillisAtUpdate = millis();
      dateFound = 1;
      //"datetime":"2019-07-08T17:49:35.738925+03:00"
      localEpoch = get_str_time(12, _tm);
    }
  }
  client.stop();
}

// =======================================================================

void updateTime()
{
  static time_t lastEpoch;
  
  time_t curEpoch = localEpoch + ((millis() - localMillisAtUpdate) / 1000) - dst;
  if (dst > 0 && lastEpoch < dst_until_Epoch && curEpoch >= dst_until_Epoch) {
    localEpoch -= dst_offset;
    curEpoch -= dst_offset;
    dst = 0;
  }
  if (dst == 0 && lastEpoch < dst_from_Epoch && curEpoch >= dst_from_Epoch) {
    localEpoch += dst_offset;
    curEpoch += dst_offset;
    dst = 1;
  }
  //DEBUG(Serial.println(lastEpoch);)
  //DEBUG(Serial.println(curEpoch);)
  //DEBUG(Serial.println(dst_from_Epoch);)
  //DEBUG(Serial.println(dst_until_Epoch);)
  //DEBUG(Serial.println(dst);)
  lastEpoch = curEpoch;

  _tm = localtime(&curEpoch);
  DEBUG(Serial.print(asctime(_tm));)
  DEBUG(Serial.print("\r");)
}

// =======================================================================
