// ===================================================================
// Input (keyboard, gamepad / fight stick, touch) and synthesized audio.
// ===================================================================
const INP = (function () {
  'use strict';
  const B = SIM.BT, keys = {}, touch = { m: 0 }, ATK = 0x7f0;
  const KB1 = { KeyW: B.U, KeyS: B.D, KeyA: B.L, KeyD: B.R, KeyJ: B.LT, KeyK: B.MD, KeyL: B.HV, KeyI: B.SP, Space: B.AS, ShiftLeft: B.AS, KeyU: B.PA, KeyO: B.DI, KeyH: B.LT | B.MD };
  const KB2 = { ArrowUp: B.U, ArrowDown: B.D, ArrowLeft: B.L, ArrowRight: B.R, Numpad1: B.LT, Numpad2: B.MD, Numpad3: B.HV, Numpad5: B.SP, Numpad0: B.AS, Numpad4: B.PA, Numpad6: B.DI, NumpadAdd: B.LT | B.MD };
  const ACTS = [['LT', B.LT, 'LIGHT'], ['MD', B.MD, 'MEDIUM'], ['HV', B.HV, 'HEAVY'], ['SP', B.SP, 'SPECIAL'], ['AS', B.AS, 'ASSIST'], ['PA', B.PA, 'DRIVE PARRY'], ['DI', B.DI, 'DRIVE IMPACT'], ['TH', B.LT | B.MD, 'THROW (LIGHT+MEDIUM)']];
  const DEFPAD = { LT: 2, MD: 0, HV: 1, SP: 3, AS: 7, PA: 5, DI: 4, TH: 6 };
  let pad = Object.assign({}, DEFPAD);
  try { const s = JSON.parse(localStorage.getItem('gk_pad2') || 'null'); if (s) pad = Object.assign(pad, s); } catch (e) { }
  function savePad() { try { localStorage.setItem('gk_pad2', JSON.stringify(pad)); } catch (e) { } }
  addEventListener('keydown', e => { keys[e.code] = 1; if (KB1[e.code] || KB2[e.code] || e.code === 'Enter' || e.code === 'Backspace' || e.code === 'Tab') e.preventDefault(); });
  addEventListener('keyup', e => { keys[e.code] = 0; });
  addEventListener('blur', () => { for (const k in keys) keys[k] = 0; });
  function kbMask(map) { let m = 0; for (const k in map) if (keys[k]) m |= map[k]; return m; }
  const HAT = [[-1, B.U], [-0.714, B.U | B.R], [-0.428, B.R], [-0.142, B.D | B.R], [0.142, B.D], [0.428, B.D | B.L], [0.714, B.L], [1, B.U | B.L]];
  function padState(gp) {
    let m = 0; const b = i => i != null && i >= 0 && gp.buttons[i] && (gp.buttons[i].pressed || gp.buttons[i].value > 0.5);
    if (b(12)) m |= B.U; if (b(13)) m |= B.D; if (b(14)) m |= B.L; if (b(15)) m |= B.R;
    const ax = gp.axes[0] || 0, ay = gp.axes[1] || 0;
    if (ax < -0.5) m |= B.L; if (ax > 0.5) m |= B.R; if (ay < -0.5) m |= B.U; if (ay > 0.5) m |= B.D;
    if (gp.mapping !== 'standard' && gp.axes.length >= 10) { const h = gp.axes[9]; for (const e of HAT) if (Math.abs(h - e[0]) < 0.08) m |= e[1]; }
    for (const a of ACTS) if (b(pad[a[0]])) m |= a[1];
    return { m: m, ok: b(0), back: b(1), start: b(9), sel: b(8), raw: gp.buttons.map(x => x.pressed) };
  }
  let padBlocked = false, devs = [], prevMenu = {}, rep = { k: '', t: 0 }, capture = null, prevRaw = {};
  const menu = { up: 0, down: 0, left: 0, right: 0, ok: 0, back: 0, start: 0, sel: 0, dev: '' };
  function poll() {
    devs = [];
    devs.push({ id: 'kb1', name: 'KEYBOARD WASD', m: kbMask(KB1), ok: keys.Enter || keys.KeyJ, back: keys.Escape || keys.Backspace || keys.KeyK, start: keys.Enter, sel: keys.KeyR });
    devs.push({ id: 'kb2', name: 'KEYBOARD ARROWS', m: kbMask(KB2), ok: keys.Numpad1 || keys.NumpadEnter, back: keys.Numpad2, start: keys.NumpadEnter, sel: 0 });
    if (touch.m || touch.on) devs.push({ id: 'touch', name: 'TOUCH', m: touch.m & SIM.MASK, ok: touch.m & B.LT, back: touch.m & B.MD, start: touch.m & 4096, sel: 0 });
    let gps = []; if (!padBlocked && navigator.getGamepads) { try { gps = navigator.getGamepads() || []; } catch (e) { padBlocked = true; } }
    for (const gp of gps) {
      if (!gp || !gp.connected) continue; const st = padState(gp), id = 'pad' + gp.index;
      if (capture) { const pr = prevRaw[id] || []; for (let i = 0; i < st.raw.length; i++) if (st.raw[i] && !pr[i] && (i < 12 || i > 15)) { const c = capture; capture = null; c(i); break; } }
      prevRaw[id] = st.raw;
      devs.push({ id: id, name: (gp.id || 'GAMEPAD').replace(/\(.*?\)/g, '').trim().slice(0, 30).toUpperCase(), std: gp.mapping === 'standard', nb: gp.buttons.length, raw: st.raw, m: st.m, ok: st.ok, back: st.back, start: st.start, sel: st.sel });
    }
    const cur = { up: 0, down: 0, left: 0, right: 0, ok: 0, back: 0, start: 0, sel: 0 }; let who = {};
    for (const d of devs) { const set = (k, v) => { if (v) { cur[k] = 1; who[k] = d.id; } }; set('up', d.m & B.U); set('down', d.m & B.D); set('left', d.m & B.L); set('right', d.m & B.R); set('ok', d.ok); set('back', d.back); set('start', d.start); set('sel', d.sel); }
    for (const k in cur) { menu[k] = cur[k] && !prevMenu[k] ? 1 : 0; if (menu[k]) menu.dev = who[k]; }
    const dk = cur.up ? 'up' : cur.down ? 'down' : cur.left ? 'left' : cur.right ? 'right' : '';
    if (dk && dk === rep.k) { rep.t++; if (rep.t > 22 && rep.t % 6 === 0) menu[dk] = 1; } else { rep.k = dk; rep.t = 0; }
    prevMenu = cur;
  }
  const dev = id => { for (const d of devs) if (d.id === id) return d.m; return 0; };
  const merged = () => { let m = 0; for (const d of devs) m |= d.m; return m; };
  return { poll, dev, merged, menu, ATK, get devs() { return devs; }, pad, ACTS, DEFPAD, savePad, touch, keys, get padBlocked() { return padBlocked; },
    capture(cb) { capture = cb; }, cancelCapture() { capture = null; }, padCount() { return devs.filter(d => d.id.indexOf('pad') === 0).length; } };
})();

const AUD = (function () {
  'use strict';
  let ac = null, sfxG, musG, nbuf, vol = { sfx: 0.8, mus: 0.45 }, timer = null, step = 0, nextT = 0, track = null;
  try { const v = JSON.parse(localStorage.getItem('gk_vol') || 'null'); if (v) vol = v; } catch (e) { }
  function init() {
    if (ac) { if (ac.state === 'suspended') ac.resume(); return; }
    const AC = window.AudioContext || window.webkitAudioContext; if (!AC) return; ac = new AC();
    const comp = ac.createDynamicsCompressor(); comp.connect(ac.destination);
    sfxG = ac.createGain(); sfxG.gain.value = vol.sfx; sfxG.connect(comp); musG = ac.createGain(); musG.gain.value = vol.mus * 0.5; musG.connect(comp);
    nbuf = ac.createBuffer(1, ac.sampleRate, ac.sampleRate); const d = nbuf.getChannelData(0); let x = 22222; for (let i = 0; i < d.length; i++) { x = (Math.imul(x, 1664525) + 1013904223) >>> 0; d[i] = x / 2147483648 - 1; }
  }
  function tone(type, f0, f1, dur, g, when, dest) {
    if (!ac) return; const t = (when || ac.currentTime), o = ac.createOscillator(), v = ac.createGain(); o.type = type; o.frequency.setValueAtTime(f0, t); if (f1 !== f0) o.frequency.exponentialRampToValueAtTime(Math.max(20, f1), t + dur);
    v.gain.setValueAtTime(g, t); v.gain.exponentialRampToValueAtTime(0.001, t + dur); o.connect(v); v.connect(dest || sfxG); o.start(t); o.stop(t + dur + 0.02);
  }
  function nz(dur, g, type, f0, f1, q, when, dest) {
    if (!ac) return; const t = (when || ac.currentTime), s = ac.createBufferSource(), f = ac.createBiquadFilter(), v = ac.createGain(); s.buffer = nbuf; s.loop = true; f.type = type; f.Q.value = q || 1; f.frequency.setValueAtTime(f0, t); f.frequency.exponentialRampToValueAtTime(Math.max(30, f1), t + dur);
    v.gain.setValueAtTime(g, t); v.gain.exponentialRampToValueAtTime(0.001, t + dur); s.connect(f); f.connect(v); v.connect(dest || sfxG); s.start(t, (t * 7.3) % 0.9); s.stop(t + dur + 0.02);
  }
  const S = {
    hit0() { nz(0.07, 0.7, 'bandpass', 2400, 700, 1.2); tone('sine', 190, 70, 0.09, 0.8); },
    hit1() { nz(0.13, 1.0, 'lowpass', 3800, 300, 0.8); tone('sine', 150, 42, 0.18, 1.1); tone('square', 90, 40, 0.07, 0.35); },
    hit2() { nz(0.24, 1.2, 'lowpass', 5200, 160, 0.7); tone('sine', 130, 30, 0.34, 1.3); tone('sawtooth', 70, 28, 0.2, 0.5); nz(0.05, 0.8, 'highpass', 5000, 3000, 1); },
    block() { tone('square', 880, 420, 0.05, 0.3); nz(0.06, 0.5, 'highpass', 3500, 2000, 2); tone('sine', 240, 150, 0.06, 0.4); },
    whiff0() { nz(0.07, 0.22, 'bandpass', 1200, 2600, 2); }, whiff1() { nz(0.11, 0.3, 'bandpass', 700, 2200, 1.6); }, whiff2() { nz(0.16, 0.36, 'bandpass', 400, 2000, 1.4); },
    fire() { tone('sawtooth', 220, 880, 0.22, 0.3); nz(0.3, 0.4, 'bandpass', 500, 3000, 3); },
    superFire() { tone('sawtooth', 110, 1400, 0.5, 0.45); nz(0.7, 0.7, 'bandpass', 300, 4200, 2); tone('sine', 60, 40, 0.6, 0.9); },
    super() { const t = ac ? ac.currentTime : 0; [0, 0.06, 0.12, 0.18, 0.24].forEach((d, i) => tone('square', 330 * Math.pow(1.335, i), 330 * Math.pow(1.335, i), 0.12, 0.3, t + d)); nz(0.6, 0.5, 'highpass', 800, 7000, 1); tone('sine', 55, 35, 0.7, 1.0); },
    ex() { tone('square', 660, 1320, 0.12, 0.3); nz(0.12, 0.3, 'highpass', 3000, 6000, 1); },
    ko() { nz(0.9, 1.3, 'lowpass', 6000, 80, 0.6); tone('sine', 110, 24, 1.1, 1.4); tone('sawtooth', 220, 30, 0.7, 0.5); },
    wb() { nz(0.3, 1.1, 'lowpass', 3000, 120, 0.8); tone('sine', 90, 30, 0.35, 1.2); tone('square', 60, 30, 0.12, 0.5); },
    kd() { nz(0.16, 0.7, 'lowpass', 900, 100, 1); tone('sine', 80, 38, 0.2, 0.9); },
    jump() { nz(0.07, 0.14, 'bandpass', 600, 1500, 2); }, land() { nz(0.06, 0.3, 'lowpass', 700, 200, 1); }, dash() { nz(0.12, 0.25, 'bandpass', 900, 300, 1.5); },
    grab() { nz(0.08, 0.5, 'bandpass', 900, 500, 2); tone('square', 200, 120, 0.06, 0.3); }, tech() { tone('square', 1200, 600, 0.09, 0.4); nz(0.1, 0.5, 'highpass', 4000, 2000, 2); },
    move() { tone('square', 660, 660, 0.04, 0.16); }, ok() { tone('square', 660, 660, 0.06, 0.2); tone('square', 990, 990, 0.1, 0.2, ac ? ac.currentTime + 0.06 : 0); }, back() { tone('square', 440, 220, 0.09, 0.2); },
    round() { const t = ac ? ac.currentTime : 0; tone('square', 392, 392, 0.14, 0.26, t); tone('square', 523, 523, 0.14, 0.26, t + 0.14); tone('square', 784, 784, 0.3, 0.26, t + 0.28); nz(0.3, 0.3, 'highpass', 6000, 3000, 1, t + 0.28); },
    fight() { tone('sawtooth', 196, 784, 0.22, 0.4); nz(0.35, 0.6, 'bandpass', 800, 5000, 1); tone('sine', 80, 40, 0.3, 0.8); },
    win() { const t = ac ? ac.currentTime : 0; [523, 659, 784, 1047, 784, 1047].forEach((f, i) => tone('square', f, f, 0.16, 0.24, t + i * 0.13)); }
  };
  function play(n) { if (ac && S[n]) S[n](); }
  // --- music: 8-bar loop on the in-sen scale, scheduled ahead on the audio clock
  const BASS = [-24, -24, -23, -23, -24, -24, -26, -29], BR = [1, 0, 1, 2, 1, 0, 1, 2, 1, 0, 1, 2, 1, 0, 2, 1];
  const LEAD = [[0, , 5, 7, , 5, 7, 10], [12, , 10, 7, , 5, 1, 0], [1, , 5, 8, , 5, 8, 13], [12, 10, 8, , 5, , 1, ,], [0, , 5, 7, , 10, 12, ,], [17, , 12, 10, , 7, 10, 12], [10, , 7, 5, , 7, 10, 12], [7, , 4, 7, , 11, 8, 4]];
  const hz = n => 440 * Math.pow(2, n / 12);
  function sched() {
    if (!ac || !track) return; const spb = 60 / track.bpm / 4;
    while (nextT < ac.currentTime + 0.12) {
      const bar = (step >> 4) & 7, st = step & 15, t = nextT, root = BASS[bar];
      if (BR[st]) tone('sawtooth', hz(root + (BR[st] === 2 ? 12 : 0)), hz(root + (BR[st] === 2 ? 12 : 0)), spb * 0.9, 0.34, t, musG);
      if (st % 2 === 0) { const n = LEAD[bar][st >> 1]; if (n != null) { tone('square', hz(n), hz(n), spb * 1.8, 0.2, t, musG); tone('square', hz(n) * 1.006, hz(n) * 1.006, spb * 1.8, 0.1, t + spb * 1.5, musG); } }
      tone('triangle', hz(root + 24 + [0, 7, 12, 7][st & 3]), hz(root + 24 + [0, 7, 12, 7][st & 3]), spb * 0.8, 0.12, t, musG);
      if (track.drums) { if (st % 4 === 0 || st === 10) { tone('sine', 150, 40, 0.11, 0.9, t, musG); } if (st === 4 || st === 12) nz(0.1, 0.5, 'highpass', 1800, 1200, 0.8, t, musG); if (st % 2 === 1) nz(0.025, 0.16, 'highpass', 8000, 7000, 1, t, musG); }
      nextT += spb; step++;
    }
  }
  function music(kind) {
    if (!ac) return; const tr = kind === 'battle' ? { bpm: 156, drums: 1, k: kind } : kind === 'menu' ? { bpm: 112, drums: 0, k: kind } : null;
    if (track && tr && track.k === tr.k) return; track = tr; step = 0; nextT = ac.currentTime + 0.05; if (timer) clearInterval(timer); timer = tr ? setInterval(sched, 25) : null;
  }
  function setVol(k, v) { vol[k] = Math.max(0, Math.min(1, v)); if (ac) { sfxG.gain.value = vol.sfx; musG.gain.value = vol.mus * 0.5; } try { localStorage.setItem('gk_vol', JSON.stringify(vol)); } catch (e) { } }
  function events(ev, s) {
    for (const e of ev) {
      if (e.t === 'hit') play(e.ko ? 'ko' : 'hit' + e.lvl); else if (e.t === 'block') play('block'); else if (e.t === 'whiff') play('whiff' + e.lvl);
      else if (e.t === 'fire') play(e.big ? 'superFire' : 'fire'); else if (e.t === 'super') play('super'); else if (e.t === 'ex' || e.t === 'di' || e.t === 'rush') play('ex'); else if (e.t === 'parry' || e.t === 'armor') play('block'); else if (e.t === 'pparry' || e.t === 'splat' || e.t === 'burnout') play('tech');
      else if (e.t === 'wbounce' || e.t === 'gbounce') play('wb'); else if (e.t === 'kd') play('kd'); else if (e.t === 'jump' || e.t === 'rise') play('jump'); else if (e.t === 'land') play('land'); else if (e.t === 'dash') play('dash');
      else if (e.t === 'grab') play('grab'); else if (e.t === 'tech' || e.t === 'clash') play('tech'); else if (e.t === 'round') play('round'); else if (e.t === 'fight') play('fight'); else if (e.t === 'winpose') play('win');
    }
  }
  return { init, play, music, setVol, events, vol };
})();

// ===================================================================
// CPU opponent. Rule-based; every choice is a pure function of the
// match state, so the same situation always produces the same play.
// ===================================================================
const AI = (function () {
  'use strict';
  const B = SIM.BT, F = SIM.F, FW = 2048, BK = 4096;
  const D = B.D, U = B.U, L = B.LT, M = B.MD, H = B.HV, SP = B.SP, AS = B.AS, DI = B.DI, PA = B.PA;
  function make(level) { return { q: [], level: level, hold: 0, cool: 0 }; }
  function seq(parts) { const o = []; for (const p of parts) for (let i = 0; i < (p[1] || 1); i++) o.push(p[0]); return o; }
  const auto = b => [[AS | b, 2], [AS, 9], [AS | b, 2], [AS, 11], [AS | b, 2], [AS, 24]];
  const drc = [[D | M, 2], [0, 3], [FW, 2], [0, 2], [FW, 2], [0, 13], [M, 2], [0, 10], [H, 2], [0, 10], [FW | SP, 2], [0, 20]];
  function think(ai, s, me) {
    const p = s.p[me], o = s.p[1 - me], lv = ai.level;
    const out = m => { let r = m & SIM.MASK; if (m & FW) r |= p.face > 0 ? B.R : B.L; if (m & BK) r |= p.face > 0 ? B.L : B.R; return r; };
    if (s.phase !== 'fight') { ai.q.length = 0; return 0; }
    if (ai.q.length) { if (p.st === 'hit' || p.st === 'ahit' || p.st === 'kd' || p.st === 'thrown' || p.st === 'block') ai.q.length = 0; else return out(ai.q.shift()); }
    const dist = Math.abs(o.x - p.x) / F, r = n => ((R.hsh(s.frame * 31 + p.x + o.x * 7 + o.hp * 13 + n * 977) * 100) | 0);
    const om = o.st === 'atk' ? SIM.CH[o.ch].mv[o.mv] : null, grounded = p.y <= 0 && p.st === 'idle';
    const push = parts => { ai.q = seq(parts); return out(ai.q.shift()); };
    const kaito = p.ch === 0, drive = !p.bo && p.dr >= 2000, lowHit = om && om.hits[0] && om.hits[0].g === 'l', hiHit = (om && om.hits[0] && om.hits[0].g === 'h' && !om.isAir);
    const myProj = s.proj.some(q => q.o === me), incoming = s.proj.some(q => q.o !== me && Math.abs(q.x - p.x) < 160 * F && (q.x - p.x) * q.dir < 0);
    if (p.st === 'block') { if (lv >= 3 && drive && p.dr >= 3000 && r(9) < 6) return out(DI); return out(BK | (lowHit ? D : 0)); }
    if (p.st === 'kd' || p.st === 'ahit') return out(r(10) < 40 ? BK : 0);
    if (grounded && om && om.di && o.mf < 22 && dist < 170) { const k = r(11); if (!p.bo && k < [0, 15, 55, 90][lv]) return out(DI); if (!p.bo && k < [0, 30, 75, 96][lv]) return push([[PA, 30]]); return out(BK); }
    const threat = (om && dist < 150 && o.mf <= om.lastAct + 1) || incoming;
    if (grounded && threat) {
      if (om && om.thr) { if (r(1) < lv * 25) return out(L | M); }
      if (o.y > 0 && dist < 95 && r(2) < [0, 20, 50, 85][lv]) return push([[FW | SP, 2], [0, 6]]);
      if (incoming && !om && drive && lv >= 2 && r(12) < 30) return push([[PA, 16]]);
      if (r(3) < [0, 35, 65, 92][lv]) return out(BK | (lowHit && !hiHit && o.y <= 0 ? D : 0));
    }
    if (!grounded) { if (p.st === 'jump' && p.vy < 100 && dist < 90 && r(4) < 60 + lv * 10) return out(H); if (p.st === 'drush' && p.sf >= 10 && dist < 80) return push([[M, 2], [0, 10], [H, 2], [0, 10], [FW | SP, 2], [0, 12]]); return out(ai.hold); }
    if (o.y > 20 * F && o.vy < 300 && dist < 105 && o.st !== 'ahit' && r(5) < [0, 20, 50, 85][lv]) return push(r(6) < 70 ? [[FW | SP, 2], [0, 6]] : [[D | H, 2], [0, 6]]);
    const open = (om && !om.isAir && o.mf > om.lastAct + 1 && om.t - o.mf > 9) || (o.st === 'land' && o.rec > 6) || o.st === 'prec' || (o.st === 'hit' && o.crum && o.stun > 30);
    if (open && dist < 78 && r(7) < [0, 40, 78, 98][lv]) return push(p.sa >= 2000 || lv < 2 ? auto(M) : auto(H));
    if (ai.cool > 0) { ai.cool--; return out(ai.hold); }
    ai.cool = [0, 16, 9, 4][lv];
    const k = r(8);
    if (o.st === 'kd' || o.st === 'getup') { if (dist > 64) return out(ai.hold = FW); ai.hold = 0; if (o.st === 'getup' && o.sf > 12) { if (k < 30) return out(L | M); if (k < 70) return push(auto(L)); return out(BK | D); } return 0; }
    if (dist > 240) {
      if (kaito && k < 42 && !myProj) return push([[SP, 2], [0, 24]]);
      if (k < 58) { ai.hold = FW; return out(FW); }
      if (drive && lv >= 2 && k < 72) return push([[FW], [0], [FW | PA, 2], [PA, 4], [0, 4]]);
      if (k < 80) return push([[FW], [0], [FW, 2], [0, 14]]);
      if (k < 90) return push([[U | FW, 3], [0, 26], [H, 2], [0, 10]]);
      ai.hold = 0; return 0;
    }
    if (dist > 80) {
      if (dist < 108 && k < 26) return push(lv >= 2 && p.dr >= 4000 && !p.bo && r(13) < 55 ? drc : [[D | M, 2], [D, 4], [SP, 2], [0, 18]]);
      if (dist < 116 && k < 38) return push([[BK | H, 2], [0, 14]]);
      if (dist < 112 && k < 46) return push([[FW | M, 2], [0, 14]]);
      if (dist < 104 && k < 54) return push([[D | FW | H, 2], [D, 14]]);
      if (!p.bo && dist < 130 && k < 60 && lv >= 2) return out(DI);
      if (k < 68 && dist > 110 && dist < 190) return push([[U | FW, 3], [0, 27], [H, 2], [0, 12]].concat(auto(M)));
      if (!kaito && k < 74 && dist < 170) return push([[SP, 2], [0, 16]]);
      if (kaito && k < 74 && dist > 150 && !myProj) return push([[SP, 2], [0, 22]]);
      if (kaito && k < 78 && dist < 150) return push([[D | SP, 2], [0, 22]]);
      if (k < 92) { ai.hold = FW; return out(FW); }
      ai.hold = BK; return out(BK);
    }
    ai.hold = 0;
    if (k < 22) return push(auto(L));
    if (k < 42) return push(auto(M));
    if (k < 52) return push(auto(H));
    if (k < 64) return out(L | M);
    if (k < 72) return push([[FW | H, 2], [0, 22]]);
    if (k < 80 && lv >= 2 && p.dr >= 4000 && !p.bo) return push(drc);
    if (k < 88) return push([[BK], [0], [BK, 2], [0, 16]]);
    ai.hold = BK | D; return out(BK | D);
  }
  return { make, think };
})();
