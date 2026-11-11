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
  const BT = { U: 1, D: 2, L: 4, R: 8, LT: 16, MD: 32, HV: 64, SP: 128, AS: 256, DI: 512, PA: 1024, HM: 2048 };
  const MASK = 4095, NB = 8;
  const STAGE = 375 * F, SEP = 345 * F, PW = 17 * F, HS = 0.7, JG = 58;
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
    if (m.mv && !m.xm) m.mv = m.mv.map(r => [r[0], r[1], tr(r[2] * HS)]); if (m.rise) m.rise = [m.rise[0], tr(m.rise[1] * HS), m.rise[2]]; if (m.pcLaunch) m.pcLaunch = [tr(m.pcLaunch[0] * HS), m.pcLaunch[1]];
    if (m.proj) { const q = m.proj; q.vx = tr(q.vx * HS); q.kb = tr(q.kb * HS); q.w = tr(q.w * 0.8); if (q.launchLast) q.launchLast = [tr(q.launchLast[0] * HS), q.launchLast[1]]; if (q.air) q.air = [tr(q.air[0] * HS), q.air[1]]; }
    m.hits.forEach((h, i) => {
      if (h.e > m.lastAct) m.lastAct = h.e;
      if (h.g == null) h.g = m.g; if (h.hsp == null) h.hsp = m.hsp; if (h.lvl == null) h.lvl = m.lvl;
      if (h.kb == null) h.kb = m.kb; if (!h.stun) h.stun = 20; if (!h.bstun) h.bstun = 14;
      if (h.grp == null) h.grp = i;
      h.kb = tr(h.kb * HS); if (h.launch) h.launch = [tr(h.launch[0] * HS), h.launch[1]]; if (h.air) h.air = [tr(h.air[0] * HS), h.air[1]];
      if (!m.xh && h.hb) h.hb = [tr(h.hb[0] * 0.75), h.hb[1], tr(h.hb[2] * 0.75), h.hb[3]]; if (m.dd != null && h.dd == null) h.dd = tr(m.dd / m.hits.length);
    });
    return m;
  }

  // Frame data follows SF6 Ken (Capcom's published frame table, damage and gauges divided by ten).
  // Reach and hitbox sizes are approximations: no numeric hitbox source was available.
  function ken() {
    const mv = {}, N = (k, o) => { mv[k] = mk(o); };
    const CS = C_SP | C_SU, ch = { 5: '5LP', 2: '2LP' };
    N('5LP', { n: 'Light Punch', su: 4, ac: 3, rc: 7, d: 30, ha: 4, ba: -1, xh: 1, hb: [17, 63, 33, 14], kb: 300, hsp: 9, dd: 50, sg: 30, ss: 1, can: C_CH | CS, chain: ch, an: { w: 'jabW', s: 'jabS' } });
    N('5LK', { n: 'Light Kick', su: 5, ac: 2, rc: 12, d: 30, ha: 0, ba: -2, xh: 1, hb: [8, 18, 46, 30], kb: 340, hsp: 9, dd: 50, sg: 30, ss: 1, an: { w: 'lkW', s: 'lkS' } });
    N('2LK', { n: 'Crouching Light Kick', su: 5, ac: 3, rc: 10, d: 20, g: 'l', ha: 1, ba: -3, xh: 1, hb: [15, 0, 41, 22], kb: 300, hsp: 9, dd: 50, sg: 30, ss: 1, crouch: 1, can: C_CH, chain: ch, an: { w: 'clkW', s: 'clkS' } });
    N('2LP', { n: 'Crouching Light Punch', su: 4, ac: 2, rc: 9, d: 30, ha: 5, ba: -1, xh: 1, hb: [19, 39, 29, 14], kb: 280, hsp: 9, dd: 50, sg: 30, ss: 1, crouch: 1, can: C_CH | CS, chain: ch, an: { w: 'cjabW', s: 'cjabS' } });
    N('5MK', { n: 'Medium Kick', su: 8, ac: 3, rc: 20, d: 60, ha: 3, ba: -5, xh: 1, hb: [14, 48, 40, 36], kb: 520, hsp: 11, lvl: 1, dd: 400, sg: 70, can: C_SU, tc: { M: 'TFK2' }, an: { w: 'lkW', s: 'mkS' } });
    N('TFK2', { n: 'Triple Flash Kicks', su: 11, ac: 2, rc: 27, d: 40, ba: -12, mv: [[1, 9, 420]], xh: 1, hb: [15, 49, 37, 29], launch: [170, 800], air: [170, 800], hsp: 11, lvl: 1, dd: 250, sg: 40, tc: { M: 'TFK3' }, an: { w: 'hkW', s: 'mkS' } });
    N('TFK3', { n: 'Triple Flash Kicks', su: 13, ac: 3, rc: 28, d: 70, ba: -11, mv: [[1, 11, 300]], xh: 1, hb: [17, 70, 39, 28], launch: [520, 720], air: [520, 720], hsp: 13, lvl: 2, dd: 300, sg: 50, an: { w: 'jhkW', s: 'hkS' } });
    N('5MP', { n: 'Medium Punch', su: 5, ac: 4, rc: 14, d: 60, ha: 4, ba: -2, xh: 1, hb: [10, 47, 26, 19], kb: 460, hsp: 11, lvl: 1, dd: 300, sg: 50, can: CS, tc: { M: 'CB2' }, an: { w: 'mpW', s: 'mpS' } });
    N('CB2', { n: 'Chin Buster', su: 11, ac: 3, rc: 27, d: 40, ba: -14, mv: [[1, 9, 420]], xh: 1, hb: [6, 40, 40, 52], launch: [110, 940], air: [110, 940], hsp: 13, lvl: 1, dd: 100, sg: 30, cx: 1, can: CS, an: { w: 'cupW', s: 'cupS' } });
    N('2MK', { n: 'Crouching Medium Kick', su: 7, ac: 3, rc: 19, d: 50, g: 'l', ha: -2, ba: -6, xh: 1, hb: [20, 0, 40, 21], kb: 460, hsp: 9, lvl: 1, dd: 200, sg: 60, ss: 1, crouch: 1, can: CS, an: { w: 'clkW', s: 'cmkS' } });
    N('5HK', { n: 'Heavy Kick', su: 12, ac: 2, rc: 23, d: 80, ha: 1, ba: -5, xh: 1, hb: [15, 47, 49, 21], kb: 900, hsp: 13, lvl: 1, dd: 600, sg: 100, ss: 2, wr: 2, pcLaunch: [160, 900], an: { w: 'hkW', s: 'hkS' } });
    N('5HP', { n: 'Heavy Punch', su: 10, ac: 5, rc: 17, d: 80, ha: 3, ba: -2, xh: 1, hb: [17, 57, 31, 16], kb: 760, hsp: 13, lvl: 1, dd: 500, sg: 100, can: CS, an: { w: 'strW', s: 'strS' } });
    N('2HP', { n: 'Crouching Heavy Punch', su: 8, ac: 4, rc: 24, d: 80, ha: 3, ba: -7, xh: 1, hb: [4, 40, 30, 72], kb: 560, air: [140, 950], hsp: 13, lvl: 1, dd: 600, sg: 100, crouch: 1, stand: 1, can: CS, an: { w: 'cupW', s: 'cupS' } });
    N('2HK', { n: 'Sweep', su: 8, ac: 3, rc: 24, d: 90, g: 'l', ba: -10, xh: 1, hb: [18, 13, 49, 28], launch: [320, 480], hardKd: 1, hsp: 13, lvl: 1, dd: 400, sg: 100, crouch: 1, an: { w: 'swW', s: 'swS' } });
    N('jLP', { n: 'Jumping Light Punch', air: 1, su: 5, ac: 7, t: 999, g: 'h', d: 30, fs: 15, fb: 11, xh: 1, hb: [-2, 51, 30, 28], kb: 400, hsp: 9, dd: 150, sg: 30, landRec: 3, an: { w: 'jump', s: 'jhpS' } });
    N('jMP', { n: 'Jumping Medium Punch', air: 1, su: 6, ac: 4, t: 999, g: 'h', d: 70, fs: 18, fb: 14, xh: 1, hb: [6, 33, 37, 19], kb: 450, air2: 1, hsp: 11, lvl: 1, dd: 250, sg: 50, landRec: 3, can: C_SP, an: { w: 'jump', s: 'jhpS' } });
    N('jHK', { n: 'Jumping Heavy Kick', air: 1, su: 10, ac: 7, t: 999, g: 'h', d: 80, fs: 20, fb: 16, xh: 1, hb: [-7, 12, 50, 21], kb: 500, hsp: 13, lvl: 1, dd: 400, sg: 100, landRec: 3, an: { w: 'jhkW', s: 'jhkS' } });
    N('jLK', { n: 'Jumping Light Kick', air: 1, su: 6, ac: 6, t: 999, g: 'h', d: 30, fs: 15, fb: 11, xh: 1, hb: [-24, 2, 47, 24], kb: 400, hsp: 9, dd: 150, sg: 30, landRec: 3, an: { w: 'jump', s: 'jlkS' } });
    N('jMK', { n: 'Jumping Medium Kick', air: 1, su: 7, ac: 6, t: 999, g: 'h', d: 50, fs: 18, fb: 14, xh: 1, hb: [-31, 14, 76, 27], kb: 450, hsp: 11, lvl: 1, dd: 250, sg: 50, landRec: 3, an: { w: 'jump', s: 'jlkS' } });
    N('jHP', { n: 'Jumping Heavy Punch', air: 1, su: 9, ac: 6, t: 999, g: 'h', d: 80, fs: 20, fb: 16, xh: 1, hb: [2, -6, 24, 30], kb: 500, hsp: 13, lvl: 1, dd: 400, sg: 100, landRec: 3, an: { w: 'jhpW', s: 'jhpS' } });
    // directional aerials: up launches, down spikes into a ground bounce, back hits behind
    N('jUP', { n: 'Rising Lash', air: 1, su: 5, t: 999, g: 'h', landRec: 2, dd: 200, sg: 50, hits: [{ s: 4, e: 6, hb: [4, 40, 42, 50], d: 34, stun: 18, bstun: 14, launch: [30, 960], air: [30, 960], kb: 300, lvl: 1, hsp: 8, g: 'h' }, { s: 7, e: 11, hb: [-34, 56, 52, 50], d: 34, stun: 18, bstun: 14, launch: [-20, 1000], air: [-20, 1000], kb: 300, lvl: 1, hsp: 8, g: 'h' }] });
    // up + special: a rising spin that drags them up with it and flicks them off the top; no more actions until landing
    N('AUP', { n: 'Riptide', sp: 1, air: 1, su: 5, t: 999, rise: [4, 150, 1080], landRec: 16, dd: 300, sg: 60, hits: [4, 8, 12, 16].map(f => ({ s: f, e: f + 2, hb: [-24, -6, 56, 76], d: 14, stun: 20, bstun: 12, launch: [0, 620], air: [0, 300], lk: 1, carry: [14, 10], kb: 0, lvl: 1, hsp: 4 })).concat([{ s: 21, e: 24, hb: [-26, 0, 60, 86], d: 50, stun: 24, bstun: 16, launch: [150, 1050], air: [150, 1050], kb: 300, lvl: 2, hsp: 12 }]) });
    // back + special: a slow, enormous swing behind. Clean it sends them flying; caught low on the way down it spikes
    N('ABK', { n: 'Undertow', sp: 1, air: 1, su: 14, t: 999, landRec: 18, dd: 500, sg: 100, hits: [{ s: 13, e: 16, hb: [-70, 14, 64, 70], d: 130, stun: 30, bstun: 22, launch: [640, 580], air: [720, 500], wb: 1, kb: 700, lvl: 2, hsp: 17 }, { s: 17, e: 21, hb: [-60, -20, 52, 44], d: 110, stun: 28, bstun: 20, launch: [300, 760], air: [110, -1500], gb: 1, kb: 500, lvl: 2, hsp: 15 }] });
    N('jDN', { n: 'Falling Heel', air: 1, su: 9, t: 999, g: 'h', landRec: 4, dd: 300, sg: 60, hits: [{ s: 8, e: 14, hb: [-6, -16, 34, 42], d: 60, stun: 22, bstun: 16, air: [60, -1300], gb: 1, kb: 400, lvl: 2, hsp: 12, g: 'h' }] });
    N('jBK', { n: 'Back Lash', air: 1, su: 7, t: 999, g: 'h', landRec: 3, dd: 250, sg: 50, hits: [{ s: 6, e: 11, hb: [-56, 22, 50, 26], d: 60, stun: 20, bstun: 15, air: [460, 520], kb: 450, lvl: 1, hsp: 11, g: 'h' }] });
    // down + special in the air: a dive that spikes an airborne opponent into the floor and bounces off them so the juggle can go on
    N('ASPK', { n: 'Falling Star', sp: 1, air: 1, su: 9, t: 999, rise: [8, 400, -1150], landRec: 12, dd: 300, sg: 60, hits: [{ s: 8, e: 34, hb: [-4, -22, 44, 56], d: 80, stun: 26, bstun: 18, launch: [90, 1050], air: [70, -1500], gb: 1, reb: 780, kb: 300, lvl: 2, hsp: 13 }] });
    // Slipstream: back + special. The body lets go of its shape and pours itself at the opponent wherever they are. On hit it pops them up and
    // leaves you beside them with every air option back. Blocked, parried or missed it is a long helpless fall.
    const hm = (k, o) => N(k, Object.assign({ n: 'Slipstream', sp: 1, t: 999, landRec: 30, dd: 300, sg: 60 }, o, { hits: [{ s: o.home[0] + 1, e: o.home[1], hb: [-6, 12, 46, 50], d: o.od ? 60 : 50, stun: 24, bstun: 12, launch: [30, 820], air: [30, 820], free: 1, np: 1, kb: 200, lvl: 2, hsp: 12 }] }));
    hm('HOME', { su: 14, home: [12, 46, 840] }); hm('HOME_OD', { od: 1, su: 10, home: [8, 44, 980], pinv: [0, 44] });
    hm('AHOME', { air: 1, su: 10, home: [8, 42, 840], hover: 8 }); hm('AHOME_OD', { air: 1, od: 1, su: 8, home: [6, 42, 980], hover: 6, pinv: [0, 42] });
    N('njHK', { n: 'Neutral Jump Heavy Kick', air: 1, su: 6, ac: 6, t: 999, g: 'h', d: 90, fs: 21, fb: 17, xh: 1, hb: [9, 28, 33, 34], kb: 500, hsp: 13, lvl: 1, dd: 400, sg: 100, landRec: 3, an: { w: 'jhkW', s: 'jhkS' } });
    N('throw', { n: 'Throw', su: 5, ac: 3, rc: 23, thr: 1, hits: [], an: { w: 'grabW', s: 'grabS' } });
    N('DI', { n: 'Drive Impact', su: 26, ac: 2, rc: 35, d: 80, arm: 2, di: 1, ba: -3, xh: 1, hb: [4, 30, 49, 44], kb: 1500, launch: [650, 540], air: [650, 540], wb: 1, lvl: 2, hsp: 16, dd: 500, ss: 1, xm: 1, mv: [[16, 26, 620]], an: { k: [[0, 'idle'], [14, 'diW'], [24, 'diW'], [26, 'diS'], [40, 'diS'], [62, 'idle']] } });
    N('DREV', { n: 'Drive Reversal', su: 20, ac: 3, rc: 26, d: 50, inv: [0, 21], ba: -6, xh: 1, hb: [4, 30, 50, 44], launch: [760, 430], air: [760, 430], lvl: 1, hsp: 11, dd: 0, xm: 1, mv: [[10, 20, 250]], an: { w: 'strW', s: 'strS' } });
    // Quick Dash branches
    N('TK', { n: 'Thunder Kick', su: 18, ac: 3, rc: 20, d: 100, g: 'h', ha: 3, ba: -3, hb: [8, 30, 52, 60], kb: 560, hsp: 13, lvl: 1, dd: 250, sg: 50, can: C_SU, mv: [[0, 12, 300]], an: { w: 'axW', s: 'axS' } });
    N('FSK', { n: 'Forward Step Kick', su: 8, ac: 4, rc: 20, wr: 4, d: 80, ha: 1, ba: -4, hb: [10, 30, 72, 18], kb: 700, hsp: 13, lvl: 1, dd: 500, sg: 100, ss: 1, can: C_SU, mv: [[0, 7, 520]], an: { w: 'lkW', s: 'mkS' } });
    // Hadoken (Modern has only the heavy and the Overdrive)
    const fb = (k, su, t, st, bs, od) => N(k, { n: 'Hadoken', sp: 1, su: su, t: t, hits: [], fc: [su, su + 12], can: C_SU, od: od, sg: 60,
      proj: { f: su - 1, vx: 780, y: 50, w: 26, h: 22, hits: od ? 2 : 1, d: od ? 40 : 60, stun: st, bstun: bs, kb: 560, hsp: 9, lvl: 1, dd: od ? 125 : 250 },
      an: { k: [[0, 'idle'], [su - 6, 'fbW'], [su - 1, 'fbS'], [su + 14, 'fbS'], [t, 'idle']] } });
    fb('HAD_H', 12, 49, 24, 20); fb('HAD_OD', 12, 40, 8, 8, 1);
    // Shoryuken
    const hbA = [2, 30, 56, 62], hbB = [0, 20, 56, 90], g = (s, e, d, o) => Object.assign({ s: s, e: e, hb: hbB, d: d, np: 1, stun: 26, kb: 0, air: [40, 600], lvl: 1, hsp: 9, bstun: 18 }, o || {});
    const fin = (s, e, d, o) => Object.assign({ s: s, e: e, hb: hbB, d: d, launch: [200, 1150], air: [200, 1150], lvl: 2, hsp: 13, bstun: 18 }, o || {});
    const sr = (k, su, hits, vy, rec, o) => N(k, Object.assign({ n: 'Shoryuken', sp: 1, su: su, t: 999, mv: [[0, su - 1, 500]], rise: [su + 1, 150, vy], landRec: rec, can: C_SU, hits: hits, ss: 2,
      an: { k: [[0, 'cupW'], [su - 1, 'srS'], [su + 12, 'srS2'], [su + 32, 'jump']], spin: [su, su + 26, 360] } }, o || {}));
    sr('SRK_L', 5, [fin(4, 13, 110, { hb: hbA, bstun: 20 })], 930, 12, { ainv: [0, 13], dd: 400, sg: 80 });
    sr('SRK_M', 6, [g(5, 6, 70, { hb: hbA }), fin(9, 14, 60)], 1147, 12, { ainv: [0, 7], dd: 400, sg: 100 });
    sr('SRK_H', 7, [g(6, 7, 80, { hb: hbA }), g(8, 9, 30), fin(10, 12, 30, { bstun: 20 })], 1393, 15, { ainv: [0, 8], dd: 420, sg: 120 });
    sr('SRK_OD', 6, [g(5, 6, 20, { hb: hbA }), g(10, 11, 20), g(15, 16, 20), g(20, 21, 20), g(25, 26, 20), fin(32, 38, 60, { bstun: 17 })], 1271, 17, { inv: [0, 9], od: 1, dd: 500, sg: 150, rise: [32, 150, 1271], an: { k: [[0, 'cupW'], [5, 'srS'], [30, 'cupS'], [33, 'srS'], [46, 'srS2'], [68, 'jump']], spin: [32, 58, 720] } });
    sr('QSRK', 8, [g(7, 8, 50, { hb: hbA }), g(9, 10, 45), g(11, 12, 25), g(13, 14, 25), fin(15, 17, 25, { bstun: 16 })], 1240, 17, { ainv: [0, 17], dd: 400, sg: 140, rise: [10, 150, 1240], fire: 1, an: { k: [[0, 'cupW'], [7, 'srS'], [24, 'srS2'], [46, 'jump']], spin: [10, 36, 720] } });
    // Tatsumaki
    const sb = [-4, 40, 58, 24], th = (s, d, last, wb) => last ? { s: s, e: s + 2, hb: sb, d: d, launch: last, air: last, wb: wb, lvl: 2, hsp: 12, bstun: 17 } : { s: s, e: s + 1, hb: sb, d: d, stun: 18, kb: 110, air: [260, 420], lvl: 1, hsp: 7, bstun: 16 };
    const ts = (k, su, t, run, spd, hits, deg, o) => N(k, Object.assign({ n: 'Tatsumaki Senpu-kyaku', sp: 1, su: su, t: t, mv: [[Math.max(1, su - 7), run, spd]], hits: hits, lowInv: [4, run], dd: 400, sg: 100,
      an: { k: [[0, 'idle'], [Math.max(1, su - 6), 'spW'], [su - 1, 'spS'], [run + 2, 'spS'], [t, 'idle']], spin: [Math.max(0, su - 4), run + 4, deg] } }, o || {}));
    ts('TAT_L', 4, 46, 14, 460, [th(3, 30), Object.assign(th(9, 40, [450, 660]), { bstun: 23 })], 720);
    ts('TAT_M', 14, 61, 30, 470, [th(13, 45), Object.assign(th(24, 45, [600, 700]), { bstun: 25 })], 720);
    ts('TAT_H', 16, 78, 47, 480, [th(15, 30), th(27, 30), Object.assign(th(40, 40, [780, 760], 1), { bstun: 26 })], 1440);
    ts('TAT_OD', 9, 90, 56, 520, [Object.assign(th(8, 30), { bstun: 21 }), Object.assign(th(18, 20), { req: 1 }), Object.assign(th(28, 20), { req: 1 }), Object.assign(th(38, 20), { req: 1 }), Object.assign(th(50, 80, [1000, 820], 1), { req: 1 })], 2160, { od: 1 });
    ts('QTAT', 13, 78, 57, 520, [th(12, 20), th(22, 20), th(32, 20), th(42, 20), { s: 52, e: 54, hb: sb, d: 40, stun: 29, kb: 320, air: [500, 620], lvl: 1, hsp: 11, bstun: 17 }], 2160, { ss: 1 });
    N('ATAT', { n: 'Air Tatsumaki', sp: 1, air: 1, su: 11, t: 999, landRec: 14, ss: 2, dd: 300, sg: 60, hits: [{ s: 10, e: 26, hb: [-10, 6, 60, 24], d: 90, launch: [520, 640], air: [520, 640], lvl: 1, hsp: 11, bstun: 14 }], an: { k: [[0, 'jump'], [9, 'spS'], [40, 'spS']], spin: [6, 30, 1080] } });
    N('ATAT_OD', { n: 'Air Tatsumaki', sp: 1, air: 1, od: 1, su: 11, t: 999, landRec: 14, ss: 2, dd: 300, sg: 100, hits: [{ s: 10, e: 14, hb: [-10, 6, 60, 24], d: 30, stun: 18, air: [200, 300], lvl: 1, hsp: 7, bstun: 14 }, { s: 17, e: 21, hb: [-10, 6, 60, 24], d: 30, stun: 18, air: [200, 300], lvl: 1, hsp: 7, bstun: 14 }, { s: 24, e: 28, hb: [-10, 6, 60, 24], d: 30, launch: [620, 700], air: [620, 700], lvl: 2, hsp: 12, bstun: 14 }], an: { k: [[0, 'jump'], [9, 'spS'], [40, 'spS']], spin: [6, 34, 1440] } });
    // Dragonlash Kick
    const dlA = { k: (su) => [[0, 'idle'], [Math.max(2, su - 14), 'jump'], [su - 4, 'jhkW'], [su - 1, 'axS'], [su + 10, 'axS']] };
    N('DL_H', { n: 'Dragonlash Kick', sp: 1, su: 28, t: 999, ag: 108, rise: [19, 830, 1032], landRec: 16, ss: 2, dd: 300, sg: 80,
      hits: [{ s: 27, e: 31, hb: [0, -34, 62, 66], d: 120, stun: 29, kb: 300, air: [300, 520], lvl: 1, hsp: 13, bstun: 27 }],
      an: { k: [[0, 'idle'], [9, 'dlW'], [18, 'dlW'], [22, 'dlF'], [26, 'dlF'], [28, 'dlS'], [34, 'dlS'], [38, 'prejump'], [54, 'idle']] } });
    const dl2 = (k, st, bs, rec, o) => N(k, Object.assign({ n: 'Dragonlash Kick', sp: 1, su: 9, t: 999, ag: 108, rise: [8, 640, 972], landRec: rec, dd: 300, sg: 120, can: C_SU,
      hits: [{ s: 8, e: 12, hb: [0, 0, 56, 70], d: 50, stun: 22, kb: 60, np: 1, air: [120, 470], lvl: 1, hsp: 9, bstun: 18 }, Object.assign({ s: 15, e: 22, hb: [0, -34, 62, 70], d: 70, stun: st, kb: 300, air: [200, 820], lvl: 2, hsp: 13, bstun: bs }, o && o.thru ? { air: [40, -1300], gb: 1, xup: 470 } : {})],
      an: { k: [[0, 'dlW'], [8, 'dlF'], [13, 'dlF'], [15, 'dlS'], [24, 'dlS'], [28, 'prejump'], [48, 'idle']] } }, o || {}));
    dl2('DL_OD', 34, 24, 23, { od: 1 }); dl2('QDL', 34, 24, 22, { thru: 1 });
    // Jinrai Kick and its follow-ups
    const fu = { S4: 'KAZE', S2: 'GORAI', S5: 'SENKA', L: 'KAZE', M: 'GORAI', H: 'SENKA' }, fuO = { S4: 'KAZE_OD', S2: 'GORAI_OD', S5: 'SENKA_OD', L: 'KAZE_OD', M: 'GORAI_OD', H: 'SENKA_OD' }, ka = { S4: 'KASAI', S2: 'KASAI', S5: 'KASAI', L: 'KASAI', M: 'KASAI', H: 'KASAI' };
    const jb = [8, 40, 62, 22], jA = (su, t) => ({ k: [[0, 'idle'], [Math.max(2, su - 6), 'lkW'], [su - 1, 'hkS'], [su + 6, 'hkS'], [t, 'idle']] });
    N('JIN_L', { n: 'Jinrai Kick', sp: 1, su: 12, ac: 3, rc: 28, d: 50, ha: 1, ba: -11, hb: jb, kb: 300, lvl: 1, hsp: 11, dd: 200, sg: 55, can: C_SU, mv: [[3, 12, 420]], fu: { w: [31, 35], to: fu }, an: jA(12, 42) });
    N('JIN_M', { n: 'Jinrai Kick', sp: 1, su: 16, ac: 3, rc: 24, d: 60, ha: 2, ba: -7, hb: jb, kb: 300, lvl: 1, hsp: 11, dd: 200, sg: 55, can: C_SU, mv: [[5, 16, 420]], fu: { w: [31, 35], to: fu }, an: jA(16, 42) });
    N('JIN_H', { n: 'Jinrai Kick', sp: 1, su: 25, ac: 3, rc: 19, d: 70, ba: -2, hb: jb, launch: [260, 760], air: [260, 760], lvl: 1, hsp: 13, dd: 400, sg: 55, can: C_SU, mv: [[10, 25, 380]], fu: { w: [35, 41], to: fu }, an: jA(25, 46) });
    N('JIN_OD', { n: 'Jinrai Kick', sp: 1, od: 1, su: 13, ac: 2, rc: 25, d: 60, ha: -4, ba: -7, hb: jb, kb: 240, lvl: 1, hsp: 11, dd: 300, sg: 40, can: C_SU, mv: [[3, 13, 460]], fu: { w: [20, 27], to: fuO }, an: jA(13, 40) });
    N('KAZE', { n: 'Kazekama Shin Kick', sp: 1, su: 6, ac: 4, rc: 19, d: 60, g: 'l', ha: 3, ba: -5, hb: [6, 0, 62, 14], kb: 420, lvl: 1, hsp: 11, dd: 200, sg: 40, can: C_SU, mv: [[0, 5, 300]], crouch: 1, an: { w: 'clkW', s: 'cmkS' } });
    N('GORAI', { n: 'Gorai Axe Kick', sp: 1, su: 18, ac: 3, rc: 20, d: 100, g: 'h', ha: 3, ba: -3, hb: [8, 30, 52, 60], kb: 520, air: [150, -1400], gb: 1, lvl: 1, hsp: 13, dd: 250, sg: 50, can: C_SU, mv: [[0, 12, 260]], an: { w: 'axW', s: 'axS' } });
    N('SENKA', { n: 'Senka Snap Kick', sp: 1, su: 10, ac: 3, rc: 25, d: 80, ba: -3, hb: [8, 58, 66, 22], launch: [520, 620], air: [520, 620], lvl: 2, hsp: 13, dd: 700, sg: 60, can: C_SU, mv: [[0, 8, 300]], an: { w: 'hkW', s: 'hkS' } });
    N('KAZE_OD', { n: 'Kazekama Shin Kick', sp: 1, su: 6, ac: 3, rc: 20, d: 50, g: 'l', ha: 3, ba: -5, hb: [6, 0, 62, 14], kb: 300, lvl: 1, hsp: 11, dd: 300, sg: 40, can: C_SU, mv: [[0, 5, 300]], crouch: 1, fu: { w: [10, 27], to: ka }, tint: 1, an: { w: 'clkW', s: 'cmkS' } });
    N('GORAI_OD', { n: 'Gorai Axe Kick', sp: 1, su: 17, ac: 3, rc: 24, d: 100, g: 'h', ha: -3, ba: -7, hb: [8, 30, 52, 60], kb: 300, air: [150, -1400], gb: 1, lvl: 1, hsp: 13, dd: 250, sg: 50, can: C_SU, mv: [[0, 12, 260]], fu: { w: [23, 42], to: ka }, tint: 1, an: { w: 'axW', s: 'axS' } });
    N('SENKA_OD', { n: 'Senka Snap Kick', sp: 1, su: 10, ac: 6, rc: 18, d: 80, ba: -4, hb: [8, 50, 66, 30], launch: [200, 800], air: [200, 800], lvl: 1, hsp: 13, dd: 700, sg: 60, can: C_SU, mv: [[0, 8, 300]], fu: { w: [21, 32], to: ka }, tint: 1, an: { w: 'hkW', s: 'hkS' } });
    N('KASAI', { n: 'Kasai Thrust Kick', sp: 1, su: 15, ac: 3, rc: 29, d: 50, ba: -12, hb: [8, 30, 70, 40], launch: [240, 800], air: [240, 800], lvl: 2, hsp: 14, dd: 500, sg: 60, cx: 2, can: C_SU, mv: [[2, 14, 520]], tint: 1, an: { w: 'lkW', s: 'mkS' } });
    // Super Arts
    // Dragonlash Flame: launcher, then spinning kicks carried up into the air; the last kick sends them back over Ken's shoulder so he lands on the far side
    const ak = (st, d) => ({ s: st, e: st + 1, hb: [-10, -10, 80, 100], d: d, stun: 34, kb: 0, air: [0, 300], lvl: 1, hsp: 6, bstun: 20, req: 1, lk: 1, carry: [24, 4] });
    N('SA1', { n: 'Dragonlash Flame', sup: 1, sa: 1, su: 7, t: 999, frz: 40, inv: [0, 8], minScale: 30, dd: 250, mv: [[2, 8, 760]], stopOnHit: 1, wEnd: [9, -41], rise: [11, 330, 1120], landRec: 12,
      hits: [{ s: 6, e: 8, hb: [2, 10, 64, 80], d: 40, stun: 34, kb: 0, launch: [0, 700], air: [0, 300], lk: 1, lvl: 2, hsp: 14, bstun: 20 }, ak(15, 25), ak(20, 25), ak(25, 25), ak(30, 25), { s: 36, e: 38, hb: [-10, -10, 80, 100], d: 60, launch: [-620, 520], air: [-620, 520], lvl: 2, hsp: 18, req: 1, bstun: 20 }] });
    const big = [-4, 0, 84, 104], rq = (s, d) => ({ s: s, e: s + 1, hb: big, d: d, stun: 34, kb: 0, air: [0, 250], lvl: 1, hsp: 6, bstun: 20, req: 2, lk: 1 });
    // Shippu Jinrai-kyaku: kicks on the ground, then a rising Tatsumaki that carries them into the air
    const tk2 = (st, d) => ({ s: st, e: st + 1, hb: big, d: d, stun: 34, kb: 0, air: [0, 300], lvl: 1, hsp: 5, bstun: 20, req: 2, lk: 1, carry: [22, 2] });
    N('SA2', { n: 'Shippu Jinrai-kyaku', sup: 1, sa: 2, su: 6, t: 999, frz: 44, minScale: 40, mv: [[4, 14, 1300]], stopOnHit: 1, dd: 500, wEnd: [88, -1], rise: [38, 110, 1060], landRec: 14,
      hits: [{ s: 5, e: 14, hb: [0, 6, 64, 110], d: 30, stun: 34, kb: 0, air: [0, 300], lk: 1, pop: [40, 640], lvl: 1, hsp: 10, bstun: 33 }, rq(20, 30), rq(26, 30), rq(32, 30), tk2(40, 15), tk2(44, 15), tk2(48, 15), tk2(52, 15), { s: 57, e: 59, hb: [-10, -10, 90, 110], d: 100, launch: [900, 900], air: [900, 900], wb: 1, lvl: 2, hsp: 18, req: 2, bstun: 27 }] });
    const fl = s => ({ s: s, e: s + 1, hb: big, d: 60, stun: 34, kb: 0, air: [0, 260], lvl: 1, hsp: 7, bstun: 20, lk: 1 });
    N('SA3', { n: 'Shinryu Reppa', sup: 1, sa: 3, su: 7, t: 999, frz: 60, inv: [0, 17], minScale: 50, partAt: 10, rise: [41, 100, 1280], landRec: 19, dd: 900, mv: [[1, 6, 1100]], stopOnHit: 1,
      hits: [{ s: 6, e: 9, hb: [0, 6, 70, 110], d: 60, stun: 34, kb: 0, air: [0, 260], lvl: 1, hsp: 7, bstun: 20 }, fl(14), fl(22), fl(30), { s: 40, e: 44, hb: big, d: 160, launch: [150, 1450], air: [150, 1450], lvl: 2, hsp: 22, bstun: 20 }],
      an: { k: [[0, 'cupW'], [6, 'srS'], [11, 'cupW'], [14, 'srS'], [19, 'cupW'], [22, 'srS'], [27, 'cupW'], [30, 'srS'], [36, 'cupW'], [40, 'srS'], [56, 'srS2'], [80, 'jump']], spin: [40, 70, 1080] } });
    // Critical Art: the Level 3 at a quarter health or less. Same opening and the same frame data, but it carries them to the top of the screen and drives them back into the floor.
    const ck = st => ({ s: st, e: st + 1, hb: big, d: 15, stun: 34, kb: 0, air: [0, 300], lvl: 1, hsp: 6, bstun: 20, req: 1, lk: 1, carry: [20, 8] });
    N('CA', { n: 'Abyssal Tide', sup: 1, sa: 3, ca: 1, su: 7, t: 999, frz: 84, inv: [0, 17], minScale: 50, partAt: 10, rise: [41, 100, 1280], landRec: 19, dd: 900, mv: [[1, 6, 1100]], stopOnHit: 1,
      hits: [{ s: 6, e: 9, hb: [0, 6, 70, 110], d: 60, stun: 34, kb: 0, air: [0, 260], lvl: 1, hsp: 7, bstun: 20 }, fl(14), fl(22), fl(30), { s: 40, e: 44, hb: big, d: 100, launch: [150, 1450], air: [150, 1450], lvl: 2, hsp: 16, bstun: 20 },
        ck(49), ck(54), ck(59), { s: 65, e: 67, hb: [-20, -30, 96, 130], d: 65, stun: 34, air: [40, -2400], gb: 1, lvl: 2, hsp: 24, req: 1, fin: 1 }] });
    return {
      name: 'RET', jp: '裂', title: 'THE PALE TIDE', hp: 1000, walkF: 230, walkB: 157, jx: 245, jxB: 196, runV: 430, mv: mv,
      sp: [{ dir: 6, k: 'SRK_M', od: 'SRK_OD' }, { dir: 4, k: 'HOME', od: 'HOME_OD' }, { dir: 2, k: 'JIN_M', od: 'JIN_OD' }, { dir: 5, k: 'HAD_H', od: 'HAD_OD' }, { air: 1, adir: 2, k: 'ASPK', od: 'ASPK' }, { air: 1, adir: 8, k: 'AUP', od: 'AUP' }, { air: 1, adir: 4, k: 'ABK', od: 'ABK' }, { air: 1, fwd: 1, adir: 6, k: 'ATAT', od: 'ATAT_OD' }, { air: 1, k: 'AHOME', od: 'AHOME_OD' }],
      mo: [{ m: '623', L: 'SRK_L', M: 'SRK_M', H: 'SRK_H', OD: 'SRK_OD' }, { m: '214', L: 'TAT_L', M: 'TAT_M', H: 'TAT_H', OD: 'TAT_OD' }, { m: '236', L: 'JIN_L', M: 'JIN_M', H: 'JIN_H', OD: 'JIN_OD' }, { m: '214', air: 1, fwd: 1, L: 'ATAT', M: 'ATAT', H: 'ATAT', OD: 'ATAT_OD' }],
      run: { L: 'STOP', M: 'TK', H: 'FSK', S6: 'QSRK', S5: 'QSRK', S2: 'QTAT', S4: 'QDL', m623: 'QSRK', m214: 'QTAT' },
      auto: { L: [['5LK'], ['JIN_L'], ['KAZE']], LC: [['2LK'], ['5LP'], ['JIN_L'], ['KAZE']], M: [['5MP'], ['CB2'], ['RUN'], ['QDL'], ['SA2', 'SRK_H']], H: [['5HP'], ['JIN_OD', 'JIN_M'], ['KAZE_OD', 'KAZE'], ['KASAI'], ['SA3']] },
      cmd: [['Hadoken', 'SP', 'Heavy fireball. Assist+SP is the two-hit Overdrive.'], ['Shoryuken', '→ + SP   (623 + L/M/H)', 'Anti-air invincible. Only the Overdrive is fully invincible.'],
        ['Slipstream', 'Homing button   (or ← + SP, air: SP)', 'Homes in. Hit = free air combo. Blocked or missed = long helpless fall.'], ['Jinrai Kick', '↓ + SP   (236 + L/M/H)', 'Then ←SP low, ↓SP overhead, SP knockdown kick.'],
        ['Tatsumaki Senpu-kyaku', '214 + L/M/H   (air: → + SP)', 'Motion only on the ground. Air: forward jump, → + SP.'], ['Quick Dash', 'M + H', 'Run. L stops, M overhead, H step kick, SP specials (←SP Dragonlash).'],
        ['Super Arts', 'H + SP   ← H + SP   ↓ H + SP', 'Dragonlash Flame, Shippu Jinrai-kyaku, Shinryu Reppa.'], ['Critical Art', '↓ H + SP   at 25% health or less', 'Abyssal Tide replaces Level 3 when nearly dead. More damage, ends in a slam.']],
      cmd2: [['Air Normals', '↑ L/M/H   ↓ M/H   ← L/M/H', 'Rising Lash launches. Falling Heel spikes. Back Lash hits behind.'], ['Air Specials', 'Homing / SP   ↑ SP   ↓ SP   ← SP', 'Slipstream, Riptide, Falling Star (spike), Undertow (behind).'],
        ['Air Movement', '↑ again   ↑ at the wall   ← →', 'Double jump, wall jump, drift. Hold ↓ to fall fast.'], ['Air Cancels', 'after an air normal hits', '↑ jump-cancels it; SP cancels it into an air special.'],
        ['Juggle Loop', 'launch, Homing, air combo, ↓ + SP', 'Slipstream pops them up twice per combo; the spike bounces them once.'], ['Grab', 'L + M   then any attack button', 'Holds them. Pummel up to five times before throwing.'],
        ['Throws', '→   ←   ↑   ↓   while holding', 'Forward, back, straight up (juggle), slam into a ground bounce.'], ['Assist Combos', 'hold Assist, tap L / M / H', 'L x3, M x5 (ends in SA2), H x5 (ends in SA3, or the Critical Art).']]
    };
  }
  function ren() { return Object.assign(ken(), { name: 'ARD', jp: '闇', title: 'THE BLACK TIDE' }); }
  const CH = [ken(), ren()];

  // ------------------------------------------------------------ state
  function newFighter(ch, i) {
    const b = []; for (let k = 0; k < NBUF; k++) b.push(0);
    return { ch: ch, x: (i ? 80 : -80) * F, y: 0, vx: 0, vy: 0, pv: 0, face: i ? -1 : 1, hp: CH[ch].hp, sa: 0, dr: DMAX, bo: 0, drT: 0, st: 'idle', sub: 'stand', sf: 0, subf: 0,
      mv: '', mf: 0, hitMask: 0, contact: 0, hitOK: 0, hs: 0, stun: 0, buf: b, prev: 0, age: [99, 99, 99, 99, 99, 99, 99, 99], d66: 99, d44: 99, combo: 0, comboDmg: 0, jug: 0,
      wbUsed: 0, gbUsed: 0, wbPend: 0, gbPend: 0, jd: 0, rec: 0, cb: 0, hitHi: 1, tinv: 0, wins: 0, back: 0, red: CH[ch].hp, redT: 0, air2: 0, noTech: 0,
      ca: 0, rq: 99, part: 0, jg: 0, cxSeq: -1, fuq: '', fum: 0, pd: [0, 0, 0, 0, 0, 0, 0, 0], mseq: 0, cs: 0, lastSeq: -1, acq: 0, rcn: 0, sshift: 0, drs: 0, pok: 0, roll: 0, hard: 0, arm: 0, armHit: 0, drb: 0, mod: 0, ac: '', aci: 0, crum: 0, pp: 0, plock: 0, half: 0, hm: 0, hmc: 0, fr: 0, hmE: 0, dj: 0, wj: 0, aa: 0 };
  }
  function newMatch(cfg) {
    const s = { frame: 0, slow: 0, phase: 'intro', pt: 0, kx: 0, round: 1, time: 99 * 60, freeze: 0, frzWho: 0, proj: [], ev: [], winner: -1, rw: -1, koType: '', cfg: { training: !!cfg.training, chars: cfg.chars.slice(), rounds: cfg.rounds || 2 }, p: [] };
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
  const parryHeld = inp => (inp & 1024) !== 0;

  // ------------------------------------------------------------ drive gauge
  function burn(s, p, i) { if (p.dr <= 0 && !p.bo) { p.dr = 0; p.bo = 1; s.ev.push({ t: 'burnout', who: i }); } }
  function spend(s, p, i, cost, pause) { if (p.bo || p.dr <= 0) return false; p.dr -= cost; p.drT = Math.max(p.drT, pause || 120); if (p.dr <= 0) { p.dr = 0; burn(s, p, i); } return true; }

  // ------------------------------------------------------------ fighter logic
  function cur(p) { return p.st === 'atk' ? CH[p.ch].mv[p.mv] : null; }
  function toIdle(p) { p.st = 'idle'; p.sf = 0; p.mv = ''; p.vx = 0; p.sub = 'stand'; p.air2 = 0; p.arm = 0; }
  function start(s, p, key, i, opt) {
    if (key === 'SA3' && CH[p.ch].mv.CA && p.hp * 4 <= CH[p.ch].hp) key = 'CA';
    const m = CH[p.ch].mv[key], fromDr = p.st === 'drush', pm = p.st === 'atk' ? CH[p.ch].mv[p.mv] : null, o = s.p[1 - i];
    if (m.sup || (pm && pm.thru)) { if (o.x > p.x) p.face = 1; else if (o.x < p.x) p.face = -1; }
    p.st = 'atk'; p.mv = key; p.mseq++; p.mf = 0; p.sf = 0; p.hitMask = 0; p.contact = 0; p.hitOK = 0; p.arm = m.arm || 0; p.armHit = 0;
    p.drb = fromDr && !m.sp && !m.sup && !m.thr ? 1 : 0; if (fromDr) p.pv = p.face * 480;
    p.mod = opt && opt.mod ? 1 : 0; p.fuq = ''; p.part = 0; p.ca = m.ca ? 1 : 0; p.fr = 0; p.hmE = 0; if (m.home) p.hm = 1; if (!(opt && opt.auto)) { p.ac = ''; p.aci = 0; }
    if (!m.isAir) { p.vx = 0; if (p.y > 0) { p.y = 0; p.vy = 0; p.air2 = 0; p.jg = 0; } }
    if (m.sa) p.sa -= m.sa * 1000; else if (m.sp) p.sa = Math.min(3000, p.sa + 10);
    if (m.frz) { s.freeze = m.frz; s.frzWho = i; s.ev.push({ t: 'super', who: i, lv: m.sa, ca: p.ca, k: key }); }
    else if (m.od) s.ev.push({ t: 'ex', who: i });
    else if (m.di) s.ev.push({ t: 'di', who: i });
    if (m.sp || m.sup) s.ev.push({ t: 'special', who: i, k: key });
    p.age = [99, 99, 99, 99, 99, 99, 99, 99];
    return true;
  }
  function pick(b, h, v, as) {
    if (b === 'L') return v < 0 ? '2LK' : as ? '5LK' : '5LP';
    if (b === 'M') return v < 0 ? '2MK' : as ? '5MP' : '5MK';
    return v < 0 ? (h > 0 ? '2HK' : '2HP') : as ? '5HP' : '5HK';
  }
  function tryAttack(s, p, o, inp, i) {
    const C = CH[p.ch], A = p.age, air = p.y > 0;
    const lt = A[0] <= BUF, md = A[1] <= BUF, hv = A[2] <= BUF, spb = A[3] <= BUF, dib = A[5] <= BUF, hmb = A[7] <= BUF;
    const m = cur(p);
    if (!(lt || md || hv || spb || dib || hmb) && !(m && m.tc && p.contact)) return false;
    let canSp = true, canSu = true, chain = null, kara = false, canN = true, ok = false;
    if (m) {
      ok = p.contact && p.mf <= m.lastAct + 10;
      kara = !m.sp && !m.sup && !m.thr && !m.di && p.mf <= 2 && !p.contact;
      canSp = (ok && (m.can & C_SP)) || kara || (ok && m.isAir && !m.sp && p.hitOK);   // any air normal that hits cancels into an air special
      canSu = (ok && (m.can & C_SU)) || kara || (m.fc && p.mf >= m.fc[0] && p.mf <= m.fc[1] && (m.can & C_SU));
      chain = (ok && (m.can & C_CH)) ? m.chain : null; canN = false;
      if (m.sup || m.home) return false;
    }
    const bi = spb ? 3 : hv ? 2 : md ? 1 : 0, pin = (p.pd[bi] & 15) | (inp & 256) | (p.pd[bi] & 256);
    const h = hdir(pin, p.face), v = vdir(pin), as = (pin & 256) !== 0;
    const myProj = s.proj.some(q => q.o === i);
    if (m && ok && m.tc && p.rq > 12) { const k = (A[1] <= 12 && m.tc.M) || (A[0] <= 12 && m.tc.L) || (A[2] <= 12 && m.tc.H); if (k) return start(s, p, k, i); }
    if (canSu && !air) {
      let lv = 0, mod = 0;
      if (motionOK(p, 'SUP') && (lt || md || hv)) lv = hv ? 3 : md ? 2 : 1;
      else if (spb && (inp & 64) && A[2] <= BUF + 2) { lv = v < 0 ? 3 : h < 0 ? 2 : 1; mod = 1; }
      else if (hv && (inp & 128) && A[3] <= BUF + 2) { lv = v < 0 ? 3 : h < 0 ? 2 : 1; mod = 1; }
      if (lv && p.sa >= lv * 1000) return start(s, p, 'SA' + lv, i, { mod: mod });
      if (lv && mod) return false;
    }
    if (dib && !air && (canN || kara) && !p.bo && p.dr > 0) { spend(s, p, i, BAR, 120); return start(s, p, 'DI', i); }
    // the homing button: Slipstream from anywhere, whatever direction is held. Assist makes it the Overdrive.
    if (hmb && !p.hm && canSp && !(m && m.sp && !kara)) {
      const od = (inp & 256) && !p.bo && p.dr > 0; if (od) spend(s, p, i, 2 * BAR, 120);
      return start(s, p, (air ? 'AHOME' : 'HOME') + (od ? '_OD' : ''), i, { mod: 1 });
    }
    if (canSp && !(m && m.sp && !kara)) {
      const nAtk = ((inp & 16) ? 1 : 0) + ((inp & 32) ? 1 : 0) + ((inp & 64) ? 1 : 0), canOD = !p.bo && p.dr > 0;
      if (lt || md || hv) for (const sp of C.mo) {
        if (!!sp.air !== air || (sp.fwd && p.jd <= 0) || !motionOK(p, sp.m)) continue;
        if (nAtk >= 2 && canOD) { spend(s, p, i, 2 * BAR, 120); return start(s, p, sp.OD, i); }
        return start(s, p, hv ? sp.H : md ? sp.M : sp.L, i);
      }
      if (spb) {
        const d = v < 0 ? 2 : (air && v > 0) ? 8 : h > 0 ? 6 : h < 0 ? 4 : 5;
        for (const sp of C.sp) {
          if (!!sp.air !== air || (!air && sp.dir !== d) || (air && sp.adir && sp.adir !== d) || (sp.fwd && p.jd <= 0) || (C.mv[sp.k].home && p.hm)) continue;
          if (as && canOD && !(C.mv[sp.od].proj && myProj)) { spend(s, p, i, 2 * BAR, 120); return start(s, p, sp.od, i, { mod: 1 }); }
          if (C.mv[sp.k].proj && myProj) continue;
          return start(s, p, sp.k, i, { mod: 1 });
        }
      }
    }
    if (!(lt || md || hv)) return false;
    if (air) { if (m && !(m.isAir && p.contact && (p.aa || 0) < 3)) return false; if (m) p.aa = (p.aa || 0) + 1;
      const vv = vdir(inp), hh = hdir(inp, p.face), k = vv > 0 && (m || p.sf > 9) ? 'jUP' : vv < 0 && (md || hv) ? 'jDN' : hh < 0 ? 'jBK' : as ? (hv ? 'jHP' : md ? 'jMK' : 'jLK') : (hv ? (p.jd === 0 ? 'njHK' : 'jHK') : md ? 'jMP' : 'jLP'); return start(s, p, k, i); }
    if ((canN || kara) && (inp & 16) && (inp & 32) && A[0] <= BUF + 2 && A[1] <= BUF + 2) { p.back = h < 0 ? 1 : 0; return start(s, p, 'throw', i); }
    const b = hv ? 'H' : md ? 'M' : 'L', nb = hv ? 2 : md ? 1 : 0, hN = hdir(p.pd[nb], p.face), vN = vdir(p.pd[nb]), k = pick(b, hN, vN, as);
    if (canN) { if (as) { p.ac = b; p.aci = 0; return start(s, p, k, i, { auto: 1 }); } return start(s, p, k, i); }
    if (chain && b === 'L') { const k2 = chain[vN < 0 ? 2 : 5]; if (k2) return start(s, p, k2, i); }
    return false;
  }
  // Assist combo: one buffered press advances to the next step as soon as that step is legal.
  function autoStep(s, p, o, i) {
    const C = CH[p.ch], step = C.auto[p.ac] && C.auto[p.ac][p.aci + 1], m = cur(p), inRun = p.st === 'run';
    if (!step || (!m && !inRun)) { p.acq = 0; return false; }
    for (const k of step) {
      if ((k === 'RUN' || k === 'KASAI') && !p.hitOK) { if (!m || p.mf > m.lastAct + 12) p.acq = 0; return false; }
      if (k === 'RUN') { if (m && p.contact && p.mf <= m.lastAct + 12) { p.aci++; p.acq = 0; enterRun(s, p, i, true); return true; } if (m && p.mf > m.lastAct + 12) p.acq = 0; return false; }
      const mm = C.mv[k];
      if (mm.sa && p.sa < mm.sa * 1000) continue;
      if (mm.od && (p.bo || p.dr <= 0)) continue;
      let legal;
      if (inRun) legal = p.sf >= 11;
      else if (m.fu) { let has = false; for (const kk in m.fu.to) if (m.fu.to[kk] === k) has = true; if (!has) continue; if (p.mf > m.fu.w[1]) { p.acq = 0; return false; } legal = p.mf >= m.fu.w[0]; }
      else {
        if (!((mm.sup && (m.can & C_SU)) || (mm.sp && !m.sp && !mm.sup) || (m.tc && (m.tc.M === k || m.tc.L === k || m.tc.H === k)) || (m.chain && !mm.sp && !mm.sup) || (k === 'SRK_H' && p.mv === 'QDL'))) continue;
        if (p.mf > m.lastAct + 12 && !(m.thru && p.y > 0)) { p.acq = 0; return false; }
        legal = p.contact && (!(mm.sup || m.sp) || m.hits.length < 2 || p.mf > m.hits[m.hits.length - 1].s) && !(m.thru && p.y > 14 * F);
      }
      if (!legal) return false;
      if (mm.od) spend(s, p, i, 2 * BAR, 120);
      p.aci++; p.acq = 0; start(s, p, k, i, { auto: 1 }); return true;
    }
    p.acq = 0; return false;
  }
  function autoPress(p, inp) {
    if (!(inp & 256)) return;
    if (!p.ac) { if (p.st === 'atk' && p.contact && p.age[0] <= BUF && (p.mv === '5LP' || p.mv === '2LK')) { p.ac = 'LC'; p.aci = -1; p.age[0] = 99; p.acq = 1; } return; }
    const b = p.ac === 'L' || p.ac === 'LC' ? 0 : p.ac === 'M' ? 1 : 2;
    if (p.age[b] <= BUF) { p.age[b] = 99; p.acq = 1; }
  }
  function fuKey(p, m, inp) {
    const h = hdir(p.pd[3], p.face), v = vdir(p.pd[3]), to = m.fu.to;
    if (p.age[3] <= BUF) return { k: to[v < 0 ? 'S2' : h < 0 ? 'S4' : 'S5'], mod: 1 };
    for (let b = 0; b < 3; b++) if (p.age[b] <= BUF && hdir(p.pd[b], p.face) > 0) return { k: b === 0 ? to.L : b === 1 ? to.M : to.H, mod: 0 };
    return null;
  }
  function runHeld(p, inp) { return p.rq <= BUF; }
  function enterParry(s, p, i) { if (!spend(s, p, i, BAR / 2, 240)) return false; p.st = 'parry'; p.sf = 0; p.mv = ''; p.vx = 0; p.plock = 0; p.pok = 0; p.arm = 0; s.ev.push({ t: 'parryOn', who: i }); return true; }
  function enterRush(s, p, o, i, fromCancel) { p.st = 'drush'; p.sf = 0; p.rcn = fromCancel ? 1 : 0; p.mv = ''; p.vx = 0; p.d66 = 99; p.arm = 0; p.drT = fromCancel ? 120 : 240; if (fromCancel && o.combo > 0) o.drs = 1; s.freeze = fromCancel ? 9 : 10; s.frzWho = i; s.ev.push({ t: 'rush', who: i }); }
  function enterRun(s, p, i, keepAuto) { p.st = 'run'; p.rq = 99; p.sf = 0; p.mv = ''; p.arm = 0; p.age = [99, 99, 99, 99, 99, 99, 99, 99]; if (!keepAuto) { p.ac = ''; p.aci = 0; p.acq = 0; } s.ev.push({ t: 'run', who: i }); }
  function neutral(s, p, o, inp, i) {
    p.st = 'idle'; p.mv = '';
    if (o.x > p.x) p.face = 1; else if (o.x < p.x) p.face = -1;
    if (runHeld(p, inp)) { enterRun(s, p, i); return; }
    if (tryAttack(s, p, o, inp, i)) return;
    const C = CH[p.ch], h = hdir(inp, p.face), v = vdir(inp);
    if (parryHeld(inp) && enterParry(s, p, i)) return;
    if (p.d66 <= BUF) { p.d66 = 99; p.st = 'dashf'; p.sf = 0; s.ev.push({ t: 'dash', who: i }); return; }
    if (p.d44 <= BUF) { p.d44 = 99; p.st = 'dashb'; p.sf = 0; s.ev.push({ t: 'dash', who: i }); return; }
    let sub;
    if (v > 0) { p.st = 'prejump'; p.sf = 0; p.jd = h; p.vx = 0; return; }
    if (v < 0) { sub = 'crouch'; p.vx = 0; }
    else if (h > 0) { sub = 'walkf'; p.vx = p.face * C.walkF; }
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
    if (m.home && f >= m.home[0] && !p.contact && !p.hmE) {
      if (f === m.home[0]) { if (p.y <= 0) p.y = 1; p.air2 = 1; p.jg = 0; s.ev.push({ t: 'home', who: i, x: p.x, y: p.y, od: m.od ? 1 : 0 }); }
      if (f <= m.home[1]) {
        const dx = o.x - p.x, dy = (o.y > 0 ? o.y : 14 * F) - p.y, len = tr(Math.sqrt(dx * dx + dy * dy)), sp = m.home[2];
        if (dx > 20 * F) p.face = 1; else if (dx < -20 * F) p.face = -1;
        if (len > sp) { p.vx = tr(dx * sp / len); p.vy = tr(dy * sp / len) + GRAV; }
        else { p.hmE = 1; p.vx = tr(p.vx / 3); }   // arrived and nothing to hit (they were invulnerable): the flight is over
      } else { p.hmE = 1; p.vx = tr(p.vx / 3); }
    }
    if (m.rise && f === m.rise[0] && !(m.wEnd && !p.hitOK)) { p.jg = 0; p.vx = p.face * m.rise[1]; p.vy = m.rise[2]; if (p.y <= 0) p.y = 1; p.air2 = 1; s.ev.push({ t: 'rise', who: i }); }
    if (m.proj && f === m.proj.f) {
      s.proj.push({ o: i, ch: p.ch, k: p.mv, x: p.x + p.face * 30 * F, y: p.y + m.proj.y * F, dir: p.face, left: m.proj.hits, cd: 0, pause: 0, life: 0, mod: p.mod });
      s.ev.push({ t: 'fire', who: i, big: m.proj.big ? 1 : 0 });
    }
    if (m.wEnd && f === m.wEnd[0] && !p.hitOK) { if (m.wEnd[1] < 0) { p.st = 'land'; p.sf = 0; p.rec = -m.wEnd[1]; p.mv = ''; p.vx = 0; return; } p.mf = m.wEnd[1]; }
    if (m.arm && f > m.lastAct) p.arm = 0;
    if (m.partAt && f === m.partAt && !p.hitOK) p.part = 1;
    for (const h of m.hits) if (f === h.s && !(h.req && !p.hitOK)) s.ev.push({ t: 'whiff', who: i, lvl: h.lvl });
    if (m.thr && f === 4) s.ev.push({ t: 'whiff', who: i, lvl: 0 });
  }
  function update(s, p, o, inp, i) {
    const C = CH[p.ch];
    p.sf++; if (p.tinv > 0) p.tinv--; if (p.pp > 0) p.pp--;
    switch (p.st) {
      case 'idle': neutral(s, p, o, inp, i); break;
      case 'prejump':
        if (p.sf >= 4) { p.st = 'jump'; p.sf = 0; p.dj = 0; p.wj = 0; p.aa = 0; p.jg = 1; p.vy = 1125; p.vx = p.jd * p.face * (p.jd < 0 ? C.jxB : C.jx); p.y = 1; s.ev.push({ t: 'jump', who: i }); }
        break;
      case 'jump': {
        // steer in the air; Up again for one weak second jump; Up against the wall to kick off it
        const pv = p.buf[p.buf.length - 2] || 0, upNew = (inp & 1) && !(pv & 1), hx = (inp & 8 ? 1 : 0) - (inp & 4 ? 1 : 0), lim = STAGE - PW;
        if ((inp & 2) && p.vy < 0 && p.vy > -1700) p.vy -= 55;                                          // hold down on the way down to fall faster
        if (hx) { p.vx += hx * 22; const mx = tr(C.jx * 11 / 10); if (p.vx > mx) p.vx = mx; if (p.vx < -mx) p.vx = -mx; }
        if (upNew && p.sf > 5 && !(inp & 240)) {
          const wall = p.x >= lim - 2 * F ? 1 : p.x <= -lim + 2 * F ? -1 : 0;
          if (wall && !p.wj && p.y > 10 * F) { p.wj = 1; p.vx = -wall * 330; p.vy = 1000; p.jg = 1; p.sf = 0; p.jd = -wall * p.face; s.ev.push({ t: 'walljump', who: i, x: p.x + wall * PW, y: p.y }); }
          else if (!p.dj) { p.dj = 1; p.vy = 800; p.vx = hx * tr(C.jx * 8 / 10); p.jg = 1; p.sf = 0; if (hx) p.jd = hx * p.face; s.ev.push({ t: 'jump', who: i, dj: 1 }); }
        }
        tryAttack(s, p, o, inp, i); break;
      }
      case 'land': p.vx = 0; if (p.sf >= p.rec) { toIdle(p); neutral(s, p, o, inp, i); } break;
      case 'dashf': p.vx = p.face * (p.sf <= 12 ? 490 : 85); if (p.sf >= 19) { toIdle(p); neutral(s, p, o, inp, i); } break;
      case 'dashb': p.vx = -p.face * (p.sf <= 14 ? 290 : 51); if (p.sf >= 23) { toIdle(p); neutral(s, p, o, inp, i); } break;
      case 'drush': {
        // builds speed out of the freeze, holds it, then bleeds off
        const f = p.sf; p.vx = p.face * (f <= 9 ? 250 + f * 35 : f <= 24 ? 557 : Math.max(0, 557 - tr((f - 24) * 53 / 2)));
        if (f >= (p.rcn ? 10 : 9) && tryAttack(s, p, o, inp, i)) { atkFrame(s, p, o, C.mv[p.mv], i); break; }
        if (f >= (p.rcn ? 25 : 24) && (hdir(inp, p.face) < 0 || vdir(inp) !== 0 || parryHeld(inp) || runHeld(p, inp))) { p.pv = tr(p.vx / 2); toIdle(p); neutral(s, p, o, inp, i); break; }
        if (f >= (p.rcn ? 46 : 45)) { p.pv = tr(p.vx / 2); toIdle(p); neutral(s, p, o, inp, i); }
        break;
      }
      case 'run': {
        const R = C.run, f = p.sf; p.vx = p.face * tr(C.runV * (f <= 8 ? 30 + f * 9 : f > 35 ? Math.max(20, 100 - (f - 35) * 8) : 100) / 100);
        autoPress(p, inp); if (p.acq && autoStep(s, p, o, i)) { atkFrame(s, p, o, C.mv[p.mv], i); break; }
        let k = '';
        if (f >= 9 && p.age[2] <= BUF) k = R.H;
        else if (f >= 11) {
          if (p.age[3] <= BUF) { const h = hdir(p.pd[3], p.face), v = vdir(p.pd[3]); k = R[v < 0 ? 'S2' : h < 0 ? 'S4' : h > 0 ? 'S6' : 'S5']; if (k) { start(s, p, k, i, { mod: 1 }); atkFrame(s, p, o, C.mv[k], i); break; } }
          else if (p.age[7] <= BUF && !p.hm) { const od = (inp & 256) && !p.bo && p.dr > 0; if (od) spend(s, p, i, 2 * BAR, 120); k = od ? 'HOME_OD' : 'HOME'; start(s, p, k, i, { mod: 1 }); atkFrame(s, p, o, C.mv[k], i); break; }
          else if (p.age[0] <= BUF || p.age[1] <= BUF) { if (motionOK(p, '623')) k = R.m623; else if (motionOK(p, '214')) k = R.m214; else k = p.age[1] <= BUF ? R.M : R.L; }
        }
        if (k === 'STOP') { p.st = 'land'; p.sf = 0; p.rec = 16; p.vx = 0; p.pv = p.face * 180; p.age = [99, 99, 99, 99, 99, 99, 99, 99]; break; }
        if (k) { start(s, p, k, i); atkFrame(s, p, o, C.mv[k], i); break; }
        if (f >= 45) { toIdle(p); neutral(s, p, o, inp, i); }
        break;
      }
      case 'parry':
        p.vx = 0; p.dr -= 9; if (p.drT < 20) p.drT = 20;
        if (p.dr <= 0) { p.dr = 0; burn(s, p, i); p.st = 'prec'; p.sf = 0; break; }
        if (p.plock > 0) { p.plock--; break; }
        if (p.sf >= 4 && p.d66 <= BUF && p.dr > 0) { spend(s, p, i, BAR / 2, 240); enterRush(s, p, o, i, false); break; }
        if (!parryHeld(inp) && p.sf >= 12) { if (p.pok) { toIdle(p); neutral(s, p, o, inp, i); } else { p.st = 'prec'; p.sf = 0; } }
        break;
      case 'prec': if (p.sf >= 33) { toIdle(p); neutral(s, p, o, inp, i); } break;
      case 'atk': {
        if (p.fr) { if (p.fr === 1) { p.vy = 900; p.vx = p.face * 30; p.dj = 0; s.ev.push({ t: 'jump', who: i, dj: 1 }); } p.fr = 0; p.st = 'jump'; p.mv = ''; p.sf = 10; p.jg = 1; p.jd = 1; p.air2 = 0; p.aa = 0; tryAttack(s, p, o, inp, i); break; }
        p.mf++; let m = C.mv[p.mv];
        if (m.isAir && p.y > 0) { const pv = p.buf[p.buf.length - 2] || 0, hx = (inp & 8 ? 1 : 0) - (inp & 4 ? 1 : 0);
          if (!m.sp && hx) { p.vx += hx * 16; const mx = tr(C.jx * 11 / 10); if (p.vx > mx) p.vx = mx; if (p.vx < -mx) p.vx = -mx; }
          if ((inp & 1) && !(pv & 1) && p.contact && !p.dj && !m.home) { p.dj = 1; p.vy = 800; p.vx = hx * tr(C.jx * 8 / 10); p.jg = 1; p.st = 'jump'; p.mv = ''; p.sf = 0; s.ev.push({ t: 'jump', who: i, dj: 1 }); break; } }
        if (m.t < 900 && p.mf >= m.t + (m.wr && !p.contact ? m.wr : 0)) { toIdle(p); neutral(s, p, o, inp, i); break; }
        autoPress(p, inp); if (p.acq && autoStep(s, p, o, i)) { if (p.st === 'atk') atkFrame(s, p, o, C.mv[p.mv], i); break; }
        if (m.fu && p.mf <= m.fu.w[1]) { if (!p.fuq && p.mf >= m.su - 1) { const r = fuKey(p, m, inp); if (r && r.k) { p.fuq = r.k; p.fum = r.mod; p.age = [99, 99, 99, 99, 99, 99, 99, 99]; } } if (p.fuq && p.mf >= m.fu.w[0]) { const k = p.fuq; start(s, p, k, i, { mod: p.fum, auto: 1 }); atkFrame(s, p, o, C.mv[k], i); break; } }
        const live = p.contact && p.mf <= m.lastAct + 10, kara = !m.sp && !m.sup && !m.thr && !m.di && p.mf <= 2 && !p.contact, ground = p.y <= 0;
        if (ground && ((live && (m.can & C_SP) && !m.sp) || kara) && runHeld(p, inp)) { enterRun(s, p, i); break; }
        if (live && (m.can & C_SP) && !m.sp && ground && !p.bo && p.dr > 0 && (p.d66 <= BUF || p.age[6] <= BUF)) { spend(s, p, i, 3 * BAR, 120); p.age = [99, 99, 99, 99, 99, 99, 99, 99]; enterRush(s, p, o, i, true); break; }
        if (kara && ground && parryHeld(inp) && enterParry(s, p, i)) break;
        if (!tryAttack(s, p, o, inp, i)) atkFrame(s, p, o, m, i); else atkFrame(s, p, o, C.mv[p.mv], i);
        break;
      }
      case 'block':
        if (p.age[5] <= BUF && hdir(p.pd[5], p.face) > 0 && !p.bo && p.dr > 0 && p.y <= 0) { spend(s, p, i, 2 * BAR, 120); p.combo = 0; start(s, p, 'DREV', i); s.ev.push({ t: 'ex', who: i }); atkFrame(s, p, o, C.mv.DREV, i); break; }
        p.stun--; if (p.stun <= 0) { p.combo = 0; p.tinv = 2; toIdle(p); neutral(s, p, o, inp, i); } break;
      case 'hit':
        p.stun--; if (p.stun <= 0) { if (p.crum) { p.crum = 0; p.st = 'kd'; p.sf = 0; p.vx = 0; s.ev.push({ t: 'kd', x: p.x, y: 0, who: i }); } else { p.combo = 0; p.half = 0; p.tinv = 2; toIdle(p); neutral(s, p, o, inp, i); } } break;
      case 'kd': p.vx = p.roll && p.sf > 8 ? -p.face * 420 : 0; if (p.sf >= KD && p.hp > 0) { p.st = 'getup'; p.sf = 0; p.combo = 0; p.half = 0; } break;
      case 'getup': p.vx = p.roll && p.sf < 10 ? -p.face * 300 : 0; if (p.sf >= GETUP) { p.tinv = 1; toIdle(p); neutral(s, p, o, inp, i); } break;
      case 'tech': if (p.sf >= 24) { toIdle(p); neutral(s, p, o, inp, i); } break;
      case 'throwing': {
        const pv = p.buf[p.buf.length - 2] || 0, np = inp & ~pv, k = o.noTech ? 17 : 10;
        if (!p.tk) {
          if (o.st === 'thrown') { o.x = p.x + p.face * 30 * F; o.y = 0; o.vx = 0; o.vy = 0; }
          if (p.sf < 9 && o.st === 'thrown' && (o.prev & 16) && (o.prev & 32) && o.age[0] <= 10 && o.age[1] <= 10 && !o.noTech) {
            p.st = 'tech'; o.st = 'tech'; p.sf = 0; o.sf = 0; o.y = 0; p.pv = -p.face * 620; o.pv = p.face * 620; s.ev.push({ t: 'tech', x: (p.x + o.x) >> 1, y: 60 * F }); break;
          }
          if (p.pmT > 0) p.pmT--;
          if (p.sf >= 9 && p.pmT <= 0) {
            const fw = inp & (p.face > 0 ? 8 : 4), bw = inp & (p.face > 0 ? 4 : 8);
            // any attack button pummels (up to five); a direction, or running out of time, throws
            if ((p.age[0] <= 6 || p.age[1] <= 6 || p.age[2] <= 6 || p.age[3] <= 6) && p.pmN < 5 && p.sf < 70) { p.age[0] = p.age[1] = p.age[2] = p.age[3] = 99; p.pmN++; p.pmT = 9; const d = tr(8 * k / 10); o.hp = Math.max(1, o.hp - d); o.redT = 50; o.hs = 3; p.hs = 3; s.ev.push({ t: 'hit', x: o.x, y: 56 * F, lvl: 0, who: i, dmg: d, combo: p.pmN, total: d * p.pmN, thr: 1, ch: 0 }); }
            else if (p.sf >= 12 && (fw || bw || (inp & 3) || p.sf >= 54)) { p.tk = bw ? 2 : fw ? 1 : (inp & 1) ? 3 : (inp & 2) ? 4 : 1; p.tf = 0; p.back = p.tk === 2 ? 1 : 0; }
          }
        } else {
          p.tf++; const side = p.tk === 2 && p.tf >= 5 ? -1 : 1;
          if (o.st === 'thrown') { o.x = p.x + p.face * side * 30 * F; o.y = p.tk === 4 ? 0 : Math.min(p.tf, 10) * 3 * F; o.vx = 0; o.vy = 0; }
          if (p.tf === 10 && o.st === 'thrown') {
            const d = tr(120 * k / 10), V = [0, [430, 720], [430, 720], [40, 1480], [150, 1080]][p.tk];
            o.hp = Math.max(s.cfg.training ? 1 : 0, o.hp - d); o.st = 'ahit'; o.sf = 0; o.y = p.tk === 4 ? 1 : 30 * F; o.vx = p.face * side * V[0]; o.vy = V[1]; o.combo = 1; o.comboDmg = d; o.jug = p.tk >= 3 ? 0 : 3; o.hard = p.tk >= 3 ? 0 : 1; o.hs = 8; p.hs = 8; o.redT = 50; o.gbUsed = 0; o.wbUsed = 0;
            p.sa = Math.min(3000, p.sa + 200); o.sa = Math.min(3000, o.sa + 40); if (!p.bo) p.dr = Math.min(DMAX, p.dr + 200); if (o.noTech && !o.bo) { o.dr -= 1000; burn(s, o, 1 - i); }
            s.ev.push({ t: 'hit', x: o.x, y: 50 * F, lvl: 2, who: i, dmg: d, combo: 1, total: d, thr: 1, ch: o.noTech ? 2 : 0 }); if (p.tk === 4) s.ev.push({ t: 'gbounce', x: o.x, y: 0, who: 1 - i });
          }
          if (p.tf >= (p.tk >= 3 ? 20 : 30)) { p.tk = 0; toIdle(p); }
        }
        break;
      }
      case 'ahit': { const hx = (inp & 8 ? 1 : 0) - (inp & 4 ? 1 : 0); if (hx && p.y > 0) { p.vx += hx * 9; if (p.vx > 560) p.vx = 560; if (p.vx < -560) p.vx = -560; } break; }
      default: break;
    }
  }
  function land(s, p, i) {
    p.dj = 0; p.wj = 0; p.aa = 0; p.hm = 0; p.fr = 0;
    p.y = 0; p.vy = 0; p.air2 = 0; p.jg = 0;
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
      const ag = p.st === 'atk' ? CH[p.ch].mv[p.mv].ag : 0;
      p.vy -= (ag ? ag : p.jg && p.st !== 'ahit' ? JG : GRAV) + (p.st === 'ahit' ? p.jug * 3 : 0);
      p.x += p.vx; p.y += p.vy;
      if (p.y <= 0) land(s, p, i);
    } else if (p.st === 'ahit') { land(s, p, i);
    } else { p.x += p.vx + p.pv; p.pv = tr(p.pv * 86 / 100); if (p.pv > -12 && p.pv < 12) p.pv = 0; }
    if (p.x > lim || p.x < -lim) {
      const sg = p.x > 0 ? 1 : -1; p.x = sg * lim;
      if (p.st === 'ahit' && p.wbPend && !p.wbUsed && p.vx * sg >= 260 && p.hp > 0) {
        p.wbUsed = 1; p.wbPend = 0; p.vx = -sg * 260; p.vy = 820; p.hs = 9; s.ev.push({ t: 'wbounce', x: p.x + sg * PW, y: p.y + 45 * F, who: i });
      } else if (p.y <= 0 && p.pv * sg > 0 && (p.st === 'hit' || p.st === 'block') && o.y <= 0) { o.pv = -p.pv; p.pv = 0; }
    }
  }
  function pushboxes(s) {
    const a = s.p[0], b = s.p[1], lim = STAGE - PW;
    const thru = (p, q) => p.st === 'atk' && ((CH[p.ch].mv[p.mv].thru && q.st === 'ahit') || (CH[p.ch].mv[p.mv].home && p.y > 0 && (q.st === 'kd' || q.st === 'getup')));
    const skip = a.st === 'thrown' || b.st === 'thrown' || a.st === 'throwing' || b.st === 'throwing' || thru(a, b) || thru(b, a);
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
    if (p.st === 'kd' && p.hp <= 0) return false;   // a knocked-out body can still be hit
    if (p.st === 'kd' || p.st === 'getup' || p.st === 'thrown' || p.st === 'throwing' || p.st === 'win') return true;
    if (p.st === 'atk') { const m = CH[p.ch].mv[p.mv]; if (m.inv && p.mf >= m.inv[0] && p.mf <= m.inv[1]) return true; }
    return false;
  }
  function hurt(p) {
    if (invul(p)) return [];
    const r = [];
    if (p.st === 'kd') r.push([p.x - 38 * F, 0, p.x + 38 * F, 40 * F]);
    else if (p.y > 0) r.push([p.x - 16 * F, p.y + 4 * F, p.x + 16 * F, p.y + 72 * F]);
    else if (crouching(p)) r.push([p.x - 20 * F, 0, p.x + 20 * F, 64 * F]);
    else r.push([p.x - 19 * F, 0, p.x + 19 * F, 89 * F]);
    if (p.st === 'atk') { const m = CH[p.ch].mv[p.mv]; if (!m.sp && !m.sup) for (const h of m.hits) if (p.mf >= h.s && p.mf <= h.e + 3) r.push(box(p, h.hb)); }
    return r;
  }
  function box(p, hb) {
    const x0 = p.x + p.face * hb[0] * F, x1 = p.x + p.face * (hb[0] + hb[2]) * F;
    return [Math.min(x0, x1), p.y + hb[1] * F, Math.max(x0, x1), p.y + (hb[1] + hb[3]) * F];
  }
  function hitboxes(p) {
    const r = []; if (p.st !== 'atk') return r; const m = CH[p.ch].mv[p.mv];
    for (const h of m.hits) if (p.mf >= h.s && p.mf <= h.e && !(p.hitMask & (1 << h.grp)) && !(h.req && !(h.req === 2 ? p.contact : p.hitOK))) r.push({ h: h, b: box(p, h.hb) });
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
    if (h.free && !isProj && how !== 'hit') { a.vx = -a.face * 110; a.vy = 520; }
    if (how === 'parry') {
      const bk = (dinp & (side > 0 ? 8 : 4)) !== 0, cr = (dinp & 2) !== 0;
      const perfect = d.sf <= 2 && d.plock === 0 && (isProj || (bk && !(h.g === 'l' && !cr) && !(h.g === 'h' && cr && !m.isAir)));
      d.dr = Math.min(DMAX, d.dr + (isProj ? 150 : 350)); d.drT = 20; d.pok = 1; d.plock = Math.max(1, h.bstun - 3); d.hs = 8; if (!isProj) { a.hs = 8; a.contact = 1; }
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
      if (!d.bo) { d.dr -= h.dd != null ? h.dd : tr(h.d * 4); d.drT = Math.max(d.drT, 90); burn(s, d, di); }
      d.st = 'block'; d.sf = 0; d.stun = h.bstun + drb + (d.bo ? 4 : 0); d.cb = (dinp & 2) ? 1 : 0; d.pv = (h.lk || h.np) ? 0 : side * Math.max(h.kb, 260); d.vx = 0; d.redT = 40;
      hsp = Math.max(4, hsp - 2); d.hs = hsp; if (!isProj) { a.hs = hsp; a.contact = 1; }
      a.sa = Math.min(3000, a.sa + tr(h.d * 3 / 10)); d.sa = Math.min(3000, d.sa + tr(h.d / 5));
      s.ev.push({ t: 'block', x: cx, y: cy, lvl: h.lvl, who: ai });
      if (m.di && Math.abs(d.x) >= STAGE - PW - 50 * F) { d.st = 'hit'; d.crum = 2; d.stun = d.bo ? 120 : 72; d.pv = 0; d.combo = 1; d.comboDmg = 0; d.jug = 0; d.wbUsed = 0; d.gbUsed = 0; d.cb = 0; s.ev.push({ t: 'splat', x: d.x + side * PW, y: 50 * F, who: ai, stun: d.bo ? 1 : 0 }); }
      if (d.hp <= 0) { d.st = 'ahit'; d.y = 1; d.vx = side * 500; d.vy = 800; }
      return;
    }
    const inCombo = d.combo > 0 && (d.st === 'hit' || d.st === 'ahit');
    if (inCombo) d.combo++; else { d.combo = 1; d.comboDmg = 0; d.jug = 0; d.wbUsed = 0; d.gbUsed = 0; d.hmc = 0; d.half = d.pp > 0 ? 1 : 0; }
    const seq = isProj ? -2 - ai : a.mseq; if (!inCombo) { d.sshift = m.ss || 0; d.drs = 0; d.cs = 1; d.lastSeq = seq; } else if (seq !== d.lastSeq) { d.cs++; d.lastSeq = seq; } if (drb) d.drs = 1; if (m.cx === 1 && seq !== d.cxSeq) { d.cxSeq = seq; d.sshift++; } let pct = SCALE[Math.min(d.cs - 1 + (d.cs > 1 ? d.sshift : 0), 9)]; if (d.drs && d.cs > 1) pct = tr(pct * 85 / 100); if (m.cx === 2 && seq !== d.cxSeq) { d.cxSeq = seq; d.sshift++; } if (m.minScale && pct < m.minScale) pct = m.minScale;
    let dmg = tr(h.d * pct / 100); if (kind) dmg = tr(dmg * 120 / 100); if (mod) dmg = tr(dmg * 80 / 100); if (a.part && m.sup && !isProj) dmg = tr(dmg * 6 / 10); if (d.half) dmg = tr(dmg / 2); if (a.bo) dmg = tr(dmg * 90 / 100); if (dmg < 1) dmg = 1;
    d.hp = Math.max(s.cfg.training ? 1 : 0, d.hp - dmg); d.comboDmg += dmg; d.redT = 50;
    const ko = d.hp <= 0, wasAir = d.y > 0 || d.st === 'ahit';
    let launch = (isProj && last && h.launchLast) ? h.launchLast : h.launch;
    const crumple = m.di && (kind || a.armHit) && !wasAir && !ko;
    if (kind === 2 && m.pcLaunch && !wasAir) launch = m.pcLaunch;
    if (crumple) { launch = null; s.slow = Math.max(s.slow, 36); }
    if (m.di && !d.bo) { d.dr -= (kind === 2 || a.armHit) ? 1500 : BAR; burn(s, d, di); }
    if (kind) hsp += 2; if (ko) hsp = 22;
    const wasCr = crouching(d), wrong = !wasAir && !isProj && (h.g === 'l' || (h.g === 'h' && !m.isAir)) && (d.st === 'idle' || d.st === 'block') ? h.g : '';
    d.crum = 0; d.arm = 0;
    if (wasAir || launch || ko || h.carry) {
      const vec = wasAir ? (((isProj && last && h.launchLast) ? h.launchLast : h.air) || [300, 620]) : (launch || [520, 820]);
      d.st = 'ahit'; d.sf = 0; d.mv = '';
      d.vx = side * vec[0] + (isProj ? 0 : tr(a.vx / 4)); d.vy = vec[1]; if (d.y <= 0) { d.y = 1; if (d.vy < 0) d.vy = 300; }
      if (ko && d.jug > 2) d.jug = 2;
      if (h.lk) { const pp = h.pop || [24, 430]; d.vx = 0; d.vy = pp[1]; if (d.y < pp[0] * F) d.y = pp[0] * F; } else d.jug++;
      if (h.xup && wasAir) a.vx = a.face * h.xup;
      if (h.reb && a.y > 0) { a.vy = h.reb; a.vx = a.face * 90; a.fr = 2; }   // bounce off them and get your air options back
      if (h.carry) { d.x = a.x + a.face * h.carry[0] * F; d.y = a.y + h.carry[1] * F; d.vx = a.vx; d.vy = a.vy; }
      if (h.free && !isProj) {
        // the first two in a combo pop them and hand the attacker a fresh jump; after that it is just a knock-away
        if ((d.hmc || 0) < 2 && !ko) { d.hmc = (d.hmc || 0) + 1; a.fr = 1; a.vx = 0; a.vy = GRAV; d.x = a.x + a.face * 24 * F; { const lm = STAGE - PW; if (d.x > lm) { a.x -= d.x - lm; d.x = lm; } else if (d.x < -lm) { a.x += -lm - d.x; d.x = -lm; } } if (d.y < a.y + 8 * F) d.y = a.y + 8 * F; d.vx = a.face * 30; d.vy = 960; d.gbPend = 0; }
        else { a.vx = tr(a.vx / 4); d.vx = side * 540; d.vy = 560; }
      }
      d.pv = 0; d.hard = (m.sup || m.hardKd) ? 1 : 0;
      d.wbPend = ((h.wb || (isProj && last && h.wbLast)) && !d.wbUsed) ? 1 : 0; d.gbPend = (h.gb && !d.gbUsed) ? 1 : 0;
    } else {
      d.st = 'hit'; d.sf = 0; d.mv = ''; d.stun = crumple ? 84 : h.stun + (kind === 2 ? 4 : kind === 1 ? 2 : 0) + drb; d.pv = crumple ? side * 200 : side * h.kb; d.vx = 0; d.cb = wasCr && !crumple && !m.stand ? 1 : 0; d.hitHi = (cy - d.y) > 44 * F ? 1 : 0; if (crumple) d.crum = 1;
    }
    d.hs = hsp; if (!isProj) { a.hs = hsp; a.contact = 1; a.hitOK = 1; }
    a.sa = Math.min(3000, a.sa + (m.sup ? 0 : m.sg != null ? tr(m.sg / Math.max(1, m.hits.length || (m.proj ? m.proj.hits : 1))) : tr(dmg * 7 / 10))); d.sa = Math.min(3000, d.sa + tr(dmg * 3 / 10));
    if (!a.bo) a.dr = Math.min(DMAX, a.dr + tr(dmg * 3 / 2));
    s.ev.push({ t: 'hit', x: cx, y: cy, lvl: h.lvl, who: ai, dmg: dmg, combo: d.combo, total: d.comboDmg, ch: kind, ko: ko ? 1 : 0, sp: special ? 1 : 0, di: m.di ? 1 : 0, crum: crumple ? 1 : 0, wrong: wrong, free: a.fr === 1 ? 1 : 0, fin: h.fin ? 1 : 0 });
  }
  function kindOf(d) { if (d.st === 'prec' || d.st === 'parry') return 2; if (d.st === 'run' || d.st === 'drush' || d.st === 'dashf' || d.st === 'dashb') return 1; if (d.st !== 'atk') return 0; const m = CH[d.ch].mv[d.mv]; return d.mf > m.lastAct ? 2 : 1; }
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
      if (['idle', 'atk', 'dashf', 'land', 'parry', 'prec', 'drush', 'run'].indexOf(d.st) < 0 && !(d.st === 'dashb' && d.sf >= 15)) continue;
      if (d.st === 'atk' && CH[d.ch].mv[d.mv].inv && invul(d)) continue;
      if (Math.abs(d.x - a.x) > 58 * F) continue;   // the ribbons reach further than arms would
      g[i] = true;
    }
    if (g[0] && g[1]) { for (let i = 0; i < 2; i++) { const p = s.p[i]; p.st = 'tech'; p.sf = 0; p.mv = ''; p.pv = -p.face * 620; } s.ev.push({ t: 'tech', x: (s.p[0].x + s.p[1].x) >> 1, y: 60 * F }); return; }
    for (let i = 0; i < 2; i++) if (g[i]) {
      const a = s.p[i], d = s.p[1 - i];
      d.noTech = ((d.st === 'atk' && !CH[d.ch].mv[d.mv].thr) || d.st === 'parry' || d.st === 'prec' || d.st === 'drush' || d.st === 'run') ? 1 : 0;
      a.st = 'throwing'; a.sf = 0; a.tk = 0; a.tf = 0; a.pmN = 0; a.pmT = 0; a.mv = ''; a.vx = 0; d.st = 'thrown'; d.sf = 0; d.mv = ''; d.vx = 0; d.vy = 0; d.pv = 0; d.face = -a.face; d.combo = 0; d.arm = 0; d.crum = 0;
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
      const d = s.p[1 - q.o], m = CH[q.ch].mv[q.k], pr = m.proj, b = pbox(q), dm = d.st === 'atk' ? CH[d.ch].mv[d.mv] : null;
      if (dm && dm.pinv && d.mf >= dm.pinv[0] && d.mf <= dm.pinv[1]) continue;
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
      const p = s.p[i]; let inp = live || (s.phase === 'ko' && s.rw === i && s.koType !== 'time') ? raw[i] : 0;
      if (s.phase === 'ko' && p.st === 'win' && (inp & 0xFF0)) { p.st = 'idle'; p.sub = 'stand'; p.sf = 0; }
      if ((inp & 12) === 12) inp &= ~12; if ((inp & 3) === 3) inp &= ~3;
      raw[i] = inp;
      const frozen = s.freeze > 0 || p.hs > 0;
      p.buf.push(inp); p.buf.shift();
      const pr = inp & ~p.prev;
      for (let b = 0; b < NB; b++) { if (pr & (16 << b)) { p.age[b] = 0; p.pd[b] = inp & 271; } else if (!frozen && p.age[b] < 99) p.age[b]++; }
      if ((inp & 32) && (inp & 64) && p.age[1] <= 3 && p.age[2] <= 3 && (pr & 96)) p.rq = 0; else if (!frozen && p.rq < 99) p.rq++;
      if (dashIn(p, 1)) p.d66 = 0; else if (!frozen && p.d66 < 99) p.d66++;
      if (dashIn(p, -1)) p.d44 = 0; else if (!frozen && p.d44 < 99) p.d44++;
      p.prev = inp;
    }
    if (s.phase === 'intro') { s.pt++; if (s.pt === 1) s.ev.push({ t: 'round', n: s.round }); if (s.pt >= INTRO) { s.phase = 'fight'; s.pt = 0; s.ev.push({ t: 'fight' }); } }
    if (s.freeze > 0) { s.freeze--; return s; }
    if (s.phase === 'ko') { s.pt++; if (s.pt < 56 && s.pt % 4 !== 0) return s; }
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
      if (p.drT > 0) p.drT--; else if (['idle', 'dashf', 'dashb', 'jump', 'land', 'prejump', 'atk', 'run'].indexOf(p.st) >= 0 && s.phase === 'fight') {
        p.dr = Math.min(DMAX, p.dr + (p.bo ? 5 : p.y > 0 ? 3 : p.st === 'idle' && p.sub === 'walkf' ? 9 : 6));
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
          s.phase = 'ko'; s.pt = 0; s.kx = 0; let w = -1;
          if (d0 || d1) { s.koType = 'ko'; w = d0 && d1 ? -1 : d0 ? 1 : 0; }
          else { s.koType = 'time'; const r0 = s.p[0].hp * CH[s.p[1].ch].hp, r1 = s.p[1].hp * CH[s.p[0].ch].hp; w = r0 > r1 ? 0 : r1 > r0 ? 1 : -1; }
          s.rw = w; if (w >= 0) { s.p[w].wins++; if (s.koType === 'ko' && s.p[w].hp >= CH[s.p[w].ch].hp) s.koType = 'perfect'; } else s.koType = d0 ? 'dko' : 'draw';
          s.ev.push({ t: 'ko', type: s.koType, w: w });
        }
      }
    } else if (s.phase === 'ko') {
      // every hit on the body buys a little more time, up to ten extra seconds
      if (s.rw >= 0 && s.ev.some(e => e.t === 'hit') && s.kx < 600 && s.pt > KOTIME - 100) { const add = Math.min(600 - s.kx, s.pt - (KOTIME - 100)); s.kx += add; s.pt -= add; }
      if (s.pt === 150 && s.rw >= 0 && s.kx === 0) { const w = s.p[s.rw]; if (w.st === 'idle' && w.y <= 0) { w.st = 'win'; w.sf = 0; w.vx = 0; } s.ev.push({ t: 'winpose', w: s.rw }); }
      if (s.pt >= KOTIME) {
        const need = s.cfg.rounds;
        if (s.p[0].wins >= need || s.p[1].wins >= need || s.round >= need * 2 + 1) {
          s.phase = 'over'; s.winner = s.p[0].wins > s.p[1].wins ? 0 : s.p[1].wins > s.p[0].wins ? 1 : -1; s.ev.push({ t: 'over', w: s.winner });
        } else { s.round++; resetRound(s); }
      }
    }
    return s;
  }
  function debugStart(s, i, key) { const p = s.p[i]; p.y = 0; start(s, p, key, i); }
  return { debugStart, F, BT, MASK, STAGE, SEP, CH, INTRO, KOTIME, KD, GETUP, DMAX, newMatch, step, clone, hash, hurt, hitboxes, pbox, crouching, invul, npad, hdir };
})();
if (typeof module !== 'undefined') module.exports = SIM;
