// The reference for the C++ rules: plays the browser game's own sim.js under Node and writes, for every frame,
// the inputs used and a checksum of the whole match state. core/check.cpp replays the inputs through the C++
// and must reproduce every line. Test tool only; nothing here ships with the game.
//   node tools/oracle.js <game dir> <mode> <frames> <seed> <out file> [detail frame]
//   mode: random | mash | cpu (browser CPU both sides) | cpuhuman (CPU against random) | training | force (mash, and every so often any move at all is started outright)
const fs = require('fs'), vm = require('vm'), path = require('path');
const [dir, mode, framesS, seedS, out, detailS] = process.argv.slice(2);
const frames = +framesS, detail = detailS ? +detailS : -1;
const io = fs.readFileSync(path.join(dir, 'io.js'), 'utf8'); const aiAt = io.indexOf('const AI = (function () {');
const src = fs.readFileSync(path.join(dir, 'sim.js'), 'utf8') + '\n;var R = { hsh: function (n) { n = Math.imul(n ^ 0x9e3779b9, 0x85ebca6b); n ^= n >>> 13; n = Math.imul(n, 0xc2b2ae35); return ((n ^ (n >>> 16)) >>> 0) / 4294967296; } };\n' + io.slice(aiAt) + '\n;globalThis.SIM = SIM; globalThis.AI = AI;';
vm.runInThisContext(src);
const ST = ['idle', 'prejump', 'jump', 'land', 'dashf', 'dashb', 'run', 'drush', 'atk', 'block', 'hit', 'ahit', 'kd', 'getup', 'thrown', 'throwing', 'win', 'parry', 'prec', 'tech'];
const SUB = ['stand', 'crouch', 'walkf', 'walkb'], PH = ['intro', 'fight', 'ko', 'over'], KO = ['', 'ko', 'perfect', 'time', 'dko', 'draw'], AC = ['', 'L', 'LC', 'M', 'H'];
const EV = ['hit', 'block', 'parry', 'pparry', 'armor', 'diclash', 'clash', 'splat', 'kd', 'gbounce', 'wbounce', 'jump', 'walljump', 'land', 'dash', 'run', 'rush', 'parryOn', 'home', 'rise', 'fire', 'whiff', 'grab', 'tech', 'pend',
  'super', 'ex', 'di', 'special', 'burnout', 'recover', 'round', 'fight', 'ko', 'winpose', 'over'];
const MVK = Object.keys(SIM.CH[0].mv), mvi = k => { const i = MVK.indexOf(k); return i; };
const ix = (a, v) => { const i = a.indexOf(v); if (i < 0) throw new Error('unknown name ' + v); return i; };
const PF = 'x y vx vy pv face hp sa dr bo drT sf subf mf hitMask contact hitOK hs stun prev d66 d44 combo comboDmg jug wbUsed gbUsed wbPend gbPend jd rec cb hitHi tinv wins back red redT air2 noTech ca rq part jg cxSeq fum mseq cs lastSeq acq rcn sshift drs pok roll hard arm armHit drb mod aci crum pp plock half hm hmc fr hmE dj wj aa tk tf pmT pmN'.split(' ');
function dump(s, names) {
  const o = []; const put = (n, v) => { o.push(v | 0); if (names) names.push(n); };
  put('frame', s.frame); put('slow', s.slow); put('phase', ix(PH, s.phase)); put('pt', s.pt); put('kx', s.kx); put('round', s.round); put('time', s.time); put('freeze', s.freeze); put('frzWho', s.frzWho); put('winner', s.winner); put('rw', s.rw); put('koType', ix(KO, s.koType));
  s.p.forEach((p, i) => {
    for (const f of PF) put('p' + i + '.' + f, p[f] || 0);
    put('p' + i + '.st', ix(ST, p.st)); put('p' + i + '.sub', ix(SUB, p.sub)); put('p' + i + '.mv', p.mv ? mvi(p.mv) : -1); put('p' + i + '.fuq', p.fuq ? mvi(p.fuq) : -1); put('p' + i + '.ac', ix(AC, p.ac));
    for (let k = 0; k < 8; k++) put('p' + i + '.age' + k, p.age[k]); for (let k = 0; k < 8; k++) put('p' + i + '.pd' + k, p.pd[k]);
  });
  put('nshots', s.proj.length);
  s.proj.forEach((q, i) => { for (const f of ['o', 'x', 'y', 'dir', 'left', 'cd', 'pause', 'life', 'mod']) put('shot' + i + '.' + f, q[f]); put('shot' + i + '.k', mvi(q.k)); });
  put('nev', s.ev.length);
  s.ev.forEach((e, i) => {
    let a = 0, b = 0, c = 0, d = 0, ee = 0, f = 0;
    switch (e.t) {
      case 'hit': a = e.lvl; b = e.dmg; c = e.combo; d = e.total; ee = e.ch || 0; f = (e.thr ? 1 : 0) | (e.ko ? 2 : 0) | (e.sp ? 4 : 0) | (e.di ? 8 : 0) | (e.crum ? 16 : 0) | (e.free ? 32 : 0) | (e.fin ? 64 : 0) | (e.wrong === 'l' ? 128 : 0) | (e.wrong === 'h' ? 256 : 0); break;
      case 'block': case 'whiff': a = e.lvl || 0; break; case 'splat': a = e.stun || 0; break; case 'jump': a = e.dj || 0; break; case 'home': a = e.od || 0; break; case 'fire': a = e.big || 0; break;
      case 'super': a = e.lv || 0; b = e.ca || 0; c = mvi(e.k); break; case 'special': c = mvi(e.k); break; case 'round': a = e.n; break; case 'ko': a = ix(KO, e.type); break;
    }
    const who = e.who !== undefined ? e.who : e.w !== undefined ? e.w : -1;
    [['type', ix(EV, e.t)], ['who', who], ['x', e.x || 0], ['y', e.y || 0], ['a', a], ['b', b], ['c', c], ['d', d], ['e', ee], ['f', f]].forEach(q => put('ev' + i + '.' + q[0], q[1]));
  });
  return o;
}
let rs = (+seedS >>> 0) || 2463534242; const rnd = () => { rs ^= rs << 13; rs >>>= 0; rs ^= rs >>> 17; rs ^= rs << 5; rs >>>= 0; return rs; };
const inp = [0, 0], hold = [0, 0], script = [[], []];
// "mash": a player who does things on purpose: motions, assist combos, dashes, parries, throws, supers
const U = 1, D = 2, L = 4, R = 8, PATS = [
  f => [[D, 3], [D | f, 3], [f | 32, 2], [0, 6]], f => [[D, 3], [D | f, 3], [f | 64, 2], [0, 6]], f => [[D, 3], [D | (f ^ 12), 3], [(f ^ 12) | 16, 2], [0, 8]], f => [[f, 2], [D, 2], [D | f, 2], [f | 64, 2], [0, 10]],
  f => [[256 | 32, 2], [256, 8], [256 | 32, 2], [256, 9], [256 | 32, 2], [256, 10], [256 | 32, 2], [256, 12], [256 | 32, 2], [256, 12]], f => [[256 | 64, 2], [256, 8], [256 | 64, 2], [256, 9], [256 | 64, 2], [256, 10], [256 | 64, 2], [256, 12], [256 | 64, 2], [256, 12]],
  f => [[256 | 16, 2], [256, 7], [256 | 16, 2], [256, 8], [256 | 16, 2], [256, 9], [256 | 16, 2], [256, 9]], f => [[f, 2], [0, 2], [f, 2], [0, 8]], f => [[f ^ 12, 2], [0, 2], [f ^ 12, 2], [0, 10]], f => [[1024, 20]], f => [[1024, 4], [f, 1], [1024, 1], [f | 1024, 2], [0, 12], [32, 2], [0, 10]],
  f => [[48, 3], [0, 12], [16, 2], [0, 6], [16, 2], [0, 8], [[f, f ^ 12, U, D][rnd() % 4], 4], [0, 20]], f => [[512, 2], [0, 30]], f => [[2048, 2], [0, 20], [64, 2], [0, 8], [D | 128, 2], [0, 10]], f => [[96, 2], [0, 12], [[16, 32, 64, 128, D | 128, (f ^ 12) | 128][rnd() % 6], 2], [0, 20]],
  f => [[192, 3], [0, 40]], f => [[(f ^ 12) | 192, 3], [0, 60]], f => [[D | 192, 3], [0, 60]], f => [[U | f, 3], [0, 12], [[16, 32, 64, 128, U | 32, D | 64, D | 128, U | 128, 2048][rnd() % 9], 2], [0, 6], [[16, 64, 128, U, D | 128][rnd() % 5], 2], [0, 20]],
  f => [[U, 3], [0, 14], [64, 2], [0, 20]], f => [[128, 2], [0, 20]], f => [[f | 128, 2], [0, 30]], f => [[D | 128, 2], [0, 14], [[128, D | 128, (f ^ 12) | 128, f | 16][rnd() % 4], 2], [0, 20]], f => [[f, 10 + rnd() % 30]], f => [[f ^ 12, 6 + rnd() % 30]], f => [[D | (f ^ 12), 10 + rnd() % 30]],
  f => [[D | 32, 2], [D, 4], [f, 2], [0, 2], [f, 2], [0, 14], [64, 2], [0, 20]], f => [[16, 2], [0, 4], [16, 2], [0, 4], [D | 16, 2], [0, 12]], f => [[32, 2], [0, 8], [32, 2], [0, 12], [32, 2], [0, 20]], f => [[f ^ 12, 30], [(f ^ 12) | 512, 2], [0, 20]], f => [[256 | 128, 2], [0, 30]], f => [[256 | 2048, 2], [0, 40]]];
function mash(i, s) {
  if (!script[i].length) { const f = s.p[i].face > 0 ? R : L; for (const [m, n] of PATS[rnd() % PATS.length](f)) for (let k = 0; k < n; k++) script[i].push(m); if (rnd() % 4 === 0) for (let k = rnd() % 20; k > 0; k--) script[i].push(0); }
  return script[i].shift();
}
function random(i) { if (hold[i]-- <= 0) { const r = rnd(); inp[i] = ((r & 4095) & ((r >>> 12) & 4095)) | ((r >>> 24) & 15); if ((r >>> 28) & 1) inp[i] &= 15; hold[i] = (rnd() >>> 8) % 9; } return inp[i]; }
const training = mode === 'training';
let s = SIM.newMatch({ chars: [0, 1], training: training, rounds: 2 }), ai = [AI.make(3), AI.make(2)];
const lines = [];
for (let f = 0; f < frames; f++) {
  let i0, i1;
  if (mode === 'random') { i0 = random(0); i1 = random(1); }
  else if (mode === 'mash' || mode === 'force' || training) { i0 = mash(0, s); i1 = mash(1, s); }
  else if (mode === 'cpu') { i0 = AI.think(ai[0], s, 0); i1 = AI.think(ai[1], s, 1); }
  else { i0 = mash(0, s); i1 = AI.think(ai[1], s, 1); }
  const reset = s.phase === 'over' ? 1 : 0;
  if (reset) { s = SIM.newMatch({ chars: [0, 1], training: training, rounds: 2 }); ai = [AI.make(3), AI.make(2)]; }
  let fw = -1, fm = -1, fhp = 0, fsa = 0;
  if (mode === 'force' && s.phase === 'fight' && f % 53 === 0 && s.p[0].st !== 'thrown' && s.p[1].st !== 'thrown' && s.p[0].st !== 'throwing' && s.p[1].st !== 'throwing') { fw = rnd() % 2; fm = rnd() % MVK.length; if (rnd() % 3 === 0) s.p[fw].hp = Math.min(s.p[fw].hp, 200); if (rnd() % 2) s.p[fw].sa = 3000; fhp = s.p[fw].hp; fsa = s.p[fw].sa; SIM.debugStart(s, fw, MVK[fm]); }
  SIM.step(s, i0, i1);
  const d = dump(s); let h = 2166136261 >>> 0; for (const v of d) { h ^= (v >>> 0); h = Math.imul(h, 16777619) >>> 0; }
  lines.push(i0 + ' ' + i1 + ' ' + reset + ' ' + (h >>> 0).toString(16) + ' ' + fw + ' ' + fm + ' ' + fhp + ' ' + fsa);
  if (f + 1 === detail) { const names = []; const dd = dump(s, names); console.log(names.map((n, k) => n + '=' + dd[k]).join('\n')); }
}
fs.writeFileSync(out, (training ? 1 : 0) + ' ' + frames + '\n' + lines.join('\n') + '\n');
if (detail < 0) console.log(mode + ' ' + frames + ' frames written');
