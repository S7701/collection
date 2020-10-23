#include <ArduinoOTA.h>   // https://github.com/jandrassy/ArduinoOTA
#include <NeoPixelBus.h>  // https://github.com/Makuna/NeoPixelBus
#include <NTPClient.h>    // https://github.com/arduino-libraries/NTPClient
#include <TimeLib.h>      // https://github.com/PaulStoffregen/Time
#include <Timezone.h>     // https://github.com/JChristensen/Timezone
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager

bool debug = true;

#define NTP_ADDRESS  F("de.pool.ntp.org")  // see ntp.org for ntp pools

WiFiUDP wifiUDP;
NTPClient ntpClient(wifiUDP, NTP_ADDRESS);

ESP8266WebServer server(80);  // instantiate server at port 80 (http port)

#define PIXEL_COUNT 110
#define STRIP_PIN   2  // for esp8266, the pin is omitted and it uses GPIO2 due to UART1 hardware use

NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart1800KbpsMethod> strip(PIXEL_COUNT, STRIP_PIN);

// RgbColor red(255, 0, 0);
// RgbColor green(0, 255, 0);
// RgbColor blue(0, 0, 255);

RgbColor fgColor(255, 0, 0);
RgbColor bgColor(0, 0, 0);
RgbColor white(255);
RgbColor black(0);

class Word {
  int from;
  int to;
  const char* txt;

public:
  Word(): Word(-1, F("")) {}
  Word(int _from, const char* _txt):  from(_from), to(_from + strlen(_txt) - 1), txt(_txt) {}

  void show() {
    if (from >= 0) {
      strip.ClearTo(fgColor, from, to);
      Serial.print(txt);
      Serial.print(F(" "));
    }
  }
};

class Phrase {
  int n;
  Word words[3];

public:
  Phrase(Word& word1): n(1) { words[0] = word1; }
  Phrase(Word& word1, Word& word2): n(2) { words[0] = word1; words[1] = word2; }
  Phrase(Word& word1, Word& word2, Word& word3): n(3) { words[0] = word1; words[1] = word2; words[2] = word3; }

  void show() {
    for (int i = 0; i < n; ++i) {
      words[i].show();
    }
  }
};

Word es(108, F("es")), ist(104, F("ist")), fuenfm(99, F("funf")); // not using ü as it counts as 2 letters
Word zehnm(88, F("zehn")), zwanzigm(92, F("zwanzig"));
Word dreiviertel(77, F("dreiviertel")), viertel(77, F("viertel"));
Word nach(68, F("nach")), vor(72, F("vor"));
Word halb(62, F("halb")), zwoelf(56, F("zwolf")); // not using ö as it counts as 2 letters
Word zwei(44, F("zwei")), ein(46, F("ein")), eins(46, F("eins")), sieben(49, F("sieben"));
Word drei(39, F("drei")), fuenf(33, F("funf")); // not using ü as it counts as 2 letters
Word elf(22, F("elf")), neun(25, F("neun")), vier(29, F("vier"));
Word acht(17, F("acht")), zehn(13, F("zehn"));
Word sechs(1, F("sechs")), uhr(8, F("uhr"));

Word hours[] = { zwoelf, eins, zwei, drei, vier, fuenf, sechs, sieben, acht, neun, zehn, elf, zwoelf, eins };

Phrase phrases[] = {
  Phrase(uhr),
  Phrase(fuenfm, nach),
  Phrase(zehnm, nach),
  Phrase(viertel),
  Phrase(zehnm, vor, halb),
  Phrase(fuenfm, vor, halb),
  Phrase(halb),
  Phrase(fuenfm, nach, halb),
  Phrase(zehnm, nach, halb),
  Phrase(dreiviertel),
  Phrase(zehnm, vor),
  Phrase(fuenfm, vor)
};

void setup() {
  Serial.begin(115200);
  Serial.println(F("Booting"));

#if STRIP_PIN != LED_BUILTIN
  // turn the builtin LED off by setting the output HIGH
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
#endif

  strip.Begin();
  for (int i = 0; i < PIXEL_COUNT; ++i) {
    strip.ClearTo(black);
    strip.SetPixelColor(i, white);
    strip.Show();
    delay(10);
  }
  strip.ClearTo(bgColor);
  strip.Show();

#if STRIP_PIN == LED_BUILTIN
  delay(1);
  // turn the builtin LED off by setting the output HIGH
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
#endif

  WiFiManager wifiManager;
  wifiManager.autoConnect(F("Wordclock"));

  ntpClient.begin();
  setSyncProvider(getNtpTime);
  setSyncInterval(3600);  // 1 hour

  ArduinoOTA.onStart([]() {
    Serial.println(F("Start"));
  });
  ArduinoOTA.onEnd([]() {
    Serial.println(F("\nEnd"));
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf(F("Progress: %u%%\r"), (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf(F("Error[%u]: "), error);
    if (error == OTA_AUTH_ERROR) Serial.println(F("Auth Failed"));
    else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin Failed"));
    else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
    else if (error == OTA_END_ERROR) Serial.println(F("End Failed"));
  });
  ArduinoOTA.begin();

  Serial.println(F("Ready"));
}

void loop() {
  static int state = 0;
  time_t local = now();

  ArduinoOTA.handle();

  switch (state) {
  case 0: // Show time on wordclock
    showTime(local);
    state = 1;
    break;
  case 1: // Wait until the first second of the current minute is over
    if (second(local) != 0) state = 2;
    break;
  case 2: // Wait for the first second in the next minute
    if (second(local) == 0) state = 0;
    break;
  }

  delay(100); // ms
}

void showTime(time_t local) {
  int hour12 = hourFormat12(local);  // 1...12 (0?)
  int minute5 = minute(local) / 5;  // 0...11

  if (minute5 >= 3) ++hour12;  // 1...13 (0?)

  Serial.printf(F("Show time: %d:%02d:%02d"), hour(local), minute(local), second(local));
  if (debug) {
    Serial.printf(F(" (hour12=%d, minute5=%d)"), hour12, minute5);
  }
  Serial.println();

#if STRIP_PIN == LED_BUILTIN
  strip.Begin();  // reinitialize LED strip
#endif

  strip.ClearTo(bgColor);
  es.show();
  ist.show();
  phrases[minute5].show();
  if ((hour12 == 1) && (minute5 == 0)) {
    ein.show();
  } else {
    hours[hour12].show();
  }
  strip.Show();

#if STRIP_PIN == LED_BUILTIN
  delay(1);
  // turn the builtin LED off by setting the output HIGH
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
#endif

  Serial.println();
}

time_t getNtpTime() {
  ntpClient.update();
  time_t utc = ntpClient.getEpochTime();

  // Central European Time (Frankfurt, Paris)
  TimeChangeRule CEST = { F("CEST"), Last, Sun, Mar, 2, 120 };  // Central European Summer Time
  TimeChangeRule CET = { F("CET "), Last, Sun, Oct, 3, 60 };    // Central European Standard Time
  Timezone tz(CEST, CET);
  time_t local = tz.toLocal(utc);

  Serial.printf(F("NTP time: %d:%02d:%02d"), hour(local), minute(local), second(local));
  Serial.println();

  showTime(local);

  return local;
}
