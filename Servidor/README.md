# 🖥️ Servidor — Semáforo IoT

Backend Node.js y dashboard web para el sistema de semáforo IoT. Desplegado en AWS EC2 Ubuntu con Apache y PM2.

---

## 📁 Estructura

```
Servidor/
├── Servicio/          # Backend Node.js
│   ├── server.js      # Servidor HTTP + WebSocket
│   ├── package.json   # Dependencias
│   └── cruces.json    # Base de datos de cruces (auto-generado)
└── Frontend/
    └── index.html     # Dashboard web en tiempo real
```

---

## ⚙️ Requisitos

- Ubuntu 20.04 / 22.04 / 24.04
- Node.js 20 LTS
- Apache2
- PM2

---

## 🚀 Instalación en AWS EC2

### 1. Instala Node.js 20

```bash
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt install -y nodejs
```

### 2. Clona el repositorio y entra a la carpeta

```bash
git clone https://github.com/TU_USUARIO/Semaforo_IoT.git
cd Semaforo_IoT/Servidor/Servicio
```

### 3. Instala dependencias

```bash
npm install
```

### 4. Actualiza la IP en el Frontend

Edita `Frontend/index.html` y reemplaza la IP:

```javascript
const WS_URL   = 'ws://TU_IP_AWS:8080';
const API_BASE = 'http://TU_IP_AWS:3000';
```

### 5. Copia el frontend a Apache

```bash
sudo mkdir /var/www/html/semaforo
sudo cp ../Frontend/index.html /var/www/html/semaforo/
```

### 6. Inicia el servidor con PM2

```bash
sudo npm install -g pm2
pm2 start server.js --name semaforo
pm2 save
pm2 startup
# Ejecuta el comando que te muestre pm2 startup
```

### 7. Abre los puertos en AWS Security Groups

| Puerto | Protocolo | Uso |
|--------|-----------|-----|
| 80 | HTTP | Apache / Dashboard |
| 3000 | TCP | API REST |
| 8080 | TCP | WebSocket |
| 22 | SSH | Administración |

---

## 🔌 API Endpoints

### POST `/api/evento`
El ESP32 envía eventos al servidor.

```json
{ "evento": "solicitud_peaton" }
```

Respuesta:
```json
{ "ok": true, "estado": "verde" }
```

### GET `/api/estado`
El ESP32 consulta el estado actual.

```json
{ "semaforo": "verde", "contador": 45 }
```

### GET `/api/historial`
Historial de los últimos 20 eventos en vivo.

### GET `/api/reporte?fecha=2026-05-08`
Reporte de cruces de una fecha específica.

```json
{
  "fecha": "2026-05-08",
  "total": 12,
  "por_hora": { "08": 3, "14": 5, "18": 4 },
  "eventos": [{ "hora": "08:23:11", "tipo": "sensor" }]
}
```

### GET `/api/reporte/all`
Resumen de todas las fechas con total de cruces.

---

## 🔄 Secuencia de estados

```
VERDE (60s) → ÁMBAR (3s) → ROJO (10s) → ÁMBAR (3s) → VERDE (60s)
```

El ciclo se inicia automáticamente cada 60 segundos o cuando el ESP32 envía `solicitud_peaton`.

Solo se registra un cruce cuando el evento viene del sensor físico, no del ciclo automático.

---

## 🛠️ Comandos útiles PM2

```bash
pm2 status              # Ver estado del servidor
pm2 logs semaforo       # Ver logs en vivo
pm2 restart semaforo    # Reiniciar servidor
pm2 monit               # Monitor de CPU y RAM
pm2 flush semaforo      # Limpiar logs
```

---

## 📊 Dashboard

Accede al dashboard en:
```
http://TU_IP_AWS/semaforo
```

Funcionalidades:
- Semáforo vehicular animado en tiempo real
- Contador regresivo con colores por estado
- Historial de eventos
- Contador de cruces del día
- Modal de reportes por fecha con hora pico
