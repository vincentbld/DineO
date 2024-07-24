#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

// Configuration NeoPixel
#define PIN            D6 // Utilisez la broche D6 pour les NeoPixels sur le Weemos D1 R1
#define NUMPIXELS      60  // Changez selon le nombre de LEDs sur votre anneau

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Configuration WiFi en mode AP
const char *ssid = "MealTimerAP";
const char *password = "password123";

// DNS server pour captive portal
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Définir les durées (en secondes) pour chaque segment
unsigned long entreeTime = 60;  // 1 minute
unsigned long platTime = 300;   // 5 minutes
unsigned long fromageTime = 120; // 2 minutes
unsigned long dessertTime = 120;  // 2 minutes

AsyncWebServer server(80);

// Page web HTML
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Meal Timer Configuration</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; text-align: center; margin: 0; padding: 0; background-color: #FFE4B5; } /* Fond en orange mandarine pâle */
    .container { max-width: 600px; margin: auto; padding: 20px; background-color: #fff; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); }
    h1 { color: #000080; } /* Texte de titre en bleu marine */
    form { display: flex; flex-direction: column; }
    input[type="text"] { padding: 10px; margin: 10px 0; font-size: 16px; }
    input[type="submit"] { padding: 10px; background-color: #4CAF50; color: #fff; border: none; cursor: pointer; font-size: 16px; }
    input[type="submit"]:hover { background-color: #45a049; }
  </style>
  </head><body>
  <div class="container">
    <h1>RepasCool par Ideaslabs</h1>
    <form action="/setTimes" method="GET">
      Entree Time (seconds): <input type="text" name="entree" value="60"><br>
      Plat Time (seconds): <input type="text" name="plat" value="300"><br>
      Fromage Time (seconds): <input type="text" name="fromage" value="120"><br>
      Dessert Time (seconds): <input type="text" name="dessert" value="120"><br>
      <input type="submit" value="Set Times">
    </form>
  </div>
</body></html>
)rawliteral";

enum MealPhase {ENTREE, PLAT, FROMAGE, DESSERT, NONE};

MealPhase currentPhase = NONE;
unsigned long phaseStartTime = 0;

void startPhase(MealPhase phase) {
  currentPhase = phase;
  phaseStartTime = millis();

  Serial.print("Starting phase: ");
  Serial.println(phase);

  strip.clear();
  strip.show();
}

void updatePhase() {
  unsigned long elapsedTime = millis() - phaseStartTime;

  int totalPixels = NUMPIXELS / 4;
  int litPixels = 0;
  uint32_t color = 0;

  switch(currentPhase) {
    case ENTREE:
      litPixels = map(elapsedTime, 0, entreeTime * 1000, 0, totalPixels);
      color = strip.Color(255, 0, 0); // Rouge
      if (elapsedTime > entreeTime * 1000) {
        startPhase(PLAT);
      }
      break;
    case PLAT:
      litPixels = map(elapsedTime, 0, platTime * 1000, 0, totalPixels);
      color = strip.Color(0, 255, 0); // Vert
      if (elapsedTime > platTime * 1000) {
        startPhase(FROMAGE);
      }
      break;
    case FROMAGE:
      litPixels = map(elapsedTime, 0, fromageTime * 1000, 0, totalPixels);
      color = strip.Color(0, 0, 255); // Bleu
      if (elapsedTime > fromageTime * 1000) {
        startPhase(DESSERT);
      }
      break;
    case DESSERT:
      litPixels = map(elapsedTime, 0, dessertTime * 1000, 0, totalPixels);
      color = strip.Color(255, 255, 0); // Jaune
      if (elapsedTime > dessertTime * 1000) {
        startPhase(NONE);
      }
      break;
    case NONE:
      strip.clear();
      strip.show();
      return;
  }

  int startPixel = currentPhase * totalPixels;
  setColorSegment(startPixel, startPixel + litPixels, color);
}

void setColorSegment(int start, int end, uint32_t color) {
  for (int i = start; i < end; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

void setup() {
  Serial.begin(115200);

  // Initialiser NeoPixel
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  // Configuration WiFi en mode AP
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Configurer le DNS server pour captive portal
  dnsServer.start(DNS_PORT, "*", IP);

  // Route pour la page web
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // Route pour configurer les temps
  server.on("/setTimes", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("entree")) {
      entreeTime = request->getParam("entree")->value().toInt();
      Serial.print("Entree Time: ");
      Serial.println(entreeTime);
    }
    if (request->hasParam("plat")) {
      platTime = request->getParam("plat")->value().toInt();
      Serial.print("Plat Time: ");
      Serial.println(platTime);
    }
    if (request->hasParam("fromage")) {
      fromageTime = request->getParam("fromage")->value().toInt();
      Serial.print("Fromage Time: ");
      Serial.println(fromageTime);
    }
    if (request->hasParam("dessert")) {
      dessertTime = request->getParam("dessert")->value().toInt();
      Serial.print("Dessert Time: ");
      Serial.println(dessertTime);
    }

    // Démarrer la première phase après la mise à jour des temps
    startPhase(ENTREE);
    
    request->send(200, "text/plain", "Times Updated and Timer Started");
  });

  server.begin();
}

void loop() {
  dnsServer.processNextRequest(); // Traiter les requêtes DNS pour captive portal
  updatePhase();
  delay(10); // Ajouter un léger délai pour éviter le wdt reset
}

