#!/usr/bin/env python3
"""SeedSigner Design Studio server: build, serve, watch, live-reload.

Started by `Start Design Studio.command` (via studio.sh, which provides Emscripten).
What it does:

  1. Builds the WASM engine (incremental) and assembles the site, the same layout
     GitHub Pages publishes: gallery at /, Classic runner at /runner/, studio at /redesign/.
  2. Serves it at http://localhost:<port>/ with no caching, and serves the Redesign's
     tokens.json + prototype.json LIVE from components/seedsigner/redesign/.
  3. Watches files and tells open pages what changed (Server-Sent Events, /events):
       tokens.json / prototype.json  -> "data"   (devices reload the JSON, ~instant)
                                         or "data-error" with the line and column
       C / C++ / CMake sources        -> "building", then "engine" or "build-failed"
       studio pages (redesign/*.html) -> "page"

Standard library only.
"""
import argparse
import http.server
import json
import os
import queue
import shutil
import socket
import socketserver
import subprocess
import sys
import threading
import time
import webbrowser

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
BUILD = os.path.join(HERE, "build-wasm")
SITE = os.path.join(HERE, "site")
DATA_SRC = os.path.join(REPO, "components", "seedsigner", "redesign")
DATA_FILES = ("tokens.json", "prototype.json")

ENGINE_DIRS = [os.path.join(REPO, "components"), os.path.join(REPO, "tools", "apps", "runner_core"),
               os.path.join(REPO, "tools", "apps", "web_runner")]
ENGINE_EXT = (".c", ".cpp", ".h", ".hpp", ".cmake", ".txt")
ENGINE_FILES = [os.path.join(HERE, "shell.html"), os.path.join(HERE, "gallery.html")]  # Classic pages
PAGE_FILES = [os.path.join(HERE, "redesign", "index.html"), os.path.join(HERE, "redesign", "device.html")]
SKIP_DIRS = {"build-wasm", "site", ".ccache", ".emcache", "src"}

clients = []
clients_lock = threading.Lock()


def log(msg):
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)


def broadcast(event, data=""):
    payload = f"event: {event}\n" + "".join(f"data: {line}\n" for line in (data or "").split("\n")) + "\n"
    with clients_lock:
        for q in list(clients):
            q.put(payload)


# ---------------------------------------------------------------------------
# Build + assemble
# ---------------------------------------------------------------------------

def build_engine():
    """Configure (first time) and build. Returns (ok, tail_of_log)."""
    if not os.path.exists(os.path.join(BUILD, "CMakeCache.txt")):
        gen = ["-G", "Ninja"] if shutil.which("ninja") else []
        cmd = ["emcmake", "cmake", "-S", HERE, "-B", BUILD, "-DCMAKE_BUILD_TYPE=Release",
               "-DDISPLAY_WIDTH=480", "-DDISPLAY_HEIGHT=320"] + gen
        r = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
        if r.returncode != 0:
            return False, (r.stdout + r.stderr)[-3000:]
    r = subprocess.run(["cmake", "--build", BUILD, "-j", str(os.cpu_count() or 4)],
                       cwd=REPO, capture_output=True, text=True)
    out = r.stdout + r.stderr
    if r.returncode != 0:
        errors = [l for l in out.splitlines() if "error" in l.lower()]
        return False, "\n".join(errors[:12]) or out[-3000:]
    return True, ""


def assemble_site():
    """Stage Classic assets (what exists locally) and assemble the Pages-identical site."""
    subprocess.run([sys.executable, os.path.join(REPO, "tools", "i18n", "gen_localized_scenarios.py")],
                   cwd=REPO, capture_output=True)
    subprocess.run([sys.executable, os.path.join(HERE, "stage_assets.py"), "--dest", BUILD,
                    "--available-only"], cwd=REPO, check=True, capture_output=True)
    shutil.copy(os.path.join(HERE, "gallery.html"), BUILD)
    subprocess.run(["bash", "scripts/ci/ci.sh", "assemble-site", SITE], cwd=REPO, check=True,
                   capture_output=True)


def copy_pages():
    for p in PAGE_FILES:
        shutil.copy(p, os.path.join(SITE, "redesign"))


# ---------------------------------------------------------------------------
# Watching
# ---------------------------------------------------------------------------

def engine_mtimes():
    out = mtimes(ENGINE_FILES)
    for root_dir in ENGINE_DIRS:
        for root, dirs, files in os.walk(root_dir):
            dirs[:] = [d for d in dirs if d not in SKIP_DIRS and not d.startswith(".")]
            for f in files:
                if f.endswith(ENGINE_EXT):
                    p = os.path.join(root, f)
                    try:
                        out[p] = os.path.getmtime(p)
                    except OSError:
                        pass
    return out


def mtimes(paths):
    out = {}
    for p in paths:
        try:
            out[p] = os.path.getmtime(p)
        except OSError:
            out[p] = 0
    return out


def check_json(path):
    try:
        with open(path, encoding="utf-8") as f:
            json.load(f)
        return None
    except json.JSONDecodeError as e:
        return f"{os.path.basename(path)}, line {e.lineno}, column {e.colno}: {e.msg}"


def watch():
    data_paths = [os.path.join(DATA_SRC, f) for f in DATA_FILES]
    data, pages, engine = mtimes(data_paths), mtimes(PAGE_FILES), engine_mtimes()
    while True:
        time.sleep(0.4)
        now = mtimes(data_paths)
        if now != data:
            data = now
            problems = [m for m in (check_json(p) for p in data_paths) if m]
            if problems:
                log("JSON error: " + "; ".join(problems))
                broadcast("data-error", "\n".join(problems))
            else:
                log("design data changed: devices reloading")
                broadcast("data")
        now = mtimes(PAGE_FILES)
        if now != pages:
            pages = now
            copy_pages()
            log("studio page changed: reloading")
            broadcast("page")
        now = engine_mtimes()
        if now != engine:
            engine = now
            log("C sources changed: rebuilding the engine…")
            broadcast("building")
            ok, detail = build_engine()
            if ok:
                assemble_site()
                log("rebuilt: devices reloading")
                broadcast("engine")
            else:
                log("BUILD FAILED\n" + detail)
                broadcast("build-failed", detail)
            engine = engine_mtimes()


# ---------------------------------------------------------------------------
# Serving
# ---------------------------------------------------------------------------

class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=SITE, **kw)

    def log_message(self, *a):
        pass

    def end_headers(self):
        self.send_header("Cache-Control", "no-store, must-revalidate")
        super().end_headers()

    def translate_path(self, path):
        clean = path.split("?", 1)[0].split("#", 1)[0]
        for name in DATA_FILES:
            if clean == f"/redesign/data/{name}":
                return os.path.join(DATA_SRC, name)
        return super().translate_path(path)

    def do_GET(self):
        if self.path.split("?", 1)[0] == "/events":
            return self.events()
        return super().do_GET()

    def events(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.end_headers()
        q = queue.Queue()
        q.put("event: hello\ndata: \n\n")
        with clients_lock:
            clients.append(q)
        try:
            while True:
                try:
                    msg = q.get(timeout=15)
                except queue.Empty:
                    msg = ": keep-alive\n\n"
                self.wfile.write(msg.encode())
                self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            with clients_lock:
                clients.remove(q)


class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True


def free_port(start):
    for port in range(start, start + 20):
        with socket.socket() as s:
            if s.connect_ex(("127.0.0.1", port)) != 0:
                return port
    sys.exit("No free port found near %d" % start)


def main():
    ap = argparse.ArgumentParser(description="SeedSigner Design Studio server")
    ap.add_argument("--port", type=int, default=8000)
    ap.add_argument("--no-open", action="store_true", help="don't open a browser")
    args = ap.parse_args()

    if not shutil.which("emcc"):
        sys.exit("Emscripten (emcc) is not on PATH. Start the studio with "
                 "'Start Design Studio.command' (or tools/apps/web_runner/studio.sh).")

    log("Building the engine (the first build takes a minute or two)…")
    ok, detail = build_engine()
    if not ok:
        print(detail)
        sys.exit("Build failed. The errors above say which file and line.")
    assemble_site()

    port = free_port(args.port)
    server = Server(("127.0.0.1", port), Handler)
    threading.Thread(target=watch, daemon=True).start()
    url = f"http://localhost:{port}/redesign/"
    print()
    print("  SeedSigner Design Studio is running:")
    print(f"    {url}")
    print()
    print("  Edit components/seedsigner/redesign/tokens.json or prototype.json and save:")
    print("  the browser updates by itself. Press Ctrl+C here to stop.")
    print()
    if not args.no_open:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")


if __name__ == "__main__":
    main()
