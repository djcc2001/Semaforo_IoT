const express = require('express');
const WebSocket = require('ws');
const cors = require('cors');
const fs = require('fs');
const path = require('path');

const app = express();
app.use(cors());
app.use(express.json());

const DATA_FILE = path.join(__dirname, 'cruces.json');

function cargarDatos() {
  if (!fs.existsSync(DATA_FILE)) fs.writeFileSync(DATA_FILE, JSON.stringify({}));
  return JSON.parse(fs.readFileSync(DATA_FILE, 'utf8'));
}

function guardarDatos(datos) {
  fs.writeFileSync(DATA_FILE, JSON.stringify(datos, null, 2));
}

function getFechaHoy() {
  return new Date().toLocaleDateString('en-CA', { timeZone: 'America/Lima' });
}

function getHoraActual() {
  return new Date().toLocaleTimeString('es-PE', { timeZone: 'America/Lima', hour12: false });
}

function getHoraKey() {
  return getHoraActual().slice(0, 2);
}

function registrarCruce() {
  const datos = cargarDatos();
  const fecha = getFechaHoy();
  const hora  = getHoraActual();
  const horaKey = getHoraKey();

  if (!datos[fecha]) datos[fecha] = { total: 0, por_hora: {}, eventos: [] };

  datos[fecha].total++;
  datos[fecha].por_hora[horaKey] = (datos[fecha].por_hora[horaKey] || 0) + 1;
  datos[fecha].eventos.push({ hora, tipo: 'sensor' });

  guardarDatos(datos);
  return datos[fecha].total;
}

function getCrucesHoy() {
  const datos = cargarDatos();
  const fecha = getFechaHoy();
  return datos[fecha] ? datos[fecha].total : 0;
}

let estado = {
  semaforo: 'verde',
  contador: 60,
  ultimoEvento: '',
  historial: [],
  crucesHoy: getCrucesHoy()
};

let contadorInterval = null;
let contadorVerde    = null;

const wss = new WebSocket.Server({ port: 8080 });

function broadcast(data) {
  wss.clients.forEach(client => {
    if (client.readyState === WebSocket.OPEN) client.send(JSON.stringify(data));
  });
}

function registrarHistorial(evento) {
  estado.historial.unshift({ evento, hora: getHoraActual() });
  if (estado.historial.length > 20) estado.historial.pop();
}

function iniciarContadorVerde() {
  if (contadorVerde) clearInterval(contadorVerde);
  estado.semaforo = 'verde';
  estado.contador = 60;
  broadcast(estado);

  contadorVerde = setInterval(() => {
    estado.contador--;
    broadcast(estado);
    if (estado.contador <= 0) {
      clearInterval(contadorVerde);
      contadorVerde = null;
      iniciarSecuencia(false);
    }
  }, 1000);
}

function iniciarSecuencia(fueSensor) {
  if (contadorInterval) return;
  if (contadorVerde) { clearInterval(contadorVerde); contadorVerde = null; }

  if (fueSensor) {
    estado.crucesHoy = registrarCruce();
    registrarHistorial('solicitud_peaton');
  }

  // Fase 1: AMBAR 3s
  estado.semaforo = 'ambar';
  estado.contador = 3;
  registrarHistorial('semaforo_ambar');
  broadcast(estado);

  contadorInterval = setInterval(() => {
    estado.contador--;
    broadcast(estado);

    if (estado.contador <= 0) {
      clearInterval(contadorInterval);
      contadorInterval = null;

      // Fase 2: ROJO 10s
      estado.semaforo = 'rojo';
      estado.contador = 10;
      registrarHistorial('peaton_cruza');
      broadcast(estado);

      contadorInterval = setInterval(() => {
        estado.contador--;
        broadcast(estado);

        if (estado.contador <= 0) {
          clearInterval(contadorInterval);
          contadorInterval = null;

          // Fase 3: AMBAR 3s
          estado.semaforo = 'ambar';
          estado.contador = 3;
          registrarHistorial('volviendo_normal');
          broadcast(estado);

          contadorInterval = setInterval(() => {
            estado.contador--;
            broadcast(estado);

            if (estado.contador <= 0) {
              clearInterval(contadorInterval);
              contadorInterval = null;

              // Fase 4: VERDE 60s
              registrarHistorial('semaforo_verde');
              iniciarContadorVerde();
            }
          }, 1000);
        }
      }, 1000);
    }
  }, 1000);
}

// ── ENDPOINTS ──────────────────────────────────────────

// ESP32 envía evento
app.post('/api/evento', (req, res) => {
  const { evento } = req.body;
  console.log('Evento:', evento, getHoraActual());
  estado.ultimoEvento = evento;

  if (evento === 'solicitud_peaton' && estado.semaforo === 'verde') {
    iniciarSecuencia(true);
  }

  res.json({ ok: true, estado: estado.semaforo });
});

// ESP32 consulta estado
app.get('/api/estado', (req, res) => {
  res.json({ semaforo: estado.semaforo, contador: estado.contador });
});

// Historial en vivo
app.get('/api/historial', (req, res) => {
  res.json(estado.historial);
});

// Reporte por fecha (default hoy)
app.get('/api/reporte', (req, res) => {
  const datos = cargarDatos();
  const fecha = req.query.fecha || getFechaHoy();
  if (!datos[fecha]) return res.json({ fecha, total: 0, por_hora: {}, eventos: [] });
  res.json({ fecha, ...datos[fecha] });
});

// Resumen todas las fechas
app.get('/api/reporte/all', (req, res) => {
  const datos = cargarDatos();
  const resumen = Object.entries(datos)
    .map(([fecha, d]) => ({ fecha, total: d.total }))
    .sort((a, b) => b.fecha.localeCompare(a.fecha));
  res.json(resumen);
});

app.listen(3000, () => {
  console.log('Servidor HTTP en puerto 3000');
  iniciarContadorVerde();
});

wss.on('listening', () => console.log('WebSocket en puerto 8080'));
