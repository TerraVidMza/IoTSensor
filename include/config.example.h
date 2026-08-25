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

// --- Identificacion de este nodo ---
// Cada variable fisica que mide este nodo es un "sensor" distinto en el
// backend (HU-18): un mismo ESP32 con DHT22 + humedad de suelo genera 3
// lecturas con 3 codigos de sensor diferentes. Deben coincidir con los
// sensores dados de alta en el backend (Propietario, pantalla de Sensores).
#define CODIGO_SENSOR_TEMPERATURA "NODO01-TEMP"
#define CODIGO_SENSOR_HUMEDAD_AMBIENTE "NODO01-HUM-AMB"
#define CODIGO_SENSOR_HUMEDAD_SUELO "NODO01-HUM-SUELO"

// --- Pines ---
#define PIN_DHT22 4          // GPIO al pin DATA del DHT22
#define PIN_HUMEDAD_SUELO 34 // GPIO ADC (entrada analogica) al sensor capacitivo

// --- Calibracion del sensor de humedad de suelo capacitivo ---
// Lectura analogica cruda (0-4095 en el ADC del ESP32) en seco total y en
// agua/tierra saturada. Varia segun el sensor y la humedad del suelo local:
// calibrar sumergiendo la sonda en agua (ADC_HUMEDAD_SUELO_EMPAPADO) y
// dejandola al aire seco (ADC_HUMEDAD_SUELO_SECO) antes de instalarla.
#define ADC_HUMEDAD_SUELO_SECO 3200
#define ADC_HUMEDAD_SUELO_EMPAPADO 1200

// --- Intervalo entre lecturas (ms) ---
// HU-19 no exige tiempo real estricto; 5 minutos alcanza para telemetria
// agroclimatica y ahorra bateria si el nodo corre a solar/LiPo.
#define INTERVALO_LECTURA_MS (5UL * 60UL * 1000UL)
