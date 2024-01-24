#include <ArduinoOTA.h>   // https://github.com/jandrassy/ArduinoOTA
#include <NeoPixelBus.h>  // https://github.com/Makuna/NeoPixelBus
#include <NTPClient.h>    // https://github.com/arduino-libraries/NTPClient
#include <TimeLib.h>      // https://github.com/PaulStoffregen/Time
#include <Timezone.h>     // https://github.com/JChristensen/Timezone
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager

#define VERSION_STR "v2.1"

bool demo = false;

#define NTP_ADDRESS     "de.pool.ntp.org"  // see ntp.org for ntp pools
#define NTP_INTERVALL   3607  // seconds = ~1 hour (prime number)
#define MAX_NTP_RETRIES 24

time_t ntpTime = 0;
unsigned ntpErrors = 0;
unsigned ntpRetries = 0;

#define DIM_BEGIN_DEFAULT    22
#define DIM_END_DEFAULT       5
#define DIM_INTERVAL_DEFAULT 60
#define DIM_INTERVAL_MAX     (6 * 60)

bool dim = true;
int dimBegin    = DIM_BEGIN_DEFAULT;
int dimEnd      = DIM_END_DEFAULT;
int dimInterval = DIM_INTERVAL_DEFAULT; // minutes

#define BRIGHTNESS_MAX 100
#define BRIGHTNESS_MIN   0

unsigned brightness         = BRIGHTNESS_MAX;
unsigned brightnessStandard = BRIGHTNESS_MAX;
unsigned brightnessDimmed   = 10;

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
  setSyncInterval(NTP_INTERVALL);  // ~1 hour

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

  server.handleClient();
  ArduinoOTA.handle();

  if (demo) {
    static time_t t = 0;
    local = t;
    t += 60;
    state = 0;
  }

  switch (state) {
  case 0: // Wait for the first second of a minute
    if (second(local) == 0) {
      updateStrip(local);
      state = 1;
    }
    break;
  case 1: // Wait until the first second of the current minute is over
    if (second(local) != 0) state = 0;
    break;
  }

  delay(333); // ms
}

void updateStrip(time_t local) {
  int hour12 = hourFormat12(local);  // 1...12 (0?)
  int hour24 = hour(local);          // 1...24 (0?)
  int minute5 = minute(local) / 5;   // 0...11

  Serial.printf("Show time: %d:%02d:%02d\n", hour(local), minute(local), second(local));

  // adjust brightness if necessary
  if (brightness > BRIGHTNESS_MAX) brightness = BRIGHTNESS_MAX;
  if (dim) {
    if (dimBegin <= hour24 || hour24 < dimEnd) {
      if (brightness > brightnessDimmed) brightness -= (brightnessStandard - brightnessDimmed) / dimInterval;
      if (brightness < brightnessDimmed) brightness = brightnessDimmed;
    }
    else {
      if (brightness < brightnessStandard) brightness += (brightnessStandard - brightnessDimmed) / dimInterval;
      if (brightness > brightnessStandard) brightness = brightnessStandard;
    }
  }
  else if (brightness != brightnessStandard) brightness = brightnessStandard;

  // all phrases from "quarter" onwards refer to the next hour
  if (minute5 >= 3) ++hour12;  // 1...13 (0?)

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
  }
  else {
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
    return 0;
  }

  ntpRetries = 0;

  time_t utc = ntpClient.getEpochTime();

  // Central European Time (Frankfurt, Paris)
  TimeChangeRule CEST = { "CEST", Last, Sun, Mar, 2, 120 };  // Central European Summer Time
  TimeChangeRule CET = { "CET ", Last, Sun, Oct, 3, 60 };    // Central European Standard Time
  Timezone tz(CEST, CET);
  ntpTime = tz.toLocal(utc);

  Serial.printf("NTP time: %d:%02d:%02d\n", hour(ntpTime), minute(ntpTime), second(ntpTime));

  return ntpTime;
}

void handleHttpGet() {
  char curTimeStr[80];
  time_t local = now();
  sprintf(curTimeStr, "%d:%02d:%02d", hour(local), minute(local), second(local));
  String html =
    "<!DOCTYPE html>" \
    "<html>" \
    "<head>" \
    "<title>Wortuhr " VERSION_STR "</title>" \
    "</head>" \
    "<body>" \
    "<form method=\"POST\" style=\"text-align:center;line-height:1.5\">" \

    "<strong>Wortuhr Einstellungen</strong>" \

    "<p>" \
    "Aktuelle Uhrzeit " \
    "<input type=\"text\" style=\"text-align:center\" name=\"curTimeStr\" size=\"8\" value=\""+ String(curTimeStr) +"\" disabled>" \
    "</p>" \

    "<p>" \
    "Aktuelle Helligkeit " \
    "<input type=\"text\" name=\"brightnessStandard\" style=\"text-align:center\" size=\"3\" value=\""+ String(brightness) +"\" disabled>" \
    " % " \
    "<br>" \
    "Standard Helligkeit " \
    "<input type=\"text\" name=\"brightnessStandard\" style=\"text-align:center\" size=\"3\" value=\""+ String(brightnessStandard) +"\">" \
    " % " \
    "<br>" \
    "Abgeblendete Helligkeit " \
    "<input type=\"text\" name=\"brightnessDimmed\" style=\"text-align:center\" size=\"3\" value=\""+ String(brightnessDimmed) +"\">" \
    " % " \
    "</p>" \

    "<p>" \
    "Abgeblendet von " \
    "<input type=\"text\" name=\"dimBegin\" style=\"text-align:center\" size=\"2\" value=\""+ String(dimBegin) +"\">" \
    " Uhr bis " \
    "<input type=\"text\" name=\"dimEnd\" style=\"text-align:center\" size=\"2\" value=\""+ String(dimEnd) +"\">" \
    " Uhr " \
    "<input type=\"radio\" name=\"dim\" value=\"on\""+ String(dim ? " checked" : "") +">" \
    "ein " \
    "<input type=\"radio\" name=\"dim\" value=\"off\""+ String(dim ? "" : " checked") +">" \
    "aus " \
    "<br>" \
    "Auf-/abblenden in " \
    "<input type=\"text\" name=\"dimInterval\" style=\"text-align:center\" size=\"2\" value=\""+ String(dimInterval) +"\">" \
    "Minuten " \
    "</p>" \

    "<p>" \
    "Demo" \
    "<input type=\"radio\" name=\"demo\" value=\"on\""+ String(demo ? " checked" : "") +">" \
    "ein " \
    "<input type=\"radio\" name=\"demo\" value=\"off\""+ String(demo ? "" : " checked") +">" \
    "aus " \
    "</p>" \

/*
    "<p>" \
    "Vordergrundfarben<br>" \
    "<input type=\"color\" name=\"fgColor0\" value=\""+ toString(fgColors[0]) +"\">" \
    "<input type=\"color\" name=\"fgColor1\" value=\""+ toString(fgColors[1]) +"\">" \
    "<input type=\"color\" name=\"fgColor2\" value=\""+ toString(fgColors[2]) +"\">" \
    "<input type=\"color\" name=\"fgColor3\" value=\""+ toString(fgColors[3]) +"\">" \
    "<input type=\"color\" name=\"fgColor4\" value=\""+ toString(fgColors[4]) +"\">" \
    "<input type=\"color\" name=\"fgColor5\" value=\""+ toString(fgColors[5]) +"\">" \
    "</p>" \

    "<p>" \
    "Hintergrundfarbe<br>" \
    "<input type=\"color\" name=\"bgColor\" value=\""+ toString(bgColor) +"\">" \
    "</p>" \
*/

    "<p>" \
    "Neustart " \
    "<input type=\"checkbox\" name=\"restart\">" \
    "</p>" \

    "<p>" \
    "<a href=\"/info\">weitere Informationen</a>" \
    "</p>" \

    "<input type=\"submit\" value=\"Aktualisieren\">" \
    "</form>" \
    "</body>" \
    "</html>";
  server.send(200, "text/html", html);
}

void handleHttpGetInfo() {
  char curTimeStr[80];
  char ntpTimeStr[80];
  time_t local = now();
  sprintf(curTimeStr, "%d:%02d:%02d", hour(local), minute(local), second(local));
  sprintf(ntpTimeStr, "%02d.%02d.%d %d:%02d:%02d", day(ntpTime), month(ntpTime), year(ntpTime), hour(ntpTime), minute(ntpTime), second(ntpTime));
  String html =
    "<!DOCTYPE html>" \
    "<html>" \
    "<head>" \
    "<title>Wortuhr " VERSION_STR "</title>" \
    "</head>" \
    "<body>" \
    "<form method=\"POST\" style=\"text-align:center;line-height:1.5\">" \

    "<strong>Wortuhr Informationen</strong>" \

    "<p>" \
    "Aktuelle Uhrzeit " \
    "<input type=\"text\" name=\"curTimeStr\" style=\"text-align:center\" size=\"8\" value=\""+ String(curTimeStr) +"\" disabled>" \
    "</p>" \

    "<p>" \
    "Letzte NTP Aktualisierung " \
    "<input type=\"text\"name=\"ntpTimeStr\"  style=\"text-align:center\" size=\"19\" value=\""+ String(ntpTimeStr) +"\" disabled>" \
    "<br>" \
    "Gesamt erfolglose NTP Aktualisierungen (NTP Errors) " \
    "<input type=\"text\" name=\"ntpErrors\" style=\"text-align:center\" size=\"2\" value=\""+ String(ntpErrors) +"\" disabled>" \
    "<br>" \
    "Wiederholt erfolglose NTP Aktualisierungen (NTP Retries) " \
    "<input type=\"text\" name=\"ntpRetries\" style=\"text-align:center\" size=\"2\" value=\""+ String(ntpRetries) +"\" disabled>" \
    "</p>" \

    "</form>" \
    "</body>" \
    "</html>";
  server.send(200, "text/html", html);
}

void handleHttpPost() {
  if (server.hasArg("restart")) {
    if (server.arg("restart") == "on") ESP.restart();
  }
  if (server.hasArg("demo")) {
    if (server.arg("demo") == "on") demo = true;
    if (server.arg("demo") == "off") demo = false;
  }
/*
  if (server.hasArg("brightness")) {
    brightness = toUnsigned(server.arg("brightness"));
    if (brightness > BRIGHTNESS_MAX) brightness = BRIGHTNESS_MAX;
  }
*/
  if (server.hasArg("brightnessStandard")) {
    brightnessStandard = toUnsigned(server.arg("brightnessStandard"));
    if (brightnessStandard > BRIGHTNESS_MAX) brightnessStandard = BRIGHTNESS_MAX;
  }
  if (server.hasArg("brightnessDimmed")) {
    brightnessDimmed = toUnsigned(server.arg("brightnessDimmed"));
    if (brightnessDimmed > BRIGHTNESS_MAX) brightnessDimmed = BRIGHTNESS_MAX;
  }
  if (server.hasArg("dim")) {
    if (server.arg("dim") == "on") dim = true;
    if (server.arg("dim") == "off") dim = false;
  }
  if (server.hasArg("dimBegin")) {
    dimBegin = toInt(server.arg("dimBegin"));
    if (dimBegin < 0 || 24 < dimBegin) dimBegin = DIM_BEGIN_DEFAULT;
  }
  if (server.hasArg("dimEnd")) {
    dimEnd = toInt(server.arg("dimEnd"));
    if (dimEnd < 0 || 24 < dimEnd) dimEnd = DIM_END_DEFAULT;
  }
  if (server.hasArg("dimInterval")) {
    dimInterval = toInt(server.arg("dimInterval"));
    if (dimInterval <= 0 || DIM_INTERVAL_MAX < dimInterval) dimInterval = DIM_INTERVAL_DEFAULT;
  }
/*
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
*/

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

  if (!demo) updateStrip(now());

  handleHttpGet();
}

RgbColor toRgbColor(const String& s) {
  char buffer[16];
  s.toCharArray(buffer, 16);
  long l = strtol(&buffer[1], NULL, 16); // skip leading #
  return RgbColor((l >> 16) & 0xFF, (l >> 8) & 0xFF, (l >> 0) & 0xFF);
}

int toInt(const String& s) {
  char buffer[16];
  s.toCharArray(buffer, 16);
  long l = strtol(buffer, NULL, 10);
  return (int) l;
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
  if (brightness >= BRIGHTNESS_MAX) return;
  rgb.R = rgb.R * brightness / BRIGHTNESS_MAX;
  rgb.G = rgb.G * brightness / BRIGHTNESS_MAX;
  rgb.B = rgb.B * brightness / BRIGHTNESS_MAX;
}
