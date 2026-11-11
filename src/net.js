// ===================================================================
// Online play: rollback netcode over a tiny message transport.
// Only button inputs cross the wire; both sides run the same sim.
// ===================================================================
const NET = (function () {
  'use strict';
  const MAXPRED = 9, HIST = 24;
  let gid = 0, tr = null, status = 'idle', err = '', code = '', host = false, sess = null, myCh = 0, remCh = -1, helloT = 0, wantRematch = [false, false], onStart = null, lastRecv = 0;
  const voice = { on: false, muted: false, err: '', remote: false }; let audioEl = null;
  function playRemote(st) { if (!audioEl) { audioEl = new Audio(); audioEl.autoplay = true; } audioEl.srcObject = st; const p = audioEl.play(); if (p && p.catch) p.catch(() => { }); voice.remote = true; }
  function hookCall(t, call) { if (!call) return; if (t.call && t.call !== call) { try { t.call.close(); } catch (e) { } } t.call = call; call.on('stream', playRemote); call.on('close', () => { if (t.call === call) voice.remote = false; }); }
  async function voiceToggle() {
    if (!tr || tr.kind !== 'p2p') return;
    if (voice.on) { voice.muted = !voice.muted; tr.stream.getAudioTracks().forEach(x => { x.enabled = !voice.muted; }); return; }
    try { const st = await navigator.mediaDevices.getUserMedia({ audio: { echoCancellation: true, noiseSuppression: true, autoGainControl: true } }); if (!tr) { st.getTracks().forEach(x => x.stop()); return; } tr.stream = st; voice.on = true; voice.muted = false; voice.err = ''; if (tr.conn && tr.conn.open) hookCall(tr, tr.peer.call(tr.conn.peer, st)); }
    catch (e) { voice.err = 'MICROPHONE BLOCKED OR NOT FOUND'; }
  }
  const inArtifact = !!(window.claude && typeof window.claude.use === 'function');

  function loadScript(src) { return new Promise((res, rej) => { const s = document.createElement('script'); s.src = src; s.onload = res; s.onerror = () => rej(new Error('load')); document.head.appendChild(s); }); }
  async function roomTransport(c) {
    const room = await window.claude.use('room'); if (!room) throw new Error('ONLINE NEEDS THE HOSTED BUILD OR AN INVITED VIEWER');
    const r = await room.join('gk' + c); const t = { kind: 'room', delay: 4, every: 2, onmsg: null, onpeer: null, send(o) { r.emit('gk', o).catch(() => { }); }, close() { try { r.leave(); } catch (e) { } } };
    r.on('gk', m => { if (!m.isMe && t.onmsg) t.onmsg(m.data); }); return t;
  }
  async function peerTransport(c, asHost) {
    if (!window.RTCPeerConnection) throw new Error('THIS BROWSER BLOCKS PEER CONNECTIONS');
    if (!window.Peer) await loadScript('https://unpkg.com/peerjs@1.5.4/dist/peerjs.min.js');
    return new Promise((res, rej) => {
      const t = { kind: 'p2p', delay: 2, every: 1, onmsg: null, conn: null, send(o) { if (t.conn && t.conn.open) t.conn.send(o); }, close() { try { peer.destroy(); } catch (e) { } } };
      const peer = asHost ? new Peer('gekitotsu-' + c) : new Peer(); let done = false; t.peer = peer;
      peer.on('call', call => { call.answer(t.stream || undefined); hookCall(t, call); });
      const wire = conn => { t.conn = conn; conn.on('open', () => { if (t.stream) hookCall(t, peer.call(conn.peer, t.stream)); }); conn.on('data', d => t.onmsg && t.onmsg(d)); conn.on('close', () => { if (status !== 'idle') { status = 'lost'; err = 'OPPONENT LEFT'; } }); };
      peer.on('open', () => { if (asHost) { done = true; res(t); } else { const conn = peer.connect('gekitotsu-' + c, { reliable: false, serialization: 'json' }); wire(conn); conn.on('open', () => { done = true; res(t); }); } });
      peer.on('connection', conn => { if (!t.conn) wire(conn); });
      peer.on('error', e => { const m = e.type === 'peer-unavailable' ? 'ROOM ' + c.toUpperCase() + ' NOT FOUND' : e.type === 'unavailable-id' ? 'ROOM CODE IN USE, TRY AGAIN' : 'NETWORK ERROR: ' + (e.type || 'unknown').toUpperCase(); if (!done) rej(new Error(m)); else if (status !== 'idle' && e.type !== 'peer-unavailable') { err = m; } });
      setTimeout(() => { if (!done) rej(new Error('CONNECTION TIMED OUT')); }, 15000);
    });
  }

  function Session(local, chars, t) {
    this.local = local; this.t = t; this.delay = t.delay; this.s = SIM.newMatch({ chars: chars }); this.f = 0; this.loc = []; this.rem = []; this.used = []; this.saved = []; this.remLast = -1; this.rb = -1;
    this.remC = 0; this.remCAt = 0; this.rtt = 0; this.lastTs = 0; this.lastTsAt = 0; this.n = 0; this.stall = 0; this.rbMax = 0; this.pres = []; this.myH = {}; this.desync = false; this.ev = [];
  }
  Session.prototype.sim = function (fr, late) {
    this.saved[fr] = SIM.clone(this.s);
    const r = this.rem[fr] !== undefined ? this.rem[fr] : (this.remLast >= 0 ? this.rem[this.remLast] : 0); this.used[fr] = r; const l = this.loc[fr] || 0;
    SIM.step(this.s, this.local === 0 ? l : r, this.local === 0 ? r : l);
    const seen = this.pres[fr] || (this.pres[fr] = {});
    for (const e of this.s.ev) { const k = e.t + (e.who === undefined ? '' : e.who); if (!seen[k]) { seen[k] = 1; if (!late || e.t === 'hit' || e.t === 'block' || e.t === 'ko' || e.t === 'round' || e.t === 'fight' || e.t === 'super' || e.t === 'wbounce') this.ev.push(e); } }
  };
  Session.prototype.tick = function (input) {
    this.ev = []; this.n++;
    const lf = this.f + this.delay; if (this.loc[lf] === undefined) { this.loc[lf] = input; for (let i = lf - 1; i >= 0 && this.loc[i] === undefined; i--) this.loc[i] = 0; }
    if (this.n % this.t.every === 0) {
      const a = Math.max(0, lf - HIST + 1), now = performance.now(), pk = { t: 'i', g: gid, f: lf, i: this.loc.slice(a, lf + 1), c: this.f, ts: now, ets: this.lastTs, edt: now - this.lastTsAt };
      if (this.f % 120 === 0 && this.f > 0) { const cf = Math.min(this.remLast, this.f - 1); if (cf > 0 && this.saved[cf] && this.rb < 0) { pk.hf = cf; pk.hv = SIM.hash(this.saved[cf]); this.myH[cf] = pk.hv; } }
      this.t.send(pk);
    }
    if (this.s.phase === 'over') return;
    if (this.f - this.remLast > MAXPRED) { this.stall++; return; }
    this.stall = 0;
    if (this.remCAt && this.f > this.remC + (performance.now() - this.remCAt + this.rtt / 2) / 16.667 + 1.5 && this.n % 5 === 0) return;           // we are running ahead: let the other side catch up
    if (this.rb >= 0 && this.rb < this.f) { const to = this.f; this.s = this.saved[this.rb]; this.rbMax = Math.max(this.rbMax, to - this.rb); for (let fr = this.rb; fr < to; fr++) this.sim(fr, true); }
    this.rb = -1;
    this.sim(this.f, false); this.f++;
    const keep = Math.min(this.remLast, this.f) - 3; if (keep > 0 && this.saved[keep - 1]) { for (let i = keep - 1; i >= 0 && this.saved[i]; i--) { this.saved[i] = null; this.pres[i] = null; } }
  };
  Session.prototype.packet = function (m) {
    const n = m.i.length, a = m.f - n + 1;
    for (let k = 0; k < n; k++) { const fr = a + k; if (fr < 0 || this.rem[fr] !== undefined) continue; this.rem[fr] = m.i[k] & SIM.MASK; if (fr < this.f && this.used[fr] !== this.rem[fr]) this.rb = this.rb < 0 ? fr : Math.min(this.rb, fr); }
    while (this.rem[this.remLast + 1] !== undefined) this.remLast++;
    const now = performance.now(); if (m.c >= this.remC) { this.remC = m.c; this.remCAt = now; } this.lastTs = m.ts; this.lastTsAt = now; if (m.ets) { const r = now - m.ets - m.edt; if (r >= 0 && r < 2000) this.rtt = this.rtt ? this.rtt * 0.9 + r * 0.1 : r; }
    if (m.hf && this.myH[m.hf] !== undefined && this.myH[m.hf] !== m.hv) this.desync = true;
    else if (m.hf && this.saved[m.hf] && m.hf <= this.remLast && this.rb < 0 && m.hf < this.f && SIM.hash(this.saved[m.hf]) !== m.hv) this.desync = true;
  };

  function begin(chars, g) { gid = g; sess = new Session(host ? 0 : 1, chars, tr); status = 'playing'; wantRematch = [false, false]; lastRecv = performance.now(); if (onStart) onStart(chars); }
  function onmsg(m) {
    if (!m || typeof m !== 'object') return; lastRecv = performance.now();
    if (m.t === 'hello' && host) { remCh = m.ch | 0; if (status === 'waiting') { status = 'starting'; const g = gid + 1; tr.send({ t: 'start', g: g, chars: [myCh, remCh & 1] }); begin([myCh, remCh & 1], g); } else if (sess && sess.remLast < 0) tr.send({ t: 'start', g: gid, chars: sess.s.cfg.chars }); }
    else if (m.t === 'start' && !host && (status === 'joining' || status === 'rematch')) begin([m.chars[0] & 1, m.chars[1] & 1], m.g | 0);
    else if (m.t === 'i' && sess && m.g === gid && Array.isArray(m.i)) sess.packet(m);
    else if (m.t === 'rematch') { if (host && sess && status === 'playing' && sess.s.phase !== 'over') { if (sess.remLast < 0) tr.send({ t: 'start', g: gid, chars: sess.s.cfg.chars }); } else { wantRematch[1] = true; checkRematch(); } }
    else if (m.t === 'bye') { status = 'lost'; err = 'OPPONENT LEFT'; }
  }
  function checkRematch() { if (host && wantRematch[0] && wantRematch[1]) { const c = sess.s.cfg.chars, g = gid + 1; tr.send({ t: 'start', g: g, chars: c }); begin(c, g); } else if (!host && wantRematch[0]) status = 'rematch'; }
  async function open(c, asHost, ch, cb) {
    close(); code = c; host = asHost; myCh = ch; onStart = cb; err = ''; status = 'connecting';
    try { tr = inArtifact ? await roomTransport(c) : await peerTransport(c, asHost); tr.onmsg = onmsg; status = asHost ? 'waiting' : 'joining'; helloT = 0; }
    catch (e) { status = 'error'; err = (e && e.message) || 'COULD NOT CONNECT'; tr = null; }
  }
  function pump() {
    if (!tr) return;
    if (status === 'joining' && (helloT++ % 30) === 0) tr.send({ t: 'hello', ch: myCh });
    if (status === 'joining' && helloT > 60 * 25) { status = 'error'; err = 'NO ONE IS HOSTING ROOM ' + code.toUpperCase(); }
    if (status === 'rematch' && (helloT++ % 30) === 0) tr.send({ t: 'rematch' });
    if ((status === 'playing') && performance.now() - lastRecv > 6000) { status = 'lost'; err = 'CONNECTION LOST'; }
  }
  function close() { if (tr) { try { tr.send({ t: 'bye' }); } catch (e) { } if (tr.stream) tr.stream.getTracks().forEach(x => x.stop()); tr.close(); } voice.on = false; voice.muted = false; voice.remote = false; voice.err = ''; if (audioEl) audioEl.srcObject = null; tr = null; sess = null; status = 'idle'; }
  function rematch() { if (!tr || wantRematch[0]) return; wantRematch[0] = true; tr.send({ t: 'rematch' }); checkRematch(); }
  function newCode() { const a = 'bcefgmnpqrtvxyz23456789'; let s = ''; const b = new Uint8Array(4); (window.crypto || { getRandomValues(x) { const t = Date.now(); for (let i = 0; i < 4; i++) x[i] = (t >> (i * 5)) & 255; } }).getRandomValues(b); for (let i = 0; i < 4; i++) s += a[b[i] % a.length]; return s; }
  return { voice, voiceToggle, open, close, pump, rematch, newCode, inArtifact, Session, get sess() { return sess; }, get status() { return status; }, get err() { return err; }, get code() { return code; }, get host() { return host; }, get kind() { return tr ? tr.kind : ''; }, get wantRematch() { return wantRematch; } };
})();
