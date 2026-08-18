# TerraVid — IoTSensor

Firmware de los nodos de telemetría del ERP agrícola TerraVid: sensores de tensión hídrica y temperatura/humedad de suelo montados en los cuarteles, que envían lecturas al [`Backend`](https://github.com/TerraVidMza/Backend) para monitoreo y alertas.

> **Estado:** este repositorio todavía no tiene código — es la fase de bootstrap (Gitflow + documentación). El stack listado abajo es el planificado según [`docs/ARCHITECTURE.md`](https://github.com/TerraVidMza/Backend/blob/develop/docs/ARCHITECTURE.md) del repo Backend, sujeto a confirmarse cuando arranque el desarrollo de firmware.

## Repositorios del proyecto

| Repo | Contenido |
|---|---|
| [`Backend`](https://github.com/TerraVidMza/Backend) | API REST — NestJS + TypeORM + PostgreSQL/PostGIS |
| [`Web`](https://github.com/TerraVidMza/Web) | Frontend web de oficina |
| [`Mobile`](https://github.com/TerraVidMza/Mobile) | App móvil offline-first para campo |
| [`IoTSensor`](https://github.com/TerraVidMza/IoTSensor) (este repo) | Firmware de los nodos de telemetría (ESP32) |

> Cómo se comunican estos 4 componentes entre sí está detallado en [`docs/ARCHITECTURE.md` del repo Backend](https://github.com/TerraVidMza/Backend/blob/develop/docs/ARCHITECTURE.md).

## Stack planificado

- **C / C++ (C++11+)**
- Microcontrolador **ESP32 WROOM/S3**, bajo consumo, alimentación solar (5V) + batería LiPo, gabinete estanco IP65
- Sensores: **DHT22** (temperatura/humedad) y sensor de humedad de suelo capacitivo
- Transmisión vía **MQTT/MQTTS**, autenticando con una `API Key` estática, directo al endpoint de ingesta del Backend

> **Decisión pendiente de validar:** la versión de MQTT (v5.0 vs v3.1.1) depende de si el firmware se implementa sobre **ESP-IDF nativo** (soporta v5.0) o **Arduino Framework** con librerías como PubSubClient (solo v3.1.1). Ver la nota correspondiente en [`docs/ARCHITECTURE.md` del repo Backend](https://github.com/TerraVidMza/Backend/blob/develop/docs/ARCHITECTURE.md) — debe confirmarse al elegir el framework de firmware.

## Contribuir

Ver [`CONTRIBUTING.md`](CONTRIBUTING.md) para la política de branching (Gitflow) del proyecto.
