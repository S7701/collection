#include <ArduinoOTA.h>   // https://github.com/jandrassy/ArduinoOTA
#include <NeoPixelBus.h>  // https://github.com/Makuna/NeoPixelBus
#include <NTPClient.h>    // https://github.com/arduino-libraries/NTPClient
#include <TimeLib.h>      // https://github.com/PaulStoffregen/Time
#include <Timezone.h>     // https://github.com/JChristensen/Timezone
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager

bool debug = true;

#define NTP_ADDRESS  "de.pool.ntp.org"  // see ntp.org for ntp pools

WiFiUDP wifiUDP;
NTPClient ntpClient(wifiUDP, NTP_ADDRESS);

ESP8266WebServer server(80);  // instantiate server at port 80 (http port)

#define PIXEL_COUNT 110
#define STRIP_PIN   2  // For esp8266, the pin is omitted and it uses GPIO2 due to UART1 hardware use.

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
  Word(): Word(-1, "") {}
  Word(int _from, const char* _txt):  from(_from), to(_from + strlen(_txt) - 1), txt(_txt) {}

  void show() {
    if (from >= 0) {
      strip.ClearTo(fgColor, from, to);
      Serial.print(txt);
      Serial.print(" ");
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

Word es(108, "es"), ist(104, "ist"), fuenfm(99, "funf"); // not using ü as it counts as 2 letters
Word zehnm(88, "zehn"), zwanzigm(92, "zwanzig");
Word dreiviertel(77, "dreiviertel"), viertel(77, "viertel");
Word nach(68, "nach"), vor(72, "vor");
Word halb(62, "halb"), zwoelf(56, "zwolf"); // not using ö as it counts as 2 letters
Word zwei(44, "zwei"), ein(46, "ein"), eins(46, "eins"), sieben(49, "sieben");
Word drei(39, "drei"), fuenf(33, "funf"); // not using ü as it counts as 2 letters
Word elf(22, "elf"), neun(25, "neun"), vier(29, "vier");
Word acht(17, "acht"), zehn(13, "zehn");
Word sechs(1, "sechs"), uhr(8, "uhr");

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
  Serial.println("Booting");

#if STRIP_PIN != LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED pin as an output
  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by setting the output HIGH
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
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED pin as an output
  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by setting the output HIGH
#endif

  WiFiManager wifiManager;
  wifiManager.autoConnect("Wordclock");

  ntpClient.begin();
  setSyncProvider(getNtpTime);
  setSyncInterval(3600);  // 1 hour

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

  Serial.println("Ready");
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

  Serial.printf("Show time: %d:%02d:%02d", hour(local), minute(local), second(local));
  if (debug) {
    Serial.printf(" (hour12=%d, minute5=%d)", hour12, minute5);
  }
  Serial.println();

#if STRIP_PIN == LED_BUILTIN
  strip.Begin();
#endif

  strip.ClearTo(bgColor);
  es.show(); ist.show();
  phrases[minute5].show();
  if ((hour12 == 1) && (minute5 == 0)) {
    ein.show();
  } else {
    hours[hour12].show();
  }
  strip.Show();

#if STRIP_PIN == LED_BUILTIN
  delay(1);
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED pin as an output
  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by setting the output HIGH
#endif

  Serial.println();
}

time_t getNtpTime() {
  ntpClient.update();
  time_t utc = ntpClient.getEpochTime();

  // Central European Time (Frankfurt, Paris)
  TimeChangeRule CEST = { "CEST", Last, Sun, Mar, 2, 120 };  // Central European Summer Time
  TimeChangeRule CET = { "CET ", Last, Sun, Oct, 3, 60 };    // Central European Standard Time
  Timezone tz(CEST, CET);
  time_t local = tz.toLocal(utc);

  Serial.printf("NTP time: %d:%02d:%02d", hour(local), minute(local), second(local));
  Serial.println();

  showTime(local);

  return local;
}
