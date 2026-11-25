"""BagoNX PC: comparte tus juegos con BagoNX por la red local de tu casa.

Cada subcarpeta de la carpeta de juegos es un juego. Su nombre tiene que
coincidir con el nombre o el id del port en BagoNX (por ejemplo "Manhunt").

Uso:  python bagonx_pc.py [carpeta_de_juegos] [--puerto 8686]

Solo usa la biblioteca estándar de Python. Los archivos no salen de tu red:
el servidor solo responde a quien sepa el PIN que se muestra al arrancar.
"""
import argparse
import hashlib
import json
import os
import secrets
import socket
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, unquote, urlsplit

HERE = os.path.dirname(os.path.abspath(__file__))
PIN_FILE = os.path.join(HERE, "pin.txt")
CACHE_FILE = os.path.join(HERE, "hash_cache.json")
DEFAULT_DIR = os.path.join(os.path.dirname(HERE), "juegos")

GAMES = []          # [{name, size, files:[{path,size,sha256}]}]
ROOT = ""
PIN = ""
LOCK = threading.Lock()


def load_pin():
    try:
        pin = open(PIN_FILE, encoding="utf-8").read().strip()
        if pin.isdigit() and len(pin) == 4:
            return pin
    except OSError:
        pass
    pin = f"{secrets.randbelow(10000):04d}"
    with open(PIN_FILE, "w", encoding="utf-8") as f:
        f.write(pin)
    return pin


def lan_ips():
    ips = set()
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("10.255.255.255", 1))  # no envía nada; solo elige la interfaz
        ips.add(s.getsockname()[0])
        s.close()
    except OSError:
        pass
    try:
        for info in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
            ips.add(info[4][0])
    except OSError:
        pass
    return sorted(ip for ip in ips if not ip.startswith("127."))


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def scan(root):
    """Lee las carpetas de juegos y calcula los SHA-256 (con caché)."""
    try:
        cache = json.load(open(CACHE_FILE, encoding="utf-8"))
    except (OSError, ValueError):
        cache = {}
    games = []
    for name in sorted(os.listdir(root), key=str.lower):
        gdir = os.path.join(root, name)
        if not os.path.isdir(gdir) or name.startswith("."):
            continue
        files, total = [], 0
        for d, dirs, names in os.walk(gdir):
            dirs.sort()
            for n in sorted(names):
                full = os.path.join(d, n)
                st = os.stat(full)
                rel = os.path.relpath(full, gdir).replace(os.sep, "/")
                key = f"{full}|{st.st_size}|{int(st.st_mtime)}"
                digest = cache.get(key)
                if digest is None:
                    print(f"  calculando huella: {name}/{rel}", end="\r")
                    digest = sha256_file(full)
                    cache[key] = digest
                files.append({"path": rel, "size": st.st_size, "sha256": digest})
                total += st.st_size
        print(" " * 100, end="\r")
        games.append({"name": name, "size": total, "files": files})
        print(f"  {name}: {len(files)} archivos, {total / 2**20:,.0f} MB")
    with open(CACHE_FILE, "w", encoding="utf-8") as f:
        json.dump(cache, f)
    return games


class Handler(BaseHTTPRequestHandler):
    server_version = "BagoNX-PC/1.0"

    def log_message(self, fmt, *args):
        pass  # sin ruido en la consola; las copias se anuncian aparte

    def deny(self, code, text):
        body = text.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        url = urlsplit(self.path)
        if parse_qs(url.query).get("k", [""])[0] != PIN:
            return self.deny(403, "PIN incorrecto")
        if url.path == "/bagonx/list":
            body = json.dumps({"games": GAMES}).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        if url.path.startswith("/bagonx/file/"):
            rel = unquote(url.path[len("/bagonx/file/"):])
            parts = rel.split("/")
            if any(p in ("", ".", "..") for p in parts) or len(parts) < 2:
                return self.deny(400, "ruta no válida")
            full = os.path.realpath(os.path.join(ROOT, *parts))
            game_dir = os.path.realpath(os.path.join(ROOT, parts[0]))
            if not full.startswith(game_dir + os.sep) or not os.path.isfile(full):
                return self.deny(404, "no existe")
            size = os.path.getsize(full)
            print(f"  enviando a {self.client_address[0]}: {rel[:70]:<70}", end="\r")
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(size))
            self.end_headers()
            try:
                with open(full, "rb") as f:
                    for chunk in iter(lambda: f.read(1 << 20), b""):
                        self.wfile.write(chunk)
            except (BrokenPipeError, ConnectionResetError):
                pass
            return
        self.deny(404, "no existe")


def main():
    global ROOT, PIN, GAMES
    # Algunas consolas de Windows no pueden mostrar "→" o "ñ": que no se cierre por eso.
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(errors="replace")
        except (AttributeError, ValueError):
            pass
    ap =argparse.ArgumentParser(description="Comparte tus juegos con BagoNX por la red local.")
    ap.add_argument("carpeta", nargs="?", default=DEFAULT_DIR)
    ap.add_argument("--puerto", type=int, default=8686)
    a = ap.parse_args()

    ROOT = os.path.realpath(a.carpeta)
    if not os.path.isdir(ROOT):
        os.makedirs(ROOT, exist_ok=True)
        print(f"Creé la carpeta {ROOT}. Pon dentro una carpeta por juego y vuelve a abrir este programa.")
    PIN = load_pin()
    print(f"Carpeta de juegos: {ROOT}")
    GAMES = scan(ROOT)
    if not GAMES:
        print("  (no hay juegos todavía)")

    srv = ThreadingHTTPServer(("0.0.0.0", a.puerto), Handler)
    print()
    print("=" * 60)
    print("  En BagoNX → Ports → Tu PC (red local), escribe:")
    for ip in lan_ips() or ["(no se encontró la IP; mírala con ipconfig)"]:
        print(f"    Dirección: {ip}")
    print(f"    PIN:       {PIN}")
    print("=" * 60)
    print("Deja esta ventana abierta mientras copias. Ctrl+C para cerrar.")
    print("Si Windows pregunta por el firewall, permite solo 'Redes privadas'.")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("\nServidor cerrado.")


if __name__ == "__main__":
    sys.exit(main())
