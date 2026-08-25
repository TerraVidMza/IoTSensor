#!/usr/bin/env python3
"""Simulador de nodo IoT TerraVid.

Reemplaza al ESP32 mientras no hay hardware disponible: genera lecturas
realistas de temperatura, humedad ambiente y humedad de suelo con un random
walk (para que se vean como series de tiempo creíbles, no ruido puro), y las
envía al mismo endpoint y con el mismo contrato que usará el firmware real
(POST /api/lecturas-iot, header X-Api-Key) — ver
Backend/src/modules/lecturas-iot/dto/ingest-lectura.dto.ts.

Uso:
    pip install -r requirements.txt
    cp .env.example .env   # completar BACKEND_URL e IOT_API_KEY
    python simulador.py               # loop continuo
    python simulador.py --once        # una sola tanda de lecturas
    python simulador.py --anomalo     # fuerza un valor fuera de rango fisico.
                                       # El backend igual responde 201 (el nodo
                                       # no tiene por que enterarse), pero
                                       # aparta el dato a lecturas_anomalas en
                                       # vez de sumarlo a los promedios (HU-19)
"""

import argparse
import os
import random
import sys
import time
from datetime import datetime, timezone

import requests
from dotenv import load_dotenv

load_dotenv()

BACKEND_URL = os.environ.get("BACKEND_URL", "").rstrip("/")
IOT_API_KEY = os.environ.get("IOT_API_KEY", "")
CODIGO_TEMPERATURA = os.environ.get("CODIGO_SENSOR_TEMPERATURA", "NODO01-TEMP")
CODIGO_HUMEDAD_AMBIENTE = os.environ.get("CODIGO_SENSOR_HUMEDAD_AMBIENTE", "NODO01-HUM-AMB")
CODIGO_HUMEDAD_SUELO = os.environ.get("CODIGO_SENSOR_HUMEDAD_SUELO", "NODO01-HUM-SUELO")
INTERVALO_SEGUNDOS = int(os.environ.get("INTERVALO_SEGUNDOS", "30"))

# Estado del random walk: arranca en valores tipicos de un dia de verano
# mendocino, y cada tanda se mueve un poco desde el valor anterior.
estado = {
    "temperatura": 24.0,
    "humedad_ambiente": 45.0,
    "humedad_suelo": 35.0,
}


def caminar(valor: float, paso_maximo: float, minimo: float, maximo: float) -> float:
    nuevo = valor + random.uniform(-paso_maximo, paso_maximo)
    return max(minimo, min(maximo, nuevo))


def timestamp_iso8601_utc() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def enviar_lectura(codigo_sensor: str, valor: float, unidad: str) -> None:
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
        print(f"[{codigo_sensor}] Error de red: {error}")
        return

    marca = "OK" if respuesta.status_code == 201 else "!!"
    print(f"[{marca}] {codigo_sensor} = {payload['valor']} {unidad} -> HTTP {respuesta.status_code}")
    if respuesta.status_code != 201:
        print(f"     Respuesta: {respuesta.text}")


def tanda_de_lecturas(forzar_anomalo: bool) -> None:
    estado["temperatura"] = caminar(estado["temperatura"], 0.8, -5, 42)
    estado["humedad_ambiente"] = caminar(estado["humedad_ambiente"], 2.0, 10, 95)
    estado["humedad_suelo"] = caminar(estado["humedad_suelo"], 1.5, 5, 90)

    temperatura = estado["temperatura"]
    if forzar_anomalo:
        # Fuera del rango fisico declarado en rangos-fisicos.ts (-15 a 60 °C):
        # sirve para demostrar en vivo que el backend lo aparta a
        # lecturas_anomalas en vez de contaminar los promedios.
        temperatura = 999.0

    enviar_lectura(CODIGO_TEMPERATURA, temperatura, "°C")
    enviar_lectura(CODIGO_HUMEDAD_AMBIENTE, estado["humedad_ambiente"], "%")
    enviar_lectura(CODIGO_HUMEDAD_SUELO, estado["humedad_suelo"], "%")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--once", action="store_true", help="Envía una sola tanda de lecturas y termina.")
    parser.add_argument(
        "--anomalo",
        action="store_true",
        help="Fuerza un valor de temperatura fuera de rango físico. El backend responde 201 igual, pero aparta el dato a lecturas_anomalas en vez de sumarlo a los promedios.",
    )
    args = parser.parse_args()

    print(f"Simulador TerraVid IoT -> {BACKEND_URL}/api/lecturas-iot")

    if args.once:
        tanda_de_lecturas(args.anomalo)
        return

    print(f"Enviando una tanda cada {INTERVALO_SEGUNDOS}s. Ctrl+C para detener.")
    try:
        while True:
            tanda_de_lecturas(args.anomalo)
            time.sleep(INTERVALO_SEGUNDOS)
    except KeyboardInterrupt:
        print("\nSimulador detenido.")


if __name__ == "__main__":
    main()
