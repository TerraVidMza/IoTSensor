# TerraVid — IoTSensor

Firmware de los nodos de telemetría del ERP agrícola TerraVid: sensores de temperatura/humedad ambiente (DHT22) y humedad de suelo (capacitivo) montados en los cuarteles, que envían lecturas al [`Backend`](https://github.com/TerraVidMza/Backend) para monitoreo (HU-20) y alertas por umbral (HU-21/HU-22).

## Repositorios del proyecto

| Repo | Contenido |
|---|---|
| [`Backend`](https://github.com/TerraVidMza/Backend) | API REST — NestJS + TypeORM + PostgreSQL/PostGIS |
| [`Web`](https://github.com/TerraVidMza/Web) | Frontend web de oficina |
| [`Mobile`](https://github.com/TerraVidMza/Mobile) | App móvil offline-first para campo |
| [`IoTSensor`](https://github.com/TerraVidMza/IoTSensor) (este repo) | Firmware de los nodos de telemetría (ESP32) |

## Stack

- **C++ (Arduino Framework)** sobre **ESP32** (WROOM/S3), vía [PlatformIO](https://platformio.org/)
- Sensores: **DHT22** (temperatura + humedad ambiente) y sensor de **humedad de suelo capacitivo** (salida analógica)
- Transmisión: **HTTP/HTTPS + API Key estática**, directo a `POST /api/lecturas-iot` del Backend

> **Nota sobre el protocolo:** la documentación de arquitectura del Backend menciona MQTT/MQTTS como opción. Se optó por HTTP/HTTPS porque es lo que el endpoint de ingesta ya implementa y tiene probado (autenticación por `X-Api-Key`, validación de rango físico, manejo de sensor inactivo/no registrado — ver [`docs/specs/HU-19`](https://github.com/TerraVidMza/Backend/blob/main/docs/specs/HU-19-ingesta-lecturas-sensores.md) del Backend). MQTT requeriría sumar un broker (ej. Mosquitto) que hoy no existe en el stack.

## Cómo funciona

Cada variable física que mide un nodo es un **sensor independiente** en el backend (HU-18): un mismo ESP32 con DHT22 + sonda de suelo genera **3 lecturas** con 3 códigos de sensor distintos (`NODO01-TEMP`, `NODO01-HUM-AMB`, `NODO01-HUM-SUELO` por defecto, configurable).

**Importante:** esos 3 sensores tienen que existir previamente en el backend (el Propietario los da de alta desde la Web, HU-18) *antes* de que el nodo mande su primera lectura — si el código no está registrado, el backend responde `404 Sensor no registrado`.

Flujo de cada ciclo del firmware:

1. Se asegura de estar conectado a WiFi (reintenta si se cayó).
2. Sincroniza el reloj por NTP (una vez al boot) — el timestamp que manda el nodo es el que el backend persiste tal cual, así que un reloj desincronizado ensucia el historial.
3. Lee DHT22 (temperatura, humedad ambiente) y el sensor de humedad de suelo.
4. Envía cada lectura como un `POST /api/lecturas-iot` separado, con el header `X-Api-Key`.
5. Espera `INTERVALO_LECTURA_MS` (5 minutos por defecto) y repite.

## Wiring (ESP32 WROOM-32)

| Sensor | Pin sensor | Pin ESP32 (default en `config.example.h`) |
|---|---|---|
| DHT22 — DATA | DATA | GPIO 4 (con resistencia pull-up de 10kΩ a 3.3V si el módulo no la trae integrada) |
| DHT22 — VCC / GND | VCC / GND | 3.3V / GND |
| Humedad de suelo capacitivo — AOUT | salida analógica | GPIO 34 (entrada ADC, *input-only*, no soporta pull-up interno) |
| Humedad de suelo capacitivo — VCC / GND | VCC / GND | 3.3V / GND |

Los pines son configurables en `config.h` (`PIN_DHT22`, `PIN_HUMEDAD_SUELO`).

## Puesta en marcha (con hardware)

1. Instalar [PlatformIO](https://platformio.org/install) (extensión de VS Code, o CLI standalone).
2. Copiar la plantilla de configuración y completarla:
   ```bash
   cp include/config.example.h include/config.h
   ```
   Editar `include/config.h`: SSID/contraseña de WiFi, URL del backend, `IOT_API_KEY` (debe coincidir con la del `.env` del backend), y los códigos de sensor (deben existir ya dados de alta en el backend).
3. Calibrar el sensor de humedad de suelo (varía por unidad): sumergido en agua anotar el valor de `ADC_HUMEDAD_SUELO_EMPAPADO`, seco al aire anotar `ADC_HUMEDAD_SUELO_SECO`, y cargar ambos en `config.h`. Se puede leer el valor crudo por el monitor serie antes de calibrar.
4. Compilar y flashear:
   ```bash
   pio run --target upload
   pio device monitor
   ```

## Probar sin hardware (simulador)

Mientras no hay un ESP32 físico a mano, `simulador/simulador.py` manda el mismo contrato HTTP que mandará el firmware real, para poder probar todo el flujo (ingesta → umbrales → alertas → dashboard) de punta a punta:

```bash
cd simulador
pip install -r requirements.txt
cp .env.example .env   # completar BACKEND_URL e IOT_API_KEY
python simulador.py                # loop continuo, una tanda cada 30s
python simulador.py --once         # una sola tanda y termina
python simulador.py --anomalo      # fuerza un valor fuera de rango físico.
                                    # El backend responde 201 igual (HU-19),
                                    # pero aparta el dato a lecturas_anomalas
                                    # en vez de sumarlo a los promedios
```

## Contribuir

Ver [`CONTRIBUTING.md`](CONTRIBUTING.md) para la política de branching (Gitflow) del proyecto.
