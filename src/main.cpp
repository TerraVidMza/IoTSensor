// TerraVid — Firmware de nodo de telemetria IoT
//
// Lee temperatura y humedad ambiente (DHT22) y humedad de suelo (sensor
// capacitivo analogico), y envia cada variable como una lectura independiente
// a POST /api/lecturas-iot del backend (HU-19), autenticando con la API key
// estatica del canal de dispositivos (header X-Api-Key, HU-19 + ApiKeyGuard).
//
// Contrato exacto esperado por el backend (ver
// Backend/src/modules/lecturas-iot/dto/ingest-lectura.dto.ts):
//   POST /api/lecturas-iot
//   Header: X-Api-Key: <IOT_API_KEY>
//   Body JSON: { "codigoSensor": string, "valor": number, "unidad": string,
//                "sensorTimestamp": string ISO 8601 }
//   Respuestas: 201 ingerida | 404 sensor no registrado | 403 sensor inactivo
//               | 400 fuera de rango fisico (queda en lecturas_anomalas)

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <time.h>

#include "config.h"

DHT dht(PIN_DHT22, DHT22);

// --- Conexion WiFi ---

void conectarWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.printf("Conectando a WiFi \"%s\"...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const unsigned long inicio = millis();
  const unsigned long timeoutMs = 20000;
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < timeoutMs) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi conectado. IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nNo se pudo conectar a WiFi. Se reintentara en el proximo ciclo.");
  }
}

// --- Reloj (NTP) ---
//
// El backend persiste sensorTimestamp tal cual lo manda el nodo, sin
// corregirlo (HU-19): si el reloj del ESP32 no esta sincronizado, el dato de
// telemetria queda con una fecha incorrecta de forma silenciosa. Se sincroniza
// una vez al boot contra servidores NTP publicos, en UTC.
bool sincronizarReloj() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print("Sincronizando reloj (NTP)...");
  struct tm horaActual;
  const unsigned long inicio = millis();
  while (!getLocalTime(&horaActual, 1000) && millis() - inicio < 15000) {
    Serial.print(".");
  }

  const bool sincronizado = horaActual.tm_year > (2020 - 1900);
  Serial.println(sincronizado ? " listo." : " fallo, se reintentara.");
  return sincronizado;
}

String timestampIso8601Utc() {
  struct tm horaActual;
  if (!getLocalTime(&horaActual, 100)) {
    // Sin reloj sincronizado no hay forma de dar una fecha real: se marca el
    // dato para no reportar un timestamp epoch (1970) que arruina el
    // historial (HU-20) y las series de HU-29.
    return "";
  }
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &horaActual);
  return String(buffer);
}

// --- Envio de una lectura ---

bool enviarLectura(const char *codigoSensor, float valor, const char *unidad) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sin WiFi: se descarta la lectura de este ciclo.");
    return false;
  }

  const String timestamp = timestampIso8601Utc();
  if (timestamp.isEmpty()) {
    Serial.println("Reloj no sincronizado: se descarta la lectura de este ciclo.");
    return false;
  }

  JsonDocument payload;
  payload["codigoSensor"] = codigoSensor;
  payload["valor"] = valor;
  payload["unidad"] = unidad;
  payload["sensorTimestamp"] = timestamp;

  String cuerpo;
  serializeJson(payload, cuerpo);

  HTTPClient http;
  const String url = String(BACKEND_URL) + "/api/lecturas-iot";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Api-Key", IOT_API_KEY);
  http.setTimeout(10000);

  const int status = http.POST(cuerpo);
  const String respuesta = http.getString();
  http.end();

  Serial.printf("[%s] %s = %.2f %s -> HTTP %d\n", timestamp.c_str(), codigoSensor, valor, unidad, status);

  if (status == 201) {
    return true;
  }

  // 404 (sensor no dado de alta), 403 (sensor inactivo) y 400 (fuera de rango
  // fisico) son errores de configuracion o de calibracion, no de red: no
  // tiene sentido reintentar en caliente, pero se loguea el detalle para
  // poder diagnosticar sin acceso al backend.
  Serial.printf("  Respuesta: %s\n", respuesta.c_str());
  return false;
}

// --- Lecturas por variable ---

void leerYEnviarDht() {
  const float temperatura = dht.readTemperature();
  const float humedad = dht.readHumidity();

  if (isnan(temperatura) || isnan(humedad)) {
    Serial.println("Lectura de DHT22 invalida (revisar cableado/PIN_DHT22).");
    return;
  }

  enviarLectura(CODIGO_SENSOR_TEMPERATURA, temperatura, "°C");
  enviarLectura(CODIGO_SENSOR_HUMEDAD_AMBIENTE, humedad, "%");
}

void leerYEnviarHumedadSuelo() {
  const int lecturaCruda = analogRead(PIN_HUMEDAD_SUELO);

  // Sensor capacitivo: mas seco = mayor lectura ADC. Se invierte el mapeo
  // para que el resultado sea 0% (seco) a 100% (empapado), y se acota por si
  // la calibracion de config.h no cubre exactamente el rango observado.
  long porcentaje = map(lecturaCruda, ADC_HUMEDAD_SUELO_SECO, ADC_HUMEDAD_SUELO_EMPAPADO, 0, 100);
  porcentaje = constrain(porcentaje, 0, 100);

  enviarLectura(CODIGO_SENSOR_HUMEDAD_SUELO, static_cast<float>(porcentaje), "%");
}

// --- Ciclo principal ---

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nTerraVid IoT — arrancando nodo de telemetria");

  dht.begin();
  analogReadResolution(12); // ADC del ESP32: 0-4095

  conectarWifi();
  if (WiFi.status() == WL_CONNECTED) {
    sincronizarReloj();
  }
}

void loop() {
  conectarWifi(); // no-op si ya esta conectado; reintenta si se cayo

  if (WiFi.status() == WL_CONNECTED) {
    leerYEnviarDht();
    leerYEnviarHumedadSuelo();
  }

  delay(INTERVALO_LECTURA_MS);
}
