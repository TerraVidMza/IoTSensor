// TerraVid — Firmware de nodo de telemetria IoT
//
// Lee temperatura y humedad ambiente (DHT22) y humedad de suelo (sensor
// capacitivo analogico), y envia cada variable como una lectura independiente
// al backend por MQTT (si MQTT_BROKER_URL esta definido en config.h) o por
// HTTP como respaldo.
//
// Transporte preferido: MQTTS (TLS, puerto 8883). El broker Mosquitto usa
// dynamic security (D-026): cada sensor tiene su propio usuario y password y
// solo puede publicar en su topico. Un nodo con 3 sensores usa 3 credenciales.
// En cada ciclo cada sensor se conecta, publica y se desconecta: el nodo
// duerme 5 minutos entre lecturas, mucho mas que el keepalive, y una conexion
// que el broker ya cerro puede aceptar un publish sin que el dato llegue.
// Si el broker no esta configurado o no responde, el firmware cae a HTTP.
//
// TLS verificado contra las raices de Let's Encrypt (include/certificados_ca.h)
// tanto para MQTTS como para el respaldo HTTPS.
//
// Contrato MQTT (ver docs/IOT-MQTT.md del Backend):
//   Topico: terravid/sensores/{codigo}/lecturas
//   QoS: 0. PubSubClient no publica con QoS 1; la confirmacion de entrega la
//   da la reconexion en cada ciclo y, si falla, el respaldo HTTP.
//   Payload JSON: { "valor": number, "unidad": string,
//                   "sensorTimestamp": string ISO 8601 }
//   El codigoSensor NO va en el payload MQTT — sale del topico autenticado.
//
// Contrato HTTP de respaldo (ver HU-19):
//   POST /api/lecturas-iot
//   Header: X-Api-Key: <IOT_API_KEY>
//   Body JSON: { "codigoSensor": string, "valor": number, "unidad": string,
//                "sensorTimestamp": string ISO 8601 }
//   Respuestas: 201 ingerida | 404 sensor no registrado | 403 sensor inactivo

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <time.h>

#include "config.h"
#include "certificados_ca.h"

#ifdef MQTT_BROKER_URL
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#endif

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

// =============================================================================
// Transporte MQTT (solo si MQTT_BROKER_URL esta definido en config.h)
// =============================================================================

#ifdef MQTT_BROKER_URL

// Datos de un cliente MQTT independiente para un sensor.
// Cada sensor tiene su propio usuario y puede publicar solo en su topico.
struct ClienteMqtt {
  // Constructor explicito: con el C++ de la toolchain del ESP32, el
  // inicializador por defecto de `cliente` impedia la inicializacion con
  // llaves y el firmware no compilaba con MQTT_BROKER_URL definido.
  ClienteMqtt(const char* u, const char* p) : usuario(u), password(p) {}

  const char* usuario;
  const char* password;
  WiFiClientSecure clienteSeguro;
  WiFiClient clientePlano;
  PubSubClient* cliente = nullptr;
};

// Un cliente MQTT por sensor. El broker rechazaria publicaciones cruzadas
// (un usuario publicando en el topico de otro sensor — D-026).
static ClienteMqtt mqttTemperatura   { MQTT_USUARIO_TEMPERATURA,   MQTT_PASSWORD_TEMPERATURA };
static ClienteMqtt mqttHumedadAmb    { MQTT_USUARIO_HUMEDAD_AMB,   MQTT_PASSWORD_HUMEDAD_AMB };
static ClienteMqtt mqttHumedadSuelo  { MQTT_USUARIO_HUMEDAD_SUELO, MQTT_PASSWORD_HUMEDAD_SUELO };

static bool mqttUsaTls = true;
static String mqttHost;
static uint16_t mqttPuerto;
static bool mqttInicializado = false;

// Parsea "mqtts://host:puerto" o "mqtt://host:puerto".
bool parsearBrokerUrl(const char* url, String& host, uint16_t& puerto, bool& tls) {
  String s(url);
  if (s.startsWith("mqtts://")) {
    tls = true;
    s = s.substring(8);
  } else if (s.startsWith("mqtt://")) {
    tls = false;
    s = s.substring(7);
  } else {
    return false;
  }
  int sep = s.lastIndexOf(':');
  if (sep < 0) {
    host = s;
    puerto = tls ? 8883 : 1883;
  } else {
    host = s.substring(0, sep);
    puerto = static_cast<uint16_t>(s.substring(sep + 1).toInt());
  }
  return true;
}

/** Prepara el PubSubClient de un sensor (no conecta aun). */
void inicializarClienteMqtt(ClienteMqtt& cm) {
  if (mqttUsaTls) {
    // Verifica el certificado del broker contra las raices de Let's Encrypt:
    // con setInsecure() cualquiera en el camino podia hacerse pasar por el
    // broker y quedarse con la contrasena del sensor.
    cm.clienteSeguro.setCACert(RAICES_CA_PEM);
    cm.cliente = new PubSubClient(cm.clienteSeguro);
  } else {
    cm.cliente = new PubSubClient(cm.clientePlano);
  }
  cm.cliente->setServer(mqttHost.c_str(), mqttPuerto);
  cm.cliente->setKeepAlive(30);
  cm.cliente->setSocketTimeout(10);
}

/** Inicializa todos los clientes MQTT. Se llama una vez en setup(). */
void inicializarMqtt() {
  if (!parsearBrokerUrl(MQTT_BROKER_URL, mqttHost, mqttPuerto, mqttUsaTls)) {
    Serial.printf("MQTT_BROKER_URL invalida: \"%s\". Se usara solo HTTP.\n", MQTT_BROKER_URL);
    return;
  }
  Serial.printf("MQTT %s -> %s:%u\n", mqttUsaTls ? "TLS" : "sin TLS",
                mqttHost.c_str(), mqttPuerto);

  inicializarClienteMqtt(mqttTemperatura);
  inicializarClienteMqtt(mqttHumedadAmb);
  inicializarClienteMqtt(mqttHumedadSuelo);
  mqttInicializado = true;
}

/**
 * Conecta (o reconecta) un cliente MQTT al broker.
 * Cada sensor se autentica con su propio usuario: el broker lo limita
 * a publicar solo en su topico (D-026).
 */
bool conectarClienteMqtt(ClienteMqtt& cm) {
  if (!mqttInicializado || !cm.cliente) return false;
  if (WiFi.status() != WL_CONNECTED) return false;
  // Siempre una conexion nueva: la del ciclo anterior ya fue cerrada.
  if (cm.cliente->connected()) cm.cliente->disconnect();

  const String clientId = String(cm.usuario) + "-" + String(random(0xffff), HEX);
  const bool ok = cm.cliente->connect(clientId.c_str(), cm.usuario, cm.password);
  if (!ok) {
    Serial.printf("  [%s] broker rechazo la conexion (estado=%d)\n",
                  cm.usuario, cm.cliente->state());
  }
  return ok;
}

/**
 * Publica una lectura por MQTT (QoS 0) con la credencial propia del sensor:
 * conecta, publica y desconecta.
 *
 * El codigoSensor NO va en el payload: sale del topico, que identifica al
 * sensor autenticado (D-026). El backend ignora codigoSensor en el payload
 * MQTT aunque estuviera presente.
 */
bool publicarPorMqtt(ClienteMqtt& cm, const char* codigoSensor,
                     float valor, const char* unidad, const String& timestamp) {
  if (!conectarClienteMqtt(cm)) return false;

  JsonDocument payload;
  payload["valor"] = valor;
  payload["unidad"] = unidad;
  payload["sensorTimestamp"] = timestamp;

  String cuerpo;
  serializeJson(payload, cuerpo);

  const String topico = String("terravid/sensores/") + codigoSensor + "/lecturas";
  bool ok = cm.cliente->publish(topico.c_str(), cuerpo.c_str(), /* retain= */ false);
  // loop() vacia el buffer de salida y detecta si el broker corto la conexion
  // (por ejemplo, una ACL que rechaza el topico). Recien despues se desconecta.
  cm.cliente->loop();
  ok = ok && cm.cliente->connected();
  cm.cliente->disconnect();

  Serial.printf("[MQTT] [%s] %s = %.2f %s -> %s\n",
                timestamp.c_str(), codigoSensor, valor, unidad,
                ok ? "publicado" : "fallo (cae a HTTP)");
  return ok;
}

#endif  // MQTT_BROKER_URL

// =============================================================================
// Transporte HTTP (respaldo, o unico si MQTT no esta configurado)
// =============================================================================

/**
 * Envia una lectura por HTTP POST (HU-19).
 * Se usa cuando MQTT no esta configurado o cuando el broker no responde.
 */
bool enviarPorHttp(const char* codigoSensor, float valor, const char* unidad,
                   const String& timestamp) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sin WiFi: se descarta la lectura de este ciclo.");
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
  if (url.startsWith("https://")) {
    http.begin(url, RAICES_CA_PEM);  // verifica el certificado de la API
  } else {
    http.begin(url);                  // solo desarrollo, en la red local
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Api-Key", IOT_API_KEY);
  http.setTimeout(10000);

  const int status = http.POST(cuerpo);
  const String respuesta = http.getString();
  http.end();

  Serial.printf("[HTTP] [%s] %s = %.2f %s -> HTTP %d\n",
                timestamp.c_str(), codigoSensor, valor, unidad, status);

  if (status == 201) {
    return true;
  }

  // 404 (sensor no dado de alta), 403 (sensor inactivo) y 400 (fuera de rango
  // fisico) son errores de configuracion o de calibracion, no de red.
  Serial.printf("  Respuesta: %s\n", respuesta.c_str());
  return false;
}

// =============================================================================
// Envio con seleccion de transporte
// =============================================================================

/**
 * Intenta MQTT con el cliente del sensor; si falla, cae a HTTP.
 * Sin MQTT configurado, va directo a HTTP.
 */
bool enviarLectura(
#ifdef MQTT_BROKER_URL
    ClienteMqtt& clienteMqtt,
#endif
    const char* codigoSensor, float valor, const char* unidad) {

  const String timestamp = timestampIso8601Utc();
  if (timestamp.isEmpty()) {
    Serial.println("Reloj no sincronizado: se descarta la lectura de este ciclo.");
    return false;
  }

#ifdef MQTT_BROKER_URL
  if (publicarPorMqtt(clienteMqtt, codigoSensor, valor, unidad, timestamp)) {
    return true;
  }
  Serial.println("  Reintentando por HTTP...");
#endif

  return enviarPorHttp(codigoSensor, valor, unidad, timestamp);
}

// --- Lecturas por variable ---

void leerYEnviarDht() {
  const float temperatura = dht.readTemperature();
  const float humedad = dht.readHumidity();

  if (isnan(temperatura) || isnan(humedad)) {
    Serial.println("Lectura de DHT22 invalida (revisar cableado/PIN_DHT22).");
    return;
  }

  enviarLectura(
#ifdef MQTT_BROKER_URL
      mqttTemperatura,
#endif
      CODIGO_SENSOR_TEMPERATURA, temperatura, "°C");

  enviarLectura(
#ifdef MQTT_BROKER_URL
      mqttHumedadAmb,
#endif
      CODIGO_SENSOR_HUMEDAD_AMBIENTE, humedad, "%");
}

void leerYEnviarHumedadSuelo() {
  const int lecturaCruda = analogRead(PIN_HUMEDAD_SUELO);

  // Sensor capacitivo: mas seco = mayor lectura ADC. Se invierte el mapeo
  // para que el resultado sea 0% (seco) a 100% (empapado), y se acota por si
  // la calibracion de config.h no cubre exactamente el rango observado.
  long porcentaje = map(lecturaCruda, ADC_HUMEDAD_SUELO_SECO, ADC_HUMEDAD_SUELO_EMPAPADO, 0, 100);
  porcentaje = constrain(porcentaje, 0, 100);

  enviarLectura(
#ifdef MQTT_BROKER_URL
      mqttHumedadSuelo,
#endif
      CODIGO_SENSOR_HUMEDAD_SUELO, static_cast<float>(porcentaje), "%");
}

// --- Ciclo principal ---

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nTerraVid IoT — arrancando nodo de telemetria");

#ifdef MQTT_BROKER_URL
  Serial.println("Transporte: MQTT (con HTTP como respaldo)");
#else
  Serial.println("Transporte: HTTP");
#endif

  dht.begin();
  analogReadResolution(12); // ADC del ESP32: 0-4095

  conectarWifi();
  if (WiFi.status() == WL_CONNECTED) {
    sincronizarReloj();
#ifdef MQTT_BROKER_URL
    inicializarMqtt();
#endif
  }
}

void loop() {
  conectarWifi(); // no-op si ya esta conectado; reintenta si se cayo

  if (WiFi.status() == WL_CONNECTED) {
    // Si al arrancar no habia WiFi, el reloj y MQTT quedaron sin preparar.
    if (timestampIso8601Utc().isEmpty()) sincronizarReloj();
#ifdef MQTT_BROKER_URL
    if (!mqttInicializado) inicializarMqtt();
#endif
  }

  if (WiFi.status() == WL_CONNECTED) {
    leerYEnviarDht();
    leerYEnviarHumedadSuelo();
  }

  delay(INTERVALO_LECTURA_MS);
}
