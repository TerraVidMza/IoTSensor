#!/usr/bin/env python3
"""Simulador de nodo IoT TerraVid.

Reemplaza al ESP32 mientras no hay hardware disponible: genera lecturas
realistas de los sensores configurados con un random walk (para que se vean
como series de tiempo creíbles, no ruido puro), y las envía al backend con
el mismo contrato que usará el firmware real.

Transportes soportados:

  HTTP  → POST /api/lecturas-iot con header X-Api-Key (HU-19).
           Es el canal de respaldo del firmware real.
  MQTT  → Publica en terravid/sensores/{codigo}/lecturas (D-026).
           Canal preferido cuando el broker Mosquitto está disponible.

Uso:
    pip install -r requirements.txt
    cp .env.example .env   # completar BACKEND_URL, IOT_API_KEY y/o MQTT_*

    python simulador.py                       # loop continuo, HTTP por defecto
    python simulador.py --once                # una sola tanda y termina
    python simulador.py --anomalo             # fuerza temperatura fuera de rango
    python simulador.py --transport mqtt      # publica por MQTT (requiere MQTT_*)
    python simulador.py --transport http      # fuerza HTTP aunque haya MQTT_URL
    python simulador.py --transport mqtt --once --anomalo   # combinables
"""

import argparse
import os
import random
import sys
import time
from datetime import datetime, timezone
from typing import Optional

import requests
from dotenv import load_dotenv

load_dotenv()

# ---------------------------------------------------------------------------
# Configuración desde el entorno
# ---------------------------------------------------------------------------

BACKEND_URL = os.environ.get("BACKEND_URL", "").rstrip("/")
IOT_API_KEY = os.environ.get("IOT_API_KEY", "")

MQTT_URL = os.environ.get("MQTT_URL", "")
MQTT_CA_CERT = os.environ.get("MQTT_CA_CERT", "")  # ruta al PEM de la CA (opcional)

INTERVALO_SEGUNDOS = int(os.environ.get("INTERVALO_SEGUNDOS", "30"))

# ---------------------------------------------------------------------------
# Sensores configurados
# ---------------------------------------------------------------------------
# Cada sensor en la lista define:
#   codigo     → debe coincidir con un sensor dado de alta en el backend
#   tipo       → uno de los tipos de sensor del backend (SensorTipo enum)
#   estado     → valor inicial del random walk
#   paso_max   → maxima variacion por ciclo
#   minimo/maximo → rango del walk (fisicamente posible segun rangos-fisicos.ts)
#   unidad     → unidad que espera el backend
#   mqtt_user  → usuario MQTT del sensor (su codigo, por variable de entorno)
#   mqtt_pass  → password MQTT (por variable de entorno, generada por la Web)
#
# Los codigos y credenciales se leen del .env para no hardcodearlos.

SENSORES = [
    {
        "codigo": os.environ.get("CODIGO_SENSOR_TEMPERATURA", "NODO01-TEMP"),
        "tipo": "temperatura",
        "estado": 24.0,
        "paso_max": 0.8,
        "minimo": -15.0,
        "maximo": 60.0,
        "unidad": "°C",
        "mqtt_user": os.environ.get("MQTT_USUARIO_TEMPERATURA", ""),
        "mqtt_pass": os.environ.get("MQTT_PASSWORD_TEMPERATURA", ""),
    },
    {
        "codigo": os.environ.get("CODIGO_SENSOR_HUMEDAD_AMBIENTE", "NODO01-HUM-AMB"),
        "tipo": "humedad_ambiente",
        "estado": 45.0,
        "paso_max": 2.0,
        "minimo": 0.0,
        "maximo": 100.0,
        "unidad": "%",
        "mqtt_user": os.environ.get("MQTT_USUARIO_HUMEDAD_AMBIENTE", ""),
        "mqtt_pass": os.environ.get("MQTT_PASSWORD_HUMEDAD_AMBIENTE", ""),
    },
    {
        "codigo": os.environ.get("CODIGO_SENSOR_HUMEDAD_SUELO", "NODO01-HUM-SUELO"),
        "tipo": "humedad_suelo",
        "estado": 35.0,
        "paso_max": 1.5,
        "minimo": 0.0,
        "maximo": 100.0,
        "unidad": "%",
        "mqtt_user": os.environ.get("MQTT_USUARIO_HUMEDAD_SUELO", ""),
        "mqtt_pass": os.environ.get("MQTT_PASSWORD_HUMEDAD_SUELO", ""),
    },
    # --- Sensores opcionales (solo activos si tienen codigo configurado) ---
    {
        "codigo": os.environ.get("CODIGO_SENSOR_PLUVIOMETRO", ""),
        "tipo": "pluviometro",
        "estado": 0.0,
        "paso_max": 2.0,
        "minimo": 0.0,
        "maximo": 500.0,
        "unidad": "mm",
        "mqtt_user": os.environ.get("MQTT_USUARIO_PLUVIOMETRO", ""),
        "mqtt_pass": os.environ.get("MQTT_PASSWORD_PLUVIOMETRO", ""),
    },
    {
        "codigo": os.environ.get("CODIGO_SENSOR_RADIACION_SOLAR", ""),
        "tipo": "radiacion_solar",
        "estado": 600.0,
        "paso_max": 30.0,
        "minimo": 0.0,
        "maximo": 1500.0,
        "unidad": "W/m2",
        "mqtt_user": os.environ.get("MQTT_USUARIO_RADIACION_SOLAR", ""),
        "mqtt_pass": os.environ.get("MQTT_PASSWORD_RADIACION_SOLAR", ""),
    },
    {
        "codigo": os.environ.get("CODIGO_SENSOR_VIENTO", ""),
        "tipo": "viento",
        "estado": 15.0,
        "paso_max": 5.0,
        "minimo": 0.0,
        "maximo": 200.0,
        "unidad": "km/h",
        "mqtt_user": os.environ.get("MQTT_USUARIO_VIENTO", ""),
        "mqtt_pass": os.environ.get("MQTT_PASSWORD_VIENTO", ""),
    },
]

# Filtra los sensores sin codigo (no configurados)
SENSORES_ACTIVOS = [s for s in SENSORES if s["codigo"]]


# ---------------------------------------------------------------------------
# Utilidades
# ---------------------------------------------------------------------------

def caminar(valor: float, paso_maximo: float, minimo: float, maximo: float) -> float:
    nuevo = valor + random.uniform(-paso_maximo, paso_maximo)
    return max(minimo, min(maximo, nuevo))


def timestamp_iso8601_utc() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


# ---------------------------------------------------------------------------
# Transporte HTTP
# ---------------------------------------------------------------------------

def enviar_por_http(codigo_sensor: str, valor: float, unidad: str) -> bool:
    if not BACKEND_URL or not IOT_API_KEY:
        print("Falta BACKEND_URL o IOT_API_KEY. Copiá .env.example a .env y completalo.")
        sys.exit(1)

    payload = {
        "codigoSensor": codigo_sensor,
        "valor": round(valor, 2),
        "unidad": unidad,
        "sensorTimestamp": timestamp_iso8601_utc(),
    }
    headers = {"X-Api-Key": IOT_API_KEY}

    try:
        respuesta = requests.post(
            f"{BACKEND_URL}/api/lecturas-iot",
            json=payload,
            headers=headers,
            timeout=10,
        )
    except requests.RequestException as error:
        print(f"  [{codigo_sensor}] Error de red HTTP: {error}")
        return False

    ok = respuesta.status_code == 201
    marca = "OK" if ok else "!!"
    print(f"[HTTP {marca}] {codigo_sensor} = {payload['valor']} {unidad} -> HTTP {respuesta.status_code}")
    if not ok:
        print(f"     Respuesta: {respuesta.text}")
    return ok


# ---------------------------------------------------------------------------
# Transporte MQTT
# ---------------------------------------------------------------------------

def crear_cliente_mqtt(mqtt_url: str, usuario: str, password: str, ca_cert: str):
    """Crea y conecta un cliente paho-mqtt al broker.

    Soporta mqtt:// (sin TLS) y mqtts:// (TLS).
    Devuelve el cliente conectado o None si falla.
    """
    try:
        import paho.mqtt.client as mqtt
    except ImportError:
        print("paho-mqtt no instalado. Ejecutá: pip install paho-mqtt")
        return None

    tls = mqtt_url.startswith("mqtts://")
    host_port = mqtt_url.removeprefix("mqtts://").removeprefix("mqtt://")
    if ":" in host_port:
        host, puerto_str = host_port.rsplit(":", 1)
        puerto = int(puerto_str)
    else:
        host = host_port
        puerto = 8883 if tls else 1883

    # MQTT 5 y no 3.1.1: en 3.1.1 el broker confirma (PUBACK) una publicación
    # que la ACL rechazó y la descarta en silencio, y el simulador la contaba
    # como enviada. En MQTT 5 el PUBACK trae el motivo ("Not authorized").
    client = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        client_id=f"simulador-{usuario}-{random.randint(0, 0xFFFF):04x}",
        protocol=mqtt.MQTTv5,
    )
    client.username_pw_set(usuario, password)
    client.user_data_set({"puback": {}, "conectado": None})

    def al_conectar(cli, datos, flags, reason_code, properties):
        datos["conectado"] = reason_code

    def al_publicar(cli, datos, mid, reason_code, properties):
        datos["puback"][mid] = reason_code

    client.on_connect = al_conectar
    client.on_publish = al_publicar

    if tls:
        import ssl
        ctx = ssl.create_default_context()
        if ca_cert:
            ctx.load_verify_locations(ca_cert)
        else:
            # Sin CA explícita: confía en las CA del sistema (Let's Encrypt funciona)
            ctx.load_default_certs()
        client.tls_set_context(ctx)

    try:
        client.connect(host, puerto, keepalive=60)
        client.loop_start()
    except Exception as error:
        print(f"No se pudo conectar al broker MQTT {host}:{puerto}: {error}")
        return None

    # connect() vuelve antes de que el broker acepte o rechace las credenciales.
    datos = client.user_data_get()
    for _ in range(50):
        if datos["conectado"] is not None:
            break
        time.sleep(0.1)
    if datos["conectado"] is None or datos["conectado"].is_failure:
        print(f"  [{usuario}] El broker rechazó la conexión: {datos['conectado']}")
        client.loop_stop()
        return None
    return client


# Pool de clientes MQTT: uno por sensor (cada sensor tiene sus propias credenciales)
_mqtt_clients: dict = {}


def obtener_cliente_mqtt(sensor: dict) -> Optional[object]:
    """Devuelve (y cachea) el cliente MQTT para el sensor dado."""
    codigo = sensor["codigo"]
    if codigo in _mqtt_clients:
        return _mqtt_clients[codigo]

    usuario = sensor.get("mqtt_user") or codigo
    password = sensor.get("mqtt_pass") or ""

    if not MQTT_URL:
        return None

    if not password:
        print(f"  [{codigo}] Sin MQTT_PASSWORD configurado — ver .env.example para MQTT_{codigo.upper().replace('-', '_')}_*")
        return None

    client = crear_cliente_mqtt(MQTT_URL, usuario, password, MQTT_CA_CERT)
    if client:
        _mqtt_clients[codigo] = client
    return client


def enviar_por_mqtt(sensor: dict, valor: float, unidad: str) -> bool:
    """Publica una lectura en el tópico del sensor (QoS 1).

    El codigoSensor NO va en el payload MQTT — sale del tópico, que es
    la identidad autenticada por el broker (D-026).
    """
    import json as _json

    client = obtener_cliente_mqtt(sensor)
    if not client:
        return False

    codigo = sensor["codigo"]
    payload = {
        "valor": round(valor, 2),
        "unidad": unidad,
        "sensorTimestamp": timestamp_iso8601_utc(),
    }
    topico = f"terravid/sensores/{codigo}/lecturas"

    try:
        result = client.publish(topico, _json.dumps(payload), qos=1)
        result.wait_for_publish(timeout=5)
    except Exception as error:
        print(f"  [{codigo}] Error al publicar por MQTT: {error}")
        return False

    motivo = client.user_data_get()["puback"].pop(result.mid, None)
    ok = result.rc == 0 and motivo is not None and not motivo.is_failure
    marca = "OK" if ok else "!!"
    print(f"[MQTT {marca}] {codigo} = {payload['valor']} {unidad} -> {motivo or 'sin confirmación'}")
    return ok


# ---------------------------------------------------------------------------
# Envío genérico (elige transporte)
# ---------------------------------------------------------------------------

def enviar_lectura(sensor: dict, valor: float, transporte: str) -> None:
    codigo = sensor["codigo"]
    unidad = sensor["unidad"]

    if transporte == "mqtt":
        if not enviar_por_mqtt(sensor, valor, unidad):
            print(f"  [{codigo}] Reintentando por HTTP...")
            enviar_por_http(codigo, valor, unidad)
    else:
        enviar_por_http(codigo, valor, unidad)


# ---------------------------------------------------------------------------
# Tanda de lecturas
# ---------------------------------------------------------------------------

def tanda_de_lecturas(transporte: str, forzar_anomalo: bool) -> None:
    for sensor in SENSORES_ACTIVOS:
        sensor["estado"] = caminar(
            sensor["estado"], sensor["paso_max"], sensor["minimo"], sensor["maximo"]
        )

    for sensor in SENSORES_ACTIVOS:
        valor = sensor["estado"]

        if forzar_anomalo and sensor["tipo"] == "temperatura":
            # Fuera del rango físico declarado en rangos-fisicos.ts (-15 a 60 °C):
            # demuestra que el backend lo aparta a lecturas_anomalas en vez de
            # contaminar los promedios (HU-19).
            valor = 999.0

        enviar_lectura(sensor, valor, transporte)


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--once", action="store_true", help="Envía una sola tanda de lecturas y termina.")
    parser.add_argument(
        "--anomalo",
        action="store_true",
        help="Fuerza temperatura fuera de rango físico. El backend responde 201 igual, "
             "pero aparta el dato a lecturas_anomalas (HU-19).",
    )
    parser.add_argument(
        "--transport",
        choices=["http", "mqtt"],
        default="mqtt" if MQTT_URL else "http",
        help="Transporte de telemetría. Por defecto: mqtt si MQTT_URL está en .env, http si no.",
    )
    args = parser.parse_args()

    print(f"Simulador TerraVid IoT  •  transporte={args.transport}")
    if args.transport == "http":
        print(f"  → {BACKEND_URL}/api/lecturas-iot")
    else:
        print(f"  → {MQTT_URL or '(MQTT_URL no configurada en .env)'}")

    print(f"  Sensores activos: {', '.join(s['codigo'] for s in SENSORES_ACTIVOS)}")

    if args.once:
        tanda_de_lecturas(args.transport, args.anomalo)
        return

    print(f"Enviando una tanda cada {INTERVALO_SEGUNDOS}s. Ctrl+C para detener.")
    try:
        while True:
            tanda_de_lecturas(args.transport, args.anomalo)
            time.sleep(INTERVALO_SEGUNDOS)
    except KeyboardInterrupt:
        print("\nSimulador detenido.")
    finally:
        # Cierra los clientes MQTT ordenadamente
        for client in _mqtt_clients.values():
            try:
                client.loop_stop()
                client.disconnect()
            except Exception:
                pass


if __name__ == "__main__":
    main()
