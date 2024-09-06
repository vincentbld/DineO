#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

// Configuration NeoPixel
#define PIN D6        // Utilisez la broche D6 pour les NeoPixels sur le Weemos D1 R1
#define NUMPIXELS 60  // Changez selon le nombre de LEDs sur votre anneau
const int PIXEL_OFFSET = 15;  // Décalage pour démarrer à partir du pixel 15

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Configuration WiFi en mode AP
const char *ssid = "MealTimerAP";
const char *password = "password123";

// DNS server pour captive portal
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Définir les durées (en secondes) pour chaque segment
unsigned long entreeTime = 60;    // 1 minute
unsigned long platTime = 300;     // 5 minutes
unsigned long fromageTime = 120;  // 2 minutes
unsigned long dessertTime = 120;  // 2 minutes

AsyncWebServer server(80);

// Page web HTML
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html lang="fr"><head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Minuteur de Repas</title>
  <style>
    body { font-family: 'Arial', sans-serif; background-color: #7EB5B2; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; }
    .container { background-color: #fff; border-radius: 10px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); max-width: 400px; width: 100%; padding: 20px; text-align: center; }
    h1 { color: #2E3374; font-size: 24px; margin-bottom: 20px; }
    label { display: block; color: #2E3374; font-size: 18px; margin-bottom: 10px; }
    input[type="text"] { width: 100%; padding: 10px; margin-bottom: 20px; border: 1px solid #ccc; border-radius: 5px; font-size: 16px; }
    input[type="submit"] { width: 100%; padding: 12px; background-color: #EA6F20; color: #fff; border: none; border-radius: 5px; font-size: 18px; cursor: pointer; transition: background-color 0.3s; }
    input[type="submit"]:hover { background-color: #EA6F20; }
  </style>
</head>
<body>
  <div class="container">
    <h1>Configurer le Minuteur de Repas</h1>
    <form action="/setTimes" method="GET">
      <label for="entree">Temps pour l'Entrée (secondes):</label>
      <input type="text" id="entree" name="entree" value="60">
      
      <label for="plat">Temps pour le Plat (secondes):</label>
      <input type="text" id="plat" name="plat" value="300">
      
      <label for="fromage">Temps pour le Fromage (secondes):</label>
      <input type="text" id="fromage" name="fromage" value="120">
      
      <label for="dessert">Temps pour le Dessert (secondes):</label>
      <input type="text" id="dessert" name="dessert" value="120">
      
      <input type="submit" value="Définir les temps">
    </form>
  </div>
</body>
</html>
)rawliteral";

enum MealPhase { ENTREE,
                 PLAT,
                 FROMAGE,
                 DESSERT,
                 NONE };

MealPhase currentPhase = NONE;
unsigned long phaseStartTime = 0;
unsigned long lastPulseTime = 0;
int pulseState = 0;  // Variable pour gérer l'état de la pulsation (de 0 à 255)
bool timerStarted = false;  // Indicateur pour savoir si le minuteur a démarré

void startPhase(MealPhase phase) {
  currentPhase = phase;
  phaseStartTime = millis();
  timerStarted = true;  // Indiquer que le minuteur a démarré

  Serial.print("Début de la phase : ");
  Serial.println(phase);

  strip.clear();
  strip.show();
}

void updatePhase() {
  if (!timerStarted) {
    pulseEffect();  // Effet de pulsation tant que le minuteur n'a pas démarré
    return;
  }

  unsigned long elapsedTime = millis() - phaseStartTime;

  int totalPixels = NUMPIXELS / 4;
  int litPixels = 0;
  uint32_t color = 0;
  uint32_t backgroundColor = strip.Color(255, 255, 180);  // Jaune très pâle pour le fond

  switch (currentPhase) {
    case ENTREE:
      litPixels = map(elapsedTime, 0, entreeTime * 1000, 0, totalPixels);
      color = strip.Color(100, 0, 0);  // Rouge
      if (elapsedTime > entreeTime * 1000) {
        startPhase(PLAT);
        return;
      }
      break;
    case PLAT:
      litPixels = map(elapsedTime, 0, platTime * 1000, 0, totalPixels);
      color = strip.Color(0, 100, 0);  // Vert
      if (elapsedTime > platTime * 1000) {
        startPhase(FROMAGE);
        return;
      }
      break;
    case FROMAGE:
      litPixels = map(elapsedTime, 0, fromageTime * 1000, 0, totalPixels);
      color = strip.Color(0, 0, 100);  // Bleu
      if (elapsedTime > fromageTime * 1000) {
        startPhase(DESSERT);
        return;
      }
      break;
    case DESSERT:
      litPixels = map(elapsedTime, 0, dessertTime * 1000, 0, totalPixels);
      color = strip.Color(100, 100, 0);  // Jaune
      if (elapsedTime > dessertTime * 1000) {
        startPhase(NONE);
        return;
      }
      break;
    case NONE:
      strip.clear();
      strip.show();
      return;
  }

  int startPixel = currentPhase * totalPixels + PIXEL_OFFSET;  // Appliquer le décalage de 15 pixels
  
  // Colorer tous les pixels du segment avec la couleur de fond
  setColorSegment(startPixel, startPixel + totalPixels, backgroundColor);
  
  // Colorer la progression avec la couleur correspondante
  setColorSegment(startPixel, startPixel + litPixels, color);
}

void setColorSegment(int start, int end, uint32_t color) {
  for (int i = start; i < end; i++) {
    if (i >= NUMPIXELS) break;  // S'assurer de ne pas dépasser le nombre total de pixels
    strip.setPixelColor(i, color);
  }
  strip.show();
}

void pulseEffect() {
  unsigned long currentTime = millis();
  if (currentTime - lastPulseTime >= 20) {  // Contrôler la vitesse de la pulsation
    pulseState = (pulseState + 5) % 510;  // Valeur cyclique de 0 à 255 et retour
    int brightness = pulseState <= 255 ? pulseState : 510 - pulseState;  // Augmenter puis diminuer la luminosité
    uint32_t pulseColor = strip.Color(brightness, 0, brightness);  // Couleur rose avec intensité variable
    for (int i = 0; i < NUMPIXELS; i++) {
      strip.setPixelColor(i, pulseColor);
    }
    strip.show();
    lastPulseTime = currentTime;
  }
}

void setup() {
  Serial.begin(115200);

  // Initialiser NeoPixel
  strip.begin();
  strip.show();  // Initialize all pixels to 'off'

  // Configuration WiFi en mode AP
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Configurer le DNS server pour captive portal
  dnsServer.start(DNS_PORT, "*", IP);

  // Route pour la page web
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  // Route pour configurer les temps
  server.on("/setTimes", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("entree")) {
      entreeTime = request->getParam("entree")->value().toInt();
      Serial.print("Temps pour l'entrée : ");
      Serial.println(entreeTime);
    }
    if (request->hasParam("plat")) {
      platTime = request->getParam("plat")->value().toInt();
      Serial.print("Temps pour le plat : ");
      Serial.println(platTime);
    }
    if (request->hasParam("fromage")) {
      fromageTime = request->getParam("fromage")->value().toInt();
      Serial.print("Temps pour le fromage :");
      Serial.println(fromageTime);
    }
    if (request->hasParam("dessert")) {
      dessertTime = request->getParam("dessert")->value().toInt();
      Serial.print("Temps pour le dessert : ");
      Serial.println(dessertTime);
    }

    // Démarrer la première phase après la mise à jour des temps
    startPhase(ENTREE);

    request->send(200, "text/plain", "Temps mis à jour et minuterie démarrée");
  });

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();  // Traiter les requêtes DNS pour captive portal
  updatePhase();
  delay(10);  // Ajouter un léger délai pour éviter le wdt reset
}
