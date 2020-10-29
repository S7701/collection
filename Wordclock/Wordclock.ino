#include <String.h>
#include <ArduinoOTA.h>   // https://github.com/jandrassy/ArduinoOTA
#include <NeoPixelBus.h>  // https://github.com/Makuna/NeoPixelBus
#include <NTPClient.h>    // https://github.com/arduino-libraries/NTPClient
#include <TimeLib.h>      // https://github.com/PaulStoffregen/Time
#include <Timezone.h>     // https://github.com/JChristensen/Timezone
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager

bool display = true;
bool demo = false;

#define NTP_ADDRESS   "de.pool.ntp.org"  // see ntp.org for ntp pools
#define NTP_INTERVALL 3600  // seconds

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
  // turn the builtin LED off by setting the output HIGH
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
#endif

  strip.Begin();
  for (int i = 0; i < PIXEL_COUNT; ++i) {
    strip.ClearTo(black);
    strip.SetPixelColor(i, white);
    strip.Show();
  }
  strip.ClearTo(bgColor);
  strip.Show();

#if STRIP_PIN == LED_BUILTIN
  // let the strip update
  delay(1);
  // turn the builtin LED off by setting the output HIGH
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED pin as an output
  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by setting the output HIGH
#endif

  WiFiManager wifiManager;
  wifiManager.autoConnect("Wordclock");

  ntpClient.begin();
  setSyncProvider(getNtpTime);
  setSyncInterval(NTP_INTERVALL);  // 1 hour

  server.on("/", HTTP_GET, handleHttpGet);
  server.on("/", HTTP_POST, handleHttpPost);
  server.onNotFound([](){
    server.send(404, "text/plain", "404: Not found");
  });
  server.begin();

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

  if (demo) {
    static time_t now = 0;
    local = now;
    now += 10;
  }

  server.handleClient();
  ArduinoOTA.handle();

  if (display) {
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
  }

  delay(100); // ms
}

void showTime(time_t local) {
  int hour12 = hourFormat12(local);  // 1...12 (0?)
  int minute5 = minute(local) / 5;  // 0...11

  if (minute5 >= 3) ++hour12;  // 1...13 (0?)

  Serial.printf("Show time: %d:%02d:%02d, display %s\n", hour(local), minute(local), second(local), display ? "on" : "off");

#if STRIP_PIN == LED_BUILTIN
  // reinitialize LED strip
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
  // let the strip update
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
  TimeChangeRule CEST = { "CEST", Last, Sun, Mar, 2, 120 };  // Central European Summer Time
  TimeChangeRule CET = { "CET ", Last, Sun, Oct, 3, 60 };    // Central European Standard Time
  Timezone tz(CEST, CET);
  time_t local = tz.toLocal(utc);

  Serial.printf("NTP time: %d:%02d:%02d\n", hour(local), minute(local), second(local));

  return local;
}

void handleHttpGet() {
  String html =
    "<!DOCTYPE html>" \
    "<html>" \
    "<head>" \
    "<title>Wortuhr</title>" \
    "</head>" \
    "<body>" \
    "<form method=\"POST\" style=\"text-align:center;line-height:1.5\">" \

    "<strong>Wortuhr Einstellungen</strong>" \

    "<p>" \
    "<label>Anzeige</label><br>" \
    "<input type=\"radio\" name=\"display\" value=\"on\"";
  html += (display ? " checked" : "");
  html +=
    ">" \
    "<label>ein</label>" \
    "<input type=\"radio\" name=\"display\" value=\"off\"";
  html += (display ? "" : " checked");
  html +=
    ">" \
    "<label>aus</label>" \
    "</p>" \

    "<p>" \
    "<label>Demo</label><br>" \
    "<input type=\"radio\" name=\"demo\" value=\"on\"";
  html += (demo ? " checked" : "");
  html +=
    ">" \
    "<label>ein</label>" \
    "<input type=\"radio\" name=\"demo\" value=\"off\"";
  html += (demo ? "" : " checked");
  html +=
    ">" \
    "<label>aus</label>" \
    "</p>" \

    "<p>" \
    "<label>Vordergrundfarbe</label><br>" \
    "<input type=\"color\" name=\"foreground\" value=\"#";
  html += toString(fgColor);
  html +=
    "\">" \
    "</p>" \

    "<p>" \
    "<label>Hintergrundfarbe</label><br>" \
    "<input type=\"color\" name=\"background\" value=\"#";
  html += toString(bgColor);
  html +=
    "\">" \
    "</p>" \

    "<input type=\"submit\" value=\"Senden\">" \
    "</form>" \
    "</body>" \
    "</html>";
  server.send(200, "text/html", html);
}

void handleHttpPost() {
  if (server.hasArg("display")) {
    if (server.arg("display") == "on") display = true;
    if (server.arg("display") == "off") display = false;
  }
  if (server.hasArg("demo")) {
    if (server.arg("demo") == "on") demo = true;
    if (server.arg("demo") == "off") demo = false;
  }
  if (server.hasArg("foreground")) {
    fgColor = toRgbColor(server.arg("foreground"));
  }
  if (server.hasArg("background")) {
    bgColor = toRgbColor(server.arg("background"));
  }

  if (!display) {
#if STRIP_PIN == LED_BUILTIN
    // reinitialize LED strip
    strip.Begin();
#endif

    strip.ClearTo(black);
    strip.Show();

#if STRIP_PIN == LED_BUILTIN
    // let the strip update
    delay(1);
    // turn the builtin LED off by setting the output HIGH
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
#endif
  }

  if (display && !demo) showTime(now());

  handleHttpGet();
}

RgbColor toRgbColor(const String& s) {
  char buffer[16];
  s.toCharArray(buffer, 16);
  long l = strtol(&buffer[1], NULL, 16);
  return RgbColor((l >> 16) & 0xFF, (l >> 8) & 0xFF, (l >> 0) & 0xFF);
}

String toString(const RgbColor& rgb) {
  long l = (((long) rgb.R) << 16) + (((long) rgb.G) << 8) + (((long) rgb.B) << 0);
  return String(l, 16);
}
