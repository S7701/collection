#include <String.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include <NTPClient.h>    // https://github.com/arduino-libraries/NTPClient
#include <TimeLib.h>      // https://github.com/PaulStoffregen/Time
#include <Timezone.h>     // https://github.com/JChristensen/Timezone
#include <NeoPixelBus.h>  // https://github.com/Makuna/NeoPixelBus
#include <ArduinoOTA.h>

bool debug = true;

#define NTP_OFFSET   0 // s
#define NTP_INTERVAL 60 * 1000 // ms
#define NTP_ADDRESS  "de.pool.ntp.org"  // see ntp.org for ntp pools

WiFiUDP wifiUDP;
NTPClient ntpClient(wifiUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

ESP8266WebServer server(80);  // instantiate server at port 80 (http port)

#define PIXEL_COUNT 110
#define PIXEL_PIN     2  // For Esp8266, the Pin is omitted and it uses GPIO2 due to UART1 hardware use.

NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart1800KbpsMethod> strip(PIXEL_COUNT, PIXEL_PIN);

//RgbColor red(255, 0, 0);
//RgbColor green(0, 255, 0);
//RgbColor blue(0, 0, 255);
RgbColor foreground(255, 0, 0);
RgbColor background(0, 0, 0);
RgbColor white(255);
RgbColor black(0);

class Word {
  int from;
  int to;
  String word;

public: 
  Word(int _from, String _word) {
    from = _from;
    to = _from + _word.length() - 1;
    word = _word;
  }

  void on() {
    for (int i = from; i <= to; ++i) {
      strip.SetPixelColor(i, foreground);
    }
    Serial.print(word);
    Serial.print(" ");
  }
};

Word sechs(1, "sechs");
Word uhr(8, "uhr");
Word zehn(13, "zehn");
Word acht(17, "acht");
Word elf(22, "elf");
Word neun(25, "neun");
Word vier(29, "vier");
Word fuenf(33, "funf"); // not using ü as it counts as 2 letters
Word drei(39, "drei");
Word zwei(44, "zwei");
Word eins(46, "eins"); Word ein(46, "ein");
Word sieben(49, "sieben");
Word zwoelf(56, "zwolf");
Word halb(62, "halb");
Word nach(68, "nach");
Word vor(72, "vor");
Word viertel(77, "viertel");
Word dreiviertel(77, "dreiviertel");
Word zehnm(88, "zehn");
Word zwanzigm(92, "zwanzig");
Word fuenfm(99, "funf");
Word ist(104, "ist");
Word es(108, "es");
Word none(0, "");

Word hourWords[] = { none, eins, zwei, drei, vier, fuenf, sechs, sieben, acht, neun, zehn, elf, zwoelf };

Word minuteWords[13][3] = {
  { uhr, none, none },          //  0
  { fuenfm, nach, none },       //  1
  { zehnm, nach, none },        //  2
  { viertel, none, none },      //  3
  { zehnm, vor, halb },         //  4
  { fuenfm, vor, halb },        //  5
  { halb, none, none },         //  6
  { fuenfm, nach, halb },       //  7
  { zehnm, nach, halb },        //  8
  { dreiviertel, none, none },  //  9
  { zehnm, vor, none },         // 10
  { fuenfm, vor, none },        // 11
  { none, none, none }          // 12
};

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

  strip.Begin();
  for (int i = 0; i < PIXEL_COUNT; ++i) {
    strip.ClearTo(black);
    strip.SetPixelColor(i, white);
    strip.Show();
    delay(10);
  }
  strip.ClearTo(background);
  strip.Show();

  connectWiFi();

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  ntpClient.begin();
  setSyncProvider(getNtpTime);
  setSyncInterval(3600);  // 1 hour

  Serial.println("Ready");
}

void loop() {
  static bool first = true;
  time_t local = now();

  ArduinoOTA.handle();

  if (first || second(local) == 0) {
    first = false;

    int curHour12 = hourFormat12(local);  // 1...12
    int curMinute5 = minute(local) / 5;  // 0...11
    int displayHour = (curMinute5 >= 3) ? (curHour12 + 1) : curHour12;  // 1...13
    if (displayHour == 13) displayHour = 1;
 
    Serial.print("Show time: ");
    Serial.print(hour(local));
    Serial.print(":");
    Serial.print(minute(local));
    Serial.print(":");
    Serial.print(second(local));
    if (debug) {
      Serial.print(" (curHour12=");
      Serial.print(curHour12);
      Serial.print(", curMinute5=");
      Serial.print(curMinute5);
      Serial.print(", displayHour=");
      Serial.print(displayHour);
      Serial.print(")");
    }
    Serial.println();
 
    strip.ClearTo(background);
    es.on();
    ist.on();
    for (int i = 0; i < 3; ++i) {
      minuteWords[curMinute5][i].on();
    }
    if ((displayHour == 1) && (curMinute5 == 0)) {
      ein.on();
    } else {
      hourWords[displayHour].on();
    }
    strip.Show();

    Serial.println(".");

    delay(1000); // 1 second
  } else {
    delay(100); // 1/10 second
  }
}

void connectWiFi() {
  WiFiManager wifiManager;
  wifiManager.autoConnect("Wordclock");
}

time_t getNtpTime() {
  ntpClient.update();
  unsigned long epochTime = ntpClient.getEpochTime();
  time_t utc = epochTime;

  // Central European Time (Frankfurt, Paris)
  TimeChangeRule CEST = { "CEST", Last, Sun, Mar, 2, 120 };  // Central European Summer Time
  TimeChangeRule CET = { "CET ", Last, Sun, Oct, 3, 60 };    // Central European Standard Time
  Timezone tz(CEST, CET);
  time_t local = tz.toLocal(utc);

  Serial.print("NTP time: ");
  Serial.print(hour(local));
  Serial.print(":");
  Serial.print(minute(local));
  Serial.print(":");
  Serial.println(second(local));

  return local;
}
