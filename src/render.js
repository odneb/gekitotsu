// ===================================================================
// Presentation: low-res 3D fighters on a 2D gameplay plane, 2D fx + HUD.
// Reads sim state only; never writes it.
// ===================================================================
const R = (function () {
  'use strict';
  const D = Math.PI / 180, F = SIM.F, VW = 480, VH = 270;
  let snapCam = true, ren, scene, cam, g2, stage, rigs = [], shadows = [], projMeshes = [], fx = [], shake = 0, flash = 0, camX = 0, camW = 440, camY = 70, tick = 0;
  const combo = [null, null]; let msg = null, superFx = null; const FJ = "'DotGothic16','Press Start 2P',monospace", FP = "'Press Start 2P','DotGothic16',monospace";

  // ----------------------------------------------------------- skeleton + hand-keyed animation
  // A pose is a set of targets in the fighter's side plane (x forward, y up, optional z toward camera):
  // hip position, spine lean, chest/pelvis twist, and where each hand and foot is. Limbs are solved by IK.
  const TH = 22, SHN = 22, SP1 = 13, SP2 = 15, UA = 14.5, FA = 14, SW = 12.5, HW = 6;
  const NUM = ['ln', 'ln2', 'tw', 'pt', 'hd', 'eL', 'eR', 'kL', 'kR', 'yaw', 'rot', 'ay'], VEC = ['hip', 'hL', 'hR', 'fL', 'fR'];
  const ST = { hip: [0, 42, 0], ln: 5, ln2: 9, tw: -50, pt: -35, hd: 0, hL: [21, 67, 0], hR: [8, 60, 0], fL: [14, 4, 0], fR: [-14, 4, 0], eL: -1, eR: -1, kL: 0, kR: 0, yaw: 0, rot: 0, ay: 0.5, aim: null, aw: 0 };
  function norm(o, b) { const r = {}; for (const k of NUM) r[k] = o[k] != null ? o[k] : b[k]; for (const k of VEC) { const v = o[k] || b[k]; r[k] = [v[0], v[1], v[2] || 0]; } if (o.ln != null && o.ln2 == null) r.ln2 = o.ln + 4; r.aim = o.aim || null; r.aw = o.aim ? 1 : 0; r.up = o.up || 0; return r; }
  function mix(a, b, k) {
    if (k <= 0) return a; if (k >= 1) return b; const r = {}; for (const n of NUM) r[n] = a[n] + (b[n] - a[n]) * k;
    for (const n of VEC) r[n] = [a[n][0] + (b[n][0] - a[n][0]) * k, a[n][1] + (b[n][1] - a[n][1]) * k, a[n][2] + (b[n][2] - a[n][2]) * k];
    if (b.aim && (!a.aim || a.aim === b.aim)) { r.aim = b.aim; r.aw = (a.aim ? a.aw : 0) + (b.aw - (a.aim ? a.aw : 0)) * k; r.up = b.up; } else if (a.aim) { r.aim = a.aim; r.aw = a.aw * (1 - k); r.up = a.up; } else { r.aim = null; r.aw = 0; r.up = 0; }
    return r;
  }
  const sm = k => k <= 0 ? 0 : k >= 1 ? 1 : k * k * (3 - 2 * k);
  const s = o => norm(o, ST);
  const CRO = s({ hip: [0, 20], ln: 20, ln2: 26, hd: -14, hL: [17, 44], hR: [9, 40], fL: [17, 4], fR: [-16, 4], kL: 0.9, kR: 0.9, tw: -45 }), c = o => norm(o, CRO);
  const JMP = s({ hip: [0, 44], ln: 10, ln2: 14, hL: [15, 60], hR: [3, 54], fL: [9, 25], fR: [-5, 19] }), j = o => norm(o, JMP);
  const PZ = {
    idle: ST, idle2: s({ hip: [0, 40.5], ln: 6, hL: [21, 65.5], hR: [8, 58.5] }), crouch: CRO, jump: JMP,
    walkA: s({ hip: [1, 42.5], fL: [18, 4], fR: [-7, 8], hL: [20, 66], hR: [9, 61] }), walkB: s({ hip: [1, 41], fL: [7, 8], fR: [-16, 4], hL: [22, 68], hR: [7, 59] }),
    prejump: s({ hip: [0, 30], ln: 16, hL: [16, 50], hR: [4, 44], kL: 0.4, kR: 0.4 }),
    fall: s({ hip: [0, 44], ln: 4, hL: [17, 70], hR: [-7, 66], fL: [9, 9], fR: [-8, 7], eL: 0, eR: 0 }),
    dashf: s({ hip: [5, 33], ln: 34, ln2: 38, hd: -20, tw: -30, hL: [22, 44], hR: [-12, 46], fL: [22, 5], fR: [-20, 9] }),
    dashb: s({ hip: [-5, 44], ln: -8, hL: [14, 64], hR: [2, 58], fL: [10, 8], fR: [-12, 6] }),
    runA: s({ hip: [4, 33], ln: 44, ln2: 48, hd: -34, tw: -75, pt: -55, hL: [-10, 40], hR: [26, 52], fL: [24, 8], fR: [-22, 18], eL: -0.2, eR: -1 }),
    runB: s({ hip: [4, 35], ln: 44, ln2: 48, hd: -34, tw: 10, pt: -5, hL: [26, 54], hR: [-12, 40], fL: [-18, 20], fR: [20, 5], eL: -1, eR: -0.2 }),
    runStop: s({ hip: [-2, 36], ln: -6, hL: [18, 58], hR: [4, 52], fL: [20, 4], fR: [-12, 4] }),
    parry: s({ hip: [0, 32], ln: 16, ln2: 20, hd: -12, tw: -60, hL: [25, 54], hR: [11, 62], eL: -0.2, eR: -0.6, fL: [18, 4], fR: [-18, 4], kL: 0.4, kR: 0.4 }),
    blockS: s({ hip: [-3, 42], ln: -5, ln2: -2, hd: -8, tw: -35, hL: [12, 74], hR: [8, 67], fL: [12, 4], fR: [-15, 4] }),
    blockC: c({ hip: [-3, 21], ln: 10, hd: -8, hL: [13, 50], hR: [9, 43] }),
    hitHi: s({ hip: [-5, 42], ln: -22, ln2: -32, hd: -28, tw: -75, hL: [4, 62], hR: [-10, 52], fL: [10, 4], fR: [-17, 4], eL: -1, eR: -1 }),
    hitMid: s({ hip: [-7, 39], ln: 28, ln2: 42, hd: 22, tw: -30, hL: [12, 44], hR: [5, 40], fL: [11, 4], fR: [-16, 4] }),
    hitBig: s({ hip: [-9, 41], ln: -30, ln2: -44, hd: -35, tw: -95, hL: [-6, 70], hR: [-18, 50], fL: [12, 8], fR: [-18, 4], eL: 0.2 }),
    hitLow: s({ hip: [-4, 37], ln: 20, ln2: 26, hd: 10, hL: [16, 50], hR: [4, 46], fL: [3, 10], fR: [-15, 4] }),
    hitCr: c({ hip: [-5, 21], ln: -4, ln2: -10, hd: -18, hL: [10, 44], hR: [-2, 36] }),
    ahit: s({ hip: [0, 30], ln: -28, ln2: -40, hd: -25, hL: [-8, 54], hR: [-16, 44], fL: [20, 22], fR: [12, 12], eL: 0, eR: 0 }),
    kd: s({ hip: [0, 6], ln: -84, ln2: -92, hd: -40, tw: -20, pt: -10, hL: [-14, 3], hR: [-30, 4, 6], fL: [38, 4], fR: [30, 9], eL: 0.3, eR: 0.3 }),
    situp: s({ hip: [-2, 9], ln: -20, ln2: 5, hd: 10, hL: [10, 14], hR: [-12, 3], fL: [26, 4], fR: [14, 4] }),
    crumA: s({ hip: [-2, 36], ln: 34, ln2: 48, hd: 32, tw: -20, hL: [14, 30], hR: [4, 28], fL: [10, 4], fR: [-14, 4], kL: 0.5, kR: 0.5 }),
    crumB: s({ hip: [3, 15], ln: 62, ln2: 74, hd: 30, tw: -15, hL: [26, 3], hR: [18, 3], fL: [8, 4], fR: [-18, 4], kL: 0.3, kR: 0.3 }),
    splat: s({ hip: [-4, 44], ln: -8, ln2: -14, hd: -22, tw: -88, hL: [-10, 76, -8], hR: [-12, 70, 8], eL: 1, eR: 1, fL: [6, 8], fR: [-8, 6] }),
    win: s({ hip: [0, 44], ln: 0, ln2: -4, tw: -85, hd: -8, hL: [10, 52], hR: [2, 100], eR: 0.2, fL: [9, 4], fR: [-10, 4] }),
    thrown: s({ hip: [0, 36], ln: -16, hd: -20, hL: [-8, 50], hR: [-14, 44], fL: [12, 10], fR: [-6, 6], eL: 0, eR: 0 }),
    grabW: s({ hip: [1, 41], ln: 10, tw: -30, hL: [22, 60], hR: [16, 54] }), grabS: s({ hip: [6, 40], ln: 20, tw: -10, hL: [35, 62], hR: [33, 55], fL: [18, 4] }),
    thPull: s({ hip: [2, 40], ln: -6, tw: -70, hL: [20, 58], hR: [16, 50] }), thKnee: s({ hip: [6, 44], ln: 12, tw: 20, hL: [24, 60], hR: [22, 54], fR: [20, 34], fL: [4, 4] }),
    thKick: s({ hip: [6, 43], ln: -18, tw: 60, pt: 55, fR: [48, 40], fL: [4, 4], hL: [8, 58], hR: [-6, 50] }),
    thSlam: s({ hip: [8, 34], ln: 44, ln2: 52, hd: -20, tw: 40, hL: [30, 24], hR: [34, 18], fL: [20, 4], fR: [-16, 6] }),
    bthRoll: s({ hip: [-10, 16], ln: -62, ln2: -70, hd: 20, hL: [10, 44], hR: [8, 38], fL: [14, 4], fR: [16, 40] }), bthToss: s({ hip: [-14, 12], ln: -84, ln2: -90, hd: 10, hL: [-24, 30], hR: [-26, 24], fL: [8, 4], fR: [-10, 52] }),
    // ---- normals
    jabW: s({ tw: -42, hL: [14, 66] }), jabS: s({ hip: [2, 42], ln: 7, tw: -74, hL: [40, 70], aim: 'hL', hR: [6, 62], hd: -4 }),
    mpW: s({ hip: [1, 40], ln: 10, tw: -68, hR: [-5, 50], hL: [17, 64] }), mpS: s({ hip: [7, 36], ln: 22, ln2: 28, hd: -18, tw: 38, pt: 5, hR: [33, 56], aim: 'hR', hL: [0, 57], fL: [18, 4], fR: [-15, 6] }),
    hpW: s({ hip: [-3, 41], ln: 0, tw: -80, pt: -55, hR: [-12, 57], hL: [20, 63] }), hpS: s({ hip: [10, 35], ln: 26, ln2: 30, hd: -22, tw: 62, pt: 22, hR: [45, 65], aim: 'hR', hL: [-5, 52], fL: [21, 4], fR: [-17, 7] }),
    lkW: s({ hip: [-2, 44], ln: -6, tw: -70, pt: -60, fL: [9, 24], fR: [-8, 4], hL: [12, 62], hR: [0, 58] }), lkS: s({ hip: [-3, 44], ln: -15, ln2: -12, tw: -86, pt: -75, fL: [51, 33], aim: 'fL', fR: [-7, 4], hL: [10, 60], hR: [-4, 56] }),
    mkW: s({ hip: [-2, 44], ln: -12, tw: -75, pt: -65, fL: [7, 32], fR: [-7, 4], hL: [8, 60], hR: [-4, 60] }), mkS: s({ hip: [-4, 45], ln: -34, ln2: -38, hd: 18, tw: -92, pt: -85, fL: [51, 64], aim: 'fL', fR: [-5, 4], hL: [-2, 56], hR: [-16, 60] }),
    hkW: s({ hip: [2, 43], ln: 4, tw: 55, pt: 45, fR: [0, 22], fL: [4, 4], hL: [12, 64], hR: [-8, 60] }), hkS: s({ hip: [2, 45], ln: -26, ln2: -30, hd: 22, tw: 150, pt: 130, fR: [61, 57], aim: 'fR', fL: [2, 4], hL: [-10, 54], hR: [-18, 64] }),
    cjabS: c({ hip: [0, 22], tw: -72, hL: [45, 46], aim: 'hL' }),
    clkS: c({ hip: [-3, 21], ln: 8, tw: -70, fL: [53, 8], aim: 'fL', ay: 0.35, fR: [-11, 4], kL: 0 }),
    cmkW: c({ hip: [-3, 22], ln: 6, fL: [10, 12], kL: 0.3 }), cmkS: c({ hip: [-7, 18], ln: -32, ln2: -24, hd: 20, tw: -84, pt: -72, fL: [57, 7], aim: 'fL', ay: 0.35, fR: [-10, 4], hL: [4, 36], hR: [-24, 3], kL: 0, eR: -0.3 }),
    cupW: c({ hip: [1, 19], ln: 22, tw: -70, hR: [0, 24], hL: [16, 44] }), cupS: s({ hip: [4, 41], ln: -5, ln2: -10, hd: -22, tw: 58, pt: 20, hR: [20, 100], aim: 'hR', up: 1, hL: [-2, 54], fL: [17, 4], fR: [-14, 5] }),
    swS: c({ hip: [-4, 15], ln: -42, ln2: -34, hd: 28, tw: -84, pt: -75, hR: [-25, 3], hL: [-4, 32], fL: [64, 16], aim: 'fL', ay: 0.25, fR: [5, 4], kL: 0, kR: 0.1, eR: -0.3 }),
    axW: s({ hip: [0, 46], ln: -14, tw: -70, pt: -60, fL: [18, 84], fR: [-6, 4], hL: [0, 58], hR: [-10, 62] }), axS: s({ hip: [4, 42], ln: 12, tw: -60, fL: [50, 42], aim: 'fL', ay: 0.3, fR: [-8, 4], hL: [10, 56], hR: [-6, 50] }),
    fskW: s({ hip: [4, 44], ln: -4, tw: 30, pt: 20, fR: [12, 30], fL: [4, 4] }), fskS: s({ hip: [12, 43], ln: -20, ln2: -18, tw: 60, pt: 55, fR: [79, 39], aim: 'fR', fL: [6, 4], hL: [8, 58], hR: [-6, 50] }),
    cbW: s({ hip: [3, 36], ln: 20, tw: -72, hR: [2, 34], hL: [16, 60] }), cbS: s({ hip: [7, 44], ln: -4, hd: -18, tw: 52, pt: 15, hR: [30, 84], aim: 'hR', up: 1, hL: [0, 54], fL: [16, 4], fR: [-10, 6] }),
    tf2W: s({ hip: [3, 43], tw: 40, pt: 30, fR: [2, 24], fL: [5, 4] }), tf2S: s({ hip: [3, 44], ln: -20, hd: 14, tw: 100, pt: 85, fR: [49, 63], aim: 'fR', fL: [4, 4], hL: [-4, 56], hR: [-12, 60] }),
    tf3S: s({ hip: [3, 46], ln: -16, ln2: -20, hd: 16, tw: 140, pt: 120, fR: [53, 84], aim: 'fR', fL: [4, 4], hL: [-4, 56], hR: [-14, 62] }),
    // ---- air normals (targets are relative to the feet point the sim tracks)
    jlpS: j({ ln: 16, tw: -74, hL: [26, 58], aim: 'hL', ay: 0.4, fL: [10, 28], fR: [-2, 24] }), jmpS: j({ ln: 26, tw: 45, hR: [40, 42], aim: 'hR', hL: [4, 56], fL: [4, 24], fR: [-10, 26] }),
    jhpW: j({ ln: -6, tw: -70, hR: [-4, 84], hL: [8, 80], eR: 0.6, eL: 0.6 }), jhpS: j({ hip: [0, 46], ln: 36, ln2: 44, hd: -20, tw: 50, hR: [23, 12], aim: 'hR', ay: 0.6, hL: [10, 30], fL: [-2, 30], fR: [-14, 34] }),
    jlkS: j({ ln: 12, fL: [20, 14], aim: 'fL', fR: [0, 26], hL: [12, 60], hR: [2, 56] }), jmkS: j({ hip: [0, 46], ln: -10, tw: -88, pt: -75, fL: [42, 27], aim: 'fL', fR: [-4, 30], hL: [6, 60], hR: [-8, 60] }),
    jhkS: j({ hip: [0, 46], ln: -12, tw: 95, pt: 80, fR: [40, 22], aim: 'fR', fL: [-2, 30], hL: [-2, 62], hR: [-10, 58] }), njhkS: j({ hip: [0, 46], ln: -10, tw: -85, pt: -75, fL: [39, 45], aim: 'fL', fR: [-30, 40], hL: [6, 66], hR: [-10, 66] }),
    // ---- specials
    hadW: s({ hip: [-4, 36], ln: 4, tw: -86, pt: -60, hL: [-8, 46], hR: [-13, 40], fL: [17, 4], fR: [-17, 4], eL: -0.4 }), hadS: s({ hip: [8, 31], ln: 22, ln2: 26, hd: -18, tw: -8, pt: -10, hL: [38, 60], hR: [36, 55], fL: [24, 4], fR: [-24, 5], eL: 0, eR: 0 }),
    srW: c({ hip: [5, 19], ln: 34, hd: -20, tw: -72, hR: [4, 18], hL: [18, 40] }), srS: s({ hip: [4, 46], ln: -8, ln2: -14, hd: -24, tw: 110, pt: 60, hR: [14, 99], hL: [-2, 52], fL: [10, 22], fR: [1, 5], eR: 0 }),
    srF: s({ hip: [2, 46], ln: 0, tw: 30, hR: [10, 92], hL: [2, 54], fL: [10, 14], fR: [-6, 8] }),
    atatS: j({ hip: [0, 46], ln: 22, ln2: 26, hd: -10, tw: -90, pt: -85, fR: [40, 38], fL: [4, 28], hL: [8, 60, -8], hR: [-4, 58, 8] }),
    bthTuck: s({ hip: [-6, 20], ln: 42, ln2: 70, hd: 30, hL: [8, 32], hR: [6, 26], fL: [8, 8], fR: [10, 12], kL: 0.2, kR: 0.2 }),
    tatS: s({ hip: [0, 45], ln: -18, ln2: -14, hd: 12, tw: -90, pt: -85, fR: [44, 48], fL: [2, 8], hL: [4, 62, -8], hR: [-8, 60, 8] }),
    dlW: s({ hip: [4, 28], ln: 34, hd: -20, tw: -60, hL: [18, 40], hR: [-8, 40], kL: 0.3, kR: 0.3 }), dlF: j({ hip: [0, 46], ln: 30, ln2: 40, tw: 60, fL: [-4, 30], fR: [-14, 36], hL: [16, 50], hR: [4, 44] }),
    dlS: j({ hip: [-2, 50], ln: -50, ln2: -55, hd: 34, tw: 140, pt: 120, fR: [46, 14], aim: 'fR', ay: 0.75, fL: [-6, 34], hL: [-14, 60], hR: [-22, 66] }),
    jinW: s({ hip: [3, 43], ln: 4, tw: 150, pt: 110, fR: [-2, 20], fL: [6, 4], hL: [10, 62], hR: [-8, 60] }), jinS: s({ hip: [6, 45], ln: -14, ln2: -18, hd: 14, tw: 135, pt: 115, fR: [67, 60], aim: 'fR', ay: 0.9, fL: [5, 4], hL: [-4, 56], hR: [-14, 64] }),
    kazeS: s({ hip: [8, 37], ln: -12, ln2: -8, hd: 8, tw: -72, pt: -62, fL: [64, 7], aim: 'fL', ay: 0.45, fR: [-10, 4], hL: [14, 62], hR: [2, 56] }),
    goW: s({ hip: [2, 50], ln: 6, tw: 60, pt: 50, fR: [14, 42], fL: [2, 12], hL: [12, 62], hR: [-4, 60] }), goS: s({ hip: [6, 40], ln: 18, tw: 40, pt: 30, fR: [56, 40], aim: 'fR', ay: 0.25, fL: [4, 4], hL: [10, 54], hR: [-4, 48] }),
    senS: s({ hip: [8, 46], ln: -30, ln2: -34, hd: 22, tw: -94, pt: -84, fL: [71, 69], aim: 'fL', fR: [3, 4], hL: [-2, 54], hR: [-14, 60] }),
    kasW: s({ hip: [2, 44], ln: -10, tw: -80, pt: -70, fL: [10, 38], fR: [-4, 4], hL: [6, 58], hR: [-6, 56] }), kasS: s({ hip: [6, 44], ln: -38, ln2: -40, hd: 26, tw: -96, pt: -88, fL: [75, 50], aim: 'fL', fR: [-4, 4], hL: [-8, 52], hR: [-20, 56] }),
    diW: s({ hip: [-5, 39], ln: -4, tw: -100, pt: -70, hR: [-18, 62], hL: [16, 60], fL: [14, 4], fR: [-16, 4], eR: 0.2 }),
    diS: s({ hip: [11, 34], ln: 20, ln2: 26, hd: -34, tw: 120, pt: 60, hR: [50, 56], aim: 'hR', hL: [-8, 56], fL: [22, 4], fR: [-16, 8], eL: 0.2 }),
    diR: s({ hip: [8, 40], ln: 8, hd: -50, tw: 172, pt: 95, hR: [22, 64], hL: [4, 70], fL: [21, 4], fR: [-14, 6], eR: 0.4, eL: 0.4 }),
    sa1S: j({ hip: [0, 48], ln: -26, hd: 20, tw: 140, pt: 120, fR: [58, 60], aim: 'fR', fL: [-4, 26], hL: [-8, 58], hR: [-18, 66] })
  };
  const P = n => PZ[n] || ST;
  function kf(keys, t) {
    if (t <= keys[0][0]) return P(keys[0][1]);
    for (let i = 1; i < keys.length; i++) if (t <= keys[i][0]) { const a = keys[i - 1], b = keys[i], e = b[2]; let u = (t - a[0]) / Math.max(1e-6, b[0] - a[0]); u = e === 'i' ? u * u * u : e === 'o' ? 1 - (1 - u) * (1 - u) * (1 - u) : e === 'l' ? u : sm(u); return mix(P(a[1]), P(b[1]), u); }
    return P(keys[keys.length - 1][1]);
  }
  // attack timelines in phase time: 0-1 startup, 1-2 active, 2-3 recovery
  const NB = { idle: 1, crouch: 1, jump: 1 }, A = (base, w, st) => ({ k: [[0, base], [0.55, w, 'o'], [1, st, 'i'], [2, st], [3, NB[base] ? base : 'idle']] });
  const Q = (base, st) => ({ k: [[0, base], [1, st, 'i'], [2, st], [3, NB[base] ? base : 'idle']] });
  const AN = {
    '5LP': Q('idle', 'jabS'), '5MP': A('idle', 'mpW', 'mpS'), '5HP': A('idle', 'hpW', 'hpS'), '5LK': A('idle', 'lkW', 'lkS'), '5MK': A('idle', 'mkW', 'mkS'), '5HK': A('idle', 'hkW', 'hkS'),
    '2LP': Q('crouch', 'cjabS'), '2LK': Q('crouch', 'clkS'), '2MK': A('crouch', 'cmkW', 'cmkS'), '2HP': A('crouch', 'cupW', 'cupS'), '2HK': A('crouch', 'cmkW', 'swS'),
    TK: A('idle', 'axW', 'axS'), FSK: A('idle', 'fskW', 'fskS'), CB2: A('mpS', 'cbW', 'cbS'), TFK2: A('mkS', 'tf2W', 'tf2S'), TFK3: A('tf2S', 'mkW', 'tf3S'),
    jLP: Q('jump', 'jlpS'), jMP: Q('jump', 'jmpS'), jHP: A('jump', 'jhpW', 'jhpS'), jLK: Q('jump', 'jlkS'), jMK: Q('jump', 'jmkS'), jHK: Q('jump', 'jhkS'), njHK: Q('jump', 'njhkS'),
    throw: A('idle', 'grabW', 'grabS'),
    DI: { k: [[0, 'idle'], [0.55, 'diW', 'o'], [0.9, 'diW'], [1, 'diS', 'i'], [2, 'diS'], [2.3, 'diR', 'o'], [2.7, 'diR'], [3, 'idle']] },
    DREV: { k: [[0, 'blockS'], [0.6, 'diW', 'o'], [1, 'diS', 'i'], [2, 'diS'], [2.4, 'diR', 'o'], [3, 'idle']] },
    HAD: { k: [[0, 'idle'], [0.6, 'hadW', 'o'], [1, 'hadS', 'i'], [2.6, 'hadS'], [3, 'idle']] },
    SRK: { k: [[0, 'idle'], [0.7, 'srW', 'o'], [1, 'srS', 'i'], [2, 'srS'], [2.5, 'srF'], [3, 'srF']] },
    TAT: { k: [[0, 'idle'], [0.5, 'tf2W'], [1, 'tatS', 'i'], [2, 'tatS'], [2.5, 'jump'], [3, 'idle']], spin: 1 },
    ATAT: { k: [[0, 'jump'], [1, 'atatS', 'i'], [2, 'atatS'], [3, 'jump']], spin: 1 },
    DL: { f: m => [[0, 'idle'], [Math.max(2, m.rise ? m.rise[0] - 8 : 3), 'dlW', 'o'], [m.rise ? m.rise[0] : 6, 'dlW'], [m.hits[m.hits.length - 1].s - 5, 'dlF'], [m.hits[m.hits.length - 1].s, 'dlS', 'i'], [m.lastAct + 2, 'dlS'], [m.lastAct + 10, 'fall']], yaw: m => [m.rise ? m.rise[0] : 6, m.hits[m.hits.length - 1].s - 1, 360] },
    JIN: A('idle', 'jinW', 'jinS'), KAZE: Q('jinS', 'kazeS'), GORAI: { k: [[0, 'jinS'], [0.55, 'goW', 'o'], [1, 'goS', 'i'], [2, 'goS'], [2.5, 'crouch'], [3, 'idle']] }, SENKA: A('jinS', 'kasW', 'senS'), KASAI: A('kazeS', 'kasW', 'kasS'),
    SA1: { f: m => { const k = [[0, 'dlW'], [4, 'kasW'], [6, 'tf3S', 'i'], [9, 'tf3S'], [11, 'dlW']], st = ['jhkS', 'sa1S', 'jmkS', 'sa1S']; for (let i = 1; i < m.hits.length - 1; i++) { const h = m.hits[i]; k.push([h.s - 2, 'dlF']); k.push([h.s, st[(i - 1) % 4], 'i']); k.push([h.s + 1, st[(i - 1) % 4]]); } const l = m.hits[m.hits.length - 1]; k.push([l.s - 3, 'dlF']); k.push([l.s, 'dlS', 'i']); k.push([l.s + 6, 'dlS']); k.push([l.s + 16, 'fall']); return k; }, yaw: m => [11, m.hits[m.hits.length - 1].s - 1, 1800] },
    SA2: { f: m => { const k = [[0, 'jinW']], st = ['jinS', 'tf3S', 'mkS', 'jinS'], n = 4; for (let i = 0; i < n; i++) { const h = m.hits[i]; if (i) k.push([h.s - 3, i & 1 ? 'mkW' : 'jinW']); k.push([h.s, st[i], 'i']); k.push([Math.min(h.e, h.s + 2), st[i]]); } k.push([m.rise[0] - 2, 'tf2W']); k.push([m.rise[0] + 2, 'tatS', 'i']); const l = m.hits[m.hits.length - 1]; k.push([l.s + 8, 'tatS']); k.push([l.s + 22, 'fall']); return k; }, yaw: m => [m.rise[0] - 1, m.hits[m.hits.length - 1].s + 8, 2160] },
    SA3: { f: m => { const k = [[0, 'mpW']], st = ['mpS', 'cbS', 'hpS', 'cbS']; for (let i = 0; i < m.hits.length - 1; i++) { const h = m.hits[i]; if (i) k.push([h.s - 4, i & 1 ? 'cbW' : 'hpW', 'o']); k.push([h.s, st[i % 4], 'i']); k.push([h.s + 2, st[i % 4]]); } const l = m.hits[m.hits.length - 1]; k.push([l.s - 5, 'srW', 'o']); k.push([l.s, 'srS', 'i']); k.push([l.s + 40, 'srS']); k.push([l.s + 60, 'srF']); return k; }, yaw: m => [m.hits[m.hits.length - 1].s, m.hits[m.hits.length - 1].s + 36, 720] }
  };
  const anOf = n => AN[n] || AN[n.replace(/^Q/, '').replace(/_(L|M|H|OD)$/, '')] || null;
  const fcache = {};
  const react = ['', ''], RX = { L: 'hitHi', M: 'hitMid', H: 'hitBig', K: 'hitBig', LOW: 'hitLow', CR: 'hitCr' };

  function fighterPose(p, o, sFrame, idx) {
    const C = SIM.CH[p.ch]; let po, hb = null, yaw = 0;
    switch (p.st) {
      case 'idle':
        if (p.sub === 'crouch') po = mix(P('idle'), P('crouch'), sm((p.subf + 1) / 4));
        else if (p.sub === 'walkf' || p.sub === 'walkb') { const k = (Math.sin(p.subf * (p.sub === 'walkf' ? 0.3 : -0.26)) + 1) / 2; po = mix(P('walkA'), P('walkB'), k); po = mix(P('idle'), po, Math.min(1, (p.subf + 1) / 4)); if (p.sub === 'walkb') po = mix(po, P('blockS'), 0.25); }
        else po = mix(P('idle'), P('idle2'), (Math.sin(sFrame * 0.09 + p.ch) + 1) / 2);
        break;
      case 'prejump': po = mix(P('idle'), P('prejump'), sm((p.sf + 1) / 3)); break;
      case 'land': po = mix(P('prejump'), P('idle'), sm(p.sf / 4)); break;
      case 'jump': po = mix(P('jump'), P('fall'), sm((300 - p.vy) / 900)); break;
      case 'dashf': po = p.sf < 4 ? mix(P('idle'), P('dashf'), sm(p.sf / 4)) : mix(P('dashf'), P('idle'), sm((p.sf - 11) / 7)); break;
      case 'dashb': po = mix(P('dashb'), P('idle'), sm((p.sf - 14) / 8)); po = norm(po, po); po.hip[1] += Math.max(0, Math.sin(Math.min(1, p.sf / 14) * Math.PI) * 9); po.fL[1] += Math.max(0, Math.sin(Math.min(1, p.sf / 14) * Math.PI) * 9); po.fR[1] += Math.max(0, Math.sin(Math.min(1, p.sf / 14) * Math.PI) * 7); break;
      case 'run': { const cyc = mix(P('runA'), P('runB'), (1 - Math.cos(p.sf * 0.52)) / 2); po = p.sf > 36 ? mix(cyc, P('runStop'), sm((p.sf - 36) / 8)) : mix(P('idle'), cyc, sm((p.sf + 1) / 5)); break; }
      case 'drush': { const cyc = mix(P('runA'), P('runB'), (1 - Math.cos(p.sf * 0.6)) / 2); po = p.sf > 24 ? mix(cyc, P('idle'), sm((p.sf - 24) / 10)) : mix(P('dashf'), cyc, sm(p.sf / 5)); break; }
      case 'parry': po = mix(P('idle'), P('parry'), sm((p.sf + 1) / 3)); break; case 'prec': po = mix(P('parry'), P('idle'), sm(p.sf / 11)); break;
      case 'block': po = P(p.cb ? 'blockC' : 'blockS'); break;
      case 'hit': {
        if (p.crum === 1) { po = mix(P('crumA'), P('crumB'), sm((p.sf - 14) / 46)); po = mix(P('hitMid'), po, sm(p.sf / 10)); break; }
        if (p.crum === 2) { po = P('splat'); break; }
        const h = P(p.cb ? 'hitCr' : RX[react[idx]] || (p.hitHi ? 'hitHi' : 'hitMid')), b = P(p.cb ? 'crouch' : 'idle'), tot = Math.max(1, p.sf + p.stun);
        po = mix(mix(b, h, Math.min(1, (p.sf + 1) / 2)), b, sm((p.sf - tot * 0.55) / (tot * 0.45))); break; }
      case 'ahit': po = norm(P('ahit'), ST); po.rot = -30 - Math.max(0, Math.min(80, (700 - p.vy) / 16)); break;
      case 'kd': po = P('kd'); break;
      case 'getup': { const u = p.sf / SIM.GETUP; po = u < 0.4 ? mix(P('kd'), P('situp'), sm(u / 0.4)) : u < 0.7 ? mix(P('situp'), P('crouch'), sm((u - 0.4) / 0.3)) : mix(P('crouch'), P('idle'), sm((u - 0.7) / 0.3)); break; }
      case 'tech': po = mix(P('hitHi'), P('idle'), sm(p.sf / 22)); break;
      case 'win': po = mix(P('idle'), P('win'), sm(p.sf / 16)); break;
      case 'throwing': if (p.back) { po = norm(kf([[0, 'grabS'], [7, 'thPull'], [12, 'bthTuck', 'i'], [26, 'bthTuck'], [31, 'bthToss'], [37, 'situp'], [44, 'idle']], p.sf), ST); po.rot = -360 * Math.max(0, Math.min(1, (p.sf - 11) / 17)); break; } po = p.back ? null : kf([[0, 'grabS'], [6, 'thPull', 'o'], [11, 'thKnee', 'i'], [15, 'thPull'], [20, 'thKnee', 'i'], [24, 'thPull'], [30, 'thKick', 'i'], [37, 'thKick'], [44, 'idle']], p.sf); break;
      case 'thrown': { po = norm(P('thrown'), ST); const t = o.sf; po.rot = t < 14 ? -8 : -Math.min(150, 8 + (t - 14) * 14); break; }
      case 'atk': {
        const m = C.mv[p.mv], an = anOf(p.mv), f = p.mf, base = m.isAir ? 'jump' : m.crouch ? 'crouch' : 'idle';
        if (!an) { po = P(base); break; }
        if (an.f) { const ck = p.ch + ':' + p.mv, keys = fcache[ck] || (fcache[ck] = an.f(m)); po = kf(keys, f); }
        else { const su = Math.max(1, m.su - 1), la = Math.max(su, m.lastAct), T = m.t < 900 ? m.t : la + 24; po = kf(an.k, f <= su ? f / su : f <= la ? 1 + (la > su ? (f - su) / (la - su) : 1) : 2 + Math.min(1, (f - la) / Math.max(1, T - la))); }
        if (an.spin) { const a = Math.max(0, m.su - 4), b = m.lastAct + 4, tot = Math.max(1, Math.round((b - a) * 27 / 360)) * 360; yaw = tot * Math.max(0, Math.min(1, (f - a) / (b - a))); }
        if (an.yaw) { const y = an.yaw(m); yaw = y[2] * Math.max(0, Math.min(1, (f - y[0]) / Math.max(1, y[1] - y[0]))); }
        if (po.aim) { let hh = null; for (const h of m.hits) if (h.hb) { hh = h; if (f <= h.e + 5) break; } if (hh) hb = hh.hb; }
        break;
      }
      default: po = P('idle');
    }
    return { po: po, hb: hb, yaw: yaw, air: p.y > 0 };
  }

  // ----------------------------------------------------------- rig
  const LOOK = [
    { top: 0xf1efe6, pants: 0xe9e6da, skin: 0xe6b088, hair: 0x1a1418, glove: 0xd01f2e, belt: 0x18181c, band: 0xd01f2e, shoe: 0xe6b088, trim: 0xd01f2e },
    { top: 0x1c1f2e, pants: 0x2848d0, skin: 0xd9a27c, hair: 0xf3dc4a, glove: 0xffd21e, belt: 0xffd21e, band: 0, shoe: 0xf4f4f4, trim: 0xffd21e }
  ];
  const ALT = [{ top: 0x27357a, pants: 0x1f2a63, band: 0xf2c230, glove: 0xf2c230, trim: 0xf2c230 }, { top: 0xb3202c, pants: 0x25252c, hair: 0xdfe6ee, glove: 0xffffff, belt: 0xffffff, trim: 0xffffff }];
  // ----------------------------------------------------------- skinned humanoid body, driven by the same pose targets
  const MS = 51;   // world units per metre of the model
  const MDL = (function () {
    if (typeof GK_MODEL === 'undefined' || typeof atob === 'undefined') return null;
    try {
      const M = GK_MODEL, bin = atob(M.b64), u8 = new Uint8Array(bin.length); for (let i = 0; i < bin.length; i++) u8[i] = bin.charCodeAt(i);
      const nv = M.nv, dv = new DataView(u8.buffer); let o = 0;
      const pos = new Float32Array(nv * 3); for (let i = 0; i < nv * 3; i++) pos[i] = dv.getInt16(o + i * 2, true) / 16000; o += nv * 6;
      const nor = new Float32Array(nv * 3); for (let i = 0; i < nv * 3; i++) nor[i] = dv.getInt8(o + i) / 127; o += nv * 3;
      const si = new Uint16Array(nv * 4); for (let i = 0; i < nv * 4; i++) si[i] = u8[o + i]; o += nv * 4;
      const sw = new Float32Array(nv * 4); for (let i = 0; i < nv * 4; i++) sw[i] = u8[o + i] / 255; o += nv * 4;
      const cat = u8.slice(o, o + nv); o += nv;
      const idx = new Uint16Array(M.ni); for (let i = 0; i < M.ni; i++) idx[i] = dv.getUint16(o + i * 2, true);
      const id = {}, world = []; M.bones.forEach((b, i) => { id[b[0]] = i; });
      const wp = i => world[i] || (world[i] = (() => { const b = M.bones[i], p = b[1] >= 0 ? wp(b[1]) : [0, 0, 0]; return [p[0] + b[2], p[1] + b[3], p[2] + b[4]]; })());
      M.bones.forEach((b, i) => wp(i));
      const gain = M.bones.map(b => /(Arm|ForeArm)$/.test(b[0]) ? 0.22 : /UpLeg$/.test(b[0]) ? 0.16 : /Leg$/.test(b[0]) ? 0.2 : /Shoulder$/.test(b[0]) ? 0.1 : 0), kind = M.bones.map(b => /Arm|Shoulder/.test(b[0]) ? 1 : /Leg/.test(b[0]) ? 2 : 0);
      for (let i = 0; i < nv; i++) { let g = 0, k = 0, bi = 0, bw = 0; for (let c = 0; c < 4; c++) { const j = si[i * 4 + c], w = sw[i * 4 + c]; g += gain[j] * w; if (w > bw && kind[j]) { bw = w; bi = j; k = kind[j]; } } if (g <= 0 || !k) continue; const c0 = world[bi];
        if (k === 1) { pos[i * 3 + 1] = c0[1] + (pos[i * 3 + 1] - c0[1]) * (1 + g); pos[i * 3 + 2] = c0[2] + (pos[i * 3 + 2] - c0[2]) * (1 + g); } else { pos[i * 3] = c0[0] + (pos[i * 3] - c0[0]) * (1 + g); pos[i * 3 + 2] = c0[2] + (pos[i * 3 + 2] - c0[2]) * (1 + g); } }
      const off = n => { const b = M.bones[id[n]]; return new THREE.Vector3(b[2] * MS, b[3] * MS, b[4] * MS); };
      return { nv, pos, nor, si, sw, cat, idx, bones: M.bones, id, world, off };
    } catch (e) { return null; }
  })();
  const _q = new THREE.Quaternion(), _m4 = new THREE.Matrix4(), _a = new THREE.Vector3(), _b = new THREE.Vector3(), _c = new THREE.Vector3();
  // orientation whose Y axis is dir and whose Z axis is as close to ref as possible (model space)
  function basisQ(dir, ref, out) {
    _a.copy(dir).normalize(); _b.copy(ref).addScaledVector(_a, -ref.dot(_a)); if (_b.lengthSq() < 1e-6) { _b.set(0.3, 0.2, 1); _b.addScaledVector(_a, -_b.dot(_a)); } _b.normalize(); _c.crossVectors(_a, _b);
    return out.setFromRotationMatrix(_m4.makeBasis(_c, _a, _b));
  }
  const V = (x, y, z) => new THREE.Vector3(x, y, z), toM = p => V(-p[2], p[1], p[0]);   // game space -> model space
  const QB = MDL ? { sp: basisQ(V(0, 1, 0), V(-1, 0, 0), new THREE.Quaternion()).invert(), aL: basisQ(V(1, 0, 0), V(0, -1, 0), new THREE.Quaternion()).invert(), aR: basisQ(V(-1, 0, 0), V(0, 1, 0), new THREE.Quaternion()).invert(), lg: basisQ(V(0, -1, 0), V(1, 0, 0), new THREE.Quaternion()).invert(), ft: basisQ(V(0, -8.729, 10.711), V(1, 0, 0), new THREE.Quaternion()).invert() } : null;
  const FTP = Math.atan2(8.729, 10.711);
  function buildRigM(ch, alt) {
    const lk = Object.assign({}, LOOK[ch], alt ? ALT[ch] : {}), nv = MDL.nv;
    const keys = ['skin', 'top', 'pants', 'belt', 'glove', 'shoe', 'hair', ch === 0 ? 'band' : 'hair', ch === 0 ? 'skin' : 'trim'], cols = keys.map(k => new THREE.Color(lk[k]));
    const col = new Float32Array(nv * 3); for (let i = 0; i < nv; i++) { const c = cols[MDL.cat[i]]; col[i * 3] = c.r; col[i * 3 + 1] = c.g; col[i * 3 + 2] = c.b; }
    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.BufferAttribute(MDL.pos, 3)); geo.setAttribute('normal', new THREE.BufferAttribute(MDL.nor, 3)); geo.setAttribute('color', new THREE.BufferAttribute(col, 3));
    geo.setAttribute('skinIndex', new THREE.BufferAttribute(MDL.si, 4)); geo.setAttribute('skinWeight', new THREE.BufferAttribute(MDL.sw, 4)); geo.setIndex(new THREE.BufferAttribute(MDL.idx, 1));
    const mat = new THREE.MeshLambertMaterial({ vertexColors: true, skinning: true });
    const ol = new THREE.MeshBasicMaterial({ color: 0x0a0810, side: THREE.BackSide, skinning: true });
    ol.onBeforeCompile = sh => { sh.vertexShader = sh.vertexShader.replace('#include <begin_vertex>', 'vec3 transformed = position + normal * 0.016;'); };
    const bones = MDL.bones.map(b => { const o = new THREE.Bone(); o.name = b[0]; o.position.set(b[2], b[3], b[4]); return o; }), inv = [];
    const mesh = new THREE.SkinnedMesh(geo, mat), out = new THREE.SkinnedMesh(geo, ol); mesh.frustumCulled = false; out.frustumCulled = false;
    MDL.bones.forEach((b, i) => { if (b[1] >= 0) bones[b[1]].add(bones[i]); else mesh.add(bones[i]); const w = MDL.world[i]; inv.push(new THREE.Matrix4().makeTranslation(-w[0], -w[1], -w[2])); });
    const sk = new THREE.Skeleton(bones, inv); mesh.bind(sk, new THREE.Matrix4()); out.bind(sk, new THREE.Matrix4());
    const B = {}; bones.forEach(o => { B[o.name] = o; });
    // fists: fingers folded away inside a glove
    const gm = new THREE.MeshLambertMaterial({ color: lk.glove }), gg = new THREE.IcosahedronGeometry(0.066, 1), go = new THREE.MeshBasicMaterial({ color: 0x0a0810, side: THREE.BackSide });
    for (const o of bones) { if (/^(Left|Right)Hand(Index|Middle|Ring|Pinky|Thumb)1$/.test(o.name)) o.scale.setScalar(0.01);
      if (o.name === 'LeftHand' || o.name === 'RightHand') { const g = new THREE.Mesh(gg, gm); g.scale.set(1.3, 1.02, 1.12); g.position.set(o.name === 'LeftHand' ? 0.07 : -0.07, 0, 0.005); const g2 = new THREE.Mesh(gg, go); g2.scale.setScalar(1.2); g.add(g2); o.add(g); } }
    const G = new THREE.Group(), M = new THREE.Group(), R = new THREE.Group(), W = new THREE.Group(), inner = new THREE.Group(); G.add(M); M.add(R); R.rotation.order = 'ZYX'; R.add(W); W.add(inner);
    const hw = MDL.world[MDL.id.Hips]; W.rotation.y = Math.PI / 2; W.scale.setScalar(MS); inner.position.set(-hw[0], -hw[1], -hw[2]); inner.add(mesh); inner.add(out);
    const O = {}; for (const n of ['Spine', 'Spine1', 'Spine2', 'Neck', 'Head', 'LeftShoulder', 'LeftArm', 'LeftForeArm', 'LeftHand', 'RightShoulder', 'RightArm', 'RightForeArm', 'RightHand', 'LeftUpLeg', 'LeftLeg', 'LeftFoot', 'RightUpLeg', 'RightLeg', 'RightFoot']) O[n] = MDL.off(n);
    const len = { UA: O.LeftForeArm.length(), FA: O.LeftHand.length(), TH: O.LeftLeg.length(), SH: O.LeftFoot.length(), AH: (MDL.world[MDL.id.LeftFoot][1]) * MS };
    return { G, M, R, sk: 1, B, O, len, mats: { body: mat, glove: gm }, q: { H: new THREE.Quaternion(), S: new THREE.Quaternion(), S2: new THREE.Quaternion(), N: new THREE.Quaternion(), a: new THREE.Quaternion(), f: new THREE.Quaternion(), t: new THREE.Quaternion(), i: new THREE.Quaternion() } };
  }
  function applyPoseM(rig, fp, face, flashK, tint) {
    const po = fp.po, B = rig.B, O = rig.O, L = rig.len, q = rig.q, hip = [po.hip[0], po.hip[1], 0];
    const a1 = po.ln * D, a2 = po.ln2 * D, tw = po.tw * D, pt = po.pt * D, cax = [Math.sin(tw), 0, Math.cos(tw)], pax = [Math.sin(pt), 0, Math.cos(pt)], mid = [(cax[0] + pax[0]) / 2, 0, (cax[2] + pax[2]) / 2];
    const ha = a2 * 0.45 + po.hd * D, hz = [cax[0] * 0.3, 0, 0.7 + cax[2] * 0.3];
    basisQ(toM([Math.sin(a1 * 0.5), Math.cos(a1 * 0.5), 0]), toM(pax), q.H).multiply(QB.sp);
    basisQ(toM([Math.sin(a1), Math.cos(a1), 0]), toM(mid), q.S).multiply(QB.sp);
    basisQ(toM([Math.sin(a2), Math.cos(a2), 0]), toM(cax), q.S2).multiply(QB.sp);
    basisQ(toM([Math.sin(ha), Math.cos(ha), 0]), toM(hz), q.N).multiply(QB.sp);
    B.Hips.quaternion.copy(q.H); B.Spine.quaternion.copy(q.i.copy(q.H).invert()).multiply(q.S); B.Spine1.quaternion.set(0, 0, 0, 1);
    B.Spine2.quaternion.copy(q.i.copy(q.S).invert()).multiply(q.S2); B.Neck.quaternion.copy(q.i.copy(q.S2).invert()).multiply(q.N); B.Head.quaternion.set(0, 0, 0, 1);
    // where the model's own shoulder and hip joints end up (model space, relative to the hips), then back to game space
    const pS2 = O.Spine.clone().applyQuaternion(q.H).add(O.Spine1.clone().applyQuaternion(q.S)).add(O.Spine2.clone().applyQuaternion(q.S));
    const mine = v => [hip[0] + v.z, hip[1] + v.y, -v.x];
    const tg = {}; for (const n of ['hL', 'hR', 'fL', 'fR']) tg[n] = po[n].slice();
    if (po.aim && fp.hb && po.aw > 0) { const h = fp.hb, t = tg[po.aim], tx = po.up ? h[0] + h[2] * 0.6 : h[0] + h[2] - 4, ty = po.up ? h[1] + h[3] - 6 : h[1] + h[3] * po.ay; t[0] += (tx - t[0]) * po.aw; t[1] += (ty - t[1]) * po.aw; }
    for (const n of ['L', 'R']) {
      const sd = n === 'L' ? -1 : 1, S = n === 'L' ? 'Left' : 'Right';
      const sh = mine(pS2.clone().add(O[S + 'Shoulder'].clone().add(O[S + 'Arm']).applyQuaternion(q.S2))), ht = tg['h' + n], e = po['e' + n];
      const ap = [-0.35, e, sd * 0.7 * (e > 0 ? 1.4 : 1)], A = ik(sh, [ht[0], ht[1], sh[2] * 0.45 + ht[2]], L.UA, L.FA, ap, 1.12);
      const hs = mine(O[S + 'UpLeg'].clone().applyQuaternion(q.H)), ft = tg['f' + n], kk = po['k' + n];
      const fty = fp.air ? ft[1] + L.AH - 4 : Math.max(L.AH, ft[1] + L.AH - 4), kp = [0.8, 0.45, sd * (0.4 + kk * 1.6)];
      const Lg = ik(hs, [ft[0], fty, sd * 5 + ft[2]], L.TH, L.SH, kp, 1.12);
      // arm: upper, fore, hand
      const u = V(A[0][0] - sh[0], A[0][1] - sh[1], A[0][2] - sh[2]), f = V(A[1][0] - A[0][0], A[1][1] - A[0][1], A[1][2] - A[0][2]);
      const ax = toM([-(u.y * ap[2] - u.z * ap[1]), -(u.z * ap[0] - u.x * ap[2]), -(u.x * ap[1] - u.y * ap[0])]), qb = n === 'L' ? QB.aL : QB.aR;
      basisQ(toM([u.x, u.y, u.z]), ax, q.a).multiply(qb); B[S + 'Arm'].quaternion.copy(q.i.copy(q.S2).invert()).multiply(q.a);
      basisQ(toM([f.x, f.y, f.z]), ax, q.f).multiply(qb); B[S + 'ForeArm'].quaternion.copy(q.i.copy(q.a).invert()).multiply(q.f);
      B[S + 'ForeArm'].position.x = O[S + 'ForeArm'].x / MS * (u.length() / L.UA); B[S + 'Hand'].position.x = O[S + 'Hand'].x / MS * (f.length() / L.FA);
      // leg: thigh, shin, foot
      const th = V(Lg[0][0] - hs[0], Lg[0][1] - hs[1], Lg[0][2] - hs[2]), sn = V(Lg[1][0] - Lg[0][0], Lg[1][1] - Lg[0][1], Lg[1][2] - Lg[0][2]);
      const kx = toM([-(th.y * kp[2] - th.z * kp[1]), -(th.z * kp[0] - th.x * kp[2]), -(th.x * kp[1] - th.y * kp[0])]);
      basisQ(toM([th.x, th.y, th.z]), kx, q.a).multiply(QB.lg); B[S + 'UpLeg'].quaternion.copy(q.i.copy(q.H).invert()).multiply(q.a);
      basisQ(toM([sn.x, sn.y, sn.z]), kx, q.f).multiply(QB.lg); B[S + 'Leg'].quaternion.copy(q.i.copy(q.a).invert()).multiply(q.f);
      B[S + 'Leg'].position.y = O[S + 'Leg'].y / MS * (th.length() / L.TH); B[S + 'Foot'].position.y = O[S + 'Foot'].y / MS * (sn.length() / L.SH);
      const sl = Math.hypot(sn.x, sn.y) || 1, k = fp.air ? 1 : Math.max(0, Math.min(1, (Lg[1][1] - L.AH - 1) / 8));
      const fx0 = sd > 0 && !fp.air ? 0.8 : 1, t = toM([fx0 * (1 - k) + (-sn.y / sl * 0.8 + sn.x / sl * 0.45) * k, (sn.x / sl * 0.8 + sn.y / sl * 0.45) * k, (sd > 0 ? 0.6 : -0.15) * (1 - k)]).normalize();
      const nn = new THREE.Vector3().crossVectors(t, kx.addScaledVector(t, -kx.dot(t)).normalize()), dir = t.clone().multiplyScalar(Math.cos(FTP)).addScaledVector(nn, -Math.sin(FTP));
      basisQ(dir, kx, q.t).multiply(QB.ft); B[S + 'Foot'].quaternion.copy(q.i.copy(q.f).invert()).multiply(q.t);
    }
    rig.R.position.set(hip[0], hip[1], 0); rig.R.rotation.set(0, (po.yaw + fp.yaw) * D, -po.rot * D); rig.M.scale.x = face;
    for (const k in rig.mats) rig.mats[k].emissive.setRGB(flashK + tint[0], flashK + tint[1], flashK + tint[2]);
  }

  // Chunky vinyl-figure fighters: big head, barrel body, fat limbs, oversized fists and boots, thick ink outline.
  const TOY = [
    { skin: 0xe8b282, hair: 0x2a2e3b, side: 0x7f828a, beard: 0xa39c8e, tache: 0x1f2027, brow: 0x1f2027, top: 0x6f6d3a, top2: 0x5b5930, shirt: 0x6c5847, belt: 0x34261d, buckle: 0xaab0b6, pants: 0x6d5c4d, cuff: 0x7d6b5a, boot: 0x2c1f18, sole: 0x1a120e, btn: 0x3a3320 },
    { skin: 0xe6ab78, hair: 0x6d472b, side: 0x8a5d3a, beard: 0x4b2d1b, tache: 0x4b2d1b, brow: 0x3b2415, top: 0xf0e7d3, top2: 0xd9cfb8, shirt: 0xf0e7d3, belt: 0x3f362e, buckle: 0x5a5046, pants: 0x3f362e, cuff: 0x574b40, boot: 0x271d17, sole: 0x17110d, btn: 0xb9b4a8, strap: 0xa3936c, strap2: 0x6d5f47 }
  ];
  const TOYALT = [{ top: 0x2f4a6e, top2: 0x263c5a, shirt: 0x8a7a66, pants: 0x3b3f4a, cuff: 0x4a4f5c }, { top: 0xc9d8e6, top2: 0xb0c2d3, shirt: 0xc9d8e6, pants: 0x4a2f2a, belt: 0x4a2f2a, cuff: 0x5e3d36, strap: 0xc7a14a }];
  function buildRig(ch, alt) {
    const lk = Object.assign({}, TOY[ch], alt ? TOYALT[ch] : {}), mats = {};
    const ramp = new THREE.DataTexture(new Uint8Array([120, 120, 120, 255, 190, 190, 190, 255, 255, 255, 255, 255]), 3, 1, THREE.RGBAFormat); ramp.minFilter = ramp.magFilter = THREE.NearestFilter; ramp.needsUpdate = true;
    const mat = k => mats[k] || (mats[k] = new THREE.MeshToonMaterial({ color: lk[k], gradientMap: ramp }));
    const ink = new THREE.MeshBasicMaterial({ color: 0x0c0810, side: THREE.BackSide }), inkN = new THREE.MeshBasicMaterial({ color: 0x0c0810, side: THREE.BackSide });
    inkN.onBeforeCompile = sh => { sh.vertexShader = sh.vertexShader.replace('#include <begin_vertex>', 'vec3 transformed = position + normal * 1.7;'); };
    const G = new THREE.Group(), M = new THREE.Group(), R = new THREE.Group(); G.add(M); M.add(R); R.rotation.order = 'ZYX';
    const add = (g, k, par, noInk) => { const m = new THREE.Mesh(g, mat(k)); (par || R).add(m); if (!noInk) m.add(new THREE.Mesh(g, inkN)); return m; };
    // rounded shapes get a smooth ink shell pushed out along their normals; boxes get a scaled shell
    const sph = (k, r, sx, sy, sz, par, x, y, z, noInk) => { const m = add(new THREE.IcosahedronGeometry(r, 2), k, par, noInk); m.scale.set(sx, sy, sz); m.position.set(x || 0, y || 0, z || 0); return m; };
    const cyl = (k, r0, r1, len, par, noInk) => { const g = new THREE.CylinderGeometry(r1, r0, len, 12); g.translate(0, len / 2, 0); return add(g, k, par, noInk); };
    const cap = (k, r, t0, t1, par) => add(new THREE.SphereGeometry(r, 16, 10, 0, Math.PI * 2, Math.PI * t0, Math.PI * (t1 - t0)), k, par);
    const box = (k, w, h, d, par, x, y, z, noInk) => { const g = new THREE.BoxGeometry(w, h, d), m = new THREE.Mesh(g, mat(k)); (par || R).add(m); m.position.set(x || 0, y || 0, z || 0); if (!noInk) { const o = new THREE.Mesh(g, ink); o.scale.set((w + 1.8) / w, (h + 1.8) / h, (d + 1.8) / d); m.add(o); } return m; };
    const b = {}, DX = 0.72;   // body depth as a fraction of its width
    // ---- hips and legs
    b.pel = cyl('pants', 11.2, 11.6, 9); b.pel.scale.set(DX, 1, 1);
    // ---- torso (local x = chest forward, y = up the spine, z = across the shoulders)
    b.abd = cyl(ch === 0 ? 'top' : 'pants', 11.6, 12.2, SP1 - 2); b.abd.scale.set(DX, 1, 1);
    b.chest = cyl('top', 12.2, 11.2, SP2); b.chest.scale.set(DX, 1, 1); sph('top', 11.2, 1, 0.42, 1, b.chest, 0, SP2, 0);
    if (ch === 0) {
      const skirt = cyl('top', 13.2, 12, 8, b.pel); skirt.position.y = 2.5;                                   // jacket hem over the hips
      box('shirt', 2, SP2 + 4, 8.5, b.chest, 12.2, SP2 / 2 - 2, 0, 1); box('top2', 2.4, SP2 - 1, 3, b.chest, 12.4, SP2 / 2 + 1.5, 5.4).rotation.x = 0.18; box('top2', 2.4, SP2 - 1, 3, b.chest, 12.4, SP2 / 2 + 1.5, -5.4).rotation.x = -0.18;
      for (let i = 0; i < 3; i++) sph('btn', 1.15, 1, 1, 1, b.chest, 12.9, 9 - i * 5.2, 8.4, 1);
      const bl = cyl('belt', 12.3, 12.3, 2.6, b.abd, 1); bl.position.y = 0.5; box('buckle', 1.6, 3.4, 4.6, b.abd, 12.9, 1.8, 0);
    } else {
      const wb = cyl('belt', 12.5, 12.5, 3, b.abd, 1); wb.position.y = SP1 - 5; sph('btn', 1.1, 1, 1, 1, b.abd, 12.7, SP1 - 3.5, 0, 1);
      for (const z of [-5.6, 5.6]) { box('strap', 1.4, SP2 + 3, 3, b.chest, 12.3, SP2 / 2 - 1.5, z); box('strap2', 1.6, SP2 + 3, 0.9, b.chest, 12.4, SP2 / 2 - 1.5, z, 1); box('strap', 1.4, SP2 + 3, 3, b.chest, -12.3, SP2 / 2 - 1.5, z); box('buckle', 1.8, 3, 3.6, b.chest, 12.9, -2.5, z); }
      sph('btn', 1, 1, 1, 1, b.chest, 12.6, 9, 0, 1); sph('btn', 1, 1, 1, 1, b.chest, 12.6, 4, 0, 1); box('top2', 1.2, 3, 9, b.chest, 12, SP2 - 1, 0, 1);
    }
    // ---- head (local x = face forward, y = up, z = toward the near ear)
    const hd = b.head = new THREE.Group(); R.add(hd); const HY = 11.5;
    sph('skin', 12.5, 0.96, 0.93, 1, hd, 0, HY, 0);
    for (const z of [-1, 1]) { sph('skin', 4.2, 0.6, 1, 0.6, hd, -2.5, HY - 2, z * 12.9); }
    for (const z of [-4.7, 4.7]) sph('tache', 1.75, 0.55, 1.5, 0.95, hd, 10.9, HY + 1.2, z, 1);                // eyes
    const brow = (z, tilt, th) => { const m = box('brow', 1.5, th, 5.6, hd, 11, HY + 5.4, z, 1); m.rotation.x = tilt; m.rotation.y = z > 0 ? -0.35 : 0.35; return m; };
    if (ch === 0) {
      brow(4.9, -0.2, 1.7); brow(-4.9, 0.2, 1.7);
      const bd = cap('beard', 12.75, 0.5, 1, hd); bd.position.set(0.25, HY - 0.2, 0); bd.scale.set(0.97, 0.94, 1.01);                  // stubble
      sph('skin', 4.6, 0.5, 0.7, 1, hd, 10, HY - 5.6, 0, 1);                                                                       // clean patch round the mouth
      const m1 = box('tache', 1.7, 2.3, 5.4, hd, 11.5, HY - 3.1, 2.4, 1); m1.rotation.x = 0.32; const m2 = box('tache', 1.7, 2.3, 5.4, hd, 11.5, HY - 3.1, -2.4, 1); m2.rotation.x = -0.32;
      box('side', 0.8, 0.7, 3, hd, 11.9, HY - 6.2, 0, 1);
      const hc = cap('hair', 13.1, 0, 0.36, hd); hc.position.set(-1.6, HY - 0.2, 0); hc.scale.set(0.98, 0.98, 1.01);
      for (const z of [-1, 1]) sph('side', 3.4, 1.4, 1, 0.3, hd, -1.5, HY + 4.2, z * 11.6, 1);
      sph('hair', 6.4, 1.55, 0.72, 1.35, hd, 2.5, HY + 12.6, 0); sph('hair', 3.4, 1.3, 0.9, 1.1, hd, 10.8, HY + 14.3, -1.5).rotation.z = 0.5;
    } else {
      brow(4.9, 0.36, 2.4); brow(-4.9, -0.36, 2.4);
      const bd = cap('beard', 13.2, 0.46, 1, hd); bd.position.set(0.5, HY - 0.6, 0); bd.scale.set(1, 1.02, 1.03);
      sph('beard', 8.6, 0.95, 0.8, 1.25, hd, 4.8, HY - 10.6, 0);
      box('beard', 1.8, 2.6, 9.6, hd, 11.6, HY - 3.2, 0, 1);
      sph('skin', 3.6, 0.45, 0.6, 1.15, hd, 11.2, HY - 5.6, 0, 1); const f1 = box('brow', 0.9, 0.8, 2.3, hd, 12.5, HY - 5.2, 1, 1); f1.rotation.x = -0.45; const f2 = box('brow', 0.9, 0.8, 2.3, hd, 12.5, HY - 5.2, -1, 1); f2.rotation.x = 0.45;
      for (let i = 0; i < 2; i++) box('side', 0.7, 0.55, 6.5 - i * 1.5, hd, 11.2 - i * 0.5, HY + 7.8 + i * 1.5, 0, 1);
      sph('tache', 0.7, 1, 1, 1, hd, 10.3, HY - 1.2, 7.2, 1);
      const hc = cap('hair', 13.1, 0, 0.37, hd); hc.position.set(-1.6, HY - 0.2, 0); hc.scale.set(0.98, 0.98, 1.01);
      for (const z of [-1, 1]) sph('side', 3.4, 1.4, 1, 0.3, hd, -1.5, HY + 4.2, z * 11.6, 1);
      const q = sph('hair', 6.6, 1.05, 1.5, 1.15, hd, 4.5, HY + 14.5, 0); q.rotation.z = -0.38; sph('hair', 4, 1, 1.25, 1, hd, 9, HY + 19.5, 0.5).rotation.z = -0.6;
    }
    // ---- arms and legs
    for (const n of ['L', 'R']) {
      b['sh' + n] = sph('top', 6.3, 1, 1, 1, R); b['ua' + n] = cyl('top', 5.6, 5.1, UA);
      b['fa' + n] = cyl(ch === 0 ? 'top' : 'skin', 4.9, 5.4, FA); const cf = cyl(ch === 0 ? 'top2' : 'top', 6, 6, 3.4, b['fa' + n]); cf.position.y = ch === 0 ? FA - 4 : -1.2;
      b['fi' + n] = sph('skin', 6.4, 1, 1.05, 1.05, R);
      b['th' + n] = cyl('pants', 7.6, 6.6, TH); b['sn' + n] = cyl('pants', 6.6, 7, SHN); const pc = cyl('cuff', 7.5, 7.5, 5, b['sn' + n]); pc.position.y = SHN - 6;
      const ft = b['ft' + n] = new THREE.Group(); R.add(ft);                                                   // local y = toward the toe, local -x = up
      sph('boot', 6.2, 0.8, 1.55, 1.02, ft, -0.6, 7.5, 0); sph('boot', 5.6, 1, 1, 1, ft, -1.6, 3.4, 0); box('sole', 1.6, 16.5, 9.6, ft, 3.2, 7.6, 0);
    }
    return { G, M, R, b, mats };
  }
  const _X = new THREE.Vector3(), _Y = new THREE.Vector3(), _Z = new THREE.Vector3(), _M = new THREE.Matrix4();
  // put a part with its base at p0, its long axis along p0->p1 and its width axis as close to zh as possible
  function place(m, p0, p1, rest, zh, off) {
    _Y.set(p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]); const len = _Y.length() || 1e-3; _Y.multiplyScalar(1 / len);
    _Z.set(zh[0], zh[1], zh[2]); _Z.addScaledVector(_Y, -_Z.dot(_Y)); if (_Z.lengthSq() < 1e-4) { _Z.set(1, 0, 0); _Z.addScaledVector(_Y, -_Z.dot(_Y)); } _Z.normalize(); _X.crossVectors(_Y, _Z);
    m.quaternion.setFromRotationMatrix(_M.makeBasis(_X, _Y, _Z)); m.position.set(p0[0], p0[1], p0[2]); if (off) m.position.addScaledVector(_Y, off); m.scale.y = rest ? len / rest : 1;
  }
  function ik(r, t, a, b, pole, maxS) {
    let dx = t[0] - r[0], dy = t[1] - r[1], dz = t[2] - r[2], d = Math.hypot(dx, dy, dz) || 1e-3; const L = (a + b) * 0.995; let sc = 1;
    if (d > L) { sc = Math.min(maxS, d / L); if (d > L * maxS) { const q = L * maxS / d; dx *= q; dy *= q; dz *= q; d = L * maxS; } }
    const Aa = a * sc, Bb = b * sc, ux = dx / d, uy = dy / d, uz = dz / d, pr = Math.max(-Aa, Math.min(Aa, (Aa * Aa - Bb * Bb + d * d) / (2 * d))), h = Math.sqrt(Math.max(0, Aa * Aa - pr * pr));
    let px = pole[0], py = pole[1], pz = pole[2]; const dot = px * ux + py * uy + pz * uz; px -= dot * ux; py -= dot * uy; pz -= dot * uz; const pl = Math.hypot(px, py, pz) || 1;
    return [[r[0] + ux * pr + px / pl * h, r[1] + uy * pr + py / pl * h, r[2] + uz * pr + pz / pl * h], [r[0] + dx, r[1] + dy, r[2] + dz]];
  }
  function applyPose(rig, fp, face, flashK, tint) {
    if (rig.sk) return applyPoseM(rig, fp, face, flashK, tint);
    const po = fp.po, b = rig.b, hip = [po.hip[0], po.hip[1], 0];
    const a1 = po.ln * D, a2 = po.ln2 * D, cb = [hip[0] + Math.sin(a1) * SP1, hip[1] + Math.cos(a1) * SP1, 0], nk = [cb[0] + Math.sin(a2) * SP2, cb[1] + Math.cos(a2) * SP2, 0];
    const tw = po.tw * D, pt = po.pt * D, cax = [Math.sin(tw), 0, Math.cos(tw)], pax = [Math.sin(pt), 0, Math.cos(pt)], mid = [(cax[0] + pax[0]) / 2, 0, (cax[2] + pax[2]) / 2];
    const sy = [nk[0] - Math.sin(a2) * 3, nk[1] - Math.cos(a2) * 3];
    const tg = {}; for (const n of ['hL', 'hR', 'fL', 'fR']) tg[n] = po[n].slice();
    if (po.aim && fp.hb && po.aw > 0) { const h = fp.hb, t = tg[po.aim], tx = po.up ? h[0] + h[2] * 0.6 : h[0] + h[2] - 3, ty = po.up ? h[1] + h[3] - 5 : h[1] + h[3] * po.ay; t[0] += (tx - t[0]) * po.aw; t[1] += (ty - t[1]) * po.aw; }
    const lim = {};
    for (const n of ['L', 'R']) {
      const sd = n === 'L' ? -1 : 1, sh = [sy[0] + cax[0] * SW * sd, sy[1], cax[2] * SW * sd], ht = tg['h' + n], e = po['e' + n];
      const A = ik(sh, [ht[0], ht[1], sh[2] * 0.45 + ht[2]], UA, FA, [-0.35, e, sd * 0.7 * (e > 0 ? 1.4 : 1)], 1.3);
      const hs = [hip[0] + pax[0] * HW * sd, hip[1] - 2, pax[2] * HW * sd], ft = tg['f' + n], kk = po['k' + n];
      const fty = fp.air ? ft[1] : Math.max(4, ft[1]);
      const Lg = ik(hs, [ft[0], fty, sd * 5 + ft[2]], TH, SHN, [0.8, 0.45, sd * (0.4 + kk * 1.6)], 1.32);
      lim[n] = { sh: sh, el: A[0], wr: A[1], hs: hs, kn: Lg[0], an: Lg[1] };
    }
    const sub = (p) => [p[0] - hip[0], p[1] - hip[1], p[2]], Zc = [0, 0, 1];
    const d0 = [Math.sin(a1 * 0.5), Math.cos(a1 * 0.5), 0];
    place(b.pel, sub([hip[0] - d0[0] * 5, hip[1] - d0[1] * 5, 0]), sub([hip[0] + d0[0] * 4, hip[1] + d0[1] * 4, 0]), 0, pax);
    place(b.abd, sub([hip[0] + d0[0] * 2, hip[1] + d0[1] * 2, 0]), sub(cb), SP1 - 2, mid);
    place(b.chest, sub(cb), sub(nk), SP2, cax);
    const ha = a2 * 0.45 + po.hd * D, back = Math.max(0, Math.min(1, (Math.abs(po.tw) - 100) / 50)), hz = [-0.5 + back * 0.9 + cax[0] * 0.12, 0, 0.82];
    place(b.head, sub([nk[0], nk[1] - 3.5, 0]), sub([nk[0] + Math.sin(ha) * 10, nk[1] - 3.5 + Math.cos(ha) * 10, 0]), 0, hz);
    for (const n of ['L', 'R']) { const l = lim[n], sd = n === 'L' ? -1 : 1;
      place(b['sh' + n], sub(l.sh), sub(l.el), 0, cax, 0); place(b['ua' + n], sub(l.sh), sub(l.el), UA, Zc); place(b['fa' + n], sub(l.el), sub(l.wr), FA, Zc);
      const fd = [l.wr[0] - l.el[0], l.wr[1] - l.el[1], l.wr[2] - l.el[2]]; place(b['fi' + n], sub(l.wr), sub([l.wr[0] + fd[0], l.wr[1] + fd[1], l.wr[2] + fd[2]]), 0, Zc, 2.5);
      place(b['th' + n], sub(l.hs), sub(l.kn), TH, Zc); place(b['sn' + n], sub(l.kn), sub(l.an), SHN, Zc);
      // foot: flat on the floor, turning to follow the shin once it leaves the ground
      const sx = l.an[0] - l.kn[0], sy2 = l.an[1] - l.kn[1], sl = Math.hypot(sx, sy2) || 1, k = fp.air ? 1 : Math.max(0, Math.min(1, (l.an[1] - 5) / 8));
      const fx0 = sd > 0 && !fp.air ? 0.8 : 1, tx = fx0 * (1 - k) + (-sy2 / sl * 0.85 + sx / sl * 0.35) * k, ty = (sx / sl * 0.85 + sy2 / sl * 0.35) * k, tz = (sd > 0 ? 0.6 : -0.15) * (1 - k);
      const a0 = [l.an[0], l.an[1] - 0.4, l.an[2]]; place(b['ft' + n], sub(a0), sub([a0[0] + tx, a0[1] + ty, a0[2] + tz]), 0, [0, 0, 1], -4.5);
    }
    rig.R.position.set(hip[0], hip[1], 0); rig.R.rotation.set(0, (po.yaw + fp.yaw) * D, -po.rot * D); rig.M.scale.x = face;
    for (const k in rig.mats) { const m = rig.mats[k]; m.emissive.setRGB(flashK + tint[0], flashK + tint[1], flashK + tint[2]); }
  }

  // ----------------------------------------------------------- stage
  function tex(w, h, fn, rep) { const c = document.createElement('canvas'); c.width = w; c.height = h; fn(c.getContext('2d'), w, h); const t = new THREE.CanvasTexture(c); t.magFilter = THREE.NearestFilter; t.minFilter = THREE.NearestFilter; if (rep) { t.wrapS = t.wrapT = THREE.RepeatWrapping; t.repeat.set(rep[0], rep[1]); } return t; }
  function hsh(n) { n = Math.imul(n ^ 0x9e3779b9, 0x85ebca6b); n ^= n >>> 13; n = Math.imul(n, 0xc2b2ae35); return ((n ^ (n >>> 16)) >>> 0) / 4294967296; }
  function buildStage() {
    const st = new THREE.Group();
    const sky = tex(512, 192, (c, w, h) => {
      const g = c.createLinearGradient(0, 0, 0, h); g.addColorStop(0, '#0b0a24'); g.addColorStop(0.45, '#3b1250'); g.addColorStop(0.72, '#b3243e'); g.addColorStop(0.9, '#f2762a'); g.addColorStop(1, '#ffc24a'); c.fillStyle = g; c.fillRect(0, 0, w, h);
      for (let i = 0; i < 90; i++) { c.fillStyle = 'rgba(255,255,255,' + (0.3 + hsh(i) * 0.6) + ')'; c.fillRect((hsh(i * 3 + 1) * w) | 0, (hsh(i * 7 + 2) * h * 0.5) | 0, 1, 1); }
      c.fillStyle = '#ff2a3c'; c.beginPath(); c.arc(256, 132, 56, 0, 7); c.fill(); c.fillStyle = '#ff5a3a'; c.beginPath(); c.arc(256, 132, 48, 0, 7); c.fill();
      for (let y = 112; y < 190; y += 7) { c.fillStyle = y > 150 ? '#b3243e' : '#7a1a48'; c.globalAlpha = 0.9; c.fillRect(190, y + ((y - 112) / 7 | 0), 132, 1 + ((y - 112) / 22 | 0)); } c.globalAlpha = 1;
      c.fillStyle = '#1a1030'; c.beginPath(); c.moveTo(60, 192); c.lineTo(150, 108); c.lineTo(166, 108); c.lineTo(250, 192); c.fill(); c.fillStyle = '#e8e0f0'; c.beginPath(); c.moveTo(150, 108); c.lineTo(166, 108); c.lineTo(176, 120); c.lineTo(168, 116); c.lineTo(160, 122); c.lineTo(152, 115); c.lineTo(142, 118); c.fill();
      c.fillStyle = '#120a22'; for (let i = 0; i < 46; i++) { const bw = 8 + (hsh(i + 50) * 14 | 0), bh = 14 + (hsh(i + 90) * 46 | 0); const x = i * 11.5; c.fillRect(x, h - bh, bw, bh); c.fillStyle = '#ffd86a'; for (let k = 0; k < 6; k++) if (hsh(i * 13 + k) > 0.5) c.fillRect(x + 2 + (k % 2) * 4, h - bh + 3 + (k >> 1) * 6, 2, 2); c.fillStyle = '#120a22'; }
    });
    const skyM = new THREE.Mesh(new THREE.PlaneGeometry(2600, 975), new THREE.MeshBasicMaterial({ map: sky, fog: false })); skyM.position.set(0, 330, -900); st.add(skyM);
    const floor = tex(64, 64, (c) => { c.fillStyle = '#3a2430'; c.fillRect(0, 0, 64, 64); for (let y = 0; y < 4; y++) for (let x = 0; x < 2; x++) { const o = (y % 2) * 16; c.fillStyle = ['#4a2e3a', '#44293a', '#523244', '#4d2c36'][(x + y * 3) % 4]; c.fillRect(x * 32 + o - 16, y * 16, 31, 15); c.fillRect(x * 32 + o + 48, y * 16, 31, 15); } c.fillStyle = 'rgba(255,190,120,0.08)'; for (let i = 0; i < 50; i++) c.fillRect(hsh(i) * 64 | 0, hsh(i + 9) * 64 | 0, 3, 1); }, [22, 6]);
    const fl = new THREE.Mesh(new THREE.PlaneGeometry(1500, 380), new THREE.MeshLambertMaterial({ map: floor })); fl.rotation.x = -Math.PI / 2; fl.position.set(0, 0, -110); st.add(fl);
    const edge = new THREE.Mesh(new THREE.BoxGeometry(1500, 30, 6), new THREE.MeshLambertMaterial({ color: 0x1c1220 })); edge.position.set(0, -15, 80); st.add(edge);
    const red = new THREE.MeshLambertMaterial({ color: 0xc8202c }), dk = new THREE.MeshLambertMaterial({ color: 0x1a1220 }), stone = new THREE.MeshLambertMaterial({ color: 0x6a5a70 });
    const B = (w, h, d, x, y, z, m) => { const b = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), m); b.position.set(x, y, z); st.add(b); return b; };
    B(18, 190, 18, -120, 95, -250, red); B(18, 190, 18, 120, 95, -250, red); B(330, 16, 22, 0, 196, -250, red); B(370, 12, 26, 0, 212, -250, dk); B(270, 12, 16, 0, 160, -250, red); B(34, 30, 6, 0, 178, -240, dk);
    const sign = (txt, col, x, y, z, w, h) => { const t = tex(64, 128, (c) => { c.fillStyle = '#0c0814'; c.fillRect(0, 0, 64, 128); c.strokeStyle = col; c.lineWidth = 3; c.strokeRect(3, 3, 58, 122); c.fillStyle = col; c.font = "44px 'DotGothic16',sans-serif"; c.textAlign = 'center'; c.textBaseline = 'middle'; [...txt].forEach((s, i) => c.fillText(s, 32, 36 + i * 54)); }); const m = new THREE.Mesh(new THREE.PlaneGeometry(w, h), new THREE.MeshBasicMaterial({ map: t, fog: false })); m.position.set(x, y, z); st.add(m); B(6, y + h / 2, 6, x, (y + h / 2) / 2 - 2, z - 5, dk); return m; };
    st.userData.signs = [sign('激突', '#ff3355', -330, 120, -200, 50, 100), sign('拳闘', '#33e0ff', 330, 120, -200, 50, 100), sign('魂', '#ffd21e', -560, 110, -160, 44, 88), sign('雷', '#ffd21e', 560, 110, -160, 44, 88)];
    st.userData.lamps = [];
    for (const x of [-460, -230, 230, 460]) { B(22, 8, 22, x, 4, -150, stone); B(10, 46, 10, x, 31, -150, stone); const l = new THREE.Mesh(new THREE.BoxGeometry(20, 18, 20), new THREE.MeshBasicMaterial({ color: 0xffb040 })); l.position.set(x, 63, -150); st.add(l); st.userData.lamps.push(l); B(28, 6, 28, x, 75, -150, stone); }
    for (let i = -6; i <= 6; i++) B(10, 34, 10, i * 110, 17, -290, dk); B(1500, 6, 8, 0, 36, -290, dk); B(1500, 6, 8, 0, 20, -290, dk);
    scene.add(st); return st;
  }

  function init(c3, c2) {
    ren = new THREE.WebGLRenderer({ canvas: c3, antialias: false, alpha: false, powerPreference: 'high-performance' }); ren.setPixelRatio(1); ren.setSize(VW, VH, false);
    scene = new THREE.Scene(); scene.background = new THREE.Color(0x0b0a24); scene.fog = new THREE.Fog(0x3b1250, 600, 1500);
    cam = new THREE.PerspectiveCamera(28, 16 / 9, 10, 3000);
    scene.add(new THREE.AmbientLight(0xa090c0, 0.72)); const d = new THREE.DirectionalLight(0xfff0dc, 0.85); d.position.set(-0.4, 1, 1.2); scene.add(d); const rim = new THREE.DirectionalLight(0xff5040, 0.35); rim.position.set(0, 0.4, -1); scene.add(rim);
    stage = buildStage();
    for (let i = 0; i < 2; i++) { const s = new THREE.Mesh(new THREE.CircleGeometry(20, 12), new THREE.MeshBasicMaterial({ color: 0x080410, transparent: true, opacity: 0.55 })); s.rotation.x = -Math.PI / 2; s.position.y = 0.4; scene.add(s); shadows.push(s); }
    g2 = c2.getContext('2d'); g2.imageSmoothingEnabled = false;
  }
  function setFighters(chars) {
    for (const r of rigs) scene.remove(r.G); rigs = [];
    for (let i = 0; i < 2; i++) { const r = buildRig(chars[i], i === 1 && chars[0] === chars[1]); scene.add(r.G); rigs.push(r); }
    fx = []; combo[0] = combo[1] = null; msg = null; superFx = null; shake = 0; flash = 0;
  }
  function toScreen(wx, wy) { const v = new THREE.Vector3(wx, wy, 0).project(cam); return [(v.x + 1) * VW / 2, (1 - v.y) * VH / 2]; }

  // ----------------------------------------------------------- events -> fx
  function events(ev, s) {
    for (const e of ev) {
      const x = (e.x || 0) / F, y = (e.y || 0) / F;
      if (e.t === 'hit' && !e.thr) { const vi = 1 - e.who, v = s.p[vi], am0 = s.p[e.who].st === 'atk' ? SIM.CH[s.p[e.who].ch].mv[s.p[e.who].mv] : null; react[vi] = v.cb ? 'CR' : e.wrong === 'l' || (am0 && am0.hits[0] && am0.hits[0].g === 'l') ? 'LOW' : !am0 || am0.hsp <= 9 ? 'L' : am0.hsp <= 11 ? 'M' : /Kick/.test(am0.n || '') ? 'K' : 'H'; }
      if (e.t === 'hit') { fx.push({ k: 'spark', x, y, lvl: e.lvl, t: 0, ko: e.ko }); if (e.di) { fx.push({ k: 'paint', x, y, t: 0, big: 1 }); fx.push({ k: 'paint', x: x + 7, y: y + 12, t: -3, big: 1 }); fx.push({ k: 'word', x, y: y + 58, t: 0, s: e.crum ? 'CRUMPLE!!' : 'DRIVE IMPACT!', c: '#ff5ad2', z: 16, life: 70 }); shake = 20; flash = 4; } const am = s.p[e.who].st === 'atk' ? SIM.CH[s.p[e.who].ch].mv[s.p[e.who].mv] : null; if (am && am.sup && e.lvl === 2) { fx.push({ k: 'kanji', t: 0, s: ['', '\u65ac', '\u7834', '\u6ec5'][am.sa], c: ['', '#33e0ff', '#ffd21e', '#ff3355'][am.sa] }); shake = 20; flash = 5; } if (e.wrong) fx.push({ k: 'word', x, y: y + 30, t: 0, s: e.wrong === 'h' ? 'OVERHEAD!' : 'LOW!', c: e.wrong === 'h' ? '#ff8a1e' : '#33a0ff' });  shake = Math.max(shake, e.ko ? 22 : [3, 7, 11][e.lvl]); if (e.lvl >= 2 || e.ko) flash = e.ko ? 6 : 2; combo[e.who] = { n: e.combo, d: e.total || e.dmg, t: 0, ch: e.ch }; }
      else if (e.t === 'block') { fx.push({ k: 'guard', x, y, t: 0 }); shake = Math.max(shake, 2); }
      else if (e.t === 'parry') fx.push({ k: 'guard', x, y, t: 0, big: 1 });
      else if (e.t === 'pparry') { fx.push({ k: 'guard', x, y, t: 0, big: 1 }); fx.push({ k: 'ring', x, y, t: 0 }); fx.push({ k: 'word', x, y: y + 40, t: 0, s: 'PERFECT PARRY', c: '#7fdcff' }); flash = 2; }
      else if (e.t === 'armor') { fx.push({ k: 'guard', x, y, t: 0, big: 1 }); fx.push({ k: 'word', x, y: y + 34, t: 0, s: 'ARMOR', c: '#ffd21e' }); shake = Math.max(shake, 6); }
      else if (e.t === 'splat') { fx.push({ k: 'paint', x, y, t: 0 }); fx.push({ k: 'word', x: x - 30, y: y + 50, t: 0, s: e.stun ? 'STUN!' : 'WALL SPLAT', c: '#ff3355' }); shake = 14; flash = 2; }
      else if (e.t === 'burnout') { const p = s.p[e.who]; fx.push({ k: 'word', x: p.x / F, y: 110, t: 0, s: 'BURNOUT', c: '#b8b0c0' }); }
      else if (e.t === 'rush') { const p = s.p[e.who]; fx.push({ k: 'dust', x: p.x / F, y: 0, t: 0, big: 1 }); fx.push({ k: 'word', x: p.x / F, y: 104, t: 0, s: 'DRIVE RUSH', c: '#5dffa0' }); }
      else if (e.t === 'di') { fx.push({ k: 'distart', who: e.who, t: 0 }); shake = Math.max(shake, 4); }
      else if (e.t === 'diclash') { fx.push({ k: 'paint', x, y, t: 0, big: 1 }); fx.push({ k: 'word', x, y: y + 60, t: 0, s: 'IMPACT CLASH!', c: '#fff', z: 16, life: 70 }); flash = 4; shake = 14; }
      else if (e.t === 'wbounce') { fx.push({ k: 'spark', x, y, lvl: 2, t: 0, wall: 1 }); shake = 14; flash = 2; }
      else if (e.t === 'gbounce' || e.t === 'kd') { fx.push({ k: 'dust', x, y: 0, t: 0, big: 1 }); shake = Math.max(shake, e.t === 'kd' ? 5 : 9); }
      else if (e.t === 'jump' || e.t === 'land' || e.t === 'dash' || e.t === 'run') { const p = s.p[e.who]; fx.push({ k: 'dust', x: p.x / F, y: 0, t: 0 }); }
      else if (e.t === 'tech' || e.t === 'clash') fx.push({ k: 'guard', x, y, t: 0, big: 1 });
      else if (e.t === 'pend') fx.push({ k: 'ring', x, y, t: 0 });
      else if (e.t === 'super') superFx = { who: e.who, t: 0, lv: e.lv, name: (e.ca ? 'Critical Art  ' : '') + SIM.CH[s.p[e.who].ch].mv['SA' + e.lv].n };
      else if (e.t === 'ex') fx.push({ k: 'exflash', who: e.who, t: 0 });
      else if (e.t === 'round') msg = { big: s.round >= s.cfg.rounds * 2 - 1 && s.p[0].wins === s.p[1].wins && s.round > 1 ? 'FINAL ROUND' : 'ROUND ' + s.round, jp: '第' + '一二三四五'[s.round - 1] + '回戦', t: 0, dur: 80, col: '#ffd21e' };
      else if (e.t === 'fight') msg = { big: 'FIGHT!', jp: '始め！', t: 0, dur: 50, col: '#ff3355', slam: 1 };
      else if (e.t === 'ko') msg = e.type === 'time' ? { big: 'TIME OVER', jp: '時間切れ', t: 0, dur: 150, col: '#ffd21e' } : e.type === 'draw' || e.type === 'dko' ? { big: e.type === 'dko' ? 'DOUBLE K.O.' : 'DRAW', jp: '引き分け', t: 0, dur: 150, col: '#ffd21e' } : { big: 'K.O.', jp: e.type === 'perfect' ? '完全勝利 PERFECT' : '一本！', t: -34, dur: 190, col: '#ff3355', slam: 1 };
    }
  }

  // ----------------------------------------------------------- 3D frame
  function draw3d(s, menuPan) {
    tick++;
    const L = stage.userData.lamps; for (let i = 0; i < L.length; i++) L[i].material.color.setHSL(0.09, 1, 0.55 + 0.07 * Math.sin(tick * 0.13 + i * 2));
    const S = stage.userData.signs; for (let i = 0; i < S.length; i++) S[i].material.color.setScalar((tick + i * 17) % 97 < 4 ? 0.45 : 1);
    let tx = 0, tw = 440, ty = 72;
    if (s) {
      const a = s.p[0], b = s.p[1], ax = a.x / F, bx = b.x / F;
      tx = (ax + bx) / 2; tw = Math.max(300, Math.min(480, Math.abs(ax - bx) + 200)); ty = 68 + Math.max(a.y, b.y) / F * 0.42;
      if (superFx && superFx.t < 40) { const p = s.p[superFx.who]; tx = p.x / F; tw = 300 - (superFx.lv || 1) * 14; ty = 62; }
      if (s.phase === 'ko' && s.pt < 150 && s.rw >= 0) { const l = s.p[1 - s.rw]; tx = l.x / F; tw = 250; ty = 44 + l.y / F * 0.6; }
      if (s.slow > 0 && s.phase === 'fight') { tw = Math.min(tw, 300); }
      if (snapCam) { camX = Math.max(-(SIM.STAGE / F + 30 - tw / 2), Math.min(SIM.STAGE / F + 30 - tw / 2, tx)); camW = tw; camY = ty; snapCam = false; }
      const lim = SIM.STAGE / F + 30 - tw / 2; tx = Math.max(-lim, Math.min(lim, tx));
      camX += (tx - camX) * 0.22; camW += (tw - camW) * 0.14; camY += (ty - camY) * 0.2;
      const dim = s.freeze > 0; stage.visible = !dim; scene.background.setHex(dim ? 0x05030c : 0x0b0a24);
      for (let i = 0; i < 2; i++) {
        const p = s.p[i], o = s.p[1 - i], r = rigs[i]; r.G.visible = true;
        const fp = fighterPose(p, o, s.frame, i);
        let jx = 0; if (p.hs > 0 && (p.st === 'hit' || p.st === 'ahit' || p.st === 'block')) jx = ((p.hs & 1) ? 1 : -1) * Math.min(3, p.hs * 0.5);
        const fl = (p.hs > 0 && (p.st === 'hit' || p.st === 'ahit')) ? Math.min(0.9, p.hs * 0.14) : 0;
        const m = p.st === 'atk' ? SIM.CH[p.ch].mv[p.mv] : null; let tint = [0, 0, 0];
        if (m && (m.od || m.sup || m.tint || m.fire)) { const k = 0.25 + 0.2 * Math.sin(s.frame * 0.9); tint = m.sup ? [k, k * 0.6, 0] : [k, k, 0]; }
        if (m && m.di) { const a = s.frame * 0.7; tint = [0.35 + 0.3 * Math.sin(a), 0.35 + 0.3 * Math.sin(a + 2.1), 0.35 + 0.3 * Math.sin(a + 4.2)]; }
        if (m && !m.isAir && m.hits[0] && m.hits[0].g === 'h' && p.mf < m.hits[0].s) { const k = 0.3 + 0.3 * Math.sin(s.frame * 1.2); tint = [k + 0.2, k * 0.4, 0]; }
        if (p.st === 'drush' || (p.st === 'atk' && p.drb)) tint = [0, 0.45, 0.18];
        if (p.st === 'parry') { const k = 0.25 + 0.15 * Math.sin(s.frame * 0.8); tint = [0.05, k, k * 2]; }
        if (p.bo && (s.frame >> 3) & 1) tint = [tint[0] - 0.18, tint[1] - 0.18, tint[2] - 0.12];
        if (s.freeze > 0 && s.frzWho === i) tint = [0.5, 0.35, 0.1];
        applyPose(r, fp, p.face, fl, tint);
        r.G.position.set(p.x / F + jx, p.y / F, i === 0 ? 2 : -2);
        shadows[i].visible = true; shadows[i].position.x = p.x / F; const sc = Math.max(0.45, 1 - p.y / F / 220); shadows[i].scale.set(sc * (p.st === 'kd' ? 1.8 : 1), sc, 1);
      }
      // projectiles
      while (projMeshes.length < s.proj.length) { const g = new THREE.Group(); const c1 = new THREE.Mesh(new THREE.IcosahedronGeometry(9, 0), new THREE.MeshBasicMaterial({ color: 0xffffff })); const c2 = new THREE.Mesh(new THREE.IcosahedronGeometry(15, 0), new THREE.MeshBasicMaterial({ color: 0x33d8ff, transparent: true, opacity: 0.6 })); const c3 = new THREE.Mesh(new THREE.IcosahedronGeometry(20, 0), new THREE.MeshBasicMaterial({ color: 0x2060ff, transparent: true, opacity: 0.3 })); g.add(c1, c2, c3); scene.add(g); projMeshes.push(g); }
      projMeshes.forEach((g, i) => { const q = s.proj[i]; g.visible = !!q; if (!q) return; const pr = SIM.CH[q.ch].mv[q.k].proj; g.position.set(q.x / F, q.y / F, 0); const k = pr.big ? 1.7 : 1, w = 1 + 0.15 * Math.sin(q.life * 1.3); g.scale.set(k * w * (pr.big ? 2.2 : 1.25), k * w, k * w); g.rotation.x = q.life * 0.5; g.children[1].material.color.setHex(q.ch === 0 ? (pr.big ? 0xffb020 : 0x33d8ff) : 0xffe040); g.children[2].material.color.setHex(pr.big ? 0xff4020 : 0x2060ff); });
    } else {
      camX = Math.sin(tick * 0.004) * 240; camW = 470; camY = 96;
      for (const r of rigs) r.G.visible = !!menuPan; for (const sh of shadows) sh.visible = !!menuPan; projMeshes.forEach(g => g.visible = false); stage.visible = true; scene.background.setHex(0x0b0a24);
      if (menuPan) { camX = 0; camW = menuPan.w || 340; camY = menuPan.y || 34; const px = menuPan.x || 75; for (let i = 0; i < 2; i++) { const fake = { ch: menuPan.chars[i], st: menuPan.pose && menuPan.pose[i] ? 'win' : 'idle', sub: 'stand', sf: 30, subf: 0, y: 0, vy: 0 }; const fp = fighterPose(fake, fake, tick); applyPose(rigs[i], fp, i ? -1 : 1, 0, [0, 0, 0]); rigs[i].G.position.set(i ? px : -px, 0, i ? -2 : 2); shadows[i].position.x = i ? px : -px; shadows[i].scale.set(1, 1, 1); } }
    }
    let sx = 0, sy = 0; if (shake > 0) { sx = ((shake * 7919) % 5 - 2) * Math.min(3, shake * 0.35); sy = ((shake * 104729) % 5 - 2) * Math.min(3, shake * 0.35); shake--; }
    const dist = camW / 0.8866; cam.position.set(camX + sx, camY + 14 + sy, dist); cam.lookAt(camX + sx, camY + sy, 0);
    ren.render(scene, cam);
  }

  // ----------------------------------------------------------- 2D helpers
  function clear() { g2.setTransform(2, 0, 0, 2, 0, 0); g2.clearRect(0, 0, VW, VH); }
  function txt(str, x, y, size, col, al, jp, ol) {
    g2.font = size + 'px ' + (jp ? FJ : FP); g2.textAlign = al || 'left'; g2.textBaseline = 'top'; x = Math.round(x); y = Math.round(y);
    if (ol !== 0) { g2.fillStyle = ol || '#0a0612'; for (const d of [[-1, 0], [1, 0], [0, -1], [0, 1], [1, 1], [-1, 1], [1, -1], [-1, -1], [0, 2], [1, 2]]) g2.fillText(str, x + d[0], y + d[1]); }
    g2.fillStyle = col || '#fff'; g2.fillText(str, x, y);
  }
  function rect(x, y, w, h, c) { g2.fillStyle = c; g2.fillRect(Math.round(x), Math.round(y), Math.round(w), Math.round(h)); }
  function burst(cx, cy, r0, r1, n, col, rot, lw) { g2.strokeStyle = col; g2.lineWidth = lw || 2; g2.beginPath(); for (let i = 0; i < n; i++) { const a = rot + i * 6.283 / n, k = 0.6 + 0.4 * hsh(i * 31 + n); g2.moveTo(cx + Math.cos(a) * r0, cy + Math.sin(a) * r0); g2.lineTo(cx + Math.cos(a) * r1 * k, cy + Math.sin(a) * r1 * k); } g2.stroke(); }

  function drawFx(s) {
    for (let i = fx.length - 1; i >= 0; i--) {
      const f = fx[i], t = f.t++; let dead = false;
      if (f.k === 'exflash') { if (s) { const p = s.p[f.who], q = toScreen(p.x / F, p.y / F + 50); g2.globalAlpha = Math.max(0, 1 - t / 14); burst(q[0], q[1], 10 + t * 3, 30 + t * 5, 14, '#ffd21e', t * 0.2, 2); g2.globalAlpha = 1; } dead = t > 14; fx[i] = f; if (dead) fx.splice(i, 1); continue; }
      if (f.k === 'distart') { if (s) { const p = s.p[f.who], q = toScreen(p.x / F, p.y / F + 50), a = Math.max(0, 1 - t / 34), cols = ['#ff5ad2', '#33e0ff', '#ffd21e', '#5dffa0', '#ff3355']; g2.fillStyle = 'rgba(10,6,18,' + (0.5 * a) + ')'; g2.fillRect(0, 0, VW, VH); g2.globalAlpha = a; for (let k = 0; k < 16; k++) { const w = 26 + hsh(k * 5) * 70, yy = q[1] - 56 + k * 7; g2.fillStyle = cols[k % 5]; g2.fillRect(Math.round(q[0] - p.face * (14 + ((t * 9 + k * 23) % 90)) - (p.face > 0 ? w : 0)), Math.round(yy), Math.round(w), 3); } burst(q[0], q[1], 16 + t, 60 + t * 3, 16, cols[(t >> 1) % 5], t * 0.3, 3); g2.globalAlpha = 1; } if (t > 34) fx.splice(i, 1); continue; }
      if (f.k === 'kanji') { const u = t / 40, sc = 1 + Math.max(0, 1 - t / 6) * 2; g2.globalAlpha = u > 0.6 ? Math.max(0, 1 - (u - 0.6) * 2.5) : 1; g2.save(); g2.translate(240, 120); g2.scale(sc, sc); txt(f.s, 0, -48, 96, f.c, 'center', 1, '#0a0612'); g2.restore(); g2.globalAlpha = 1; if (t > 40) fx.splice(i, 1); continue; }
      if (t < 0) continue;
      const q = toScreen(f.x, f.y), x = q[0], y = q[1];
      if (f.k === 'spark') {
        const big = [1, 1.6, 2.3][f.lvl] * (f.ko ? 1.8 : 1), life = f.ko ? 26 : 12 + f.lvl * 3;
        const u = t / life, r = (6 + 30 * Math.sqrt(u)) * big;
        if (t < 4) { g2.fillStyle = '#fff'; const q2 = (11 - t * 2) * big; g2.beginPath(); for (let k = 0; k < 8; k++) { const a = k * 0.785 + 0.3, rr = k % 2 ? q2 * 0.45 : q2 * 1.5; g2.lineTo(x + Math.cos(a) * rr, y + Math.sin(a) * rr); } g2.fill(); }
        g2.globalAlpha = 1 - u * u; burst(x, y, r * 0.45, r * 1.25, 9 + f.lvl * 4, '#0a0612', f.x * 0.37, f.lvl ? 6 : 5); burst(x, y, r * 0.45, r * 1.25, 9 + f.lvl * 4, f.lvl ? '#ffe36a' : '#ffffff', f.x * 0.37, f.lvl ? 4 : 3); burst(x, y, r * 0.2, r * 0.8, 7, f.lvl >= 2 ? '#ff5a2a' : '#ffb040', 0.5 + f.y, 3);
        if (f.lvl >= 1) { g2.strokeStyle = '#fff'; g2.lineWidth = 2; g2.beginPath(); g2.arc(x, y, r * 0.9, 0, 7); g2.stroke(); }
        g2.globalAlpha = 1; dead = t >= life;
      } else if (f.k === 'guard') {
        const u = t / 10, r = (f.big ? 14 : 8) + 14 * u; g2.globalAlpha = 1 - u; g2.strokeStyle = '#7fdcff'; g2.lineWidth = 3; g2.beginPath(); g2.arc(x, y, r, -1.2, 1.2); g2.stroke(); g2.beginPath(); g2.arc(x, y, r, 1.94, 4.34); g2.stroke(); burst(x, y, r * 0.4, r * 1.1, 6, '#e8fbff', 0.3, 1); g2.globalAlpha = 1; dead = t >= 10;
      } else if (f.k === 'dust') {
        const n = f.big ? 7 : 4, life = f.big ? 18 : 12, u = t / life; g2.fillStyle = 'rgba(230,210,220,' + (0.7 * (1 - u)) + ')';
        for (let k = 0; k < n; k++) { const dir = (k % 2 ? 1 : -1) * (1 + (k >> 1)); const s2 = Math.round((f.big ? 6 : 4) * (1 - u * 0.6)); g2.fillRect(Math.round(x + dir * (6 + 22 * u) * (f.big ? 1.4 : 1)) - s2 / 2, Math.round(y - 2 - u * 8 * (1 + (k % 3))) - s2, s2, s2); }
        dead = t >= life;
      } else if (f.k === 'word') { const u = t / (f.life || 44); g2.globalAlpha = u > 0.75 ? Math.max(0, 1 - (u - 0.75) * 4) : 1; txt(f.s, x, y - 14 - t * 0.35, f.z || 8, f.c, 'center'); g2.globalAlpha = 1; dead = t >= (f.life || 44);
      } else if (f.k === 'paint') { const u = t / 22; g2.globalAlpha = 1 - u * u; const cols = ['#ff5ad2', '#33e0ff', '#ffd21e', '#5dffa0', '#ff3355']; for (let k = 0; k < 14; k++) { const a = k * 0.449 + f.x, rr = (10 + 60 * Math.sqrt(u)) * (0.5 + hsh(k * 7 + 3)) * (f.big ? 1.8 : 1); g2.fillStyle = cols[k % 5]; const s2 = Math.round((f.big ? 18 : 10) * (1 - u * 0.5) * (0.6 + hsh(k))); g2.fillRect(Math.round(x + Math.cos(a) * rr) - s2 / 2, Math.round(y + Math.sin(a) * rr * 0.8) - s2 / 2, s2, s2); } g2.globalAlpha = 1; dead = t >= 22;
      } else if (f.k === 'ring') { const u = t / 9; g2.globalAlpha = 1 - u; g2.strokeStyle = '#9fe8ff'; g2.lineWidth = 2; g2.beginPath(); g2.arc(x, y, 6 + 22 * u, 0, 7); g2.stroke(); g2.globalAlpha = 1; dead = t >= 9; }
      if (dead) fx.splice(i, 1);
    }
    if (flash > 0) { g2.fillStyle = 'rgba(255,255,255,' + Math.min(0.85, flash * 0.28) + ')'; g2.fillRect(0, 0, VW, VH); flash--; }
  }

  function bar(x, y, w, h, frac, redFrac, flip, col) {
    rect(x - 2, y - 2, w + 4, h + 4, '#0a0612'); rect(x - 1, y - 1, w + 2, h + 2, '#f4f0e0'); rect(x, y, w, h, '#2a1024');
    const rw = Math.round(w * redFrac), fw = Math.round(w * frac);
    rect(flip ? x + w - rw : x, y, rw, h, '#ff3355'); rect(flip ? x + w - fw : x, y, fw, h, col); rect(flip ? x + w - fw : x, y, fw, 2, 'rgba(255,255,255,0.55)');
  }
  function hud(s, o) {
    o = o || {};
    for (let i = 0; i < 2; i++) {
      const p = s.p[i], C = SIM.CH[p.ch], flip = i === 0, x = i ? 262 : 22, w = 196;
      const fr = p.hp / C.hp; bar(x, 16, w, 9, fr, p.red / C.hp, flip, fr < 0.25 ? ((s.frame >> 3) & 1 ? '#ffd21e' : '#ff8a1e') : '#ffd21e');
      txt(C.name, i ? 458 : 22, 29, 8, '#fff', i ? 'right' : 'left'); txt(C.jp, i ? 458 - C.name.length * 8 - 6 : 22 + C.name.length * 8 + 6, 26, 12, '#ff3355', i ? 'right' : 'left', 1);
      if (o.tag && o.tag[i]) txt(o.tag[i], i ? 262 : 218, 29, 8, '#7fdcff', i ? 'left' : 'right');
      for (let k = 0; k < s.cfg.rounds; k++) { const px = i ? 262 + k * 11 : 211 - k * 11; rect(px - 1, 3, 9, 9, '#0a0612'); rect(px, 4, 7, 7, k < p.wins ? '#ff3355' : '#3a2440'); if (k < p.wins) rect(px + 2, 6, 3, 3, '#ffd21e'); }
      // drive gauge: six bars under the life bar
      for (let k = 0; k < 6; k++) { const gx = flip ? 218 - 15 - k * 16 : 262 + k * 16, fillv = Math.max(0, Math.min(1000, p.dr - k * 1000)) / 1000, w2 = Math.round(13 * fillv); rect(gx - 1, 39, 15, 6, '#0a0612'); rect(gx, 40, 13, 4, p.bo ? '#3a3440' : '#12301e'); rect(flip ? gx + 13 - w2 : gx, 40, w2, 4, p.bo ? ((s.frame >> 3) & 1 ? '#8d7fa0' : '#5a5066') : '#5dffa0'); }
      if (p.bo) txt('BURNOUT', flip ? 118 : 362, 38, 8, (s.frame >> 3) & 1 ? '#ff3355' : '#8d7fa0', flip ? 'right' : 'left');
      // super art gauge: three stocks
      const mx = i ? 338 : 22, mw = 120, lv = Math.floor(p.sa / 1000);
      rect(mx - 2, 252, mw + 4, 10, '#0a0612'); rect(mx - 1, 253, mw + 2, 8, '#f4f0e0'); rect(mx, 254, mw, 6, '#1a1030');
      const fw = Math.round(mw * p.sa / 3000); rect(i ? mx + mw - fw : mx, 254, fw, 6, lv >= 3 ? ((s.frame >> 2) & 1 ? '#fff' : '#ffd21e') : lv >= 1 ? '#ffb020' : '#2a7ad8'); rect(mx + 39, 254, 2, 6, '#0a0612'); rect(mx + 79, 254, 2, 6, '#0a0612');
      txt('SA' + lv, i ? mx - 6 : mx + mw + 6, 253, 8, lv >= 3 ? '#ffd21e' : lv ? '#ffb020' : '#5a4a6a', i ? 'right' : 'left');
      // guard cue: say what the incoming attack is before it lands
      const cm = p.st === 'atk' ? SIM.CH[p.ch].mv[p.mv] : null;
      if (cm && (cm.od || cm.sup || cm.fire || cm.tint) && p.mf >= cm.su - 2) { const fq = toScreen(p.x / F + p.face * 16, p.y / F + 70); for (let k = 0; k < 10; k++) { const a = (s.frame * 3 + k * 37) % 26; g2.fillStyle = ['#fff3a0', '#ffd21e', '#ff8a1e', '#ff3355'][(a / 7) | 0]; const z = 5 - (a / 6 | 0); g2.fillRect(Math.round(fq[0] + (hsh(k * 9 + (s.frame >> 2)) - 0.5) * 30), Math.round(fq[1] - a * 1.6 + (hsh(k) - 0.5) * 24), z, z); } }
      if (cm && !cm.isAir && cm.hits[0] && p.mf <= cm.hits[0].e && (cm.hits[0].g === 'h' || cm.thr || cm.di)) { const q = toScreen(p.x / F, p.y / F + 112), oh = cm.hits[0].g === 'h'; if ((s.frame >> 1) & 1 || p.mf > 4) { txt('!', q[0], q[1] - 12, 16, cm.di ? '#ff5ad2' : '#ff8a1e', 'center'); txt(cm.di ? 'IMPACT' : oh ? 'OVERHEAD' : 'THROW', q[0], q[1] + 6, 8, cm.di ? '#ff5ad2' : '#ff8a1e', 'center'); } }
      const c = combo[i];
      if (c) { c.t++; if (c.n >= 2 || c.ch) { const pop = c.t < 5 ? 1 + (5 - c.t) * 0.12 : 1, cx = i ? 400 : 80, a = c.t > 70 ? Math.max(0, 1 - (c.t - 70) / 12) : 1; g2.globalAlpha = a; if (c.n >= 2) { g2.save(); g2.translate(cx, 78); g2.scale(pop, pop); txt('' + c.n, 0, -16, 24, '#ffd21e', 'right'); txt('HITS', 4, -8, 8, '#fff', 'left'); g2.restore(); txt(c.d + ' DMG', cx, 90, 8, '#ff8a6a', 'center'); txt('連撃', cx + 40, 60, 12, '#ff3355', 'center', 1); } if (c.ch && c.t < 40) txt(c.ch === 2 ? 'PUNISH COUNTER' : 'COUNTER', cx, 104, 8, c.ch === 2 ? '#ffd21e' : '#33e0ff', 'center'); g2.globalAlpha = 1; } if (c.t > 82) combo[i] = null; }
    }
    rect(226, 8, 28, 26, '#0a0612'); rect(227, 9, 26, 24, '#f4f0e0'); rect(228, 10, 24, 22, '#1a1030');
    const tm = s.cfg.training ? '∞' : ('' + Math.max(0, Math.ceil(s.time / 60))).padStart(2, '0'); txt(tm, 240.5, 14, s.cfg.training ? 16 : 12, s.time < 600 && !s.cfg.training ? '#ff3355' : '#ffd21e', 'center', s.cfg.training);
    if (superFx) {
      const t = superFx.t++, i = superFx.who, lv = superFx.lv || 1, dur = [0, 40, 44, 60][lv], col = ['', '#33e0ff', '#ffd21e', '#ff3355'][lv];
      if (t < dur + 6) {
        const p = s.p[i], q = toScreen(p.x / F, p.y / F + 55), dir = i ? -1 : 1;
        if (lv >= 2) { g2.fillStyle = lv === 3 ? 'rgba(120,0,24,' + (0.3 + 0.14 * Math.sin(t * 0.8)) + ')' : 'rgba(40,24,0,0.28)'; g2.fillRect(0, 0, VW, VH); }
        g2.globalAlpha = 0.85; burst(q[0], q[1], 40 + (t % 6) * 6, 420, 24 + lv * 10, col, t * 0.05 * dir, 2); burst(q[0], q[1], 26, Math.max(30, 100 - t * 2), 12, '#fff', -t * 0.15, 3);
        for (let k = 0; k < lv; k++) { g2.strokeStyle = k % 2 ? '#fff' : col; g2.lineWidth = 3; g2.beginPath(); g2.arc(q[0], q[1], ((t * 7 + k * 60) % 190) + 10, 0, 7); g2.stroke(); }
        g2.globalAlpha = 1;
        const bh = lv === 3 ? 36 : lv === 2 ? 22 : 10, sl = Math.min(1, t / 6); rect(0, 0, VW, bh * sl, '#0a0612'); rect(0, VH - bh * sl, VW, bh * sl, '#0a0612'); rect(0, bh * sl, VW, 2, col); rect(0, VH - bh * sl - 2, VW, 2, col);
        if (lv === 3) { g2.globalAlpha = 0.4; for (let k = 0; k < 4; k++) txt('\u6fc0\u7a81', ((k * 190 + t * 9 * dir) % 760 + 760) % 760 - 150, 76, 96, k % 2 ? '#ff3355' : '#ffd21e', 'left', 1, 0); g2.globalAlpha = 1; }
        const sx = Math.min(1, t / 8), bx = i ? 480 - 300 * sx : -300 + 300 * sx; rect(bx, 186, 300, 36, 'rgba(10,6,18,0.9)'); rect(bx, 186, 300, 2, col); rect(bx, 220, 300, 2, col);
        txt(superFx.name.toUpperCase(), bx + 150, 192, 8, '#fff', 'center'); txt('\u8d85\u5fc5\u6bba  SUPER ART', bx + 132, 202, 16, col, 'center', 1); for (let k = 0; k < 3; k++) rect(bx + 232 + k * 14, 206, 10, 10, k < lv ? col : '#3a2440');
      }
      if (t > dur + 16) superFx = null;
    }
    if (msg) {
      const t = msg.t++; if (t >= 0) { const u = Math.min(1, t / 7), out = t > msg.dur - 8 ? (t - (msg.dur - 8)) / 8 : 0; let sc = msg.slam ? 1 + (1 - u) * 2.5 : 1; g2.globalAlpha = Math.max(0, (msg.slam ? u : 1) * (1 - out));
      const w = msg.slam ? 480 : 480 * sm(t / 10); rect(240 - w / 2, 106, w, 46, 'rgba(10,6,18,0.72)'); rect(240 - w / 2, 106, w, 2, msg.col); rect(240 - w / 2, 150, w, 2, msg.col);
      g2.save(); g2.translate(240, 122); g2.scale(sc, sc); txt(msg.big, 0, -10, 24, msg.col, 'center'); g2.restore(); txt(msg.jp, 240, 136, 12, '#fff', 'center', 1); g2.globalAlpha = 1; if (t >= msg.dur) msg = null; }
    }
  }
  function boxes(s) {
    const dr = (b, col) => { const a = toScreen(b[0] / F, b[3] / F), c = toScreen(b[2] / F, b[1] / F); g2.fillStyle = col + '44'; g2.fillRect(a[0], a[1], c[0] - a[0], c[1] - a[1]); g2.strokeStyle = col; g2.lineWidth = 1; g2.strokeRect(a[0] + 0.5, a[1] + 0.5, c[0] - a[0], c[1] - a[1]); };
    for (const p of s.p) { for (const u of SIM.hurt(p)) dr(u, '#33e0ff'); for (const h of SIM.hitboxes(p)) dr(h.b, '#ff3355'); }
    for (const q of s.proj) dr(SIM.pbox(q), '#ff3355');
  }
  return { snap() { snapCam = true; }, init, setFighters, events, draw3d, clear, drawFx, hud, boxes, txt, rect, burst, toScreen, get g() { return g2; }, get tick() { return tick; }, VW, VH, hsh, say(m) { msg = m; }, FJ, FP };
})();
