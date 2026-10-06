# Guía Completa de Configuración: Nodo IoT TerraVid (ESP32)

Esta guía detalla el proceso completo paso a paso para ensamblar, configurar, calibrar y poner en marcha un nodo físico de telemetría IoT para el sistema TerraVid.

---

## 1. Requisitos Previos

### Hardware Necesario
* **Microcontrolador:** Placa de desarrollo ESP32 (preferentemente modelo WROOM-32 o S3).
* **Sensor de Clima:** Módulo DHT22 (mide temperatura y humedad ambiente).
* **Sensor de Suelo:** Sensor de humedad de suelo capacitivo analógico (evita los resistivos porque se corroen rápidamente).
* **Conectividad:** Cables Dupont (M-H y H-H), protoboard (opcional para pruebas) y un cable micro-USB o USB-C (con capacidad de transmisión de datos, no solo carga) para conectar el ESP32 a la PC.

### Software Necesario
* **Visual Studio Code (VS Code):** Descárgalo e instálalo desde [code.visualstudio.com](https://code.visualstudio.com/).
* **Extensión PlatformIO:** Dentro de VS Code, ve a la pestaña de extensiones, busca "PlatformIO IDE" e instálalo.
* **Drivers USB a UART:** Dependiendo de tu ESP32, es posible que necesites instalar los drivers del chip conversor (CP210x o CH340). Si al conectar la placa tu PC no reconoce un nuevo puerto COM, instala el driver correspondiente.

---

## 2. Cableado (Wiring) del Hardware

Asegúrate de que el ESP32 **no esté conectado a la corriente** mientras realizas el cableado.

### Sensor DHT22
| Pin del Sensor DHT22 | Conexión al ESP32 | Notas |
| :--- | :--- | :--- |
| **VCC / +** | `3.3V` | El DHT22 soporta 3.3V, ideal para el ESP32. |
| **GND / -** | `GND` | Tierra. |
| **DATA / OUT** | `GPIO 4` | Pin de datos configurado por defecto. |
> [!NOTE] 
> Si tu DHT22 es el chip suelto (sin PCB soldado), necesitarás colocar una resistencia pull-up de 10kΩ entre los pines VCC y DATA. Si es un módulo (montado en una plaquita), la resistencia ya viene integrada.

### Sensor de Humedad de Suelo Capacitivo
| Pin del Sensor Capacitivo | Conexión al ESP32 | Notas |
| :--- | :--- | :--- |
| **VCC / +** | `3.3V` | Alimentación. |
| **GND / -** | `GND` | Tierra. |
| **AOUT / Analog** | `GPIO 34` | Pin de lectura analógica (ADC). El pin 34 es de solo entrada (*input-only*). |

---

## 3. Alta y Aprovisionamiento en el Backend

Antes de encender el dispositivo, el backend debe estar preparado para recibir sus datos.

1. **Crear los Sensores Físicos:**
   * Ingresa a la plataforma Web de TerraVid con un usuario de rol **Propietario** o **Agrónomo**.
   * Ve al módulo **Telemetría** > **Sensores** > **Nuevo sensor**.
   * Crea tres sensores separados (uno por cada variable física). Asígnales códigos claros, por ejemplo:
     * `NODO01-TEMP` (Tipo: Temperatura)
     * `NODO01-HUM-AMB` (Tipo: Humedad Ambiente)
     * `NODO01-HUM-SUELO` (Tipo: Humedad de Suelo)

2. **Generar Credenciales MQTT (D-026):**
   * En la misma pantalla de Telemetría, entra al detalle de cada sensor recién creado.
   * Busca la sección **Credencial MQTT** y haz clic en **Generar**.
   * **Copia inmediatamente la contraseña generada**, ya que por seguridad el sistema solo la muestra una vez. 
   * Repite este proceso para los 3 sensores. Terminarás con 3 usuarios (los códigos del sensor) y 3 contraseñas distintas.

---

## 4. Configuración del Firmware

Ahora prepararemos el código que se ejecutará en la placa.

1. **Abrir el Proyecto:** 
   Abre la carpeta del repositorio `IoTSensor` utilizando VS Code (PlatformIO lo reconocerá automáticamente).
2. **Crear el archivo de configuración:**
   En el panel explorador, ve a la carpeta `include/`, haz una copia del archivo `config.example.h` y renómbrala a `config.h`.
3. **Completar datos de Red y Backend:**
   Abre `config.h` y ajusta los siguientes valores:
   ```cpp
   // Tus credenciales de la red WiFi 2.4GHz (el ESP32 no soporta 5GHz)
   #define WIFI_SSID "Tu_Red_WiFi"
   #define WIFI_PASSWORD "Tu_Password"

   // URL de la API de TerraVid
   #define BACKEND_URL "https://tu-dominio-terravid.com"
   #define IOT_API_KEY "TU_API_KEY_DE_RESPALDO"
   ```
4. **Completar Credenciales MQTT:**
   ```cpp
   #define MQTT_BROKER_URL "mqtts://tu-dominio-terravid.com:8883" // Usa mqtt:// y puerto 1883 si pruebas en local sin TLS

   // Reemplaza con los códigos y contraseñas que obtuviste en el Paso 3
   #define MQTT_USUARIO_TEMPERATURA    "NODO01-TEMP"
   #define MQTT_PASSWORD_TEMPERATURA   "password_generada_1"

   #define MQTT_USUARIO_HUMEDAD_AMB    "NODO01-HUM-AMB"
   #define MQTT_PASSWORD_HUMEDAD_AMB   "password_generada_2"

   #define MQTT_USUARIO_HUMEDAD_SUELO  "NODO01-HUM-SUELO"
   #define MQTT_PASSWORD_HUMEDAD_SUELO "password_generada_3"
   
   // Los códigos de los sensores
   #define CODIGO_SENSOR_TEMPERATURA "NODO01-TEMP"
   #define CODIGO_SENSOR_HUMEDAD_AMBIENTE "NODO01-HUM-AMB"
   #define CODIGO_SENSOR_HUMEDAD_SUELO "NODO01-HUM-SUELO"
   ```

---

## 5. Calibración del Sensor de Suelo

Los sensores capacitivos de suelo tienen variaciones de fábrica, por lo que es vital calibrarlos.

1. En el archivo `config.h`, localiza estas líneas:
   ```cpp
   #define ADC_HUMEDAD_SUELO_SECO 3200
   #define ADC_HUMEDAD_SUELO_EMPAPADO 1200
   ```
2. **Temporalmente**, puedes flashear la placa (ver Paso 6) para abrir el monitor serie y ver qué valor devuelve la lectura cruda.
3. **Calibrar el límite seco:** Seca completamente la sonda con un paño y déjala al aire libre. Anota el valor que muestra el monitor y colócalo en `ADC_HUMEDAD_SUELO_SECO`.
4. **Calibrar el límite húmedo:** Sumerge la parte medidora de la sonda en un vaso con agua (no mojes el circuito superior). Anota el valor que muestra el monitor y colócalo en `ADC_HUMEDAD_SUELO_EMPAPADO`.

---

## 6. Compilación y Flasheo

Con todo configurado y conectado, subiremos el programa al ESP32.

1. Conecta el ESP32 a la PC mediante el cable USB.
2. En VS Code (PlatformIO), busca los íconos de acción en la barra azul inferior.
3. Haz clic en el ícono de **Upload** (una flecha apuntando a la derecha `→`).
4. PlatformIO descargará las librerías necesarias (DHT, ArduinoJson, PubSubClient), compilará el código fuente y lo grabará en el microcontrolador.
> [!TIP]
> Si en la terminal de PlatformIO aparece "Connecting..." y unos puntos suspensivos que no avanzan, mantén presionado el botón físico **BOOT** en la placa ESP32 por un par de segundos hasta que comience la subida.

---

## 7. Verificación Final

1. Una vez finalizada la subida, haz clic en el ícono del **Serial Monitor** (un enchufe 🔌) en la barra inferior de PlatformIO.
2. Asegúrate de que los baudios estén en `115200`. Deberías ver un log similar a este:
   ```text
   TerraVid IoT — arrancando nodo de telemetria
   Transporte: MQTT (con HTTP como respaldo)
   Conectando a WiFi "Tu_Red_WiFi"...
   WiFi conectado. IP: 192.168.1.50
   Sincronizando reloj (NTP)... listo.
   [MQTT] [2026-10-06T02:15:00Z] NODO01-TEMP = 24.50 °C -> publicado
   [MQTT] [2026-10-06T02:15:01Z] NODO01-HUM-AMB = 45.20 % -> publicado
   [MQTT] [2026-10-06T02:15:02Z] NODO01-HUM-SUELO = 38.00 % -> publicado
   ```
3. Finalmente, abre la plataforma Web de TerraVid y verifica en el Panel de Sensores que los datos estén llegando en tiempo real y perdiendo el badge de "Sin lecturas".

¡El nodo ya está listo para ser instalado en el campo de cultivo!
