"""Design Studio data layer: edits to the design JSON, version history, comments.

Used by studio.py (the server) and studio_comments.py (the agent's CLI). Standard library only.

Edits are SURGICAL: the JSON text is parsed with source spans, and an edit replaces only the
text of the values it changes. Everything else (the hand formatting of tokens.json and
prototype.json, notes, alignment) stays byte for byte, so a studio save never reformats what
it didn't touch. New values that span lines are written in the files' house style.

Everything the studio keeps lives in <repo>/.studio/ (gitignored):
  history/index.json        the version list
  history/<id>.json         one version: the full text of both files
  comments.json             comments and their threads
  comments/<id>.png         the screen snapshot sent with a comment
See context/design-studio.md.
"""
import json
import os
import tempfile
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
DATA_DIR = os.path.join(REPO, "components", "seedsigner", "redesign")
FILES = {"tokens": os.path.join(DATA_DIR, "tokens.json"),
         "prototype": os.path.join(DATA_DIR, "prototype.json")}
STORE = os.path.join(REPO, ".studio")
HISTORY = os.path.join(STORE, "history")
COMMENTS = os.path.join(STORE, "comments.json")
SNAPS = os.path.join(STORE, "comments")
WIDTH = 118          # house style: wrap new values before this column

lock = threading.RLock()


class EditError(Exception):
    """An edit that can't apply (stale expectation, bad path). Nothing was written."""


# ---------------------------------------------------------------------------
# JSON with source spans
# ---------------------------------------------------------------------------

class Node:
    __slots__ = ("kind", "start", "end", "items", "value")

    def __init__(self, kind, start):
        self.kind, self.start, self.end, self.items, self.value = kind, start, start, None, None


def parse(text):
    """Parse JSON, keeping each value's [start, end) span. Objects: items = [(key, Node)]."""
    dec = json.JSONDecoder()
    n = len(text)

    def ws(i):
        while i < n and text[i] in " \t\r\n":
            i += 1
        return i

    def value(i):
        i = ws(i)
        c = text[i]
        if c == "{":
            node = Node("obj", i)
            node.items = []
            i = ws(i + 1)
            if text[i] == "}":
                node.end = i + 1
                return node
            while True:
                key, i = dec.raw_decode(text, ws(i))
                i = ws(i)
                assert text[i] == ":", f"expected ':' at {i}"
                child = value(i + 1)
                node.items.append((key, child))
                i = ws(child.end)
                if text[i] == ",":
                    i += 1
                    continue
                assert text[i] == "}", f"expected '}}' at {i}"
                node.end = i + 1
                return node
        if c == "[":
            node = Node("arr", i)
            node.items = []
            i = ws(i + 1)
            if text[i] == "]":
                node.end = i + 1
                return node
            while True:
                child = value(i)
                node.items.append(child)
                i = ws(child.end)
                if text[i] == ",":
                    i += 1
                    continue
                assert text[i] == "]", f"expected ']' at {i}"
                node.end = i + 1
                return node
        node = Node("val", i)
        node.value, node.end = dec.raw_decode(text, i)
        return node

    return value(0)


def find(root, path):
    """The node at a path of keys and indices, or None."""
    node = root
    for p in path:
        if node is None:
            return None
        if node.kind == "obj" and isinstance(p, str):
            node = next((v for k, v in node.items if k == p), None)
        elif node.kind == "arr" and isinstance(p, int) and 0 <= p < len(node.items):
            node = node.items[p]
        else:
            return None
    return node


def get(data, path):
    """(found, value) for a path into plain JSON data."""
    for p in path:
        if isinstance(data, dict) and isinstance(p, str) and p in data:
            data = data[p]
        elif isinstance(data, list) and isinstance(p, int) and 0 <= p < len(data):
            data = data[p]
        else:
            return False, None
    return True, data


# ---------------------------------------------------------------------------
# House-style serialisation for new values
# ---------------------------------------------------------------------------

def inline(v):
    if isinstance(v, dict):
        if not v:
            return "{}"
        return "{ " + ", ".join(json.dumps(k, ensure_ascii=False) + ": " + inline(x) for k, x in v.items()) + " }"
    if isinstance(v, list):
        if not v:
            return "[]"
        return "[" + ", ".join(inline(x) for x in v) + "]"
    return json.dumps(v, ensure_ascii=False)


def is_obj_list(v):
    return isinstance(v, list) and v and all(isinstance(x, dict) for x in v)


def house(v, indent):
    """A value in the files' style: inline when it fits; arrays of objects one per line."""
    one = inline(v)
    pad = " " * indent
    if isinstance(v, list) and is_obj_list(v):
        return "[\n" + ",\n".join(pad + "  " + house(x, indent + 2) for x in v) + "\n" + pad + "]"
    if isinstance(v, dict):
        keys = list(v)
        tail = keys[-1] if keys else None
        if tail is not None and is_obj_list(v[tail]):
            # { "type": "group", "children": [ … ]} : head inline, children one per line
            head = ", ".join(json.dumps(k, ensure_ascii=False) + ": " + inline(v[k]) for k in keys[:-1])
            items = ",\n".join(pad + "  " + house(x, indent + 2) for x in v[tail])
            return "{ " + (head + ", " if head else "") + json.dumps(tail, ensure_ascii=False) + ": [\n" + \
                items + "\n" + pad + "]}"
        if indent + len(one) <= WIDTH:
            return one
        parts = [json.dumps(k, ensure_ascii=False) + ": " + house(x, indent + 2) for k, x in v.items()]
        return "{\n" + ",\n".join(pad + "  " + p for p in parts) + "\n" + pad + "}"
    if isinstance(v, list) and indent + len(one) > WIDTH:
        return "[\n" + ",\n".join(pad + "  " + house(x, indent + 2) for x in v) + "\n" + pad + "]"
    return one


def indent_at(text, pos):
    line_start = text.rfind("\n", 0, pos) + 1
    i = line_start
    while i < len(text) and text[i] == " ":
        i += 1
    return i - line_start


# ---------------------------------------------------------------------------
# Edits
# ---------------------------------------------------------------------------

def _set(text, root, path, value):
    """Return text with path set to value, touching as little text as possible."""
    node = find(root, path)
    if node is not None:
        old = json.loads(text[node.start:node.end])
        if old == value:
            return text
        if node.kind == "obj" and isinstance(value, dict):
            shared = [k for k in old if k in value]
            if shared == [k for k in value if k in old]:      # same order: edit member by member
                for k in [k for k in old if k not in value]:
                    text = _delete(text, parse(text), path + [k])
                for k in value:
                    text = _set(text, parse(text), path + [k], value[k])
                return text
        if node.kind == "arr" and isinstance(value, list) and len(old) == len(value):
            for i in range(len(value)):
                text = _set(text, parse(text), path + [i], value[i])
            return text
        return text[:node.start] + house(value, indent_at(text, node.start)) + text[node.end:]
    parent = find(root, path[:-1])
    key = path[-1] if path else None
    if parent is None or parent.kind != "obj" or not isinstance(key, str):
        raise EditError("Can't add " + "/".join(map(str, path)) + ": its parent doesn't exist.")
    member = json.dumps(key, ensure_ascii=False) + ": "
    if not parent.items:
        return text[:parent.start] + "{ " + member + inline(value) + " }" + text[parent.end:]
    last = parent.items[-1][1]
    last_key = _key_start(text, parent, len(parent.items) - 1)
    if text.rfind("\n", parent.start, last_key) > parent.start and "\n" in text[last.end:parent.end]:
        sep = ",\n" + " " * indent_at(text, last_key)     # one member per line
        ind = indent_at(text, last_key)
    else:
        sep, ind = ", ", indent_at(text, last.start)
    return text[:last.end] + sep + member + house(value, ind + len(member)) + text[last.end:]


def _key_start(text, parent, i):
    """Where member i's key starts (its opening quote)."""
    node = parent.items[i][1]
    colon = text.rfind(":", parent.start, node.start)
    end_q = text.rfind('"', parent.start, colon)
    return text.rfind('"', parent.start, end_q)


def _delete(text, root, path):
    parent = find(root, path[:-1])
    if parent is None or parent.kind not in ("obj", "arr") or not path:
        return text
    if parent.kind == "arr":
        keys, nodes = list(range(len(parent.items))), parent.items
        starts = [n.start for n in nodes]
    else:
        keys, nodes = [k for k, _ in parent.items], [n for _, n in parent.items]
        starts = [_key_start(text, parent, i) for i in range(len(nodes))]
    if path[-1] not in keys:
        return text
    i = keys.index(path[-1])
    if i > 0:                       # drop ", <member>" after the previous member
        return text[:nodes[i - 1].end] + text[nodes[i].end:]
    if len(nodes) > 1:              # first of several: drop "<member>, " up to the next one
        return text[:starts[0]] + text[starts[1]:]
    return text[:parent.start] + ("[]" if parent.kind == "arr" else "{}") + text[parent.end:]


def apply_ops(text, ops):
    """Apply [{path, value} | {path, delete: true}] with optional expect checks. Returns new text."""
    data = json.loads(text)
    for op in ops:
        path = list(op["path"])
        if "expect" in op or "expect_missing" in op:
            found, cur = get(data, path)
            if op.get("expect_missing"):
                if found:
                    raise EditError("changed")
            elif not found or cur != op["expect"]:
                raise EditError("changed")
        root = parse(text)
        if op.get("delete"):
            text = _delete(text, root, path)
        else:
            text = _set(text, root, path, op["value"])
        data = json.loads(text)        # also proves the result is valid JSON
    return text


def read(name):
    with open(FILES[name], encoding="utf-8") as f:
        return f.read()


def write_atomic(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=os.path.dirname(path), prefix=".tmp-")
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        f.write(text)
    os.replace(tmp, path)


# Text the studio itself last wrote, so the watcher can tell studio saves from outside edits.
written = {}


def edit(name, ops, label):
    """Apply ops to a design file on disk, save it, and record a version."""
    with lock:
        before = read(name)
        after = apply_ops(before, ops)
        if after == before:
            return None
        written[name] = after
        write_atomic(FILES[name], after)
        return record("you", label)


# ---------------------------------------------------------------------------
# Version history
# ---------------------------------------------------------------------------

def _index():
    try:
        with open(os.path.join(HISTORY, "index.json"), encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return []


def _save_index(idx):
    write_atomic(os.path.join(HISTORY, "index.json"), json.dumps(idx, indent=1, ensure_ascii=False))


def history():
    with lock:
        return _index()


def version_files(vid):
    with open(os.path.join(HISTORY, os.path.basename(vid) + ".json"), encoding="utf-8") as f:
        return json.load(f)


def record(who, title, detail=""):
    """Snapshot both files as a new version. Skips if nothing changed since the last one."""
    with lock:
        files = {n: read(n) for n in FILES}
        idx = _index()
        if idx:
            try:
                if version_files(idx[-1]["id"]) == files:
                    return None
            except OSError:
                pass
        vid = time.strftime("%Y%m%d-%H%M%S") + "-%03d" % (int(time.time() * 1000) % 1000)
        while any(v["id"] == vid for v in idx):
            vid += "b"
        write_atomic(os.path.join(HISTORY, vid + ".json"), json.dumps(files, ensure_ascii=False))
        entry = {"id": vid, "t": time.time(), "who": who, "title": title, "detail": detail}
        idx.append(entry)
        _save_index(idx)
        return entry


def restore(vid):
    with lock:
        idx = _index()
        src = next((v for v in idx if v["id"] == vid), None)
        if not src:
            raise EditError("No such version")
        files = version_files(vid)
        for n, text in files.items():
            written[n] = text
            write_atomic(FILES[n], text)
        label = src.get("name") or src["title"]
        return record("you", "Restored “%s”" % label, time.strftime("from %b %d, %H:%M", time.localtime(src["t"])))


def name_latest(name):
    with lock:
        idx = _index()
        if not idx:
            record("you", "Named version")
            idx = _index()
        idx[-1]["name"] = name
        _save_index(idx)
        return idx[-1]


def claim_outside(since, title, detail):
    """Relabel outside changes made since `since` as the agent's (the Agent badge)."""
    with lock:
        idx = _index()
        hit = False
        for v in idx:
            if v["t"] >= since and v["who"] in ("outside", "agent"):
                if v["who"] == "outside":
                    v["who"], v["title"], v["detail"] = "agent", title, detail
                else:
                    v["detail"] = (v.get("detail") + "; " if v.get("detail") else "") + detail
                hit = True
        if hit:
            _save_index(idx)
        return hit


# ---------------------------------------------------------------------------
# Comments
# ---------------------------------------------------------------------------

def comments():
    try:
        with open(COMMENTS, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return {"next": 1, "comments": []}


def save_comments(db):
    write_atomic(COMMENTS, json.dumps(db, indent=1, ensure_ascii=False))


def comment_action(body):
    """create / reply / delete / reopen / resolve / send, from the studio page."""
    import base64
    with lock:
        db = comments()
        act = body.get("action")
        now = time.time()
        if act == "create":
            c = {k: body.get(k) for k in ("screen", "path", "part", "x", "y", "dx", "dy", "size", "tokens")}
            c.update({"id": db["next"], "status": "draft", "created": now,
                      "thread": [{"who": "you", "text": body["text"], "t": now}]})
            db["next"] += 1
            db["comments"].append(c)
        else:
            c = next((x for x in db["comments"] if x["id"] == body.get("id")), None)
            if act == "send":
                drafts = [x for x in db["comments"] if x["status"] == "draft"]
                for x in drafts:
                    x["status"], x["sent_at"] = "sent", now
                    png = (body.get("snapshots") or {}).get(str(x["id"]))
                    if png and png.startswith("data:image/png;base64,"):
                        os.makedirs(SNAPS, exist_ok=True)
                        with open(os.path.join(SNAPS, "%d.png" % x["id"]), "wb") as f:
                            f.write(base64.b64decode(png.split(",", 1)[1]))
                        x["snapshot"] = ".studio/comments/%d.png" % x["id"]
            elif c is None:
                raise EditError("No such comment")
            elif act == "reply":
                c["thread"].append({"who": "you", "text": body["text"], "t": now})
                c["status"] = "draft"
            elif act == "delete":
                db["comments"].remove(c)
            elif act == "reopen":
                c["status"] = "draft"
            elif act == "resolve":
                c["status"] = "resolved"
        save_comments(db)
        return db


def agent_answer(cid, text, resolve):
    """The agent's reply (question back) or resolution, from studio_comments.py."""
    with lock:
        db = comments()
        c = next((x for x in db["comments"] if x["id"] == cid), None)
        if c is None:
            raise EditError("No comment #%d" % cid)
        c["thread"].append({"who": "agent", "text": text, "t": time.time()})
        c["status"] = "resolved" if resolve else "replied"
        save_comments(db)
        since = c.get("sent_at") or c.get("created") or 0
        claim_outside(since, ("Addressed comment #%d" % cid) if resolve else ("Answered comment #%d" % cid),
                      "#%d: %s" % (cid, text))
        return c
