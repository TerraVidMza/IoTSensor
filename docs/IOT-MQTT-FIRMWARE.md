# TerraVid IoTSensor — Guía MQTT (D-026)

Cómo configurar y usar el canal MQTT en el firmware del ESP32 y en el simulador.
El Backend documenta el **contrato de mensajes** en
[`docs/IOT-MQTT.md`](../../Backend/docs/IOT-MQTT.md); esta guía se enfoca en
**cómo aprovisionar el dispositivo**.

## Cuándo usar MQTT vs HTTP

| Canal | Cuándo | Ventajas |
|---|---|---|
| **MQTT** (preferido) | Producción con broker disponible | Credenciales por sensor; un nodo comprometido no puede hacerse pasar por otro |
| **HTTP** | Desarrollo local, fallback automático | Más simple; no requiere broker |

El firmware **auto-detecta** el transporte: si `MQTT_BROKER_URL` está definido
en `config.h`, usa MQTT y cae a HTTP solo si el broker no responde.

## Paso a paso — firmware ESP32

### 1. Crear el sensor en el backend

El Propietario o Agrónomo da de alta el sensor en **Telemetría → Sensores →
Nuevo sensor**. El código del sensor (ej. `NODO01-TEMP`) es el que se carga
en `config.h` — debe coincidir exactamente (case-sensitive).

### 2. Generar la credencial MQTT

En **Telemetría → sensor → Credencial MQTT** hacer clic en *Generar*. El
backend crea un usuario en Mosquitto con permisos solo para publicar en el
tópico de ese sensor. La contraseña se muestra **una única vez** — anotarla
antes de cerrar la pantalla.

> ⚠️ Cada llamada a "Generar" regenera la credencial: la anterior deja de
> funcionar. Si se pierde la contraseña, hay que generar una nueva y
> reflashear el dispositivo.

### 3. Copiar `config.example.h` a `config.h` y completar

```bash
cp include/config.example.h include/config.h
```

Campos MQTT a completar (además de WiFi, backend URL y códigos de sensor):

```c
// URL del broker. En producción, con TLS:
#define MQTT_BROKER_URL "mqtts://terravid-api.duckdns.org:8883"
// En desarrollo local (sin TLS):
// #define MQTT_BROKER_URL "mqtt://192.168.1.100:1883"

// Usuario MQTT = código del sensor (uno por variable física).
// El broker solo autoriza a cada usuario a publicar en su propio tópico.
#define MQTT_USUARIO_TEMPERATURA    "NODO01-TEMP"
#define MQTT_PASSWORD_TEMPERATURA   "<password-generada>"

#define MQTT_USUARIO_HUMEDAD_AMB    "NODO01-HUM-AMB"
#define MQTT_PASSWORD_HUMEDAD_AMB   "<password-generada>"

#define MQTT_USUARIO_HUMEDAD_SUELO  "NODO01-HUM-SUELO"
#define MQTT_PASSWORD_HUMEDAD_SUELO "<password-generada>"
```

> **Un cliente MQTT por sensor.** El broker Mosquitto usa dynamic security
> (D-026): cada sensor tiene un rol propio que solo permite publicar en
> `terravid/sensores/{codigo}/lecturas`. El firmware abre **3 conexiones MQTT
> simultáneas** (una por variable). Si se usara una sola credencial para los
> 3 tópicos, el broker rechazaría los dos que no coinciden con el usuario
> autenticado.

### 4. Compilar y flashear

```bash
pio run --target upload
pio device monitor
```

El monitor serie muestra qué transporte está activo:

```
TerraVid IoT — arrancando nodo de telemetria
Transporte: MQTT (con HTTP como respaldo)
MQTT TLS -> terravid-api.duckdns.org:8883
Sincronizando reloj (NTP)... listo.
[MQTT] [2026-10-05T15:00:00Z] NODO01-TEMP = 23.40 °C -> publicado
[MQTT] [2026-10-05T15:00:01Z] NODO01-HUM-AMB = 48.20 % -> publicado
[MQTT] [2026-10-05T15:00:02Z] NODO01-HUM-SUELO = 34.10 % -> publicado
```

### 5. Verificar en el backend

La Web muestra la última lectura de cada sensor en **Telemetría → panel de
sensores**. Si el badge "Sin lecturas" persiste más de 1 minuto tras flashear,
revisar:

1. El monitor serie: ¿dice "publicado" o "fallo (cae a HTTP)"?
2. Si falla MQTT, ¿el fallback HTTP también falla? El HTTP es más diagnóstico
   (devuelve 404, 403 o 400 con mensaje).
3. Verificar que `MQTT_BROKER_URL` y cada par `MQTT_USUARIO_*`/`MQTT_PASSWORD_*`
   coinciden con lo que muestra la Web.


## Paso a paso — simulador Python

```bash
cd simulador
pip install -r requirements.txt
cp .env.example .env
```

Completar en `.env`:

```env
MQTT_URL=mqtts://terravid-api.duckdns.org:8883

CODIGO_SENSOR_TEMPERATURA=NODO01-TEMP
MQTT_USUARIO_TEMPERATURA=NODO01-TEMP
MQTT_PASSWORD_TEMPERATURA=<password-generada>

CODIGO_SENSOR_HUMEDAD_AMBIENTE=NODO01-HUM-AMB
MQTT_USUARIO_HUMEDAD_AMBIENTE=NODO01-HUM-AMB
MQTT_PASSWORD_HUMEDAD_AMBIENTE=<password-generada>

CODIGO_SENSOR_HUMEDAD_SUELO=NODO01-HUM-SUELO
MQTT_USUARIO_HUMEDAD_SUELO=NODO01-HUM-SUELO
MQTT_PASSWORD_HUMEDAD_SUELO=<password-generada>
```

```bash
python simulador.py                     # MQTT si MQTT_URL está en .env
python simulador.py --transport http    # fuerza HTTP
python simulador.py --transport mqtt --once   # una tanda por MQTT
```

## Nota sobre TLS en desarrollo local

Para el broker local (`mqtt://`) no hace falta TLS. Para producción
(`mqtts://`), el certificado de Let's Encrypt se valida contra las CA raíz del
sistema en el simulador, y contra `setInsecure()` en el ESP32 para simplificar
el aprovisionamiento.

**Para validación estricta en el ESP32** (recomendado en producción) se puede
poner el PEM de ISRG Root X1 en una variable de `config.h` y pasarlo a
`WiFiClientSecure::setCACert()`. La ventaja es que detecta si el certificado
del broker fue reemplazado; la desventaja es que hay que actualizar el firmware
cuando Let's Encrypt cambie de CA raíz.
