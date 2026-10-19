// SeedSigner Design Studio (see context/design-studio.md).
//
// Design mode: a canvas of every screen, grouped by the flows in prototype.json. The pictures
// come from a hidden engine (device.html?snap=1) that renders each screen and reports every
// part's box, so selecting on the canvas hits the same parts the inspector knows.
// Prototype mode: live devices (one engine each), tap / inspect / comment, frame-rate dial.
// Edits go to studio.py (/api/edit), which saves the JSON files surgically and records a
// version; the files' change comes back as an SSE "data" event and everything re-renders.
(function () {
'use strict';

// ── Helpers ─────────────────────────────────────────────────────────────────
var $ = function (s, r) { return (r || document).querySelector(s); };
var $$ = function (s, r) { return [].slice.call((r || document).querySelectorAll(s)); };
var esc = function (s) { return String(s == null ? '' : s).replace(/[&<>"]/g, function (c) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]; }); };
var clone = function (o) { return JSON.parse(JSON.stringify(o)); };
var same = function (a, b) { return JSON.stringify(a) === JSON.stringify(b); };
var ICON = {
  select: '<path d="M4 4l7.07 17 2.51-7.39L21 11.07z"/>',
  hand: '<path d="M18 11V6a2 2 0 0 0-4 0v5M14 10V4a2 2 0 0 0-4 0v6M10 10.5V6a2 2 0 0 0-4 0v8M18 8a2 2 0 1 1 4 0v6a8 8 0 0 1-8 8h-2c-2.8 0-4.5-.86-6-2.34l-3.6-3.6a2 2 0 0 1 2.83-2.82L7 15"/>',
  comment: '<path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h12a2 2 0 0 1 2 2z"/>',
  play: '<path d="M7 4.5v15l12-7.5z"/>',
  x: '<path d="M18 6 6 18M6 6l12 12"/>',
  check: '<path d="M20 6 9 17l-5-5"/>',
  chev: '<path d="m6 9 6 6 6-6"/>',
  ext: '<path d="M15 3h6v6M10 14 21 3M21 14v5a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5"/>',
  send: '<path d="M22 2 11 13M22 2l-7 20-4-9-9-4z"/>',
  restart: '<path d="M3 12a9 9 0 1 0 3-6.7L3 8M3 3v5h5"/>',
  back: '<path d="m15 18-6-6 6-6"/>',
  inspect: '<path d="M3 3h6M3 3v6M21 3h-6M21 3v6M3 21h6M3 21v-6M21 21h-6M21 21v-6M9 12h6M12 9v6"/>',
  tap: '<path d="M9 9V4.5a1.5 1.5 0 0 1 3 0V12M12 10.5a1.5 1.5 0 0 1 3 0V12M15 11a1.5 1.5 0 0 1 3 0v3a6 6 0 0 1-6 6h-1a6 6 0 0 1-4.5-2L4 14.5a1.5 1.5 0 0 1 2.2-2L9 15"/>',
  bookmark: '<path d="M19 21l-7-5-7 5V5a2 2 0 0 1 2-2h10a2 2 0 0 1 2 2z"/>',
  braces: '<path d="M8 3H7a2 2 0 0 0-2 2v5a2 2 0 0 1-2 2 2 2 0 0 1 2 2v5a2 2 0 0 0 2 2h1M16 21h1a2 2 0 0 0 2-2v-5a2 2 0 0 1 2-2 2 2 0 0 1-2-2V5a2 2 0 0 0-2-2h-1"/>',
  eye: '<path d="M2 12s3.5-7 10-7 10 7 10 7-3.5 7-10 7S2 12 2 12z"/><circle cx="12" cy="12" r="3"/>',
  devices: '<path d="M4 3h9v18H4zM15 7h5v14h-5z"/>',
  plus: '<path d="M12 5v14M5 12h14"/>',
  spark: '<path d="M12 3v4M12 17v4M3 12h4M17 12h4M6 6l2.5 2.5M15.5 15.5 18 18M6 18l2.5-2.5M15.5 8.5 18 6"/>'
};
var ic = function (n) { return '<svg class="i" viewBox="0 0 24 24">' + ICON[n] + '</svg>'; };
function now() { var d = new Date(); return ('0' + d.getHours()).slice(-2) + ':' + ('0' + d.getMinutes()).slice(-2) + ':' + ('0' + d.getSeconds()).slice(-2); }
function hm(t) { var d = new Date(t * 1000); return ('0' + d.getHours()).slice(-2) + ':' + ('0' + d.getMinutes()).slice(-2); }
function dayOf(t) {
  var d = new Date(t * 1000), today = new Date(); today.setHours(0, 0, 0, 0);
  var diff = Math.round((today - new Date(d.getFullYear(), d.getMonth(), d.getDate())) / 864e5);
  return diff === 0 ? 'Today' : diff === 1 ? 'Yesterday' : d.toLocaleDateString([], { month: 'short', day: 'numeric' });
}
var store = { get: function (k, d) { try { var v = localStorage.getItem('ds.' + k); return v == null ? d : JSON.parse(v); } catch (e) { return d; } },
              set: function (k, v) { try { localStorage.setItem('ds.' + k, JSON.stringify(v)); } catch (e) {} } };
var tt; function toast(t) { var el = $('#toast'); el.textContent = t; el.classList.add('show'); clearTimeout(tt); tt = setTimeout(function () { el.classList.remove('show'); }, 1800); }
function typing(e) { var t = e.target; return t && (t.tagName === 'INPUT' || t.tagName === 'TEXTAREA' || t.tagName === 'SELECT' || t.isContentEditable); }

// ── Devices (context/devices.md). Portrait only; each is the portrait of an engine profile. ──
var DEVICES = [
  { id: 'ws35', name: 'Waveshare 3.5″', w: 320, h: 480 },
  { id: 'ws43', name: 'Waveshare 4.3″', w: 480, h: 800 },
  { id: 'gt43', name: 'Guition 4.3″', w: 480, h: 800 },
  { id: 'p720', name: '720p panel', w: 720, h: 1280 }
];
function dev(id) { return DEVICES.filter(function (d) { return d.id === id; })[0] || null; }
function gcd(a, b) { return b ? gcd(b, a % b) : a; }
function aspect(d) { var g = gcd(d.w, d.h); return d.w / g + ':' + d.h / g; }
function px(d) { return d.w / 320; }          // device pixels per point
function fmt(n) { return String(Math.round(n * 100) / 100); }

// ── State ───────────────────────────────────────────────────────────────────
var q = new URLSearchParams(location.search);
var S = {
  mode: q.get('mode') === 'proto' ? 'proto' : 'design', tool: 'select',
  canvasDev: dev(q.get('c')) ? q.get('c') : 'ws35', k: 0.5, tx: 0, ty: 0, fitted: false,
  proto: null, tokens: null, viewProto: null, shots: {}, shotDev: null,
  sel: null, hover: null, leftTab: 'screens', rightTab: 'design', showHistory: false, viewing: null,
  jsonScope: 'sel', undo: [], redo: [], edited: {}, comments: [], history: [], versionsFilter: 'all',
  activeComment: null, hideResolved: false, justSent: 0, composer: null, pcomposer: null,
  readonly: true, problems: {}, uiHidden: false,
  slots: readSlots(), playPath: decodeURIComponent((location.hash || '').slice(1)), pmode: 'interact',
  fps: store.get('fps', 60), outlines: {}, frames: {}
};
var LAYOUT = {};

function readSlots() {
  if (!q.has('d1') && !q.has('d2')) return ['ws35', 'ws43'];
  return [q.get('d1'), q.get('d2')].map(function (id) { return dev(id) ? id : ''; });
}
function writeUrl() {
  var u = new URLSearchParams(location.search);
  u.set('d1', S.slots[0] || ''); u.set('d2', S.slots[1] || ''); u.set('c', S.canvasDev);
  if (S.mode === 'proto') u.set('mode', 'proto'); else u.delete('mode');
  try { history.replaceState(null, '', '?' + u.toString() + (S.playPath ? '#' + S.playPath : '')); } catch (e) {}
}

// ── The design data ─────────────────────────────────────────────────────────
function P() { return S.viewProto || S.proto || { screens: {} }; }
function screenIds() { return Object.keys(P().screens || {}); }
function meta(id) {
  var s = (P().screens || {})[id] || {};
  return { id: id, title: (s.navbar && s.navbar.title) || id, type: s.type || '' };
}
function flows() {
  var ids = screenIds(), used = {}, out = [];
  (P().flows || []).forEach(function (f) {
    var scr = (f.screens || []).filter(function (id) { return ids.indexOf(id) >= 0; });
    scr.forEach(function (id) { used[id] = true; });
    out.push({ id: f.id, title: f.title || f.id, sub: f.description || '', screens: scr });
  });
  var rest = ids.filter(function (id) { return !used[id]; });
  if (rest.length) out.push({ id: '_ungrouped', title: 'Ungrouped', sub: 'Screens in no flow. Add them to "flows" in prototype.json.', screens: rest });
  return out;
}
function flowProblems() {
  var ids = screenIds(), out = [];
  (P().flows || []).forEach(function (f) {
    (f.screens || []).forEach(function (id) { if (ids.indexOf(id) < 0) out.push('Flow “' + (f.title || f.id) + '” lists “' + id + '”, which isn’t a screen.'); });
  });
  return out;
}
function flowOf(id) { var f = flows(); for (var i = 0; i < f.length; i++) if (f[i].screens.indexOf(id) >= 0) return f[i]; return null; }

// Inspector paths ("home.body[0][1]") → JSON paths into prototype.json.
function jpath(p) {
  var m = /^([a-z0-9_]+)\.(navbar|body|actions)((?:\[\d+\])*)$/.exec(p || '');
  if (!m) return null;
  var out = ['screens', m[1], m[2]];
  (m[3].match(/\d+/g) || []).forEach(function (n, i) { if (i > 0) out.push('children'); out.push(Number(n)); });
  return out;
}
function getAt(data, path) {
  for (var i = 0; i < path.length; i++) {
    if (data == null || typeof data !== 'object' || !(path[i] in data)) return { found: false };
    data = data[path[i]];
  }
  return { found: true, value: data };
}
function getNode(ipath) { var p = jpath(ipath); if (!p) return null; var r = getAt(S.proto, p); return r.found ? r.value : null; }
function setAt(data, path, value, del) {
  for (var i = 0; i < path.length - 1; i++) data = data[path[i]];
  if (del) { if (Array.isArray(data)) data.splice(path[path.length - 1], 1); else delete data[path[path.length - 1]]; }
  else data[path[path.length - 1]] = value;
}
function nodeText(n) { return n ? (n.label || n.title || n.fp || n.text || n.badge || '') : ''; }

// Parts on a screen, from the renderer, in points: [{x,y,w,h,component,path,tokens,parents,…}]
function boxes(screen) {
  var shot = S.shots[screen]; if (!shot) return [];
  var k = px(dev(S.shotDev));
  return shot.boxes.map(function (b) { return Object.assign({}, b, { x: b.x / k, y: b.y / k, w: b.w / k, h: b.h / k }); });
}
function hit(list, x, y) {
  var best = null;
  list.forEach(function (b) {
    if (b.component === 'screen' || /^shell\./.test(b.component)) return;
    if (x < b.x || y < b.y || x > b.x + b.w || y > b.y + b.h) return;
    if (!best || b.w * b.h < best.w * best.h) best = b;
  });
  return best;
}
function boxByPath(list, path) { return path ? list.filter(function (b) { return b.path === path; })[0] || null : null; }
function artH() { var d = dev(S.canvasDev); return 320 * d.h / d.w; }

// ── Server ──────────────────────────────────────────────────────────────────
function api(method, url, body) {
  return fetch(url, { method: method, headers: body ? { 'Content-Type': 'application/json' } : {}, body: body ? JSON.stringify(body) : undefined, cache: 'no-store' })
    .then(function (r) { return r.json().then(function (j) { if (!r.ok) throw new Error(j.error || r.status); return j; }); });
}
function loadData() {
  return Promise.all(['prototype', 'tokens'].map(function (n) {
    return fetch('data/' + n + '.json?_=' + Date.now(), { cache: 'no-store' }).then(function (r) { return r.json(); });
  })).then(function (d) { S.proto = d[0]; S.tokens = d[1]; });
}
function loadComments() { if (S.readonly) return Promise.resolve(); return api('GET', '/api/comments').then(function (db) { S.comments = db.comments || []; }); }
function loadHistory() { if (S.readonly) return Promise.resolve(); return api('GET', '/api/history').then(function (h) { S.history = h; }); }

// ── Edits, undo, redo ───────────────────────────────────────────────────────
// Each edit: ops with `expect` (what the studio believes is there now), plus inverse ops, so
// undo can't silently overwrite a change someone made since (the server refuses instead).
function edit(file, changes, label, screen) {
  if (S.readonly) return toast('View only. Run the studio on your Mac to edit.');
  if (S.viewing) return toast('You’re looking at an old version. Restore it or go back to current first.');
  var data = file === 'tokens' ? S.tokens : S.proto, ops = [], inv = [];
  changes.forEach(function (c) {
    var cur = getAt(data, c.path);
    var op = { path: c.path }, back = { path: c.path };
    if (cur.found) op.expect = cur.value; else op.expect_missing = true;
    if (c.del) op['delete'] = true; else op.value = c.value;
    if (c.del) back.expect_missing = true; else back.expect = c.value;
    if (cur.found) back.value = cur.value; else back['delete'] = true;
    ops.push(op); inv.unshift(back);
  });
  var entry = { file: file, label: label, ops: ops, inv: inv, screen: screen };
  return send(file, ops, label).then(function () {
    S.undo.push(entry); S.redo = []; if (screen) S.edited[screen] = true;
    ops.forEach(function (o) { setAt(data, o.path, o.value, o['delete']); });
    savedNow(); renderAll(); toast(label);
  });
}
function send(file, ops, label) {
  return api('POST', '/api/edit', { file: file, ops: ops, label: label }).catch(function (e) { toast(String(e.message)); throw e; });
}
function undo() {
  var e = S.undo[S.undo.length - 1]; if (!e) return toast('Nothing to undo');
  send(e.file, e.inv, 'Undo: ' + e.label).then(function () {
    S.undo.pop(); S.redo.push(e); toast('Undid: ' + e.label);
    return loadData().then(renderAll);
  }, function () { S.undo.pop(); toast('Can’t undo: this was changed outside the studio. Use version history.'); updateTop(); });
}
function redo() {
  var e = S.redo[S.redo.length - 1]; if (!e) return toast('Nothing to redo');
  send(e.file, e.ops, e.label).then(function () {
    S.redo.pop(); S.undo.push(e); toast('Redid: ' + e.label);
    return loadData().then(renderAll);
  }, function () { S.redo.pop(); updateTop(); });
}
function savedNow() { $('#saved').textContent = 'Saved ' + now().slice(0, 5); }

// ── Body classes, top bar ───────────────────────────────────────────────────
function applyBody() {
  document.body.className = ['mode-' + S.mode, S.uiHidden ? 'ui-hidden' : '', S.hideResolved ? 'hide-resolved' : ''].join(' ');
  $$('#mode-seg [data-mode]').forEach(function (b) { b.classList.toggle('on', b.dataset.mode === S.mode); });
}
function tb(id, label, key) { return '<button class="tool' + (S.tool === id ? ' on' : '') + '" data-tool="' + id + '" title="' + label + ' (' + key + ')" aria-pressed="' + (S.tool === id) + '">' + ic(id) + '</button>'; }
function renderCenter() {
  if (S.mode === 'proto') { $('#center').innerHTML = ''; return; }
  $('#center').innerHTML = '<div class="tools" role="toolbar">' + tb('select', 'Select', 'V') + tb('hand', 'Hand', 'H') + (S.readonly ? '' : tb('comment', 'Comment', 'C')) + '</div>' +
    '<span class="sep"></span><select class="select" id="canvas-dev" title="Device size on the canvas">' +
    DEVICES.map(function (d) { return '<option value="' + d.id + '"' + (d.id === S.canvasDev ? ' selected' : '') + '>' + esc(d.name) + ' · ' + d.w + ' × ' + d.h + '</option>'; }).join('') + '</select>';
  $$('#center [data-tool]').forEach(function (b) { b.onclick = function () { setTool(b.dataset.tool); }; });
  $('#canvas-dev').onchange = function () { S.canvasDev = this.value; writeUrl(); S.fitted = false; buildSnap(); };
}
function updateTop() {
  var u = S.undo[S.undo.length - 1], r = S.redo[S.redo.length - 1];
  $('#undo').disabled = !u || S.readonly; $('#redo').disabled = !r || S.readonly;
  $('#undo').title = u ? 'Undo ' + u.label + ' (⌘Z)' : 'Nothing to undo';
  $('#redo').title = r ? 'Redo ' + r.label + ' (⇧⌘Z)' : 'Nothing to redo';
  $('#hist-btn').hidden = S.readonly;
  $('#hist-btn').classList.toggle('on', S.showHistory);
  var n = S.comments.filter(function (c) { return c.status === 'draft'; }).length;
  $('#unsent-btn').hidden = !n; $('#unsent-n').textContent = n;
}
function setTool(t) {
  S.tool = t; S.composer = null; S.hover = null;
  $('#canvas').className = 'tool-' + t + (S.viewing ? ' viewing' : '');
  renderCenter(); drawOverlay();
}
function setMode(m) {
  if (m === S.mode) return;
  S.mode = m; S.composer = null; S.pcomposer = null;
  if (m === 'proto' && S.sel) S.playPath = pathTo(S.sel.screen);
  writeUrl(); applyBody(); renderAll();
  if (m === 'proto') buildDevices(); else { destroyDevices(); setTimeout(applyView, 0); }
}
function pathTo(id) { var start = (S.proto && S.proto.start) || 'home'; return id === start ? start : start + '/' + id; }

// ── Canvas: the renderer (hidden engine) ────────────────────────────────────
var snapFrame = $('#snap-host');
function buildSnap() {
  var d = dev(S.canvasDev);
  $('#loading').hidden = false; $('#loading').textContent = 'Rendering screens…';
  snapFrame.src = 'device.html?snap=1&w=' + d.w + '&h=' + d.h + '&name=snap&_=' + Date.now();
}
function snapAll() {
  if (!snapFrame.contentWindow) return;
  snapFrame.contentWindow.postMessage({ src: 'rd-studio', cmd: 'snap', ids: screenIds() }, '*');
}

// ── Canvas: layout, view, overlay ───────────────────────────────────────────
var ART_GAP = 56, ROW_GAP = 110, HEAD = 200;
function layoutWorld() {
  var y = 0, html = '', h = artH();
  LAYOUT = {};
  flows().forEach(function (f) {
    html += '<div class="sect" style="left:0;top:' + y + 'px"><h3>' + esc(f.title) + '</h3><p>' + esc(f.sub) + '</p></div>';
    var ay = y + HEAD;
    f.screens.forEach(function (id, i) {
      var x = i * (320 + ART_GAP), m = meta(id), shot = S.shots[id];
      LAYOUT[id] = { x: x, y: ay, w: 320, h: h };
      html += '<div class="art" data-id="' + id + '" style="left:' + x + 'px;top:' + ay + 'px;width:320px;height:' + h + 'px">' +
        '<div class="lbl"><span class="nm">' + esc(m.title) + ' <span class="ty mono">' + id + '</span></span><span class="ty">' + esc(m.type.replace(/_/g, ' ')) + '</span></div>' +
        '<div class="frame">' + (shot ? '<img alt="" src="' + shot.url + '">' : '') + '</div></div>';
    });
    y = ay + h + ROW_GAP;
  });
  $('#world').innerHTML = html;
}
function worldBounds() {
  var maxX = 0, maxY = 0;
  Object.keys(LAYOUT).forEach(function (k) { var a = LAYOUT[k]; maxX = Math.max(maxX, a.x + a.w); maxY = Math.max(maxY, a.y + a.h); });
  return { w: maxX || 1, h: maxY || 1 };
}
function fit() {
  var b = worldBounds(), c = $('#canvas').getBoundingClientRect(), pad = 64;
  if (!c.width) return;
  S.k = Math.max(0.08, Math.min(1, (c.width - pad * 2) / b.w, (c.height - pad * 2) / b.h));
  S.tx = (c.width - b.w * S.k) / 2; S.ty = Math.max(pad, (c.height - b.h * S.k) / 2);
  applyView();
}
function zoomTo(k, cx, cy) {
  var c = $('#canvas').getBoundingClientRect();
  if (cx == null) { cx = c.width / 2; cy = c.height / 2; }
  k = Math.max(0.08, Math.min(4, k));
  var wx = (cx - S.tx) / S.k, wy = (cy - S.ty) / S.k;
  S.k = k; S.tx = cx - wx * k; S.ty = cy - wy * k; applyView();
}
function focusScreen(id, zoom) {
  var a = LAYOUT[id]; if (!a) return; var c = $('#canvas').getBoundingClientRect();
  if (zoom) S.k = Math.min(1.25, (c.height - 120) / a.h);
  S.tx = c.width / 2 - (a.x + a.w / 2) * S.k; S.ty = c.height / 2 - (a.y + a.h / 2) * S.k; applyView();
}
function applyView() {
  var w = $('#world'); w.style.transform = 'translate(' + S.tx + 'px,' + S.ty + 'px) scale(' + S.k + ')';
  w.style.setProperty('--inv', 1 / S.k);
  w.classList.toggle('far', S.k < 0.34); w.classList.toggle('mid', S.k < 0.6);
  $('#zpct').textContent = Math.round(S.k * 100) + '%';
  drawOverlay();
}
function toScreen(x, y) { return { x: x * S.k + S.tx, y: y * S.k + S.ty }; }
function toWorld(ev) { var r = $('#canvas').getBoundingClientRect(); return { x: (ev.clientX - r.left - S.tx) / S.k, y: (ev.clientY - r.top - S.ty) / S.k }; }
function artAt(p) { for (var id in LAYOUT) { var a = LAYOUT[id]; if (p.x >= a.x && p.x <= a.x + a.w && p.y >= a.y && p.y <= a.y + a.h) return id; } return null; }
function boxScreen(screen, b) { var a = LAYOUT[screen], p = toScreen(a.x + b.x, a.y + b.y); return { x: p.x, y: p.y, w: b.w * S.k, h: b.h * S.k }; }
// Where a comment's pin sits on a screen whose parts are `list`: on its part when it has one.
function pinPos(c, list, height) {
  var b = boxByPath(list, c.path);
  if (b && c.dx != null) return { x: b.x + c.dx, y: b.y + c.dy };
  return { x: c.x, y: Math.min(c.y, height - 4) };
}

function drawOverlay() {
  if (S.mode !== 'design') return;
  var html = '';
  $$('.art').forEach(function (el) {
    var id = el.dataset.id;
    el.classList.toggle('on', !!S.sel && S.sel.screen === id && !S.sel.comp);
    el.classList.toggle('edited', !!S.edited[id]);
  });
  if (S.hover && S.tool === 'select' && LAYOUT[S.hover.screen] && !(S.sel && S.hover.key === S.sel.key)) {
    var hb = boxScreen(S.hover.screen, S.hover.box);
    html += '<div class="hbox" style="left:' + hb.x + 'px;top:' + hb.y + 'px;width:' + hb.w + 'px;height:' + hb.h + 'px"></div>';
  }
  if (S.sel && LAYOUT[S.sel.screen]) {
    var b = (S.sel.path && boxByPath(boxes(S.sel.screen), S.sel.path)) || S.sel.box || { x: 0, y: 0, w: 320, h: artH() }, sb = boxScreen(S.sel.screen, b);
    var tag = S.sel.comp ? S.sel.comp.split(' ')[0] + ' · ' + Math.round(b.w) + ' × ' + Math.round(b.h) : meta(S.sel.screen).title + ' · 320 × ' + Math.round(artH());
    html += '<div class="sbox" style="left:' + sb.x + 'px;top:' + sb.y + 'px;width:' + sb.w + 'px;height:' + sb.h + 'px"><span class="tag">' + esc(tag) + ' pt</span>' +
      [[-4, -4], [sb.w - 3, -4], [-4, sb.h - 3], [sb.w - 3, sb.h - 3]].map(function (q) { return '<i class="hd" style="left:' + q[0] + 'px;top:' + q[1] + 'px"></i>'; }).join('') + '</div>';
  }
  if (!S.viewing) S.comments.forEach(function (c) {
    if (!LAYOUT[c.screen]) return;
    var a = LAYOUT[c.screen], pp = pinPos(c, boxes(c.screen), a.h), p = toScreen(a.x + pp.x, a.y + pp.y);
    html += '<button class="pin ' + c.status + (S.activeComment === c.id ? ' on' : '') + '" data-c="' + c.id + '" style="left:' + p.x + 'px;top:' + p.y + 'px" title="' + esc(c.thread[0].text) + '">' + c.id + '</button>';
  });
  if (S.composer && LAYOUT[S.composer.screen]) {
    var ca = LAYOUT[S.composer.screen], cp = toScreen(ca.x + S.composer.x, ca.y + S.composer.y);
    html += '<span class="pin" style="left:' + cp.x + 'px;top:' + cp.y + 'px">+</span>' + composerHtml(cp.x + 30, cp.y - 24, meta(S.composer.screen).title, S.composer.part, 'ctext', 'Make the fee line easier to read');
  }
  var draft = $('#ctext'), keep = draft ? draft.value : '';   // a redraw keeps what's been typed
  $('#ov').innerHTML = html;
  if (keep && $('#ctext')) $('#ctext').value = keep;
  $$('#ov .pin[data-c]').forEach(function (p) {
    p.onclick = function (e) { e.stopPropagation(); openComment(+p.dataset.c); };
    p.onpointerdown = function (e) { e.stopPropagation(); };
  });
  if (S.composer) wireComposer('ctext', function () { S.composer = null; drawOverlay(); }, addCanvasComment);
}
function composerHtml(x, y, title, part, id, placeholder) {
  return '<div class="pop" style="left:' + x + 'px;top:' + y + 'px"><div class="note">' + esc(title) + (part ? ' · ' + esc(part.split(' ')[0]) : '') + '</div>' +
    '<textarea class="input" id="' + id + '" placeholder="' + esc(placeholder) + '"></textarea>' +
    '<div class="row"><span class="note">Saved here until you send</span><span style="display:flex;gap:6px"><button class="btn ghost sm" data-cancel>Cancel</button><button class="btn primary sm" data-add>Comment</button></span></div></div>';
}
function wireComposer(id, cancel, add) {
  var ta = $('#' + id); if (!ta) return;
  var pop = ta.closest('.pop');
  if (document.activeElement !== ta) ta.focus();
  ta.onkeydown = function (e) { e.stopPropagation(); if (e.key === 'Enter' && (e.metaKey || e.ctrlKey)) add(); if (e.key === 'Escape') cancel(); };
  $('[data-cancel]', pop).onclick = cancel;
  $('[data-add]', pop).onclick = add;
  pop.onpointerdown = function (e) { e.stopPropagation(); };
  pop.onclick = function (e) { e.stopPropagation(); };
}
function select(sel) {
  S.sel = sel; S.composer = null;
  if (sel) S.sel.key = sel.screen + '|' + (sel.path || sel.comp || '');
  renderAll();
}

// ── Canvas pointer ──────────────────────────────────────────────────────────
(function () {
  var cv = $('#canvas'), drag = null, space = false;
  document.addEventListener('keydown', function (e) { if (e.code === 'Space' && !typing(e) && S.mode === 'design') { space = true; cv.classList.add('panning'); e.preventDefault(); } });
  document.addEventListener('keyup', function (e) { if (e.code === 'Space') { space = false; cv.classList.remove('panning'); } });
  cv.addEventListener('pointerdown', function (e) {
    if (e.target.closest('#zoombar, #vbanner, .pop, .pin, .inline-edit')) return;
    if (space || S.tool === 'hand' || e.button === 1) { drag = { x: e.clientX, y: e.clientY, tx: S.tx, ty: S.ty }; cv.classList.add('dragging'); cv.setPointerCapture(e.pointerId); return; }
    var p = toWorld(e), id = artAt(p);
    if (S.tool === 'comment' && !S.viewing) {
      e.preventDefault();   // keep focus for the composer that opens now
      if (!id) { S.composer = null; drawOverlay(); return; }
      var a = LAYOUT[id], b = hit(boxes(id), p.x - a.x, p.y - a.y);
      S.composer = { screen: id, x: p.x - a.x, y: p.y - a.y, part: b && b.component, path: b && b.path, box: b, tokens: b && b.tokens };
      drawOverlay(); return;
    }
    var lbl = e.target.closest('.art .lbl');
    if (lbl) { select({ screen: lbl.parentNode.dataset.id }); return; }
    if (!id) { select(null); return; }
    var a2 = LAYOUT[id], hb = hit(boxes(id), p.x - a2.x, p.y - a2.y);
    select(hb ? { screen: id, path: hb.path, comp: hb.component, box: hb } : { screen: id });
  });
  cv.addEventListener('pointermove', function (e) {
    if (drag) { S.tx = drag.tx + e.clientX - drag.x; S.ty = drag.ty + e.clientY - drag.y; applyView(); return; }
    if (S.tool !== 'select' || space) { if (S.hover) { S.hover = null; drawOverlay(); } return; }
    var p = toWorld(e), id = artAt(p), h = null;
    if (id) { var a = LAYOUT[id], b = hit(boxes(id), p.x - a.x, p.y - a.y); h = { screen: id, box: b || { x: 0, y: 0, w: 320, h: a.h }, key: id + '|' + (b ? (b.path || b.component) : '') }; }
    if ((h && h.key) !== (S.hover && S.hover.key)) { S.hover = h; drawOverlay(); }
  });
  cv.addEventListener('pointerup', function () { drag = null; cv.classList.remove('dragging'); });
  cv.addEventListener('pointerleave', function () { if (S.hover) { S.hover = null; drawOverlay(); } });
  cv.addEventListener('dblclick', function (e) {
    if (S.tool !== 'select' || S.viewing || S.readonly) return;
    var p = toWorld(e), id = artAt(p); if (!id) return;
    var a = LAYOUT[id], b = hit(boxes(id), p.x - a.x, p.y - a.y); if (!b || !b.path) return;
    var n = getNode(b.path), key = n && ['label', 'title', 'text', 'sub'].filter(function (k) { return typeof n[k] === 'string'; })[0];
    if (!key) return toast('Nothing to type here. Use the Design tab.');
    var sb = boxScreen(id, b), inp = document.createElement('input');
    inp.className = 'inline-edit'; inp.value = n[key];
    inp.style.cssText = 'left:' + sb.x + 'px;top:' + (sb.y + sb.h / 2 - 16) + 'px;width:' + Math.max(160, sb.w) + 'px;height:32px';
    $('#ov').appendChild(inp); inp.focus(); inp.select();
    var done = false;
    function commit(save) {
      if (done) return; done = true; var v = inp.value; inp.remove();
      if (save && v !== n[key]) edit('prototype', [{ path: jpath(b.path).concat(key), value: v }], 'Changed text to “' + v + '”', id);
    }
    inp.onkeydown = function (ev) { ev.stopPropagation(); if (ev.key === 'Enter') commit(true); if (ev.key === 'Escape') commit(false); };
    inp.onblur = function () { commit(true); };
  });
  cv.addEventListener('wheel', function (e) {
    e.preventDefault();
    var r = cv.getBoundingClientRect();
    if (e.ctrlKey || e.metaKey) zoomTo(S.k * Math.exp(-e.deltaY * 0.01), e.clientX - r.left, e.clientY - r.top);
    else { S.tx -= e.deltaX; S.ty -= e.deltaY; applyView(); }
  }, { passive: false });
})();

// ── Comments ────────────────────────────────────────────────────────────────
function commentBody(screen, x, y, b, part, path, size, text) {
  return { action: 'create', screen: screen, x: x, y: y, dx: b ? x - b.x : null, dy: b ? y - b.y : null,
           part: part || null, path: path || null, tokens: (b && b.tokens) || null, size: size, text: text };
}
function addCanvasComment() {
  var t = ($('#ctext').value || '').trim(); if (!t) { $('#ctext').focus(); return; }
  var c = S.composer, d = dev(S.canvasDev);
  api('POST', '/api/comments', commentBody(c.screen, c.x, c.y, c.box, c.part, c.path, d.w + 'x' + d.h, t)).then(function (db) {
    S.comments = db.comments; S.composer = null; S.rightTab = 'comments'; S.showHistory = false; renderAll(); toast('Comment saved. Send when you’re ready.');
  }, function (e) { toast(String(e.message)); });
}
function openComment(id) {
  var c = S.comments.filter(function (x) { return x.id === id; })[0]; if (!c) return;
  S.activeComment = id; S.rightTab = 'comments'; S.showHistory = false;
  if (S.mode === 'design') focusScreen(c.screen);
  else { S.playPath = pathTo(c.screen); sendAll({ cmd: 'show', path: S.playPath }); }
  renderAll();
}
// The picture that goes with a sent comment: its screen with the pin drawn on it.
function commentSnapshot(c) {
  var shot = S.shots[c.screen]; if (!shot) return Promise.resolve(null);
  return new Promise(function (resolve) {
    var img = new Image();
    img.onload = function () {
      var cv = document.createElement('canvas'); cv.width = img.width; cv.height = img.height;
      var g = cv.getContext('2d'), k = img.width / 320, pp = pinPos(c, boxes(c.screen), img.height / k);
      g.drawImage(img, 0, 0);
      var x = pp.x * k, y = pp.y * k, r = 12 * k;
      g.fillStyle = getComputedStyle(document.documentElement).getPropertyValue('--pin').trim();
      g.beginPath(); g.moveTo(x, y); g.lineTo(x, y - r); g.arc(x + r, y - r, r, Math.PI, Math.PI / 2, false); g.closePath(); g.fill();
      g.fillStyle = getComputedStyle(document.documentElement).getPropertyValue('--pin-ink').trim();
      g.font = '700 ' + Math.round(11 * k) + 'px sans-serif'; g.textAlign = 'center'; g.textBaseline = 'middle';
      g.fillText(String(c.id), x + r, y - r);
      resolve(cv.toDataURL('image/png'));
    };
    img.src = shot.url;
  });
}
function sendComments() {
  var list = S.comments.filter(function (c) { return c.status === 'draft'; });
  if (!list.length) return;
  Promise.all(list.map(commentSnapshot)).then(function (pngs) {
    var snaps = {}; list.forEach(function (c, i) { if (pngs[i]) snaps[c.id] = pngs[i]; });
    return api('POST', '/api/comments', { action: 'send', snapshots: snaps });
  }).then(function (db) {
    S.comments = db.comments; S.justSent = list.length; renderAll();
    toast('Sent ' + list.length + ' comment' + (list.length > 1 ? 's' : '') + ' to the agent queue');
  }, function (e) { toast(String(e.message)); });
}
function commentAction(body) { return api('POST', '/api/comments', body).then(function (db) { S.comments = db.comments; renderAll(); }, function (e) { toast(String(e.message)); }); }

// ── Left panel ──────────────────────────────────────────────────────────────
function curScreen() { return S.mode === 'proto' ? S.playPath.split('/').pop() || (S.proto && S.proto.start) : S.sel && S.sel.screen; }
function renderLeft() {
  $$('#ltabs button').forEach(function (b) { b.classList.toggle('on', b.dataset.tab === S.leftTab); b.onclick = function () { S.leftTab = b.dataset.tab; renderLeft(); }; });
  var body = $('#lbody');
  if (!S.proto) { body.innerHTML = ''; return; }
  body.innerHTML = S.leftTab === 'screens' ? screensTree() : S.leftTab === 'layers' ? layersTree() : tokensPanel();
  wireLeft();
}
function screensTree() {
  var cur = curScreen();
  return '<div class="tree">' + flows().map(function (f) {
    return '<div class="grp"><div class="grph">' + ic('chev') + esc(f.title) + '<span class="n">' + f.screens.length + '</span></div><div class="rows">' +
      f.screens.map(function (id) {
        var m = meta(id), n = S.comments.filter(function (c) { return c.screen === id && c.status !== 'resolved'; }).length;
        return '<div class="trow' + (id === cur ? ' on' : '') + (S.edited[id] ? ' edited' : '') + '" data-screen="' + id + '"><span class="nm">' + esc(m.title) + '</span>' +
          (n ? '<span class="badge">' + n + '</span>' : '<span class="ty">' + esc(m.type.replace(/_/g, ' ')) + '</span>') + '</div>';
      }).join('') + '</div></div>';
  }).join('') + '</div>';
}
function sortKey(path) { var p = jpath(path); return [{ navbar: 0, body: 1, actions: 2 }[p[2]]].concat(p.slice(3).filter(function (x) { return typeof x === 'number'; })); }
function cmpKey(a, b) { for (var i = 0; i < Math.max(a.length, b.length); i++) { var x = a[i] == null ? -1 : a[i], y = b[i] == null ? -1 : b[i]; if (x !== y) return x - y; } return 0; }
function layersTree() {
  var sc = curScreen();
  if (!sc) return '<div class="empty"><b>No screen selected</b><span>Click a screen on the canvas to see its layers.</span></div>';
  var list = (S.mode === 'proto' ? liveBoxes() : boxes(sc)).filter(function (b) { return b.path && jpath(b.path); });
  list.sort(function (a, b) { return cmpKey(sortKey(a.path), sortKey(b.path)); });
  var selPath = S.sel && S.sel.path, m = meta(sc);
  return '<div class="tree"><div class="grph" style="cursor:default">' + esc(m.title) + '<span class="n">' + esc(m.type.replace(/_/g, ' ')) + '</span></div>' +
    list.map(function (b) {
      var depth = jpath(b.path).filter(function (x) { return typeof x === 'number'; }).length;
      return '<div class="trow' + (b.path === selPath ? ' on' : '') + '" data-layer="' + esc(b.path) + '" data-screen="' + sc + '" style="padding-left:' + (14 + depth * 14) + 'px">' +
        '<span class="nm">' + esc(b.component.split(' ')[0]) + '</span><span class="ty">' + esc(nodeText(getNode(b.path))) + '</span></div>';
    }).join('') + '<p class="note" style="padding:10px 6px">Reordering, duplicating and adding parts come later. For now, use the JSON tab.</p></div>';
}
function tokensPanel() {
  var t = S.tokens, out = '', ro = S.readonly ? ' readonly' : '';
  function head(g, small) { return '<div class="sech">' + g[0].toUpperCase() + g.slice(1) + ' <small>' + small + (S.readonly ? '' : ' <button class="btn ghost sm" data-addtok="' + g + '">' + ic('plus') + 'New</button>') + '</small></div>'; }
  out += '<div class="sec">' + head('color', 'every screen updates') + Object.keys(t.color).filter(function (k) { return k[0] !== '_'; }).map(function (k) {
    return '<div class="field" data-tokrow="color.' + k + '"><label class="mono">' + esc(k) + '</label><span style="display:flex;gap:6px;align-items:center"><input type="color" value="' + esc(String(t.color[k]).toLowerCase()) + '" data-tok="color.' + k + '" style="width:28px;height:24px;border:0;background:none;padding:0"' + (S.readonly ? ' disabled' : '') + '><input class="input mono" value="' + esc(t.color[k]) + '" data-tok="color.' + k + '"' + ro + '></span></div>';
  }).join('') + '</div>';
  ['space', 'radius', 'metric', 'icon', 'opacity'].forEach(function (g) {
    if (!t[g]) return;
    out += '<div class="sec">' + head(g, g === 'opacity' ? '0 to 1' : 'pt') + Object.keys(t[g]).filter(function (k) { return k[0] !== '_'; }).map(function (k) {
      return '<div class="field" data-tokrow="' + g + '.' + k + '"><label class="mono">' + esc(k) + '</label><input class="input mono" type="number" step="any" value="' + t[g][k] + '" data-tok="' + g + '.' + k + '"' + ro + '></div>';
    }).join('') + '</div>';
  });
  out += '<div class="sec"><div class="sech">Type <small>size / line height, pt</small></div>' + Object.keys(t.type).filter(function (k) { return k[0] !== '_'; }).map(function (k) {
    return '<div class="field" data-tokrow="type.' + k + '"><label class="mono">' + esc(k) + '</label><span style="display:flex;gap:6px"><input class="input mono" type="number" value="' + t.type[k].size + '" data-tok="type.' + k + '.size"' + ro + '><input class="input mono" type="number" value="' + t.type[k].lh + '" data-tok="type.' + k + '.lh"' + ro + '></span></div>';
  }).join('') + '</div>';
  return out;
}
function wireLeft() {
  $$('#lbody .grph').forEach(function (h) { if (h.parentNode.classList.contains('grp')) h.onclick = function () { h.parentNode.classList.toggle('closed'); }; });
  $$('#lbody .trow[data-screen]').forEach(function (r) {
    r.onclick = function () {
      var id = r.dataset.screen, lp = r.dataset.layer;
      if (lp) {
        var list = S.mode === 'proto' ? liveBoxes() : boxes(id), b = boxByPath(list, lp);
        S.sel = { screen: id, path: lp, comp: b && b.component, box: b }; S.sel.key = id + '|' + lp;
        if (S.mode === 'proto') sendAll({ cmd: 'select', path: lp });
        renderAll(); return;
      }
      if (S.mode === 'proto') { S.playPath = pathTo(id); sendAll({ cmd: 'show', path: S.playPath }); writeUrl(); renderAll(); return; }
      select({ screen: id }); focusScreen(id);
    };
    r.ondblclick = function () { if (!r.dataset.layer && S.mode === 'design') focusScreen(r.dataset.screen, true); };
  });
  $$('#lbody [data-tok]').forEach(function (inp) {
    inp.onchange = function () {
      var path = inp.dataset.tok.split('.'), v = inp.type === 'number' ? Number(inp.value) : inp.value;
      if (inp.type === 'color') v = v.toUpperCase();
      if (path[0] === 'color' && !/^#[0-9A-Fa-f]{6}$/.test(v)) { toast('Colours are #RRGGBB'); renderLeft(); return; }
      edit('tokens', [{ path: path, value: v }], 'Token ' + inp.dataset.tok + ' → ' + v);
    };
  });
  $$('#lbody [data-addtok]').forEach(function (b) { b.onclick = function () { newToken(b.dataset.addtok); }; });
}
function newToken(group) {
  dialog('New ' + group + ' token', '<label class="note">Name</label><input class="input mono" id="nt-name" placeholder="' + (group === 'color' ? 'accentSoft' : 'rowTall') + '">' +
    '<label class="note">Value</label><input class="input mono" id="nt-val" placeholder="' + (group === 'color' ? '#FF9F0A' : '52') + '">' +
    '<p class="note">Then pick it where it’s needed, or comment to ask the agent to use it.</p>', 'Add token', function () {
    var name = $('#nt-name').value.trim(), raw = $('#nt-val').value.trim();
    if (!/^[A-Za-z][A-Za-z0-9]*$/.test(name)) { toast('Use letters and digits, like rowTall'); return false; }
    if (S.tokens[group][name] !== undefined) { toast('“' + name + '” already exists'); return false; }
    var v = group === 'color' ? raw.toUpperCase() : Number(raw);
    if (group === 'color' ? !/^#[0-9A-F]{6}$/.test(v) : !isFinite(v) || raw === '') { toast(group === 'color' ? 'Colours are #RRGGBB' : 'Enter a number'); return false; }
    edit('tokens', [{ path: [group, name], value: v }], 'New token ' + group + '.' + name);
  });
}

// ── Right panel ─────────────────────────────────────────────────────────────
function renderRight() {
  var r = $('#right');
  // Don't rebuild the JSON editor under someone typing in it (their own save comes back as an update).
  if (document.activeElement && document.activeElement.id === 'code' && S.rightTab === 'json' && !S.showHistory) return;
  if (S.showHistory && !S.readonly) { r.innerHTML = historyPanel(); wireHistory(); return; }
  var unsent = S.comments.filter(function (c) { return c.status === 'draft'; }).length;
  var tabs = [['design', 'Design'], ['json', 'JSON']].concat(S.readonly ? [] : [['comments', 'Comments']]);
  if (S.readonly && S.rightTab === 'comments') S.rightTab = 'design';
  r.innerHTML = '<div class="ptabs">' + tabs.map(function (t) {
    return '<button data-rt="' + t[0] + '" class="' + (S.rightTab === t[0] ? 'on' : '') + '">' + t[1] + (t[0] === 'comments' && unsent ? '<span class="badge">' + unsent + '</span>' : '') + '</button>';
  }).join('') + '</div><div id="banner"></div>' + problemsHtml() + '<div id="rbody" style="flex:1;min-height:0;display:flex;flex-direction:column"></div>';
  $$('[data-rt]', r).forEach(function (b) { b.onclick = function () { S.rightTab = b.dataset.rt; renderRight(); }; });
  showBanner();
  var body = $('#rbody');
  if (!S.proto) return;
  if (S.rightTab === 'design') { body.innerHTML = '<div class="scroll" style="flex:1">' + designTab() + '</div>'; wireDesign(); }
  else if (S.rightTab === 'json') { body.innerHTML = jsonTab(); wireJson(); }
  else { body.innerHTML = commentsTab(); wireComments(); }
}
function problemsHtml() {
  var lines = [];
  Object.keys(S.problems).forEach(function (k) {
    var p = S.problems[k] || {};
    if (p.error) lines.push('<span class="e">' + esc(p.error) + '</span>');
    (p.warnings || []).forEach(function (w) { var l = '<span class="w">' + esc(w) + '</span>'; if (lines.indexOf(l) < 0) lines.push(l); });
  });
  flowProblems().forEach(function (w) { lines.push('<span class="w">' + esc(w) + '</span>'); });
  return lines.length ? '<div class="problems" aria-label="Problems">' + lines.join('') + '</div>' : '';
}
var STYLE_NUMS = ['h', 'w', 'tracking', 'weight', 'gap', 'size', 'lh', 'pad'];
function designTab() {
  var s = S.sel;
  if (!s) return '<div class="empty">' + ic('select') + '<b>Nothing selected</b><span>' + (S.mode === 'proto' ? 'Choose Inspect, then click anything on a device.' : S.readonly ? 'Click a screen or anything on it to inspect it.' : 'Click a screen or anything on it. Double-click text to edit it in place.') + '</span></div>';
  var scr = S.proto.screens[s.screen] || {}, ro = S.readonly ? ' readonly' : '', dis = S.readonly ? ' disabled' : '';
  if (!s.comp) {
    var f = flowOf(s.screen), m = meta(s.screen);
    return '<div class="selhead"><span class="t">' + esc(m.title) + '<span class="chip">' + esc(m.type.replace(/_/g, ' ')) + '</span></span><span class="p">' + s.screen + '</span></div>' +
      '<div class="sec"><div class="sech">Screen</div>' +
      field('Title', '<input class="input" data-screen-key="navbar.title" value="' + esc(scr.navbar && scr.navbar.title) + '"' + ro + '>') +
      field('Left button', pick('navbar.left', scr.navbar && scr.navbar.left || 'none', ['back', 'power', 'none'], dis)) +
      field('Type', pick('type', scr.type, ['menu_grid', 'menu_list', 'menu_bottom_list', 'input', 'review', 'dialog'], dis)) +
      field('Flow', '<input class="input" value="' + esc(f ? f.title : '') + '" readonly title="Edit flows in prototype.json for now">') +
      '</div><div class="sec"><div class="sech">Note <small>_note, never shown on the device</small></div><textarea class="input" data-screen-key="_note" rows="3"' + ro + '>' + esc(scr._note || '') + '</textarea></div>' +
      '<div class="sec"><div class="sech">Parts</div><p class="note">' + ((scr.body || []).length) + ' in the body, ' + ((scr.actions || []).length) + ' in the actions. See Layers, or edit the structure in the JSON tab.</p></div>';
  }
  var n = s.path ? getNode(s.path) : null, b = s.box || {};
  var html = '<div class="selhead"><span class="t">' + esc(s.comp.split(' ')[0]) + (b.muted ? '<span class="chip">muted</span>' : '') + '</span><span class="p">' + esc(s.path || s.comp) + '</span></div>';
  html += '<div class="sec"><div class="sech">Size <small>' + esc(meta(s.screen).title) + '</small></div><div style="display:flex;gap:6px;flex-wrap:wrap"><span class="chip">W ' + Math.round(b.w || 0) + ' pt</span><span class="chip">H ' + Math.round(b.h || 0) + ' pt</span>' + (b.responsive ? '<span class="chip">' + esc(b.responsive) + '</span>' : '') + '</div></div>';
  if (n) {
    var keys = Object.keys(n).filter(function (k) { return k !== 'type' && k !== 'children' && k[0] !== '_'; });
    var text = keys.filter(function (k) { return typeof n[k] === 'string' && k !== 'go' && k !== 'icon'; });
    var bools = keys.filter(function (k) { return typeof n[k] === 'boolean'; });
    var nums = keys.filter(function (k) { return typeof n[k] === 'number'; });
    if (text.length) html += '<div class="sec"><div class="sech">Content</div>' + text.map(function (k) { return field(k, '<input class="input" data-k="' + k + '" value="' + esc(n[k]) + '"' + ro + '>'); }).join('') + '</div>';
    if ('icon' in n || 'go' in n) {
      html += '<div class="sec"><div class="sech">Behaviour</div>';
      if ('icon' in n) html += field('Icon', '<input class="input mono" data-k="icon" value="' + esc(n.icon) + '"' + ro + ' list="icon-names">') + iconList();
      if ('go' in n) html += field('Goes to', pickGo(n.go, dis));
      html += '</div>';
    }
    if (bools.length || nums.length) html += '<div class="sec"><div class="sech">Options</div>' +
      bools.map(function (k) { return field(k, '<button class="switch' + (n[k] ? ' on' : '') + '" data-bool="' + k + '" aria-pressed="' + n[k] + '"' + dis + '></button>'); }).join('') +
      nums.map(function (k) {
        return STYLE_NUMS.indexOf(k) >= 0
          ? field(k, '<span class="chip" title="Styling comes from tokens">' + n[k] + ' · not a token</span>')
          : field(k, '<input class="input mono" type="number" step="any" data-num="' + k + '" value="' + n[k] + '"' + ro + '>');
      }).join('') +
      (nums.some(function (k) { return STYLE_NUMS.indexOf(k) >= 0; }) ? '<p class="note">Styling comes from tokens. Edit the token in Tokens, or comment to ask the agent to use one here.</p>' : '') + '</div>';
  } else {
    var up = owner(s.path);
    html += '<div class="sec"><p class="note">' + (up ? 'This is drawn by its parent, <b>' + esc(up.comp.split(' ')[0]) + '</b>, from the parent’s data. Edit it there.' : 'This part belongs to the screen’s frame (built in C), so it has no JSON of its own. Comment on it to ask the agent for a change.') + '</p>' +
      (up ? '<button class="btn sm" id="sel-parent" style="margin-top:8px">Select ' + esc(up.comp.split(' ')[0]) + '</button>' : '') + '</div>';
  }
  var tk = b.tokens || {}, tkeys = Object.keys(tk);
  if (tkeys.length) html += '<div class="sec"><div class="sech">Tokens <small>click to edit it everywhere</small></div>' + tkeys.map(function (k) {
    var tv = String(tk[k]).split(' ')[0], val = tokenValue(tv), sw = /^color\./.test(tv) && val ? '<span class="sw" style="background:' + esc(val) + '"></span>' : '';
    return '<div class="tokrow" data-goto="' + esc(tv) + '"><span class="k">' + esc(k) + '</span><span class="v">' + sw + '<span class="chip tok">' + esc(tk[k]) + '</span></span></div>';
  }).join('') + '</div>';
  if (b.parents && b.parents.length) html += '<div class="sec"><div class="sech">Inside</div><p class="note mono">' + b.parents.map(esc).join(' › ') + '</p></div>';
  return html;
}
var ICONS_SEEN = null;
function iconList() {
  if (!ICONS_SEEN) { ICONS_SEEN = {}; JSON.stringify(S.proto).replace(/"icon":"([a-z_]+)"/g, function (_, n) { ICONS_SEEN[n] = 1; }); }
  return '<datalist id="icon-names">' + Object.keys(ICONS_SEEN).sort().map(function (n) { return '<option value="' + n + '">'; }).join('') + '</datalist>';
}
function owner(path) {
  var p = path;
  while (p && /[.[]/.test(p)) {
    p = p.replace(/(\.[a-z_]+(\[\d+\])?|\[\d+\])$/, '');
    if (getNode(p)) { var list = S.mode === 'proto' ? liveBoxes() : boxes(p.split('.')[0]), b = boxByPath(list, p); return { path: p, comp: b ? b.component : getNode(p).type, box: b }; }
  }
  return null;
}
function tokenValue(path) { var o = S.tokens; path.split('.').forEach(function (k) { o = o && o[k]; }); return o && typeof o === 'object' ? (o.size ? o.size + ' / ' + o.lh : '') : o; }
function field(label, ctl) { return '<div class="field"><label>' + esc(label) + '</label>' + ctl + '</div>'; }
function pick(key, v, opts, dis) { return '<select class="select" style="width:100%" data-screen-key="' + key + '"' + dis + '>' + opts.map(function (o) { return '<option' + (o === v ? ' selected' : '') + '>' + o + '</option>'; }).join('') + '</select>'; }
function pickGo(v, dis) {
  var opts = screenIds().slice(); if (v && opts.indexOf(v) < 0) opts.unshift(v);
  return '<select class="select" style="width:100%" data-k="go"' + dis + '>' + opts.map(function (o) {
    return '<option value="' + esc(o) + '"' + (o === v ? ' selected' : '') + '>' + esc(S.proto.screens[o] ? meta(o).title + ' · ' + o : o) + '</option>';
  }).join('') + '</select>';
}
function wireDesign() {
  var s = S.sel; if (!s) return;
  $$('#rbody [data-k]').forEach(function (el) {
    el.onchange = function () { var k = el.dataset.k; edit('prototype', [{ path: jpath(s.path).concat(k), value: el.value }], 'Changed ' + k + ' to “' + el.value + '”', s.screen); };
  });
  $$('#rbody [data-bool]').forEach(function (el) {
    el.onclick = function () { var k = el.dataset.bool, n = getNode(s.path); edit('prototype', [{ path: jpath(s.path).concat(k), value: !n[k] }], 'Turned ' + k + ' ' + (n[k] ? 'off' : 'on'), s.screen); };
  });
  $$('#rbody [data-num]').forEach(function (el) {
    el.onchange = function () { var k = el.dataset.num; edit('prototype', [{ path: jpath(s.path).concat(k), value: Number(el.value) }], 'Changed ' + k + ' to ' + el.value, s.screen); };
  });
  $$('#rbody [data-screen-key]').forEach(function (el) {
    el.onchange = function () {
      var key = el.dataset.screenKey, path = ['screens', s.screen].concat(key.split('.')), v = el.value;
      var change = key === 'navbar.left' && v === 'none' ? { path: path, del: true } : key === '_note' && !v ? { path: path, del: true } : { path: path, value: v };
      edit('prototype', [change], 'Changed ' + meta(s.screen).title + ' ' + key.replace('navbar.', ''), s.screen);
    };
  });
  var spb = $('#sel-parent');
  if (spb) spb.onclick = function () { var up = owner(s.path); S.sel = { screen: s.screen, path: up.path, comp: up.comp, box: up.box }; S.sel.key = s.screen + '|' + up.path; if (S.mode === 'proto') sendAll({ cmd: 'select', path: up.path }); renderAll(); };
  $$('#rbody [data-goto]').forEach(function (el) {
    el.onclick = function () {
      S.leftTab = 'tokens'; S.uiHidden = false; applyBody(); renderLeft();
      var row = $('#lbody [data-tokrow="' + el.dataset.goto + '"]');
      if (row) { row.scrollIntoView({ block: 'center' }); var inp = $('input.input', row); if (inp) inp.focus(); }
      else toast(el.dataset.goto + ' isn’t a token you can edit here');
    };
  });
}
function jsonTab() {
  var s = S.sel;
  if (!s) return '<div class="empty">' + ic('braces') + '<b>Nothing selected</b><span>Select a screen or a part to see and edit its JSON.</span></div>';
  var hasPart = !!(s.path && getNode(s.path)), scope = hasPart ? S.jsonScope : 'screen';
  var obj = scope === 'sel' ? getNode(s.path) : S.proto.screens[s.screen];
  var txt = JSON.stringify(obj, null, 2) || '', lines = txt.split('\n').length;
  return '<div style="padding:10px 12px;display:flex;flex-direction:column;gap:8px;flex:1;min-height:0">' +
    '<div class="seg full"><button data-scope="sel" class="' + (scope === 'sel' ? 'on' : '') + '"' + (hasPart ? '' : ' disabled') + '>Selection</button><button data-scope="screen" class="' + (scope === 'screen' ? 'on' : '') + '">Whole screen</button></div>' +
    '<div class="note mono">' + esc(scope === 'sel' ? 'prototype.json › ' + s.path : 'prototype.json › screens › ' + s.screen) + '</div>' +
    '<div class="jsonwrap scroll" id="jw" style="flex:1;min-height:0"><div class="gutter" id="gut">' + range(lines) + '</div><textarea class="code" id="code" spellcheck="false"' + (S.readonly || S.viewing ? ' readonly' : '') + '>' + esc(txt) + '</textarea></div>' +
    '<div class="jstat ok" id="jstat"><span class="dot"></span>' + (S.readonly ? 'View only' : 'Valid · changes save as you type') + '</div></div>';
}
function range(n) { var a = []; for (var i = 1; i <= n; i++) a.push(i); return a.join('\n'); }
function wireJson() {
  $$('[data-scope]').forEach(function (b) { b.onclick = function () { if (!b.disabled) { S.jsonScope = b.dataset.scope; renderRight(); } }; });
  var ta = $('#code'); if (!ta) return;
  var s = S.sel, timer;
  function size() { ta.style.height = 'auto'; ta.style.height = (ta.scrollHeight + 14) + 'px'; $('#gut').textContent = range(ta.value.split('\n').length); }
  ta.onblur = function () { setTimeout(function () { if (document.activeElement !== ta && S.rightTab === 'json') renderRight(); }, 0); };
  size();
  ta.onkeydown = function (e) { e.stopPropagation(); if (e.key === 'Tab') { e.preventDefault(); document.execCommand('insertText', false, '  '); } };
  ta.oninput = function () {
    size(); clearTimeout(timer);
    timer = setTimeout(function () {
      var st = $('#jstat'), jw = $('#jw'), v;
      try { v = JSON.parse(ta.value); } catch (e) {
        var m = /position (\d+)/.exec(e.message), pos = m ? +m[1] : 0, pre = ta.value.slice(0, pos);
        st.className = 'jstat bad'; jw.classList.add('bad');
        st.innerHTML = '<span class="dot"></span>Line ' + pre.split('\n').length + ', column ' + (pos - pre.lastIndexOf('\n')) + ': ' + esc(e.message.replace(/ in JSON.*$/, '').replace(/ at position \d+.*$/, '')) + '. The devices keep the last good version.';
        return;
      }
      var scope = s.path && getNode(s.path) ? S.jsonScope : 'screen';
      var path = scope === 'sel' ? jpath(s.path) : ['screens', s.screen];
      st.className = 'jstat ok'; jw.classList.remove('bad'); st.innerHTML = '<span class="dot"></span>Saving…';
      var focus = document.activeElement === ta, caret = ta.selectionStart;
      edit('prototype', [{ path: path, value: v }], 'Edited the JSON of ' + (scope === 'sel' ? s.comp.split(' ')[0] : meta(s.screen).title), s.screen).then(function () {
        if (focus) { var t2 = $('#code'); if (t2) { t2.focus(); t2.selectionStart = t2.selectionEnd = Math.min(caret, t2.value.length); } }
      }, function () {});
    }, 500);
  };
}
var STATUS = { draft: 'Not sent', sent: 'Waiting for agent', replied: 'Agent replied', resolved: 'Resolved' };
function commentsTab() {
  var drafts = S.comments.filter(function (c) { return c.status === 'draft'; }).length;
  var order = { draft: 0, replied: 1, sent: 2, resolved: 3 };
  var list = S.comments.slice().sort(function (a, b) { return order[a.status] - order[b.status] || b.id - a.id; });
  var resolvedN = S.comments.filter(function (c) { return c.status === 'resolved'; }).length;
  var html = '<div class="scroll" style="flex:1;min-height:0">';
  if (!S.comments.length) html += '<div class="empty">' + ic('comment') + '<b>No comments yet</b><span>Press C, then click anywhere on a screen, on the canvas or a live device.</span></div>';
  list.forEach(function (c) {
    if (c.status === 'resolved' && S.hideResolved) return;
    html += '<div class="cmt' + (S.activeComment === c.id ? ' on' : '') + '" data-c="' + c.id + '"><div class="h"><span class="num ' + c.status + '">' + c.id + '</span><span class="where">' + esc(meta(c.screen).title) + (c.part ? ' · ' + esc(c.part.split(' ')[0]) : '') + ' · ' + hm(c.created) + '</span><span class="st ' + c.status + '">' + STATUS[c.status] + '</span></div>' +
      c.thread.map(function (m) { return '<div class="msg ' + m.who + '">' + (m.who === 'agent' ? '<div class="who">Agent</div>' : '') + esc(m.text) + '</div>'; }).join('') +
      (S.activeComment === c.id && c.status !== 'resolved' ? '<textarea class="input" data-reply="' + c.id + '" rows="2" placeholder="Reply"></textarea>' : '') +
      '<div class="acts">' + (c.status === 'draft' ? '<button class="btn ghost sm" data-act="delete" data-id="' + c.id + '">Delete</button>' : '') +
      (c.status === 'resolved' ? '<button class="btn ghost sm" data-act="reopen" data-id="' + c.id + '">Reopen</button>' : '<button class="btn ghost sm" data-act="resolve" data-id="' + c.id + '">Resolve</button>') + '</div></div>';
  });
  html += '</div><div class="cfoot">';
  if (resolvedN) html += '<button class="btn ghost sm" id="toggle-res" style="align-self:flex-start">' + (S.hideResolved ? 'Show' : 'Hide') + ' ' + resolvedN + ' resolved</button>';
  if (S.justSent) html += '<div class="callout"><b>Sent ' + S.justSent + ' to the agent queue.</b> They’re saved, so no agent has to be watching. Any time, ask your agent to <b>address the Design Studio comments</b>.</div>';
  html += '<button class="btn primary wide" id="send"' + (drafts ? '' : ' disabled') + '>' + ic('send') + 'Send ' + (drafts || '') + ' to agent</button>';
  html += '<p class="note">Comments stay on this Mac. Write as many as you like, then send them as one batch.</p></div>';
  return html;
}
function wireComments() {
  $$('.cmt').forEach(function (el) { el.onclick = function (e) { if (e.target.closest('button, textarea')) return; openComment(+el.dataset.c); }; });
  $$('[data-act]').forEach(function (b) { b.onclick = function () { commentAction({ action: b.dataset.act, id: +b.dataset.id }); }; });
  $$('[data-reply]').forEach(function (ta) {
    ta.onkeydown = function (e) {
      e.stopPropagation();
      if (e.key === 'Enter' && (e.metaKey || e.ctrlKey) && ta.value.trim()) commentAction({ action: 'reply', id: +ta.dataset.reply, text: ta.value.trim() }).then(function () { toast('Reply saved. It goes with your next batch.'); });
    };
  });
  var s = $('#send'); if (s) s.onclick = sendComments;
  var t = $('#toggle-res'); if (t) t.onclick = function () { S.hideResolved = !S.hideResolved; applyBody(); renderRight(); drawOverlay(); };
}

// ── Version history ─────────────────────────────────────────────────────────
var WHO = { you: 'You', agent: 'Made by your agent', outside: 'Outside the studio' };
function historyGroups() {
  // Consecutive saves by the same author within 5 minutes read as one row.
  var out = [];
  S.history.slice().reverse().forEach(function (v) {
    var g = out[out.length - 1];
    if (g && !v.name && !g.items[0].name && g.who === v.who && g.items[g.items.length - 1].t - v.t < 300) g.items.push(v);
    else out.push({ who: v.who, items: [v] });
  });
  return out;
}
function historyPanel() {
  var html = '<div class="ph"><h2>Version history</h2><button class="ibtn" id="hist-x" title="Close (Esc)">' + ic('x') + '</button></div>' +
    '<div style="padding:10px 12px;display:flex;flex-direction:column;gap:8px;border-bottom:1px solid var(--line)"><button class="btn wide" id="name-ver">' + ic('bookmark') + 'Name this version</button>' +
    '<div class="seg full"><button data-vf="all" class="' + (S.versionsFilter === 'all' ? 'on' : '') + '">All</button><button data-vf="named" class="' + (S.versionsFilter === 'named' ? 'on' : '') + '">Named</button><button data-vf="agent" class="' + (S.versionsFilter === 'agent' ? 'on' : '') + '">Agent</button></div></div>' +
    '<div class="scroll vline" style="flex:1;min-height:0;padding-bottom:16px">';
  var day = '', first = true;
  historyGroups().forEach(function (g) {
    var v = g.items[0];
    if (S.versionsFilter === 'named' && !v.name) return;
    if (S.versionsFilter === 'agent' && g.who !== 'agent') return;
    var d = dayOf(v.t);
    if (d !== day) { day = d; html += '<div class="vday">' + d + '</div>'; }
    var title = v.name || (g.items.length > 1 ? g.items.length + ' edits' : v.title);
    var cur = first && !S.viewing; first = false;
    html += '<div class="ver ' + g.who + (v.name ? ' named' : '') + ((S.viewing ? S.viewing.id === v.id : cur) ? ' on' : '') + '" data-v="' + v.id + '"><span class="vd"></span><div>' +
      '<div class="vt">' + (cur ? 'Current version' : esc(title)) + (g.who === 'agent' ? '<span class="abadge">' + ic('spark') + 'Agent</span>' : '') + '</div>' +
      '<div class="vm">' + (v.name ? 'Named · ' : '') + WHO[g.who] + ' · ' + hm(v.t) + '</div>' +
      g.items.slice(0, 4).map(function (x) { return '<div class="vx">' + (g.items.length > 1 || cur ? hm(x.t) + ' · ' + esc(x.title) : '') + (x.detail ? (g.items.length > 1 || cur ? ' · ' : '') + esc(x.detail) : '') + '</div>'; }).join('') +
      (g.items.length > 4 ? '<div class="vx">and ' + (g.items.length - 4) + ' more</div>' : '') + '</div></div>';
  });
  if (!S.history.length) html += '<div class="empty"><b>No versions yet</b><span>Every save shows up here.</span></div>';
  html += '<p class="note" style="padding:12px">Every save is kept: yours, the agent’s and any edit made outside the studio. Click a version to look at it; restoring adds a new version, so you can always roll forward again.</p></div>';
  return html;
}
function wireHistory() {
  $('#hist-x').onclick = closeHistory;
  $('#name-ver').onclick = function () {
    dialog('Name this version', '<input class="input" id="vn" placeholder="Before the row-height test">', 'Name', function () {
      var n = $('#vn').value.trim(); if (!n) return false;
      api('POST', '/api/history/name', { name: n }).then(function () { toast('Named “' + n + '”'); });
    });
  };
  $$('[data-vf]').forEach(function (b) { b.onclick = function () { S.versionsFilter = b.dataset.vf; renderRight(); }; });
  $$('.ver').forEach(function (el) {
    el.onclick = function () {
      var latest = S.history[S.history.length - 1], v = S.history.filter(function (x) { return x.id === el.dataset.v; })[0];
      if (!v || v === latest) return exitPreview();
      previewVersion(v);
    };
  });
}
function closeHistory() { S.showHistory = false; exitPreview(); renderAll(); }
function previewVersion(v) {
  api('GET', '/api/history/' + v.id).then(function (files) {
    S.viewing = v; S.viewProto = JSON.parse(files.prototype); S.sel = null;
    if (S.mode !== 'design') setMode('design');
    $('#canvas').classList.add('viewing');
    snapFrame.contentWindow.postMessage({ src: 'rd-studio', cmd: 'load-texts', tokens: files.tokens, prototype: files.prototype }, '*');
    renderAll();
  }, function (e) { toast(String(e.message)); });
}
function exitPreview() {
  if (!S.viewing) return;
  S.viewing = null; S.viewProto = null; $('#canvas').classList.remove('viewing');
  snapFrame.contentWindow.postMessage({ src: 'rd-studio', cmd: 'reload-data' }, '*');
  renderAll();
}
function renderBanner() {
  var b = $('#vbanner');
  if (!S.viewing) { b.classList.remove('show'); return; }
  b.classList.add('show');
  b.innerHTML = ic('eye') + '<span>Viewing <b>' + esc(S.viewing.name || S.viewing.title) + '</b> · ' + dayOf(S.viewing.t) + ' ' + hm(S.viewing.t) + '</span><button class="btn primary sm" id="vrestore">Restore this version</button><button class="btn ghost sm" id="vback">Back to current</button>';
  $('#vrestore').onclick = function () {
    api('POST', '/api/history/' + S.viewing.id + '/restore').then(function () { toast('Restored. The version before it is still in the list.'); S.undo = []; S.redo = []; exitPreview(); }, function (e) { toast(String(e.message)); });
  };
  $('#vback').onclick = exitPreview;
}

// ── Prototype mode: live devices ────────────────────────────────────────────
function activeSlots() { var out = []; S.slots.forEach(function (id, i) { if (dev(id)) out.push({ slot: String(i), d: dev(id) }); }); return out; }
function destroyDevices() { $('#stage').innerHTML = ''; S.frames = {}; S.outlines = {}; }
function buildDevices() {
  destroyDevices();
  var act = activeSlots(), st = $('#stage');
  if (!act.length) { st.innerHTML = '<div class="empty"><b>No device chosen</b><span>Pick one under Devices.</span></div>'; renderPbar(); return; }
  act.forEach(function (a) {
    var fig = document.createElement('figure'); fig.className = 'dev';
    fig.innerHTML = '<div class="bezel"><iframe title="' + esc(a.d.name) + '"></iframe><div class="pins"></div></div><figcaption></figcaption>';
    var f = $('iframe', fig);
    f.src = 'device.html?w=' + a.d.w + '&h=' + a.d.h + '&name=' + a.slot + '&path=' + encodeURIComponent(S.playPath) + '&_=' + Date.now();
    st.appendChild(fig);
    S.frames[a.slot] = { el: f, fig: fig, d: a.d };
  });
  layoutDevices(); renderPbar();
}
function layoutDevices() {
  var devs = Object.keys(S.frames).map(function (k) { return S.frames[k].d; });
  if (!devs.length) return;
  var st = $('#stage'), n = devs.length;
  var availH = st.clientHeight - 40 - 20 - 58, availW = st.clientWidth - 48 - n * 22 - (n - 1) * 40;
  var maxH = Math.max.apply(null, devs.map(function (d) { return d.h; })), sumW = devs.reduce(function (a, d) { return a + d.w; }, 0);
  var k = Math.max(0.1, Math.min(1, availH / maxH, availW / sumW));
  Object.keys(S.frames).forEach(function (slot) {
    var fr = S.frames[slot], d = fr.d;
    fr.el.style.width = Math.round(d.w * k) + 'px'; fr.el.style.height = Math.round(d.h * k) + 'px'; fr.k = k;
    $('figcaption', fr.fig).innerHTML = '<b>' + esc(d.name) + '</b><span>' + d.w + ' × ' + d.h + ' px · ' + aspect(d) + '</span><span class="d">320 × ' + Math.round(d.h / px(d)) + ' pt · 1 pt = ' + fmt(px(d)) + ' px · ' + Math.round(k * 100) + '%</span>';
  });
  drawLivePins();
}
function sendTo(slot, msg) { msg.src = 'rd-studio'; var fr = S.frames[slot]; if (fr && fr.el.contentWindow) fr.el.contentWindow.postMessage(msg, '*'); }
function sendAll(msg) { Object.keys(S.frames).forEach(function (s) { sendTo(s, Object.assign({}, msg)); }); }
function setPMode(m) {
  if (m === 'comment' && S.readonly) return;
  S.pmode = m; S.pcomposer = null; sendAll({ cmd: 'mode', mode: m }); renderPbar(); drawLivePins();
}
function renderPbar() {
  if (S.mode !== 'proto') return;
  var cur = curScreen() || '', m = meta(cur);
  var modes = [['interact', 'tap', 'Interact', 'V'], ['inspect', 'inspect', 'Inspect', 'I']].concat(S.readonly ? [] : [['comment', 'comment', 'Comment', 'C']]);
  $('#pbar').innerHTML = '<span class="title">' + ic('play') + esc(m.title) + ' <span class="chip">' + esc(cur) + '</span></span>' +
    '<div class="seg">' + modes.map(function (x) { return '<button data-pm="' + x[0] + '" class="' + (S.pmode === x[0] ? 'on' : '') + '" title="' + x[2] + ' (' + x[3] + ')">' + ic(x[1]) + x[2] + '</button>'; }).join('') + '</div>' +
    '<button class="ibtn" id="p-back" title="Back"' + (S.playPath.indexOf('/') > 0 ? '' : ' disabled') + '>' + ic('back') + '</button>' +
    '<button class="ibtn" id="p-restart" title="Restart from the start screen">' + ic('restart') + '</button>' +
    '<button class="btn ghost" id="p-devices" title="Choose the devices">' + ic('devices') + 'Devices</button>' +
    '<div class="pright"><span class="lbl">Frame rate</span><div class="seg" title="Draw only this many frames a second, to feel a slow board. Input is read once per frame too.">' +
    [[60, '60'], [30, '30'], [15, '15'], [10, '10 · worst']].map(function (f) { return '<button data-fps="' + f[0] + '" class="' + (S.fps === f[0] ? 'on' : '') + '">' + f[1] + '</button>'; }).join('') + '</div></div>';
  $$('[data-pm]').forEach(function (b) { b.onclick = function () { setPMode(b.dataset.pm); }; });
  $$('[data-fps]').forEach(function (b) { b.onclick = function () { S.fps = +b.dataset.fps; store.set('fps', S.fps); sendAll({ cmd: 'fps', fps: S.fps }); renderPbar(); toast(S.fps === 60 ? 'Full speed' : 'Drawing ' + S.fps + ' frames a second, like a slow board'); }; });
  $('#p-back').onclick = function () { var p = S.playPath.split('/'); if (p.length > 1) { p.pop(); S.playPath = p.join('/'); sendAll({ cmd: 'show', path: S.playPath }); writeUrl(); renderAll(); } };
  $('#p-restart').onclick = function () { S.playPath = ''; sendAll({ cmd: 'show', path: '' }); writeUrl(); };
  $('#p-devices').onclick = function () { devicesMenu(this); };
}
function devicesMenu(anchor) {
  var html = [0, 1].map(function (i) {
    var other = S.slots[1 - i];
    return '<div class="mh">Device ' + (i + 1) + '</div>' + [{ id: '', name: 'None' }].concat(DEVICES).map(function (d) {
      var off = d.id && d.id === other;
      return '<button data-slot="' + i + '" data-dev="' + d.id + '"' + (off ? ' disabled style="opacity:.4"' : '') + '><span class="ck">' + ((S.slots[i] || '') === d.id ? ic('check') : '') + '</span>' + esc(d.name) +
        (d.id ? '<span class="mt">' + d.w + ' × ' + d.h + ' · ' + aspect(d) + '</span>' : '') + '</button>';
    }).join('');
  }).join('<hr>');
  showMenu(anchor, html, function (m) {
    $$('[data-slot]', m).forEach(function (b) { b.onclick = function () { if (b.disabled) return; S.slots[+b.dataset.slot] = b.dataset.dev; writeUrl(); hideMenu(); buildDevices(); }; });
  });
}
function liveBoxes() {
  var first = Object.keys(S.outlines)[0], o = first && S.outlines[first];
  if (!o) return [];
  var k = px(S.frames[first].d);
  return o.boxes.map(function (b) { return Object.assign({}, b, { x: b.x / k, y: b.y / k, w: b.w / k, h: b.h / k }); });
}
function drawLivePins() {
  var cur = curScreen();
  Object.keys(S.frames).forEach(function (slot) {
    var fr = S.frames[slot], layer = $('.pins', fr.fig), o = S.outlines[slot], k = px(fr.d), scale = fr.k || 1;
    if (!layer) return;
    var list = o ? o.boxes.map(function (b) { return Object.assign({}, b, { x: b.x / k, y: b.y / k, w: b.w / k, h: b.h / k }); }) : [];
    var html = S.comments.filter(function (c) { return c.screen === cur && !(c.status === 'resolved' && S.hideResolved); }).map(function (c) {
      var p = pinPos(c, list, fr.d.h / k);
      return '<button class="pin ' + c.status + '" data-c="' + c.id + '" style="left:' + p.x * k * scale + 'px;top:' + p.y * k * scale + 'px;pointer-events:auto" title="' + esc(c.thread[0].text) + '">' + c.id + '</button>';
    }).join('');
    if (S.pcomposer && S.pcomposer.slot === slot) {
      var pc = S.pcomposer;
      html += '<span class="pin" style="left:' + pc.x * k * scale + 'px;top:' + pc.y * k * scale + 'px">+</span>' + composerHtml(pc.x * k * scale + 26, pc.y * k * scale - 24, meta(cur).title + ' · live', pc.part, 'ptext', 'This tap feels slow at 10 fps');
    }
    var pd = $('#ptext', layer), pkeep = pd ? pd.value : '';
    layer.innerHTML = html;
    if (pkeep && $('#ptext', layer)) $('#ptext', layer).value = pkeep;
    $$('.pin[data-c]', layer).forEach(function (p) { p.onclick = function (e) { e.stopPropagation(); openComment(+p.dataset.c); }; });
  });
  if (S.pcomposer) wireComposer('ptext', function () { S.pcomposer = null; drawLivePins(); }, addLiveComment);
}
function addLiveComment() {
  var t = ($('#ptext').value || '').trim(); if (!t) { $('#ptext').focus(); return; }
  var c = S.pcomposer, d = S.frames[c.slot].d;
  api('POST', '/api/comments', commentBody(curScreen(), c.x, c.y, c.box, c.part, c.path, d.w + 'x' + d.h, t)).then(function (db) {
    S.comments = db.comments; S.pcomposer = null; S.rightTab = 'comments'; S.showHistory = false; renderAll(); toast('Comment saved. Send when you’re ready.');
  }, function (e) { toast(String(e.message)); });
}

// ── Messages from the engines ───────────────────────────────────────────────
window.addEventListener('message', function (ev) {
  var m = ev.data || {};
  if (m.src !== 'rd-device') return;
  if (m.name === 'snap') {
    if (m.evt === 'ready' || m.evt === 'reloaded') snapAll();
    else if (m.evt === 'snaps') {
      S.shots = m.detail.shots; S.shotDev = DEVICES.filter(function (d) { return d.w === m.detail.w && d.h === m.detail.h; })[0].id;
      $('#loading').hidden = true;
      layoutWorld(); if (!S.fitted) { fit(); S.fitted = true; } else applyView();
      renderLeft(); renderRight();
    } else if (m.evt === 'status' && !S.viewing) { S.problems.canvas = m.detail; renderRight(); }
    return;
  }
  if (!S.frames[m.name]) return;
  if (m.evt === 'ready') {
    sendTo(m.name, { cmd: 'mode', mode: S.pmode }); sendTo(m.name, { cmd: 'fps', fps: S.fps });
  } else if (m.evt === 'navigate') {
    if (m.detail !== S.playPath) {
      S.playPath = m.detail; writeUrl();
      Object.keys(S.frames).forEach(function (s) { if (s !== m.name) sendTo(s, { cmd: 'show', path: m.detail }); });
      S.sel = null; S.pcomposer = null; renderAll();
    }
  } else if (m.evt === 'outline') {
    S.outlines[m.name] = m.detail; drawLivePins();
    if (S.leftTab === 'layers') renderLeft();
  } else if (m.evt === 'status') {
    S.problems['device' + m.name] = m.detail; renderRight();
  } else if (m.evt === 'hover' && S.pmode === 'inspect' && !S.selPinned) {
    // Hover previews in the Design tab only while nothing is pinned.
  } else if (m.evt === 'select') {
    var info = m.detail, cur = curScreen(), k = px(S.frames[m.name].d);
    S.sel = info.found ? { screen: cur, path: info.path, comp: info.component, box: Object.assign({}, info, { x: info.x / k, y: info.y / k, w: info.w / k, h: info.h / k }) } : { screen: cur };
    S.sel.key = cur + '|' + (info.path || info.component || '');
    Object.keys(S.frames).forEach(function (s) { if (s !== m.name) sendTo(s, { cmd: 'select', path: info.path }); });
    if (S.rightTab === 'comments') S.rightTab = 'design';
    renderLeft(); renderRight();
  } else if (m.evt === 'pick') {
    var kk = px(S.frames[m.name].d), i2 = m.detail.info || {};
    var box = i2.found ? { x: i2.x / kk, y: i2.y / kk, w: i2.w / kk, h: i2.h / kk, tokens: i2.tokens } : null;
    S.pcomposer = { slot: m.name, x: m.detail.x / kk, y: m.detail.y / kk, part: i2.found ? i2.component : null, path: i2.found ? i2.path : null, box: box };
    drawLivePins();
  }
});

// ── Menus, dialogs ──────────────────────────────────────────────────────────
function showMenu(anchor, html, wire) {
  var m = $('#menu'), r = anchor.getBoundingClientRect();
  m.innerHTML = html; m.classList.add('show');
  var left = Math.min(r.left, innerWidth - m.offsetWidth - 8), top = r.bottom + 6;
  if (top + m.offsetHeight > innerHeight) top = Math.max(8, r.top - m.offsetHeight - 6);
  m.style.left = left + 'px'; m.style.top = top + 'px';
  wire(m);
}
function hideMenu() { $('#menu').classList.remove('show'); }
document.addEventListener('pointerdown', function (e) { if (!e.target.closest('#menu, #menu-btn, #zpct, #p-devices')) hideMenu(); });
function dialog(title, body, okLabel, onOk) {
  var sc = $('#scrim');
  sc.innerHTML = '<div class="dialog" role="dialog" aria-label="' + esc(title) + '"><div class="ph"><h2>' + esc(title) + '</h2><button class="ibtn" data-x>' + ic('x') + '</button></div><div class="body">' + body + '</div>' +
    '<div class="foot"><button class="btn ghost" data-x>Cancel</button><button class="btn primary" data-ok>' + esc(okLabel) + '</button></div></div>';
  sc.classList.add('show');
  var close = function () { sc.classList.remove('show'); sc.innerHTML = ''; };
  $$('[data-x]', sc).forEach(function (b) { b.onclick = close; });
  var ok = function () { if (onOk() !== false) close(); };
  $('[data-ok]', sc).onclick = ok;
  $$('input', sc).forEach(function (i) { i.onkeydown = function (e) { e.stopPropagation(); if (e.key === 'Enter') ok(); if (e.key === 'Escape') close(); }; });
  var first = $('input', sc); if (first) setTimeout(function () { first.focus(); }, 0);
  sc.onclick = function (e) { if (e.target === sc) close(); };
}
function shortcuts() {
  var rows = [['Design ↔ Prototype', '⇧E'], ['Select / Interact', 'V'], ['Hand (or hold Space)', 'H'], ['Comment', 'C'], ['Inspect (Prototype)', 'I'],
    ['Undo', '⌘Z'], ['Redo', '⇧⌘Z'], ['Version history', '⌥⌘H'], ['Zoom to fit', '⇧1'], ['Zoom to selection', '⇧2'], ['Zoom to 100%', '⇧0'],
    ['Zoom in / out', '+ / −'], ['Edit text in place', 'Double-click'], ['Post a comment', '⌘↵'], ['Hide panels', '⌘\\'], ['Close, leave, deselect', 'Esc']];
  dialog('Keyboard shortcuts', '<div class="grid" style="padding:0">' + rows.map(function (r) { return '<div class="kr"><span>' + r[0] + '</span><kbd>' + r[1] + '</kbd></div>'; }).join('') + '</div>', 'Done', function () {});
}
$('#menu-btn').onclick = function () {
  showMenu(this, '<button data-act="keys"><span class="ck"></span>Keyboard shortcuts<kbd>?</kbd></button>' +
    '<button data-act="hide"><span class="ck"></span>' + (S.uiHidden ? 'Show' : 'Hide') + ' panels<kbd>⌘\\</kbd></button><hr>' +
    '<button data-act="classic"><span class="ck">' + ic('ext') + '</span>Open the Classic runner<span class="mt">landscape screens</span></button>',
    function (m) {
      $('[data-act="keys"]', m).onclick = function () { hideMenu(); shortcuts(); };
      $('[data-act="hide"]', m).onclick = function () { hideMenu(); toggleUi(); };
      $('[data-act="classic"]', m).onclick = function () { hideMenu(); window.open('../runner/', '_blank'); };
    });
};
function toggleUi() { S.uiHidden = !S.uiHidden; applyBody(); setTimeout(function () { applyView(); layoutDevices(); }, 0); }
$('#zpct').onclick = function () {
  showMenu(this, '<button data-z="in">Zoom in<kbd>+</kbd></button><button data-z="out">Zoom out<kbd>−</kbd></button><hr>' +
    '<button data-z="fit">Zoom to fit<kbd>⇧1</kbd></button><button data-z="sel">Zoom to selection<kbd>⇧2</kbd></button><hr><button data-z="50">50%</button><button data-z="100">100%<kbd>⇧0</kbd></button>', function (m) {
    $$('[data-z]', m).forEach(function (b) { b.onclick = function () { zoomCmd(b.dataset.z); hideMenu(); }; });
  });
};
function zoomCmd(z) {
  if (z === 'in') zoomTo(S.k * 1.25); else if (z === 'out') zoomTo(S.k / 1.25); else if (z === 'fit') fit();
  else if (z === 'sel') { if (S.sel) focusScreen(S.sel.screen, true); } else zoomTo(+z / 100);
}
$('#zin').onclick = function () { zoomCmd('in'); }; $('#zout').onclick = function () { zoomCmd('out'); };
$$('#mode-seg [data-mode]').forEach(function (b) { b.onclick = function () { setMode(b.dataset.mode); }; });
$('#undo').onclick = undo; $('#redo').onclick = redo;
$('#hist-btn').onclick = function () { if (S.showHistory) closeHistory(); else { S.showHistory = true; loadHistory().then(renderAll); } };
$('#unsent-btn').onclick = function () { S.rightTab = 'comments'; S.showHistory = false; renderAll(); };

// ── Keyboard ────────────────────────────────────────────────────────────────
document.addEventListener('keydown', function (e) {
  if (typing(e) || $('#scrim').classList.contains('show')) return;
  var k = e.key, mod = e.metaKey || e.ctrlKey;
  if (mod && k.toLowerCase() === 'z') { e.preventDefault(); if (!S.readonly) (e.shiftKey ? redo : undo)(); return; }
  if (mod && k === '\\') { e.preventDefault(); toggleUi(); return; }
  if (mod && e.altKey && (e.code === 'KeyH')) { e.preventDefault(); $('#hist-btn').click(); return; }
  if (mod) return;
  if (e.shiftKey && e.code === 'KeyE') { e.preventDefault(); setMode(S.mode === 'proto' ? 'design' : 'proto'); return; }
  if (k === '?') return shortcuts();
  if (k === 'Escape') {
    if (S.composer) { S.composer = null; return drawOverlay(); }
    if (S.pcomposer) { S.pcomposer = null; return drawLivePins(); }
    if (S.viewing) return exitPreview();
    if (S.showHistory) return closeHistory();
    if (S.mode === 'proto' && S.pmode !== 'interact') return setPMode('interact');
    return select(null);
  }
  if (S.mode === 'proto') {
    if (k === 'i' || k === 'I') setPMode(S.pmode === 'inspect' ? 'interact' : 'inspect');
    else if (k === 'c' || k === 'C') setPMode('comment');
    else if (k === 'v' || k === 'V') setPMode('interact');
    return;
  }
  if (e.shiftKey && e.code === 'Digit1') return fit();
  if (e.shiftKey && e.code === 'Digit2') return zoomCmd('sel');
  if (e.shiftKey && e.code === 'Digit0') return zoomTo(1);
  if (k === 'v' || k === 'V') setTool('select');
  else if (k === 'h' || k === 'H') setTool('hand');
  else if ((k === 'c' || k === 'C') && !S.readonly) setTool('comment');
  else if (k === '=' || k === '+') zoomCmd('in');
  else if (k === '-') zoomCmd('out');
});

// ── Live reload (only when served by the studio server) ─────────────────────
function setLive(cls, text, tip) { var el = $('#live'); el.className = 'status ' + cls; $('#live-text').textContent = text; el.title = tip || text; }
function setBanner(cls, text) { S.banner = cls ? { cls: cls, text: text } : null; showBanner(); }
function showBanner() { var b = $('#banner'); if (!b) return; b.className = S.banner ? S.banner.cls : ''; b.textContent = S.banner ? S.banner.text : ''; }
function refreshAll() {
  return loadData().then(function () {
    ICONS_SEEN = null;
    if (S.sel && S.sel.path && !getNode(S.sel.path) && !owner(S.sel.path)) S.sel = { screen: S.sel.screen };
    if (S.sel && !S.proto.screens[S.sel.screen]) S.sel = null;
    renderAll();
  });
}
function startLive() {
  if (S.readonly || !window.EventSource) return;
  var es = new EventSource('/events');
  es.addEventListener('hello', function () { setLive('live', 'Live', 'The studio server is running; edits save to the JSON files'); });
  es.addEventListener('data', function () {
    delete S.problems.json; setBanner();
    if (!S.viewing) snapFrame.contentWindow.postMessage({ src: 'rd-studio', cmd: 'reload-data' }, '*');
    sendAll({ cmd: 'reload-data' });
    refreshAll(); setLive('live', 'Updated ' + now());
  });
  es.addEventListener('data-error', function (e) { S.problems.json = { error: e.data }; setBanner('err', e.data); setLive('bad', 'Fix the JSON'); renderRight(); });
  es.addEventListener('building', function () { setBanner('busy', 'Rebuilding the engine (C changed)…'); setLive('busy', 'Rebuilding'); });
  es.addEventListener('build-failed', function (e) { setBanner('err', 'Build failed:\n' + e.data); setLive('bad', 'Build failed'); });
  es.addEventListener('engine', function () { setBanner(); buildSnap(); if (S.mode === 'proto') buildDevices(); setLive('live', 'Rebuilt ' + now()); });
  es.addEventListener('page', function () { location.reload(); });
  es.addEventListener('comments', function () { loadComments().then(renderAll); });
  es.addEventListener('history', function () { loadHistory().then(function () { if (S.showHistory) renderRight(); }); });
  es.onerror = function () { setLive('bad', 'Server stopped', 'Start the studio again'); };
}

// ── Render ──────────────────────────────────────────────────────────────────
function renderAll() {
  applyBody(); renderCenter(); renderLeft(); renderRight(); renderBanner(); updateTop();
  if (S.mode === 'proto') { renderPbar(); drawLivePins(); } else { if (!S.viewing) layoutWorldIfNeeded(); drawOverlay(); }
}
var lastLayoutKey = '';
function layoutWorldIfNeeded() {
  var key = JSON.stringify([P().flows, screenIds(), Object.keys(S.shots).length, S.shotDev, S.viewing && S.viewing.id]);
  var titles = screenIds().map(function (id) { return meta(id).title + meta(id).type; }).join('|');
  if (key + titles !== lastLayoutKey) { lastLayoutKey = key + titles; layoutWorld(); applyView(); }
}
window.addEventListener('resize', function () { if (S.mode === 'proto') layoutDevices(); else applyView(); });

// Start: are we on the studio server (edit) or a static site (view only)?
fetch('/api/history', { cache: 'no-store' }).then(function (r) { return r.ok ? r.json() : Promise.reject(); }).then(function (h) {
  S.readonly = false; S.history = h; $('#saved').textContent = 'Live';
}, function () {
  S.readonly = true; $('#saved').textContent = 'View only';
  setLive('', 'View only', 'This copy has no studio server. Run the studio on your Mac to edit.');
}).then(function () { return Promise.all([loadData(), loadComments()]); }).then(function () {
  applyBody(); renderAll(); buildSnap(); startLive(); setTool('select');
  if (S.mode === 'proto') buildDevices();
});
})();
