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
                 FINISHED,
                 NONE };

MealPhase currentPhase = NONE;
unsigned long phaseStartTime = 0;

// Démarrer une nouvelle phase
void startPhase(MealPhase phase) {
  currentPhase = phase;
  phaseStartTime = millis();

  Serial.print("Début de la phase : ");
  Serial.println(phase);

  // Si on est dans la phase NONE (attente), on commence la pulsation rose
  if (currentPhase == NONE) {
    pulseEffect(strip.Color(255, 20, 147), 50);  // Rose pulsant
    return;
  }

  // Si une phase est démarrée, faire clignoter le fond du segment 5 fois
  flashSegment(PIXEL_OFFSET + (phase * (NUMPIXELS / 4)), 5);

  // Après le clignotement, démarrer la phase
  delay(1000);  // Délai pour laisser le temps de clignoter
}

// Clignoter le fond du segment pour une phase donnée
void flashSegment(int startPixel, int flashes) {
  for (int i = 0; i < flashes; i++) {
    setColorSegment(startPixel, startPixel + (NUMPIXELS / 4), strip.Color(100, 100, 80)); // Jaune pâle
    delay(200);
    strip.clear();
    strip.show();
    delay(200);
  }
}

// Mise à jour de la phase actuelle
void updatePhase() {
  unsigned long elapsedTime = millis() - phaseStartTime;

  // Couleur de fond jaune pâle
  uint32_t backgroundColor = strip.Color(100, 100, 80);

  // Couleur de progression en fonction de la phase
  uint32_t color = 0;
  int totalPixels = NUMPIXELS / 4;  // Chaque phase utilise 1/4 des pixels
  int litPixels = 0;

  switch (currentPhase) {
    case ENTREE:
      litPixels = map(elapsedTime, 0, entreeTime * 1000, 0, totalPixels);
      color = strip.Color(100, 0, 0);  // Rouge pour l'entrée
      if (elapsedTime > entreeTime * 1000) {
        startPhase(PLAT);
      }
      break;
    case PLAT:
      litPixels = map(elapsedTime, 0, platTime * 1000, 0, totalPixels);
      color = strip.Color(0, 100, 0);  // Vert pour le plat
      if (elapsedTime > platTime * 1000) {
        startPhase(FROMAGE);
      }
      break;
    case FROMAGE:
      litPixels = map(elapsedTime, 0, fromageTime * 1000, 0, totalPixels);
      color = strip.Color(0, 0, 100);  // Bleu pour le fromage
      if (elapsedTime > fromageTime * 1000) {
        startPhase(DESSERT);
      }
      break;
    case DESSERT:
      litPixels = map(elapsedTime, 0, dessertTime * 1000, 0, totalPixels);
      color = strip.Color(100, 100, 0);  // Jaune pour le dessert
      if (elapsedTime > dessertTime * 1000) {
        startPhase(FINISHED);  // Terminer le cycle
      }
      break;
    case FINISHED:
      // Cycle terminé, appliquer une pulsation verte
      pulseEffect(strip.Color(0, 100, 0), 100);  // Vert pulsant
      return;
    case NONE:
      // Mode attente, appliquer une pulsation rose
      pulseEffect(strip.Color(255, 20, 147), 50);  // Rose pulsant
      return;
  }

  // Appliquer le fond jaune pâle au segment actif
  int startPixel = PIXEL_OFFSET + (currentPhase * totalPixels);
  setColorSegment(startPixel, startPixel + totalPixels, backgroundColor);

  // Appliquer la couleur de progression
  setColorSegment(startPixel, startPixel + litPixels, color);
}

// Appliquer une couleur à un segment de l'anneau
void setColorSegment(int start, int end, uint32_t color) {
  for (int i = start; i < end && i < NUMPIXELS; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

// Effet de pulsation pour l'attente ou la fin du cycle
void pulseEffect(uint32_t color, int maxBrightness) {
  static int brightness = 0;
  static int direction = 1;

  // Incrément plus petit pour un effet plus doux
  brightness += direction * 2;  // Ajustez la vitesse en modifiant l'incrément

  // Limiter la luminosité à maxBrightness
  if (brightness <= 0 || brightness >= maxBrightness) {
    direction = -direction;
  }

  // Appliquer la couleur avec la luminosité modifiée
  uint8_t r = (color >> 16 & 0xFF) * brightness / 255;
  uint8_t g = (color >> 8 & 0xFF) * brightness / 255;
  uint8_t b = (color & 0xFF) * brightness / 255;
  uint32_t dimColor = strip.Color(r, g, b);

  // Appliquer la couleur à tous les pixels
  for (int i = 0; i < NUMPIXELS; i++) {
    strip.setPixelColor(i, dimColor);
  }
  strip.show();

  // Délai ajusté pour une pulsation plus lente
  delay(20);  // Ajustez cette valeur pour la vitesse du pulse
}

void setup() {
  Serial.begin(115200);

  // Initialiser NeoPixel
  strip.begin();
  strip.show();  // Initialiser tous les pixels à 'off'

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
