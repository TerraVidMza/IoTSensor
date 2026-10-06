// Copiar este archivo a "config.h" (mismo directorio) y completar con los
// valores reales. "config.h" esta en .gitignore: nunca se commitea, porque
// tiene la contraseña de WiFi y la API key del backend.
#pragma once

// --- WiFi ---
#define WIFI_SSID "NombreDeTuRedWiFi"
#define WIFI_PASSWORD "contraseñaDeTuRedWiFi"

// --- Backend TerraVid ---
// Sin protocolo final "/" y SIN el "/api" (se agrega en el codigo).
// Ejemplo produccion: "https://terravid-api.duckdns.org"
// Ejemplo local (ESP32 y PC en la misma red WiFi, IP de la PC, no "localhost"):
// "http://192.168.1.100:3000"
#define BACKEND_URL "https://CAMBIAR-ANTES-DE-USAR.duckdns.org"

// Debe coincidir exactamente con IOT_API_KEY configurada en el .env del backend.
#define IOT_API_KEY "CAMBIAR-POR-LA-CLAVE-REAL"

// --- Transporte de telemetria (HTTP vs MQTT) ---
//
// Si MQTT_BROKER_URL esta definido, el firmware usa MQTTS como canal principal
// y HTTP como respaldo (cuando el broker no responde).
// Si MQTT_BROKER_URL esta comentado o vacio, la telemetria va siempre por HTTP.
//
// Broker de produccion: mqtts://terravid-api.duckdns.org:8883 (TLS, puerto 8883)
// Broker local (desarrollo): mqtt://192.168.1.100:1883 (sin TLS)
#define MQTT_BROKER_URL "mqtts://CAMBIAR-ANTES-DE-USAR.duckdns.org:8883"

// Credenciales MQTT por sensor (D-026).
//
// El broker Mosquitto usa dynamic security: cada sensor tiene su propio
// usuario (= su codigo) y solo puede publicar en su topico propio
// (terravid/sensores/{codigo}/lecturas). Por eso un nodo con 3 sensores
// necesita 3 conjuntos de credenciales — uno por variable.
//
// Obtenerlas: en la Web ir a Telemetria → sensor → "Credencial MQTT".
// Cada llamada regenera la credencial; la password se ve una sola vez.
//
// MQTT_USUARIO_* debe ser identico a CODIGO_SENSOR_* de abajo.
#define MQTT_USUARIO_TEMPERATURA    "NODO01-TEMP"
#define MQTT_PASSWORD_TEMPERATURA   "CAMBIAR-POR-LA-PASSWORD-GENERADA"

#define MQTT_USUARIO_HUMEDAD_AMB    "NODO01-HUM-AMB"
#define MQTT_PASSWORD_HUMEDAD_AMB   "CAMBIAR-POR-LA-PASSWORD-GENERADA"

#define MQTT_USUARIO_HUMEDAD_SUELO  "NODO01-HUM-SUELO"
#define MQTT_PASSWORD_HUMEDAD_SUELO "CAMBIAR-POR-LA-PASSWORD-GENERADA"

// --- Identificacion de este nodo ---
// Cada variable fisica que mide este nodo es un "sensor" distinto en el
// backend (HU-18): un mismo ESP32 con DHT22 + humedad de suelo genera 3
// lecturas con 3 codigos de sensor diferentes. Deben coincidir con los
// sensores dados de alta en el backend (Propietario, pantalla de Sensores).
#define CODIGO_SENSOR_TEMPERATURA "NODO01-TEMP"
#define CODIGO_SENSOR_HUMEDAD_AMBIENTE "NODO01-HUM-AMB"
#define CODIGO_SENSOR_HUMEDAD_SUELO "NODO01-HUM-SUELO"

// --- Sensores conectados ---
// Poner en 0 el que todavia no este cableado. Con un sensor desconectado la
// entrada analogica flota y se mandarian valores al azar como mediciones.
// Con los dos en 0 el nodo solo hace el autodiagnostico de arranque (WiFi,
// reloj y credenciales MQTT) y no publica nada.
#define SENSOR_DHT22_CONECTADO 1
#define SENSOR_SUELO_CONECTADO 1

// --- Pines y calibracion, segun la placa ---
#if defined(ESP8266)
// NodeMCU v1.0 (ESP-12E). GPIO 4 esta rotulado "D2" en la placa.
#define PIN_DHT22 4           // D2 -> DATA del DHT22
#define PIN_HUMEDAD_SUELO A0  // unica entrada analogica (0-1023, admite 0-3,3 V)
// Lectura cruda en seco y en agua. Calibrar con el sensor real: anotar el
// valor que muestra el monitor serie con la sonda al aire y sumergida.
#define ADC_HUMEDAD_SUELO_SECO 800
#define ADC_HUMEDAD_SUELO_EMPAPADO 300
#else
// ESP32
#define PIN_DHT22 4           // GPIO 4 -> DATA del DHT22
#define PIN_HUMEDAD_SUELO 34  // GPIO ADC (entrada analogica, 0-4095)
#define ADC_HUMEDAD_SUELO_SECO 3200
#define ADC_HUMEDAD_SUELO_EMPAPADO 1200
#endif

// --- Intervalo entre lecturas (ms) ---
// HU-19 no exige tiempo real estricto; 5 minutos alcanza para telemetria
// agroclimatica y ahorra bateria si el nodo corre a solar/LiPo.
#define INTERVALO_LECTURA_MS (5UL * 60UL * 1000UL)
