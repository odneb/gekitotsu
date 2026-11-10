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
  const TH = 23, SHN = 23, SP1 = 14, SP2 = 17, UA = 17, FA = 19, SW = 8.6, HW = 4.2;
  const NUM = ['ln', 'ln2', 'tw', 'pt', 'hd', 'eL', 'eR', 'kL', 'kR', 'yaw', 'rot', 'ay'], VEC = ['hip', 'hL', 'hR', 'fL', 'fR'];
  const ST = { hip: [0, 37, 0], ln: 24, ln2: 36, tw: -48, pt: -32, hd: -14, hL: [25, 27, 0], hR: [9, 23, 0], fL: [17, 4, 0], fR: [-16, 4, 0], eL: -0.7, eR: -0.7, kL: 0.25, kR: 0.25, yaw: 0, rot: 0, ay: 0.5, aim: null, aw: 0 };
  function norm(o, b) { const r = {}; for (const k of NUM) r[k] = o[k] != null ? o[k] : b[k]; for (const k of VEC) { const v = o[k] || b[k]; r[k] = [v[0], v[1], v[2] || 0]; } if (o.ln != null && o.ln2 == null) r.ln2 = o.ln + 4; r.aim = o.aim || null; r.aw = o.aim ? 1 : 0; r.up = o.up || 0; return r; }
  function mix(a, b, k) {
    if (k <= 0) return a; if (k >= 1) return b; const r = {}; for (const n of NUM) r[n] = a[n] + (b[n] - a[n]) * k;
    for (const n of VEC) r[n] = [a[n][0] + (b[n][0] - a[n][0]) * k, a[n][1] + (b[n][1] - a[n][1]) * k, a[n][2] + (b[n][2] - a[n][2]) * k];
    if (b.aim && (!a.aim || a.aim === b.aim)) { r.aim = b.aim; r.aw = (a.aim ? a.aw : 0) + (b.aw - (a.aim ? a.aw : 0)) * k; r.up = b.up; } else if (a.aim) { r.aim = a.aim; r.aw = a.aw * (1 - k); r.up = a.up; } else { r.aim = null; r.aw = 0; r.up = 0; }
    return r;
  }
  const sm = k => k <= 0 ? 0 : k >= 1 ? 1 : k * k * (3 - 2 * k);
  const s = o => norm(o, ST);
  const CRO = s({ hip: [0, 16], ln: 44, ln2: 54, hd: -30, hL: [24, 4], hR: [4, 5], fL: [19, 4], fR: [-17, 4], kL: 1, kR: 1, tw: -40 }), c = o => norm(o, CRO);
  const JMP = s({ hip: [0, 44], ln: 20, ln2: 30, hd: -10, hL: [22, 62], hR: [-14, 56], fL: [12, 24], fR: [-12, 18], eL: 0, eR: 0 }), j = o => norm(o, JMP);
  const PZ = {
    idle: ST, idle2: s({ hip: [0, 35], ln: 27, ln2: 40, hd: -18, hL: [27, 24], hR: [7, 26] }), crouch: CRO, jump: JMP,
    walkA: s({ hip: [2, 36], ln: 30, fL: [22, 4], fR: [-8, 9], hL: [16, 26], hR: [20, 20] }), walkB: s({ hip: [2, 34], ln: 30, fL: [6, 9], fR: [-20, 4], hL: [30, 22], hR: [2, 28] }),
    prejump: s({ hip: [0, 18], ln: 46, hd: -30, hL: [22, 5], hR: [2, 6], kL: 0.8, kR: 0.8 }),
    fall: s({ hip: [0, 46], ln: 6, ln2: 2, hd: 6, hL: [10, 84], hR: [-10, 80], fL: [8, 6], fR: [-9, 4], eL: 0.4, eR: 0.4 }),
    dashf: s({ hip: [6, 24], ln: 58, ln2: 66, hd: -46, tw: -30, hL: [40, 22], hR: [26, 12], fL: [20, 5], fR: [-24, 10] }),
    dashb: s({ hip: [-6, 44], ln: -16, ln2: -8, hd: 6, hL: [26, 50], hR: [18, 42], fL: [10, 9], fR: [-12, 6], eL: 0, eR: 0 }),
    runA: s({ hip: [4, 25], ln: 64, ln2: 70, hd: -52, tw: -70, pt: -50, hL: [38, 4], hR: [10, 16], fL: [-4, 11], fR: [-28, 4], eL: -0.2, eR: -1 }),
    runB: s({ hip: [4, 28], ln: 60, ln2: 66, hd: -50, tw: 5, pt: -5, hL: [14, 18], hR: [34, 4], fL: [10, 4], fR: [-16, 16], eL: -1, eR: -0.2 }),
    runStop: s({ hip: [-3, 30], ln: 8, hd: -6, hL: [26, 30], hR: [10, 22], fL: [22, 4], fR: [-12, 4] }),
    parry: s({ hip: [0, 34], ln: 14, ln2: 20, hd: -10, tw: -60, hL: [15, 60], hR: [19, 50], eL: 0.3, eR: -0.9, fL: [18, 4], fR: [-18, 4], kL: 0.4, kR: 0.4 }),
    pryH: s({ hip: [-5, 33], ln: -40, ln2: -54, hd: 22, tw: -80, hL: [22, 72], hR: [-10, 36], eL: 0.6, fL: [17, 4], fR: [-15, 4], kL: 0.5, kR: 0.5 }), pryH2: s({ hip: [-2, 22], ln: 52, ln2: 70, hd: -50, tw: -20, hL: [26, 50], hR: [12, 8], eL: 0.8, fL: [19, 4], fR: [-17, 4], kL: 0.8, kR: 0.8 }),
    pryM: s({ hip: [-4, 36], ln: 4, ln2: -6, hd: 8, tw: 105, pt: 60, hL: [26, 48], hR: [-4, 30], eL: 0.2, fL: [10, 4], fR: [-14, 4] }), pryM2: s({ hip: [-7, 38], ln: -18, ln2: 10, hd: -10, tw: -110, pt: -80, hL: [8, 30], hR: [24, 52], eR: 0.2, fL: [12, 4], fR: [-16, 4] }),
    pryL: s({ hip: [-3, 42], ln: 10, ln2: 22, hd: -12, tw: -60, hL: [24, 20], hR: [8, 44], fL: [12, 26], fR: [-10, 4], kL: 0.2 }), pryL2: s({ hip: [-4, 46], ln: -8, ln2: 6, hd: -4, tw: -70, hL: [10, 52], hR: [20, 16], fL: [6, 30], fR: [-8, 12] }),
    thUp: s({ hip: [2, 44], ln: -8, ln2: -14, hd: 14, hL: [14, 96], hR: [6, 92], eL: 0.6, eR: 0.6 }),
    jupS: j({ hip: [0, 46], ln: -22, ln2: -32, hd: 22, tw: -70, hL: [12, 100], aim: 'hL', up: 1, hR: [-10, 58], fL: [6, 20], fR: [-10, 26] }),
    jdnS: j({ hip: [0, 50], ln: 12, hd: -10, fL: [6, 0], aim: 'fL', ay: 0.25, fR: [-8, 32], hL: [18, 74], hR: [-16, 70], eL: 0.4, eR: 0.4 }),
    jbkS: j({ hip: [0, 46], ln: 16, tw: 130, pt: 110, fR: [-54, 34], fL: [8, 26], hL: [20, 56], hR: [6, 50] }),
    aspkS: j({ hip: [0, 52], ln: -32, ln2: -22, hd: 22, tw: -80, pt: -70, fL: [38, 6], aim: 'fL', ay: 0.3, fR: [-6, 42], hL: [-14, 74], hR: [-24, 62], eL: 0.3, eR: 0.3 }),
    blockS: s({ hip: [-3, 38], ln: 4, ln2: 10, hd: -6, tw: -35, hL: [13, 68], hR: [15, 56], eL: 0.2, eR: -0.8, fL: [13, 4], fR: [-16, 4] }),
    blockC: c({ hip: [-3, 17], ln: 30, ln2: 36, hd: -20, hL: [14, 40], hR: [16, 30], eL: 0.2 }),
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
    win: s({ hip: [0, 47], ln: -6, ln2: -10, tw: -88, hd: 18, hL: [26, 34], hR: [-24, 36], eL: -0.2, eR: -0.2, fL: [8, 4], fR: [-9, 4], kL: 0, kR: 0 }),
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
    jUP: Q('jump', 'jupS'), jDN: A('jump', 'jhpW', 'jdnS'), jBK: Q('jump', 'jbkS'), ASPK: Q('jump', 'aspkS'),
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
      case 'parry': { const rg = rigs[idx], base = mix(P('idle'), P('parry'), sm((p.sf + 1) / 3)); if (rg && rg.pk && rg.pt < 18) { const w = rg.pt < 2 ? (rg.pt + 1) / 2 : 1 - sm((rg.pt - 7) / 11); po = mix(base, P(rg.pk + (rg.pn & 1 ? '2' : '')), w); } else po = base; break; } case 'prec': po = mix(P('parry'), P('idle'), sm(p.sf / 11)); break;
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
      case 'throwing': {
        if (!p.tk) po = p.sf < 8 ? mix(P('grabS'), P('thPull'), sm(p.sf / 8)) : p.pmT > 0 ? mix(P('thPull'), P('thKnee'), Math.sin(p.pmT / 11 * Math.PI)) : P('thPull');
        else { const t = p.tf; po = norm(p.tk === 2 ? kf([[0, 'thPull'], [4, 'bthTuck', 'i'], [10, 'bthToss'], [18, 'situp'], [30, 'idle']], t) : p.tk === 3 ? kf([[0, 'thPull'], [5, 'cupW', 'o'], [10, 'thUp', 'i'], [18, 'thUp'], [30, 'idle']], t) : p.tk === 4 ? kf([[0, 'thPull'], [5, 'thUp', 'o'], [10, 'thSlam', 'i'], [18, 'thSlam'], [30, 'idle']], t) : kf([[0, 'thPull'], [5, 'thKnee'], [10, 'thKick', 'i'], [17, 'thKick'], [30, 'idle']], t), ST);
          if (p.tk === 2) po.rot = -200 * Math.max(0, Math.min(1, (t - 3) / 8)) * (t < 14 ? 1 : Math.max(0, 1 - (t - 14) / 6)); }
        break; }
      case 'thrown': { po = norm(P('thrown'), ST); po.rot = o.tk ? -Math.min(120, (o.tf || 0) * 12) : -8 - (o.pmT > 0 ? 12 * Math.sin(o.pmT / 11 * Math.PI) : 0); break; }
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
    return { po: po, hb: hb, yaw: yaw, air: p.y > 0, loose: p.st === 'idle' || p.st === 'parry' || p.st === 'win' ? 1 : p.st === 'atk' ? 0.4 : p.st === 'block' ? 0.5 : p.st === 'kd' || p.st === 'thrown' ? 0 : 0.6 };
  }

  // ----------------------------------------------------------- rig
  const LOOK = [
    { top: 0xf1efe6, pants: 0xe9e6da, skin: 0xe6b088, hair: 0x1a1418, glove: 0xd01f2e, belt: 0x18181c, band: 0xd01f2e, shoe: 0xe6b088, trim: 0xd01f2e },
    { top: 0x1c1f2e, pants: 0x2848d0, skin: 0xd9a27c, hair: 0xf3dc4a, glove: 0xf1dcb4, belt: 0xf1dcb4, band: 0, shoe: 0xf4f4f4, trim: 0xf1dcb4 }
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

  // Two original fighters. ONIMARU: a horned oni brawler with a white mane, rope belt and flared hakama.
  // KITSUNEBI: a fox-masked shinobi with a glowing scarf and a great tail. Manes, scarf and tail trail behind with their own motion.
  // RET and ARD: slender fluid beings. Long limbs, clawed hands, a smooth mask with slit eyes, ribbons that stream off the body.
  // Semi-translucent bodies; a liquid pool follows each one across the floor and sparks of their energy drift off on the wind.
  const LOOKS = [
    { body: 0xffffff, body2: 0xd3dcf0, acc: 0x4aa8ff, acc2: 0x9fd0ff, eye: 0x16244e, ink: 0x27375e, op: 0.9 },
    { body: 0x121016, body2: 0x2a1626, acc: 0xff4aa0, acc2: 0xff9ccb, eye: 0xff6ab4, ink: 0xd8327a, op: 0.9 }
  ];
  const LOOKALT = [{ body: 0xdfe9e6, body2: 0xb3cfc8, acc: 0x2fd6b4, acc2: 0x9ff0dc, ink: 0x1f4a44 }, { body: 0x16101c, body2: 0x24163a, acc: 0xb06aff, acc2: 0xd6b0ff, eye: 0xc79aff, ink: 0x8a4ae0 }];
  function buildRig(ch, alt) {
    const lk = Object.assign({}, LOOKS[ch], alt ? LOOKALT[ch] : {}), mats = {}, lit = {};
    const rv = ch === 0 ? [178, 222, 255] : [110, 185, 255], ramp = new THREE.DataTexture(new Uint8Array([rv[0], rv[0], rv[0], 255, rv[1], rv[1], rv[1], 255, 255, 255, 255, 255]), 3, 1, THREE.RGBAFormat); ramp.minFilter = ramp.magFilter = THREE.NearestFilter; ramp.needsUpdate = true;
    const mat = k => (k === 'acc' || k === 'acc2' || k === 'eye') ? (lit[k] || (lit[k] = new THREE.MeshBasicMaterial({ color: lk[k], transparent: true, opacity: 0.95 }))) : mats[k] || (mats[k] = new THREE.MeshToonMaterial({ color: lk[k], gradientMap: ramp, transparent: true, opacity: lk.op }));
    const inkN = new THREE.MeshBasicMaterial({ color: lk.ink, side: THREE.BackSide, transparent: true, opacity: 0.9 });
    inkN.onBeforeCompile = sh => { sh.vertexShader = sh.vertexShader.replace('#include <begin_vertex>', 'vec3 transformed = position + normal * 1.15;'); };
    const G = new THREE.Group(), M = new THREE.Group(), R = new THREE.Group(), T = new THREE.Group(); G.add(M); M.add(R); R.rotation.order = 'ZYX';
    const add = (g, k, par, noInk) => { const m = new THREE.Mesh(g, mat(k)); (par || R).add(m); if (!noInk) m.add(new THREE.Mesh(g, inkN)); return m; };
    const sph = (k, r, sx, sy, sz, par, x, y, z, noInk) => { const m = add(new THREE.IcosahedronGeometry(r, 2), k, par, noInk); m.scale.set(sx, sy, sz); m.position.set(x || 0, y || 0, z || 0); return m; };
    const cyl = (k, r0, r1, len, par, noInk) => { const g = new THREE.CylinderGeometry(r1, r0, len, 10); g.translate(0, len / 2, 0); return add(g, k, par, noInk); };
    const cone = (k, r, h, par, x, y, z, noInk) => { const g = new THREE.ConeGeometry(r, h, 7); g.translate(0, h / 2, 0); const m = add(g, k, par, noInk); m.position.set(x, y, z); return m; };
    const box = (k, w, h, d, par, x, y, z) => { const m = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), mat(k)); (par || R).add(m); m.position.set(x || 0, y || 0, z || 0); return m; };
    const b = {}, trails = [];
    const trail = (par, off, n, len, grav, back, mk) => { const t = { par: par, off: new THREE.Vector3(off[0], off[1], off[2]), pts: [], segs: [], len: len, grav: grav, back: back, init: 0 }; for (let i = 0; i <= n; i++) t.pts.push(new THREE.Vector3()); for (let i = 0; i < n; i++) { const m = mk(i, n); T.add(m); t.segs.push(m); } trails.push(t); };
    // a ribbon: wide where it leaves the body, a thread at the tip, the last third in the accent colour
    const ribbon = (par, off, n, len, grav, back, w0) => trail(par, off, n, len, grav, back, (i, nn) => { const m = new THREE.Group(), u = i / (nn - 1), w = w0 * (1 - u * 0.86) + 0.5; const s1 = add(new THREE.BoxGeometry(w, len + 0.6, 1), u > 0.62 ? 'acc' : u > 0.4 ? 'body2' : 'body', m, u > 0.62); s1.position.y = len / 2; return m; });
    // torso: wasp waist under a flared ribcage
    b.pel = sph('body', 5.4, 0.8, 0.95, 1, R); b.abd = cyl('body', 3.6, 4.4, SP1 - 2); b.abd.scale.set(0.8, 1, 1);
    b.chest = cyl('body', 4.6, 8.4, SP2); b.chest.scale.set(0.72, 1, 1); sph('body', 8.4, 1, 0.45, 1, b.chest, 0, SP2, 0);
    for (const z of [-1, 1]) { const st = box('acc', 0.8, SP2 * 0.8, 1.1, b.chest, 7, SP2 * 0.45, z * 2.6); st.rotation.x = z * 0.32; }
    const sp = add(new THREE.TorusGeometry(2.6, 0.8, 5, 12), 'acc', b.chest, 1); sp.position.set(0, SP2 - 2, 8.6);
    // head: a long smooth mask, nothing on it but two slits
    const hd = b.head = new THREE.Group(); R.add(hd);
    const nk = cyl('body', 2.1, 2.3, 7, hd); nk.position.y = -4;
    const sk = new THREE.Group(); hd.add(sk); sk.position.set(1, 8.5, 0); sk.rotation.z = -0.42;
    sph('body', 6.3, 1.02, 1.5, 0.86, sk, 0, 0, 0); const bk = sph('body2', 6.1, 0.86, 1.36, 0.9, sk, -1.9, 1.2, 0, 1); sph('acc', 2, 0.5, 2.6, 1.6, sk, -6.2, 3, 0, 1);
    for (const z of [-1, 1]) { const e = box('eye', 0.9, 3.6, 1.3, sk, 5.5, -3.4, z * 2.2); e.rotation.x = z * 0.5; e.rotation.z = 0.25; }
    ribbon(sk, [-5, 6, 1.5], 13, 6.2, -0.05, 2.2, 5.6); ribbon(sk, [-5.5, 2, -1.5], 10, 6, 0.02, 1.9, 4.4);
    ribbon(b.chest, [-5.5, SP2 - 3, 5], 8, 5.6, 0.06, 1.6, 3.6); ribbon(b.chest, [-5.5, SP2 - 3, -5], 7, 5.6, 0.08, 1.4, 3.2); ribbon(b.pel, [-4, 1, 0], 9, 5.8, 0.1, 1.5, 4.2);
    for (const n of ['L', 'R']) { const sd = n === 'L' ? -1 : 1;
      b['sh' + n] = sph('body', 4.3, 1, 1, 1, R); const r1 = add(new THREE.TorusGeometry(2.4, 0.7, 5, 12), 'acc', b['sh' + n], 1); r1.position.z = sd * 3.6;
      b['ua' + n] = cyl('body', 3.3, 2.9, UA / 2); b['ua' + n + 'b'] = cyl('body', 2.9, 2.4, UA / 2); sph('body', 2.9, 1, 1, 1, b['ua' + n + 'b'], 0, 0, 0, 1); b['fa' + n] = cyl('body', 2.4, 2.9, FA / 2); sph('body', 2.5, 1, 1, 1, b['fa' + n], 0, 0, 0, 1); b['fa' + n + 'b'] = cyl('body', 2.9, 3.5, FA / 2); sph('body', 2.9, 1, 1, 1, b['fa' + n + 'b'], 0, 0, 0, 1); const bd = cyl('acc', 3.2, 3.5, 1.3, b['fa' + n + 'b'], 1); bd.position.y = FA / 2 - 5; const b2 = cyl('body2', 2.9, 3.6, 5, b['fa' + n + 'b'], 1); b2.position.y = FA / 2 - 5;
      const hnd = b['fi' + n] = new THREE.Group(); R.add(hnd); sph('body2', 3.3, 0.8, 1.5, 1.15, hnd, 0, 1, 0);                    // local y runs out along the forearm
      for (let k = -1; k <= 1; k++) { const c = cone(k === 0 ? 'body2' : 'body2', 1.15, 11 - Math.abs(k) * 1.5, hnd, k * 1.9, 3.4, k * 0.8); c.rotation.z = -k * 0.24; const tip = cone('acc', 0.75, 4, c, 0, 7.4 - Math.abs(k) * 1.5, 0, 1); }
      const th = cone('body2', 1, 6, hnd, -2.6, 0.5, 2 * sd); th.rotation.z = 0.9;
      b['th' + n] = cyl('body', 4.5, 3.8, TH / 2); b['th' + n + 'b'] = cyl('body', 3.8, 3, TH / 2); sph('body', 3.8, 1, 1, 1, b['th' + n + 'b'], 0, 0, 0, 1); b['sn' + n] = cyl('body', 3, 2.6, SHN / 2); sph('body', 3.1, 1, 1, 1, b['sn' + n], 0, 0, 0, 1); b['sn' + n + 'b'] = cyl('body', 2.6, 2.2, SHN / 2); sph('body', 2.6, 1, 1, 1, b['sn' + n + 'b'], 0, 0, 0, 1); const kb = cyl('acc', 3.1, 2.9, 1.2, b['sn' + n], 1); kb.position.y = 3; const s2 = cyl('body2', 2.6, 2.4, 7, b['sn' + n + 'b'], 1); s2.position.y = SHN / 2 - 7;
      const ft = b['ft' + n] = new THREE.Group(); R.add(ft); sph('body2', 3, 0.85, 1.2, 1, ft, 0, 3.2, 0); const toe = cone('body2', 2.5, 13, ft, 0.6, 4, 0); toe.scale.set(0.7, 1, 1); cone('acc', 1.1, 4.5, toe, 0, 9, 0, 1);
    }
    // the pool underneath, and the pool of sparks this body sheds
    const pm = new THREE.MeshBasicMaterial({ color: lk.body, transparent: true, opacity: 0.72, depthWrite: false }), rm = new THREE.MeshBasicMaterial({ color: lk.acc, transparent: true, opacity: 0.55, depthWrite: false, blending: THREE.AdditiveBlending });
    const pool = new THREE.Group(); T.add(pool); const drops = [];
    for (let i = 0; i < 5; i++) { const g = new THREE.Group(); const c = new THREE.Mesh(new THREE.CircleGeometry(1, 18), pm), rr = new THREE.Mesh(new THREE.RingGeometry(1, 1.14, 18), rm); g.add(c); g.add(rr); g.rotation.x = -Math.PI / 2; pool.add(g); drops.push({ g: g, x: 0, r: i ? 3 + i * 1.3 : 19, o: i ? (i % 2 ? 1 : -1) * (14 + i * 6) : 0 }); }
    const NP = 46, pg = new THREE.BufferGeometry(), pp = new Float32Array(NP * 3); pg.setAttribute('position', new THREE.BufferAttribute(pp, 3));
    const pts = new THREE.Points(pg, new THREE.PointsMaterial({ color: lk.acc2, size: 2, sizeAttenuation: false, transparent: true, opacity: 0.9, depthWrite: false, blending: ch === 0 ? THREE.NormalBlending : THREE.AdditiveBlending })); pts.frustumCulled = false; T.add(pts);
    const sparks = []; for (let i = 0; i < NP; i++) { sparks.push({ life: 0, vx: 0, vy: 0 }); pp[i * 3 + 1] = -99; }
    const allM = Object.values(mats).concat(Object.values(lit), [inkN]); for (const m of allM) m.userData.o = m.opacity;
    G.traverse(o => o.layers.enable(1)); for (const t of trails) for (const sg of t.segs) sg.traverse(o => o.layers.enable(1));
    return { G, M, R, T, b, mats, lit, lk, allM, trails, drops, pts, pp, sparks, seed: ch * 977 + 13, acc: lk.acc, emit: [b.head, b.chest, b.fiL, b.fiR, b.ftL, b.ftR, b.pel] };
  }
  const _tv = new THREE.Vector3(), _tr = new THREE.Vector3();
  if (typeof window !== 'undefined') window.__rigs = () => rigs;
  function trailStep(rig, face) {
    if (!rig.trails) return; rig.G.updateMatrixWorld(true); const gx = rig.G.position.x, gy = rig.G.position.y;
    for (const t of rig.trails) {
      const a = t.par.localToWorld(_tv.copy(t.off)), n = t.pts.length;
      if (!t.init || t.pts[0].distanceToSquared(a) > 4900) { for (let i = 0; i < n; i++) t.pts[i].set(a.x - face * t.len * i * 0.7, a.y + t.len * i * 0.1, a.z); t.init = 1; }
      t.pts[0].copy(a);
      for (let i = 1; i < n; i++) { const p = t.pts[i], q = t.pts[i - 1], wv = Math.sin(tick * 0.09 + i * 0.7 + t.len) * 0.5;
        _tr.set(q.x - face * t.len * 0.62 * t.back + WIND * 3, q.y - t.len * 0.3 + wv * 2, q.z); p.lerp(_tr, 0.085); p.y -= t.grav; p.x += WIND * (0.5 + i * 0.07); p.y += wv * 0.35; p.z += (a.z - p.z) * 0.2;
        _tr.subVectors(p, q); const L = _tr.length() || 1; p.copy(q).addScaledVector(_tr, t.len / L); if (p.y < 1.5) p.y = 1.5; }
      for (let i = 0; i < n - 1; i++) { const p0 = t.pts[i], p1 = t.pts[i + 1]; place(t.segs[i], [p0.x, p0.y, p0.z], [p1.x, p1.y, p1.z], t.len, [0, 0, 1]); }
    }
    // pool: the main body of it lags behind and smears along the direction of travel; droplets trail after it and lean with the wind
    if (rig.drops) { const air = Math.max(0.25, 1 - gy / 70);
      for (let i = 0; i < rig.drops.length; i++) { const d = rig.drops[i]; if (Math.abs(d.x - gx) > 200) d.x = gx; const tx = gx + d.o * (0.8 + 0.2 * Math.sin(tick * 0.05 + i)) + WIND * i * 3; d.x += (tx - d.x) * (i ? 0.07 + i * 0.012 : 0.2);
        const sm2 = Math.min(1.3, Math.abs(tx - d.x) * 0.06), w = (1 + 0.07 * Math.sin(tick * 0.11 + i * 2)) * d.r * air; d.g.position.set(d.x, 0.45 + i * 0.02, rig.G.position.z + (i ? ((i * 37) % 9) - 4 : 0)); d.g.scale.set(w * (1 + sm2), w * 0.42, 1); } }
    // sparks: shed from the head, hands, feet and core, rising and going with the wind
    if (rig.sparks) { const pp = rig.pp;
      for (let i = 0; i < rig.sparks.length; i++) { const sp = rig.sparks[i];
        if (sp.life <= 0) { if (hsh(rig.seed + tick * 31 + i * 7) < 0.08) { const o = rig.emit[(hsh(rig.seed + tick + i * 3) * rig.emit.length) | 0]; o.getWorldPosition(_tv); pp[i * 3] = _tv.x + (hsh(i + tick) - 0.5) * 6; pp[i * 3 + 1] = _tv.y + (hsh(i * 5 + tick) - 0.5) * 6; pp[i * 3 + 2] = _tv.z + 3; sp.life = 30 + (hsh(i * 11 + tick) * 40 | 0); sp.vx = (hsh(i * 13 + tick) - 0.5) * 0.6; sp.vy = 0.25 + hsh(i * 17 + tick) * 0.6; } else pp[i * 3 + 1] = -99; continue; }
        sp.life--; pp[i * 3] += sp.vx + WIND * 0.9; pp[i * 3 + 1] += sp.vy; sp.vx *= 0.97; }
      rig.pts.geometry.attributes.position.needsUpdate = true; }
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
    const lo = fp.loose == null ? 1 : fp.loose, s1 = Math.sin(tick * 0.052 + rig.seed), s2 = Math.sin(tick * 0.037 + rig.seed * 1.7), s3 = Math.sin(tick * 0.083 + rig.seed * 0.6), bk = rig.boK = (rig.boK || 0) + ((rig.boT || 0) - (rig.boK || 0)) * 0.06, vl = Math.max(-6, Math.min(6, (rig.vel || 0) * face)), po0 = fp.po,
      po = Object.assign({}, po0, { hip: [po0.hip[0] + s1 * 3.2 * lo, po0.hip[1] + s3 * 1.3 * lo - bk * 3, 0], ln: po0.ln + s2 * 6 * lo + bk * 9, ln2: po0.ln2 - s2 * 8 * lo + s3 * 3 + bk * 12, hd: po0.hd + s1 * 9 * lo - bk * 12, rot: po0.rot + s2 * 2.2 * lo + vl * 1.4, hL: [po0.hL[0] + s2 * 3.5 * lo, po0.hL[1] + s3 * 3 * lo - bk * 5, po0.hL[2]], hR: [po0.hR[0] - s1 * 3.5 * lo, po0.hR[1] + s2 * 3 * lo - bk * 5, po0.hR[2]] }), b = rig.b, hip = [po.hip[0], po.hip[1], 0];
    const a1 = po.ln * D, a2 = po.ln2 * D, cb = [hip[0] + Math.sin(a1) * SP1, hip[1] + Math.cos(a1) * SP1, 0], nk = [cb[0] + Math.sin(a2) * SP2, cb[1] + Math.cos(a2) * SP2, 0];
    const tw = po.tw * D, pt = po.pt * D, cax = [Math.sin(tw), 0, Math.cos(tw)], pax = [Math.sin(pt), 0, Math.cos(pt)], mid = [(cax[0] + pax[0]) / 2, 0, (cax[2] + pax[2]) / 2];
    const sy = [nk[0] - Math.sin(a2) * 3, nk[1] - Math.cos(a2) * 3];
    const tg = {}; for (const n of ['hL', 'hR', 'fL', 'fR']) tg[n] = po[n].slice();
    if (po.aim && fp.hb && po.aw > 0) { const h = fp.hb, t = tg[po.aim], tx = po.up ? h[0] + h[2] * 0.6 : h[0] + h[2] - 3, ty = po.up ? h[1] + h[3] - 5 : h[1] + h[3] * po.ay; t[0] += (tx - t[0]) * po.aw; t[1] += (ty - t[1]) * po.aw; }
    const lim = {};
    for (const n of ['L', 'R']) {
      const sd = n === 'L' ? -1 : 1, sh = [sy[0] + cax[0] * SW * sd, sy[1], cax[2] * SW * sd], ht = tg['h' + n], e = po['e' + n];
      const A = ik(sh, [ht[0], ht[1], sh[2] * 0.45 + ht[2]], UA, FA, [-0.35, e, sd * 0.7 * (e > 0 ? 1.4 : 1)], 1.75);
      const hs = [hip[0] + pax[0] * HW * sd, hip[1] - 2, pax[2] * HW * sd], ft = tg['f' + n], kk = po['k' + n];
      const fty = fp.air ? ft[1] : Math.max(4, ft[1]);
      const Lg = ik(hs, [ft[0], fty, sd * 5 + ft[2]], TH, SHN, [0.8, 0.45, sd * (0.4 + kk * 1.6)], 1.6);
      lim[n] = { sh: sh, el: A[0], wr: A[1], hs: hs, kn: Lg[0], an: Lg[1] };
    }
    const sub = (p) => [p[0] - hip[0], p[1] - hip[1], p[2]], Zc = [0, 0, 1];
    const d0 = [Math.sin(a1 * 0.5), Math.cos(a1 * 0.5), 0];
    place(b.pel, sub(hip), sub([hip[0] + d0[0] * 4, hip[1] + d0[1] * 4, 0]), 0, pax);
    place(b.abd, sub([hip[0] + d0[0] * 2, hip[1] + d0[1] * 2, 0]), sub(cb), SP1 - 2, mid);
    place(b.chest, sub(cb), sub(nk), SP2, cax);
    const ha = a2 * 0.45 + po.hd * D, back = Math.max(0, Math.min(1, (Math.abs(po.tw) - 100) / 50)), hz = [-0.5 + back * 0.9 + cax[0] * 0.12, 0, 0.82];
    place(b.head, sub([nk[0], nk[1] + 1, 0]), sub([nk[0] + Math.sin(ha) * 10, nk[1] + 1 + Math.cos(ha) * 10, 0]), 0, hz);
    const wob = i => Math.sin(tick * 0.085 + rig.seed + i * 1.9), lo2 = 0.45 + 0.55 * lo;
    const bend = (k, p0, p1, rest, amt) => { const dx = p1[0] - p0[0], dy = p1[1] - p0[1], L = Math.hypot(dx, dy) || 1, m = [(p0[0] + p1[0]) / 2 - dy / L * amt, (p0[1] + p1[1]) / 2 + dx / L * amt, (p0[2] + p1[2]) / 2]; place(b[k], sub(p0), sub(m), rest / 2, Zc); place(b[k + 'b'], sub(m), sub(p1), rest / 2, Zc); };
    for (const n of ['L', 'R']) { const l = lim[n], sd = n === 'L' ? -1 : 1;
      place(b['sh' + n], sub(l.sh), sub(l.el), 0, cax, 0); bend('ua' + n, l.sh, l.el, UA, (2.6 * wob(sd) + 1.2 * sd) * lo2); bend('fa' + n, l.el, l.wr, FA, -2.8 * wob(sd + 2) * lo2);
      const fd = [l.wr[0] - l.el[0], l.wr[1] - l.el[1], l.wr[2] - l.el[2]]; place(b['fi' + n], sub(l.wr), sub([l.wr[0] + fd[0], l.wr[1] + fd[1], l.wr[2] + fd[2]]), 0, Zc, 0);
      bend('th' + n, l.hs, l.kn, TH, 1.6 * wob(sd + 4) + 1.1); bend('sn' + n, l.kn, l.an, SHN, -1.8 * wob(sd + 6) - 0.6);
      // foot: flat on the floor, turning to follow the shin once it leaves the ground
      const sx = l.an[0] - l.kn[0], sy2 = l.an[1] - l.kn[1], sl = Math.hypot(sx, sy2) || 1, k = fp.air ? 1 : Math.max(0, Math.min(1, (l.an[1] - 5) / 8));
      const fx0 = sd > 0 && !fp.air ? 0.8 : 1, tx = fx0 * (1 - k) + (-sy2 / sl * 0.85 + sx / sl * 0.35) * k, ty = (sx / sl * 0.85 + sy2 / sl * 0.35) * k, tz = (sd > 0 ? 0.6 : -0.15) * (1 - k);
      const a0 = [l.an[0], l.an[1] - 0.4, l.an[2]]; place(b['ft' + n], sub(a0), sub([a0[0] + tx, a0[1] + ty, a0[2] + tz]), 0, [0, 0, 1], -3);
    }
    rig.R.position.set(hip[0], hip[1], 0); rig.R.rotation.set(0, (po.yaw + fp.yaw) * D, -po.rot * D); rig.M.scale.x = face;
    { const sq = 1 + 0.035 * s3 * lo; rig.R.scale.set(1 / sq + Math.abs(vl) * 0.02, sq - Math.abs(vl) * 0.012, 1); if (rig.lk) { rig.mats.body.color.setHex(rig.lk.body).lerp(_gc, bk * 0.6); rig.lit.acc.color.setHex(rig.lk.acc).lerp(_gc, bk * 0.85); } }
    for (const k in rig.mats) { const m = rig.mats[k]; m.emissive.setRGB(flashK + tint[0], flashK + tint[1], flashK + tint[2]); }
  }

  // ----------------------------------------------------------- stage
  function tex(w, h, fn, rep) { const c = document.createElement('canvas'); c.width = w; c.height = h; fn(c.getContext('2d'), w, h); const t = new THREE.CanvasTexture(c); t.magFilter = THREE.NearestFilter; t.minFilter = THREE.NearestFilter; if (rep) { t.wrapS = t.wrapT = THREE.RepeatWrapping; t.repeat.set(rep[0], rep[1]); } return t; }
  function hsh(n) { n = Math.imul(n ^ 0x9e3779b9, 0x85ebca6b); n ^= n >>> 13; n = Math.imul(n, 0xc2b2ae35); return ((n ^ (n >>> 16)) >>> 0) / 4294967296; }
  function buildStage() {
    const st = new THREE.Group();
    // dusk sky: charcoal into plum, one enormous crimson sun, pixel cloud bands, temple and torii silhouettes on the horizon
    const sky = tex(512, 192, (c, w, h) => {
      const g = c.createLinearGradient(0, 0, 0, h); g.addColorStop(0, '#16111b'); g.addColorStop(0.36, '#291a27'); g.addColorStop(0.6, '#472231'); g.addColorStop(0.8, '#742b35'); g.addColorStop(0.88, '#9a3b3a'); g.addColorStop(1, '#3a1d28'); c.fillStyle = g; c.fillRect(0, 0, w, h);
      const sx = 256, sy = 118, sr = 37; for (let dy = -sr; dy <= sr; dy++) { const hw = Math.round(Math.sqrt(sr * sr - dy * dy) / 2) * 2; c.fillStyle = dy > sr * 0.55 ? '#a81f25' : dy > 0 ? '#bb2328' : '#c9282b'; c.fillRect(sx - hw, sy + dy, hw * 2, 1); }
      const band = (x, y, wd, col, a) => { c.globalAlpha = a; c.fillStyle = col; for (let i = 0; i < 4; i++) { const k = [0.55, 1, 0.8, 0.4][i]; c.fillRect(Math.round(x + wd * (1 - k) / 2 + hsh(x + i) * 8), y + i, Math.round(wd * k), 1); } c.globalAlpha = 1; };
      band(262, 98, 86, '#e9a58a', 0.95); band(196, 112, 60, '#d98f7c', 0.9); band(224, 132, 96, '#e9a58a', 0.85); band(300, 124, 44, '#c97a70', 0.8);
      band(40, 70, 90, '#3b2434', 0.9); band(380, 58, 110, '#3b2434', 0.9); band(120, 92, 70, '#5a2c3a', 0.8); band(410, 100, 80, '#5a2c3a', 0.8); band(10, 120, 70, '#6a3340', 0.7);
      c.fillStyle = '#3b2230'; c.beginPath(); c.moveTo(0, 164); for (let x = 0; x <= w; x += 8) c.lineTo(x, 150 - 10 * Math.sin(x * 0.021) - 6 * Math.sin(x * 0.05 + 1)); c.lineTo(w, 192); c.lineTo(0, 192); c.fill();
      const sil = '#241722', win = '#ff7e3a';
      const roof = (x, y, wd) => { c.fillStyle = sil; c.fillRect(x - wd / 2, y, wd, 3); c.fillRect(x - wd / 2 - 3, y + 1, 3, 2); c.fillRect(x + wd / 2, y + 1, 3, 2); c.fillRect(x - wd / 2 - 5, y - 1, 3, 2); c.fillRect(x + wd / 2 + 2, y - 1, 3, 2); c.fillRect(x - wd / 2 + 4, y - 2, wd - 8, 2); };
      const pagoda = (x, by, tiers, wd) => { let y = by; for (let i = 0; i < tiers; i++) { const tw = wd - i * 8; c.fillStyle = sil; c.fillRect(x - tw / 2 + 4, y - 10, tw - 8, 10); if (i < 2) { c.fillStyle = win; for (let k = -1; k <= 1; k += 2) c.fillRect(x + k * (tw / 5) - 2, y - 7, 3, 4); } roof(x, y - 13, tw + 6); y -= 13; } c.fillStyle = sil; c.fillRect(x - 1, y - 8, 2, 8); };
      const torii = (x, by, s) => { c.fillStyle = sil; c.fillRect(x - 9 * s, by - 20 * s, 2 * s, 20 * s); c.fillRect(x + 7 * s, by - 20 * s, 2 * s, 20 * s); c.fillRect(x - 13 * s, by - 23 * s, 26 * s, 2 * s); c.fillRect(x - 15 * s, by - 25 * s, 30 * s, 2 * s); c.fillRect(x - 10 * s, by - 17 * s, 20 * s, 1.5 * s); };
      pagoda(418, 160, 3, 52); pagoda(470, 162, 2, 40); pagoda(356, 162, 2, 30); torii(92, 160, 1.4); torii(150, 162, 0.8); pagoda(34, 162, 2, 36);
      c.fillStyle = sil; c.fillRect(0, 160, w, 32);
      const m = c.createLinearGradient(0, 140, 0, 170); m.addColorStop(0, 'rgba(190,70,60,0)'); m.addColorStop(1, 'rgba(190,70,60,0.35)'); c.fillStyle = m; c.fillRect(0, 140, w, 30);
    });
    const skyM = new THREE.Mesh(new THREE.PlaneGeometry(2600, 975), new THREE.MeshBasicMaterial({ map: sky, fog: false })); skyM.position.set(0, 330, -900); st.add(skyM);
    // rain-wet flagstones with the sun lying across them
    const floor = tex(64, 64, (c) => { c.fillStyle = '#1d161c'; c.fillRect(0, 0, 64, 64); for (let y = 0; y < 4; y++) for (let x = 0; x < 2; x++) { const o = (y % 2) * 16; c.fillStyle = ['#2c2229', '#31262d', '#281e25', '#352830'][(x + y * 3) % 4]; c.fillRect(x * 32 + o - 16, y * 16, 31, 15); c.fillRect(x * 32 + o + 48, y * 16, 31, 15); } for (let i = 0; i < 46; i++) { c.fillStyle = hsh(i * 5) > 0.6 ? 'rgba(230,90,70,0.16)' : 'rgba(255,220,190,0.07)'; c.fillRect(hsh(i) * 64 | 0, hsh(i + 9) * 64 | 0, 2 + (hsh(i + 3) * 4 | 0), 1); } }, [22, 6]);
    const far = new THREE.Mesh(new THREE.PlaneGeometry(3200, 800), new THREE.MeshBasicMaterial({ color: 0x1c131a })); far.rotation.x = -Math.PI / 2; far.position.set(0, -0.6, -690); st.add(far);
    const fl = new THREE.Mesh(new THREE.PlaneGeometry(1500, 380), new THREE.MeshPhongMaterial({ map: floor, shininess: 46, specular: 0x9a6a66 })); fl.rotation.x = -Math.PI / 2; fl.position.set(0, 0, -110); st.add(fl);
    const glare = tex(32, 64, (c) => { for (let y = 0; y < 64; y++) { const a = Math.pow(1 - y / 64, 1.4) * 0.75; for (let x = 0; x < 32; x++) { const e = 1 - Math.abs(x - 15.5) / 16; if (hsh(x * 7 + y * 131) < e * 1.5) { c.fillStyle = 'rgba(214,52,44,' + (a * (0.5 + 0.5 * e)).toFixed(3) + ')'; c.fillRect(x, y, 1, 1); } } } });
    const gl = new THREE.Mesh(new THREE.PlaneGeometry(330, 380), new THREE.MeshBasicMaterial({ map: glare, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, fog: false })); gl.rotation.x = -Math.PI / 2; gl.position.set(0, 0.3, -110); st.add(gl);
    const edge = new THREE.Mesh(new THREE.BoxGeometry(1500, 30, 6), new THREE.MeshBasicMaterial({ color: 0x120c11 })); edge.position.set(0, -15, 80); st.add(edge);
    const silM = new THREE.MeshBasicMaterial({ color: 0x21151d }), sil2 = new THREE.MeshBasicMaterial({ color: 0x2e1c26 }), stone = new THREE.MeshLambertMaterial({ color: 0x4a3a40 }), stone2 = new THREE.MeshLambertMaterial({ color: 0x3a2c32 });
    const B = (w, h, d, x, y, z, m) => { const b = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), m); b.position.set(x, y, z); st.add(b); return b; };
    const gate = (x, z, s, m) => { B(14 * s, 150 * s, 14 * s, x - 78 * s, 75 * s, z, m); B(14 * s, 150 * s, 14 * s, x + 78 * s, 75 * s, z, m); B(230 * s, 12 * s, 18 * s, x, 150 * s, z, m); B(260 * s, 11 * s, 22 * s, x, 164 * s, z, m); B(24 * s, 8 * s, 24 * s, x - 136 * s, 171 * s, z, m).rotation.z = 0.25; B(24 * s, 8 * s, 24 * s, x + 136 * s, 171 * s, z, m).rotation.z = -0.25; B(180 * s, 9 * s, 12 * s, x, 122 * s, z, m); };
    gate(-360, -300, 1, silM); gate(-600, -420, 0.7, sil2); gate(470, -440, 0.75, sil2);
    // broken stone steps and blocks at the edges of the yard
    for (const sgn of [-1, 1]) { B(120, 26, 70, sgn * 560, 13, -150, stone); B(90, 24, 60, sgn * 590, 38, -160, stone2); B(60, 22, 50, sgn * 610, 61, -170, stone); B(46, 16, 40, sgn * 430, 8, -60, stone2).rotation.y = 0.3 * sgn; B(30, 12, 26, sgn * 250, 6, -230, stone2).rotation.y = -0.4 * sgn; }
    st.userData.signs = []; st.userData.lamps = [];
    for (const x of [-250, 250]) { B(20, 8, 20, x, 4, -190, stone); B(9, 40, 9, x, 28, -190, stone); const l = new THREE.Mesh(new THREE.BoxGeometry(17, 15, 17), new THREE.MeshBasicMaterial({ color: 0xff8a3a })); l.position.set(x, 56, -190); st.add(l); st.userData.lamps.push(l); const pl = new THREE.PointLight(0xff8a3a, 0.9, 300, 1.4); pl.position.set(x, 58, -160); pl.layers.enable(1); st.add(pl); (st.userData.lampL = st.userData.lampL || []).push(pl); B(26, 6, 26, x, 67, -190, stone); B(14, 6, 14, x, 73, -190, stone); }
    // maples: dark leaning trunks and a canopy of flat red blocks
    const leafM = [0xd8322e, 0xb81f26, 0xef5a3c, 0x8f1a22, 0xc62828].map(c => new THREE.MeshBasicMaterial({ color: c, fog: false })), bark = new THREE.MeshBasicMaterial({ color: 0x1c1118 });
    const maple = (x, z, s, dir, seed) => { const t1 = B(16 * s, 150 * s, 16 * s, x, 70 * s, z, bark); t1.rotation.z = 0.22 * dir; const t2 = B(11 * s, 110 * s, 11 * s, x - dir * 46 * s, 176 * s, z, bark); t2.rotation.z = 0.75 * dir; const t3 = B(8 * s, 90 * s, 8 * s, x - dir * 120 * s, 214 * s, z + 4, bark); t3.rotation.z = 1.15 * dir;
      for (let i = 0; i < 90; i++) { const a = hsh(seed + i) * 6.283, rr = Math.sqrt(hsh(seed + i * 3 + 1)), lx = x - dir * 95 * s + Math.cos(a) * rr * 150 * s, ly = 222 * s + Math.sin(a) * rr * 52 * s - Math.abs(Math.cos(a)) * 14 * s, sz = (8 + hsh(seed + i * 7) * 16) * s; const m = new THREE.Mesh(new THREE.BoxGeometry(sz, sz * 0.8, 4), leafM[(hsh(seed + i * 11) * 5) | 0]); m.position.set(lx, ly, z + 10 + hsh(seed + i * 13) * 30); st.add(m); } };
    maple(470, -140, 1, 1, 10); maple(-520, -170, 0.9, -1, 400);
    // leaves on the wind
    st.userData.leaves = []; const lg = new THREE.PlaneGeometry(4.5, 3.5);
    for (let i = 0; i < 44; i++) { const m = new THREE.Mesh(lg, leafM[i % 5]); m.material.side = THREE.DoubleSide; m.userData = { x: (hsh(i * 3 + 700) - 0.5) * 1500, y: hsh(i * 5 + 701) * 260, z: -220 + hsh(i * 7 + 702) * 290, v: 0.35 + hsh(i * 11 + 703) * 0.55, p: hsh(i * 13) * 6.283 }; st.add(m); st.userData.leaves.push(m); }
    st.userData.fx = { sky: skyM, glare: gl };
    scene.add(st); return st;
  }

  // ----------------------------------------------------------- atmosphere director
  // One mood is always being blended toward. Which mood depends on the fight: the round, who is close to death, burnout, supers, the KO.
  // A stage only has to hand over its sky planes and floor glare; the lights, weather, wind, rays, mist and grade are shared by every stage.
  let ambL, keyL, rimL, sunL, projL, hitL = [], hitN = 0, WIND = -0.5, atm = null;
  const _clip = [new THREE.Plane(new THREE.Vector3(0, -1, 0), 0)], _noclip = [], _gc = new THREE.Color(0x6a6e78);
  const CSSA = ['#4aa8ff', '#ff4aa0'], CSSB = ['#ffffff', '#17121c'], CSSH = ['#eef4ff', '#ff5aa8'];
  const C3 = h => [(h >> 16 & 255) / 255, (h >> 8 & 255) / 255, (h & 255) / 255];
  const MOOD = {
    dusk: { amb: C3(0xa88a94), ambI: 0.6, key: C3(0xffe8d0), keyI: 0.62, rim: C3(0xff3a2a), rimI: 0.95, fog: C3(0x4a2030), fogN: 560, sky: [1, 1, 1], night: 0, glare: C3(0xd6342c), glareO: 1, wind: 0.5, leaf: 1, rain: 0, snow: 0, ember: 0, ash: 0, vig: C3(0x140408), vigA: 0.3, rays: 0.4, mist: 0.2, bolt: 0 },
    storm: { amb: C3(0x7f7a9c), ambI: 0.52, key: C3(0xcfd6ff), keyI: 0.5, rim: C3(0xd8327a), rimI: 1.05, fog: C3(0x2c2440), fogN: 400, sky: [0.6, 0.52, 0.78], night: 0, glare: C3(0xb02a6a), glareO: 0.75, wind: 1.5, leaf: 0.7, rain: 1, snow: 0, ember: 0, ash: 0, vig: C3(0x06040e), vigA: 0.44, rays: 0.1, mist: 0.5, bolt: 0.35 },
    moon: { amb: C3(0x7088b8), ambI: 0.6, key: C3(0xd8e6ff), keyI: 0.55, rim: C3(0x5aa0ff), rimI: 1.15, fog: C3(0x1c2848), fogN: 500, sky: [1, 1, 1], night: 1, glare: C3(0x6aa0e8), glareO: 0.95, wind: 0.3, leaf: 0, rain: 0, snow: 1, ember: 0, ash: 0, vig: C3(0x02060e), vigA: 0.38, rays: 0.55, mist: 0.4, bolt: 0 }
  };
  const ACC = [C3(0x4aa8ff), C3(0xff4aa0)];
  const mcopy = m => { const r = {}; for (const k in m) r[k] = Array.isArray(m[k]) ? m[k].slice() : m[k]; return r; };
  const mlerp = (a, b, k) => { for (const n in b) { if (Array.isArray(b[n])) for (let i = 0; i < 3; i++) a[n][i] += (b[n][i] - a[n][i]) * k; else a[n] += (b[n] - a[n]) * k; } };
  function atmInit() {
    const A = atm = { cur: mcopy(MOOD.dusk), flash: 0, beat: 0, crit: 0 };
    // night version of the stage sky: the same picture pushed cold, the sun turned into a pale moon
    const fx = stage.userData.fx;
    if (fx && fx.sky) { const src = fx.sky.material.map.image; const t = tex(src.width, src.height, (c, w, h) => { c.drawImage(src, 0, 0); const im = c.getImageData(0, 0, w, h), d = im.data; for (let i = 0; i < d.length; i += 4) { const r = d[i], g = d[i + 1], b = d[i + 2]; if (r > 240 && g > 100 && g < 150) continue; if (r > 160 && g < 60) { d[i] = 206; d[i + 1] = 222; d[i + 2] = 246; continue; } d[i] = b * 0.55 + r * 0.12; d[i + 1] = g * 0.75 + r * 0.1; d[i + 2] = r * 0.72 + b * 0.5 + 14; } c.putImageData(im, 0, 0); for (let i = 0; i < 70; i++) { c.fillStyle = 'rgba(230,240,255,' + (0.3 + hsh(i) * 0.6) + ')'; c.fillRect(hsh(i * 3 + 1) * w | 0, hsh(i * 7 + 2) * h * 0.42 | 0, 1, 1); } });
      A.night = new THREE.Mesh(fx.sky.geometry, new THREE.MeshBasicMaterial({ map: t, fog: false, transparent: true, opacity: 0 })); A.night.position.copy(fx.sky.position); A.night.position.z += 2; scene.add(A.night); }
    // light shafts fanning out from behind the skyline
    A.rays = []; for (let i = 0; i < 7; i++) { const m = new THREE.Mesh(new THREE.PlaneGeometry(46 + (i % 3) * 22, 1100), new THREE.MeshBasicMaterial({ color: 0xffffff, transparent: true, opacity: 0, depthWrite: false, blending: THREE.AdditiveBlending, fog: false })); m.position.set(0, 190, -860); m.rotation.z = (i - 3) * 0.3 + 0.07; scene.add(m); A.rays.push(m); }
    // ground mist in two drifting sheets
    const mt = tex(128, 16, (c, w, h) => { for (let i = 0; i < 90; i++) { c.fillStyle = 'rgba(255,255,255,' + (0.05 + hsh(i) * 0.16).toFixed(3) + ')'; const y = hsh(i * 3) * h | 0; c.fillRect(hsh(i * 7) * w | 0, y, 10 + hsh(i * 11) * 44, 1 + (y > 8 ? 1 : 0)); } }, [3, 1]);
    A.mist = []; for (let i = 0; i < 2; i++) { const t2 = mt.clone(); t2.needsUpdate = true; const m = new THREE.Mesh(new THREE.PlaneGeometry(1700, 70), new THREE.MeshBasicMaterial({ map: t2, transparent: true, opacity: 0.3, depthWrite: false, fog: false })); m.position.set(0, 30, i ? -220 : 50); scene.add(m); A.mist.push(m); }
    // rain as slanted streaks, and one cloud of motes that serves as snow, embers or ash
    const NR = 170, rg = new THREE.BufferGeometry(); A.rp = new Float32Array(NR * 6); rg.setAttribute('position', new THREE.BufferAttribute(A.rp, 3)); A.rain = new THREE.LineSegments(rg, new THREE.LineBasicMaterial({ color: 0xaab8e0, transparent: true, opacity: 0.55, fog: false })); A.rain.frustumCulled = false; scene.add(A.rain);
    A.rd = []; for (let i = 0; i < NR; i++) A.rd.push({ x: (hsh(i * 3 + 5) - 0.5) * 900, y: hsh(i * 7 + 6) * 300, z: -240 + hsh(i * 11 + 7) * 320, v: 7 + hsh(i * 13) * 5 });
    const NM = 150, mg = new THREE.BufferGeometry(); A.mp = new Float32Array(NM * 3); mg.setAttribute('position', new THREE.BufferAttribute(A.mp, 3)); A.motes = new THREE.Points(mg, new THREE.PointsMaterial({ color: 0xffffff, size: 2, sizeAttenuation: false, transparent: true, opacity: 0.9, depthWrite: false, fog: false })); A.motes.frustumCulled = false; scene.add(A.motes);
    A.md = []; for (let i = 0; i < NM; i++) A.md.push({ x: (hsh(i * 3 + 50) - 0.5) * 900, y: hsh(i * 7 + 60) * 280, z: -240 + hsh(i * 11 + 70) * 320, p: hsh(i * 17) * 6.283, v: 0.5 + hsh(i * 19) * 0.8 });
  }
  function atmStep(s) {
    const A = atm; if (!A) return; const t = mcopy(MOOD.dusk); let rate = 0.035, crit = 0, sup = -1;
    if (s) {
      const a = s.p[0], b = s.p[1], need = s.cfg.rounds, ha = a.hp / SIM.CH[a.ch].hp, hb = b.hp / SIM.CH[b.ch].hp, live = s.phase === 'fight' || s.phase === 'intro';
      const final = !s.cfg.training && a.wins === need - 1 && b.wins === need - 1 && need > 1;
      mlerp(t, final ? MOOD.moon : (s.round % 2 === 0 && !s.cfg.training) ? MOOD.storm : MOOD.dusk, 1);
      // both nearly dead: the world leans in. Wind up, embers, a harder backlight, thunder
      if (live && ha <= 0.25 && hb <= 0.25) { crit = 2; t.rimI *= 1.4; t.wind += 1; t.ember = 1; t.keyI *= 0.78; t.rays += 0.35; t.vig = C3(0x3a0408); t.vigA = 0.55; t.bolt = Math.max(t.bolt, 0.5); t.fogN -= 120; }
      else if (live && (ha <= 0.25 || hb <= 0.25)) { crit = 1; t.vig = C3(0x2a0408); t.vigA += 0.1; t.rimI *= 1.15; }
      // burnout drains the colour out of everything
      if (a.bo || b.bo) { t.amb = [0.52, 0.58, 0.66]; t.sky = [t.sky[0] * 0.6, t.sky[1] * 0.68, t.sky[2] * 0.8]; t.keyI *= 0.72; t.rimI *= 0.55; t.ash = 1; t.leaf *= 0.25; t.glareO *= 0.4; t.vig = C3(0x0a1018); t.vigA += 0.12; t.mist += 0.25; }
      // a super takes the light for itself
      for (let i = 0; i < 2; i++) { const p = s.p[i], m = p.st === 'atk' ? SIM.CH[p.ch].mv[p.mv] : null; if (m && m.sup) sup = i; }
      if (sup >= 0) { rate = 0.22; t.ambI *= 0.42; t.keyI *= 0.5; t.rim = ACC[s.p[sup].ch].slice(); t.rimI = 2; t.rays = 1; t.vigA = 0.62; t.glare = ACC[s.p[sup].ch].slice(); t.wind += 0.8; t.sky = [t.sky[0] * 0.45, t.sky[1] * 0.45, t.sky[2] * 0.5]; }
      if (s.phase === 'ko') { rate = 0.1; t.rimI *= 1.5; t.keyI *= 0.6; t.vigA = 0.55; t.wind *= 0.3; t.rays += 0.4; }
    }
    A.crit = crit; mlerp(A.cur, t, rate); const c = A.cur;
    // lightning
    if (c.bolt > 0.2 && A.flash <= 0 && hsh(tick * 7 + 3) < c.bolt * 0.012) A.flash = 9;
    const fl = A.flash > 0 ? (A.flash > 6 || A.flash === 3 ? 1 : 0.25) : 0; if (A.flash > 0) A.flash--;
    WIND = -(c.wind + 0.25 * Math.sin(tick * 0.013) + 0.15 * Math.sin(tick * 0.041));
    ambL.color.setRGB(c.amb[0], c.amb[1], c.amb[2]); ambL.intensity = c.ambI + fl * 0.9; keyL.color.setRGB(c.key[0], c.key[1], c.key[2]); keyL.intensity = c.keyI + fl * 0.5;
    const shim = 1 + 0.06 * Math.sin(tick * 0.21) + 0.04 * Math.sin(tick * 0.057); rimL.color.setRGB(c.rim[0], c.rim[1], c.rim[2]); rimL.intensity = c.rimI * shim * 0.42; sunL.color.setRGB(c.rim[0], c.rim[1], c.rim[2]); sunL.intensity = c.rimI * shim * 1.25 + fl; for (const h of hitL) h.intensity *= 0.8;
    { const q = s && s.proj && s.proj.find(x => x); if (q) { projL.position.set(q.x / F, q.y / F, 24); projL.color.set(CSSA[q.ch]); projL.intensity = 1.5 + 0.3 * Math.sin(tick * 0.9); } else projL.intensity = 0; }
    scene.fog.color.setRGB(c.fog[0], c.fog[1], c.fog[2]); scene.fog.near = c.fogN;
    const fx = stage.userData.fx; if (fx) { if (fx.sky) fx.sky.material.color.setRGB(Math.min(1, c.sky[0] + fl), Math.min(1, c.sky[1] + fl), Math.min(1, c.sky[2] + fl)); if (fx.glare) { fx.glare.material.color.setRGB(c.glare[0], c.glare[1], c.glare[2]); fx.glare.material.opacity = c.glareO * 0.6 * (0.85 + 0.15 * Math.sin(tick * 0.09)); } }
    if (A.night) { A.night.material.opacity = Math.max(0, Math.min(1, c.night)); A.night.visible = c.night > 0.01; }
    for (let i = 0; i < A.rays.length; i++) { const m = A.rays[i]; m.material.color.setRGB(c.rim[0], c.rim[1], c.rim[2]); m.material.opacity = c.rays * 0.085 * (0.6 + 0.4 * Math.sin(tick * 0.02 + i * 1.7)); m.rotation.z = (i - 3) * 0.3 + 0.07 + 0.03 * Math.sin(tick * 0.008 + i); m.visible = stage.visible; }
    for (let i = 0; i < A.mist.length; i++) { const m = A.mist[i]; m.material.color.setRGB(Math.min(1, c.fog[0] * 2.2 + 0.1), Math.min(1, c.fog[1] * 2.2 + 0.1), Math.min(1, c.fog[2] * 2.2 + 0.1)); m.material.opacity = c.mist * (i ? 0.9 : 0.55); m.material.map.offset.x -= WIND * (i ? 0.0005 : 0.0011); m.position.x = camX; m.visible = stage.visible; }
    // rain
    const rp = A.rp, nr = Math.round(A.rd.length * Math.min(1, c.rain)); A.rain.visible = nr > 0 && stage.visible; A.rain.geometry.setDrawRange(0, nr * 2);
    for (let i = 0; i < nr; i++) { const d = A.rd[i]; d.y -= d.v; d.x += WIND * 2.2; if (d.y < 0) { d.y = 290 + (i % 9) * 4; d.x = camX + ((i * 131 + tick * 3) % 900) - 400; } rp[i * 6] = d.x; rp[i * 6 + 1] = d.y; rp[i * 6 + 2] = d.z; rp[i * 6 + 3] = d.x - WIND * 3.4; rp[i * 6 + 4] = d.y + 13; rp[i * 6 + 5] = d.z; }
    if (nr) A.rain.geometry.attributes.position.needsUpdate = true;
    // motes
    const kind = c.ember > 0.3 ? 2 : c.ash > 0.3 ? 3 : c.snow > 0.3 ? 1 : 0, dens = kind === 2 ? c.ember : kind === 3 ? c.ash : c.snow, nm = kind ? Math.round(A.md.length * Math.min(1, dens)) : 0, mp = A.mp;
    A.motes.visible = nm > 0 && stage.visible; A.motes.geometry.setDrawRange(0, nm); A.motes.material.color.setHex(kind === 2 ? 0xff7a3a : kind === 3 ? 0x9a9aa6 : 0xeaf2ff); A.motes.material.blending = kind === 2 ? THREE.AdditiveBlending : THREE.NormalBlending;
    for (let i = 0; i < nm; i++) { const d = A.md[i], vy = kind === 2 ? d.v * 0.9 : kind === 3 ? -d.v * 0.3 : -d.v * 0.75; d.y += vy; d.x += WIND * (kind === 2 ? 1.1 : 0.8) + Math.sin(tick * 0.04 + d.p) * 0.4; if (d.y < 0 || d.y > 290) { d.y = vy > 0 ? 1 : 288; d.x = camX + ((i * 173 + tick * 5) % 900) - 400; } mp[i * 3] = d.x; mp[i * 3 + 1] = d.y; mp[i * 3 + 2] = d.z; }
    if (nm) A.motes.geometry.attributes.position.needsUpdate = true;
    const LV = stage.userData.leaves; if (LV) { const nl = Math.round(LV.length * Math.min(1, c.leaf)); for (let i = 0; i < LV.length; i++) LV[i].visible = i < nl; }
    A.fl = fl;
  }
  // the grade that sits over the picture: a vignette in the mood's colour, a heartbeat when someone is nearly dead, the white of a lightning strike
  function atmOverlay() {
    const A = atm; if (!A) return; const c = A.cur; let a = c.vigA; if (A.crit) { const ph = (tick % 54) / 54, beat = Math.max(0, 1 - ph * 5) + Math.max(0, 1 - Math.abs(ph - 0.26) * 7) * 0.6; a += beat * (A.crit === 2 ? 0.22 : 0.14); }
    const g = g2.createRadialGradient(VW / 2, VH / 2, VH * 0.36, VW / 2, VH / 2, VW * 0.64), col = (c.vig[0] * 255 | 0) + ',' + (c.vig[1] * 255 | 0) + ',' + (c.vig[2] * 255 | 0);
    g.addColorStop(0, 'rgba(' + col + ',0)'); g.addColorStop(1, 'rgba(' + col + ',' + Math.min(0.9, a).toFixed(3) + ')'); g2.fillStyle = g; g2.fillRect(0, 0, VW, VH);
    if (A.fl > 0) { g2.fillStyle = 'rgba(235,240,255,' + (A.fl * 0.22).toFixed(3) + ')'; g2.fillRect(0, 0, VW, VH); }
  }

  function init(c3, c2) {
    ren = new THREE.WebGLRenderer({ canvas: c3, antialias: false, alpha: false, powerPreference: 'high-performance' }); ren.setPixelRatio(1); ren.setSize(VW, VH, false);
    scene = new THREE.Scene(); scene.background = new THREE.Color(0x16111b); scene.fog = new THREE.Fog(0x4a2030, 560, 1500);
    cam = new THREE.PerspectiveCamera(46, 16 / 9, 5, 3200);
    ambL = new THREE.AmbientLight(0xa88a94, 0.6); scene.add(ambL); keyL = new THREE.DirectionalLight(0xffe8d0, 0.62); keyL.position.set(-0.4, 0.9, 1.2); scene.add(keyL); rimL = new THREE.DirectionalLight(0xff3a2a, 0.45); rimL.position.set(0.15, 0.5, -1); scene.add(rimL);
    sunL = new THREE.PointLight(0xff3a2a, 1.1, 0, 1); sunL.position.set(0, 215, -430); scene.add(sunL); projL = new THREE.PointLight(0xffffff, 0, 280, 1.4); scene.add(projL);
    for (let i = 0; i < 2; i++) { const h = new THREE.PointLight(0xffffff, 0, 320, 1.3); scene.add(h); hitL.push(h); }
    for (const l of [ambL, keyL, rimL, sunL, projL, hitL[0], hitL[1]]) l.layers.enable(1);
    stage = buildStage(); atmInit();
    for (let i = 0; i < 2; i++) { const s = new THREE.Mesh(new THREE.CircleGeometry(20, 12), new THREE.MeshBasicMaterial({ color: 0x080410, transparent: true, opacity: 0.55 })); s.rotation.x = -Math.PI / 2; s.position.y = 0.4; scene.add(s); shadows.push(s); }
    g2 = c2.getContext('2d'); g2.imageSmoothingEnabled = false;
  }
  function setFighters(chars) {
    for (const r of rigs) { scene.remove(r.G); if (r.T) scene.remove(r.T); } rigs = [];
    for (let i = 0; i < 2; i++) { const r = buildRig(chars[i], i === 1 && chars[0] === chars[1]); scene.add(r.G); scene.add(r.T); rigs.push(r); }
    fx = []; combo[0] = combo[1] = null; msg = null; superFx = null; shake = 0; flash = 0;
  }
  function toScreen(wx, wy) { const v = new THREE.Vector3(wx, wy, 0).project(cam); return [(v.x + 1) * VW / 2, (1 - v.y) * VH / 2]; }

  // ----------------------------------------------------------- events -> fx
  function events(ev, s) {
    for (const e of ev) {
      const x = (e.x || 0) / F, y = (e.y || 0) / F;
      if ((e.t === 'parry' || e.t === 'pparry') && rigs[e.who]) { const rg = rigs[e.who], hy = y - s.p[e.who].y / F; rg.pt = 0; rg.pk = hy > 58 ? 'pryH' : hy < 26 ? 'pryL' : 'pryM'; rg.pn = (rg.pn || 0) + 1; }
      if (e.t === 'hit' && !e.thr) { const vi = 1 - e.who, v = s.p[vi], am0 = s.p[e.who].st === 'atk' ? SIM.CH[s.p[e.who].ch].mv[s.p[e.who].mv] : null; react[vi] = v.cb ? 'CR' : e.wrong === 'l' || (am0 && am0.hits[0] && am0.hits[0].g === 'l') ? 'LOW' : !am0 || am0.hsp <= 9 ? 'L' : am0.hsp <= 11 ? 'M' : /Kick/.test(am0.n || '') ? 'K' : 'H'; }
      if (e.t === 'hit') { fx.push({ k: 'spark', x, y, lvl: e.lvl, t: 0, ko: e.ko, c: CSSA[s.p[e.who].ch], c2: CSSB[s.p[e.who].ch], d: s.p[e.who].face }); { const hl = hitL[hitN++ & 1]; if (hl) { hl.position.set(x, y, 34); hl.color.set(CSSA[s.p[e.who].ch]); hl.intensity = 1.8 + e.lvl * 1.1; } } if (e.di) { fx.push({ k: 'paint', x, y, t: 0, big: 1 }); fx.push({ k: 'paint', x: x + 7, y: y + 12, t: -3, big: 1 }); fx.push({ k: 'word', x, y: y + 58, t: 0, s: e.crum ? 'CRUMPLE!!' : 'DRIVE IMPACT!', c: '#ff5a3a', z: 16, life: 70 }); shake = 20; flash = 4; } const am = s.p[e.who].st === 'atk' ? SIM.CH[s.p[e.who].ch].mv[s.p[e.who].mv] : null; if (am && am.sup && e.lvl === 2) { fx.push({ k: 'kanji', t: 0, s: ['', '\u65ac', '\u7834', '\u6ec5'][am.sa], c: ['', '#e9a58a', '#f1dcb4', '#d8322e'][am.sa] }); shake = 20; flash = 5; } if (e.wrong) fx.push({ k: 'word', x, y: y + 30, t: 0, s: e.wrong === 'h' ? 'OVERHEAD!' : 'LOW!', c: e.wrong === 'h' ? '#ff8a1e' : '#e9a58a' });  shake = Math.max(shake, e.ko ? 22 : [3, 7, 11][e.lvl]); if (e.lvl >= 2 || e.ko) flash = e.ko ? 6 : 2; combo[e.who] = { n: e.combo, d: e.total || e.dmg, t: 0, ch: e.ch }; }
      else if (e.t === 'block') { fx.push({ k: 'guard', x, y, t: 0 }); shake = Math.max(shake, 2); }
      else if (e.t === 'parry') { fx.push({ k: 'guard', x, y, t: 0, big: 1, c: CSSA[s.p[e.who].ch] }); fx.push({ k: 'spark', x, y, lvl: 0, t: 0, c: CSSA[s.p[e.who].ch], c2: CSSB[s.p[e.who].ch], d: s.p[e.who].face }); }
      else if (e.t === 'pparry') { fx.push({ k: 'guard', x, y, t: 0, big: 1, c: CSSA[s.p[e.who].ch] }); fx.push({ k: 'ripple', x, y, t: 0, c: '#ffffff', r: 60 }); fx.push({ k: 'ring', x, y, t: 0 }); fx.push({ k: 'word', x, y: y + 40, t: 0, s: 'PERFECT PARRY', c: '#f6e7c8' }); flash = 2; }
      else if (e.t === 'armor') { fx.push({ k: 'guard', x, y, t: 0, big: 1 }); fx.push({ k: 'word', x, y: y + 34, t: 0, s: 'ARMOR', c: '#f1dcb4' }); shake = Math.max(shake, 6); }
      else if (e.t === 'splat') { fx.push({ k: 'paint', x, y, t: 0 }); fx.push({ k: 'word', x: x - 30, y: y + 50, t: 0, s: e.stun ? 'STUN!' : 'WALL SPLAT', c: '#d8322e' }); shake = 14; flash = 2; }
      else if (e.t === 'walljump') { fx.push({ k: 'dust', x, y: y + 10, t: 0, big: 1 }); fx.push({ k: 'ripple', x, y: y + 30, t: 0, c: CSSA[s.p[e.who].ch], r: 20 }); }
      else if (e.t === 'burnout') { const p = s.p[e.who]; fx.push({ k: 'spark', x: p.x / F, y: p.y / F + 44, lvl: 2, t: 0, c: '#8a8e98', c2: '#3a3a44', d: p.face }); fx.push({ k: 'spark', x: p.x / F, y: p.y / F + 20, lvl: 2, t: -4, c: '#5a5e68', c2: CSSA[p.ch], d: -p.face }); fx.push({ k: 'ripple', x: p.x / F, y: p.y / F + 40, t: 0, c: '#8a8e98', r: 70 }); shake = Math.max(shake, 10); fx.push({ k: 'word', x: p.x / F, y: 110, t: 0, s: 'BURNOUT', c: '#b8b0c0' }); }
      else if (e.t === 'rush') { const p = s.p[e.who]; fx.push({ k: 'dust', x: p.x / F, y: 0, t: 0, big: 1 }); fx.push({ k: 'word', x: p.x / F, y: 104, t: 0, s: 'DRIVE RUSH', c: '#f08a5a' }); }
      else if (e.t === 'di') { fx.push({ k: 'distart', who: e.who, t: 0 }); shake = Math.max(shake, 4); }
      else if (e.t === 'diclash') { fx.push({ k: 'paint', x, y, t: 0, big: 1 }); fx.push({ k: 'word', x, y: y + 60, t: 0, s: 'IMPACT CLASH!', c: '#fff', z: 16, life: 70 }); flash = 4; shake = 14; }
      else if (e.t === 'wbounce') { fx.push({ k: 'spark', x, y, lvl: 2, t: 0, wall: 1 }); shake = 14; flash = 2; }
      else if (e.t === 'gbounce' || e.t === 'kd') { fx.push({ k: 'dust', x, y: 0, t: 0, big: 1 }); shake = Math.max(shake, e.t === 'kd' ? 5 : 9); }
      else if (e.t === 'jump' || e.t === 'land' || e.t === 'dash' || e.t === 'run') { const p = s.p[e.who]; fx.push({ k: 'dust', x: p.x / F, y: 0, t: 0 }); }
      else if (e.t === 'tech' || e.t === 'clash') fx.push({ k: 'guard', x, y, t: 0, big: 1 });
      else if (e.t === 'pend') fx.push({ k: 'ring', x, y, t: 0 });
      else if (e.t === 'super') superFx = { who: e.who, t: 0, lv: e.lv, name: (e.ca ? 'Critical Art  ' : '') + SIM.CH[s.p[e.who].ch].mv['SA' + e.lv].n };
      else if (e.t === 'ex') fx.push({ k: 'exflash', who: e.who, t: 0 });
      else if (e.t === 'round') msg = { big: s.round >= s.cfg.rounds * 2 - 1 && s.p[0].wins === s.p[1].wins && s.round > 1 ? 'FINAL ROUND' : 'ROUND ' + s.round, jp: '第' + '一二三四五'[s.round - 1] + '回戦', t: 0, dur: 80, col: '#f1dcb4' };
      else if (e.t === 'fight') msg = { big: 'FIGHT!', jp: '始め！', t: 0, dur: 50, col: '#d8322e', slam: 1 };
      else if (e.t === 'ko') msg = e.type === 'time' ? { big: 'TIME OVER', jp: '時間切れ', t: 0, dur: 150, col: '#f1dcb4' } : e.type === 'draw' || e.type === 'dko' ? { big: e.type === 'dko' ? 'DOUBLE K.O.' : 'DRAW', jp: '引き分け', t: 0, dur: 150, col: '#f1dcb4' } : { big: 'K.O.', jp: e.type === 'perfect' ? '完全勝利 PERFECT' : '一本！', t: -34, dur: 190, col: '#d8322e', slam: 1 };
    }
  }

  // ----------------------------------------------------------- 3D frame
  function draw3d(s, menuPan) {
    tick++; atmStep(s);
    const L = stage.userData.lamps; for (let i = 0; i < L.length; i++) { const fk = Math.sin(tick * 0.13 + i * 2) + 0.5 * Math.sin(tick * 0.37 + i); L[i].material.color.setHSL(0.07, 1, 0.55 + 0.06 * fk); if (stage.userData.lampL) stage.userData.lampL[i].intensity = 0.9 + 0.3 * fk; }
    const LV = stage.userData.leaves; for (let i = 0; i < LV.length; i++) { const m = LV[i], u = m.userData; u.y -= u.v; u.x += WIND * (0.9 + u.v); if (u.y < 0) { u.y = 250 + (i % 7) * 6; u.x = camX + ((i * 137) % 900) - 380; } m.position.set(u.x + Math.sin(tick * 0.03 + u.p) * 14, u.y, u.z); m.rotation.set(tick * 0.04 + u.p, tick * 0.05 * u.v, tick * 0.03 + u.p); }
    let tx = 0, tw = 440, ty = 72;
    if (s) {
      const a = s.p[0], b = s.p[1], ax = a.x / F, bx = b.x / F;
      tx = (ax + bx) / 2; tw = Math.max(250, Math.min(470, Math.abs(ax - bx) + 170)); ty = 68 + Math.max(a.y, b.y) / F * 0.42;
      if (superFx && superFx.t < 40) { const p = s.p[superFx.who]; tx = p.x / F; tw = 300 - (superFx.lv || 1) * 14; ty = 62; }
      if (s.phase === 'ko' && s.pt < 150 && s.rw >= 0) { const l = s.p[1 - s.rw]; tx = l.x / F; tw = 250; ty = 44 + l.y / F * 0.6; }
      if (s.slow > 0 && s.phase === 'fight') { tw = Math.min(tw, 300); }
      if (snapCam) { camX = Math.max(-(SIM.STAGE / F + 30 - tw / 2), Math.min(SIM.STAGE / F + 30 - tw / 2, tx)); camW = tw; camY = ty; snapCam = false; }
      const lim = SIM.STAGE / F + 30 - tw / 2; tx = Math.max(-lim, Math.min(lim, tx));
      camX += (tx - camX) * 0.22; camW += (tw - camW) * 0.14; camY += (ty - camY) * 0.2;
      const dim = s.freeze > 0; stage.visible = !dim; scene.background.setHex(dim ? 0x07040a : 0x16111b);
      for (let i = 0; i < 2; i++) {
        const p = s.p[i], o = s.p[1 - i], r = rigs[i]; r.G.visible = true; if (r.T) r.T.visible = true;
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
        r.G.position.set(p.x / F + jx, p.y / F, i === 0 ? 2 : -2); trailStep(r, p.face);
        { r.pt = r.pt == null ? 99 : r.pt + 1; r.boT = p.bo ? 1 : 0; const gx = p.x / F, vx0 = gx - (r.lx == null ? gx : r.lx); r.lx = gx; r.vel = (r.vel || 0) * 0.7 + vx0 * 0.3;
          if (Math.abs(vx0) > 2.2 && (tick & 1)) fx.push({ k: 'echo', x: gx - vx0 * 2, y: p.y / F + 34, t: 0, c: CSSB[p.ch], c2: CSSA[p.ch] });
          if (p.st === 'parry' && tick % 6 === 0) fx.push({ k: 'ripple', x: gx + p.face * 8, y: p.y / F + 38, t: 0, c: CSSA[p.ch], r: 24 }); }
        shadows[i].visible = true; shadows[i].position.x = p.x / F; const sc = Math.max(0.45, 1 - p.y / F / 220); shadows[i].scale.set(sc * (p.st === 'kd' ? 1.8 : 1), sc, 1);
      }
      // projectiles
      while (projMeshes.length < s.proj.length) { const g = new THREE.Group(); const c1 = new THREE.Mesh(new THREE.IcosahedronGeometry(9, 0), new THREE.MeshBasicMaterial({ color: 0xffffff })); const c2 = new THREE.Mesh(new THREE.IcosahedronGeometry(15, 0), new THREE.MeshBasicMaterial({ color: 0x33d8ff, transparent: true, opacity: 0.6 })); const c3 = new THREE.Mesh(new THREE.IcosahedronGeometry(20, 0), new THREE.MeshBasicMaterial({ color: 0x2060ff, transparent: true, opacity: 0.3 })); g.add(c1, c2, c3); scene.add(g); projMeshes.push(g); }
      projMeshes.forEach((g, i) => { const q = s.proj[i]; g.visible = !!q; if (!q) return; const pr = SIM.CH[q.ch].mv[q.k].proj; g.position.set(q.x / F, q.y / F, 0); const k = pr.big ? 1.7 : 1, w = 1 + 0.15 * Math.sin(q.life * 1.3); g.scale.set(k * w * (pr.big ? 2.2 : 1.25), k * w, k * w); g.rotation.x = q.life * 0.5; g.children[1].material.color.setHex(pr.big ? 0xffb07a : q.ch === 0 ? 0xff5a3a : 0xf3d9a8); g.children[2].material.color.setHex(q.ch === 0 || pr.big ? 0xb81c24 : 0xa8763c); });
    } else {
      camX = Math.sin(tick * 0.004) * 240; camW = 470; camY = 96;
      for (const r of rigs) { r.G.visible = !!menuPan; if (r.T) r.T.visible = !!menuPan; } for (const sh of shadows) sh.visible = !!menuPan; projMeshes.forEach(g => g.visible = false); stage.visible = true; scene.background.setHex(0x16111b);
      if (menuPan) { camX = 0; camW = menuPan.w || 340; camY = menuPan.y || 34; const px = menuPan.x || 75; for (let i = 0; i < 2; i++) { const fake = { ch: menuPan.chars[i], st: menuPan.pose && menuPan.pose[i] ? 'win' : 'idle', sub: 'stand', sf: 30, subf: 0, y: 0, vy: 0 }; const fp = fighterPose(fake, fake, tick); applyPose(rigs[i], fp, i ? -1 : 1, 0, [0, 0, 0]); rigs[i].G.position.set(i ? px : -px, 0, i ? -2 : 2); trailStep(rigs[i], i ? -1 : 1); shadows[i].position.x = i ? px : -px; shadows[i].scale.set(1, 1, 1); } }
    }
    let sx = 0, sy = 0; if (shake > 0) { sx = ((shake * 7919) % 5 - 2) * Math.min(3, shake * 0.35); sy = ((shake * 104729) % 5 - 2) * Math.min(3, shake * 0.35); shake--; }
    const dist = camW * 0.71; cam.position.set(camX + sx, camY - 8 + sy, dist); cam.lookAt(camX + sx, camY - 17 + sy, 0);
    ren.render(scene, cam);
    // second pass: the fighters again, flipped through the floor and faded, so the wet stone carries their reflections
    if (rigs.length && rigs[0].G.visible && rigs[0].allM) {
      const bg = scene.background; scene.background = null; ren.autoClear = false; ren.clearDepth(); scene.scale.y = -1; cam.layers.set(1); ren.clippingPlanes = _clip;
      for (const rg of rigs) for (const m of rg.allM) m.opacity = m.userData.o * 0.3;
      ren.render(scene, cam);
      for (const rg of rigs) for (const m of rg.allM) m.opacity = m.userData.o;
      ren.clippingPlanes = _noclip; cam.layers.enableAll(); scene.scale.y = 1; scene.updateMatrix(); scene.matrixWorld.identity(); ren.autoClear = true; scene.background = bg;
    }
  }

  // ----------------------------------------------------------- 2D helpers
  function clear() { g2.setTransform(2, 0, 0, 2, 0, 0); g2.clearRect(0, 0, VW, VH); }
  function txt(str, x, y, size, col, al, jp, ol) {
    g2.font = size + 'px ' + (jp ? FJ : FP); g2.textAlign = al || 'left'; g2.textBaseline = 'top'; x = Math.round(x); y = Math.round(y);
    if (ol !== 0) { g2.fillStyle = ol || '#120b10'; for (const d of [[-1, 0], [1, 0], [0, -1], [0, 1], [1, 1], [-1, 1], [1, -1], [-1, -1], [0, 2], [1, 2]]) g2.fillText(str, x + d[0], y + d[1]); }
    g2.fillStyle = col || '#fff'; g2.fillText(str, x, y);
  }
  function rect(x, y, w, h, c) { g2.fillStyle = c; g2.fillRect(Math.round(x), Math.round(y), Math.round(w), Math.round(h)); }
  function burst(cx, cy, r0, r1, n, col, rot, lw) { g2.strokeStyle = col; g2.lineWidth = lw || 2; g2.beginPath(); for (let i = 0; i < n; i++) { const a = rot + i * 6.283 / n, k = 0.6 + 0.4 * hsh(i * 31 + n); g2.moveTo(cx + Math.cos(a) * r0, cy + Math.sin(a) * r0); g2.lineTo(cx + Math.cos(a) * r1 * k, cy + Math.sin(a) * r1 * k); } g2.stroke(); }

  function drawFx(s) {
    atmOverlay();
    for (let i = fx.length - 1; i >= 0; i--) {
      const f = fx[i], t = f.t++; let dead = false;
      if (f.k === 'exflash') { if (s) { const p = s.p[f.who], q = toScreen(p.x / F, p.y / F + 50); g2.globalAlpha = Math.max(0, 1 - t / 14); burst(q[0], q[1], 10 + t * 3, 30 + t * 5, 14, '#f1dcb4', t * 0.2, 2); g2.globalAlpha = 1; } dead = t > 14; fx[i] = f; if (dead) fx.splice(i, 1); continue; }
      if (f.k === 'distart') { if (s) { const p = s.p[f.who], q = toScreen(p.x / F, p.y / F + 50), a = Math.max(0, 1 - t / 34), cols = ['#ff5a3a', '#e9a58a', '#f1dcb4', '#f08a5a', '#d8322e']; g2.fillStyle = 'rgba(10,6,18,' + (0.5 * a) + ')'; g2.fillRect(0, 0, VW, VH); g2.globalAlpha = a; for (let k = 0; k < 16; k++) { const w = 26 + hsh(k * 5) * 70, yy = q[1] - 56 + k * 7; g2.fillStyle = cols[k % 5]; g2.fillRect(Math.round(q[0] - p.face * (14 + ((t * 9 + k * 23) % 90)) - (p.face > 0 ? w : 0)), Math.round(yy), Math.round(w), 3); } burst(q[0], q[1], 16 + t, 60 + t * 3, 16, cols[(t >> 1) % 5], t * 0.3, 3); g2.globalAlpha = 1; } if (t > 34) fx.splice(i, 1); continue; }
      if (f.k === 'kanji') { const u = t / 40, sc = 1 + Math.max(0, 1 - t / 6) * 2; g2.globalAlpha = u > 0.6 ? Math.max(0, 1 - (u - 0.6) * 2.5) : 1; g2.save(); g2.translate(240, 120); g2.scale(sc, sc); txt(f.s, 0, -48, 96, f.c, 'center', 1, '#120b10'); g2.restore(); g2.globalAlpha = 1; if (t > 40) fx.splice(i, 1); continue; }
      if (t < 0) continue;
      const q = toScreen(f.x, f.y), x = q[0], y = q[1];
      if (f.k === 'spark') {
        // a burst of the attacker's own substance: a bright core, droplets thrown forward and falling, two ripples
        const big = [1, 1.5, 2.1][f.lvl] * (f.ko ? 1.8 : 1), life = f.ko ? 30 : 16 + f.lvl * 4, u = t / life, c1 = f.c || '#f1dcb4', c0 = f.c2 || '#ffffff', dir = f.d || 1;
        if (t < 5) { g2.fillStyle = c0 === '#ffffff' ? '#ffffff' : c1; g2.beginPath(); g2.ellipse(x, y, (13 - t * 2.4) * big, (9 - t * 1.6) * big, 0, 0, 7); g2.fill(); }
        g2.globalAlpha = Math.max(0, 1 - u * u); const n = 9 + f.lvl * 5;
        for (let k = 0; k < n; k++) { const h1 = hsh(k * 7 + (f.x | 0)), h2 = hsh(k * 13 + (f.y | 0) + 5), a = (h1 - 0.5) * 2.6 + (dir > 0 ? 0 : Math.PI), sp = (10 + 46 * h2) * big, e = 1 - Math.pow(1 - u, 2.2), px = x + Math.cos(a) * sp * e, py = y - Math.sin(a) * sp * e * 0.8 + u * u * 26 * (0.4 + h2), sz = Math.max(0.6, (2.2 + 3.2 * hsh(k * 3 + 1)) * big * (1 - u * 0.8));
          g2.fillStyle = k % 3 === 0 ? c0 : c1; g2.beginPath(); g2.ellipse(px, py, sz * (1.2 + (1 - u) * 1.6), sz * 0.6, -Math.atan2(Math.sin(a) * 0.8 - u * 0.9, Math.cos(a)), 0, 7); g2.fill(); }
        g2.strokeStyle = c1; g2.lineWidth = Math.max(1, 3 * (1 - u)); g2.beginPath(); g2.ellipse(x, y, (8 + 30 * Math.sqrt(u)) * big, (4 + 13 * Math.sqrt(u)) * big, 0, 0, 7); g2.stroke();
        if (f.lvl >= 1) { g2.strokeStyle = c0; g2.lineWidth = 1; g2.beginPath(); g2.ellipse(x, y, (4 + 44 * u) * big, (2 + 18 * u) * big, 0, 0, 7); g2.stroke(); }
        g2.globalAlpha = 1; dead = t >= life;
      } else if (f.k === 'guard') {
        const u = t / 12, c1 = f.c || '#f6e7c8'; g2.globalAlpha = 1 - u; g2.strokeStyle = c1; for (let k = 0; k < 3; k++) { const rr = (f.big ? 10 : 6) + 16 * u + k * 5; g2.lineWidth = 2.5 - k * 0.7; g2.beginPath(); g2.ellipse(x, y, rr * 0.55, rr, 0, 0, 7); g2.stroke(); } g2.globalAlpha = 1; dead = t >= 12;
      } else if (f.k === 'ripple') {
        const u = t / 16, rr = (f.r || 20) * (0.5 + u); g2.globalAlpha = (1 - u) * 0.8; g2.strokeStyle = f.c; g2.lineWidth = 1.5; g2.beginPath(); g2.ellipse(x, y, rr * 0.6, rr * 1.5, 0, 0, 7); g2.stroke(); g2.globalAlpha = 1; dead = t >= 16;
      } else if (f.k === 'echo') {
        const u = t / 10; g2.globalAlpha = (1 - u) * 0.3; g2.fillStyle = f.c; g2.beginPath(); g2.ellipse(x, y - 4, 7 * (1 - u * 0.5), 30, 0.12, 0, 7); g2.fill(); g2.strokeStyle = f.c2; g2.lineWidth = 1; g2.stroke(); g2.globalAlpha = 1; dead = t >= 10;
      } else if (f.k === 'dust') {
        const n = f.big ? 7 : 4, life = f.big ? 18 : 12, u = t / life; g2.fillStyle = 'rgba(230,210,220,' + (0.7 * (1 - u)) + ')';
        for (let k = 0; k < n; k++) { const dir = (k % 2 ? 1 : -1) * (1 + (k >> 1)); const s2 = Math.round((f.big ? 6 : 4) * (1 - u * 0.6)); g2.fillRect(Math.round(x + dir * (6 + 22 * u) * (f.big ? 1.4 : 1)) - s2 / 2, Math.round(y - 2 - u * 8 * (1 + (k % 3))) - s2, s2, s2); }
        dead = t >= life;
      } else if (f.k === 'word') { const u = t / (f.life || 44); g2.globalAlpha = u > 0.75 ? Math.max(0, 1 - (u - 0.75) * 4) : 1; txt(f.s, x, y - 14 - t * 0.35, f.z || 8, f.c, 'center'); g2.globalAlpha = 1; dead = t >= (f.life || 44);
      } else if (f.k === 'paint') { const u = t / 22; g2.globalAlpha = 1 - u * u; const cols = ['#ff5a3a', '#e9a58a', '#f1dcb4', '#f08a5a', '#d8322e']; for (let k = 0; k < 14; k++) { const a = k * 0.449 + f.x, rr = (10 + 60 * Math.sqrt(u)) * (0.5 + hsh(k * 7 + 3)) * (f.big ? 1.8 : 1); g2.fillStyle = cols[k % 5]; const s2 = Math.round((f.big ? 18 : 10) * (1 - u * 0.5) * (0.6 + hsh(k))); g2.fillRect(Math.round(x + Math.cos(a) * rr) - s2 / 2, Math.round(y + Math.sin(a) * rr * 0.8) - s2 / 2, s2, s2); } g2.globalAlpha = 1; dead = t >= 22;
      } else if (f.k === 'ring') { const u = t / 9; g2.globalAlpha = 1 - u; g2.strokeStyle = '#f6e7c8'; g2.lineWidth = 2; g2.beginPath(); g2.arc(x, y, 6 + 22 * u, 0, 7); g2.stroke(); g2.globalAlpha = 1; dead = t >= 9; }
      if (dead) fx.splice(i, 1);
    }
    if (flash > 0) { g2.fillStyle = 'rgba(255,255,255,' + Math.min(0.85, flash * 0.28) + ')'; g2.fillRect(0, 0, VW, VH); flash--; }
  }

  function bar(x, y, w, h, frac, redFrac, flip, col, ac) {
    const sk = flip ? -5 : 5, P4 = (x0, x1, c, pd) => { if (x1 <= x0) return; g2.fillStyle = c; g2.beginPath(); g2.moveTo(x0 + sk - pd, y - pd); g2.lineTo(x1 + sk + pd, y - pd); g2.lineTo(x1 + pd, y + h + pd); g2.lineTo(x0 - pd, y + h + pd); g2.fill(); };
    P4(x, x + w, 'rgba(8,5,10,0.78)', 2); P4(x, x + w, 'rgba(255,255,255,0.08)', 0);
    const rw = Math.round(w * redFrac), fw = Math.round(w * frac);
    P4(flip ? x + w - rw : x, flip ? x + w : x + rw, '#b8463c', 0); P4(flip ? x + w - fw : x, flip ? x + w : x + fw, col, 0);
    g2.fillStyle = 'rgba(255,255,255,0.5)'; g2.fillRect((flip ? x + w - fw : x) + sk, y, fw, 1); g2.fillStyle = ac || col; g2.fillRect(flip ? x + w - fw : x, y + h + 1, fw, 1); g2.fillRect(flip ? x - 4 : x + w + 2, y - 2, 2, h + 4);
  }
  function hud(s, o) {
    o = o || {};
    for (let i = 0; i < 2; i++) {
      const p = s.p[i], C = SIM.CH[p.ch], flip = i === 0, x = i ? 262 : 22, w = 196;
      const fr = p.hp / C.hp; bar(x, 16, w, 8, fr, p.red / C.hp, flip, fr < 0.25 ? ((s.frame >> 3) & 1 ? CSSH[p.ch] : '#ff5a3a') : CSSH[p.ch], CSSA[p.ch]);
      txt(C.name, i ? 458 : 22, 29, 8, '#fff', i ? 'right' : 'left'); txt(C.jp, i ? 458 - C.name.length * 8 - 6 : 22 + C.name.length * 8 + 6, 26, 12, '#d8322e', i ? 'right' : 'left', 1);
      if (o.tag && o.tag[i]) txt(o.tag[i], i ? 262 : 218, 29, 8, '#f6e7c8', i ? 'left' : 'right');
      for (let k = 0; k < s.cfg.rounds; k++) { const px = i ? 262 + k * 11 : 211 - k * 11; rect(px - 1, 3, 9, 9, '#120b10'); rect(px, 4, 7, 7, k < p.wins ? '#d8322e' : '#3a2228'); if (k < p.wins) rect(px + 2, 6, 3, 3, '#f1dcb4'); }
      // drive gauge: six bars under the life bar
      for (let k = 0; k < 6; k++) { const gx = flip ? 218 - 15 - k * 16 : 262 + k * 16, fillv = Math.max(0, Math.min(1000, p.dr - k * 1000)) / 1000, w2 = Math.round(13 * fillv); rect(gx - 1, 39, 15, 6, '#120b10'); rect(gx, 40, 13, 4, p.bo ? '#3a3440' : '#30161a'); rect(flip ? gx + 13 - w2 : gx, 40, w2, 4, p.bo ? ((s.frame >> 3) & 1 ? '#9a8488' : '#5a5066') : CSSA[p.ch]); }
      if (p.bo) txt('BURNOUT', flip ? 118 : 362, 38, 8, (s.frame >> 3) & 1 ? '#d8322e' : '#9a8488', flip ? 'right' : 'left');
      // super art gauge: three stocks
      const mx = i ? 338 : 22, mw = 120, lv = Math.floor(p.sa / 1000);
      rect(mx - 2, 252, mw + 4, 10, '#120b10'); rect(mx - 1, 253, mw + 2, 8, '#f4f0e0'); rect(mx, 254, mw, 6, '#1c1418');
      const fw = Math.round(mw * p.sa / 3000); rect(i ? mx + mw - fw : mx, 254, fw, 6, lv >= 3 ? ((s.frame >> 2) & 1 ? '#fff' : '#f1dcb4') : lv >= 1 ? '#ffb020' : '#b8463c'); rect(mx + 39, 254, 2, 6, '#120b10'); rect(mx + 79, 254, 2, 6, '#120b10');
      txt('SA' + lv, i ? mx - 6 : mx + mw + 6, 253, 8, lv >= 3 ? '#f1dcb4' : lv ? '#ffb020' : '#5a4448', i ? 'right' : 'left');
      // guard cue: say what the incoming attack is before it lands
      const cm = p.st === 'atk' ? SIM.CH[p.ch].mv[p.mv] : null;
      if (cm && (cm.od || cm.sup || cm.fire || cm.tint) && p.mf >= cm.su - 2) { const fq = toScreen(p.x / F + p.face * 16, p.y / F + 70); for (let k = 0; k < 10; k++) { const a = (s.frame * 3 + k * 37) % 26; g2.fillStyle = ['#fff3a0', '#f1dcb4', '#ff8a1e', '#d8322e'][(a / 7) | 0]; const z = 5 - (a / 6 | 0); g2.fillRect(Math.round(fq[0] + (hsh(k * 9 + (s.frame >> 2)) - 0.5) * 30), Math.round(fq[1] - a * 1.6 + (hsh(k) - 0.5) * 24), z, z); } }
      if (cm && !cm.isAir && cm.hits[0] && p.mf <= cm.hits[0].e && (cm.hits[0].g === 'h' || cm.thr || cm.di)) { const q = toScreen(p.x / F, p.y / F + 112), oh = cm.hits[0].g === 'h'; if ((s.frame >> 1) & 1 || p.mf > 4) { txt('!', q[0], q[1] - 12, 16, cm.di ? '#ff5a3a' : '#ff8a1e', 'center'); txt(cm.di ? 'IMPACT' : oh ? 'OVERHEAD' : 'THROW', q[0], q[1] + 6, 8, cm.di ? '#ff5a3a' : '#ff8a1e', 'center'); } }
      const c = combo[i];
      if (c) { c.t++; if (c.n >= 2 || c.ch) { const pop = c.t < 5 ? 1 + (5 - c.t) * 0.12 : 1, cx = i ? 400 : 80, a = c.t > 70 ? Math.max(0, 1 - (c.t - 70) / 12) : 1; g2.globalAlpha = a; if (c.n >= 2) { g2.save(); g2.translate(cx, 78); g2.scale(pop, pop); txt('' + c.n, 0, -16, 24, '#f1dcb4', 'right'); txt('HITS', 4, -8, 8, '#fff', 'left'); g2.restore(); txt(c.d + ' DMG', cx, 90, 8, '#ff8a6a', 'center'); txt('連撃', cx + 40, 60, 12, '#d8322e', 'center', 1); } if (c.ch && c.t < 40) txt(c.ch === 2 ? 'PUNISH COUNTER' : 'COUNTER', cx, 104, 8, c.ch === 2 ? '#f1dcb4' : '#e9a58a', 'center'); g2.globalAlpha = 1; } if (c.t > 82) combo[i] = null; }
    }
    rect(226, 8, 28, 26, '#120b10'); rect(227, 9, 26, 24, '#f4f0e0'); rect(228, 10, 24, 22, '#1c1418');
    const tm = s.cfg.training ? '∞' : ('' + Math.max(0, Math.ceil(s.time / 60))).padStart(2, '0'); txt(tm, 240.5, 14, s.cfg.training ? 16 : 12, s.time < 600 && !s.cfg.training ? '#d8322e' : '#f1dcb4', 'center', s.cfg.training);
    if (superFx) {
      const t = superFx.t++, i = superFx.who, lv = superFx.lv || 1, dur = [0, 40, 44, 60][lv], col = ['', '#e9a58a', '#f1dcb4', '#d8322e'][lv];
      if (t < dur + 6) {
        const p = s.p[i], q = toScreen(p.x / F, p.y / F + 55), dir = i ? -1 : 1;
        if (lv >= 2) { g2.fillStyle = lv === 3 ? 'rgba(120,0,24,' + (0.3 + 0.14 * Math.sin(t * 0.8)) + ')' : 'rgba(40,24,0,0.28)'; g2.fillRect(0, 0, VW, VH); }
        g2.globalAlpha = 0.85; burst(q[0], q[1], 40 + (t % 6) * 6, 420, 24 + lv * 10, col, t * 0.05 * dir, 2); burst(q[0], q[1], 26, Math.max(30, 100 - t * 2), 12, '#fff', -t * 0.15, 3);
        for (let k = 0; k < lv; k++) { g2.strokeStyle = k % 2 ? '#fff' : col; g2.lineWidth = 3; g2.beginPath(); g2.arc(q[0], q[1], ((t * 7 + k * 60) % 190) + 10, 0, 7); g2.stroke(); }
        g2.globalAlpha = 1;
        const bh = lv === 3 ? 36 : lv === 2 ? 22 : 10, sl = Math.min(1, t / 6); rect(0, 0, VW, bh * sl, '#120b10'); rect(0, VH - bh * sl, VW, bh * sl, '#120b10'); rect(0, bh * sl, VW, 2, col); rect(0, VH - bh * sl - 2, VW, 2, col);
        if (lv === 3) { g2.globalAlpha = 0.4; for (let k = 0; k < 4; k++) txt('\u6fc0\u7a81', ((k * 190 + t * 9 * dir) % 760 + 760) % 760 - 150, 76, 96, k % 2 ? '#d8322e' : '#f1dcb4', 'left', 1, 0); g2.globalAlpha = 1; }
        const sx = Math.min(1, t / 8), bx = i ? 480 - 300 * sx : -300 + 300 * sx; rect(bx, 186, 300, 36, 'rgba(10,6,18,0.9)'); rect(bx, 186, 300, 2, col); rect(bx, 220, 300, 2, col);
        txt(superFx.name.toUpperCase(), bx + 150, 192, 8, '#fff', 'center'); txt('\u8d85\u5fc5\u6bba  SUPER ART', bx + 132, 202, 16, col, 'center', 1); for (let k = 0; k < 3; k++) rect(bx + 232 + k * 14, 206, 10, 10, k < lv ? col : '#3a2228');
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
    for (const p of s.p) { for (const u of SIM.hurt(p)) dr(u, '#e9a58a'); for (const h of SIM.hitboxes(p)) dr(h.b, '#d8322e'); }
    for (const q of s.proj) dr(SIM.pbox(q), '#d8322e');
  }
  return { snap() { snapCam = true; }, init, setFighters, events, draw3d, clear, drawFx, hud, boxes, txt, rect, burst, toScreen, get g() { return g2; }, get tick() { return tick; }, VW, VH, hsh, say(m) { msg = m; }, FJ, FP };
})();
