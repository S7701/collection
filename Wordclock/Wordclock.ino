#include <ArduinoOTA.h>   // https://github.com/jandrassy/ArduinoOTA
#include <NeoPixelBus.h>  // https://github.com/Makuna/NeoPixelBus
#include <NTPClient.h>    // https://github.com/arduino-libraries/NTPClient
#include <TimeLib.h>      // https://github.com/PaulStoffregen/Time
#include <Timezone.h>     // https://github.com/JChristensen/Timezone
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager

#define NTP_ADDRESS      "de.pool.ntp.org"  // see ntp.org for ntp pools
#define NTP_INTERVALL    3607  // seconds = ~1 hour (prime number)
#define MAX_NTP_RETRIES  24

#define MAX_BRIGHTNESS 100

bool display = true;
bool demo = false;
time_t ntpTime = 0;
time_t ntpErrorTime = 0;
unsigned ntpErrors = 0;
unsigned ntpRetries = 0;

WiFiUDP wifiUDP;
NTPClient ntpClient(wifiUDP, NTP_ADDRESS);

ESP8266WebServer server(80);  // instantiate server at port 80 (http port)

#define PIXEL_COUNT 110
#define STRIP_PIN   2  // for esp8266, the pin is omitted and it uses GPIO2 due to UART1 hardware use

NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart1800KbpsMethod> strip(PIXEL_COUNT, STRIP_PIN);

RgbColor black(0, 0, 0);
RgbColor white(255, 255, 255);

RgbColor fgColors[] = {
  RgbColor(255, 0, 0),   // red
  RgbColor(127, 127, 0), // yellow
  RgbColor(0, 255, 0),   // green
  RgbColor(0, 127, 127), // cyan
  RgbColor(0, 0, 255),   // blue
  RgbColor(127, 0, 127)  // magenta
};
const unsigned fgColorCount = sizeof fgColors / sizeof fgColors[0];

unsigned fgColorFirstWord = 0; // index in color table for first word
unsigned fgColorNextWord = 0;  // index in color table for next word

unsigned brightness = MAX_BRIGHTNESS;

RgbColor bgColor(0, 0, 0); // black

void setBrightness(RgbColor& rgb);

class Word {
  int from;
  int to;
  const char* txt;

public:
  Word(): Word(-1, "") {}
  Word(int _from, const char* _txt):  from(_from), to(_from + strlen(_txt) - 1), txt(_txt) {}

  void show() {
    if (from >= 0) {
      RgbColor color = fgColors[fgColorNextWord];
      setBrightness(color);
      strip.ClearTo(color, from, to);

      // switch to next color in color list
      if (++fgColorNextWord >= fgColorCount) fgColorNextWord = 0;

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
  server.on("/info", HTTP_GET, handleHttpGetInfo);
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
    case 0: // Show time on wordclock; update every minute
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

  // start with next color in color list
  if (++fgColorFirstWord >= fgColorCount) fgColorFirstWord = 0;
  fgColorNextWord = fgColorFirstWord;

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
  if (!ntpClient.update()) {
    ++ntpErrors;
    if (++ntpRetries == MAX_NTP_RETRIES) ESP.restart();
  } else
    ntpRetries = 0;

  time_t utc = ntpClient.getEpochTime();

  // Central European Time (Frankfurt, Paris)
  TimeChangeRule CEST = { "CEST", Last, Sun, Mar, 2, 120 };  // Central European Summer Time
  TimeChangeRule CET = { "CET ", Last, Sun, Oct, 3, 60 };    // Central European Standard Time
  Timezone tz(CEST, CET);
  ntpTime = tz.toLocal(utc);

  if (ntpRetries) ntpErrorTime = ntpTime;

  Serial.printf("NTP time: %d:%02d:%02d\n", hour(ntpTime), minute(ntpTime), second(ntpTime));

  return ntpTime;
}

void handleHttpGet() {
  char cur_time[80];
  char ntp_time[80];
  char ntp_error_time[80];
  time_t local = now();
  sprintf(cur_time, "%d:%02d:%02d", hour(local), minute(local), second(local));
  sprintf(ntp_time, "%02d.%02d.%d %d:%02d:%02d", day(ntpTime), month(ntpTime), year(ntpTime), hour(ntpTime), minute(ntpTime), second(ntpTime));
  sprintf(ntp_error_time, "%02d.%02d.%d %d:%02d:%02d", day(ntpErrorTime), month(ntpErrorTime), year(ntpErrorTime), hour(ntpErrorTime), minute(ntpErrorTime), second(ntpErrorTime));
  String html =
    "<!DOCTYPE html>" \
    "<html>" \
    "<head>" \
    "<title>Wortuhr v1.0</title>" \
    "</head>" \
    "<body>" \
    "<form method=\"POST\" style=\"text-align:center;line-height:1.5\">" \

    "<strong>Wortuhr Einstellungen</strong>" \

    "<p>" \
    "<label>aktuelle Uhrzeit</label><br>" \
    "<input type=\"text\" style=\"text-align:center\" name=\"cur_time\" size=\"10\" disabled value=\""+ String(cur_time) +"\">" \
    "</p>" \

    "<p>" \
    "<label>Anzeige</label><br>" \
    "<input type=\"radio\" name=\"display\" value=\"on\""+ String(display ? " checked" : "") +">" \
    "<label>ein</label>" \
    "<input type=\"radio\" name=\"display\" value=\"off\""+ String(display ? "" : " checked") +">" \
    "<label>aus</label>" \
    "</p>" \

    "<p>" \
    "<label>Helligkeit</label><br>" \
    "<input type=\"range\" name=\"brightness\" min=\"0\" max=\""+ String(MAX_BRIGHTNESS) +"\" step=\"10\" value=\""+ String(brightness) +"\">"
    "</p>" \

    "<p>" \
    "<label>Demo</label><br>" \
    "<input type=\"radio\" name=\"demo\" value=\"on\""+ String(demo ? " checked" : "") +">" \
    "<label>ein</label>" \
    "<input type=\"radio\" name=\"demo\" value=\"off\""+ String(demo ? "" : " checked") +">" \
    "<label>aus</label>" \
    "</p>" \

/*
    "<p>" \
    "<label>Vordergrundfarben</label><br>" \
    "<input type=\"color\" name=\"fgColor0\" value=\""+ toString(fgColors[0]) +"\">" \
    "<input type=\"color\" name=\"fgColor1\" value=\""+ toString(fgColors[1]) +"\">" \
    "<input type=\"color\" name=\"fgColor2\" value=\""+ toString(fgColors[2]) +"\">" \
    "<input type=\"color\" name=\"fgColor3\" value=\""+ toString(fgColors[3]) +"\">" \
    "<input type=\"color\" name=\"fgColor4\" value=\""+ toString(fgColors[4]) +"\">" \
    "<input type=\"color\" name=\"fgColor5\" value=\""+ toString(fgColors[5]) +"\">" \
    "</p>" \

    "<p>" \
    "<label>Hintergrundfarbe</label><br>" \
    "<input type=\"color\" name=\"bgColor\" value=\""+ toString(bgColor) +"\">" \
    "</p>" \
*/

    "<p>" \
    "<a href=\"/info\">Wortuhr Informationen</a>" \
    "</p>" \

    "<input type=\"submit\" value=\"Aktualisieren\">" \
    "</form>" \
    "</body>" \
    "</html>";
  server.send(200, "text/html", html);
}

void handleHttpGetInfo() {
  char cur_time[80];
  char ntp_time[80];
  char ntp_error_time[80];
  time_t local = now();
  sprintf(cur_time, "%d:%02d:%02d", hour(local), minute(local), second(local));
  sprintf(ntp_time, "%02d.%02d.%d %d:%02d:%02d", day(ntpTime), month(ntpTime), year(ntpTime), hour(ntpTime), minute(ntpTime), second(ntpTime));
  sprintf(ntp_error_time, "%02d.%02d.%d %d:%02d:%02d", day(ntpErrorTime), month(ntpErrorTime), year(ntpErrorTime), hour(ntpErrorTime), minute(ntpErrorTime), second(ntpErrorTime));
  String html =
    "<!DOCTYPE html>" \
    "<html>" \
    "<head>" \
    "<title>Wortuhr v1.0</title>" \
    "</head>" \
    "<body>" \
    "<form method=\"POST\" style=\"text-align:center;line-height:1.5\">" \

    "<strong>Wortuhr Informationen</strong>" \

    "<p>" \
    "<label>aktuelle Uhrzeit</label><br>" \
    "<input type=\"text\" style=\"text-align:center\" name=\"cur_time\" size=\"10\" disabled value=\""+ String(cur_time) +"\">" \
    "</p>" \

    "<p>" \
    "<label>letzte NTP Aktualisierung</label><br>" \
    "<input type=\"text\" style=\"text-align:center\" name=\"ntp_time\" size=\"20\" disabled value=\""+ String(ntp_time) +"\">" \
    "</p>" \

    "<p>" \
    "<label>letzte erfolglose NTP Aktualisierung</label><br>" \
    "<input type=\"text\" style=\"text-align:center\" name=\"ntp_error_time\" size=\"20\" disabled value=\""+ String(ntp_error_time) +"\">" \
    "</p>" \

    "<p>" \
    "<label>erfolglose NTP Aktualisierungen</label><br>" \
    "<input type=\"text\" style=\"text-align:center\" name=\"ntpErrors\" size=\"5\" disabled value=\""+ String(ntpErrors) +"\">" \
    "</p>" \

    "<p>" \
    "<label>wiederholt erfolglose NTP Aktualisierungen</label><br>" \
    "<input type=\"text\" style=\"text-align:center\" name=\"ntpRetries\" size=\"5\" disabled value=\""+ String(ntpRetries) +"\">" \
    "</p>" \

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
  if (server.hasArg("fgColor0")) {
    fgColors[0] = toRgbColor(server.arg("fgColor0"));
  }
  if (server.hasArg("fgColor1")) {
    fgColors[1] = toRgbColor(server.arg("fgColor1"));
  }
  if (server.hasArg("fgColor2")) {
    fgColors[2] = toRgbColor(server.arg("fgColor2"));
  }
  if (server.hasArg("fgColor3")) {
    fgColors[3] = toRgbColor(server.arg("fgColor3"));
  }
  if (server.hasArg("fgColor4")) {
    fgColors[4] = toRgbColor(server.arg("fgColor4"));
  }
  if (server.hasArg("fgColor5")) {
    fgColors[5] = toRgbColor(server.arg("fgColor5"));
  }
  if (server.hasArg("bgColor")) {
    bgColor = toRgbColor(server.arg("bgColor"));
  }
  if (server.hasArg("brightness")) {
    brightness = toUnsigned(server.arg("brightness"));
    if (brightness > MAX_BRIGHTNESS) brightness = MAX_BRIGHTNESS;
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
  long l = strtol(&buffer[1], NULL, 16); // skip leading #
  return RgbColor((l >> 16) & 0xFF, (l >> 8) & 0xFF, (l >> 0) & 0xFF);
}

unsigned toUnsigned(const String& s) {
  char buffer[16];
  s.toCharArray(buffer, 16);
  long l = strtol(buffer, NULL, 10);
  return (unsigned) l;
}

String toString(const RgbColor& rgb) {
  char str[8];
  sprintf(str, "#%02X%02X%02X", rgb.R, rgb.G, rgb.B);
  return String(str);
}

void setBrightness(RgbColor& rgb) {
  if (brightness >= MAX_BRIGHTNESS) return;
  rgb.R = rgb.R * brightness / MAX_BRIGHTNESS;
  rgb.G = rgb.G * brightness / MAX_BRIGHTNESS;
  rgb.B = rgb.B* brightness / MAX_BRIGHTNESS;
}
