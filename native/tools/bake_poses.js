// Writes Content/Gk/poses.txt and poses.bin: the browser game's own animation, sampled frame by frame.
// It runs the browser's pose code (the pose table, the per-move timelines and fighterPose from render.js) under Node
// for every move and every state, and records the pose targets it produces: hips, spine lean and twist, hands and feet.
// The game then solves those targets onto its skeleton. Run:  node tools/bake_poses.js <browser game dir> <out dir>
const fs = require('fs'), path = require('path');
const [dir, out] = process.argv.slice(2);
const SIM = require(path.resolve(dir, 'sim.js'));
const src = fs.readFileSync(path.join(dir, 'render.js'), 'utf8').split('\n');
const a = src.findIndex(l => l.includes('// ----------------------------------------------------------- skeleton + hand-keyed animation')), b = src.findIndex(l => l.includes('// ----------------------------------------------------------- rig'));
const body = 'const D = Math.PI / 180, rigs = [{}, {}], KM = null; let tick = 0;\n' + src.slice(a, b).join('\n') + '\nreturn { fighterPose, react, rigs };';
const R = new Function('SIM', body)(SIM);
const C = SIM.CH[0], MVK = Object.keys(C.mv);
const base = () => { const s = SIM.newMatch({ chars: [0, 1], training: true }); return s.p; };
const clips = [], data = [];
function sample(p, o, sFrame, reactType) {
  R.react[0] = reactType || ''; R.rigs[0] = {};
  const r = R.fighterPose(p, o, sFrame, 0), po = r.po, tg = { hL: po.hL.slice(), hR: po.hR.slice(), fL: po.fL.slice(), fR: po.fR.slice() };
  if (po.aim && r.hb && po.aw > 0) { const h = r.hb, t = tg[po.aim], tx = po.up ? h[0] + h[2] * 0.6 : h[0] + h[2] - 3, ty = po.up ? h[1] + h[3] - 5 : h[1] + h[3] * po.ay; t[0] += (tx - t[0]) * po.aw; t[1] += (ty - t[1]) * po.aw; }
  const v = [po.hip[0], po.hip[1], po.ln, po.ln2, po.tw, po.pt, po.hd, po.eL, po.eR, po.kL, po.kR, po.rot || 0, (po.yaw || 0) + (r.yaw || 0)].concat(tg.hL, tg.hR, tg.fL, tg.fR);
  for (const x of v) if (!isFinite(x)) throw new Error('bad pose value in ' + p.st + ' ' + p.mv);
  return v;
}
function clip(name, n, make) { clips.push([name, data.length / 25, n]); for (let k = 0; k < n; k++) { const q = make(k); const p = q.p, o = q.o || base()[1]; for (const x of sample(p, o, q.t || 0, q.react)) data.push(x); } }
const P = (set) => Object.assign(base()[0], set);
clip('idle', 262, k => ({ p: P({ st: 'idle', sub: 'stand' }), t: k }));
clip('crouch', 17, k => ({ p: P({ st: 'idle', sub: 'crouch', subf: Math.min(k, 4) }) }));
clip('walkf', 65, k => ({ p: P({ st: 'idle', sub: 'walkf', subf: k }) }));
clip('walkb', 71, k => ({ p: P({ st: 'idle', sub: 'walkb', subf: k }) }));
clip('prejump', 5, k => ({ p: P({ st: 'prejump', sf: k }) }));
clip('land', 7, k => ({ p: P({ st: 'land', sf: k }) }));
clip('jump', 21, k => ({ p: P({ st: 'jump', y: 100, jd: 0, vy: 1125 - (k / 20) * 2250 }) }));
clip('dashf', 20, k => ({ p: P({ st: 'dashf', sf: k }) }));
clip('dashb', 24, k => ({ p: P({ st: 'dashb', sf: k }) }));
clip('run', 46, k => ({ p: P({ st: 'run', sf: k }) }));
clip('drush', 47, k => ({ p: P({ st: 'drush', sf: k }) }));
clip('parry', 5, k => ({ p: P({ st: 'parry', sf: k }) }));
clip('prec', 13, k => ({ p: P({ st: 'prec', sf: k }) }));
clip('blockS', 1, k => ({ p: P({ st: 'block', cb: 0 }) }));
clip('blockC', 1, k => ({ p: P({ st: 'block', cb: 1 }) }));
for (const [nm, rt, cb] of [['hitL', 'L', 0], ['hitM', 'M', 0], ['hitH', 'H', 0], ['hitLow', 'LOW', 0], ['hitCr', 'CR', 1]]) clip(nm, 21, k => ({ p: P({ st: 'hit', sf: k, stun: 20 - k, cb: cb, crum: 0 }), react: rt }));
clip('crumple', 85, k => ({ p: P({ st: 'hit', sf: k, stun: 84 - k, crum: 1 }) }));
clip('splat', 1, k => ({ p: P({ st: 'hit', sf: 5, stun: 60, crum: 2 }) }));
clip('ahit', 1, k => ({ p: P({ st: 'ahit', y: 100, vy: 700, sf: 0 }) }));
clip('kd', 1, k => ({ p: P({ st: 'kd' }) }));
clip('getup', SIM.GETUP + 1, k => ({ p: P({ st: 'getup', sf: k }) }));
clip('tech', 25, k => ({ p: P({ st: 'tech', sf: k }) }));
clip('win', 41, k => ({ p: P({ st: 'win', sf: k }), t: 0 }));
clip('grab', 11, k => ({ p: P({ st: 'throwing', tk: 0, sf: k, pmT: 0 }) }));
clip('bite', 10, k => ({ p: P({ st: 'throwing', tk: 0, sf: 20, pmT: 9 - k }) }));
for (let tk = 1; tk <= 4; tk++) clip('throw' + tk, 31, k => ({ p: P({ st: 'throwing', tk: tk, tf: k, sf: 30 }) }));
clip('thrown', 1, k => ({ p: P({ st: 'thrown' }), o: Object.assign(base()[1], { st: 'throwing', tk: 0, tf: 0, pmT: 0 }), t: 0 }));
const lens = [];
MVK.forEach((key, id) => {
  const m = C.mv[key], n = Math.min(200, (m.t < 900 ? m.t : m.lastAct + 40) + 1); lens.push(n);
  clip('mv_' + key, n, k => ({ p: P({ st: 'atk', mv: key, mf: k, y: m.isAir || (m.rise && k >= m.rise[0]) ? 100 : 0, vx: m.home ? m.home[2] : 0, vy: 62, contact: 0, fr: 0, hmE: 0 }) }));
});
fs.mkdirSync(out, { recursive: true });
fs.writeFileSync(path.join(out, 'poses.txt'), '# GEKITOTSU poses, sampled from the browser game by tools/bake_poses.js. name, first frame, frames. 25 numbers a frame in poses.bin.\n' + clips.map(c => c.join(' ')).join('\n') + '\n');
fs.writeFileSync(path.join(out, 'poses.bin'), Buffer.from(new Float32Array(data).buffer));
console.log(clips.length + ' clips, ' + data.length / 25 + ' frames, ' + (data.length * 4 >> 10) + ' KB');
