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
- Transmisión: **MQTTS (TLS, puerto 8883)** como canal preferido + **HTTP/HTTPS + API Key** como respaldo

> **Canales de telemetría:** el firmware soporta dos transportes:
>
> - **MQTT** (preferido): cada sensor tiene su propio usuario y contraseña en el broker Mosquitto, generados por el backend ([D-026](../../Backend/docs/DECISIONS.md)). La identidad la da el tópico (`terravid/sensores/{codigo}/lecturas`), no el payload. Un nodo comprometido no puede publicar como otro sensor.
> - **HTTP** (respaldo): `POST /api/lecturas-iot` con `X-Api-Key` estático, como implementa HU-19. Se activa automáticamente si el broker no está configurado o no responde.
>
> Ver [`docs/IOT-MQTT-FIRMWARE.md`](docs/IOT-MQTT-FIRMWARE.md) para el aprovisionamiento de credenciales MQTT y [`docs/IOT-MQTT.md`](../../Backend/docs/IOT-MQTT.md) del Backend para el contrato de mensajes.

## Cómo funciona

Cada variable física que mide un nodo es un **sensor independiente** en el backend (HU-18): un mismo ESP32 con DHT22 + sonda de suelo genera **3 lecturas** con 3 códigos de sensor distintos (`NODO01-TEMP`, `NODO01-HUM-AMB`, `NODO01-HUM-SUELO` por defecto, configurable).

**Importante:** esos 3 sensores tienen que existir previamente en el backend (el Propietario los da de alta desde la Web, HU-18) *antes* de que el nodo mande su primera lectura — si el código no está registrado, el backend responde `404 Sensor no registrado`.

Flujo de cada ciclo del firmware:

1. Se asegura de estar conectado a WiFi (reintenta si se cayó).
2. Sincroniza el reloj por NTP (una vez al boot) — el timestamp que manda el nodo es el que el backend persiste tal cual, así que un reloj desincronizado ensucia el historial.
3. Lee DHT22 (temperatura, humedad ambiente) y el sensor de humedad de suelo.
4. Envía cada lectura por **MQTT** (si `MQTT_BROKER_URL` está configurado) o por **HTTP** como respaldo.
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
   Editar `include/config.h`: SSID/contraseña de WiFi, URL del backend, `IOT_API_KEY` (respaldo HTTP), y los códigos de sensor (deben existir ya dados de alta en el backend).
   
   **Opcional — MQTT (canal preferido):** completar también `MQTT_BROKER_URL`, `MQTT_USUARIO` y `MQTT_PASSWORD`. Las credenciales se generan en la Web (Telemetría → sensor → *Credencial MQTT*). Ver [`docs/IOT-MQTT-FIRMWARE.md`](docs/IOT-MQTT-FIRMWARE.md) para el paso a paso completo.

3. Calibrar el sensor de humedad de suelo (varía por unidad): sumergido en agua anotar el valor de `ADC_HUMEDAD_SUELO_EMPAPADO`, seco al aire anotar `ADC_HUMEDAD_SUELO_SECO`, y cargar ambos en `config.h`. Se puede leer el valor crudo por el monitor serie antes de calibrar.
4. Compilar y flashear:
   ```bash
   pio run --target upload
   pio device monitor
   ```

## Probar sin hardware (simulador)

Mientras no hay un ESP32 físico a mano, `simulador/simulador.py` manda el mismo
contrato que usará el firmware real, tanto por HTTP como por MQTT, para poder
probar todo el flujo (ingesta → umbrales → alertas → dashboard) de punta a punta:

```bash
cd simulador
pip install -r requirements.txt
cp .env.example .env   # completar BACKEND_URL, IOT_API_KEY y/o MQTT_*

python simulador.py                     # loop continuo (MQTT si MQTT_URL está en .env, HTTP si no)
python simulador.py --once              # una sola tanda y termina
python simulador.py --anomalo           # fuerza temperatura fuera de rango físico:
                                        # el backend responde 201 igual (HU-19),
                                        # pero aparta el dato a lecturas_anomalas
                                        # en vez de sumarlo a los promedios
python simulador.py --transport mqtt    # fuerza MQTT aunque no haya MQTT_URL
python simulador.py --transport http    # fuerza HTTP aunque haya MQTT_URL
```

El simulador soporta todos los tipos de sensor del backend (temperatura, humedad
ambiente, humedad suelo, pluviómetro, radiación solar, viento). Los opcionales se
activan poniendo su `CODIGO_SENSOR_*` en `.env`.


## Contribuir

Ver [`CONTRIBUTING.md`](CONTRIBUTING.md) para la política de branching (Gitflow) del proyecto.
