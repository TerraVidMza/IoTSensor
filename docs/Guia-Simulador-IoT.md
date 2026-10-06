# Guía de Utilización: Simulador de Nodo IoT TerraVid

El simulador (`simulador.py`) es un script en Python que reemplaza físicamente a la placa ESP32. Su objetivo es generar lecturas realistas utilizando un *random walk* (para imitar series de tiempo naturales) y enviarlas al backend cumpliendo con el mismo contrato de red que usa el firmware real, tanto por MQTT como por HTTP.

Esto es ideal para probar toda la cadena de datos (Ingesta → Base de datos → Reglas de umbrales → Alertas → Dashboard) sin necesidad de tener el hardware ensamblado.

---

## 1. Requisitos Previos

Necesitarás tener instalado en tu computadora:
* **Python 3.8+**
* Un editor de código o terminal abierta en la carpeta `IoTSensor/simulador`

### Instalación de dependencias
1. Abre tu terminal y navega hasta el directorio del simulador:
   ```bash
   cd D:\Users\sergi\Code\TerraVid\IoTSensor\simulador
   ```
2. Instala las librerías necesarias (se utiliza `requests` para HTTP y `paho-mqtt` para MQTT):
   ```bash
   pip install -r requirements.txt
   ```

---

## 2. Configuración (.env)

El simulador utiliza un archivo de variables de entorno para manejar los secretos sin subirlos al repositorio.

1. Haz una copia del archivo de ejemplo:
   ```bash
   # En Windows (Powershell)
   Copy-Item .env.example .env
   ```
2. Abre el archivo `.env` resultante y completa los datos fundamentales:

   **Para el transporte HTTP:**
   ```env
   BACKEND_URL=https://tu-dominio-terravid.com
   IOT_API_KEY=tu_api_key_de_respaldo
   ```

   **Para el transporte MQTT (Recomendado):**
   ```env
   MQTT_URL=mqtts://tu-dominio-terravid.com:8883
   
   # Completa los códigos de tus sensores dados de alta en el backend
   # y las contraseñas que obtuviste al generar sus credenciales.
   CODIGO_SENSOR_TEMPERATURA=NODO01-TEMP
   MQTT_USUARIO_TEMPERATURA=NODO01-TEMP
   MQTT_PASSWORD_TEMPERATURA=password_1

   CODIGO_SENSOR_HUMEDAD_AMBIENTE=NODO01-HUM-AMB
   MQTT_USUARIO_HUMEDAD_AMBIENTE=NODO01-HUM-AMB
   MQTT_PASSWORD_HUMEDAD_AMBIENTE=password_2

   CODIGO_SENSOR_HUMEDAD_SUELO=NODO01-HUM-SUELO
   MQTT_USUARIO_HUMEDAD_SUELO=NODO01-HUM-SUELO
   MQTT_PASSWORD_HUMEDAD_SUELO=password_3
   ```
   > [!TIP]
   > También puedes descomentar y configurar sensores adicionales (como Pluviómetro, Radiación Solar y Viento) si los necesitas.

---

## 3. Comandos de Ejecución

El script cuenta con varias banderas útiles según lo que desees probar:

### Ejecución Básica
Si tienes configurado `MQTT_URL` en tu `.env`, por defecto usará MQTT. Mandará una lectura cada 30 segundos indefinidamente (puedes detenerlo con `Ctrl+C`).
```bash
python simulador.py
```

### Ejecutar una sola tanda
Ideal si solo quieres comprobar que la conexión funciona o necesitas enviar un solo punto de datos. El script enviará las medidas de todos los sensores activos y luego terminará automáticamente.
```bash
python simulador.py --once
```

### Forzar transporte específico
Si configuraste MQTT pero quieres probar que el respaldo HTTP (fallback) del backend está funcionando correctamente, puedes forzarlo:
```bash
python simulador.py --transport http
```
Del mismo modo, puedes forzar `--transport mqtt`.

### Simular Valores Anómalos
Esta es una característica clave para probar las **reglas de filtrado del backend**. Al usar la bandera `--anomalo`, el simulador mandará una lectura de temperatura irreal (ej. `999.0 °C`). El backend aceptará el payload (devolviendo código `201`) pero el dato debería ir a la tabla de `lecturas_anomalas` y no afectar los promedios ni generar falsas alertas agroclimáticas.
```bash
python simulador.py --anomalo
```

### Combinación de Banderas
Puedes combinar múltiples instrucciones. Por ejemplo, mandar una sola medición, por HTTP y con valores anómalos:
```bash
python simulador.py --transport http --once --anomalo
```

---

## 4. Analizando el Output

Cuando el simulador corre, su terminal te dirá exactamente qué ocurre.
* `[MQTT OK]` o `[HTTP OK]` significa que el backend recibió e ingirió el dato.
* `[MQTT !!]` o `[HTTP !!]` significa que la transmisión falló. Si es por HTTP, el script imprimirá la respuesta del backend (`404 Sensor no registrado` o `403 Sensor Inactivo`), lo cual es sumamente útil para detectar problemas de configuración.
