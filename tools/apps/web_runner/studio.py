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
       studio pages (redesign/*)      -> "page"
       .studio/comments.json          -> "comments" (the agent answered)
  4. Saves the studio's edits, version history and comments (/api/…, studio_store.py), so
     the page can edit the design files. Every change to them is a version in .studio/.

Standard library only.
"""
import argparse
import fcntl
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

import studio_store

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
PAGE_FILES = [os.path.join(HERE, "redesign", f) for f in ("index.html", "studio.css", "studio.js", "device.html")]
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
    notes = mtimes([studio_store.COMMENTS])
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
                # A change the studio didn't write (an agent, an editor) is a version too.
                texts = {n: studio_store.read(n) for n in studio_store.FILES}
                if texts != studio_store.written:
                    studio_store.written.update(texts)
                    if studio_store.record("outside", "Changed outside the studio"):
                        broadcast("history")
        now = mtimes([studio_store.COMMENTS])
        if now != notes:
            notes = now
            broadcast("comments")
            broadcast("history")
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
        route = self.path.split("?", 1)[0]
        if route == "/events":
            return self.events()
        if route == "/api/history":
            return self.reply(studio_store.history())
        if route.startswith("/api/history/"):
            try:
                return self.reply(studio_store.version_files(route.rsplit("/", 1)[1]))
            except OSError:
                return self.reply({"error": "No such version"}, 404)
        if route == "/api/comments":
            return self.reply(studio_store.comments())
        return super().do_GET()

    # ---- The studio's edits, history and comments (studio_store.py) ----------
    def reply(self, obj, code=200):
        body = json.dumps(obj, ensure_ascii=False).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        route = self.path.split("?", 1)[0]
        try:
            body = json.loads(self.rfile.read(int(self.headers.get("Content-Length") or 0)) or b"{}")
            if route == "/api/edit":
                if body.get("file") not in studio_store.FILES:
                    return self.reply({"error": "Unknown file"}, 400)
                v = studio_store.edit(body["file"], body.get("ops") or [], body.get("label") or "Edit")
                broadcast("history")
                return self.reply({"ok": True, "version": v})
            if route == "/api/history/name":
                v = studio_store.name_latest((body.get("name") or "").strip() or "Named version")
                broadcast("history")
                return self.reply({"ok": True, "version": v})
            if route.startswith("/api/history/") and route.endswith("/restore"):
                v = studio_store.restore(route.split("/")[3])
                broadcast("history")
                return self.reply({"ok": True, "version": v})
            if route == "/api/comments":
                db = studio_store.comment_action(body)
                broadcast("comments")
                return self.reply(db)
        except studio_store.EditError as e:
            msg = str(e)
            if msg == "changed":
                msg = "This was changed outside the studio since. Use version history."
            return self.reply({"error": msg}, 409)
        except (ValueError, KeyError, TypeError) as e:
            return self.reply({"error": "Bad request: %s" % e}, 400)
        return self.reply({"error": "Not found"}, 404)

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

    # One studio per checkout: a second launch (say, a double double-click) opens the
    # running one instead of starting a server that would race it in build-wasm/.
    os.makedirs(BUILD, exist_ok=True)
    lock = open(os.path.join(BUILD, ".studio.lock"), "a+")
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        lock.seek(0)
        url = lock.read().strip() or f"http://localhost:{args.port}/redesign/"
        print(f"The Design Studio is already running: {url}")
        if not args.no_open:
            webbrowser.open(url)
        return

    if not shutil.which("emcc"):
        sys.exit("Emscripten (emcc) is not on PATH. Start the studio with "
                 "'Start Design Studio.command' (or tools/apps/web_runner/studio.sh).")

    log("Building the engine (the first build takes a minute or two)…")
    ok, detail = build_engine()
    if not ok:
        print(detail)
        sys.exit("Build failed. The errors above say which file and line.")
    assemble_site()

    # The files as they are now: a version (so the first edit can be rolled back), and the
    # baseline that tells the watcher which later changes weren't the studio's.
    studio_store.written.update({n: studio_store.read(n) for n in studio_store.FILES})
    studio_store.record("outside", "Changed outside the studio" if studio_store.history() else "Starting point")
    port = free_port(args.port)
    server = Server(("127.0.0.1", port), Handler)
    threading.Thread(target=watch, daemon=True).start()
    url = f"http://localhost:{port}/redesign/"
    lock.seek(0)
    lock.truncate()
    lock.write(url)
    lock.flush()
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
