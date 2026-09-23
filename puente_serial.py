#!/usr/bin/env python3
"""
puente_serial.py - Smart Plant Bot

Puente OPCIONAL entre el protocolo Serial del Arduino Uno (ver
protocolo_serial.ino) y HTTP en localhost. Sirve como alternativa para
navegadores que no soportan Web Serial API (por ejemplo Firefox o
Safari): la interfaz les habla a este puente por HTTP, y el puente le
habla al Arduino por el puerto serie de siempre.

Si el navegador soporta Web Serial (Chrome/Edge), este puente NO hace
falta: plantabot.html puede hablarle directo al Arduino via
navigator.serial. Ver README.md.

Dependencias: pyserial (`pip install pyserial`) + solo biblioteca
estandar (http.server, json, glob, threading, etc.).

Uso:
    python3 puente_serial.py [--puerto /dev/ttyACM0] [--http-puerto 8765]

Sin --puerto, autodetecta: prueba /dev/ttyACM* y /dev/ttyUSB* en
Linux/Mac (un Arduino Uno usa el ATmega16U2 para USB y enumera como
ttyACM, no ttyUSB -- eso es tipico de clones con chip CH340/FTDI, por
eso se prueban los dos patrones) y COM* en Windows.
"""

import argparse
import glob
import json
import platform
import threading
import time
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print(
        "ERROR: falta pyserial. Instalalo con: pip install pyserial",
    )
    raise SystemExit(1)

BAUDIOS = 9600
TIMEOUT_LECTURA_S = 2.0
ESPERA_ARRANQUE_S = 2.0  # el Uno se resetea al abrir el puerto (DTR)


def detectar_puertos():
    """Devuelve una lista de puertos serie candidatos, sin abrirlos."""
    sistema = platform.system()
    if sistema == "Windows":
        return [p.device for p in list_ports.comports() if p.device.upper().startswith("COM")]

    # Linux/Mac: /dev/ttyACM* primero (Uno real, ATmega16U2), despues
    # /dev/ttyUSB* (clones con CH340/FTDI u otros adaptadores).
    candidatos = sorted(glob.glob("/dev/ttyACM*")) + sorted(glob.glob("/dev/ttyUSB*"))
    return candidatos


class PuenteSerial:
    """Envuelve la conexion serie y el protocolo de linea con el Arduino."""

    def __init__(self, puerto=None):
        self.puerto_nombre = puerto or self._elegir_puerto()
        self.conexion = serial.Serial(self.puerto_nombre, BAUDIOS, timeout=TIMEOUT_LECTURA_S)
        self.lock = threading.Lock()

        # Abrir el puerto resetea el Uno (via DTR): esperamos a que
        # termine de arrancar antes de mandarle comandos.
        time.sleep(ESPERA_ARRANQUE_S)
        self.conexion.reset_input_buffer()

    def _elegir_puerto(self):
        candidatos = detectar_puertos()
        if not candidatos:
            raise RuntimeError(
                "No se encontro ningun puerto serie candidato "
                "(probados: /dev/ttyACM*, /dev/ttyUSB*, COM*). "
                "Conecta el Arduino o pasa --puerto explicitamente."
            )
        return candidatos[0]

    def enviar_comando(self, comando):
        """Manda una linea de comando y junta la respuesta completa.

        La ultima linea de toda respuesta del firmware empieza con
        "OK;" o "ERR;" (ver protocolo_serial.ino): se lee hasta
        encontrar esa linea, o hasta que se agote el timeout.
        """
        with self.lock:
            self.conexion.reset_input_buffer()
            self.conexion.write((comando.strip() + "\n").encode("utf-8"))

            lineas = []
            while True:
                cruda = self.conexion.readline()
                if not cruda:
                    break  # timeout sin recibir la linea terminal
                texto = cruda.decode("utf-8", errors="replace").strip("\r\n")
                if texto == "":
                    continue
                lineas.append(texto)
                if texto.startswith("OK;") or texto.startswith("ERR;"):
                    break
            return lineas

    def cerrar(self):
        self.conexion.close()


class ManejadorHTTP(BaseHTTPRequestHandler):
    puente = None  # se asigna en main() antes de levantar el servidor

    def _responder_json(self, status, payload):
        cuerpo = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(cuerpo)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(cuerpo)

    def do_OPTIONS(self):
        # Preflight CORS, por si la interfaz corre en otro origen/puerto.
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def do_GET(self):
        ruta = urllib.parse.urlparse(self.path)
        if ruta.path == "/puerto":
            self._responder_json(200, {"puerto": ManejadorHTTP.puente.puerto_nombre})
            return
        if ruta.path == "/comando":
            qs = urllib.parse.parse_qs(ruta.query)
            comando = qs.get("c", [None])[0]
            self._manejar_comando(comando)
            return
        self._responder_json(404, {"error": "ruta no encontrada"})

    def do_POST(self):
        ruta = urllib.parse.urlparse(self.path)
        if ruta.path == "/comando":
            largo = int(self.headers.get("Content-Length", "0"))
            cuerpo = self.rfile.read(largo).decode("utf-8", errors="replace") if largo else ""
            self._manejar_comando(cuerpo.strip())
            return
        self._responder_json(404, {"error": "ruta no encontrada"})

    def _manejar_comando(self, comando):
        if not comando:
            self._responder_json(400, {"error": "falta el parametro de comando"})
            return
        try:
            lineas = ManejadorHTTP.puente.enviar_comando(comando)
        except Exception as e:  # el puerto se desconecto, timeout raro, etc.
            self._responder_json(500, {"error": str(e)})
            return
        self._responder_json(200, {"comando": comando, "lineas": lineas})

    def log_message(self, formato, *args):
        pass  # silencia el log de acceso por defecto (ver README)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--puerto", default=None,
        help="Puerto serie (ej: /dev/ttyACM0, COM3). Si se omite, se autodetecta.",
    )
    parser.add_argument(
        "--http-puerto", type=int, default=8765,
        help="Puerto HTTP local donde escucha el puente (default: 8765).",
    )
    args = parser.parse_args()

    puente = PuenteSerial(args.puerto)
    print(f"Conectado a {puente.puerto_nombre} a {BAUDIOS} baudios.")

    ManejadorHTTP.puente = puente
    servidor = ThreadingHTTPServer(("127.0.0.1", args.http_puerto), ManejadorHTTP)
    print(f"Puente HTTP escuchando en http://127.0.0.1:{args.http_puerto}")
    print("Rutas: GET /puerto | GET /comando?c=<CMD> | POST /comando (body=CMD)")
    print("Ctrl+C para salir.")

    try:
        servidor.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        servidor.server_close()
        puente.cerrar()


if __name__ == "__main__":
    main()
