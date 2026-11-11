// ===================================================================
// Game shell: scenes, menus, modes, fixed 60 Hz loop.
// ===================================================================
(function () {
  'use strict';
  const B = SIM.BT, F = SIM.F, c3 = document.getElementById('gl'), c2 = document.getElementById('hud'), wrap = document.getElementById('cab');
  const T = R.txt, RC = R.rect, CH = SIM.CH, mn = INP.menu;
  let opt = { level: 2, rounds: 2, boxes: false, dummy: 0 };
  try { Object.assign(opt, JSON.parse(localStorage.getItem('gk_opt') || '{}')); } catch (e) { }
  const saveOpt = () => { try { localStorage.setItem('gk_opt', JSON.stringify(opt)); } catch (e) { } };
  let scene = 'title', st = 0, cur = 0, hot = [], click = null, match = null, sel = null, cmd = null, arc = null, lob = null, bc = null, pan = { chars: [0, 1] };
  const inv = (location.hash.match(/^#join-([a-z0-9]{4})$/) || [])[1] || '';
  const PADN = ['CROSS', 'CIRCLE', 'SQUARE', 'TRIANGLE', 'L1', 'R1', 'L2', 'R2', 'SHARE', 'OPTIONS', 'L3', 'R3'];
  const DUMMY = ['STAND', 'CROUCH', 'JUMP', 'BLOCK ALL', 'CPU'];

  function go(s, keep) { scene = s; st = 0; if (!keep) cur = 0; AUD.music(s === 'fight' ? 'battle' : 'menu'); linkBar(false); }
  function nav(n, wrapAround) {
    let ok = mn.ok || mn.start;
    if (mn.up) { cur = (cur + n - 1) % n; AUD.play('move'); } if (mn.down) { cur = (cur + 1) % n; AUD.play('move'); }
    if (click) { for (const h of hot) if (click.x >= h.x && click.x < h.x + h.w && click.y >= h.y && click.y < h.y + h.h) { cur = h.i; ok = 1; } }
    if (ok) AUD.play('ok'); return ok;
  }
  function list(items, x, y, dy, w, al) {
    items.forEach((it, i) => { const on = i === cur, yy = y + i * dy; if (on) { RC(x - 8, yy - 4, w, dy - 2, '#d01f2e'); RC(x - 8, yy - 4, 3, dy - 2, '#f1dcb4'); } T(it, al === 'center' ? x - 8 + w / 2 : x, yy, 8, on ? '#fff' : '#cdbcae', al || 'left', 0, on ? '#5a0a14' : undefined); hot.push({ x: x - 8, y: yy - 4, w: w, h: dy, i: i }); });
  }
  function panel(x, y, w, h, title) { RC(x, y, w, h, 'rgba(10,6,18,0.86)'); RC(x, y, w, 2, '#d8322e'); RC(x, y + h - 2, w, 2, '#d8322e'); if (title) { RC(x, y, w, 16, '#d01f2e'); T(title, x + 8, y + 4, 8, '#fff', 'left', 0, '#5a0a14'); } }
  function foot(s) { T(s, 240, 258, 8, '#9a8488', 'center', 1); }
  function logo(y, k) { T('激突', 240, y, 64 * k, '#d8322e', 'center', 1, '#f1dcb4'); }

  // ------------------------------------------------------------ match control
  function startMatch(mode, chars, extra) {
    R.setFighters(chars); R.snap();
    match = Object.assign({ mode: mode, chars: chars, s: SIM.newMatch({ chars: chars, training: mode === 'training', rounds: opt.rounds }), paused: false, ai: null, dev: null, hist: [], lastIn: -1, adv: null, act: [0, 0], was: [true, true], ex: false, overT: 0 }, extra || {});
    if (mode === 'arcade') match.ai = AI.make(Math.min(3, opt.level + (arc.stage === 2 ? 1 : 0)));
    if (mode === 'training') match.ai = AI.make(opt.level);
    go('fight');
  }
  function dummyInput(s) {
    const d = s.p[1], a = s.p[0];
    switch (opt.dummy) {
      case 1: return B.D; case 2: return B.U;
      case 3: { const m = a.st === 'atk' ? CH[a.ch].mv[a.mv] : null; const low = m && (m.g === 'l' || (m.hits[0] && m.hits[0].g === 'l')); return (d.x >= a.x ? B.R : B.L) | (low ? B.D : 0); }
      case 4: return AI.think(match.ai, s, 1);
    } return 0;
  }
  function trainingTrack(s, i0) {
    if (i0 !== match.lastIn) { match.hist.unshift({ m: i0, f: 1 }); if (match.hist.length > 12) match.hist.pop(); match.lastIn = i0; } else if (match.hist[0] && match.hist[0].f < 99) match.hist[0].f++;
    for (let i = 0; i < 2; i++) { const idle = s.p[i].st === 'idle'; if (idle && !match.was[i]) match.act[i] = s.frame; match.was[i] = idle; }
    if (s.p[1].st === 'hit' || s.p[1].st === 'block') match.ex = true;
    if (match.ex && match.was[0] && match.was[1]) { match.adv = match.act[1] - match.act[0]; match.ex = false; }
    if (s.p[1].st === 'ahit' || s.p[1].st === 'kd') match.ex = false;
  }
  function fightTick() {
    const m = match;
    if (m.mode === 'online') {
      NET.pump(); const ss = NET.sess; if (!ss) { return; }
      ss.tick(INP.merged()); m.s = ss.s; R.events(ss.ev, m.s); AUD.events(ss.ev, m.s);
      if (NET.status === 'lost' || NET.status === 'error') { if (mn.ok || mn.start || mn.back || click) { NET.close(); go('menu'); } return; }
      if (m.s.phase === 'over') { m.overT++; if (m.overT > 50) { if (mn.ok || mn.start) { AUD.play('ok'); NET.rematch(); } if (mn.back) { NET.close(); go('menu'); } } }
      return;
    }
    if (m.paused) return pauseTick();
    if (m.s.phase !== 'over' && (mn.start || (click && click.y < 40 && click.x > 200 && click.x < 280))) { m.paused = true; cur = 0; AUD.play('ok'); return; }
    let i0, i1; const s = m.s;
    if (m.mode === 'versus') { i0 = INP.dev(m.dev[0]); i1 = INP.dev(m.dev[1]); }
    else { i0 = INP.merged(); i1 = m.mode === 'training' ? dummyInput(s) : AI.think(m.ai, s, 1); }
    if (m.mode === 'training' && mn.sel) { const c = s.cfg; m.s = SIM.newMatch({ chars: c.chars, training: true }); return; }
    SIM.step(s, i0, i1); R.events(s.ev, s); AUD.events(s.ev, s);
    if (m.mode === 'training') trainingTrack(s, i0);
    if (s.phase === 'over') {
      m.overT++; if (m.overT === 1) cur = 0;
      if (m.overT > 50) {
        if (m.mode === 'arcade') {
          if (s.winner === 0) { if (mn.ok || mn.start || click) { arc.stage++; if (arc.stage >= 3) go('ending'); else go('vs'); } }
          else { const k = nav(2); if (k) { if (cur === 0) go('vs'); else go('menu'); } }
        } else { const k = nav(3); if (k) { if (cur === 0) startMatch('versus', m.chars, { dev: m.dev }); else if (cur === 1) { selInit('versus', m.dev); } else go('menu'); } }
      }
    }
  }
  function pauseItems() { const m = match; return m.mode === 'training' ? ['RESUME', 'DUMMY: ' + DUMMY[opt.dummy], 'HITBOXES: ' + (opt.boxes ? 'ON' : 'OFF'), 'RESET POSITIONS', 'COMMAND LIST', 'QUIT TO MENU'] : ['RESUME', 'COMMAND LIST', 'QUIT TO MENU']; }
  function pauseTick() {
    const it = pauseItems(), m = match; if (mn.back) { m.paused = false; return; }
    const tr = m.mode === 'training';
    if (tr && cur === 1 && (mn.left || mn.right)) { opt.dummy = (opt.dummy + (mn.right ? 1 : DUMMY.length - 1)) % DUMMY.length; saveOpt(); AUD.play('move'); }
    if (!nav(it.length)) return; const name = it[cur];
    if (name === 'RESUME') m.paused = false; else if (name.indexOf('DUMMY') === 0) { opt.dummy = (opt.dummy + 1) % DUMMY.length; saveOpt(); }
    else if (name.indexOf('HITBOX') === 0) { opt.boxes = !opt.boxes; saveOpt(); } else if (name === 'RESET POSITIONS') { m.s = SIM.newMatch({ chars: m.chars, training: true }); m.paused = false; }
    else if (name === 'COMMAND LIST') { cmd = { ch: m.chars[0], from: 'fight' }; scene = 'cmd'; } else { go('menu'); }
  }
  const DIRG = ['', '↙', '↓', '↘', '←', '', '→', '↖', '↑', '↗'];
  function fightDraw() {
    const m = match, s = m.s; R.drawFx(s); if (opt.boxes && m.mode === 'training') R.boxes(s);
    const tag = m.mode === 'online' ? (NET.host ? ['YOU', ''] : ['', 'YOU']) : m.mode === 'versus' ? ['P1', 'P2'] : m.mode === 'arcade' ? ['P1', 'CPU'] : ['P1', 'DUMMY'];
    R.hud(s, { tag: tag });
    if (m.mode === 'training') {
      m.hist.forEach((h, i) => { const y = 60 + i * 11, d = SIM.npad(h.m, 1); T(('' + h.f).padStart(2, ' '), 6, y, 8, '#9a8488'); T(DIRG[d] || '·', 28, y - 2, 10, '#fff', 'left', 1); let x = 42; [['L', 16, '#f6e7c8'], ['M', 32, '#f1dcb4'], ['H', 64, '#ff5a3a'], ['SP', 128, '#f08a5a'], ['AS', 256, '#cdbcae'], ['DI', 512, '#ff5a3a'], ['PA', 1024, '#e9a58a']].forEach(b => { if (h.m & b[1]) { T(b[0], x, y, 8, b[2]); x += b[0].length * 8 + 4; } }); });
      if (m.adv !== null) T('FRAME ADV ' + (m.adv > 0 ? '+' : '') + m.adv, 240, 44, 8, m.adv >= 0 ? '#f08a5a' : '#ff7f8a', 'center');
      T('DUMMY: ' + DUMMY[opt.dummy], 474, 60, 8, '#9a8488', 'right'); T('START: MENU', 474, 71, 8, '#9a8488', 'right');
    }
    if (m.mode === 'online') {
      const ss = NET.sess; if (ss) { T((NET.kind === 'p2p' ? 'P2P ' : 'RELAY ') + Math.round(ss.rtt) + 'MS  RB' + ss.rbMax, 240, 40, 8, ss.desync ? '#d8322e' : '#9a8488', 'center'); if (st % 60 === 0) ss.rbMax = 0; if (ss.stall > 20) T('WAITING FOR OPPONENT...', 240, 100, 8, '#f1dcb4', 'center'); if (ss.desync) T('DESYNC DETECTED', 240, 50, 8, '#d8322e', 'center'); if (NET.voice.on) T(NET.voice.muted ? 'MIC MUTED' : 'MIC LIVE', 240, 238, 8, NET.voice.muted ? '#9a8488' : '#f08a5a', 'center'); }
      if (NET.status === 'lost' || NET.status === 'error') { panel(110, 100, 260, 60, 'ONLINE'); T(NET.err || 'CONNECTION LOST', 240, 124, 8, '#f1dcb4', 'center'); T('PRESS ANY BUTTON', 240, 142, 8, '#fff', 'center'); }
      else if (s.phase === 'over' && m.overT > 50) { panel(120, 150, 240, 66, s.winner < 0 ? 'DRAW GAME' : (s.winner === (NET.host ? 0 : 1) ? 'YOU WIN  勝利' : 'YOU LOSE  敗北')); T(NET.wantRematch[0] ? 'WAITING FOR OPPONENT...' : 'CONFIRM: REMATCH', 240, 176, 8, '#fff', 'center'); T(NET.wantRematch[1] ? 'OPPONENT WANTS A REMATCH' : 'BACK: LEAVE', 240, 194, 8, NET.wantRematch[1] ? '#f1dcb4' : '#cdbcae', 'center'); }
    } else if (s.phase === 'over' && m.overT > 50) {
      const w = s.winner; panel(130, 140, 220, 96, w < 0 ? 'DRAW GAME' : CH[s.p[w].ch].name + ' WINS  勝利');
      if (m.mode === 'arcade') { if (w === 0) { T(arc.stage >= 2 ? 'FINAL STAGE CLEAR' : 'STAGE ' + (arc.stage + 1) + ' CLEAR', 240, 172, 8, '#f1dcb4', 'center'); T('PRESS START', 240, 196, 8, '#fff', 'center'); } else list(['CONTINUE', 'MAIN MENU'], 190, 174, 20, 116, 'center'); }
      else list(['REMATCH', 'CHARACTER SELECT', 'MAIN MENU'], 170, 168, 20, 156, 'center');
    }
    if (m.paused) { const it = pauseItems(); panel(140, 70, 200, 40 + it.length * 20, 'PAUSE  一時停止'); list(it, 160, 98, 20, 176); }
  }

  // ------------------------------------------------------------ select
  function selInit(mode, dev) { sel = { mode: mode, c: [0, 1], lock: [false, false], dev: dev || null, prev: {}, t: 0 }; if (mode === 'arcade' || mode === 'online') sel.lock[1] = true; R.setFighters(sel.c); go('select'); }
  function selTick() {
    const s = sel; let done = false; s.t++;
    const before = s.c.join();
    const side = (i, l, r, ok, back) => { if (s.lock[i]) { if (back) { s.lock[i] = false; AUD.play('back'); } return false; } if (back) return true; if (l || r) { s.c[i] ^= 1; AUD.play('move'); } if (ok) { s.lock[i] = true; AUD.play('ok'); } return false; };
    if (s.mode === 'versus') {
      let exit = false;
      for (let i = 0; i < 2; i++) { const m = INP.dev(s.dev[i]), p = s.prev[i] === undefined ? 255 : s.prev[i], e = m & ~p; s.prev[i] = m; if (side(i, e & B.L, e & B.R, e & (B.LT | B.MD), e & B.HV)) exit = true; }
      if (exit && s.t > 10) { AUD.play('back'); go('menu'); return; }
    } else {
      const act = !s.lock[0] ? 0 : 1;
      if (mn.back) { AUD.play('back'); if (s.mode === 'training' && s.lock[0]) s.lock[0] = false; else { go('menu'); return; } }
      else if (click) { const i = click.x < 240 ? 0 : 1; if (s.mode === 'training' && i !== act && !(act === 1 && i === 0)) { } side(act, 0, 0, 1, 0); }
      else side(act, mn.left, mn.right, mn.ok || mn.start, 0);
      if (s.mode === 'arcade' || s.mode === 'online') s.c[1] = s.c[0] ^ 1;
    }
    if (s.c.join() !== before) R.setFighters(s.c);
    if (s.lock[0] && s.lock[1]) {
      if (s.mode === 'arcade') { arc = { ch: s.c[0], stage: 0 }; go('vs'); }
      else if (s.mode === 'online') lobInit(s.c[0]);
      else if (s.mode === 'training') startMatch('training', s.c.slice());
      else { arc = null; match = { pending: { mode: 'versus', chars: s.c.slice(), dev: s.dev } }; go('vs'); }
    }
  }
  function selDraw() {
    const s = sel; RC(0, 0, 480, 34, 'rgba(10,6,18,0.86)'); RC(0, 34, 480, 2, '#d8322e'); T('CHARACTER SELECT', 240, 8, 16, '#fff', 'center'); T('闘士選択', 240, 40, 12, '#f1dcb4', 'center', 1);
    for (let i = 0; i < 2; i++) {
      const C = CH[s.c[i]], x = i ? 300 : 20, active = !s.lock[i] && (s.mode === 'versus' || i === (s.lock[0] ? 1 : 0));
      panel(x, 176, 160, 66); T(C.name, x + 80, 184, 16, i ? '#e9a58a' : '#f1dcb4', 'center'); T(C.jp + '  ' + C.title, x + 80, 204, 8, '#fff', 'center', 1);
      const lab = s.mode === 'versus' ? 'P' + (i + 1) : i === 0 ? 'YOU' : s.mode === 'training' ? 'DUMMY' : s.mode === 'online' ? 'RIVAL' : 'CPU';
      T(s.lock[i] && (i === 0 || s.mode === 'versus' || s.mode === 'training') ? lab + ' READY' : active ? '◀ ' + lab + ' ▶' : lab, x + 80, 224, 8, active && (s.t >> 4) & 1 ? '#d8322e' : '#cdbcae', 'center', 1);
      hot.push({ x: x, y: 60, w: 160, h: 180, i: i });
    }
    T('VS', 240, 196, 16, '#d8322e', 'center'); foot(s.mode === 'versus' ? 'LEFT/RIGHT: CHANGE   LIGHT OR MEDIUM: LOCK IN   HEAVY: UNLOCK' : 'LEFT/RIGHT: CHANGE   CONFIRM: LOCK IN   BACK: RETURN');
  }

  // ------------------------------------------------------------ online lobby
  function linkBar(on, text) { const b = document.getElementById('linkbar'); b.hidden = !on; if (on) document.getElementById('link').value = text; }
  function lobInit(ch) { lob = { ch: ch, mode: inv ? 'join' : 'menu', code: inv ? inv.split('') : ['a', 'a', 'a', 'a'], pos: 0 }; go('lobby'); if (inv) NET.open(inv, false, ch, onNetStart); }
  function onNetStart(chars) { startMatch('online', chars); match.s = NET.sess.s; }
  const AL = 'abcdefghjkmnpqrstuvwxyz23456789';
  function lobTick() {
    const l = lob; NET.pump();
    if (l.mode === 'menu') { if (mn.back) { selInit('online'); return; } if (nav(2)) { if (cur === 0) { const c = NET.newCode(); l.mode = 'host'; NET.open(c, true, l.ch, onNetStart); try { location.hash = 'join-' + c; } catch (e) { } linkBar(true, NET.inArtifact ? 'Room code ' + c.toUpperCase() + ' (share this page, then ONLINE > JOIN)' : location.href.split('#')[0] + '#join-' + c); } else { l.mode = 'code'; } } return; }
    if (l.mode === 'code') {
      if (mn.back) { l.mode = 'menu'; AUD.play('back'); return; }
      for (const k in INP.keys) if (INP.keys[k] === 1 && /^(Key[A-Z]|Digit[2-9])$/.test(k)) { const chv = k.slice(-1).toLowerCase(); if (AL.indexOf(chv) >= 0 && !'wasduijkolh'.includes(chv)) { INP.keys[k] = 2; l.code[l.pos] = chv; l.pos = Math.min(3, l.pos + 1); } }
      if (mn.left) l.pos = (l.pos + 3) % 4; if (mn.right) l.pos = (l.pos + 1) % 4;
      if (mn.up || mn.down) { const i = AL.indexOf(l.code[l.pos]); l.code[l.pos] = AL[(i + (mn.up ? 1 : AL.length - 1)) % AL.length]; AUD.play('move'); }
      if (mn.ok || mn.start) { AUD.play('ok'); l.mode = 'join'; NET.open(l.code.join(''), false, l.ch, onNetStart); } return;
    }
    if (mn.back || ((NET.status === 'error') && (mn.ok || click))) { NET.close(); l.mode = 'menu'; linkBar(false); AUD.play('back');  }
  }
  function lobDraw() {
    const l = lob; panel(70, 46, 340, 176, 'ONLINE VERSUS  通信対戦'); T('YOU: ' + CH[l.ch].name, 240, 70, 8, '#f1dcb4', 'center');
    if (l.mode === 'menu') { list(['HOST A ROOM', 'JOIN WITH A CODE'], 150, 100, 22, 196, 'center'); T(NET.inArtifact ? 'INSIDE CLAUDE: BOTH PLAYERS MUST BE INVITED TO THIS PAGE.' : 'DIRECT PEER-TO-PEER WITH ROLLBACK NETCODE.', 240, 160, 8, '#9a8488', 'center', 1); T(NET.inArtifact ? 'THE HOSTED BUILD WORKS FOR ANYONE WITH THE LINK.' : 'HOST A ROOM, SEND THE LINK, FIGHT.', 240, 174, 8, '#9a8488', 'center', 1); }
    else if (l.mode === 'code') { T('ENTER ROOM CODE', 240, 96, 8, '#fff', 'center'); for (let i = 0; i < 4; i++) { const x = 170 + i * 38; RC(x, 116, 30, 36, i === l.pos ? '#d01f2e' : '#2a181e'); T(l.code[i].toUpperCase(), x + 15, 126, 16, '#fff', 'center'); } T('UP/DOWN: LETTER   LEFT/RIGHT: MOVE   CONFIRM: JOIN', 240, 172, 8, '#9a8488', 'center', 1); }
    else {
      const stt = NET.status; T('ROOM CODE', 240, 92, 8, '#cdbcae', 'center'); T((NET.code || l.code.join('')).toUpperCase(), 240, 108, 24, '#fff', 'center');
      const dots = '.'.repeat((st >> 4) % 4);
      T(stt === 'connecting' ? 'CONNECTING' + dots : stt === 'waiting' ? 'WAITING FOR CHALLENGER' + dots : stt === 'joining' ? 'KNOCKING ON THE DOJO DOOR' + dots : stt === 'error' ? NET.err : stt.toUpperCase(), 240, 150, 8, stt === 'error' ? '#d8322e' : '#f1dcb4', 'center');
      if (stt === 'waiting') T(NET.inArtifact ? 'FRIEND: OPEN THIS PAGE > ONLINE > JOIN > CODE' : 'SEND THE LINK BELOW TO YOUR RIVAL', 240, 172, 8, '#9a8488', 'center', 1);
      T('BACK: CANCEL', 240, 200, 8, '#9a8488', 'center');
    }
  }

  // ------------------------------------------------------------ scenes
  const MENU = [['ARCADE', 'アーケード', 'Fight the CPU through three stages.'], ['VERSUS', '対戦', 'Two players, one screen. Pads, sticks or keyboard.'], ['ONLINE', '通信対戦', 'Rollback netcode. Host a room and send the link.'], ['TRAINING', '修行', 'Frame data, hitboxes, input history, dummy settings.'], ['COMMAND LIST', '技表', 'Every move for both fighters.'], ['OPTIONS', '設定', 'Difficulty, rounds, volume, button config.']];
  const SC = {
    title: {
      tick() { if (mn.ok || mn.start || click || INP.merged() & INP.ATK) { AUD.init(); AUD.play('ok'); if (inv) selInit('online'); else go('menu'); } },
      draw() { const b = 1 + Math.max(0, 1 - st / 14) * 1.5; RC(0, 54, 480, 112, 'rgba(10,6,18,0.55)'); RC(0, 54, 480, 2, '#d8322e'); RC(0, 164, 480, 2, '#d8322e'); R.g.save(); R.g.translate(240, 100); R.g.scale(b, b); T('激突', 0, -42, 64, '#d8322e', 'center', 1, '#f1dcb4'); R.g.restore(); T('GEKITOTSU', 240, 132, 24, '#fff', 'center'); T('IRON FIST COLLISION', 240, 170, 8, '#f1dcb4', 'center');
        if ((st >> 5) & 1) T(inv ? 'INVITED TO ROOM ' + inv.toUpperCase() + ' - PRESS START' : 'PRESS START', 240, 204, 8, '#fff', 'center'); T('60 FPS FIXED STEP · ZERO RNG · ROLLBACK NETCODE', 240, 236, 8, '#9a8488', 'center', 1); T('BUILD ' + (window.GK_BUILD || 'dev'), 474, 4, 8, '#9a8488', 'right', 1); T(INP.padBlocked ? 'THIS VIEWER BLOCKS CONTROLLERS: KEYBOARD ONLY HERE. PADS WORK IN THE HOSTED BUILD.' : INP.padCount() ? INP.padCount() + ' CONTROLLER' + (INP.padCount() > 1 ? 'S' : '') + ' READY' : 'PLUG IN A PAD AND PRESS ANY BUTTON, OR USE THE KEYBOARD', 240, 250, 8, INP.padBlocked ? '#f1dcb4' : INP.padCount() ? '#f08a5a' : '#9a8488', 'center', 1); }
    },
    menu: {
      tick() { if (mn.back) { go('title'); return; } if (!nav(MENU.length)) return; const k = MENU[cur][0];
        if (k === 'ARCADE') selInit('arcade'); else if (k === 'VERSUS') { go('devs'); sel = { dev: [], prev: {} }; } else if (k === 'ONLINE') selInit('online'); else if (k === 'TRAINING') selInit('training'); else if (k === 'COMMAND LIST') { cmd = { ch: 0, from: 'menu' }; go('cmd'); } else go('options'); },
      draw() { logo(8, 0.5); T('GEKITOTSU', 240, 46, 8, '#fff', 'center'); panel(130, 66, 220, 150); list(MENU.map(m => m[0]), 150, 82, 22, 196); MENU.forEach((m, i) => { T(m[1], 336, 82 + i * 22 - 2, 12, i === cur ? '#f1dcb4' : '#5a4448', 'right', 1, i === cur ? '#5a0a14' : undefined); }); foot(MENU[cur][2]); }
    },
    devs: {
      tick() { if (mn.back && sel.dev.length === 0) { go('menu'); return; } if (mn.back) { sel.dev.pop(); AUD.play('back'); } const p = {}; for (const d of INP.devs) p[d.id] = d.m; if (st > 12 && sel.dev.length < 2) { for (const d of INP.devs) if ((d.m & INP.ATK) && !(sel.prev[d.id] & INP.ATK) && sel.dev.indexOf(d.id) < 0) { sel.dev.push(d.id); AUD.play('ok'); break; } } sel.prev = p; if (sel.dev.length === 2) { sel.t = (sel.t || 0) + 1; if (sel.t > 40) selInit('versus', sel.dev.slice()); } },
      draw() { panel(60, 50, 360, 160, 'VERSUS  対戦  CONTROLLER SETUP'); for (let i = 0; i < 2; i++) { const id = sel.dev[i], d = INP.devs.find(x => x.id === id); T('PLAYER ' + (i + 1), 80, 86 + i * 44, 8, i ? '#e9a58a' : '#f1dcb4'); T(id ? (d ? d.name : id.toUpperCase()) : (sel.dev.length === i && (st >> 4) & 1 ? 'PRESS AN ATTACK BUTTON' : '...'), 80, 102 + i * 44, 8, id ? '#f08a5a' : '#fff'); } T('KEYBOARD 1: WASD + J K L I     KEYBOARD 2: ARROWS + NUMPAD 1 2 3 5', 240, 180, 8, '#9a8488', 'center', 1); T(INP.padCount() + ' PAD(S) DETECTED', 240, 194, 8, '#9a8488', 'center'); foot('BACK: RETURN'); }
    },
    select: { tick: selTick, draw: selDraw },
    vs: {
      tick() { if (st === 1) { if (arc) { const opp = [arc.ch ^ 1, arc.ch, arc.ch ^ 1][arc.stage]; pan = { chars: [arc.ch, opp] }; } else pan = { chars: match.pending.chars }; R.setFighters(pan.chars); AUD.play('round'); } if (st > 110 || (st > 20 && (mn.ok || mn.start))) { if (arc) startMatch('arcade', pan.chars); else startMatch(match.pending.mode, match.pending.chars, { dev: match.pending.dev }); } },
      draw() { const a = CH[pan.chars[0]], b = CH[pan.chars[1]], k = Math.min(1, st / 12); RC(0, 96, 480, 70, 'rgba(10,6,18,0.8)'); RC(0, 96, 480, 2, '#d8322e'); RC(0, 164, 480, 2, '#d8322e'); T(a.name, -160 + 280 * k, 112, 24, '#f1dcb4', 'center'); T(a.jp, -160 + 280 * k, 140, 16, '#fff', 'center', 1); T(b.name, 640 - 280 * k, 112, 24, '#e9a58a', 'center'); T(b.jp, 640 - 280 * k, 140, 16, '#fff', 'center', 1); if (st > 12) T('VS', 240, 118, 24, '#d8322e', 'center'); if (arc) T(arc.stage === 2 ? 'FINAL STAGE  最終戦' : 'STAGE ' + (arc.stage + 1), 240, 74, 8, '#fff', 'center', 1); T('紅の屋上  CRIMSON ROOFTOP', 240, 176, 8, '#cdbcae', 'center', 1); }
    },
    fight: { tick: fightTick, draw: fightDraw },
    lobby: { tick: lobTick, draw: lobDraw },
    ending: {
      tick() { if (st === 1) AUD.play('win'); if (st > 60 && (mn.ok || mn.start || click)) go('menu'); },
      draw() { RC(0, 60, 480, 130, 'rgba(10,6,18,0.8)'); T('天下無双', 240, 70, 48, '#f1dcb4', 'center', 1, '#d01f2e'); T('ALL STAGES CLEAR', 240, 130, 16, '#fff', 'center'); T(CH[arc.ch].name + ' STANDS UNRIVALED UNDER HEAVEN', 240, 158, 8, '#cdbcae', 'center'); if (st > 60 && (st >> 5) & 1) T('PRESS START', 240, 176, 8, '#fff', 'center'); }
    },
    cmd: {
      tick() { cmd.pg = cmd.pg || 0; if (mn.left || mn.right || (click && click.y < 44)) { cmd.ch ^= 1; AUD.play('move'); } if (mn.up || mn.down) { cmd.pg = (cmd.pg + (mn.down ? 1 : 2)) % 3; AUD.play('move'); }
        const tap = click && click.y >= 44; if (tap && cmd.pg < 2) { cmd.pg++; AUD.play('move'); } else if (mn.back || mn.ok || mn.start || tap) { AUD.play('back'); if (cmd.from === 'fight') scene = 'fight'; else go('menu', 1), cur = 4; } },
      draw() { const C = CH[cmd.ch], pg = cmd.pg || 0; panel(16, 12, 448, 240, '◀  ' + C.name + ' ' + C.jp + '  COMMAND LIST  ▶'); let y = 34;
        const SYS = [['Normals', 'L   ↓L   M   ↓M   H   ↓H   ↘H', 'Punch, low kick, kick (M M M Triple Flash), low poke, roundhouse, uppercut, sweep.'], ['Assist', 'Assist + L / M / H / SP', 'Light kick, medium punch, heavy punch. With SP: Overdrive special (2 Drive bars).'],
          ['Drive Impact', 'Impact', 'Armoured strike. Pressed while blocking it is the Drive Reversal.'], ['Drive Parry', 'hold Parry', 'Slips whatever comes in and drains the gauge. The first frames are a Perfect Parry.'],
          ['Drive Rush', '→→ from Parry or a cancellable normal', 'A fast run that makes the next hit better. From a normal it costs 3 bars.'], ['Burnout', 'empty Drive gauge', 'No Drive moves, blocked specials chip you, and you are stuck longer when blocking.']];
        (pg === 0 ? C.cmd : pg === 1 ? C.cmd2 || [] : SYS).forEach(m => { T(m[0].toUpperCase(), 28, y, 8, '#f1dcb4'); T(m[1], 452, y - 2, 8, '#e9a58a', 'right', 1); T(m[2], 28, y + 10, 8, '#cdbcae', 'left', 1); y += 22; });
        T('▲▼   ' + ['SPECIALS & SUPERS', 'AIR, JUGGLES & GRAB', 'SYSTEM'][pg] + '   ' + (pg + 1) + ' / 3', 240, 238, 8, '#9a8488', 'center', 1); }
    },
    options: {
      tick() { const n = 6; if (mn.back) { saveOpt(); go('menu', 1); cur = 5; return; } const d = mn.right ? 1 : mn.left ? -1 : 0;
        if (d) { AUD.play('move'); if (cur === 0) opt.level = Math.max(1, Math.min(3, opt.level + d)); if (cur === 1) opt.rounds = Math.max(1, Math.min(3, opt.rounds + d)); if (cur === 2) AUD.setVol('mus', AUD.vol.mus + d * 0.1); if (cur === 3) AUD.setVol('sfx', AUD.vol.sfx + d * 0.1); saveOpt(); }
        if (nav(n)) { if (cur === 4) { bc = { wait: -1 }; go('buttons'); } else if (cur === 5) { saveOpt(); go('menu', 1); cur = 5; } } },
      draw() { panel(100, 40, 280, 180, 'OPTIONS  設定'); const bar = v => '■'.repeat(Math.round(v * 10)) + '□'.repeat(10 - Math.round(v * 10)); list(['CPU LEVEL', 'ROUNDS TO WIN', 'MUSIC', 'SOUND FX', 'BUTTON CONFIG', 'BACK'], 120, 70, 22, 256); const v = [['EASY', 'NORMAL', 'HARD'][opt.level - 1], '' + opt.rounds, bar(AUD.vol.mus), bar(AUD.vol.sfx), '▶', '']; v.forEach((s, i) => T(s, 364, 70 + i * 22 - (i === 2 || i === 3 ? 2 : 0), i === 2 || i === 3 ? 10 : 8, '#f1dcb4', 'right', i === 2 || i === 3 || i === 4)); foot('LEFT/RIGHT: CHANGE   BACK: RETURN'); }
    },
    buttons: {
      tick() { const A = INP.ACTS, n = A.length + 3;
        const cap = i => { bc.wait = i; bc.t = 0; INP.capture(btn => { const a = A[i][0]; for (const k in INP.pad) if (INP.pad[k] === btn) INP.pad[k] = bc.all ? -1 : INP.pad[a]; INP.pad[a] = btn; INP.savePad(); AUD.play('ok'); if (bc.all && i + 1 < A.length) cap(i + 1); else { bc.wait = -1; bc.all = false; } }); };
        if (bc.wait >= 0) { bc.t++; if (bc.t > 600 || INP.keys.Escape || INP.keys.Backspace) { INP.cancelCapture(); bc.wait = -1; bc.all = false; } return; } if (mn.back) { go('options', 1); cur = 4; return; }
        if (nav(n)) { if (cur < A.length) { bc.all = false; cap(cur); } else if (cur === A.length) { bc.all = true; cap(0); } else if (cur === A.length + 1) { Object.assign(INP.pad, INP.DEFPAD); INP.savePad(); } else { go('options', 1); cur = 4; } } },
      draw() { const A = INP.ACTS; panel(60, 10, 360, 242, 'CONTROLLER MAPPER  PAD / FIGHT STICK'); list(A.map(a => a[2]).concat(['MAP ALL BUTTONS IN ORDER', 'RESET TO PS LAYOUT', 'BACK']), 80, 32, 15, 336); A.forEach((a, i) => T(bc.wait === i ? ((bc.t >> 3) & 1 ? 'PRESS A BUTTON' : 'ESC = CANCEL') : INP.pad[a[0]] < 0 ? 'NOT SET' : (PADN[INP.pad[a[0]]] || 'BUTTON ' + INP.pad[a[0]]), 404, 32 + i * 15, 8, bc.wait === i ? '#fff' : '#f1dcb4', 'right'));
        const pads = INP.devs.filter(d => d.id.indexOf('pad') === 0), m = INP.merged(); let x = 80; T('TEST', x, 206, 8, '#9a8488'); x += 40; T(DIRG[SIM.npad(m, 1)] || '\u00b7', x, 203, 12, '#fff', 'left', 1); x += 22; [['L', 16], ['M', 32], ['H', 64], ['SP', 128], ['AS', 256], ['PA', 1024], ['DI', 512]].forEach(b => { T(b[0], x, 206, 8, m & b[1] ? '#d8322e' : '#3a2a30'); x += b[0].length * 8 + 6; });
        const raw = []; pads.forEach(d => d.raw.forEach((v, i) => { if (v) raw.push(PADN[i] && d.std ? PADN[i] : '#' + i); })); T(raw.join(' ').slice(0, 26), 404, 206, 8, '#f6e7c8', 'right');
        T(INP.padBlocked ? 'CONTROLLERS ARE BLOCKED IN THIS VIEWER - USE THE HOSTED BUILD' : pads.length ? pads.map(d => d.name + (d.std ? ' \u00b7 STANDARD LAYOUT' : ' \u00b7 CUSTOM LAYOUT') + ' \u00b7 ' + d.nb + ' BUTTONS').join('   ') : 'NO CONTROLLER SEEN YET - PRESS ANY BUTTON ON IT', 240, 222, 8, pads.length ? '#f08a5a' : '#f1dcb4', 'center', 1);
        T('DIRECTIONS READ FROM D-PAD, LEFT STICK AND HAT AUTOMATICALLY', 240, 234, 8, '#9a8488', 'center', 1); foot('KEYBOARD: WASD, J K L = LIGHT MEDIUM HEAVY, I = SPECIAL, SPACE = ASSIST, U = PARRY, O = IMPACT, H = THROW, K+L = RUN'); }
    }
  };

  // ------------------------------------------------------------ loop
  let vshow = null;
  function voiceUi() { const on = NET.kind === 'p2p' && (scene === 'fight' || scene === 'lobby'); if (on !== vshow) { vshow = on; document.getElementById('voicebar').hidden = !on; dispatchEvent(new Event('resize')); } if (!on) return; const v = NET.voice; document.getElementById('voice').textContent = !v.on ? 'TURN ON VOICE CHAT' : v.muted ? 'MIC MUTED - UNMUTE' : 'MIC LIVE - MUTE'; document.getElementById('voicest').textContent = v.err ? v.err : v.remote ? 'Connected to your opponent\u2019s audio' : v.on ? 'Waiting for your opponent to join voice' : 'Your opponent hears nothing until you turn it on'; }
  function tick() { st++; INP.poll(); if (st % 10 === 0) voiceUi(); if (scene !== 'title' && (mn.ok || mn.start)) AUD.init(); SC[scene].tick(); click = null; }
  function draw() {
    const inFight = scene === 'fight' || (scene === 'cmd' && cmd && cmd.from === 'fight');
    R.draw3d(inFight && match ? match.s : null, scene === 'select' ? { chars: sel.c, pose: sel.lock } : scene === 'vs' ? { chars: pan.chars, w: 350, y: 68, x: 80 } : scene === 'ending' ? { chars: [arc.ch, arc.ch ^ 1], pose: [true, false], w: 300, y: 40 } : null);
    R.clear(); hot = []; if (scene === 'cmd' && cmd.from === 'fight') fightDraw(); SC[scene].draw();
    if (scene !== 'fight' && scene !== 'title' && scene !== 'vs' && scene !== 'select' && scene !== 'ending') { /* scanline tint for menu screens */ }
  }
  let last = performance.now(), acc = 0;
  function frame(now) { requestAnimationFrame(frame); acc += Math.min(120, now - last); last = now; let n = 0; while (acc >= 16.6667 && n < 5) { tick(); acc -= 16.6667; n++; } if (n) draw(); }
  function vpos(e) { const r = c2.getBoundingClientRect(); return { x: (e.clientX - r.left) / r.width * 480, y: (e.clientY - r.top) / r.height * 270 }; }
  c2.addEventListener('pointerdown', e => { click = vpos(e); AUD.init(); window.focus(); });
  addEventListener('keydown', () => AUD.init(), { once: false });
  document.getElementById('fs').addEventListener('click', () => { const p = wrap.requestFullscreen ? wrap.requestFullscreen() : null; if (p && p.catch) p.catch(() => { }); });
  document.getElementById('voice').addEventListener('click', () => { NET.voiceToggle(); c2.focus(); });
  document.getElementById('copy').addEventListener('click', () => { const i = document.getElementById('link'); const done = () => { document.getElementById('copy').textContent = 'Copied'; }; if (navigator.clipboard && navigator.clipboard.writeText) navigator.clipboard.writeText(i.value).then(done).catch(() => { i.select(); }); else i.select(); });
  // touch: a floating stick for the left thumb, a cluster of round buttons for the right. A touch that lands between two buttons presses both.
  (function () {
    const el = document.getElementById('touch'), T = INP.touch, B = SIM.BT; if (!el) return;
    // eight attack buttons for one thumb: L M H SP along the bottom, and directly above each one its Assist version (AL AM AH AS)
    const DEF = [['L', B.LT, 7.65, 1.7, 0.92], ['M', B.MD, 5.6, 1.7, 0.92], ['H', B.HV, 3.55, 1.7, 0.92], ['SP', B.SP, 1.5, 1.7, 0.92],
      ['AL', B.AS | B.LT, 7.65, 3.85, 0.86, 2], ['AM', B.AS | B.MD, 5.6, 3.85, 0.86, 2], ['AH', B.AS | B.HV, 3.55, 3.85, 0.86, 2], ['AS', B.AS | B.SP, 1.5, 3.85, 0.86, 2],
      ['PA', B.PA, 9.9, 2.5, 0.8], ['DI', B.DI, 9.5, 4.75, 0.74], ['GRAB', B.LT | B.MD, 6.6, 5.85, 0.66, 1], ['RUN', B.MD | B.HV, 4.55, 5.85, 0.66, 1], ['II', 4096, 1.3, -1, 0.6, 1]];
    let btn = [], R = 34, stick = null, base = null, knob = null, live = false; const ptr = new Map();
    function layout() {
      const W = innerWidth, H = innerHeight, port = H > W; R = Math.max(24, Math.min(44, Math.min(W, H) * 0.086)); if (port) R = Math.max(24, Math.min(40, W * 0.062));
      const sr = Math.max(8, R * 0.3); el.innerHTML = ''; btn = [];
      for (const d of DEF) { const b = document.createElement('div'), r = R * d[4]; b.className = 'tb' + (d[5] === 1 ? ' sm' : d[5] === 2 ? ' au' : ''); b.textContent = d[0]; b.style.width = b.style.height = r * 2 + 'px'; b.style.fontSize = Math.max(7, r * (d[0].length > 2 ? 0.34 : 0.5)) + 'px';
        const x = W - sr - d[2] * R, y = d[3] < 0 ? sr + R * 0.9 : H - sr - d[3] * R; b.style.left = x + 'px'; b.style.top = y + 'px'; el.appendChild(b); btn.push({ el: b, bits: d[1], x, y, r }); }
      base = document.createElement('div'); base.className = 'stk'; base.style.width = base.style.height = R * 3.6 + 'px'; knob = document.createElement('div'); knob.className = 'knob'; knob.style.width = knob.style.height = R * 1.5 + 'px'; el.appendChild(base); el.appendChild(knob); rest();
    }
    function rest() { const sr = Math.max(8, R * 0.3), x = sr + R * 2.6, y = innerHeight - sr - R * 2.8; base.style.left = knob.style.left = x + 'px'; base.style.top = knob.style.top = y + 'px'; base.style.opacity = knob.style.opacity = 0.55; }
    function hits(x, y) { let out = [], best = null, bd = 1e9; for (const b of btn) { const d = Math.hypot(x - b.x, y - b.y); if (d < b.r * 1.42) out.push(b); if (d < bd) { bd = d; best = b; } } if (!out.length && best && bd < best.r * 2) out.push(best); return out; }
    function calc() {
      let m = 0; for (const b of btn) b.on = 0;
      for (const p of ptr.values()) {
        if (p.stick) { const dx = p.x - p.ox, dy = p.y - p.oy, d = Math.hypot(dx, dy), mx = R * 1.5;
          if (d > mx) { p.ox += dx * (1 - mx / d); p.oy += dy * (1 - mx / d); }                       // the base follows a thumb that wanders, so it never has to lift
          if (d > R * 0.42) { const a = Math.atan2(dy, dx), c = Math.cos(a), s2 = Math.sin(a); if (c > 0.38) m |= B.R; if (c < -0.38) m |= B.L; if (s2 < -0.42) m |= B.U; if (s2 > 0.42) m |= B.D; }
          base.style.left = p.ox + 'px'; base.style.top = p.oy + 'px'; const k = Math.min(1, mx / (d || 1)); knob.style.left = p.ox + dx * k + 'px'; knob.style.top = p.oy + dy * k + 'px'; base.style.opacity = knob.style.opacity = 1;
        } else for (const b of hits(p.x, p.y)) { m |= b.bits; b.on = 1; }
      }
      for (const b of btn) b.el.classList.toggle('on', !!b.on);
      if ((m & ~T.m & 0x1ff0) && navigator.vibrate) { try { navigator.vibrate(6); } catch (e) { } }
      T.m = m; T.on = 1;
    }
    function down(e) { e.preventDefault(); AUD.init(); const onBtn = hits(e.clientX, e.clientY).length && Math.min.apply(null, btn.map(b => Math.hypot(e.clientX - b.x, e.clientY - b.y) / b.r)) < 1.3, hasStick = [...ptr.values()].some(p => p.stick);
      const st = !onBtn && !hasStick && e.clientX < innerWidth * 0.5; ptr.set(e.pointerId, { stick: st, x: e.clientX, y: e.clientY, ox: e.clientX, oy: e.clientY }); try { el.setPointerCapture(e.pointerId); } catch (x) { } calc(); }
    function move(e) { const p = ptr.get(e.pointerId); if (!p) return; e.preventDefault(); p.x = e.clientX; p.y = e.clientY; calc(); }
    function up(e) { const p = ptr.get(e.pointerId); if (!p) return; e.preventDefault(); ptr.delete(e.pointerId); if (p.stick) rest(); calc(); }
    function enable() { if (live) return; live = true; document.body.classList.add('mob'); layout(); dispatchEvent(new Event('resize')); }
    el.addEventListener('pointerdown', down); el.addEventListener('pointermove', move); el.addEventListener('pointerup', up); el.addEventListener('pointercancel', up); el.addEventListener('contextmenu', e => e.preventDefault());
    addEventListener('resize', () => { if (live) layout(); }); addEventListener('blur', () => { ptr.clear(); if (live) { T.m = 0; calc(); } });
    if ((window.matchMedia && matchMedia('(pointer:coarse)').matches) || /[?&]touch=1/.test(location.search)) enable();
    addEventListener('pointerdown', e => { if (e.pointerType === 'touch') enable(); }, { passive: true });
  })();
  const boot = () => { R.init(c3, c2); R.setFighters([0, 1]); go('title'); AUD.music(null); requestAnimationFrame(frame); };
  if (document.fonts && document.fonts.load) Promise.all([document.fonts.load("16px 'Press Start 2P'"), document.fonts.load("16px 'DotGothic16'", '激突')]).then(boot, boot); else boot();
  window.GK = { get scene() { return scene; }, get match() { return match; }, startMatch, selInit, opt };
})();
