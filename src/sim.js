// ===================================================================
// GEKITOTSU deterministic simulation core. Integer math only, no RNG,
// no wall-clock, no floats. state = step(state, inputP1, inputP2).
// Modern-style controls: Light / Medium / Heavy / Special + Assist,
// with a Drive gauge (Impact, Parry, Rush, Reversal, Overdrive, Burnout)
// and a three-level Super Art gauge.
// ===================================================================
const SIM = (function () {
  'use strict';
  const F = 100;
  const BT = { U: 1, D: 2, L: 4, R: 8, LT: 16, MD: 32, HV: 64, SP: 128, AS: 256, DI: 512, PA: 1024 };
  const MASK = 2047, NB = 7;
  const STAGE = 560 * F, SEP = 400 * F, PW = 23 * F;
  const GRAV = 62, BUF = 5, NBUF = 40, DMAX = 6000, BAR = 1000;
  const INTRO = 120, KOTIME = 290, KD = 26, GETUP = 22;
  const SCALE = [100, 100, 80, 70, 60, 50, 40, 30, 20, 10];
  const C_CH = 1, C_SP = 2, C_SU = 4;
  const tr = Math.trunc;

  function mk(o) {
    const m = Object.assign({ su: 5, ac: 3, rc: 8, d: 30, ha: 2, ba: 0, g: 'm', hsp: 8, kb: 500, lvl: 0, can: 0 }, o);
    if (!m.t) m.t = m.su - 1 + m.ac + m.rc;
    if (!m.hits) m.hits = [{ s: m.su - 1, e: m.su - 2 + m.ac, hb: m.hb, d: m.d, stun: m.fs || (m.ac + m.rc + m.ha), bstun: m.fb || (m.ac + m.rc + m.ba), launch: m.launch, air: Array.isArray(m.air) ? m.air : undefined, wb: m.wb, gb: m.gb }];
    m.isAir = m.air === 1; m.lastAct = 0;
    m.hits.forEach((h, i) => {
      if (h.e > m.lastAct) m.lastAct = h.e;
      if (h.g == null) h.g = m.g; if (h.hsp == null) h.hsp = m.hsp; if (h.lvl == null) h.lvl = m.lvl;
      if (h.kb == null) h.kb = m.kb; if (!h.stun) h.stun = 20; if (!h.bstun) h.bstun = 14;
      if (h.grp == null) h.grp = i;
    });
    return m;
  }

  function normals() {
    return {
      '5L': mk({ n: 'Jab', su: 4, ac: 3, rc: 7, d: 30, ha: 4, ba: -1, hb: [8, 60, 42, 14], kb: 300, hsp: 7, can: C_CH | C_SP | C_SU, chain: ['5L', '2L'], an: { w: 'jabW', s: 'jabS' } }),
      '5M': mk({ n: 'Body Blow', su: 5, ac: 4, rc: 14, d: 60, ha: 5, ba: -1, hb: [8, 50, 50, 20], kb: 480, hsp: 9, can: C_CH | C_SP | C_SU, chain: ['5H'], an: { w: 'mpW', s: 'mpS' } }),
      '5H': mk({ n: 'Straight', su: 9, ac: 4, rc: 18, d: 80, ha: 3, ba: -2, hb: [6, 56, 56, 18], kb: 760, hsp: 11, lvl: 1, can: C_SP | C_SU, an: { w: 'strW', s: 'strS' } }),
      '2L': mk({ n: 'Low Kick', su: 5, ac: 3, rc: 9, d: 25, g: 'l', ha: 3, ba: -2, hb: [6, 0, 46, 12], kb: 300, hsp: 7, crouch: 1, can: C_CH, chain: ['2L', '5L'], an: { w: 'clkW', s: 'clkS' } }),
      '2M': mk({ n: 'Low Forward', su: 7, ac: 3, rc: 19, d: 50, g: 'l', ha: -2, ba: -6, hb: [6, 0, 68, 13], kb: 460, hsp: 9, crouch: 1, can: C_SP | C_SU, an: { w: 'clkW', s: 'cmkS' } }),
      '2H': mk({ n: 'Rising Upper', su: 8, ac: 4, rc: 22, d: 80, ha: 3, ba: -10, hb: [2, 46, 36, 66], kb: 560, air: [140, 950], hsp: 11, lvl: 1, crouch: 1, can: C_SP | C_SU, an: { w: 'cupW', s: 'cupS' } }),
      '6M': mk({ n: 'Step Kick', su: 8, ac: 3, rc: 20, d: 60, ha: 3, ba: -5, hb: [10, 28, 64, 16], kb: 600, hsp: 9, mv: [[2, 8, 350]], an: { w: 'lkW', s: 'lkS' } }),
      '4H': mk({ n: 'Roundhouse', su: 11, ac: 3, rc: 22, d: 90, ha: 1, ba: -5, hb: [8, 58, 68, 22], kb: 1000, hsp: 12, lvl: 1, an: { w: 'hkW', s: 'hkS' } }),
      '6H': mk({ n: 'Axe Kick', su: 20, ac: 3, rc: 20, d: 100, g: 'h', ha: 3, ba: -3, hb: [8, 30, 50, 60], kb: 600, hsp: 11, lvl: 1, an: { w: 'axW', s: 'axS' } }),
      '3H': mk({ n: 'Sweep', su: 8, ac: 3, rc: 24, d: 90, g: 'l', ba: -10, hb: [6, 0, 66, 14], launch: [320, 480], hardKd: 1, hsp: 11, lvl: 1, crouch: 1, an: { w: 'swW', s: 'swS' } }),
      'jL': mk({ n: 'Air Knee', air: 1, su: 6, ac: 10, t: 999, g: 'h', d: 30, fs: 15, fb: 11, hb: [-18, 4, 48, 20], kb: 400, hsp: 7, landRec: 3, an: { w: 'jump', s: 'jlkS' } }),
      'jM': mk({ n: 'Air Cross Kick', air: 1, su: 7, ac: 8, t: 999, g: 'h', d: 50, fs: 18, fb: 14, hb: [-22, 2, 58, 20], kb: 450, hsp: 9, landRec: 3, an: { w: 'jump', s: 'jlkS' } }),
      'jH': mk({ n: 'Air Axe Kick', air: 1, su: 9, ac: 7, t: 999, g: 'h', d: 80, fs: 20, fb: 16, hb: [4, -4, 56, 26], kb: 500, hsp: 11, lvl: 1, landRec: 3, an: { w: 'jhkW', s: 'jhkS' } }),
      'throw': mk({ n: 'Throw', su: 5, ac: 3, rc: 23, thr: 1, hits: [], an: { w: 'grabW', s: 'grabS' } }),
      'DI': mk({ n: 'Drive Impact', su: 26, ac: 2, rc: 35, d: 80, arm: 2, di: 1, ba: -3, hb: [4, 30, 62, 44], kb: 1500, launch: [650, 540], air: [650, 540], wb: 1, lvl: 2, hsp: 16, mv: [[16, 26, 520]], an: { k: [[0, 'idle'], [14, 'diW'], [24, 'diW'], [26, 'diS'], [40, 'diS'], [62, 'idle']] } }),
      'DREV': mk({ n: 'Drive Reversal', su: 18, ac: 3, rc: 26, d: 50, inv: [0, 21], ba: -8, hb: [4, 30, 60, 44], launch: [760, 430], air: [760, 430], lvl: 1, hsp: 10, mv: [[10, 18, 400]], an: { w: 'strW', s: 'strS' } })
    };
  }
  const std = (pre) => ({ L: pre + 'L', M: pre + 'M', H: pre + 'H', OD: pre + 'OD' });

  function kaito() {
    const mv = normals();
    const fb = (k, su, vx, st, bs, od) => { mv[k] = mk({ n: 'Hadan', sp: 1, su: su, t: su + (od ? 22 : 33), hits: [], fc: [su, su + 12], can: C_SU, od: od,
      proj: { f: su - 1, vx: vx, y: 50, w: 26, h: 22, hits: od ? 2 : 1, d: od ? 40 : 60, stun: st, bstun: bs, kb: 560, hsp: 9, lvl: 1 },
      an: { k: [[0, 'idle'], [su - 6, 'fbW'], [su - 1, 'fbS'], [su + 14, 'fbS'], [su + (od ? 22 : 33), 'idle']] } }); };
    fb('236L', 16, 420, 32, 26); fb('236M', 14, 600, 30, 24); fb('236H', 12, 780, 28, 22); fb('236OD', 12, 820, 26, 19, 1);
    const hbA = [2, 30, 56, 62], hbB = [0, 20, 56, 90];
    const sr = (k, su, hits, vy, rec, o) => { mv[k] = mk(Object.assign({ n: 'Shoryu', sp: 1, su: su, t: 999, mv: [[0, su - 1, 500]], rise: [su + 1, 150, vy], landRec: rec, can: C_SU, hits: hits, ainv: [0, 12],
      an: { k: [[0, 'cupW'], [su - 1, 'srS'], [su + 12, 'srS2'], [su + 32, 'jump']], spin: [su, su + 26, 360] } }, o || {})); };
    sr('623L', 5, [{ s: 4, e: 13, hb: hbA, d: 110, launch: [200, 1100], air: [200, 1050], lvl: 2, hsp: 13, bstun: 18 }], 950, 14);
    sr('623M', 6, [{ s: 5, e: 6, hb: hbA, d: 70, stun: 24, kb: 0, air: [50, 620], lvl: 1, hsp: 9, bstun: 18 }, { s: 9, e: 16, hb: hbB, d: 60, launch: [200, 1100], air: [200, 1100], lvl: 2, hsp: 12, bstun: 18 }], 1050, 18);
    sr('623H', 7, [{ s: 6, e: 7, hb: hbA, d: 80, stun: 24, kb: 0, air: [40, 600], lvl: 1, hsp: 9, bstun: 18 }, { s: 10, e: 12, hb: hbB, d: 30, stun: 24, kb: 0, air: [50, 640], lvl: 1, hsp: 7, bstun: 18 }, { s: 14, e: 20, hb: hbB, d: 30, launch: [220, 1150], air: [220, 1150], lvl: 2, hsp: 13, bstun: 18 }], 1150, 22);
    sr('623OD', 6, [{ s: 5, e: 6, hb: hbA, d: 40, stun: 24, kb: 0, air: [40, 560], lvl: 1, hsp: 7, bstun: 18 }, { s: 9, e: 10, hb: hbB, d: 30, stun: 24, kb: 0, air: [40, 600], lvl: 1, hsp: 6, bstun: 18 }, { s: 12, e: 13, hb: hbB, d: 30, stun: 24, kb: 0, air: [40, 600], lvl: 1, hsp: 6, bstun: 18 }, { s: 16, e: 22, hb: hbB, d: 80, launch: [220, 1200], air: [220, 1200], lvl: 2, hsp: 14, bstun: 18 }], 1200, 24, { inv: [0, 11], od: 1, ainv: null });
    const sb = [-4, 40, 56, 24], sh = (s, d, last, wb) => last ? { s: s, e: s + 2, hb: sb, d: d, launch: last, air: last, wb: wb, lvl: 2, hsp: 12, bstun: 17 } : { s: s, e: s + 1, hb: sb, d: d, stun: 17, kb: 110, air: [260, 420], lvl: 1, hsp: 7, bstun: 16 };
    const ts = (k, su, t, run, spd, hits, deg, o) => { mv[k] = mk(Object.assign({ n: 'Senpu', sp: 1, su: su, t: t, mv: [[su - 7, run, spd]], can: C_SU, hits: hits, an: { k: [[0, 'idle'], [su - 6, 'spW'], [su - 1, 'spS'], [run + 2, 'spS'], [t, 'idle']], spin: [su - 4, run + 4, deg] } }, o || {})); };
    ts('214L', 12, 40, 22, 480, [sh(11, 30), sh(15, 40, [450, 660])], 720);
    ts('214M', 14, 50, 30, 480, [sh(13, 45), sh(20, 45, [600, 700])], 720);
    ts('214H', 16, 58, 38, 500, [sh(15, 30), sh(21, 60), sh(27, 40, [780, 760], 1)], 1080);
    ts('214OD', 9, 60, 36, 600, [sh(8, 30), sh(13, 20), sh(18, 20), sh(23, 20), sh(28, 80, [1000, 820], 1)], 1800, { od: 1 });
    const dl = (k, su, d, ha, ba, od) => { mv[k] = mk({ n: 'Ryubi Kick', sp: 1, su: su, ac: 5, rc: 21, d: d, ha: ha, ba: ba, hb: [4, 16, 54, 66], kb: 500, lvl: 1, hsp: 11, mv: [[4, su - 1, 540]], lowInv: [5, su + 3], od: od, can: C_SU,
      an: { k: [[0, 'idle'], [5, 'jump'], [su - 4, 'jhkW'], [su - 1, 'axS'], [su + 8, 'axS'], [su + 25, 'idle']] } }); };
    dl('2SP', 20, 95, 2, -4); dl('2SPOD', 15, 100, 4, -2, 1);
    mv.SA1 = mk({ n: 'Enryu Kick', sup: 1, sa: 1, su: 7, t: 62, frz: 40, inv: [0, 8], minScale: 30, mv: [[6, 22, 900]],
      hits: [{ s: 6, e: 8, hb: sb, d: 60, stun: 20, kb: 60, air: [320, 360], lvl: 1, hsp: 8, bstun: 16 }, { s: 12, e: 14, hb: sb, d: 60, stun: 20, kb: 60, air: [320, 360], lvl: 1, hsp: 8, bstun: 16 }, { s: 18, e: 21, hb: sb, d: 80, launch: [720, 800], air: [720, 800], wb: 1, lvl: 2, hsp: 14, bstun: 14 }],
      an: { k: [[0, 'spW'], [6, 'spS'], [26, 'spS'], [62, 'idle']], spin: [4, 26, 1080] } });
    mv.SA2 = mk({ n: 'Shinku Reppa', sup: 1, sa: 2, su: 10, t: 66, frz: 44, inv: [0, 12], minScale: 40, hits: [],
      proj: { f: 10, vx: 950, y: 50, w: 72, h: 36, hits: 5, d: 56, stun: 24, bstun: 16, kb: 250, hsp: 7, lvl: 2, launchLast: [1000, 820], wbLast: 1, big: 1, air: [250, 320] },
      an: { k: [[0, 'fbW'], [9, 'fbW'], [11, 'fbS'], [50, 'fbS'], [66, 'idle']] } });
    const big = [-4, 0, 80, 100], fl = s => ({ s: s, e: s + 1, hb: big, d: 60, stun: 34, kb: 0, air: [0, 260], lvl: 1, hsp: 7, bstun: 20, req: s > 6 ? 1 : 0 });
    mv.SA3 = mk({ n: 'Shinryu Gekitotsu', sup: 1, sa: 3, su: 7, t: 999, frz: 60, inv: [0, 14], minScale: 50, wEnd: [12, -44], rise: [41, 100, 1250], landRec: 30,
      hits: [Object.assign(fl(6), { hb: [0, 20, 60, 70], e: 9 }), fl(14), fl(22), fl(30), { s: 40, e: 46, hb: big, d: 160, launch: [150, 1450], air: [150, 1450], lvl: 2, hsp: 22, req: 1 }],
      an: { k: [[0, 'cupW'], [6, 'strS'], [11, 'jabW'], [14, 'mpS'], [19, 'strW'], [22, 'strS'], [27, 'cupW'], [30, 'cupS'], [36, 'cupW'], [40, 'srS'], [56, 'srS2'], [80, 'jump']], spin: [40, 70, 720] } });
    return {
      name: 'KAITO', jp: '海斗', title: 'THE IRON WAVE', hp: 1000, walkF: 300, walkB: 230, dash: 640, jx: 330, mv: mv,
      sp: [Object.assign({ m: '623', dir: 6 }, std('623')), Object.assign({ m: '214', dir: 4 }, std('214')), Object.assign({ m: '236', dir: 5 }, std('236')), { m: '', dir: 2, L: '2SP', M: '2SP', H: '2SP', OD: '2SPOD' }],
      auto: { L: [['2L'], ['5L'], ['623L']], M: [['5M'], ['5H'], ['SA2', '214M']], H: [['5H'], ['623H'], ['SA3']] },
      cmd: [['Hadan', 'SP  /  236 + attack', 'Fireball. Strength sets the speed.'], ['Shoryu', '→ + SP  /  623 + attack', 'Rising upper. Beats jump-ins. Only the Overdrive is fully invincible.'], ['Senpu', '← + SP  /  214 + attack', 'Advancing spin kicks. Heavy and Overdrive wall-bounce.'], ['Ryubi Kick', '↓ + SP', 'Hopping axe kick. Goes over lows, plus on hit.'],
        ['SA1 Enryu Kick', 'H + SP  /  236236 + L', 'Level 1 Super Art.'], ['SA2 Shinku Reppa', '← + H + SP  /  236236 + M', 'Level 2 Super Art. Full-screen beam.'], ['SA3 Shinryu Gekitotsu', '↓ + H + SP  /  236236 + H', 'Level 3 Super Art.']]
    };
  }

  function ren() {
    const mv = normals();
    mv['5H'].d = 85; mv['5H'].hits[0].d = 85;
    const el = (k, su, vx, d, lx, ly, bs, t, od) => { mv[k] = mk({ n: 'Raijin Elbow', sp: 1, su: su, t: t, mv: [[3, su + 1, vx]], stopOnHit: 1, can: C_SU, od: od,
      hits: [{ s: su - 1, e: su + 2, hb: [4, 44, 44, 26], d: d, launch: [lx, ly], air: [lx, ly], wb: 1, lvl: 2, hsp: 12, bstun: bs }],
      an: { k: [[0, 'idle'], [3, 'elW'], [su - 1, 'elS'], [su + 6, 'elS'], [t, 'idle']] } }); };
    el('236L', 11, 880, 80, 850, 480, 23, 38); el('236M', 13, 960, 90, 920, 500, 22, 41); el('236H', 15, 1050, 100, 1000, 520, 21, 44); el('236OD', 9, 1300, 110, 1350, 640, 26, 38, 1);
    const kn = (k, su, vy, d, rec, o) => { mv[k] = mk(Object.assign({ n: 'Tensho Knee', sp: 1, su: su, t: 999, mv: [[0, su - 1, 420]], rise: [su + 1, 300, vy], landRec: rec, ainv: [0, 12], can: C_SU,
      hits: [{ s: su - 1, e: su + 7, hb: [2, 28, 44, 56], d: d, launch: [220, 1150], air: [220, 1000], lvl: 1, hsp: 11, bstun: 16 }],
      an: { k: [[0, 'cupW'], [su - 1, 'knS'], [su + 14, 'knS'], [su + 26, 'jump']] } }, o || {})); };
    kn('623L', 5, 720, 80, 9); kn('623M', 5, 840, 95, 12); kn('623H', 6, 950, 110, 16);
    kn('623OD', 5, 760, 0, 9, { inv: [0, 10], ainv: null, od: 1, hits: [{ s: 4, e: 5, hb: [2, 28, 44, 56], d: 60, launch: [100, 760], air: [100, 760], lvl: 1, hsp: 8, bstun: 16 }, { s: 9, e: 14, hb: [2, 28, 44, 62], d: 70, launch: [200, 1250], air: [200, 1200], lvl: 2, hsp: 12, bstun: 16 }] });
    const hm = (k, su, d, ba, od) => { mv[k] = mk({ n: 'Jiraishin', sp: 1, su: su, ac: 3, rc: 20, g: 'h', d: d, hb: [6, 16, 46, 74], launch: [80, 960], air: [150, -1500], gb: 1, ba: ba, lvl: 2, hsp: 13, can: C_SU, od: od,
      an: { k: [[0, 'idle'], [su - 6, 'hmW'], [su - 1, 'hmS'], [su + 8, 'hmS'], [su + 22, 'idle']] } }); };
    hm('214L', 18, 90, -6); hm('214M', 20, 100, -5); hm('214H', 22, 110, -4); hm('214OD', 14, 110, 0, 1);
    const dk = (k, vx, vy, od) => { mv[k] = mk({ n: 'Kudaki Dive', sp: 1, air: 1, su: 6, t: 999, hover: 5, rise: [5, vx, vy], landRec: 8, minY: 42, od: od,
      hits: [{ s: 5, e: 70, hb: [0, -8, 38, 32], d: od ? 90 : 65, stun: 18, bstun: 13, kb: 500, lvl: 1, hsp: 9, air: [400, 520], launch: od ? [150, 900] : undefined }],
      an: { k: [[0, 'jump'], [5, 'dkS'], [80, 'dkS']] } }); };
    dk('j236L', 820, -720); dk('j236M', 650, -880); dk('j236H', 480, -1020); dk('j236OD', 900, -1000, 1);
    mv['2SP'] = mk({ n: 'Suberi', sp: 1, su: 12, ac: 8, rc: 18, d: 80, g: 'l', ba: -12, hb: [4, 0, 60, 16], launch: [300, 520], lvl: 1, hsp: 11, crouch: 1, mv: [[6, 20, 760]], can: C_SU, an: { k: [[0, 'crouch'], [8, 'swS'], [22, 'swS'], [37, 'crouch']] } });
    mv['2SPOD'] = mk({ n: 'Suberi', sp: 1, od: 1, su: 9, ac: 8, rc: 16, d: 100, g: 'l', ba: -6, hb: [4, 0, 64, 16], launch: [150, 980], lvl: 1, hsp: 12, crouch: 1, mv: [[4, 17, 900]], can: C_SU, an: { k: [[0, 'crouch'], [6, 'swS'], [18, 'swS'], [32, 'crouch']] } });
    mv.SA1 = mk({ n: 'Raigeki', sup: 1, sa: 1, su: 8, t: 54, frz: 40, inv: [0, 9], minScale: 30, mv: [[6, 16, 1400]], stopOnHit: 1,
      hits: [{ s: 7, e: 16, hb: [0, 30, 56, 50], d: 200, launch: [1300, 720], air: [1300, 720], wb: 1, lvl: 2, hsp: 16, bstun: 20 }], an: { k: [[0, 'elW'], [7, 'elS'], [26, 'elS'], [54, 'idle']] } });
    const big = [-4, 0, 84, 104], sh = (s, d) => ({ s: s, e: s + 1, hb: big, d: d, stun: 34, kb: 0, air: [0, 250], lvl: 1, hsp: 6, bstun: 20, req: 1 });
    const rush = (n, lv, hd, fin, fz, ms) => mk({ n: n, sup: 1, sa: lv, su: 9, t: 96, frz: fz, inv: [0, 10], minScale: ms, mv: [[6, 17, 1400]], stopOnHit: 1, wEnd: [19, 64],
      hits: [{ s: 8, e: 17, hb: [0, 30, 52, 50], d: 30, stun: 34, kb: 0, air: [0, 300], lvl: 1, hsp: 10, bstun: 22 }, sh(24, hd), sh(31, hd), sh(38, hd), sh(45, hd), sh(52, hd), { s: 66, e: 68, hb: big, d: fin, launch: [1350, 900], air: [1350, 900], wb: 1, lvl: 2, hsp: 18, req: 1 }],
      an: { k: [[0, 'elW'], [7, 'elS'], [20, 'elS'], [24, 'jabS'], [27, 'strW'], [31, 'strS'], [35, 'lkW'], [38, 'lkS'], [42, 'jabW'], [45, 'jabS'], [49, 'strW'], [52, 'strS'], [58, 'hkW'], [66, 'hkS'], [78, 'hkS'], [96, 'idle']] } });
    mv.SA2 = rush('Gouraijin', 2, 30, 100, 44, 40); mv.SA3 = rush('Shin Gouraijin', 3, 44, 150, 60, 50);
    return {
      name: 'REN', jp: '蓮', title: 'THUNDER BREAKER', hp: 950, walkF: 345, walkB: 255, dash: 720, jx: 350, mv: mv,
      sp: [Object.assign({ m: '623', dir: 6 }, std('623')), Object.assign({ m: '214', dir: 4 }, std('214')), Object.assign({ m: '236', dir: 5 }, std('236')), { m: '', dir: 2, L: '2SP', M: '2SP', H: '2SP', OD: '2SPOD' }, Object.assign({ m: '236', air: 1, dir: 0 }, std('j236'))],
      auto: { L: [['2L'], ['5L'], ['623L']], M: [['5M'], ['5H'], ['SA2', '236M']], H: [['5H'], ['236H'], ['SA3']] },
      cmd: [['Raijin Elbow', 'SP  /  236 + attack', 'Dashing elbow. Wall-bounces near the corner.'], ['Tensho Knee', '→ + SP  /  623 + attack', 'Launcher. Light recovers fast enough to juggle after.'], ['Jiraishin', '← + SP  /  214 + attack', 'Overhead hammer. Pops grounded foes up, ground-bounces airborne ones.'], ['Suberi', '↓ + SP', 'Low slide. Overdrive launches.'], ['Kudaki Dive', 'air SP  /  air 236 + attack', 'Dive kick. Light is shallow, heavy is steep.'],
        ['SA1 Raigeki', 'H + SP  /  236236 + L', 'Level 1 Super Art.'], ['SA2 Gouraijin', '← + H + SP  /  236236 + M', 'Level 2 Super Art.'], ['SA3 Shin Gouraijin', '↓ + H + SP  /  236236 + H', 'Level 3 Super Art.']]
    };
  }
  const CH = [kaito(), ren()];

  // ------------------------------------------------------------ state
  function newFighter(ch, i) {
    const b = []; for (let k = 0; k < NBUF; k++) b.push(0);
    return { ch: ch, x: (i ? 120 : -120) * F, y: 0, vx: 0, vy: 0, pv: 0, face: i ? -1 : 1, hp: CH[ch].hp, sa: 0, dr: DMAX, bo: 0, drT: 0, st: 'idle', sub: 'stand', sf: 0, subf: 0,
      mv: '', mf: 0, hitMask: 0, contact: 0, hitOK: 0, hs: 0, stun: 0, buf: b, prev: 0, age: [99, 99, 99, 99, 99, 99, 99], d66: 99, d44: 99, combo: 0, comboDmg: 0, jug: 0,
      wbUsed: 0, gbUsed: 0, wbPend: 0, gbPend: 0, jd: 0, rec: 0, cb: 0, hitHi: 1, tinv: 0, wins: 0, back: 0, red: CH[ch].hp, redT: 0, air2: 0, noTech: 0,
      roll: 0, hard: 0, arm: 0, armHit: 0, drb: 0, mod: 0, ac: '', aci: 0, crum: 0, pp: 0, plock: 0, half: 0 };
  }
  function newMatch(cfg) {
    const s = { frame: 0, slow: 0, phase: 'intro', pt: 0, round: 1, time: 99 * 60, freeze: 0, frzWho: 0, proj: [], ev: [], winner: -1, rw: -1, koType: '', cfg: { training: !!cfg.training, chars: cfg.chars.slice(), rounds: cfg.rounds || 2 }, p: [] };
    s.p = [newFighter(cfg.chars[0], 0), newFighter(cfg.chars[1], 1)];
    if (s.cfg.training) { s.phase = 'fight'; s.p[0].sa = 3000; s.p[1].sa = 3000; }
    return s;
  }
  function resetRound(s) {
    const keep = s.p.map(p => [p.wins, p.sa]);
    s.p = [newFighter(s.cfg.chars[0], 0), newFighter(s.cfg.chars[1], 1)];
    s.p.forEach((p, i) => { p.wins = keep[i][0]; p.sa = keep[i][1]; });
    s.proj = []; s.time = 99 * 60; s.phase = 'intro'; s.pt = 0; s.freeze = 0; s.slow = 0; s.koType = '';
  }
  function clone(o) {
    if (o === null || typeof o !== 'object') return o;
    if (Array.isArray(o)) { const a = new Array(o.length); for (let i = 0; i < o.length; i++) a[i] = clone(o[i]); return a; }
    const r = {}; for (const k in o) r[k] = clone(o[k]); return r;
  }
  function hash(s) {
    let h = 2166136261 >>> 0; const mix = v => { h ^= (v | 0); h = Math.imul(h, 16777619) >>> 0; };
    mix(s.frame); mix(s.slow); mix(s.time); mix(s.proj.length);
    for (const p of s.p) { mix(p.x); mix(p.y); mix(p.vx); mix(p.vy); mix(p.hp); mix(p.sa); mix(p.dr); mix(p.sf); mix(p.mf); mix(p.hs); mix(p.stun); mix(p.st.length); }
    for (const q of s.proj) { mix(q.x); mix(q.left); }
    return h;
  }

  // ------------------------------------------------------------ input helpers
  function hdir(inp, face) { return (((inp & 8) ? 1 : 0) - ((inp & 4) ? 1 : 0)) * face; }
  function vdir(inp) { return ((inp & 1) ? 1 : 0) - ((inp & 2) ? 1 : 0); }
  function npad(inp, face) { return 5 + hdir(inp, face) + 3 * vdir(inp); }
  function motion(p, seq, win) {
    let k = seq.length - 1; const b = p.buf, n = b.length;
    for (let i = n - 1; i >= n - win && i >= 0; i--) { if (npad(b[i], p.face) === seq[k]) { k--; if (k < 0) return true; } }
    return false;
  }
  function motionOK(p, m) {
    if (m === '236') return motion(p, [2, 3, 6], 15);
    if (m === '214') return motion(p, [2, 1, 4], 15);
    if (m === '623') {
      if (!motion(p, [6, 2, 3], 20)) return false;
      for (let i = NBUF - 1; i >= NBUF - 12; i--) { const d = npad(p.buf[i], p.face); if (d === 3 || d === 2 || d === 1) return true; if (d === 6) return false; }
      return false;
    }
    if (m === 'SUP') return motion(p, [2, 6, 2, 6], 34);
    return false;
  }
  function dashIn(p, dir) {
    const b = p.buf, n = b.length;
    if (hdir(b[n - 1], p.face) !== dir || vdir(b[n - 1]) !== 0) return false;
    if (hdir(b[n - 2], p.face) !== 0) return false;
    for (let i = n - 3; i >= n - 11; i--) { const h = hdir(b[i], p.face); if (h === dir) return true; if (h !== 0) return false; }
    return false;
  }
  const parryHeld = inp => (inp & 1024) || ((inp & 32) && (inp & 64));

  // ------------------------------------------------------------ drive gauge
  function burn(s, p, i) { if (p.dr <= 0 && !p.bo) { p.dr = 0; p.bo = 1; s.ev.push({ t: 'burnout', who: i }); } }
  function spend(s, p, i, cost) { if (p.bo || p.dr <= 0) return false; p.dr -= cost; p.drT = 70; if (p.dr <= 0) { p.dr = 0; burn(s, p, i); } return true; }

  // ------------------------------------------------------------ fighter logic
  function cur(p) { return p.st === 'atk' ? CH[p.ch].mv[p.mv] : null; }
  function toIdle(p) { p.st = 'idle'; p.sf = 0; p.mv = ''; p.vx = 0; p.sub = 'stand'; p.air2 = 0; p.arm = 0; }
  function start(s, p, key, i, opt) {
    const m = CH[p.ch].mv[key], fromDr = p.st === 'drush';
    p.st = 'atk'; p.mv = key; p.mf = 0; p.sf = 0; p.hitMask = 0; p.contact = 0; p.hitOK = 0; p.arm = m.arm || 0; p.armHit = 0;
    p.drb = fromDr && !m.sp && !m.sup && !m.thr ? 1 : 0; if (fromDr) p.pv = p.face * 560;
    p.mod = opt && opt.mod ? 1 : 0; if (!(opt && opt.auto)) { p.ac = ''; p.aci = 0; }
    if (!m.isAir) p.vx = 0;
    if (m.sa) p.sa -= m.sa * 1000; else if (m.sp) p.sa = Math.min(3000, p.sa + 10);
    if (m.frz) { s.freeze = m.frz; s.frzWho = i; s.ev.push({ t: 'super', who: i, lv: m.sa }); }
    else if (m.od) s.ev.push({ t: 'ex', who: i });
    else if (m.di) s.ev.push({ t: 'di', who: i });
    if (m.sp || m.sup) s.ev.push({ t: 'special', who: i, k: key });
    p.age = [99, 99, 99, 99, 99, 99, 99];
    return true;
  }
  function tryAttack(s, p, o, inp, i) {
    const C = CH[p.ch], A = p.age, air = p.y > 0;
    const lt = A[0] <= BUF, md = A[1] <= BUF, hv = A[2] <= BUF, spb = A[3] <= BUF, dib = A[5] <= BUF;
    if (!(lt || md || hv || spb || dib)) return false;
    const m = cur(p); let canSp = true, canSu = true, chain = null, kara = false, canN = true, ok = false;
    if (m) {
      ok = p.contact && p.mf <= m.lastAct + 10;
      kara = !m.sp && !m.sup && !m.thr && !m.di && p.mf <= 2 && !p.contact;
      canSp = (ok && (m.can & C_SP)) || kara;
      canSu = (ok && (m.can & C_SU)) || kara || (m.fc && p.mf >= m.fc[0] && p.mf <= m.fc[1] && (m.can & C_SU));
      chain = (ok && (m.can & C_CH)) ? m.chain : null; canN = false;
      if (m.sup) return false;
    }
    const h = hdir(inp, p.face), v = vdir(inp), as = (inp & 256) !== 0;
    const myProj = s.proj.some(q => q.o === i);
    // assist combo continuation: same button again while the last step is touching
    if (m && ok && p.ac && as && ((p.ac === 'L' && lt) || (p.ac === 'M' && md) || (p.ac === 'H' && hv))) {
      const step = C.auto[p.ac][p.aci + 1];
      if (step) for (const k of step) { const mm = C.mv[k]; if (mm.sa && (p.sa < mm.sa * 1000 || air)) continue; if (mm.proj && myProj) continue; p.aci++; return start(s, p, k, i, { auto: 1, mod: mm.sp || mm.sup }); }
    }
    // super arts
    if (canSu && !air) {
      let lv = 0, mod = 0;
      if (motionOK(p, 'SUP') && (lt || md || hv)) lv = hv ? 3 : md ? 2 : 1;
      else if (spb && (inp & 64) && A[2] <= BUF + 2) { lv = v < 0 ? 3 : h < 0 ? 2 : 1; mod = 1; }
      else if (hv && (inp & 128) && A[3] <= BUF + 2) { lv = v < 0 ? 3 : h < 0 ? 2 : 1; mod = 1; }
      if (lv && p.sa >= lv * 1000 && !(C.mv['SA' + lv].proj && myProj)) return start(s, p, 'SA' + lv, i, { mod: mod });
      if (lv && mod) return false;
    }
    // drive impact
    if (dib && !air && (canN || kara) && !p.bo && p.dr > 0) { spend(s, p, i, BAR); return start(s, p, 'DI', i); }
    // specials
    if (canSp && !(m && m.sp && !kara)) {
      const nAtk = ((inp & 16) ? 1 : 0) + ((inp & 32) ? 1 : 0) + ((inp & 64) ? 1 : 0);
      if (lt || md || hv) for (const sp of C.sp) {
        if (!sp.m || !!sp.air !== air || !motionOK(p, sp.m)) continue;
        let key = hv ? sp.H : md ? sp.M : sp.L; const mm0 = C.mv[key];
        if (mm0.minY && p.y < mm0.minY * F) continue;
        if (nAtk >= 2 && !p.bo && p.dr > 0) { if (C.mv[sp.OD].proj && myProj) continue; spend(s, p, i, 2 * BAR); return start(s, p, sp.OD, i); }
        if (mm0.proj && myProj) continue;
        return start(s, p, key, i);
      }
      if (spb) {
        const d = v < 0 ? 2 : h > 0 ? 6 : h < 0 ? 4 : 5;
        for (const sp of C.sp) {
          if (!!sp.air !== air || (!air && sp.dir !== d)) continue;
          const mm0 = C.mv[sp.M]; if (mm0.minY && p.y < mm0.minY * F) continue;
          if (as && !p.bo && p.dr > 0 && !(C.mv[sp.OD].proj && myProj)) { spend(s, p, i, 2 * BAR); return start(s, p, sp.OD, i, { mod: 1 }); }
          if (mm0.proj && myProj) continue;
          return start(s, p, sp.M, i, { mod: 1 });
        }
      }
    }
    if (!(lt || md || hv)) return false;
    if (air) { if (m) return false; return start(s, p, hv ? 'jH' : md ? 'jM' : 'jL', i); }
    if (parryHeld(inp) && (canN || kara)) return false;
    // throw
    if ((canN || kara) && (inp & 16) && (inp & 32) && A[0] <= BUF + 2 && A[1] <= BUF + 2) { p.back = h < 0 ? 1 : 0; return start(s, p, 'throw', i); }
    const b = hv ? 'H' : md ? 'M' : 'L';
    let k = b === 'H' ? (v < 0 ? (h > 0 ? '3H' : '2H') : h > 0 ? '6H' : h < 0 ? '4H' : '5H') : b === 'M' ? (v < 0 ? '2M' : h > 0 ? '6M' : '5M') : (v < 0 ? '2L' : '5L');
    if (canN) {
      if (as) { p.ac = b; p.aci = 0; return start(s, p, C.auto[b][0][0], i, { auto: 1 }); }
      return start(s, p, k, i);
    }
    if (chain) {
      if (chain.indexOf(k) < 0) k = (v < 0 ? '5' : '2') + b; if (chain.indexOf(k) < 0) k = '5' + b;
      if (chain.indexOf(k) >= 0) return start(s, p, k, i);
    }
    return false;
  }
  function enterParry(s, p, i) { if (!spend(s, p, i, BAR / 2)) return false; p.st = 'parry'; p.sf = 0; p.mv = ''; p.vx = 0; p.plock = 0; p.arm = 0; s.ev.push({ t: 'parryOn', who: i }); return true; }
  function enterRush(s, p, i, fromCancel) { p.st = 'drush'; p.sf = fromCancel ? 2 : 0; p.mv = ''; p.vx = 0; p.d66 = 99; p.arm = 0; s.ev.push({ t: 'rush', who: i }); }
  function neutral(s, p, o, inp, i) {
    p.st = 'idle'; p.mv = '';
    if (o.x > p.x) p.face = 1; else if (o.x < p.x) p.face = -1;
    if (tryAttack(s, p, o, inp, i)) return;
    const C = CH[p.ch], h = hdir(inp, p.face), v = vdir(inp);
    if (parryHeld(inp) && enterParry(s, p, i)) return;
    if (p.d66 <= BUF) { p.d66 = 99; p.st = 'dashf'; p.sf = 0; s.ev.push({ t: 'dash', who: i }); return; }
    if (p.d44 <= BUF) { p.d44 = 99; p.st = 'dashb'; p.sf = 0; s.ev.push({ t: 'dash', who: i }); return; }
    let sub;
    if (v > 0) { p.st = 'prejump'; p.sf = 0; p.jd = h; p.vx = 0; return; }
    if (v < 0) { sub = 'crouch'; p.vx = 0; }
    else if (h > 0) { sub = 'walkf'; p.vx = p.face * tr(C.walkF * (p.bo ? 85 : 100) / 100); }
    else if (h < 0) { sub = 'walkb'; p.vx = -p.face * C.walkB; }
    else { sub = 'stand'; p.vx = 0; }
    if (sub !== p.sub) { p.sub = sub; p.subf = 0; } else p.subf++;
  }
  function atkFrame(s, p, o, m, i) {
    const f = p.mf;
    if (!m.isAir || m.sp) {
      let vx = 0, on = false;
      if (m.mv && !(m.stopOnHit && p.contact)) for (const r of m.mv) if (f >= r[0] && f <= r[1]) { vx = r[2]; on = true; }
      if (p.y <= 0 && !p.air2) p.vx = p.face * vx; else if (on) p.vx = p.face * vx;
    }
    if (m.hover && f < m.hover) { p.vx = 0; p.vy = GRAV; }
    if (m.rise && f === m.rise[0] && !(m.wEnd && !p.hitOK)) { p.vx = p.face * m.rise[1]; p.vy = m.rise[2]; if (p.y <= 0) p.y = 1; p.air2 = 1; s.ev.push({ t: 'rise', who: i }); }
    if (m.proj && f === m.proj.f) {
      s.proj.push({ o: i, ch: p.ch, k: p.mv, x: p.x + p.face * 40 * F, y: p.y + m.proj.y * F, dir: p.face, left: m.proj.hits, cd: 0, pause: 0, life: 0, mod: p.mod });
      s.ev.push({ t: 'fire', who: i, big: m.proj.big ? 1 : 0 });
    }
    if (m.wEnd && f === m.wEnd[0] && !p.hitOK) { if (m.wEnd[1] < 0) { p.st = 'land'; p.sf = 0; p.rec = -m.wEnd[1]; p.mv = ''; p.vx = 0; return; } p.mf = m.wEnd[1]; }
    if (m.arm && f > m.lastAct) p.arm = 0;
    for (const h of m.hits) if (f === h.s && !(h.req && !p.hitOK)) s.ev.push({ t: 'whiff', who: i, lvl: h.lvl });
    if (m.thr && f === 4) s.ev.push({ t: 'whiff', who: i, lvl: 0 });
  }
  function update(s, p, o, inp, i) {
    const C = CH[p.ch];
    p.sf++; if (p.tinv > 0) p.tinv--; if (p.pp > 0) p.pp--;
    switch (p.st) {
      case 'idle': neutral(s, p, o, inp, i); break;
      case 'prejump':
        if (p.sf >= 4) { p.st = 'jump'; p.sf = 0; p.vy = 1160; p.vx = p.jd * p.face * (p.jd < 0 ? C.jx - 40 : C.jx); p.y = 1; s.ev.push({ t: 'jump', who: i }); }
        break;
      case 'jump': tryAttack(s, p, o, inp, i); break;
      case 'land': p.vx = 0; if (p.sf >= p.rec) { toIdle(p); neutral(s, p, o, inp, i); } break;
      case 'dashf': p.vx = p.face * (p.sf < 11 ? C.dash : 180); if (p.sf >= 17) { toIdle(p); neutral(s, p, o, inp, i); } break;
      case 'dashb': p.vx = -p.face * (p.sf < 12 ? 600 : 120); if (p.sf >= 22) { toIdle(p); neutral(s, p, o, inp, i); } break;
      case 'drush':
        p.vx = p.face * (p.sf < 3 ? 0 : p.sf < 22 ? 980 : 420);
        if (p.sf >= 9 && tryAttack(s, p, o, inp, i)) { atkFrame(s, p, o, C.mv[p.mv], i); break; }
        if (p.sf >= 34) { toIdle(p); neutral(s, p, o, inp, i); }
        break;
      case 'parry':
        p.vx = 0; p.dr -= 8; p.drT = 70;
        if (p.dr <= 0) { p.dr = 0; burn(s, p, i); p.st = 'prec'; p.sf = 0; break; }
        if (p.plock > 0) { p.plock--; break; }
        if (p.d66 <= BUF && p.dr > 0) { spend(s, p, i, BAR / 2); enterRush(s, p, i, false); break; }
        if (!parryHeld(inp) && p.sf >= 10) { p.st = 'prec'; p.sf = 0; }
        break;
      case 'prec': if (p.sf >= 11) { toIdle(p); neutral(s, p, o, inp, i); } break;
      case 'atk': {
        p.mf++; let m = C.mv[p.mv];
        if (m.t < 900 && p.mf >= m.t) { toIdle(p); neutral(s, p, o, inp, i); break; }
        const live = p.contact && p.mf <= m.lastAct + 10, karaP = !m.sp && !m.sup && !m.thr && !m.di && p.mf <= 2 && !p.contact;
        if (live && (m.can & C_SP) && !m.sp && p.y <= 0 && !p.bo && p.dr > 0 && (p.d66 <= BUF || p.age[6] <= BUF || (p.age[1] <= BUF && p.age[2] <= BUF && parryHeld(inp)))) { spend(s, p, i, 3 * BAR); p.age = [99, 99, 99, 99, 99, 99, 99]; enterRush(s, p, i, true); break; }
        if (karaP && p.y <= 0 && parryHeld(inp) && enterParry(s, p, i)) break;
        if (!tryAttack(s, p, o, inp, i)) atkFrame(s, p, o, m, i); else atkFrame(s, p, o, C.mv[p.mv], i);
        break;
      }
      case 'block':
        if (p.age[5] <= BUF && !p.bo && p.dr > 0 && p.y <= 0) { spend(s, p, i, 2 * BAR); p.combo = 0; start(s, p, 'DREV', i); s.ev.push({ t: 'ex', who: i }); atkFrame(s, p, o, C.mv.DREV, i); break; }
        p.stun--; if (p.stun <= 0) { p.combo = 0; p.tinv = 6; toIdle(p); neutral(s, p, o, inp, i); } break;
      case 'hit':
        p.stun--; if (p.stun <= 0) { if (p.crum) { p.crum = 0; p.st = 'kd'; p.sf = 0; p.vx = 0; s.ev.push({ t: 'kd', x: p.x, y: 0, who: i }); } else { p.combo = 0; p.half = 0; p.tinv = 6; toIdle(p); neutral(s, p, o, inp, i); } } break;
      case 'kd': p.vx = p.roll && p.sf > 8 ? -p.face * 420 : 0; if (p.sf >= KD && p.hp > 0) { p.st = 'getup'; p.sf = 0; p.combo = 0; p.half = 0; } break;
      case 'getup': p.vx = p.roll && p.sf < 10 ? -p.face * 300 : 0; if (p.sf >= GETUP) { p.tinv = 8; toIdle(p); neutral(s, p, o, inp, i); } break;
      case 'tech': if (p.sf >= 24) { toIdle(p); neutral(s, p, o, inp, i); } break;
      case 'throwing': {
        const side = p.back && p.sf >= 12 ? -1 : 1;
        if (o.st === 'thrown') { o.x = p.x + p.face * side * 34 * F; o.y = p.sf >= 12 && p.sf < 22 ? 30 * F : 0; o.vx = 0; o.vy = 0; }
        if (p.sf < 8 && o.st === 'thrown' && (o.prev & 16) && (o.prev & 32) && o.age[0] <= 9 && o.age[1] <= 9 && !o.noTech) {
          p.st = 'tech'; o.st = 'tech'; p.sf = 0; o.sf = 0; o.y = 0; p.pv = -p.face * 900; o.pv = p.face * 900; s.ev.push({ t: 'tech', x: (p.x + o.x) >> 1, y: 60 * F }); break;
        }
        if (p.sf === 22) {
          const d = o.noTech ? 170 : 120; o.hp = Math.max(s.cfg.training ? 1 : 0, o.hp - d); o.st = 'ahit'; o.sf = 0; o.y = 20 * F; o.vx = p.face * side * 620; o.vy = 720; o.combo = 1; o.comboDmg = d; o.jug = 3; o.hard = 1; o.hs = 8; p.hs = 8; o.redT = 50;
          p.sa = Math.min(3000, p.sa + 80); o.sa = Math.min(3000, o.sa + 40);
          s.ev.push({ t: 'hit', x: o.x, y: 50 * F, lvl: 2, who: i, dmg: d, combo: 1, total: d, thr: 1, ch: o.noTech ? 2 : 0 });
        }
        if (p.sf >= 44) { toIdle(p); }
        break;
      }
      default: break;
    }
  }
  function land(s, p, i) {
    p.y = 0; p.vy = 0; p.air2 = 0;
    if (p.st === 'ahit') {
      if (p.gbPend && !p.gbUsed && p.hp > 0) { p.gbUsed = 1; p.gbPend = 0; p.y = 1; p.vy = 880; p.vx = tr(p.vx / 3); p.hs = 5; s.ev.push({ t: 'gbounce', x: p.x, y: 0, who: i }); return; }
      p.st = 'kd'; p.sf = 0; p.pv = tr(p.vx / 2); p.vx = 0; p.roll = (!p.hard && p.hp > 0 && hdir(p.prev, p.face) < 0) ? 1 : 0; s.ev.push({ t: 'kd', x: p.x, y: 0, who: i });
    } else if (p.st === 'atk') {
      const m = CH[p.ch].mv[p.mv]; p.st = 'land'; p.sf = 0; p.rec = m.landRec || 3; p.mv = ''; p.vx = 0; p.arm = 0; s.ev.push({ t: 'land', who: i });
    } else if (p.st === 'jump') { p.st = 'land'; p.sf = 0; p.rec = 2; p.vx = 0; s.ev.push({ t: 'land', who: i }); }
  }
  function physics(s, p, o, i) {
    if (p.st === 'thrown') return;
    const lim = STAGE - PW;
    if (p.y > 0) {
      p.vy -= GRAV + (p.st === 'ahit' ? p.jug * 5 : 0);
      p.x += p.vx; p.y += p.vy;
      if (p.y <= 0) land(s, p, i);
    } else if (p.st === 'ahit') { land(s, p, i);
    } else { p.x += p.vx + p.pv; p.pv = tr(p.pv * 86 / 100); if (p.pv > -12 && p.pv < 12) p.pv = 0; }
    if (p.x > lim || p.x < -lim) {
      const sg = p.x > 0 ? 1 : -1; p.x = sg * lim;
      if (p.st === 'ahit' && p.wbPend && !p.wbUsed && p.vx * sg >= 380 && p.hp > 0) {
        p.wbUsed = 1; p.wbPend = 0; p.vx = -sg * 260; p.vy = 820; p.hs = 9; s.ev.push({ t: 'wbounce', x: p.x + sg * PW, y: p.y + 45 * F, who: i });
      } else if (p.y <= 0 && p.pv * sg > 0 && (p.st === 'hit' || p.st === 'block') && o.y <= 0) { o.pv = -p.pv; p.pv = 0; }
    }
  }
  function pushboxes(s) {
    const a = s.p[0], b = s.p[1], lim = STAGE - PW;
    const skip = a.st === 'thrown' || b.st === 'thrown' || a.st === 'throwing' || b.st === 'throwing';
    if (!skip && a.y < b.y + 70 * F && b.y < a.y + 70 * F) {
      let dx = b.x - a.x; const ov = 2 * PW - Math.abs(dx);
      if (ov > 0) {
        let dir = dx > 0 ? 1 : dx < 0 ? -1 : (a.face > 0 ? 1 : -1);
        const h = ov >> 1; a.x -= dir * h; b.x += dir * (ov - h);
        if (a.x < -lim) { b.x += -lim - a.x; a.x = -lim; } if (a.x > lim) { b.x -= a.x - lim; a.x = lim; }
        if (b.x < -lim) { a.x += -lim - b.x; b.x = -lim; } if (b.x > lim) { a.x -= b.x - lim; b.x = lim; }
      }
    }
    for (const p of s.p) { if (p.x > lim) p.x = lim; if (p.x < -lim) p.x = -lim; }
    const d = b.x - a.x; if (Math.abs(d) > SEP) { const ex = (Math.abs(d) - SEP), sg = d > 0 ? 1 : -1, h = ex >> 1; a.x += sg * h; b.x -= sg * (ex - h); }
  }

  // ------------------------------------------------------------ boxes
  function crouching(p) {
    if (p.st === 'idle') return p.sub === 'crouch';
    if (p.st === 'atk') return !!CH[p.ch].mv[p.mv].crouch;
    if (p.st === 'block' || p.st === 'hit') return !!p.cb;
    return false;
  }
  function invul(p) {
    if (p.st === 'kd' || p.st === 'getup' || p.st === 'thrown' || p.st === 'throwing' || p.st === 'win') return true;
    if (p.st === 'dashb' && p.sf < 7) return true;
    if (p.st === 'atk') { const m = CH[p.ch].mv[p.mv]; if (m.inv && p.mf >= m.inv[0] && p.mf <= m.inv[1]) return true; }
    return false;
  }
  function hurt(p) {
    if (invul(p)) return [];
    const r = [];
    if (p.y > 0) r.push([p.x - 16 * F, p.y + 4 * F, p.x + 16 * F, p.y + 72 * F]);
    else if (crouching(p)) r.push([p.x - 20 * F, 0, p.x + 20 * F, 56 * F]);
    else r.push([p.x - 18 * F, 0, p.x + 18 * F, 86 * F]);
    if (p.st === 'atk') { const m = CH[p.ch].mv[p.mv]; if (!m.sp && !m.sup) for (const h of m.hits) if (p.mf >= h.s && p.mf <= h.e + 3) r.push(box(p, h.hb)); }
    return r;
  }
  function box(p, hb) {
    const x0 = p.x + p.face * hb[0] * F, x1 = p.x + p.face * (hb[0] + hb[2]) * F;
    return [Math.min(x0, x1), p.y + hb[1] * F, Math.max(x0, x1), p.y + (hb[1] + hb[3]) * F];
  }
  function hitboxes(p) {
    const r = []; if (p.st !== 'atk') return r; const m = CH[p.ch].mv[p.mv];
    for (const h of m.hits) if (p.mf >= h.s && p.mf <= h.e && !(p.hitMask & (1 << h.grp)) && !(h.req && !p.hitOK)) r.push({ h: h, b: box(p, h.hb) });
    return r;
  }
  function pbox(q) { const pr = CH[q.ch].mv[q.k].proj; return [q.x - pr.w * F / 2, q.y - pr.h * F / 2, q.x + pr.w * F / 2, q.y + pr.h * F / 2]; }
  function ovl(a, b) { return a[0] < b[2] && b[0] < a[2] && a[1] < b[3] && b[1] < a[3]; }

  // ------------------------------------------------------------ hits
  function canBlock(d, inp, fromX, g) {
    if (d.y > 0) return false;
    if (!(d.st === 'block' || d.st === 'idle' || d.st === 'land')) return false;
    const side = d.x > fromX ? 1 : d.x < fromX ? -1 : -d.face;
    if (!(inp & (side > 0 ? 8 : 4)) || (inp & (side > 0 ? 4 : 8))) return false;
    const cr = (inp & 2) !== 0;
    if (g === 'l' && !cr) return false; if (g === 'h' && cr) return false;
    return true;
  }
  // how the defender answers a touch: 'parry' | 'armor' | 'block' | 'hit'
  function answer(d, h, fromX, m, isProj) {
    if (d.st === 'parry' && d.y <= 0) return 'parry';
    if (d.st === 'atk' && d.arm > 0 && !m.sup) return 'armor';
    return canBlock(d, d.prev, fromX, isProj ? 'm' : h.g) ? 'block' : 'hit';
  }
  function immune(d, a, h, isProj) {
    if (d.st !== 'atk') return false; const m = CH[d.ch].mv[d.mv];
    if (!isProj && m.ainv && a.y > 0 && d.mf >= m.ainv[0] && d.mf <= m.ainv[1]) return true;
    if (!isProj && m.lowInv && h.g === 'l' && d.mf >= m.lowInv[0] && d.mf <= m.lowInv[1]) return true;
    return false;
  }
  function applyHit(s, ai, h, fromX, cx, cy, how, isProj, last, kind, m, mod) {
    const a = s.p[ai], d = s.p[1 - ai], di = 1 - ai, dinp = d.prev;
    const side = d.x > fromX ? 1 : d.x < fromX ? -1 : -d.face;
    const special = !!(m.sp || m.sup), drb = !isProj && a.drb ? 4 : 0;
    let hsp = h.hsp; if (!isProj) a.drb = 0;
    if (how === 'parry') {
      const perfect = d.sf <= 2 && d.plock === 0;
      d.dr = Math.min(DMAX, d.dr + 600); d.plock = Math.max(1, h.bstun - 3); d.hs = 8; if (!isProj) { a.hs = 8; a.contact = 1; }
      if (perfect) { s.freeze = isProj ? 8 : 20; s.frzWho = di; toIdle(d); d.hs = 0; d.tinv = 4; a.pp = 70; if (!isProj) a.hs = 14; s.ev.push({ t: 'pparry', x: cx, y: cy, who: di }); }
      else s.ev.push({ t: 'parry', x: cx, y: cy, who: di });
      return;
    }
    if (how === 'armor') {
      const dm = Math.max(1, tr(h.d / 2)); d.hp = Math.max(1, d.hp - dm); d.arm--; d.armHit = 1; d.redT = 50; d.hs = 9; if (!isProj) { a.hs = 9; a.contact = 1; a.hitOK = 1; }
      s.ev.push({ t: 'armor', x: cx, y: cy, who: di }); if (m.di && CH[d.ch].mv[d.mv].di) { s.slow = 54; s.ev.push({ t: 'diclash', x: cx, y: cy }); } return;
    }
    if (how === 'block') {
      const chip = (special && d.bo) ? Math.max(1, tr(h.d / 4)) : 0;
      d.hp = Math.max(s.cfg.training ? 1 : 0, d.hp - chip);
      if (!d.bo) { d.dr -= m.di ? 500 : tr(h.d * (special ? 3 : 5)); d.drT = 40; burn(s, d, di); }
      d.st = 'block'; d.sf = 0; d.stun = h.bstun + drb + (d.bo ? 4 : 0); d.cb = (dinp & 2) ? 1 : 0; d.pv = side * Math.max(h.kb, 380); d.vx = 0; d.redT = 40;
      hsp = Math.max(4, hsp - 2); d.hs = hsp; if (!isProj) { a.hs = hsp; a.contact = 1; }
      a.sa = Math.min(3000, a.sa + tr(h.d * 3 / 10)); d.sa = Math.min(3000, d.sa + tr(h.d / 5));
      s.ev.push({ t: 'block', x: cx, y: cy, lvl: h.lvl, who: ai });
      if (m.di && Math.abs(d.x) >= STAGE - PW - 70 * F) { d.st = 'hit'; d.crum = 2; d.stun = d.bo ? 120 : 72; d.pv = 0; d.combo = 1; d.comboDmg = 0; d.jug = 0; d.wbUsed = 0; d.gbUsed = 0; d.cb = 0; s.ev.push({ t: 'splat', x: d.x + side * PW, y: 50 * F, who: ai, stun: d.bo ? 1 : 0 }); }
      if (d.hp <= 0) { d.st = 'ahit'; d.y = 1; d.vx = side * 500; d.vy = 800; }
      return;
    }
    const inCombo = d.combo > 0 && (d.st === 'hit' || d.st === 'ahit');
    if (inCombo) d.combo++; else { d.combo = 1; d.comboDmg = 0; d.jug = 0; d.wbUsed = 0; d.gbUsed = 0; d.half = d.pp > 0 ? 1 : 0; }
    let pct = SCALE[Math.min(d.combo - 1, 9)]; if (m.minScale && pct < m.minScale) pct = m.minScale;
    let dmg = tr(h.d * pct / 100); if (kind) dmg = tr(dmg * 120 / 100); if (mod) dmg = tr(dmg * 80 / 100); if (d.half) dmg = tr(dmg / 2); if (a.bo) dmg = tr(dmg * 90 / 100); if (dmg < 1) dmg = 1;
    d.hp = Math.max(s.cfg.training ? 1 : 0, d.hp - dmg); d.comboDmg += dmg; d.redT = 50;
    const ko = d.hp <= 0, wasAir = d.y > 0 || d.st === 'ahit';
    let launch = (isProj && last && h.launchLast) ? h.launchLast : h.launch;
    const crumple = m.di && (kind || a.armHit) && !wasAir && !ko;
    if (crumple) { launch = null; s.slow = Math.max(s.slow, 36); }
    if (m.di && !d.bo) { d.dr -= BAR; burn(s, d, di); }
    if (kind) hsp += 2; if (ko) hsp = 22;
    const wasCr = crouching(d), wrong = !wasAir && !isProj && (h.g === 'l' || (h.g === 'h' && !m.isAir)) && (d.st === 'idle' || d.st === 'block') ? h.g : '';
    d.crum = 0; d.arm = 0;
    if (wasAir || launch || ko) {
      const vec = wasAir ? (((isProj && last && h.launchLast) ? h.launchLast : h.air) || [300, 620]) : (launch || [520, 820]);
      d.st = 'ahit'; d.sf = 0; d.mv = '';
      d.vx = side * vec[0] + (isProj ? 0 : tr(a.vx / 4)); d.vy = vec[1]; if (d.y <= 0) { d.y = 1; if (d.vy < 0) d.vy = 300; }
      d.jug++; d.pv = 0; d.hard = (m.sup || m.hardKd) ? 1 : 0;
      d.wbPend = ((h.wb || (isProj && last && h.wbLast)) && !d.wbUsed) ? 1 : 0; d.gbPend = (h.gb && !d.gbUsed) ? 1 : 0;
    } else {
      d.st = 'hit'; d.sf = 0; d.mv = ''; d.stun = crumple ? 84 : h.stun + (kind === 2 ? 4 : kind === 1 ? 2 : 0) + drb; d.pv = crumple ? side * 200 : side * h.kb; d.vx = 0; d.cb = wasCr && !crumple ? 1 : 0; d.hitHi = (cy - d.y) > 44 * F ? 1 : 0; if (crumple) d.crum = 1;
    }
    d.hs = hsp; if (!isProj) { a.hs = hsp; a.contact = 1; a.hitOK = 1; }
    a.sa = Math.min(3000, a.sa + (m.sup ? 0 : tr(dmg * 7 / 10))); d.sa = Math.min(3000, d.sa + tr(dmg * 3 / 10));
    if (!a.bo) a.dr = Math.min(DMAX, a.dr + tr(dmg * 2));
    s.ev.push({ t: 'hit', x: cx, y: cy, lvl: h.lvl, who: ai, dmg: dmg, combo: d.combo, total: d.comboDmg, ch: kind, ko: ko ? 1 : 0, sp: special ? 1 : 0, di: m.di ? 1 : 0, crum: crumple ? 1 : 0, wrong: wrong });
  }
  function kindOf(d) { if (d.st === 'prec') return 2; if (d.st !== 'atk') return 0; const m = CH[d.ch].mv[d.mv]; return d.mf > m.lastAct ? 2 : 1; }
  function resolveHits(s) {
    const list = [];
    for (let i = 0; i < 2; i++) {
      const a = s.p[i], d = s.p[1 - i];
      const hbs = hitboxes(a); if (!hbs.length) continue;
      const hu = hurt(d), m = CH[a.ch].mv[a.mv]; let done = false;
      for (const hb of hbs) { if (done) break; if (immune(d, a, hb.h, false)) continue; for (const u of hu) if (ovl(hb.b, u)) {
        const cx = (Math.max(hb.b[0], u[0]) + Math.min(hb.b[2], u[2])) >> 1, cy = (Math.max(hb.b[1], u[1]) + Math.min(hb.b[3], u[3])) >> 1;
        list.push({ ai: i, h: hb.h, cx: cx, cy: cy, how: answer(d, hb.h, a.x, m, false), kind: kindOf(d), m: m, mod: a.mod }); done = true; break;
      } }
    }
    for (const r of list) { const a = s.p[r.ai]; a.hitMask |= (1 << r.h.grp); applyHit(s, r.ai, r.h, a.x, r.cx, r.cy, r.how, false, false, r.kind, r.m, r.mod); }
  }
  function resolveThrows(s) {
    const g = [false, false];
    for (let i = 0; i < 2; i++) {
      const a = s.p[i], d = s.p[1 - i];
      if (a.st !== 'atk' || a.hs > 0) continue; const m = CH[a.ch].mv[a.mv]; if (!m.thr || a.mf < 4 || a.mf > 6) continue;
      if (d.y > 0 || d.tinv > 0) continue;
      if (['idle', 'atk', 'dashf', 'land', 'parry', 'prec', 'drush'].indexOf(d.st) < 0 && !(d.st === 'dashb' && d.sf >= 7)) continue;
      if (d.st === 'atk' && CH[d.ch].mv[d.mv].inv && invul(d)) continue;
      if (Math.abs(d.x - a.x) > 2 * PW + 16 * F) continue;
      g[i] = true;
    }
    if (g[0] && g[1]) { for (let i = 0; i < 2; i++) { const p = s.p[i]; p.st = 'tech'; p.sf = 0; p.mv = ''; p.pv = -p.face * 900; } s.ev.push({ t: 'tech', x: (s.p[0].x + s.p[1].x) >> 1, y: 60 * F }); return; }
    for (let i = 0; i < 2; i++) if (g[i]) {
      const a = s.p[i], d = s.p[1 - i];
      d.noTech = ((d.st === 'atk' && !CH[d.ch].mv[d.mv].thr) || d.st === 'parry' || d.st === 'prec' || d.st === 'drush') ? 1 : 0;
      a.st = 'throwing'; a.sf = 0; a.mv = ''; a.vx = 0; d.st = 'thrown'; d.sf = 0; d.mv = ''; d.vx = 0; d.vy = 0; d.pv = 0; d.face = -a.face; d.combo = 0; d.arm = 0; d.crum = 0;
      s.ev.push({ t: 'grab', who: i, x: d.x, y: 60 * F });
    }
  }
  function projectiles(s) {
    const P = s.proj;
    for (const q of P) {
      const pr = CH[q.ch].mv[q.k].proj; q.life++;
      if (q.pause > 0) { q.pause--; continue; }
      if (q.cd > 0) { q.cd--; q.x += q.dir * tr(pr.vx / 6); } else q.x += q.dir * pr.vx;
      if (Math.abs(q.x) > STAGE + 60 * F || q.life > 300) q.left = 0;
    }
    for (let i = 0; i < P.length; i++) for (let j = i + 1; j < P.length; j++) {
      const a = P[i], b = P[j]; if (a.o === b.o || a.left <= 0 || b.left <= 0) continue;
      if (ovl(pbox(a), pbox(b))) { a.left--; b.left--; a.cd = 6; b.cd = 6; s.ev.push({ t: 'clash', x: (a.x + b.x) >> 1, y: a.y }); }
    }
    for (const q of P) {
      if (q.left <= 0 || q.cd > 0 || q.pause > 0) continue;
      const d = s.p[1 - q.o], m = CH[q.ch].mv[q.k], pr = m.proj, b = pbox(q);
      for (const u of hurt(d)) if (ovl(b, u)) {
        q.left--; const last = q.left <= 0, fx = q.x - q.dir * 50 * F;
        applyHit(s, q.o, pr, fx, (Math.max(b[0], u[0]) + Math.min(b[2], u[2])) >> 1, q.y, answer(d, pr, fx, m, true), true, last, kindOf(d), m, q.mod);
        q.cd = 5; q.pause = d.hs; break;
      }
    }
    for (let i = P.length - 1; i >= 0; i--) if (P[i].left <= 0) { s.ev.push({ t: 'pend', x: P[i].x, y: P[i].y }); P.splice(i, 1); }
  }

  // ------------------------------------------------------------ step
  function step(s, i0, i1) {
    s.ev = []; s.frame++;
    const raw = [i0 & MASK, i1 & MASK];
    const live = s.phase === 'fight';
    for (let i = 0; i < 2; i++) {
      const p = s.p[i]; let inp = live ? raw[i] : 0;
      if ((inp & 12) === 12) inp &= ~12; if ((inp & 3) === 3) inp &= ~2;
      raw[i] = inp;
      const frozen = s.freeze > 0 || p.hs > 0;
      p.buf.push(inp); p.buf.shift();
      const pr = inp & ~p.prev;
      for (let b = 0; b < NB; b++) { if (pr & (16 << b)) p.age[b] = 0; else if (!frozen && p.age[b] < 99) p.age[b]++; }
      if (dashIn(p, 1)) p.d66 = 0; else if (!frozen && p.d66 < 99) p.d66++;
      if (dashIn(p, -1)) p.d44 = 0; else if (!frozen && p.d44 < 99) p.d44++;
      p.prev = inp;
    }
    if (s.phase === 'intro') { s.pt++; if (s.pt === 1) s.ev.push({ t: 'round', n: s.round }); if (s.pt >= INTRO) { s.phase = 'fight'; s.pt = 0; s.ev.push({ t: 'fight' }); } }
    if (s.freeze > 0) { s.freeze--; return s; }
    if (s.phase === 'ko') { s.pt++; if (s.pt < 140 && s.pt % 4 !== 0) return s; }
    else if (s.slow > 0) { s.slow--; if (s.slow % 3 !== 0) return s; }
    const fr = [s.p[0].hs > 0, s.p[1].hs > 0];
    for (let i = 0; i < 2; i++) { const p = s.p[i]; if (fr[i]) p.hs--; else update(s, p, s.p[1 - i], raw[i], i); }
    for (let i = 0; i < 2; i++) if (!fr[i]) physics(s, s.p[i], s.p[1 - i], i);
    pushboxes(s);
    resolveThrows(s);
    projectiles(s);
    resolveHits(s);
    for (let i = 0; i < 2; i++) {
      const p = s.p[i];
      if (p.redT > 0) p.redT--; else if (p.red > p.hp) p.red = Math.max(p.hp, p.red - 8); if (p.red < p.hp) p.red = p.hp;
      if (p.drT > 0) p.drT--; else if (['idle', 'dashf', 'dashb', 'jump', 'land', 'prejump', 'atk'].indexOf(p.st) >= 0 && s.phase === 'fight') {
        p.dr = Math.min(DMAX, p.dr + (p.bo ? 6 : p.st === 'idle' && p.sub === 'walkf' ? 7 : 5));
        if (p.bo && p.dr >= DMAX) { p.bo = 0; s.ev.push({ t: 'recover', who: i }); }
      }
    }
    if (s.phase === 'fight') {
      if (s.cfg.training) {
        for (const p of s.p) if (p.combo === 0 && p.st === 'idle' && p.redT === 0) { p.hp = CH[p.ch].hp; p.sa = 3000; if (p.drT === 0 && !p.bo) p.dr = DMAX; }
      } else {
        s.time--;
        const d0 = s.p[0].hp <= 0, d1 = s.p[1].hp <= 0;
        if (d0 || d1 || s.time <= 0) {
          s.phase = 'ko'; s.pt = 0; let w = -1;
          if (d0 || d1) { s.koType = 'ko'; w = d0 && d1 ? -1 : d0 ? 1 : 0; }
          else { s.koType = 'time'; const r0 = s.p[0].hp * CH[s.p[1].ch].hp, r1 = s.p[1].hp * CH[s.p[0].ch].hp; w = r0 > r1 ? 0 : r1 > r0 ? 1 : -1; }
          s.rw = w; if (w >= 0) { s.p[w].wins++; if (s.koType === 'ko' && s.p[w].hp >= CH[s.p[w].ch].hp) s.koType = 'perfect'; } else s.koType = d0 ? 'dko' : 'draw';
          s.ev.push({ t: 'ko', type: s.koType, w: w });
        }
      }
    } else if (s.phase === 'ko') {
      if (s.pt === 150 && s.rw >= 0) { const w = s.p[s.rw]; if (w.st === 'idle' && w.y <= 0) { w.st = 'win'; w.sf = 0; w.vx = 0; } s.ev.push({ t: 'winpose', w: s.rw }); }
      if (s.pt >= KOTIME) {
        const need = s.cfg.rounds;
        if (s.p[0].wins >= need || s.p[1].wins >= need || s.round >= need * 2 + 1) {
          s.phase = 'over'; s.winner = s.p[0].wins > s.p[1].wins ? 0 : s.p[1].wins > s.p[0].wins ? 1 : -1; s.ev.push({ t: 'over', w: s.winner });
        } else { s.round++; resetRound(s); }
      }
    }
    return s;
  }
  return { F, BT, MASK, STAGE, SEP, CH, INTRO, KOTIME, KD, GETUP, DMAX, newMatch, step, clone, hash, hurt, hitboxes, pbox, crouching, invul, npad, hdir };
})();
if (typeof module !== 'undefined') module.exports = SIM;
