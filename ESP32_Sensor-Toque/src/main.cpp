#include <Arduino.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define PIN_SENSOR  14
#define LED_ROJO    17
#define LED_VERDE   19
#define PIN_BOOT     0  // botón BOOT integrado

const char* SERVER_EVENTO = "http://3.131.82.32:3000/api/evento";
const char* SERVER_ESTADO = "http://3.131.82.32:3000/api/estado";

unsigned long ultimoPolling  = 0;
unsigned long ultimoParpadeo = 0;
const int INTERVALO_POLLING  = 500;
const int INTERVALO_PARPADEO = 300;

String estadoActual  = "verde";
bool estadoParpadeo  = false;
bool sensorEnviado   = false;
unsigned long tiempoSensor = 0;

// Control botón BOOT
unsigned long tiempoBootPresionado = 0;
bool bootPresionado = false;
bool portalActivado = false;

TaskHandle_t tareaHTTP;
bool pedirEstado  = false;
bool pedirEvento  = false;
bool pedirPortal  = false;
String eventoQueue = "";

WiFiManager wm;

void tareaHTTPFunc(void* param) {
  for (;;) {
    if (pedirEvento && eventoQueue != "") {
      pedirEvento = false;
      HTTPClient http;
      http.begin(SERVER_EVENTO);
      http.addHeader("Content-Type", "application/json");
      String body = "{\"evento\":\"" + eventoQueue + "\"}";
      int code = http.POST(body);
      Serial.println("POST " + eventoQueue + " -> HTTP " + String(code));
      http.end();
      eventoQueue = "";
    }

    if (pedirEstado) {
      pedirEstado = false;
      HTTPClient http;
      http.begin(SERVER_ESTADO);
      int code = http.GET();
      if (code == 200) {
        String payload = http.getString();
        StaticJsonDocument<128> doc;
        deserializeJson(doc, payload);
        estadoActual = doc["semaforo"].as<String>();
      }
      http.end();
    }

    if (pedirPortal) {
      pedirPortal = false;
      Serial.println(">> Activando portal WiFi...");

      // Parpadeo 3 veces rápido como confirmación
      for (int i = 0; i < 3; i++) {
        digitalWrite(LED_ROJO, HIGH);
        digitalWrite(LED_VERDE, HIGH);
        vTaskDelay(150 / portTICK_PERIOD_MS);
        digitalWrite(LED_ROJO, LOW);
        digitalWrite(LED_VERDE, LOW);
        vTaskDelay(150 / portTICK_PERIOD_MS);
      }

      wm.resetSettings();
      wm.startConfigPortal("Semaforo-IoT", "semaforo123");

      Serial.println(">> WiFi reconfigurado, reiniciando...");
      ESP.restart();
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void actualizarLeds() {
  if (portalActivado) return; // no tocar LEDs durante portal
  unsigned long ahora = millis();

  if (estadoActual == "rojo") {
    digitalWrite(LED_ROJO,  LOW);
    digitalWrite(LED_VERDE, HIGH);
  } else if (estadoActual == "ambar") {
    if (ahora - ultimoParpadeo >= INTERVALO_PARPADEO) {
      ultimoParpadeo = ahora;
      estadoParpadeo = !estadoParpadeo;
      digitalWrite(LED_ROJO,  estadoParpadeo ? HIGH : LOW);
      digitalWrite(LED_VERDE, estadoParpadeo ? LOW  : HIGH);
    }
  } else {
    // verde
    digitalWrite(LED_ROJO,  HIGH);
    digitalWrite(LED_VERDE, LOW);
  }
}

void verificarBoot() {
  bool botonActivo = (digitalRead(PIN_BOOT) == LOW); // BOOT es activo en LOW

  if (botonActivo && !bootPresionado) {
    // Acaba de presionar
    bootPresionado = true;
    tiempoBootPresionado = millis();
    Serial.println("BOOT presionado...");
  }

  if (bootPresionado && botonActivo) {
    // Sigue presionado — verificar si pasaron 3 segundos
    if (millis() - tiempoBootPresionado >= 3000 && !portalActivado) {
      portalActivado = true;
      pedirPortal = true;
      Serial.println(">> 3 segundos! Activando portal...");
    }
  }

  if (!botonActivo && bootPresionado) {
    // Soltó el botón
    bootPresionado = false;
    if (!portalActivado) {
      Serial.println("BOOT soltado (menos de 3s, ignorado)");
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SENSOR, INPUT);
  pinMode(LED_ROJO,   OUTPUT);
  pinMode(LED_VERDE,  OUTPUT);
  pinMode(PIN_BOOT,   INPUT_PULLUP);

  digitalWrite(LED_ROJO,  HIGH);
  digitalWrite(LED_VERDE, HIGH);

  wm.setAPCallback([](WiFiManager* wm) {
    Serial.println("Conectate al WiFi: Semaforo-IoT");
    Serial.println("Contraseña: semaforo123");
    Serial.println("Luego abre: 192.168.4.1");
  });

  if (!wm.autoConnect("Semaforo-IoT", "semaforo123")) {
    Serial.println("Fallo WiFi, reiniciando...");
    delay(2000);
    ESP.restart();
  }

  Serial.println("WiFi conectado! IP: " + WiFi.localIP().toString());
  digitalWrite(LED_ROJO,  HIGH);
  digitalWrite(LED_VERDE, LOW);

  xTaskCreatePinnedToCore(
    tareaHTTPFunc,
    "tareaHTTP",
    8192,
    NULL,
    1,
    &tareaHTTP,
    0
  );

  Serial.println("Sistema listo. Esperando peaton...");
  Serial.println("(Mantén BOOT 3 segundos para cambiar WiFi)");
}

void loop() {
  unsigned long ahora = millis();

  // Verificar botón BOOT
  verificarBoot();

  // Sensor
  bool sensorActivo = digitalRead(PIN_SENSOR);
  if (sensorActivo && !sensorEnviado) {
    Serial.println(">> Peaton solicita paso!");
    eventoQueue  = "solicitud_peaton";
    pedirEvento  = true;
    sensorEnviado = true;
    tiempoSensor  = ahora;
  }
  if (sensorEnviado && ahora - tiempoSensor >= 2000) {
    sensorEnviado = false;
  }

  // Polling
  if (ahora - ultimoPolling >= INTERVALO_POLLING) {
    ultimoPolling = ahora;
    pedirEstado   = true;
  }

  actualizarLeds();
  delay(10);
}