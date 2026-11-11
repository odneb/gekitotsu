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
  const TH = 23, SHN = 23, SP1 = 14, SP2 = 17, UA = 17, FA = 19, SW = 8.6, HW = 4.2, DIMS = { TH, SHN, SP1, SP2, UA, FA, SW, HW };
  const NUM = ['ln', 'ln2', 'tw', 'pt', 'hd', 'eL', 'eR', 'kL', 'kR', 'yaw', 'rot', 'ay'], VEC = ['hip', 'hL', 'hR', 'fL', 'fR'];
  const ST = { hip: [-2, 45, 0], ln: -5, ln2: 3, tw: -60, pt: -42, hd: 4, hL: [21, 67, 0], hR: [-11, 39, 0], fL: [13, 4, 0], fR: [-13, 4, 0], eL: -0.7, eR: -0.5, kL: 0.2, kR: 0.5, yaw: 0, rot: 0, ay: 0.5, aim: null, aw: 0 };
  function norm(o, b) { const r = {}; for (const k of NUM) r[k] = o[k] != null ? o[k] : b[k]; for (const k of VEC) { const v = o[k] || b[k]; r[k] = [v[0], v[1], v[2] || 0]; } if (o.ln != null && o.ln2 == null) r.ln2 = o.ln + 4; r.aim = o.aim || null; r.aw = o.aim ? 1 : 0; r.up = o.up || 0; return r; }
  function mix(a, b, k) {
    if (k <= 0) return a; if (k >= 1) return b; const r = {}; for (const n of NUM) r[n] = a[n] + (b[n] - a[n]) * k;
    for (const n of VEC) r[n] = [a[n][0] + (b[n][0] - a[n][0]) * k, a[n][1] + (b[n][1] - a[n][1]) * k, a[n][2] + (b[n][2] - a[n][2]) * k];
    if (b.aim && (!a.aim || a.aim === b.aim)) { r.aim = b.aim; r.aw = (a.aim ? a.aw : 0) + (b.aw - (a.aim ? a.aw : 0)) * k; r.up = b.up; } else if (a.aim) { r.aim = a.aim; r.aw = a.aw * (1 - k); r.up = a.up; } else { r.aim = null; r.aw = 0; r.up = 0; }
    return r;
  }
  const sm = k => k <= 0 ? 0 : k >= 1 ? 1 : k * k * (3 - 2 * k);
  const s = o => norm(o, ST);
  // He sits to crouch: a floating lotus, feet crossed off the floor.
  const CRO = s({ hip: [-1, 19], ln: 4, ln2: 10, tw: -62, pt: -45, hd: 2, hL: [13, 27], hR: [-9, 25], fL: [8, 9, 7], fR: [0, 10, -7], kL: 1.2, kR: 1.2 }), c = o => norm(o, CRO);
  // In the air he folds up small.
  const JMP = s({ hip: [0, 46], ln: 22, ln2: 34, hd: -16, tw: -50, hL: [10, 58, -4], hR: [2, 54, 4], eL: -0.2, eR: -0.2, fL: [12, 30], fR: [-2, 24], kL: 0.5, kR: 0.5 }), j = o => norm(o, JMP);
  // Every pose below belongs to this fighter: a reed-in-the-wind stance, strikes thrown while falling away or toppling in,
  // spins, a floating seat, and a stiff arms-folded glide.
  const PZ = {
    // ---- standing, moving
    idle: ST,
    idle2: s({ hip: [3, 43], ln: 16, ln2: 26, hd: -16, tw: -52, hL: [27, 50], hR: [-3, 46], eL: -0.4, fL: [14, 4], fR: [-12, 4], kL: 0.3, kR: 0.4 }),
    idle3: s({ hip: [-6, 46], ln: -16, ln2: -10, hd: 12, tw: -70, hL: [13, 76], hR: [-17, 34], fL: [9, 8], fR: [-12, 4], kL: 0.1, kR: 0.6 }),
    crouch: CRO, jump: JMP,
    walkA: s({ hip: [1, 44], ln: -10, ln2: -2, hd: 8, tw: -70, hL: [16, 70], hR: [-14, 44], fL: [18, 4], fR: [2, 10, -4], kL: 0.2, kR: 0.7 }),
    walkB: s({ hip: [3, 42], ln: 14, ln2: 20, hd: -12, tw: -46, hL: [28, 52], hR: [-6, 36], fL: [4, 9, 3], fR: [-16, 4], kL: 0.6, kR: 0.3 }),
    wbkA: s({ hip: [-3, 43], ln: 20, ln2: 30, hd: -18, tw: -56, hL: [24, 56], hR: [2, 50], fL: [14, 4], fR: [-18, 8], kL: 0.2, kR: 0.4 }),
    wbkB: s({ hip: [-5, 45], ln: 4, ln2: 10, hd: -2, tw: -66, hL: [18, 66], hR: [-8, 42], fL: [6, 8], fR: [-13, 4], kL: 0.4, kR: 0.5 }),
    prejump: s({ hip: [0, 24], ln: 34, ln2: 44, hd: -26, hL: [-10, 30], hR: [-16, 26], eL: 0.4, eR: 0.4, fL: [10, 4], fR: [-10, 4], kL: 0.6, kR: 0.6 }),
    landP: s({ hip: [1, 22], ln: 40, ln2: 46, hd: -34, tw: -50, hL: [16, 5], hR: [-14, 42, 8], eR: 0.4, fL: [14, 4], fR: [-14, 4], kL: 0.7, kR: 0.5 }),
    fall: s({ hip: [0, 46], ln: -16, ln2: -24, hd: 14, tw: -72, hL: [10, 76, -16], hR: [-12, 74, 16], eL: 0.2, eR: 0.2, fL: [8, 8], fR: [-14, 12], kL: 0.1, kR: 0.3 }),
    dashf: s({ hip: [2, 47], ln: -8, ln2: -4, hd: 8, tw: -78, pt: -60, hL: [9, 64, 5], hR: [5, 60, -5], eL: -0.9, eR: -0.9, fL: [13, 25], fR: [-6, 6], kL: 0.2, kR: 0.2 }),
    drG: s({ hip: [6, 38], ln: 30, ln2: 36, hd: -30, tw: -70, pt: -50, hL: [14, 56, 5], hR: [10, 52, -5], eL: -0.9, eR: -0.9, fL: [14, 14], fR: [-14, 8], kL: 0.2, kR: 0.2 }),
    dashb: s({ hip: [-4, 40], ln: -44, ln2: -52, hd: 30, tw: -80, hL: [16, 60, -14], hR: [-6, 58, 14], eL: 0.3, eR: 0.3, fL: [16, 6], fR: [-6, 4], kL: 0.2, kR: 0.4 }),
    runA: s({ hip: [6, 30], ln: 70, ln2: 78, hd: -62, tw: -30, pt: -34, hL: [-22, 48, -4], hR: [-28, 42, 4], eL: 0.6, eR: 0.6, fL: [26, 6], fR: [-26, 20], kL: 0.1, kR: 0.1 }),
    runB: s({ hip: [6, 32], ln: 68, ln2: 76, hd: -60, tw: -6, pt: 16, hL: [-26, 44, -4], hR: [-24, 46, 4], eL: 0.6, eR: 0.6, fL: [-24, 20], fR: [28, 6], kL: 0.1, kR: 0.1 }),
    runStop: s({ hip: [-5, 36], ln: -18, ln2: -10, hd: 10, tw: -66, hL: [22, 58], hR: [-14, 50], fL: [22, 4], fR: [-8, 4], kL: 0, kR: 0.5 }),
    // ---- defence
    parry: s({ hip: [-2, 44], ln: -2, ln2: 4, hd: 0, tw: -72, hL: [20, 78], hR: [8, 52], eL: -0.3, eR: -0.9, fL: [12, 4], fR: [-14, 4], kL: 0.3, kR: 0.5 }),
    pryH: s({ hip: [-5, 43], ln: -24, ln2: -30, hd: 16, tw: -84, hL: [18, 92], hR: [-8, 46], eL: 0.5, fL: [13, 4], fR: [-13, 4], kL: 0.3, kR: 0.5 }),
    pryH2: s({ hip: [0, 34], ln: 30, ln2: 40, hd: -30, tw: -30, hL: [10, 50], hR: [22, 84], eR: 0.5, fL: [15, 4], fR: [-14, 4], kL: 0.6, kR: 0.6 }),
    pryM: s({ hip: [-3, 44], ln: -4, hd: 4, tw: 40, pt: 10, hL: [24, 62, -10], hR: [-6, 44], eL: 0.1, fL: [11, 4], fR: [-14, 4] }),
    pryM2: s({ hip: [-5, 43], ln: 8, ln2: 16, hd: -8, tw: -100, pt: -70, hL: [6, 50], hR: [24, 58, 6], eR: 0.1, fL: [12, 4], fR: [-15, 4] }),
    pryL: s({ hip: [-3, 47], ln: 8, ln2: 18, hd: -10, tw: -62, hL: [22, 30], hR: [-6, 56], fL: [12, 26], fR: [-9, 4], kL: 0.2 }),
    pryL2: s({ hip: [-3, 52], ln: 12, ln2: 20, hd: -8, tw: -70, hL: [8, 60, -8], hR: [2, 58, 8], fL: [8, 30], fR: [-6, 26], kL: 0.6, kR: 0.6 }),
    blockS: s({ hip: [-4, 47], ln: -4, ln2: 6, hd: -4, tw: -50, hL: [14, 76, 4], hR: [12, 60, -4], eL: -0.2, eR: -0.9, fL: [8, 24], fR: [-8, 4], kL: 0.1, kR: 0.3 }),
    blockC: c({ hip: [-2, 19], ln: 16, ln2: 26, hd: -14, hL: [12, 44, 5], hR: [10, 36, -5], eL: -0.2, eR: -0.8 }),
    // ---- taking it
    hitHi: s({ hip: [-6, 46], ln: -30, ln2: -44, hd: -30, tw: -78, hL: [2, 84, -8], hR: [-16, 66, 8], eL: 0.4, eR: 0.2, fL: [10, 10], fR: [-16, 4] }),
    hitMid: s({ hip: [-8, 40], ln: 36, ln2: 52, hd: 26, tw: -36, hL: [12, 40, 4], hR: [6, 36, -4], fL: [8, 4], fR: [-14, 4], kL: -0.1, kR: -0.1 }),
    hitBig: s({ hip: [-10, 44], ln: -24, ln2: -40, hd: -34, tw: -130, pt: -80, hL: [-12, 80, -10], hR: [-24, 58, 10], eL: 0.5, fL: [8, 14], fR: [-18, 4] }),
    hitLow: s({ hip: [-5, 42], ln: 24, ln2: 30, hd: 8, hL: [18, 58, -8], hR: [0, 56, 8], eL: 0.3, eR: 0.3, fL: [0, 18], fR: [-14, 4] }),
    hitCr: c({ hip: [-5, 21], ln: -26, ln2: -34, hd: -16, hL: [8, 46, -8], hR: [-8, 40, 8], fL: [14, 16], fR: [4, 14] }),
    ahit: s({ hip: [0, 30], ln: -30, ln2: -44, hd: -28, hL: [-10, 56, -12], hR: [-18, 46, 12], fL: [22, 26], fR: [12, 12], eL: 0.2, eR: 0.2 }),
    kd: s({ hip: [0, 6], ln: -86, ln2: -92, hd: -30, tw: -30, pt: -10, hL: [-22, 4, -12], hR: [-34, 5, 12], fL: [30, 16], fR: [36, 4], eL: 0.3, eR: 0.3, kL: 0.2 }),
    kipA: s({ hip: [-2, 14], ln: -100, ln2: -110, hd: 20, hL: [-16, 6, -8], hR: [-22, 6, 8], fL: [-20, 44], fR: [-26, 38], kL: 0.1, kR: 0.1 }),
    kipB: s({ hip: [4, 30], ln: -30, ln2: -10, hd: 0, hL: [-10, 40], hR: [-16, 36], fL: [16, 6], fR: [6, 4], kL: 0.4, kR: 0.4 }),
    situp: s({ hip: [-2, 12], ln: -10, ln2: 8, hd: 10, hL: [10, 18], hR: [-10, 8], fL: [22, 5], fR: [12, 5] }),
    crumA: s({ hip: [-3, 40], ln: 30, ln2: 46, hd: 30, tw: -24, hL: [12, 34], hR: [0, 30], fL: [6, 4, 4], fR: [-10, 4, -3], kL: -0.2, kR: -0.2 }),
    crumB: s({ hip: [4, 13], ln: 70, ln2: 82, hd: 26, tw: -16, hL: [28, 4, -6], hR: [20, 4, 6], fL: [-6, 4], fR: [-20, 4], kL: 0.4, kR: 0.4 }),
    splat: s({ hip: [-4, 46], ln: -6, ln2: -12, hd: -22, tw: -88, hL: [-8, 84, -14], hR: [-10, 78, 14], eL: 1, eR: 1, fL: [4, 10, -6], fR: [-6, 8, 6] }),
    thrown: s({ hip: [0, 36], ln: -20, ln2: -30, hd: -24, hL: [-6, 44, -8], hR: [-14, 38, 8], fL: [10, 8], fR: [-4, 4], eL: 0, eR: 0 }),
    win: s({ hip: [0, 34], ln: -4, ln2: 2, tw: -84, pt: -70, hd: 14, hL: [6, 54, 6], hR: [6, 50, -6], eL: -0.9, eR: -0.9, fL: [8, 24, 8], fR: [0, 25, -8], kL: 1.3, kR: 1.3 }),
    // ---- throws
    tHold: s({ hip: [1, 44], ln: 4, ln2: 10, hd: -10, tw: -70, hL: [22, 62], hR: [16, 50], eL: -0.3, eR: -0.5, fL: [14, 4], fR: [-13, 4] }),
    tReach: s({ hip: [7, 40], ln: 30, ln2: 40, hd: -28, tw: -36, hL: [30, 60], hR: [24, 48], fL: [20, 4], fR: [-16, 5] }),
    tBite: s({ hip: [4, 43], ln: 18, ln2: 34, hd: -36, tw: -40, hL: [26, 62], hR: [20, 54], fL: [15, 4], fR: [-12, 4] }),
    thKick: s({ hip: [8, 42], ln: 26, ln2: 32, hd: -20, tw: -20, hL: [38, 62, -4], hR: [36, 56, 4], eL: 0, eR: 0, fL: [22, 4], fR: [-18, 6] }),
    thUp: s({ hip: [2, 46], ln: -12, ln2: -20, hd: 18, hL: [10, 104], hR: [2, 100], eL: 0.6, eR: 0.6, fL: [12, 4], fR: [-12, 4] }),
    thSlam: s({ hip: [8, 30], ln: 50, ln2: 60, hd: -24, tw: 30, hL: [30, 16], hR: [34, 12], fL: [22, 4], fR: [-16, 6] }),
    bthTuck: s({ hip: [-6, 18], ln: -40, ln2: -20, hd: 20, hL: [10, 40], hR: [8, 34], fL: [12, 30], fR: [6, 6], kL: 0.2, kR: 0.2 }),
    bthToss: s({ hip: [-12, 10], ln: -88, ln2: -96, hd: 10, hL: [-26, 26], hR: [-28, 20], fL: [-8, 56], fR: [4, 8] }),
    // ---- normals
    jabS: s({ hip: [-1, 45], ln: -10, ln2: -4, hd: 6, tw: -84, pt: -50, hL: [40, 70], aim: 'hL', hR: [-14, 46], eR: -0.3, fL: [13, 4], fR: [-13, 4] }),
    mpW: s({ hip: [-3, 45], ln: -14, ln2: -6, tw: -80, pt: -56, hR: [-8, 56], hL: [18, 70], fL: [12, 4], fR: [-13, 5] }),
    mpS: s({ hip: [9, 38], ln: 34, ln2: 40, hd: -30, tw: 44, pt: 10, hR: [34, 56], aim: 'hR', hL: [-8, 48], eL: 0.3, fL: [19, 4], fR: [-16, 12], kR: 0 }),
    hpW: s({ hip: [-5, 44], ln: -22, ln2: -30, hd: 16, tw: -96, pt: -60, hR: [-18, 84], eR: 0.7, hL: [16, 60], fL: [12, 6], fR: [-14, 4] }),
    hpS: s({ hip: [11, 37], ln: 30, ln2: 38, hd: -26, tw: 70, pt: 26, hR: [45, 65], aim: 'hR', hL: [-12, 40], eL: 0.2, fL: [22, 4], fR: [-18, 8] }),
    lkW: s({ hip: [-3, 46], ln: -8, tw: -74, pt: -62, fL: [8, 22], fR: [-8, 4], hL: [14, 66], hR: [-10, 44] }),
    lkS: s({ hip: [-4, 46], ln: -18, ln2: -14, hd: 8, tw: -88, pt: -78, fL: [51, 33], aim: 'fL', fR: [-7, 4], hL: [8, 64, -6], hR: [-14, 52, 8] }),
    mkW: s({ hip: [-3, 46], ln: -16, tw: -80, pt: -68, fL: [6, 34], fR: [-7, 4], hL: [8, 62], hR: [-10, 50] }),
    mkS: s({ hip: [-5, 45], ln: -52, ln2: -58, hd: 30, tw: -94, pt: -86, fL: [51, 64], aim: 'fL', fR: [-5, 4], hL: [-10, 44, -10], hR: [-24, 40, 10], eL: 0.2, eR: 0.2 }),
    hkW: s({ hip: [1, 44], ln: 10, tw: 60, pt: 50, fR: [-4, 20], fL: [4, 4], hL: [10, 60, -8], hR: [-10, 56, 8] }),
    hkS: s({ hip: [2, 45], ln: -30, ln2: -36, hd: 24, tw: 150, pt: 130, fR: [61, 57], aim: 'fR', fL: [2, 4], hL: [-14, 50, -8], hR: [-22, 64, 8] }),
    cjabS: c({ hip: [0, 20], ln: 10, tw: -80, hL: [45, 46], aim: 'hL', hR: [-8, 24] }),
    clkS: c({ hip: [-3, 19], ln: -10, ln2: -4, hd: 6, tw: -76, pt: -60, fL: [53, 8], aim: 'fL', ay: 0.35, fR: [-2, 10, -6], kL: 0, hL: [6, 30], hR: [-14, 8] }),
    cmkW: c({ hip: [-4, 18], ln: -16, fL: [14, 12], kL: 0.4, hR: [-14, 6] }),
    cmkS: c({ hip: [-6, 13], ln: -50, ln2: -40, hd: 28, tw: -86, pt: -74, fL: [57, 7], aim: 'fL', ay: 0.35, fR: [-4, 6, -6], hL: [-6, 28], hR: [-26, 4], kL: 0, eR: -0.3 }),
    cupW: c({ hip: [1, 15], ln: 34, ln2: 44, hd: -24, tw: -70, hR: [4, 16], hL: [14, 30], fL: [10, 4], fR: [-8, 4], kL: 0.8, kR: 0.8 }),
    cupS: s({ hip: [4, 46], ln: -10, ln2: -16, hd: -24, tw: 64, pt: 24, hR: [20, 100], aim: 'hR', up: 1, hL: [-8, 44], fL: [14, 8], fR: [-10, 4] }),
    swS: c({ hip: [-4, 12], ln: -56, ln2: -46, hd: 30, tw: -86, pt: -78, hR: [-26, 4], hL: [-10, 22], fL: [64, 16], aim: 'fL', ay: 0.25, fR: [-20, 6], kL: 0, kR: 0, eR: -0.3 }),
    axW: s({ hip: [0, 48], ln: -20, ln2: -14, hd: 10, tw: -74, pt: -64, fL: [16, 92], fR: [-6, 4], hL: [-4, 56, -10], hR: [-12, 60, 10] }),
    axS: s({ hip: [5, 41], ln: 20, ln2: 26, hd: -14, tw: -62, fL: [50, 42], aim: 'fL', ay: 0.3, fR: [-9, 4], hL: [6, 50], hR: [-12, 62] }),
    fskW: s({ hip: [5, 46], ln: -6, tw: 34, pt: 24, fR: [10, 34], fL: [4, 4], hL: [12, 62], hR: [-6, 56] }),
    fskS: s({ hip: [13, 44], ln: -34, ln2: -30, hd: 16, tw: 64, pt: 58, fR: [79, 39], aim: 'fR', fL: [6, 4], hL: [2, 60, -8], hR: [-16, 48, 8] }),
    cbW: s({ hip: [4, 34], ln: 28, tw: -76, hR: [0, 30], hL: [16, 54], kL: 0.4, kR: 0.4 }),
    cbS: s({ hip: [8, 46], ln: -8, ln2: -14, hd: -20, tw: 56, pt: 18, hR: [30, 84], aim: 'hR', up: 1, hL: [-8, 46], fL: [16, 6], fR: [-10, 8] }),
    tf2W: s({ hip: [3, 45], tw: 44, pt: 34, fR: [0, 26], fL: [5, 4], hL: [10, 62], hR: [-8, 54] }),
    tf2S: s({ hip: [3, 45], ln: -26, ln2: -30, hd: 16, tw: 104, pt: 88, fR: [49, 63], aim: 'fR', fL: [4, 4], hL: [-8, 52, -8], hR: [-16, 60, 8] }),
    tf3S: s({ hip: [3, 47], ln: -34, ln2: -40, hd: 22, tw: 144, pt: 124, fR: [53, 84], aim: 'fR', fL: [4, 6], hL: [-12, 48, -8], hR: [-20, 58, 8] }),
    // ---- air normals (targets are relative to the feet point the sim tracks)
    jlpS: j({ ln: 20, tw: -78, hL: [26, 58], aim: 'hL', ay: 0.4, hR: [-10, 50], fL: [10, 30], fR: [-4, 22] }),
    jmpS: j({ ln: 30, tw: 48, hR: [40, 42], aim: 'hR', hL: [-6, 56], fL: [2, 26], fR: [-12, 30] }),
    jhpW: j({ ln: -10, tw: -74, hR: [-6, 88], hL: [6, 84], eR: 0.6, eL: 0.6, fL: [6, 22], fR: [-10, 16] }),
    jhpS: j({ ln: 42, ln2: 50, hd: -24, tw: 54, hR: [23, 12], aim: 'hR', ay: 0.6, hL: [12, 20], fL: [-6, 32], fR: [-16, 38] }),
    jlkS: j({ ln: 14, fL: [20, 14], aim: 'fL', fR: [-2, 30], hL: [10, 62, -6], hR: [-4, 56, 6] }),
    jmkS: j({ ln: -20, ln2: -16, tw: -90, pt: -78, fL: [42, 27], aim: 'fL', fR: [-8, 34], hL: [2, 62, -8], hR: [-12, 58, 8] }),
    jhkS: j({ ln: -18, tw: 100, pt: 84, fR: [40, 22], aim: 'fR', fL: [-6, 34], hL: [-6, 60, -8], hR: [-14, 56, 8] }),
    njhkS: j({ ln: -6, tw: -88, pt: -78, fL: [39, 45], aim: 'fL', fR: [-34, 42], hL: [8, 70, -10], hR: [-10, 70, 10] }),
    jupK: j({ ln: -24, ln2: -32, hd: 18, tw: -84, rot: -50, fL: [30, 80], aim: 'fL', up: 1, fR: [-6, 28], hL: [2, 48, -8], hR: [-14, 42, 8] }),
    jupB: j({ hip: [0, 48], ln: -36, ln2: -46, hd: 28, tw: -84, rot: -170, fL: [10, 92], fR: [-8, 66], hL: [10, 40, -10], hR: [-12, 36, 10], eL: 0, eR: 0 }), jumpR: j({ rot: -360 }),
    abkW: j({ ln: 36, ln2: 50, hd: -34, tw: -30, hL: [24, 86], hR: [16, 92], eL: 0.6, eR: 0.6, fL: [10, 22], fR: [-8, 26] }),
    abkS: j({ ln: -50, ln2: -66, hd: 38, tw: 150, pt: 100, hL: [-56, 44], hR: [-62, 30], eL: 0.2, eR: 0.2, fL: [18, 32], fR: [6, 16] }),
    abkS2: j({ ln: -34, ln2: -34, hd: 30, tw: 160, pt: 110, hL: [-48, 4], hR: [-54, -6], fL: [16, 36], fR: [8, 20] }),
    jdnS: j({ hip: [0, 50], ln: 14, hd: -10, fL: [6, 0], aim: 'fL', ay: 0.25, fR: [-10, 36], hL: [16, 78, -10], hR: [-16, 74, 10], eL: 0.4, eR: 0.4 }),
    jbkS: j({ ln: 20, tw: 134, pt: 112, fR: [-54, 34], fL: [8, 28], hL: [22, 54], hR: [8, 48] }),
    aspkS: j({ hip: [0, 52], ln: -36, ln2: -26, hd: 24, tw: -84, pt: -74, fL: [38, 6], aim: 'fL', ay: 0.3, fR: [-8, 46], hL: [-16, 78, -8], hR: [-26, 66, 8], eL: 0.3, eR: 0.3 }),
    // ---- specials
    hadW: s({ hip: [-5, 38], ln: -6, tw: -96, pt: -64, hL: [-4, 44, 4], hR: [-10, 40, -2], fL: [14, 4], fR: [-16, 4], eL: -0.4 }),
    hadS: s({ hip: [9, 33], ln: 24, ln2: 28, hd: -20, tw: -84, pt: -40, hL: [40, 60], hR: [-24, 52], eL: 0, eR: 0.2, fL: [26, 4], fR: [-24, 5] }),
    srW: c({ hip: [5, 17], ln: 40, hd: -24, tw: -76, hR: [2, 14], hL: [16, 34], fL: [12, 4], fR: [-8, 4], kL: 0.6, kR: 0.6 }),
    srS: s({ hip: [4, 47], ln: -12, ln2: -18, hd: -22, tw: 114, pt: 64, hR: [14, 99], hL: [-10, 44], fL: [14, 30], fR: [0, 5], eR: 0, kL: 0.1 }),
    srF: s({ hip: [2, 46], ln: -4, tw: 30, hR: [6, 88], hL: [-6, 50, -8], fL: [8, 16], fR: [-8, 8] }),
    tatS: s({ hip: [0, 46], ln: -24, ln2: -20, hd: 14, tw: -90, pt: -86, fR: [44, 48], fL: [2, 10], hL: [2, 66, -14], hR: [-10, 62, 14] }),
    atatS: j({ ln: 26, ln2: 30, hd: -12, tw: -90, pt: -86, fR: [40, 38], fL: [2, 30], hL: [6, 62, -12], hR: [-6, 60, 12] }),
    dlW: s({ hip: [4, 26], ln: 40, hd: -24, tw: -64, hL: [-8, 36], hR: [-16, 30], eL: 0.4, eR: 0.4, kL: 0.4, kR: 0.4 }),
    dlF: j({ ln: 36, ln2: 48, tw: 60, fL: [-2, 32], fR: [-12, 38], hL: [12, 50, -6], hR: [2, 46, 6] }),
    dlS: j({ hip: [-2, 50], ln: -54, ln2: -60, hd: 36, tw: 140, pt: 120, fR: [46, 14], aim: 'fR', ay: 0.75, fL: [-8, 36], hL: [-16, 60, -10], hR: [-24, 68, 10] }),
    jinW: s({ hip: [3, 45], ln: 6, tw: 150, pt: 110, fR: [-4, 22], fL: [6, 4], hL: [8, 62, -8], hR: [-10, 56, 8] }),
    jinS: s({ hip: [6, 46], ln: -22, ln2: -26, hd: 16, tw: 138, pt: 118, fR: [67, 60], aim: 'fR', ay: 0.9, fL: [5, 4], hL: [-10, 52, -8], hR: [-18, 64, 8] }),
    kazeS: s({ hip: [8, 34], ln: -20, ln2: -14, hd: 10, tw: -76, pt: -66, fL: [64, 7], aim: 'fL', ay: 0.45, fR: [-10, 4], hL: [10, 58, -6], hR: [-8, 50, 8], kR: 0.6 }),
    goW: s({ hip: [2, 51], ln: -10, tw: 64, pt: 54, fR: [14, 86], fL: [2, 10], hL: [8, 60, -10], hR: [-10, 56, 10] }),
    goS: s({ hip: [6, 40], ln: 24, tw: 44, pt: 34, fR: [56, 40], aim: 'fR', ay: 0.25, fL: [4, 4], hL: [8, 50], hR: [-10, 60] }),
    senS: s({ hip: [8, 47], ln: -40, ln2: -44, hd: 26, tw: -96, pt: -86, fL: [71, 69], aim: 'fL', fR: [3, 4], hL: [-10, 48, -8], hR: [-20, 56, 8] }),
    kasW: s({ hip: [2, 46], ln: -14, tw: -84, pt: -74, fL: [8, 40], fR: [-4, 4], hL: [4, 58], hR: [-10, 52] }),
    kasS: s({ hip: [6, 45], ln: -48, ln2: -50, hd: 30, tw: -98, pt: -90, fL: [75, 50], aim: 'fL', fR: [-4, 4], hL: [-14, 46, -8], hR: [-26, 50, 8] }),
    diW: s({ hip: [-6, 41], ln: -8, tw: -130, pt: -80, hR: [-22, 60], hL: [10, 56], fL: [13, 4], fR: [-16, 4], eR: 0.2 }),
    diS: s({ hip: [12, 35], ln: 26, ln2: 32, hd: -36, tw: 124, pt: 64, hR: [50, 56], aim: 'hR', hL: [-14, 50], fL: [23, 4], fR: [-17, 8], eL: 0.2 }),
    diR: s({ hip: [8, 42], ln: 6, hd: -40, tw: 170, pt: 96, hR: [18, 60], hL: [0, 68], fL: [20, 4], fR: [-14, 6], eR: 0.4, eL: 0.4 }),
    model: s({ hip: [0, 50.6], ln: 1, ln2: -2, tw: 0, pt: 0, hd: 5, hL: [2, 49.5, -15], hR: [2, 49.5, 15], eL: -0.6, eR: -0.6, fL: [1, 4, -4], fR: [1, 4, 4], kL: 0, kR: 0 }),
    homeW: c({ hip: [-5, 21], ln: 62, ln2: 70, hd: -48, tw: -60, hL: [-22, 34], hR: [-28, 26], fL: [14, 4], fR: [-12, 4], eL: 0.5, eR: 0.5, kL: 0.6, kR: 0.6 }),
    homeT: j({ hip: [-2, 46], ln: 54, ln2: 64, hd: -44, hL: [-16, 46], hR: [-22, 40], fL: [8, 32], fR: [-6, 28] }),
    homeS: j({ hip: [0, 44], ln: 6, ln2: 8, hd: -28, tw: -70, pt: -36, hR: [6, 96], aim: 'hR', up: 1, hL: [-8, 34], fL: [2, 2], fR: [-6, -6], eL: 0.2, eR: 0 }),
    caS: j({ hip: [0, 50], ln: 34, ln2: 40, hd: -26, tw: 44, fL: [10, -4], aim: 'fL', ay: 0.2, fR: [-12, 38], hL: [20, 84, -10], hR: [-18, 80, 10], eL: 0.5, eR: 0.5 }),
    topS: j({ hip: [0, 46], ln: -8, ln2: -6, hd: 4, tw: -90, pt: -86, fL: [40, 40], aim: 'fL', fR: [-4, 20], hL: [20, 70, -18], hR: [-20, 70, 18], eL: 0.3, eR: 0.3 }),
    topS2: j({ hip: [0, 46], ln: 10, ln2: 14, hd: -6, tw: 90, pt: 86, fR: [40, 30], aim: 'fR', fL: [-4, 24], hL: [-18, 66, -18], hR: [18, 66, 18], eL: 0.3, eR: 0.3 }),
    sa1S: j({ hip: [0, 48], ln: -30, hd: 22, tw: 142, pt: 122, fR: [58, 60], aim: 'fR', fL: [-6, 28], hL: [-12, 56, -8], hR: [-20, 68, 8] })
  };
  const PZK = {};
  // a fighter can bring its own version of a pose; anything it does not override is shared
  let PCH = 0;
  const P = n => ((PCH === 1 || KM) && PZK[n]) || PZ[n] || ST;
  function kf(keys, t) {
    if (t <= keys[0][0]) return P(keys[0][1]);
    for (let i = 1; i < keys.length; i++) if (t <= keys[i][0]) { const a = keys[i - 1], b = keys[i], e = b[2]; let u = (t - a[0]) / Math.max(1e-6, b[0] - a[0]); u = e === 'i' ? u * u * u : e === 'o' ? 1 - (1 - u) * (1 - u) * (1 - u) : e === 'l' ? u : sm(u); return mix(P(a[1]), P(b[1]), u); }
    return P(keys[keys.length - 1][1]);
  }
  // attack timelines in phase time: 0-1 startup, 1-2 active, 2-3 recovery. rc is a pose he passes through on the way home;
  // ys turns him right round over a span of phase time; gh leaves afterimages.
  const NB = { idle: 1, crouch: 1, jump: 1 }, HOMEP = b => NB[b] ? b : 'idle';
  const A = (base, w, st, rc, x) => Object.assign({ k: rc ? [[0, base], [0.55, w, 'o'], [1, st, 'i'], [2, st], [2.5, rc, 'o'], [3, HOMEP(base)]] : [[0, base], [0.55, w, 'o'], [1, st, 'i'], [2, st], [3, HOMEP(base)]] }, x);
  const Q = (base, st, rc, x) => Object.assign({ k: rc ? [[0, base], [1, st, 'i'], [2, st], [2.5, rc, 'o'], [3, HOMEP(base)]] : [[0, base], [1, st, 'i'], [2, st], [3, HOMEP(base)]] }, x);
  const AN = {
    '5LP': Q('idle', 'jabS', 'idle3'), '5MP': A('idle', 'mpW', 'mpS', 'idle2'), '5HP': A('idle', 'hpW', 'hpS', 'idle2'),
    '5LK': A('idle', 'lkW', 'lkS', 'idle3'), '5MK': A('idle', 'mkW', 'mkS', 'idle3'), '5HK': A('idle', 'hkW', 'hkS', 'idle3', { ys: [0.08, 1, 360], gh: 1 }),
    '2LP': Q('crouch', 'cjabS'), '2LK': Q('crouch', 'clkS'), '2MK': A('crouch', 'cmkW', 'cmkS'), '2HP': A('crouch', 'cupW', 'cupS', 'fall'), '2HK': A('crouch', 'cmkW', 'swS', null, { ys: [0.1, 1, 360] }),
    TK: A('idle', 'axW', 'axS', 'idle2'), FSK: A('idle', 'fskW', 'fskS', 'idle3'), CB2: A('mpS', 'cbW', 'cbS', 'fall'), TFK2: A('mkS', 'tf2W', 'tf2S'), TFK3: A('tf2S', 'mkW', 'tf3S', 'idle3', { ys: [0.05, 1, 360], gh: 1 }),
    jUP: { k: [[0, 'jump'], [1, 'jupK', 'i'], [1.5, 'jupB'], [2, 'jupB'], [2.6, 'jumpR'], [3, 'jumpR']] }, AUP: { k: [[0, 'jump'], [1, 'tatS', 'i'], [1.8, 'tatS'], [2, 'srS', 'i'], [3, 'fall']], spin: 1, gh: 1 }, ABK: { k: [[0, 'jump'], [0.7, 'abkW', 'o'], [1, 'abkS', 'i'], [1.5, 'abkS'], [2, 'abkS2'], [3, 'fall']] },
    throw: A('idle', 'tHold', 'tReach'), jDN: A('jump', 'jhpW', 'jdnS'), jBK: Q('jump', 'jbkS'), ASPK: Q('jump', 'aspkS', null, { gh: 1 }),
    jLP: Q('jump', 'jlpS'), jMP: Q('jump', 'jmpS'), jHP: A('jump', 'jhpW', 'jhpS'), jLK: Q('jump', 'jlkS'), jMK: Q('jump', 'jmkS'), jHK: Q('jump', 'jhkS'), njHK: Q('jump', 'njhkS'),

    DI: { k: [[0, 'idle'], [0.5, 'diW', 'o'], [0.9, 'diW'], [1, 'diS', 'i'], [2, 'diS'], [2.3, 'diR', 'o'], [2.7, 'diR'], [3, 'idle']], gh: 1 },
    DREV: { k: [[0, 'blockS'], [0.6, 'diW', 'o'], [1, 'diS', 'i'], [2, 'diS'], [2.4, 'diR', 'o'], [3, 'idle']], gh: 1 },
    HAD: { k: [[0, 'idle'], [0.35, 'idle3', 'o'], [0.7, 'hadW', 'o'], [1, 'hadS', 'i'], [2.5, 'hadS'], [2.8, 'idle2', 'o'], [3, 'idle']] },
    SRK: { k: [[0, 'idle'], [0.7, 'srW', 'o'], [1, 'srS', 'i'], [2, 'srS'], [2.5, 'srF'], [3, 'fall']], ys: [1.05, 2.3, 360], gh: 1 },
    TAT: { k: [[0, 'idle'], [0.5, 'tf2W'], [1, 'tatS', 'i'], [2, 'tatS'], [2.5, 'fall'], [3, 'idle']], spin: 1, gh: 1 },
    ATAT: { k: [[0, 'jump'], [1, 'atatS', 'i'], [2, 'atatS'], [3, 'fall']], spin: 1, gh: 1 },
    HOME: Q('idle', 'homeS'), AHOME: Q('jump', 'homeS'), AHOME_OD: Q('jump', 'homeS'),
    DL: { f: m => [[0, 'idle'], [Math.max(2, m.rise ? m.rise[0] - 8 : 3), 'dlW', 'o'], [m.rise ? m.rise[0] : 6, 'dlW'], [m.hits[m.hits.length - 1].s - 5, 'dlF'], [m.hits[m.hits.length - 1].s, 'dlS', 'i'], [m.lastAct + 2, 'dlS'], [m.lastAct + 10, 'fall']], yaw: m => [m.rise ? m.rise[0] : 6, m.hits[m.hits.length - 1].s - 1, 360], gh: 1 },
    JIN: A('idle', 'jinW', 'jinS', 'idle3'), KAZE: Q('jinS', 'kazeS', 'idle2'), GORAI: { k: [[0, 'jinS'], [0.55, 'goW', 'o'], [1, 'goS', 'i'], [2, 'goS'], [2.5, 'landP'], [3, 'idle']] }, SENKA: A('jinS', 'kasW', 'senS', 'idle3'), KASAI: A('kazeS', 'kasW', 'kasS', 'idle3'),
    SA1: { f: m => { const k = [[0, 'crouch'], [4, 'srW', 'o'], [6, 'cupS', 'i'], [9, 'cupS'], [11, 'jump']], st = ['topS', 'topS2', 'njhkS', 'topS2']; for (let i = 1; i < m.hits.length - 1; i++) { const h = m.hits[i]; k.push([h.s - 2, 'jump']); k.push([h.s, st[(i - 1) % 4], 'i']); k.push([h.s + 1, st[(i - 1) % 4]]); } const l = m.hits[m.hits.length - 1]; k.push([l.s - 3, 'jhpW']); k.push([l.s, 'dlS', 'i']); k.push([l.s + 6, 'dlS']); k.push([l.s + 16, 'fall']); return k; }, yaw: m => [11, m.hits[m.hits.length - 1].s - 1, 1800], gh: 1 },
    SA2: { f: m => { const k = [[0, 'idle3']], st = ['mpS', 'mkS', 'hpS', 'jinS'], w = ['mpW', 'mkW', 'hpW', 'jinW'], n = 4; for (let i = 0; i < n; i++) { const h = m.hits[i]; if (i) k.push([h.s - 3, w[i]]); k.push([h.s, st[i], 'i']); k.push([Math.min(h.e, h.s + 2), st[i]]); } k.push([m.rise[0] - 2, 'srW']); k.push([m.rise[0] + 2, 'tatS', 'i']); const l = m.hits[m.hits.length - 1]; k.push([l.s + 8, 'tatS']); k.push([l.s + 22, 'fall']); return k; }, yaw: m => [m.rise[0] - 1, m.hits[m.hits.length - 1].s + 8, 2160], gh: 1 },
    CA: { f: m => { const k = [[0, 'idle3']], st = ['jabS', 'mkS', 'mpS', 'hpS'], w = ['idle3', 'mkW', 'mpW', 'hpW'], air = ['jmkS', 'jhkS', 'njhkS']; for (let i = 0; i < 4; i++) { const h = m.hits[i]; if (i) k.push([h.s - 4, w[i], 'o']); k.push([h.s, st[i], 'i']); k.push([h.s + 2, st[i]]); } const l = m.hits[4]; k.push([l.s - 5, 'cupW', 'o']); k.push([l.s, 'cupS', 'i']); k.push([l.s + 5, 'cupS']); for (let i = 5; i < 8; i++) { const h = m.hits[i]; k.push([h.s - 2, 'jump']); k.push([h.s, air[i - 5], 'i']); k.push([h.s + 1, air[i - 5]]); } const z = m.hits[8]; k.push([z.s - 4, 'jhpW', 'o']); k.push([z.s, 'caS', 'i']); k.push([z.s + 12, 'caS']); k.push([z.s + 24, 'fall']); return k; }, yaw: m => [m.hits[4].s, m.hits[8].s - 3, 1440], gh: 1 },
    SA3: { f: m => { const k = [[0, 'dashf']], st = ['hpS', 'cbS', 'mkS', 'mpS'], w = ['hpW', 'cbW', 'mkW', 'mpW']; for (let i = 0; i < m.hits.length - 1; i++) { const h = m.hits[i]; if (i) k.push([h.s - 4, w[i % 4], 'o']); k.push([h.s, st[i % 4], 'i']); k.push([h.s + 2, st[i % 4]]); } const l = m.hits[m.hits.length - 1]; k.push([l.s - 5, 'srW', 'o']); k.push([l.s, 'srS', 'i']); k.push([l.s + 18, 'srS']); k.push([l.s + 34, 'win', 'o']); k.push([l.s + 58, 'win']); k.push([l.s + 70, 'fall']); return k; }, yaw: m => [m.hits[m.hits.length - 1].s, m.hits[m.hits.length - 1].s + 30, 720], gh: 1 }
  };
  const anOf = n => AN[n] || AN[n.replace(/^Q/, '').replace(/_(L|M|H|OD)$/, '')] || null;
  const fcache = {};
  const react = ['', ''], RX = { L: 'hitHi', M: 'hitMid', H: 'hitBig', K: 'hitBig', LOW: 'hitLow', CR: 'hitCr' };

  const cp = o => Object.assign({}, o, { hip: o.hip.slice(), hL: o.hL.slice(), hR: o.hR.slice(), fL: o.fL.slice(), fR: o.fR.slice() });
  const lift = (po, b) => { po.hip[1] += b; po.fL[1] += b; po.fR[1] += b; po.hL[1] += b; po.hR[1] += b; return po; };
  function fighterPose(p, o, sFrame, idx) {
    const C = SIM.CH[p.ch], rg = rigs[idx] || {}; let po, hb = null, yaw = 0, echo = 0, loose = 0.6; PCH = p.ch;
    if (p.st !== 'jump' && p.st !== 'atk') rg.jrot = 0;
    switch (p.st) {
      case 'idle':
        if (p.sub === 'crouch') { po = mix(P('idle'), P('crouch'), sm((p.subf + 1) / 5)); if (p.subf > 4) po = lift(cp(po), Math.sin(sFrame * 0.07 + p.ch) * 1.5 * Math.min(1, (p.subf - 4) / 8)); loose = 0.7; }
        else if (p.sub === 'walkf') { const k = (Math.sin(p.subf * 0.21) + 1) / 2; po = mix(P('idle'), mix(P('walkA'), P('walkB'), sm(k)), Math.min(1, (p.subf + 1) / 5)); loose = 1.2; }
        else if (p.sub === 'walkb') { const k = (Math.sin(p.subf * 0.19) + 1) / 2; po = mix(P('idle'), mix(P('wbkA'), P('wbkB'), sm(k)), Math.min(1, (p.subf + 1) / 5)); loose = 1.2; }
        // the stance never settles: he tips forward, catches himself, teeters back on his heel
        else { const t = sFrame * 0.048 + p.ch * 2.1, a = (Math.sin(t) + 1) / 2, b = Math.max(0, Math.sin(t * 0.61 + 1.3)); po = mix(mix(P('idle'), P('idle2'), sm(a) * 0.8), P('idle3'), sm(b) * 0.85); loose = 1.5; }
        break;
      case 'prejump': po = mix(P('idle'), P('prejump'), sm((p.sf + 1) / 3)); break;
      case 'land': po = mix(P('landP'), P('idle'), sm(p.sf / 5)); break;
      // a tight tuck going up, opening out on the way down; a jump with any travel in it is a somersault
      case 'jump': { const u = Math.max(0, Math.min(1, (1125 - p.vy) / 2250)); po = cp(mix(P('jump'), P('fall'), sm((u - 0.55) / 0.4))); if (p.jd) po.rot = p.jd * 360 * sm(Math.min(1, u / 0.8)); rg.jrot = po.rot; break; }
      // dashes are a glide: upright, arms folded, one knee up, an inch off the floor
      case 'dashf': { po = cp(p.sf < 3 ? mix(P('idle'), P('dashf'), sm((p.sf + 1) / 3)) : mix(P('dashf'), P('idle'), sm((p.sf - 12) / 6))); const b = Math.sin(Math.min(1, p.sf / 16) * Math.PI) * 5; po.hip[1] += b; po.fL[1] += b; po.fR[1] += b; break; }
      case 'dashb': { po = cp(mix(P('dashb'), P('idle'), sm((p.sf - 13) / 8))); const b = Math.sin(Math.min(1, p.sf / 14) * Math.PI) * 6; po.hip[1] += b; po.fL[1] += b; po.fR[1] += b * 0.8; break; }
      case 'run': { const cyc = mix(P('runA'), P('runB'), (1 - Math.cos(p.sf * 0.56)) / 2); po = p.sf > 36 ? mix(cyc, P('runStop'), sm((p.sf - 36) / 8)) : mix(P('idle'), cyc, sm((p.sf + 1) / 4)); break; }
      case 'drush': { po = cp(p.sf > 24 ? mix(P('drG'), P('idle'), sm((p.sf - 24) / 10)) : mix(P('dashf'), P('drG'), sm(p.sf / 5))); const b = 3 + Math.sin(p.sf * 0.5) * 1.2; po.hip[1] += b; po.fL[1] += b; po.fR[1] += b; break; }
      case 'parry': { const base = mix(P('idle'), P('parry'), sm((p.sf + 1) / 3)); if (rg && rg.pk && rg.pt < 18) { const w = rg.pt < 2 ? (rg.pt + 1) / 2 : 1 - sm((rg.pt - 7) / 11); po = mix(base, P(rg.pk + (rg.pn & 1 ? '2' : '')), w); } else po = base; loose = 1; break; }
      case 'prec': po = mix(P('parry'), P('idle'), sm(p.sf / 11)); break;
      case 'block': po = P(p.cb ? 'blockC' : 'blockS'); loose = 0.5; break;
      case 'hit': {
        if (p.crum === 1) { po = mix(P('crumA'), P('crumB'), sm((p.sf - 14) / 46)); po = mix(P('hitMid'), po, sm(p.sf / 10)); break; }
        if (p.crum === 2) { po = P('splat'); break; }
        const h = P(p.cb ? 'hitCr' : RX[react[idx]] || (p.hitHi ? 'hitHi' : 'hitMid')), b = P(p.cb ? 'crouch' : 'idle'), tot = Math.max(1, p.sf + p.stun);
        po = mix(mix(b, h, Math.min(1, (p.sf + 1) / 2)), b, sm((p.sf - tot * 0.55) / (tot * 0.45))); break; }
      // knocked into the air he goes limp and turns over like a thrown coat
      case 'ahit': po = cp(P('ahit')); po.rot = -30 - Math.max(0, Math.min(80, (700 - p.vy) / 16)); po.yaw = p.sf * 7; loose = 0; break;
      case 'kd': po = P('kd'); loose = 0; break;
      // up off his back with a kip: legs over the head, snap, standing
      case 'getup': { const u = p.sf / SIM.GETUP; po = u < 0.35 ? mix(P('kd'), P('kipA'), sm(u / 0.35)) : u < 0.62 ? mix(P('kipA'), P('kipB'), sm((u - 0.35) / 0.27)) : mix(P('kipB'), P('idle'), sm((u - 0.62) / 0.38)); break; }
      case 'tech': { po = cp(mix(P('jump'), P('idle'), sm((p.sf - 13) / 9))); po.rot = -360 * sm(Math.min(1, p.sf / 16)); echo = 1; break; }
      // he wins sitting down, in mid-air
      case 'win': { const k = sm(p.sf / 26); po = lift(cp(mix(P('idle'), P('win'), k)), k * Math.sin(sFrame * 0.06) * 2.6); loose = 1; break; }
      case 'throwing': {
        if (!p.tk) po = p.sf < 8 ? mix(P('tReach'), P('tHold'), sm(p.sf / 8)) : p.pmT > 0 ? mix(P('tHold'), P('tBite'), Math.sin(p.pmT / 9 * Math.PI)) : P('tHold');
        else { const t = p.tf; po = cp(p.tk === 2 ? kf([[0, 'tHold'], [4, 'bthTuck', 'i'], [10, 'bthToss'], [18, 'kipB'], [30, 'idle']], t) : p.tk === 3 ? kf([[0, 'tHold'], [5, 'cupW', 'o'], [10, 'thUp', 'i'], [18, 'thUp'], [30, 'idle']], t) : p.tk === 4 ? kf([[0, 'tHold'], [5, 'thUp', 'o'], [10, 'thSlam', 'i'], [17, 'thSlam'], [23, 'landP'], [30, 'idle']], t) : kf([[0, 'tHold'], [5, 'idle3', 'o'], [10, 'thKick', 'i'], [17, 'thKick'], [30, 'idle']], t));
          if (p.tk === 1) { po.yaw = 360 * sm((t - 1) / 8); echo = t < 14 ? 1 : 0; }
          if (p.tk === 2) po.rot = -200 * Math.max(0, Math.min(1, (t - 3) / 8)) * (t < 14 ? 1 : Math.max(0, 1 - (t - 14) / 6)); }
        loose = 0; break; }
      case 'thrown': { po = cp(P('thrown')); po.hip[1] += 9 + 2 * Math.sin(sFrame * 0.2); po.fL[1] += 6; po.fR[1] += 8; po.rot = o.tk ? -Math.min(120, (o.tf || 0) * 12) : -8 - (o.pmT > 0 ? 12 * Math.sin(o.pmT / 11 * Math.PI) : 0); loose = 0; break; }
      case 'atk': {
        const m = C.mv[p.mv], an = anOf(p.mv), f = p.mf, base = m.isAir ? 'jump' : m.crouch ? 'crouch' : 'idle'; loose = 0.4;
        if (m.home) {
          // gather, then stretch into a dart that points wherever it is flying; anything but a clean hit and it loses its shape and tumbles
          const h0 = m.home[0], w = P(m.isAir ? 'homeT' : 'homeW');
          if (f < h0) po = mix(P(base), w, sm((f + 1) / (h0 * 0.75)));
          else if ((p.contact && !p.fr) || p.hmE) { po = cp(P('ahit')); po.rot = -60 - 30 * Math.sin(f * 0.5); }
          else { const k = sm((f - h0 + 1) / 3); po = cp(mix(w, P('homeS'), k)); po.rot = (82 - Math.atan2(p.vy - 62, Math.abs(p.vx) + 1) / D) * k; hb = m.hits[0].hb; }
          break;
        }
        if (!an) { po = P(base); break; }
        if (an.f) { const ck = p.ch + ':' + p.mv, keys = fcache[ck] || (fcache[ck] = an.f(m)); po = kf(keys, f); }
        else { const su = Math.max(1, m.su - 1), la = Math.max(su, m.lastAct), T = m.t < 900 ? m.t : la + 24, ph = f <= su ? f / su : f <= la ? 1 + (la > su ? (f - su) / (la - su) : 1) : 2 + Math.min(1, (f - la) / Math.max(1, T - la)); po = kf(an.k, ph);
          if (an.ys) yaw = an.ys[2] * sm((ph - an.ys[0]) / (an.ys[1] - an.ys[0])); }
        if (an.spin) { const a = Math.max(0, m.su - 4), b = m.lastAct + 4, tot = Math.max(1, Math.round((b - a) * 27 / 360)) * 360; yaw = tot * Math.max(0, Math.min(1, (f - a) / (b - a))); }
        if (an.yaw) { const y = an.yaw(m); yaw = y[2] * Math.max(0, Math.min(1, (f - y[0]) / Math.max(1, y[1] - y[0]))); }
        if (an.gh && f <= m.lastAct + 6) echo = 1;
        if (po.aim) { let hh = null; for (const h of m.hits) if (h.hb) { hh = h; if (f <= h.e + 5) break; } if (hh) hb = hh.hb; }
        // an attack thrown out of a somersault finishes the turn instead of snapping level
        if (m.isAir && rg.jrot) { const jr = ((rg.jrot % 360) + 540) % 360 - 180, k = Math.pow(0.7, f + 1); if (Math.abs(jr * k) > 1) { po = cp(po); po.rot += jr * k; } }
        break;
      }
      default: po = P('idle');
    }
    return { po: po, hb: hb, yaw: yaw, echo: echo, air: p.y > 0, loose: loose };
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
    return { G, M, R, T, b, mats, lit, lk, ink: inkN, pm, ch, allM, trails, drops, pts, pp, sparks, seed: ch * 977 + 13, acc: lk.acc, emit: [b.head, b.chest, b.fiL, b.fiR, b.ftL, b.ftR, b.pel] };
  }
  const _tv = new THREE.Vector3(), _tr = new THREE.Vector3();
  if (typeof window !== 'undefined') window.__rigs = () => rigs;
  // ----------------------------------------------------------- the masked one
  // Modelled to the character sheet as lofted forms: every limb and the torso is a stack of cross-sections (width, depth, how far
  // forward it sits), so the silhouette carries real anatomy: chest forward of the waist, a pointed shoulder cap, forearms that
  // taper to a narrow wrist, trousers that swell to the knee and snap in to a cuff, slim boots. Flat two-tone shading on those
  // facets is what keeps it reading as pixel art at game resolution. Cloth is drawn as continuous ribbons with their own outline
  // (wide in the middle, torn to points), each following a curve it is always trying to return to.
  // Units: the figure is about 95 tall, 8 heads. Shoulders 22 across, waist 9.4, arms 30 plus an 8.5 hand, legs 51.
  const LOOKK = { body: 0x2b2a2f, body2: 0x1d1c21, belt: 0x3a2a2c, belt2: 0x533a36, red: 0x8a1f27, red2: 0x61151d, red3: 0xa2352f, mask: 0xd8d0c2, mask2: 0xb4a498, eye: 0x1c0d13, acc: 0xff4a36, acc2: 0xff9a78, ink: 0x2c1119 };
  const LOOKKALT = { red: 0x3466d0, red2: 0x23468f, red3: 0x6c9cf2, acc: 0x5a8cff, acc2: 0x9fc0ff, ink: 0x141c38 };
  function buildRigK(ch, alt) {
    const lk = Object.assign({}, LOOKK, alt ? LOOKKALT : {}), mats = {}, lit = {}, TAU = Math.PI * 2;
    const dim = { TH: 23.5, SHN: 23.5, SP1: 12, SP2: 14.5, UA: 14, FA: 13, SW: 8.3, HW: 3.8 }, TH = dim.TH, SHN = dim.SHN, SP1 = dim.SP1, SP2 = dim.SP2, UA = dim.UA, FA = dim.FA;
    const ramp = new THREE.DataTexture(new Uint8Array([128, 128, 128, 255, 255, 255, 255, 255]), 2, 1, THREE.RGBAFormat); ramp.minFilter = ramp.magFilter = THREE.NearestFilter; ramp.needsUpdate = true;
    // keys starting c_ are cloth: same colours, drawn from both sides
    const mat = k => k === 'eye' ? (lit[k] || (lit[k] = new THREE.MeshBasicMaterial({ color: lk[k], transparent: true, opacity: 1 }))) : mats[k] || (mats[k] = new THREE.MeshToonMaterial({ color: lk[k.replace('c_', '')], gradientMap: ramp, flatShading: true, transparent: true, opacity: 1, side: k[1] === '_' ? THREE.DoubleSide : THREE.FrontSide }));
    const inkN = new THREE.MeshBasicMaterial({ color: lk.ink });
    const G = new THREE.Group(), M = new THREE.Group(), R = new THREE.Group(), T = new THREE.Group(); G.add(M); M.add(R); R.rotation.order = 'ZYX';
    const grp = par => { const g = new THREE.Group(); (par || R).add(g); return g; };
    const put = (g, k, par, x, y, z) => { const m = new THREE.Mesh(g, mat(k)); par.add(m); m.position.set(x || 0, y || 0, z || 0); return m; };
    const box = (k, w, h, d, par, x, y, z) => put(new THREE.BoxGeometry(w, h, d), k, par, x, y, z);
    const ico = (k, r, d, par, x, y, z) => put(new THREE.IcosahedronGeometry(r, d), k, par, x, y, z);
    const plate = (k, pts, th, par) => { const sh = new THREE.Shape(); pts.forEach((q, i) => i ? sh.lineTo(q[0], q[1]) : sh.moveTo(q[0], q[1])); const g = new THREE.ExtrudeGeometry(sh, { depth: th, bevelEnabled: false }); g.rotateY(-Math.PI / 2); return put(g, k, par, 0, 0, 0); };
    const tri = (bins, k, a, b, c) => { (bins[k] || (bins[k] = [])).push(a[0], a[1], a[2], b[0], b[1], b[2], c[0], c[1], c[2]); };
    const bake = (bins, par) => { const g = grp(par); for (const k in bins) { const geo = new THREE.BufferGeometry(); geo.setAttribute('position', new THREE.BufferAttribute(new Float32Array(bins[k]), 3)); geo.computeVertexNormals(); g.add(new THREE.Mesh(geo, mat(k))); } return g; };
    // a cross-section is [height, half-width (side to side), half-depth (front to back), forward offset, colour of the band above it, squareness]
    const ringPt = (s, a) => { const cx = Math.cos(a), cz = Math.sin(a), e = s[5] || 2.3, f = Math.pow(Math.pow(Math.abs(cx), e) + Math.pow(Math.abs(cz), e), -1 / e); return [(s[3] || 0) + cx * f * s[2], s[0], cz * f * s[1]]; };
    const loft = (secs, N, par, key, pick) => { const bins = {}, Rg = secs.map(q => { const r = []; for (let k = 0; k < N; k++) r.push(ringPt(q, (k + 0.5) / N * TAU)); return r; }), L = Rg.length;
      for (let i = 0; i < L - 1; i++) for (let k = 0; k < N; k++) { const k2 = (k + 1) % N, a = Rg[i][k], b2 = Rg[i][k2], c = Rg[i + 1][k2], d = Rg[i + 1][k], kk = (pick && pick(i, (k + 1) / N * TAU)) || secs[i][4] || key; tri(bins, kk, a, d, b2); tri(bins, kk, b2, d, c); }
      const c0 = [secs[0][3] || 0, secs[0][0], 0], c1 = [secs[L - 1][3] || 0, secs[L - 1][0], 0], k0 = secs[0][4] || key, k1 = secs[L - 2][4] || key;
      for (let k = 0; k < N; k++) { const k2 = (k + 1) % N; tri(bins, k0, c0, Rg[0][k], Rg[0][k2]); tri(bins, k1, c1, Rg[L - 1][k2], Rg[L - 1][k]); }
      return bake(bins, par); };
    const secAt = (secs, y) => { let i = 0; while (i < secs.length - 2 && secs[i + 1][0] < y) i++; const a = secs[i], c = secs[i + 1], u = Math.max(0, Math.min(1, (y - a[0]) / ((c[0] - a[0]) || 1))); return [y, a[1] + (c[1] - a[1]) * u, a[2] + (c[2] - a[2]) * u, (a[3] || 0) + ((c[3] || 0) - (a[3] || 0)) * u, 0, a[5] || 2.3]; };
    // a band of cloth lying on the surface of a lofted form, from one [height, side] point to another, on the front (+1) or the back (-1)
    const sash = (secs, p0, p1, hw, sign, key, par, rows) => { const bins = {}, pt = (y, z) => { const q = secAt(secs, y), zz = Math.max(-q[1] * 0.985, Math.min(q[1] * 0.985, z)), e = q[5]; return [q[3] + sign * (q[2] * Math.pow(Math.max(0, 1 - Math.pow(Math.abs(zz / q[1]), e)), 1 / e) + 0.4), y, zz]; }; let pl, pr;
      for (let j = 0; j <= (rows || 9); j++) { const u = j / (rows || 9), y = p0[0] + (p1[0] - p0[0]) * u, z = p0[1] + (p1[1] - p0[1]) * u, l = pt(y, z - hw), r = pt(y, z + hw); if (j) { tri(bins, key, pl, l, pr); tri(bins, key, pr, l, r); } pl = l; pr = r; }
      return bake(bins, par); };
    const b = {}, trails = [];
    // a ribbon: n lengths, each trying to sit at angle a0 + curl*i (0 = straight back, 90 = up, -90 = down). hw(u) gives [edge, edge] half-widths
    // along it. flat: 0 = its face is toward the side camera, 1 = toward front and back. cross adds a second, narrower face the other way
    const strip = (par, off, o) => { const n = o.n + 1, t = { strip: 1, par: par, off: new THREE.Vector3(off[0], off[1], off[2]), pts: [], n: n, len: o.len, a0: o.a0, curl: o.curl || 0, st: o.st || 0.16, grav: o.grav || 0, wav: o.wav == null ? 0.2 : o.wav, ws: o.ws || 0.06, wk: o.wk == null ? 0.5 : o.wk, sz: o.sz || 0, ph: trails.length * 1.7, axis: o.flat ? 1 : 0, init: 0, wL: [], wR: [] };
      for (let i = 0; i < n; i++) { t.pts.push(new THREE.Vector3()); const w = o.hw(i / (n - 1), i); t.wL.push(w[0]); t.wR.push(w[1]); }
      const mk = () => { const geo = new THREE.BufferGeometry(), pos = new Float32Array(n * 6), idx = []; for (let i = 0; i < n - 1; i++) idx.push(i * 2, i * 2 + 1, i * 2 + 2, i * 2 + 1, i * 2 + 3, i * 2 + 2); geo.setAttribute('position', new THREE.BufferAttribute(pos, 3)); const nrm = new Float32Array(n * 6); for (let i = 0; i < n * 2; i++) nrm[i * 3 + 2] = 1; geo.setAttribute('normal', new THREE.BufferAttribute(nrm, 3)); geo.setIndex(idx); const m = new THREE.Mesh(geo, mat('c_' + o.key)); m.frustumCulled = false; m.layers.enable(1); T.add(m); return { geo: geo, pos: pos }; };
      t.a = mk(); if (o.cross) { t.b = mk(); t.w2 = t.wL.map((w, i) => Math.min(o.cross, (w + t.wR[i]) * 0.5)); }
      trails.push(t); return t; };
    const lerpK = (keys, u) => { let i = 0; while (i < keys.length - 2 && keys[i + 1][0] < u) i++; const a = keys[i], c = keys[i + 1], k = Math.max(0, Math.min(1, (u - a[0]) / ((c[0] - a[0]) || 1))); return a[1] + (c[1] - a[1]) * k; };

    // ---- torso (dark), from the hips up. The chest sits forward of the waist and the back is nearly straight
    const PEL = [[-5.2, 7.4, 4.5, -0.2], [-1, 8.6, 5, -0.1], [2.2, 7, 4.4, 0], [4.6, 5.1, 3.5, 0.1]];
    b.pel = grp(); loft(PEL, 10, b.pel, 'body');
    const ABD = [[-0.6, 5.1, 3.5, 0.1], [3, 4.7, 3.15, 0.15], [6.5, 4.75, 3.2, 0.3], [SP1 - 2, 5, 3.35, 0.45], [SP1 - 1.2, 5.05, 3.4, 0.5]];
    b.abd = grp(); loft(ABD, 10, b.abd, 'body');
    const belt = loft([[5.4, 5.25, 3.7, 0.25, 'belt'], [8.6, 5.45, 3.85, 0.4, 'belt'], [8.6, 5.45, 3.85, 0.4]], 10, b.abd, 'belt'); const bs = loft([[6.5, 5.5, 3.95, 0.3, 'belt2'], [7.5, 5.6, 4.0, 0.35]], 10, b.abd, 'belt2'); bs.rotation.x = 0.07; box('belt2', 1.3, 2.7, 2.6, b.abd, 4.2, 7, 1.2); box('belt', 0.9, 4, 1.5, b.abd, 4.3, 4.6, 1.7);
    const CH = [[0, 5.05, 3.4, 0.5], [3, 5.6, 4.0, 1.0], [6.8, 6.5, 4.8, 1.7], [10.2, 7.1, 4.9, 1.6], [12.7, 7.3, 4.2, 0.9], [14.1, 6.0, 3.4, 0.3], [15, 3.6, 2.8, 0.1]];
    b.chest = grp(); loft(CH, 12, b.chest, 'body');
    // the two sashes: over each shoulder, crossing on the chest and again on the back, then running under the belt
    for (const k of [-1, 1]) { sash(CH, [13.9, k * 5.3], [0.2, -k * 2.9], 1.9, 1, 'c_red', b.chest, 10); sash(CH, [13.9, k * 5.3], [0.2, -k * 2.9], 2, -1, 'c_red2', b.chest, 10); sash(ABD, [SP1 - 1.2, -k * 2.9], [8.5, -k * 3.3], 1.9, 1, 'c_red', b.abd, 3); sash(ABD, [SP1 - 1.2, -k * 2.9], [8.5, -k * 3.3], 2, -1, 'c_red2', b.abd, 3);
      const cp = box('red', 4.6, 1.1, 3.6, b.chest, 0.4, 13.9, k * 5.3); cp.rotation.x = -k * 0.5; }
    // collar: seated wide on the shoulders, drawn in, then standing open round the neck
    const COL = [[SP2 - 2.4, 6.9, 4.5, 0.2], [SP2 - 0.6, 5.5, 4.2, 0.2], [SP2 + 1.4, 3.9, 3.5, 0.3], [SP2 + 4.2, 4.0, 3.8, 0.55], [SP2 + 6.0, 4.5, 4.3, 0.8], [SP2 + 6.0, 3.7, 3.5, 0.8, 'body2'], [SP2 + 4.6, 2.6, 2.6, 0.6, 'body2']];
    const col = loft(COL, 10, b.chest, 'body'); col.rotation.z = -0.07;
    // ---- head
    const hd = b.head = grp(); loft([[-4, 2.1, 2.2, 0], [1, 1.8, 1.9, 0.3], [6.5, 1.85, 2.0, 1.0]], 8, hd, 'body');
    const sk = grp(hd); sk.position.set(1.5, 11.6, 0); sk.rotation.z = -0.08;
    loft([[-6.3, 1.9, 2.0, 0.2], [-4.2, 2.9, 3.1, 0.6], [-1.6, 3.5, 4.0, 0.2], [0.6, 3.7, 4.3, -0.1]], 10, sk, 'body');   // jaw and the back of the neck
    const cap = ico('red', 4.5, 2, sk, -0.2, 1.3, 0); cap.scale.set(1.03, 0.98, 0.86); const band = loft([[-1.5, 3.85, 4.5, -0.2, 'red2'], [-0.5, 3.95, 4.6, -0.2]], 12, sk, 'red2');
    const bun = ico('red2', 1.75, 1, sk, -4.3, 3.2, 0); bun.scale.set(1, 1, 1.05);
    for (const z of [-1, 1]) { const e = put(new THREE.CylinderGeometry(1.55, 1.55, 0.9, 10), 'body2', sk, -0.6, -0.9, z * 3.7); e.rotation.x = Math.PI / 2; const d = put(new THREE.CylinderGeometry(0.6, 0.6, 0.5, 6), 'body', sk, -0.6, -0.9, z * 4.25); d.rotation.x = Math.PI / 2; }
    // mask: two halves meeting on a ridge, square across the brow, straight to the cheek, then a long point below the chin
    for (const z of [-1, 1]) { const h = plate(z > 0 ? 'mask' : 'mask2', z > 0 ? [[0, 4.7], [3.3, 4.5], [3.45, -0.7], [0, -6.9]] : [[0, 4.7], [0, -6.9], [-3.45, -0.7], [-3.3, 4.5]], 1.1, sk); h.position.set(5.0, 0.2, 0); h.rotation.y = -z * 0.27; h.rotation.z = 0.05;
      const e = box('eye', 0.5, 1.15, 2.1, h, 0.12, 0.6, z * 1.75); e.rotation.x = z * 0.45; }
    // ribbons from the knot: rise back off the head, then arc over and fall; thin at the root, a little wider through the middle, pointed
    const rib = (u) => { const w = 0.42 + 0.62 * Math.sin(Math.PI * Math.min(1, u * 1.25)) * (1 - u * 0.5); return [u > 0.97 ? 0.05 : w, u > 0.97 ? 0.05 : w]; };
    strip(sk, [-5.4, 3.5, 0.6], { n: 16, len: 2.9, a0: 34, curl: -5.4, st: 0.2, wav: 0.42, ws: 0.07, wk: 0.35, hw: rib, key: 'red' });
    strip(sk, [-5.4, 3.0, -0.6], { n: 14, len: 2.9, a0: 8, curl: -5.6, st: 0.2, wav: 0.46, ws: 0.06, wk: 0.35, hw: rib, key: 'red2' });
    strip(sk, [-5.2, 3.9, 0], { n: 13, len: 2.8, a0: 58, curl: -8.5, st: 0.22, wav: 0.38, ws: 0.08, wk: 0.3, hw: rib, key: 'red3' });
    // ---- the coat below the belt. In front, a panel over each thigh to the knee, straight inner edge, torn hem.
    const hang = (W) => (u, i) => { const w = W * lerpK([[0, 0.8], [0.3, 1], [0.72, 0.86], [1, 0.02]], u), bite = i % 3 === 2 && u > 0.5 && u < 0.97 ? 0.55 : 1; return [w * bite, w]; };
    for (const z of [-1, 1]) strip(b.abd, [4.0, 5.6, z * 3.2], { n: 9, len: 3.3, a0: -101, curl: 0.2, st: 0.3, grav: 0.02, wav: 0.07, wk: 0.25, hw: hang(3.0), key: 'red', flat: 1, cross: 1.4, sz: z * 0.42 });
    // behind, the long tails: broad through the middle, swept back and down to torn points, the nearer one longest
    const tail = (W, pk) => (u, i) => { const w = W * lerpK([[0, 0.42], [0.22, 0.9], [pk, 1], [0.78, 0.62], [1, 0.02]], u), bite = (i % 3 === 1 && u > 0.38 && u < 0.96) ? 0.5 : 1; return [w * 0.85, w * bite]; };
    strip(b.abd, [-3.3, 5.9, 2.2], { n: 14, len: 3.5, a0: -34, curl: -1.7, st: 0.17, grav: 0.01, wav: 0.14, ws: 0.05, wk: 0.5, hw: tail(4.3, 0.45), key: 'red', cross: 1.6, sz: 0.3 });
    strip(b.abd, [-3.2, 5.7, -2.2], { n: 12, len: 3.4, a0: -50, curl: -1.6, st: 0.18, grav: 0.012, wav: 0.13, ws: 0.055, wk: 0.45, hw: tail(3.7, 0.4), key: 'red2', cross: 1.5, sz: -0.3 });
    strip(b.abd, [-3.4, 6.2, 0], { n: 11, len: 3.3, a0: -22, curl: -1.4, st: 0.16, grav: 0.008, wav: 0.18, ws: 0.06, wk: 0.55, hw: tail(2.5, 0.4), key: 'red3', sz: 0 });
    // the loose ends of the sashes, hanging from behind each shoulder
    for (const z of [-1, 1]) strip(b.chest, [-3.9, 10.5, z * 3.6], { n: 12, len: 3.3, a0: -78, curl: 1.1, st: 0.2, grav: 0.02, wav: 0.15, wk: 0.5, hw: u => { const w = 1.25 * lerpK([[0, 0.9], [0.6, 1], [1, 0.03]], u); return [w, w]; }, key: z > 0 ? 'red' : 'red2', cross: 0.8, sz: z * 0.12 });
    for (const n of ['L', 'R']) { const sd = n === 'L' ? -1 : 1;
      // ---- arm: a pointed shoulder cap, red to the elbow, then a dark bracer with a flared top tapering to a narrow wrist with a red band
      b['sh' + n] = ico('red', 2.6, 1, R, 0, 0, 0);
      const dk = (i, a) => null;
      b['ua' + n] = loft([[-0.8, 2.3, 2.4, 0, 'red'], [1.4, 2.95, 2.95, 0, 'red'], [4.2, 2.75, 2.7, 0.1, 'red'], [5, 2.45, 2.45, 0.1, 'red2'], [UA / 2, 2.35, 2.35, 0.1]], 10, R, 'red');
      b['ua' + n + 'b'] = loft([[0, 2.35, 2.35, 0.1, 'red2'], [2.6, 2.3, 2.35, 0.15, 'red2'], [5.4, 2.0, 2.05, 0.05, 'red2'], [UA / 2, 1.85, 1.9, 0]], 10, R, 'red2'); ico('red2', 1.9, 1, b['ua' + n + 'b'], 0, UA / 2, 0);
      b['fa' + n] = loft([[0, 1.85, 1.9, 0, 'red2'], [1.1, 1.9, 1.95, 0, 'body2'], [1.3, 2.3, 2.4, 0, 'body2'], [4, 2.2, 2.3, 0, 'body2'], [FA / 2, 1.95, 2.05, 0]], 10, R, 'body2');
      b['fa' + n + 'b'] = loft([[0, 1.95, 2.05, 0, 'body2'], [3.2, 1.6, 1.75, 0, 'body2'], [4.2, 1.4, 1.55, 0, 'red'], [4.2, 1.7, 1.85, 0, 'red'], [5.5, 1.65, 1.8, 0, 'red'], [5.5, 1.35, 1.45, 0, 'body2'], [FA / 2, 1.3, 1.4, 0]], 10, R, 'body2');
      // hand: a narrow palm seen edge-on from the front, fingers together in two long wedges, the thumb lying forward
      const hnd = b['fi' + n] = grp(); loft([[-0.3, 0.9, 1.4, 0], [1.6, 0.95, 1.95, 0.1], [3.9, 0.85, 2.0, 0.15], [4.3, 0.7, 1.8, 0.15]], 8, hnd, 'body2');
      for (const f of [-1, 1]) { const fg = loft([[0, 0.6, 0.85, 0], [2.2, 0.55, 0.75, 0], [4.3 - (f > 0 ? 0 : 0.5), 0.3, 0.3, 0]], 6, hnd, 'body2'); fg.position.set(0.15 + f * 0.95, 4.1, 0); fg.rotation.z = -f * 0.06 - 0.04; fg.rotation.x = sd * 0.06; }
      const tb = loft([[0, 0.5, 0.6, 0], [1.6, 0.5, 0.55, 0], [3, 0.25, 0.3, 0]], 6, hnd, 'body2'); tb.position.set(-1.5, 1.3, sd * 0.3); tb.rotation.z = 0.55;
      // ---- leg: trousers swelling from the hip to their fullest just below the knee, drawn in hard to a cuff, slim boot under it
      b['th' + n] = loft([[-0.8, 4.6, 4.7, 0], [1.5, 4.9, 5.0, 0], [6, 5.6, 5.5, 0.1], [TH / 2, 6.3, 6.0, 0.2]], 10, R, 'body');
      b['th' + n + 'b'] = loft([[0, 6.3, 6.0, 0.2], [6, 6.95, 6.5, 0.3], [TH / 2, 7.3, 6.8, 0.35]], 10, R, 'body'); ico('body', 6.7, 1, b['th' + n + 'b'], 0.3, TH / 2, 0);
      b['sn' + n] = loft([[0, 7.3, 6.8, 0.35, 'body'], [2.4, 7.4, 7.0, 0.3, 'body'], [4.6, 6.7, 6.3, 0.2, 'body'], [6.3, 4.6, 4.4, 0.1, 'body'], [7.1, 3.2, 3.15, 0, 'belt'], [8.2, 3.2, 3.15, 0, 'belt'], [8.2, 2.7, 2.75, 0, 'body2'], [SHN / 2, 2.55, 2.65, 0]], 10, R, 'body');
      b['sn' + n + 'b'] = loft([[0, 2.55, 2.65, 0, 'body2'], [5, 2.25, 2.45, 0, 'body2'], [8.2, 2.05, 2.3, 0, 'red'], [8.2, 2.35, 2.6, 0, 'red'], [9.9, 2.3, 2.6, 0, 'red'], [9.9, 2.0, 2.35, 0, 'body2'], [SHN / 2, 2.0, 2.4, 0]], 10, R, 'body2');
      // shoe: long and low, heel back of the ankle, a red strap over the instep (local y runs toe-ward, x is down)
      const ft = b['ft' + n] = grp(); const shoe = loft([[-2.6, 1.6, 1.5, 0.75, 'body2'], [-0.6, 2.0, 2.2, 0.05, 'body2'], [2.4, 2.1, 2.1, 0.15, 'red'], [2.4, 2.3, 2.3, 0.15, 'red'], [4.2, 2.3, 2.0, 0.45, 'red'], [4.2, 2.1, 1.75, 0.5, 'body2'], [7.4, 1.9, 1.25, 1.0, 'body2'], [9.8, 1.1, 0.6, 1.65]], 8, ft, 'body2'); shoe.position.set(1.2, 3, 0);
    }
    // a few embers
    const NP = 12, pg = new THREE.BufferGeometry(), pp = new Float32Array(NP * 3); pg.setAttribute('position', new THREE.BufferAttribute(pp, 3));
    const pts = new THREE.Points(pg, new THREE.PointsMaterial({ color: lk.acc, size: 2, sizeAttenuation: false, transparent: true, opacity: 0.9, depthWrite: false })); pts.frustumCulled = false; T.add(pts);
    const sparks = []; for (let i = 0; i < NP; i++) { sparks.push({ life: 0, vx: 0, vy: 0 }); pp[i * 3 + 1] = -99; }
    const allM = Object.values(mats).concat(Object.values(lit), [inkN]); for (const m of allM) { m.userData.o = m.opacity; m.userData.c = m.color.getHex(); }
    G.traverse(o => o.layers.enable(1));
    return { G, M, R, T, b, mats, lit, lk, ink: inkN, ch, dim, allM, trails, pts, pp, sparks, solid: 1, seed: ch * 977 + 13, acc: lk.acc, emit: [b.chest, b.pel] };
  }
  // a ribbon made as one continuous strip: every point leans toward its own resting angle off the one before it, ripples, and goes with the wind
  function stripStep(rig, t, a, face, gr, ti) {
    const P = t.pts, n = t.n;
    if (!t.init || P[0].distanceToSquared(a) > 4900) { for (let i = 0; i < n; i++) { const ang = (t.a0 + t.curl * i) * D; P[i].set(i ? P[i - 1].x - face * Math.cos(ang) * t.len : a.x, Math.max(1.2, i ? P[i - 1].y + Math.sin(ang) * t.len : a.y), a.z + t.sz * i); } t.init = 1; }
    P[0].copy(a);
    if (gr) { const sgn = ti === 1 ? -1 : 1, rad = (gr.wrap ? 9 - gr.sq * 4 : 0);
      for (let i = 1; i < n; i++) { const u = i / (n - 1), k = Math.min(1, u * 1.35), p = P[i]; let x = a.x + (gr.x - a.x) * k, y = a.y + (gr.y - a.y) * k + Math.sin(Math.min(1, k) * Math.PI) * 13 * sgn * (ti === 2 ? 0.4 : 1), z = a.z;
        if (gr.wrap && u > 0.6) { const an = (u - 0.6) * 15 * sgn + tick * 0.22 * sgn + ti * 2; x = gr.x + Math.cos(an) * rad; y = gr.y - 6 + (u - 0.6) * 44 - ti * 5 + Math.sin(an) * 3; z = a.z + Math.sin(an) * rad; }
        _tr.set(x, y, z); p.lerp(_tr, 0.5); }
    } else for (let i = 1; i < n; i++) { const p = P[i], q = P[i - 1], u = i / (n - 1), ang = (t.a0 + t.curl * i) * D + t.wav * Math.sin(tick * t.ws - i * 0.55 + t.ph) * (0.3 + 0.7 * u);
      _tr.set(q.x - face * Math.cos(ang) * t.len, q.y + Math.sin(ang) * t.len, a.z + t.sz * i); p.lerp(_tr, t.st); p.x += WIND * t.wk * u; p.y -= t.grav;
      _tr.subVectors(p, q); const L = _tr.length() || 1; p.copy(q).addScaledVector(_tr, t.len / L); if (p.y < 1.2) p.y = 1.2; }
    for (let pass = 0; pass < (t.b ? 2 : 1); pass++) { const pos = pass ? t.b.pos : t.a.pos, ax = pass ? 1 - t.axis : t.axis;
      for (let i = 0; i < n; i++) { const p = P[i], p0 = P[i ? i - 1 : 0], p1 = P[i < n - 1 ? i + 1 : i], tx = p1.x - p0.x, ty = p1.y - p0.y, tz = p1.z - p0.z; let nx, ny, nz;
        if (ax) { nx = 0; ny = tz; nz = -ty; if (Math.abs(ty) < 0.3 * Math.abs(tx)) { ny = 0; nz = 1; } } else { nx = -ty * face; ny = tx * face; nz = 0; }
        const l = Math.hypot(nx, ny, nz) || 1, wl = pass ? t.w2[i] : t.wL[i], wr = pass ? t.w2[i] : t.wR[i], o = i * 6; nx /= l; ny /= l; nz /= l;
        pos[o] = p.x + nx * wl; pos[o + 1] = p.y + ny * wl; pos[o + 2] = p.z + nz * wl; pos[o + 3] = p.x - nx * wr; pos[o + 4] = p.y - ny * wr; pos[o + 5] = p.z - nz * wr; }
      (pass ? t.b : t.a).geo.attributes.position.needsUpdate = true; }
  }
  // ----------------------------------------------------------- the masked one: the sculpted model on its canon rig
  // The mesh and its 24-joint skeleton come in as authored. Every joint is driven directly: the pose gives a position for it and a
  // direction for its bone, and the bone is turned from where it pointed in the bind pose to where it points now. The coat, the head
  // ribbons and the loose strands are not on that skeleton at all: each has its own chain of joints, cut from the mesh and re-weighted,
  // hung from the body and simulated (see clothStep).
  const MSK = 63;   // world units per metre of the model
  const KM = (function () {
    if (typeof MDK === 'undefined' || typeof atob === 'undefined' || typeof THREE === 'undefined') return null;
    const dec = (b, T) => { const bin = atob(b), u8 = new Uint8Array(bin.length); for (let i = 0; i < bin.length; i++) u8[i] = bin.charCodeAt(i); return new T(u8.buffer); };
    const M = MDK, nv = M.nv, q = dec(M.pos, Uint16Array), pos = new Float32Array(nv * 3); for (let i = 0; i < nv * 3; i++) { const a = i % 3; pos[i] = M.mn[a] + q[i] / 65535 * (M.mx[a] - M.mn[a]); }
    const n8 = dec(M.nor, Int8Array), nor = new Float32Array(nv * 3); for (let i = 0; i < nv * 3; i++) nor[i] = n8[i] / 127;
    const u16 = dec(M.uv, Uint16Array), uv = new Float32Array(nv * 2); for (let i = 0; i < nv * 2; i++) uv[i] = u16[i] / 65535;
    const s8 = dec(M.si, Uint8Array), si = new Uint16Array(nv * 4); for (let i = 0; i < nv * 4; i++) si[i] = s8[i];
    const w8 = dec(M.sw, Uint8Array), sw = new Float32Array(nv * 4); for (let i = 0; i < nv * 4; i++) sw[i] = w8[i] / 255;
    const geo = new THREE.BufferGeometry(); geo.setAttribute('position', new THREE.BufferAttribute(pos, 3)); geo.setAttribute('normal', new THREE.BufferAttribute(nor, 3)); geo.setAttribute('uv', new THREE.BufferAttribute(uv, 2));
    geo.setAttribute('skinIndex', new THREE.BufferAttribute(si, 4)); geo.setAttribute('skinWeight', new THREE.BufferAttribute(sw, 4)); geo.setIndex(new THREE.BufferAttribute(dec(M.idx, Uint16Array), 1));
    const id = {}, bp = M.bones.map((b, i) => { id[b[0]] = i; return [b[1], b[2], b[3]]; }), B = n => bp[id[n]], sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]], cr = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]], len = v => Math.hypot(v[0], v[1], v[2]);
    const RT = [-1, 0, 0], F = {}, fr = (n, d, h) => { F[n] = { i: id[n], F: frame3(d, h, []), d: [d[0] / len(d), d[1] / len(d), d[2] / len(d)] }; };
    fr('Hips', sub(B('Spine02'), B('Hips')), RT); fr('Spine02', sub(B('Spine01'), B('Spine02')), RT); fr('Spine01', sub(B('Spine'), B('Spine01')), RT); fr('Spine', sub(B('neck'), B('Spine')), RT); fr('neck', sub(B('Head'), B('neck')), RT); fr('Head', [0, 1, 0], RT);
    for (const S of ['Left', 'Right']) { const u = sub(B(S + 'ForeArm'), B(S + 'Arm')), f = sub(B(S + 'Hand'), B(S + 'ForeArm')), ax = cr(u, f); fr(S + 'Arm', u, ax); fr(S + 'ForeArm', f, ax);
      const t = sub(B(S + 'Leg'), B(S + 'UpLeg')), sn = sub(B(S + 'Foot'), B(S + 'Leg')), kx = cr(t, sn); fr(S + 'UpLeg', t, kx); fr(S + 'Leg', sn, kx); const fd = sub(B(S + 'ToeBase'), B(S + 'Foot')); fr(S + 'Foot', [fd[0], 0, fd[2]], RT); }
    const L = (a, b) => len(sub(B(a), B(b))) * MSK;
    return { geo, id, bp, B, F, chains: M.chains, nb: M.bones.length, len: { s: [L('Spine02', 'Hips'), L('Spine01', 'Spine02'), L('Spine', 'Spine01'), L('neck', 'Spine'), L('Head', 'neck')], UA: L('LeftForeArm', 'LeftArm'), FA: L('LeftHand', 'LeftForeArm'), TH: L('LeftLeg', 'LeftUpLeg'), SH: L('LeftFoot', 'LeftLeg'), AH: (MDK.hipY + B('LeftFoot')[1]) * MSK }, tex: null };
  })();
  // an orthonormal frame whose first axis is d and whose third is as close to h as it can be; nine numbers, one axis after another
  function frame3(d, h, out) {
    let x0 = d[0], x1 = d[1], x2 = d[2], l = Math.hypot(x0, x1, x2) || 1; x0 /= l; x1 /= l; x2 /= l;
    let k = h[0] * x0 + h[1] * x1 + h[2] * x2, z0 = h[0] - k * x0, z1 = h[1] - k * x1, z2 = h[2] - k * x2; l = Math.hypot(z0, z1, z2);
    if (l < 1e-5) { z0 = -x1; z1 = x0; z2 = 0; if (Math.hypot(z0, z1) < 1e-4) { z0 = 0; z1 = -x2; z2 = x1; } l = Math.hypot(z0, z1, z2); }
    z0 /= l; z1 /= l; z2 /= l; out[0] = x0; out[1] = x1; out[2] = x2; out[3] = z1 * x2 - z2 * x1; out[4] = z2 * x0 - z0 * x2; out[5] = z0 * x1 - z1 * x0; out[6] = z0; out[7] = z1; out[8] = z2; return out;
  }
  const DITH = 'if (uDith > 0.0) { vec2 dp = mod(floor(gl_FragCoord.xy), 2.0); float dt = (dp.x + dp.y * 2.0 == 0.0) ? 0.2 : (dp.x + dp.y * 2.0 == 3.0) ? 0.4 : (dp.x + dp.y * 2.0 == 1.0) ? 0.6 : 0.8; if (uDith > dt) discard; }\n';
  // Colour themes for the shared body. 0 is the model as painted. The others re-colour it in the shader: the red cloth becomes a ramp
  // from shade to light, the dark cloth takes a tint, and the pale mask is left alone.
  const THEMES = [
    { ghost: 0x9c1420, pts: 0xff4a36, parry: [1, 0.18, 0.59], home: [1, 0.47, 0.74] },
    { swap: 1, ghost: 0x2a4a9c, pts: 0x5a8cff, parry: [1, 0.18, 0.59], home: [1, 0.47, 0.74] },
    { a: [0.74, 0.93, 1.0], b: [0.2, 0.45, 0.95], d: [0.4, 1.3, 4.6], ghost: 0x6fa8ff, pts: 0x9fd0ff, parry: [0.12, 0.49, 1], home: [0.35, 0.68, 1] },
    { a: [0.78, 1.0, 0.9], b: [0.12, 0.6, 0.56], d: [0.5, 1.7, 1.5], ghost: 0x3fb9a6, pts: 0x9fffe0, parry: [0.12, 0.49, 1], home: [0.35, 0.68, 1] }];
  function buildRigS(ch, alt) { const TH = THEMES[(ch === 0 ? 2 : 0) + (alt ? 1 : 0)];
    const G = new THREE.Group(), M = new THREE.Group(), R = new THREE.Group(), W = new THREE.Group(), T = new THREE.Group(); G.add(M); M.add(R); R.add(W); R.rotation.order = 'ZYX'; W.rotation.y = Math.PI / 2; W.scale.setScalar(MSK);
    if (!KM.tex) { const img = new Image(), tx = new THREE.Texture(img); tx.flipY = false; tx.magFilter = THREE.NearestFilter; img.onload = () => { tx.needsUpdate = true; }; img.src = MDK.tex; KM.tex = tx; }
    const ramp = new THREE.DataTexture(new Uint8Array([96, 96, 96, 255, 176, 176, 176, 255, 255, 255, 255, 255]), 3, 1, THREE.RGBAFormat); ramp.minFilter = ramp.magFilter = THREE.NearestFilter; ramp.needsUpdate = true;
    const U = { rim: { value: new THREE.Color(0x000000) }, flat: { value: new THREE.Vector4(1, 1, 1, 0) }, grey: { value: 0 }, swap: { value: TH.swap ? 1 : 0 }, dith: { value: 0 }, th: { value: TH.a ? 1 : 0 }, thA: { value: new THREE.Vector3().fromArray(TH.a || [1, 1, 1]) }, thB: { value: new THREE.Vector3().fromArray(TH.b || [1, 1, 1]) }, thD: { value: new THREE.Vector3().fromArray(TH.d || [1, 1, 1]) } };
    const mat = new THREE.MeshToonMaterial({ map: KM.tex, gradientMap: ramp, skinning: true, transparent: true, opacity: 1, side: THREE.DoubleSide }); if (TH.a) mat.color.setRGB(0.9, 0.93, 0.95); else mat.color.setRGB(0.78, 0.84, 0.86);
    // an edge light off the stage's backlight, a flat state colour (parry, Slipstream), burnout grey, and a colour swap for the mirror match
    mat.onBeforeCompile = sh => { sh.uniforms.uRim = U.rim; sh.uniforms.uFlat = U.flat; sh.uniforms.uGrey = U.grey; sh.uniforms.uSwap = U.swap; sh.uniforms.uDith = U.dith; sh.uniforms.uTh = U.th; sh.uniforms.uThA = U.thA; sh.uniforms.uThB = U.thB; sh.uniforms.uThD = U.thD;
      sh.fragmentShader = 'uniform vec3 uRim; uniform vec4 uFlat; uniform float uGrey; uniform float uSwap; uniform float uDith; uniform float uTh; uniform vec3 uThA; uniform vec3 uThB; uniform vec3 uThD;\n' + sh.fragmentShader.replace('#include <map_fragment>',
        '#include <map_fragment>\n if (uTh > 0.5) { vec3 tc = texture2D(map, vUv).rgb; float rd = clamp((tc.r - max(tc.g, tc.b)) / max(tc.r, 0.05) * 1.7, 0.0, 1.0); float tl = dot(tc, vec3(0.3, 0.5, 0.2)); vec3 dk = vec3(tl) * mix(uThD, vec3(1.0), smoothstep(0.3, 0.62, tl)); diffuseColor.rgb = diffuse * mix(dk, mix(uThB, uThA, smoothstep(0.12, 0.7, tc.r)), rd); }\n').replace('#include <tonemapping_fragment>',
        DITH + 'float rimK = pow(1.0 - clamp(dot(normalize(vViewPosition), normal), 0.0, 1.0), 2.4);\n gl_FragColor.rgb = mix(gl_FragColor.rgb, gl_FragColor.bgr, uSwap);\n gl_FragColor.rgb += uRim * smoothstep(0.42, 0.5, rimK) * min(1.0, 0.28 + 1.5 * max(gl_FragColor.r, gl_FragColor.b));\n gl_FragColor.rgb = mix(gl_FragColor.rgb, vec3(dot(gl_FragColor.rgb, vec3(0.3, 0.5, 0.2))) * vec3(0.8, 0.86, 0.96) + 0.04, uGrey);\n gl_FragColor.rgb = mix(gl_FragColor.rgb, uFlat.rgb * (0.62 + 0.38 * smoothstep(0.2, 0.3, rimK) + 0.3 * step(0.2, dot(gl_FragColor.rgb, vec3(0.34)))), uFlat.a);\n#include <tonemapping_fragment>'); };
    const bones = [], inv = []; for (let i = 0; i < KM.nb; i++) { const o = new THREE.Bone(), b = KM.bp[i]; o.matrixAutoUpdate = false; o.matrix.makeTranslation(b[0], b[1], b[2]); bones.push(o); inv.push(new THREE.Matrix4().makeTranslation(-b[0], -b[1], -b[2])); (i < 24 ? W : T).add(o); }
    const mesh = new THREE.SkinnedMesh(KM.geo, mat); mesh.frustumCulled = false; W.add(mesh); mesh.bind(new THREE.Skeleton(bones, inv), new THREE.Matrix4());
    const chains = KM.chains.map((c, k) => { const n = c.j.length, P = [], Q = [], Tg = [], len = [0], dir = []; for (let i = 0; i < n; i++) { P.push(new THREE.Vector3()); Q.push(new THREE.Vector3()); Tg.push(new THREE.Vector3()); const a = KM.bp[c.j[i]];
        if (i) { const b = KM.bp[c.j[i - 1]]; len.push(Math.hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]) * MSK); dir.push(new THREE.Vector3(a[0] - b[0], a[1] - b[1], a[2] - b[2]).normalize()); } }
      return { a: c.a, j: c.j, kind: c.kind, p: P, q: Q, t: Tg, len: len, dir: dir, ph: k * 1.37, init: 0 }; });
    const NP = 12, pg = new THREE.BufferGeometry(), pp = new Float32Array(NP * 3); pg.setAttribute('position', new THREE.BufferAttribute(pp, 3));
    const pts = new THREE.Points(pg, new THREE.PointsMaterial({ color: TH.pts, size: 2, sizeAttenuation: false, transparent: true, opacity: 0.9, depthWrite: false })); pts.frustumCulled = false; T.add(pts);
    const sparks = []; for (let i = 0; i < NP; i++) { sparks.push({ life: 0, vx: 0, vy: 0 }); pp[i * 3 + 1] = -99; }
    const ghosts = []; for (let g = 0; g < 4; g++) { const gu = { dith: { value: 1 } }, gm = new THREE.MeshBasicMaterial({ color: TH.ghost, skinning: true, side: THREE.DoubleSide });
      gm.onBeforeCompile = sh => { sh.uniforms.uDith = gu.dith; sh.fragmentShader = 'uniform float uDith;\n' + sh.fragmentShader.replace('#include <tonemapping_fragment>', DITH + '#include <tonemapping_fragment>'); };
      const gb = [], gi = []; for (let i = 0; i < KM.nb; i++) { const o = new THREE.Bone(), b = KM.bp[i]; o.matrixAutoUpdate = false; gb.push(o); gi.push(inv[i]); T.add(o); }
      const ms = new THREE.SkinnedMesh(KM.geo, gm); ms.frustumCulled = false; ms.visible = false; T.add(ms); ms.bind(new THREE.Skeleton(gb, gi), new THREE.Matrix4()); ghosts.push({ ms: ms, gb: gb, u: gu, life: 0, max: 1 }); }
    mat.userData.o = 1; G.traverse(o => o.layers.enable(1));
    return { G, M, R, T, W, mesh, bones, ghosts, gn: 0, skin: 1, ch, th: TH, U, chains, trails: [], mats: { body: mat }, lit: {}, allM: [mat], pts, pp, sparks, seed: ch * 977 + 13, acc: TH.pts, emit: [bones[KM.id.Spine01], bones[KM.id.Hips]], sc: { R: [], F: [], p: {}, r: {} } };
  }
  const _kF = [], _white = new THREE.Color(0xffffff);
  function applyPoseS(rig, fp, face, flashK, tint) {
    const K = KM, Ln = K.len, bones = rig.bones, lo = (fp.loose == null ? 1 : fp.loose) * 0.45, s1 = Math.sin(tick * 0.052 + rig.seed), s2 = Math.sin(tick * 0.037 + rig.seed * 1.7), s3 = Math.sin(tick * 0.083 + rig.seed * 0.6), bk = rig.boK = (rig.boK || 0) + ((rig.boT || 0) - (rig.boK || 0)) * 0.14, vl = Math.max(-6, Math.min(6, (rig.vel || 0) * face)), po0 = fp.po;
    const po = Object.assign({}, po0, { hip: [po0.hip[0] + s1 * 2.2 * lo, po0.hip[1] + s3 * 1.1 * lo - bk * 3, 0], ln: po0.ln + s2 * 4 * lo + bk * 9, ln2: po0.ln2 - s2 * 5 * lo + s3 * 2 + bk * 12, hd: po0.hd + s1 * 6 * lo - bk * 12, rot: po0.rot + s2 * 1.4 * lo + vl * 1.2, hL: [po0.hL[0] + s2 * 2.5 * lo, po0.hL[1] + s3 * 2 * lo - bk * 5, po0.hL[2]], hR: [po0.hR[0] - s1 * 2.5 * lo, po0.hR[1] + s2 * 2 * lo - bk * 5, po0.hR[2]] });
    const hip = [po.hip[0], po.hip[1], 0], a1 = po.ln * D, a2 = po.ln2 * D, tw = po.tw * D, pt = po.pt * D, cax = [Math.sin(tw), 0, Math.cos(tw)], pax = [Math.sin(pt), 0, Math.cos(pt)], mid = [(cax[0] + pax[0]) / 2, 0, (cax[2] + pax[2]) / 2];
    const ha = a2 * 0.45 + po.hd * D, back = Math.max(0, Math.min(1, (Math.abs(po.tw) - 100) / 50)), hz = [fp.straight ? 0 : -0.5 + back * 0.9 + cax[0] * 0.12, 0, 0.82];
    // model space, relative to the hips: x = the figure's left, y = up, z = the way it faces
    const mv = v => [-v[2], v[1], v[0]], mp = g => [-g[2] / MSK, (g[1] - hip[1]) / MSK, (g[0] - hip[0]) / MSK], gp = m => [hip[0] + m[2] * MSK, hip[1] + m[1] * MSK, -m[0] * MSK];
    const P = rig.sc.p, Rm = rig.sc.r;
    // turn bone n from its bind frame to (dir, hint), put its joint at p, optionally stretched along its length
    const drive = (n, p, dir, hint, st) => { const f = K.F[n], Ft = frame3(dir, hint, _kF), Fb = f.F, r = Rm[n] || (Rm[n] = []);
      for (let a = 0; a < 3; a++) for (let c = 0; c < 3; c++) r[a * 3 + c] = Ft[a] * Fb[c] + Ft[3 + a] * Fb[3 + c] + Ft[6 + a] * Fb[6 + c];   // r[row*3+col]
      P[n] = p; const e = bones[f.i].matrix.elements, k = (st || 1) - 1, db = f.d;
      for (let c = 0; c < 3; c++) { e[c * 4] = r[c] + k * Ft[0] * db[c]; e[c * 4 + 1] = r[3 + c] + k * Ft[1] * db[c]; e[c * 4 + 2] = r[6 + c] + k * Ft[2] * db[c]; e[c * 4 + 3] = 0; }
      e[12] = p[0]; e[13] = p[1]; e[14] = p[2]; e[15] = 1; bones[f.i].matrixWorldNeedsUpdate = true; };
    // a joint that simply rides on bone n
    const ride = (child, n) => { const r = Rm[n], p = P[n], a = K.B(child), b = K.B(n), d0 = a[0] - b[0], d1 = a[1] - b[1], d2 = a[2] - b[2], q = [p[0] + r[0] * d0 + r[1] * d1 + r[2] * d2, p[1] + r[3] * d0 + r[4] * d1 + r[5] * d2, p[2] + r[6] * d0 + r[7] * d1 + r[8] * d2];
      const e = bones[K.id[child]].matrix.elements; for (let c = 0; c < 3; c++) { e[c * 4] = r[c]; e[c * 4 + 1] = r[3 + c]; e[c * 4 + 2] = r[6 + c]; e[c * 4 + 3] = 0; } e[12] = q[0]; e[13] = q[1]; e[14] = q[2]; e[15] = 1; bones[K.id[child]].matrixWorldNeedsUpdate = true; P[child] = q; Rm[child] = r; return q; };
    if (fp.bindPose) { for (let i = 0; i < 24; i++) { const b = K.bp[i]; bones[i].matrix.makeTranslation(b[0], b[1], b[2]); bones[i].matrixWorldNeedsUpdate = true; } hip[1] = MDK.hipY * MSK; }
    else {
      const d0 = [Math.sin(a1 * 0.5), Math.cos(a1 * 0.5), 0], d1 = [Math.sin(a1), Math.cos(a1), 0], d2 = [Math.sin(a2), Math.cos(a2), 0], dh = [Math.sin(ha), Math.cos(ha), 0], add = (p, d, l) => [p[0] + d[0] * l, p[1] + d[1] * l, p[2] + d[2] * l];
      const g02 = add(hip, d0, Ln.s[0]), g01 = add(g02, d1, Ln.s[1]), gsp = add(g01, d2, Ln.s[2]), gnk = add(gsp, d2, Ln.s[3]), ghd = add(gnk, dh, Ln.s[4]);
      drive('Hips', [0, 0, 0], mv(d0), mv(pax)); drive('Spine02', mp(g02), mv(d1), mv(mid)); drive('Spine01', mp(g01), mv(d2), mv(cax)); drive('Spine', mp(gsp), mv(d2), mv(cax)); drive('neck', mp(gnk), mv(dh), mv(hz)); drive('Head', mp(ghd), mv(dh), mv(hz)); ride('head_end', 'Head'); ride('headfront', 'Head');
      const tg = {}; for (const n of ['hL', 'hR', 'fL', 'fR']) tg[n] = po[n].slice();
      if (po.aim && fp.hb && po.aw > 0) { const h = fp.hb, t = tg[po.aim], tx = po.up ? h[0] + h[2] * 0.6 : h[0] + h[2] - 3, ty = po.up ? h[1] + h[3] - 5 : h[1] + h[3] * po.ay; t[0] += (tx - t[0]) * po.aw; t[1] += (ty - t[1]) * po.aw; }
      const cr = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]], sb = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]], ln = v => Math.hypot(v[0], v[1], v[2]);
      for (const n of ['L', 'R']) { const sd = n === 'L' ? -1 : 1, S = n === 'L' ? 'Left' : 'Right';
        ride(S + 'Shoulder', 'Spine'); const shM = ride(S + 'Arm', 'Spine'), sh = gp(shM), ht = tg['h' + n], e = po['e' + n], pole = [-0.35, e, sd * 0.7 * (e > 0 ? 1.4 : 1)];
        const A = ik(sh, [ht[0], ht[1], sh[2] * 0.45 + ht[2]], Ln.UA, Ln.FA, pole, 1.3), u = mv(sb(A[0], sh)), f = mv(sb(A[1], A[0])); let ax = cr(u, f); if (ln(ax) < 0.02 * ln(u) * ln(f)) ax = cr(u, mv(pole));
        drive(S + 'Arm', shM, u, ax, ln(u) / Ln.UA); drive(S + 'ForeArm', mp(A[0]), f, ax, ln(f) / Ln.FA); P[S + 'ForeArm'] = mp(A[0]); const wr = mp(A[1]);
        { const r = Rm[S + 'ForeArm'], eH = bones[K.id[S + 'Hand']].matrix.elements; for (let c = 0; c < 3; c++) { eH[c * 4] = r[c]; eH[c * 4 + 1] = r[3 + c]; eH[c * 4 + 2] = r[6 + c]; eH[c * 4 + 3] = 0; } eH[12] = wr[0]; eH[13] = wr[1]; eH[14] = wr[2]; eH[15] = 1; bones[K.id[S + 'Hand']].matrixWorldNeedsUpdate = true; }
        const hsM = ride(S + 'UpLeg', 'Hips'), hs = gp(hsM), ft = tg['f' + n], kk = po['k' + n], ay = ft[1] - 4 + Ln.AH, kp = [0.8, 0.45, sd * (0.4 + kk * 1.6)];
        const Lg = ik(hs, [ft[0], fp.air ? ay : Math.max(Ln.AH, ay), sd * 5.5 + ft[2]], Ln.TH, Ln.SH, kp, 1.22), th = mv(sb(Lg[0], hs)), sn = mv(sb(Lg[1], Lg[0])); let kx = cr(th, sn); if (ln(kx) < 0.02 * ln(th) * ln(sn)) kx = cr(th, mv(kp));
        drive(S + 'UpLeg', hsM, th, kx, ln(th) / Ln.TH); drive(S + 'Leg', mp(Lg[0]), sn, kx, ln(sn) / Ln.SH);
        const sx = Lg[1][0] - Lg[0][0], sy2 = Lg[1][1] - Lg[0][1], sl = Math.hypot(sx, sy2) || 1, k = fp.air ? 1 : Math.max(0, Math.min(1, (Lg[1][1] - Ln.AH - 1) / 8)), fx0 = sd > 0 && !fp.air ? 0.8 : 1;
        drive(S + 'Foot', mp(Lg[1]), mv([fx0 * (1 - k) + (-sy2 / sl * 0.85 + sx / sl * 0.35) * k, (sx / sl * 0.85 + sy2 / sl * 0.35) * k, (sd > 0 ? 0.6 : -0.15) * (1 - k)]), mv([0, 0, 1])); ride(S + 'ToeBase', S + 'Foot');
      }
    }
    rig.R.position.set(hip[0], hip[1], 0); rig.R.rotation.set(0, (po.yaw + fp.yaw) * D, -po.rot * D); rig.M.scale.x = face;
    // state: parrying it is its accent colour outright, flying a Slipstream it goes pale, burnt out it drains to ash
    const U = rig.U, md = rig.mode; if (md === 'parry') { const c = rig.th.parry; U.flat.value.set(c[0], c[1], c[2], 1); } else if (md === 'home') { const c = rig.th.home; U.flat.value.set(c[0], c[1], c[2], 0.85); } else U.flat.value.w = 0;
    U.grey.value = bk * ((tick % 53) < 2 && bk > 0.5 ? 0.2 : 0.92); if (rimL) U.rim.value.copy(rimL.color).lerp(_white, 0.55).multiplyScalar(Math.min(0.42, rimL.intensity * 0.26));
    U.dith.value = rig.phase || 0;
    rig.mats.body.emissive.setRGB(flashK + tint[0], flashK + tint[1], flashK + tint[2]);
  }
  // Cloth. Each chain hangs off one body joint. Its points are simulated in the world: they keep their momentum, sag, drift with the wind,
  // and are drawn back toward the shape they were sculpted in, firmly near the root and loosely at the tip. Each link keeps its length.
  const CLP = [{ damp: 0.86, g: 0.05, w: 0.3, tb: 0.06, ws: 0.05, k0: 0.36, k1: 0.07 }, { damp: 0.9, g: 0.012, w: 0.55, tb: 0.24, ws: 0.07, k0: 0.17, k1: 0.022 }, { damp: 0.9, g: 0.03, w: 0.45, tb: 0.13, ws: 0.06, k0: 0.22, k1: 0.035 }];
  const _cA = new THREE.Matrix4(), _cT = new THREE.Matrix4(), _cL = new THREE.Matrix4(), _cv = new THREE.Vector3(), _c0 = new THREE.Vector3(), _c1 = new THREE.Vector3(), _cq = new THREE.Quaternion();
  function clothStep(rig) {
    const bones = rig.bones;
    for (const c of rig.chains) { const bp = KM.bp[c.a], n = c.j.length, P = c.p, Q = c.q, Tg = c.t, pr = CLP[c.kind];
      _cA.copy(bones[c.a].matrixWorld).multiply(_cT.makeTranslation(-bp[0], -bp[1], -bp[2]));
      for (let i = 0; i < n; i++) { const b = KM.bp[c.j[i]]; Tg[i].set(b[0], b[1], b[2]).applyMatrix4(_cA); }
      if (!c.init || P[0].distanceToSquared(Tg[0]) > 4900) { for (let i = 0; i < n; i++) { P[i].copy(Tg[i]); Q[i].copy(Tg[i]); } c.init = 1; }
      P[0].copy(Tg[0]); Q[0].copy(Tg[0]);
      for (let i = 1; i < n; i++) { const p = P[i], u = i / (n - 1);
        _cv.subVectors(p, Q[i]).multiplyScalar(pr.damp); Q[i].copy(p); p.add(_cv);
        p.y -= pr.g * (0.4 + u); p.x += WIND * pr.w * u + Math.sin(tick * pr.ws + i * 0.9 + c.ph) * pr.tb * u; p.y += Math.cos(tick * pr.ws * 0.8 + i * 1.1 + c.ph) * pr.tb * 0.6 * u;
        p.lerp(Tg[i], pr.k0 + (pr.k1 - pr.k0) * u);
        _cv.subVectors(p, P[i - 1]); const L = _cv.length() || 1; p.copy(P[i - 1]).addScaledVector(_cv, c.len[i] / L); if (p.y < 1) p.y = 1; }
      _cL.copy(_cA).setPosition(0, 0, 0);
      for (let i = 0; i < n; i++) { const k = i < n - 1 ? i : i - 1, bo = bones[c.j[i]]; _c0.copy(c.dir[k]).transformDirection(_cA); _c1.subVectors(P[k + 1], P[k]).normalize(); _cq.setFromUnitVectors(_c0, _c1);
        bo.matrix.makeRotationFromQuaternion(_cq).multiply(_cL).setPosition(P[i]); bo.matrixWorldNeedsUpdate = true; }
    }
    // afterimages
    for (const g of rig.ghosts) { if (g.life > 0) { g.life--; g.ms.visible = g.life > 0 && rig.G.visible; g.u.dith.value = 0.45 + 0.4 * (1 - g.life / g.max); } }
    if (rig.echo > 0 && rig.G.visible && tick % (rig.echo > 1 ? 3 : 4) === 0) { const g = rig.ghosts[rig.gn++ % rig.ghosts.length]; rig.mesh.updateMatrixWorld(true); for (let i = 0; i < bones.length; i++) { if (i < 24) bones[i].updateMatrixWorld(true); g.gb[i].matrix.copy(i < 24 ? bones[i].matrixWorld : bones[i].matrix); g.gb[i].matrixWorldNeedsUpdate = true; } g.life = g.max = rig.echo > 1 ? 11 : 9; g.ms.visible = true; g.u.dith.value = 0.45; }
  }
  const Z001 = [0, 0, 1];
  function trailStep(rig, face) {
    if (!rig.trails) return; rig.G.updateMatrixWorld(true); if (rig.skin) clothStep(rig); const gx = rig.G.position.x, gy = rig.G.position.y;
    for (let ti = 0; ti < rig.trails.length; ti++) { const t = rig.trails[ti];
      const a = t.par.localToWorld(_tv.copy(t.off)), n = t.pts.length, gr = rig.grab && ti < 3 ? rig.grab : null;
      if (t.strip) { stripStep(rig, t, a, face, gr, ti); continue; }
      if (!t.init || t.pts[0].distanceToSquared(a) > 4900) { for (let i = 0; i < n; i++) t.pts[i].set(a.x - face * t.len * i * 0.7, a.y + t.len * i * 0.1, a.z); t.init = 1; }
      t.pts[0].copy(a);
      if (gr) { const sgn = ti === 1 ? -1 : 1, rad = (gr.wrap ? 9 - gr.sq * 4 : 0);
        for (let i = 1; i < n; i++) { const u = i / (n - 1), k = Math.min(1, u * 1.35), p = t.pts[i]; let x = a.x + (gr.x - a.x) * k, y = a.y + (gr.y - a.y) * k + Math.sin(Math.min(1, k) * Math.PI) * 13 * sgn * (ti === 2 ? 0.4 : 1), z = a.z;
          if (gr.wrap && u > 0.6) { const an = (u - 0.6) * 15 * sgn + tick * 0.22 * sgn + ti * 2; x = gr.x + Math.cos(an) * rad; y = gr.y - 6 + (u - 0.6) * 44 - ti * 5 + Math.sin(an) * 3; z = a.z + Math.sin(an) * rad; }
          _tr.set(x, y, z); p.lerp(_tr, 0.5); }
      } else
      for (let i = 1; i < n; i++) { const p = t.pts[i], q = t.pts[i - 1], wv = Math.sin(tick * 0.09 + i * 0.7 + t.len) * 0.5;
        _tr.set(q.x - face * t.len * 0.62 * t.back + WIND * 3, q.y - t.len * 0.3 + wv * 2, q.z); p.lerp(_tr, 0.085); p.y -= t.grav; p.x += WIND * (0.5 + i * 0.07); p.y += wv * 0.35; p.z += (a.z + (t.sz || 0) * i - p.z) * 0.2;
        _tr.subVectors(p, q); const L = _tr.length() || 1; p.copy(q).addScaledVector(_tr, t.len / L); if (p.y < 1.5) p.y = 1.5; }
      for (let i = 0; i < n - 1; i++) { const p0 = t.pts[i], p1 = t.pts[i + 1]; place(t.segs[i], [p0.x, p0.y, p0.z], [p1.x, p1.y, p1.z], t.len, t.zh || Z001); }
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
    if (rig.skin) return applyPoseS(rig, fp, face, flashK, tint);
    const dm = rig.dim || DIMS, SP1 = dm.SP1, SP2 = dm.SP2, UA = dm.UA, FA = dm.FA, TH = dm.TH, SHN = dm.SHN, SW = dm.SW, HW = dm.HW;
    const lo = (fp.loose == null ? 1 : fp.loose) * (rig.solid ? 0.28 : 1), s1 = Math.sin(tick * 0.052 + rig.seed), s2 = Math.sin(tick * 0.037 + rig.seed * 1.7), s3 = Math.sin(tick * 0.083 + rig.seed * 0.6), bk = rig.boK = (rig.boK || 0) + ((rig.boT || 0) - (rig.boK || 0)) * 0.14, vl = Math.max(-6, Math.min(6, (rig.vel || 0) * face)), po0 = fp.po,
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
    const ha = a2 * 0.45 + po.hd * D, back = Math.max(0, Math.min(1, (Math.abs(po.tw) - 100) / 50)), hz = [fp.straight ? 0 : -0.5 + back * 0.9 + cax[0] * 0.12, 0, 0.82];
    place(b.head, sub([nk[0], nk[1] + 1, 0]), sub([nk[0] + Math.sin(ha) * 10, nk[1] + 1 + Math.cos(ha) * 10, 0]), 0, hz);
    const wob = i => Math.sin(tick * 0.085 + rig.seed + i * 1.9), lo2 = 0.45 + 0.55 * lo;
    const bend = (k, p0, p1, rest, amt) => { if (rig.solid) amt = 0; const dx = p1[0] - p0[0], dy = p1[1] - p0[1], L = Math.hypot(dx, dy) || 1, m = [(p0[0] + p1[0]) / 2 - dy / L * amt, (p0[1] + p1[1]) / 2 + dx / L * amt, (p0[2] + p1[2]) / 2]; place(b[k], sub(p0), sub(m), rest / 2, Zc); place(b[k + 'b'], sub(m), sub(p1), rest / 2, Zc); };
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
    if (rig.solid) { rig.R.scale.set(1, 1, 1);
      const pc = rig.mode === 'parry' ? (rig.ch ? 0xff2f96 : 0x1f7dff) : rig.mode === 'home' ? (rig.ch ? 0xff78bc : 0x58aeff) : -1, fk = bk > 0.5 && (tick % 53) < 2 ? 0.2 : 1, g = bk * 0.94 * fk;
      for (const m of rig.allM) { if (pc >= 0) m.color.setHex(m === rig.ink || m === rig.lit.eye ? 0xffffff : pc); else m.color.setHex(m.userData.c).lerp(m === rig.ink ? _gcA : m === rig.mats.body || m === rig.mats.body2 ? _gcB : _gc, m === rig.lit.eye ? 0 : g); }
    } else { const sq = 1 + 0.035 * s3 * lo; rig.R.scale.set(1 / sq + Math.abs(vl) * 0.02, sq - Math.abs(vl) * 0.012, 1); if (rig.lk) {
        // the body says what state it is in: parrying it becomes its own accent colour outright (Ret all blue, Ard all pink),
        // flying a Slipstream it goes pale and bright, burnt out every colour drains to ash and the accents die
        const L = rig.lk, Mt = rig.mats, Lt = rig.lit, set = (m, hex) => { if (m) m.color.setHex(hex); };
        if (rig.mode === 'parry') { const pc = rig.ch ? 0xff2f96 : 0x1f7dff; set(Mt.body, pc); set(Mt.body2, pc); set(Lt.acc, 0xffffff); set(Lt.eye, 0xffffff); set(rig.ink, L.acc2); set(rig.pm, pc); }
        else if (rig.mode === 'home') { rig.ink.color.setHex(0xffffff); const hc = rig.ch ? 0xff78bc : 0x58aeff; set(Mt.body, hc); set(Mt.body2, hc); set(Lt.acc, 0xffffff); set(Lt.eye, 0xffffff); set(rig.pm, hc); }
        else { const fk = bk > 0.5 && (tick % 53) < 2 ? 0.2 : 1, g = bk * 0.94 * fk, ash = rig.ch ? _gcB : _gcA;
          Mt.body.color.setHex(L.body).lerp(ash, g); if (Mt.body2) Mt.body2.color.setHex(L.body2).lerp(_gcD, g); Lt.acc.color.setHex(L.acc).lerp(_gcD, Math.min(1, bk * fk)); if (Lt.eye) Lt.eye.color.setHex(L.eye).lerp(_gcD, bk * 0.7 * fk);
          rig.ink.color.setHex(L.ink).lerp(rig.ch ? _gcA : _gcD, g); if (rig.pm) rig.pm.color.setHex(L.body).lerp(ash, g); }
      } }
    for (const k in rig.mats) { const m = rig.mats[k]; m.emissive.setRGB(flashK + tint[0], flashK + tint[1], flashK + tint[2]); }
  }

  // ----------------------------------------------------------- stage
  function tex(w, h, fn, rep) { const c = document.createElement('canvas'); c.width = w; c.height = h; fn(c.getContext('2d'), w, h); const t = new THREE.CanvasTexture(c); t.magFilter = THREE.NearestFilter; t.minFilter = THREE.NearestFilter; if (rep) { t.wrapS = t.wrapT = THREE.RepeatWrapping; t.repeat.set(rep[0], rep[1]); } return t; }
  function hsh(n) { n = Math.imul(n ^ 0x9e3779b9, 0x85ebca6b); n ^= n >>> 13; n = Math.imul(n, 0xc2b2ae35); return ((n ^ (n >>> 16)) >>> 0) / 4294967296; }
  function buildStage() {
    const st = new THREE.Group();
    // dusk sky: charcoal into plum, one enormous crimson sun, pixel cloud bands, temple and torii silhouettes on the horizon
    const sky = tex(768, 192, (c, w, h) => {
      const g = c.createLinearGradient(0, 0, 0, h); g.addColorStop(0, '#16111b'); g.addColorStop(0.36, '#291a27'); g.addColorStop(0.6, '#472231'); g.addColorStop(0.8, '#742b35'); g.addColorStop(0.88, '#9a3b3a'); g.addColorStop(1, '#3a1d28'); c.fillStyle = g; c.fillRect(0, 0, w, h);
      const sx = 384, sy = 116, sr = 40; for (let dy = -sr; dy <= sr; dy++) { const hw = Math.round(Math.sqrt(sr * sr - dy * dy) / 2) * 2; c.fillStyle = dy > sr * 0.55 ? '#a81f25' : dy > 0 ? '#bb2328' : '#c9282b'; c.fillRect(sx - hw, sy + dy, hw * 2, 1); }
      const band = (x, y, wd, col, a) => { c.globalAlpha = a; c.fillStyle = col; for (let i = 0; i < 4; i++) { const k = [0.55, 1, 0.8, 0.4][i]; c.fillRect(Math.round(x + wd * (1 - k) / 2 + hsh(x + i) * 8), y + i, Math.round(wd * k), 1); } c.globalAlpha = 1; };
      band(390, 96, 92, '#e9a58a', 0.95); band(318, 110, 66, '#d98f7c', 0.9); band(350, 132, 104, '#e9a58a', 0.85); band(432, 124, 48, '#c97a70', 0.8);
      band(168, 70, 90, '#3b2434', 0.9); band(508, 58, 110, '#3b2434', 0.9); band(248, 92, 70, '#5a2c3a', 0.8); band(538, 100, 80, '#5a2c3a', 0.8); band(138, 120, 70, '#6a3340', 0.7);
      band(20, 52, 120, '#3b2434', 0.9); band(60, 104, 84, '#5a2c3a', 0.8); band(640, 76, 110, '#3b2434', 0.9); band(690, 118, 70, '#6a3340', 0.7); band(604, 132, 60, '#5a2c3a', 0.7); band(96, 138, 50, '#6a3340', 0.6);
      c.fillStyle = '#3b2230'; c.beginPath(); c.moveTo(0, 164); for (let x = 0; x <= w; x += 8) c.lineTo(x, 150 - 10 * Math.sin(x * 0.021) - 6 * Math.sin(x * 0.05 + 1)); c.lineTo(w, 192); c.lineTo(0, 192); c.fill();
      const sil = '#241722', win = '#ff7e3a';
      const roof = (x, y, wd) => { c.fillStyle = sil; c.fillRect(x - wd / 2, y, wd, 3); c.fillRect(x - wd / 2 - 3, y + 1, 3, 2); c.fillRect(x + wd / 2, y + 1, 3, 2); c.fillRect(x - wd / 2 - 5, y - 1, 3, 2); c.fillRect(x + wd / 2 + 2, y - 1, 3, 2); c.fillRect(x - wd / 2 + 4, y - 2, wd - 8, 2); };
      const pagoda = (x, by, tiers, wd) => { let y = by; for (let i = 0; i < tiers; i++) { const tw = wd - i * 8; c.fillStyle = sil; c.fillRect(x - tw / 2 + 4, y - 10, tw - 8, 10); if (i < 2) { c.fillStyle = win; for (let k = -1; k <= 1; k += 2) c.fillRect(x + k * (tw / 5) - 2, y - 7, 3, 4); } roof(x, y - 13, tw + 6); y -= 13; } c.fillStyle = sil; c.fillRect(x - 1, y - 8, 2, 8); };
      const torii = (x, by, s) => { c.fillStyle = sil; c.fillRect(x - 9 * s, by - 20 * s, 2 * s, 20 * s); c.fillRect(x + 7 * s, by - 20 * s, 2 * s, 20 * s); c.fillRect(x - 13 * s, by - 23 * s, 26 * s, 2 * s); c.fillRect(x - 15 * s, by - 25 * s, 30 * s, 2 * s); c.fillRect(x - 10 * s, by - 17 * s, 20 * s, 1.5 * s); };
      pagoda(546, 160, 3, 52); pagoda(598, 162, 2, 40); pagoda(484, 162, 2, 30); torii(220, 160, 1.4); torii(278, 162, 0.8); pagoda(162, 162, 2, 36);
      pagoda(60, 161, 3, 44); torii(112, 162, 1); pagoda(690, 161, 2, 38); torii(650, 162, 1.2); pagoda(736, 162, 3, 46); pagoda(18, 162, 2, 30);
      c.fillStyle = sil; c.fillRect(0, 160, w, 32);
      const m = c.createLinearGradient(0, 140, 0, 170); m.addColorStop(0, 'rgba(190,70,60,0)'); m.addColorStop(1, 'rgba(190,70,60,0.35)'); c.fillStyle = m; c.fillRect(0, 140, w, 30);
    });
    // the sky is the inside of a drum around the whole yard, so the camera can swing and never find the edge of the painting
    const SR = 1250, SH = SR * 2 * Math.PI / 768 * 192;
    const skyM = new THREE.Mesh(new THREE.CylinderGeometry(SR, SR, SH, 56, 1, true), new THREE.MeshBasicMaterial({ map: sky, fog: false, side: THREE.BackSide })); skyM.scale.x = -1; skyM.position.set(0, SH / 2 - SH * 32 / 192 - 6, 0); st.add(skyM);
    const cap = new THREE.Mesh(new THREE.CircleGeometry(SR, 40), new THREE.MeshBasicMaterial({ color: 0x16111b, fog: false, side: THREE.DoubleSide })); cap.rotation.x = Math.PI / 2; cap.position.y = SH / 2; skyM.add(cap);   // lid, so a low camera looking up never sees past the painting
    // rain-wet flagstones with the sun lying across them
    const floor = tex(64, 64, (c) => { c.fillStyle = '#1d161c'; c.fillRect(0, 0, 64, 64); for (let y = 0; y < 4; y++) for (let x = 0; x < 2; x++) { const o = (y % 2) * 16; c.fillStyle = ['#2c2229', '#31262d', '#281e25', '#352830'][(x + y * 3) % 4]; c.fillRect(x * 32 + o - 16, y * 16, 31, 15); c.fillRect(x * 32 + o + 48, y * 16, 31, 15); } for (let i = 0; i < 46; i++) { c.fillStyle = hsh(i * 5) > 0.6 ? 'rgba(230,90,70,0.16)' : 'rgba(255,220,190,0.07)'; c.fillRect(hsh(i) * 64 | 0, hsh(i + 9) * 64 | 0, 2 + (hsh(i + 3) * 4 | 0), 1); } }, [35, 17]);
    const far = new THREE.Mesh(new THREE.CircleGeometry(1260, 40), new THREE.MeshBasicMaterial({ color: 0x1c131a })); far.rotation.x = -Math.PI / 2; far.position.set(0, -0.6, 0); st.add(far);
    const fl = new THREE.Mesh(new THREE.PlaneGeometry(2400, 1100), new THREE.MeshLambertMaterial({ map: floor })); fl.rotation.x = -Math.PI / 2; fl.position.set(0, 0, 170); st.add(fl);
    const glare = tex(32, 64, (c) => { for (let y = 0; y < 64; y++) { const a = Math.pow(1 - y / 64, 1.4) * 0.75; for (let x = 0; x < 32; x++) { const e = 1 - Math.abs(x - 15.5) / 16; if (hsh(x * 7 + y * 131) < e * 1.5) { c.fillStyle = 'rgba(214,52,44,' + (a * (0.5 + 0.5 * e)).toFixed(3) + ')'; c.fillRect(x, y, 1, 1); } } } });
    const gl = new THREE.Mesh(new THREE.PlaneGeometry(330, 760), new THREE.MeshBasicMaterial({ map: glare, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, fog: false })); gl.rotation.x = -Math.PI / 2; gl.position.set(0, 0.3, 0); st.add(gl);
    const silM = new THREE.MeshBasicMaterial({ color: 0x21151d }), sil2 = new THREE.MeshBasicMaterial({ color: 0x2e1c26 }), stone = new THREE.MeshLambertMaterial({ color: 0x4a3a40 }), stone2 = new THREE.MeshLambertMaterial({ color: 0x3a2c32 });
    const B = (w, h, d, x, y, z, m) => { const b = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), m); b.position.set(x, y, z); st.add(b); return b; };
    const gate = (x, z, s, m) => { B(14 * s, 150 * s, 14 * s, x - 78 * s, 75 * s, z, m); B(14 * s, 150 * s, 14 * s, x + 78 * s, 75 * s, z, m); B(230 * s, 12 * s, 18 * s, x, 150 * s, z, m); B(260 * s, 11 * s, 22 * s, x, 164 * s, z, m); B(24 * s, 8 * s, 24 * s, x - 136 * s, 171 * s, z, m).rotation.z = 0.25; B(24 * s, 8 * s, 24 * s, x + 136 * s, 171 * s, z, m).rotation.z = -0.25; B(180 * s, 9 * s, 12 * s, x, 122 * s, z, m); };
    gate(-360, -300, 1, silM); gate(-600, -420, 0.7, sil2); gate(470, -440, 0.75, sil2); gate(-900, -260, 1.1, silM); gate(880, -300, 1.15, silM); gate(120, -640, 0.9, sil2); gate(-1000, 240, 1.2, silM); gate(1020, 200, 1.2, silM);
    // broken stone steps and blocks at the edges of the yard
    for (const sgn of [-1, 1]) { B(120, 26, 70, sgn * 560, 13, -150, stone); B(90, 24, 60, sgn * 590, 38, -160, stone2); B(60, 22, 50, sgn * 610, 61, -170, stone); B(46, 16, 40, sgn * 430, 8, -60, stone2).rotation.y = 0.3 * sgn; B(30, 12, 26, sgn * 250, 6, -230, stone2).rotation.y = -0.4 * sgn; }
    for (let i = 0; i < 14; i++) { const x = (hsh(i * 7 + 900) - 0.5) * 1100, z = 34 + hsh(i * 13 + 901) * 26, w = 7 + hsh(i * 3 + 902) * 13; B(w, 3 + hsh(i * 5 + 903) * 5, w * 0.8, x, 2, z, i & 1 ? stone : stone2).rotation.y = hsh(i * 11) * 3; }
    for (let i = 0; i < 10; i++) { const sgn = i & 1 ? 1 : -1, x = sgn * (640 + (i >> 1) * 90), z = -260 + (i >> 1) * 120; B(12, 8, 12, x, 4, z, stone); B(6, 34, 6, x, 24, z, stone); const l = new THREE.Mesh(new THREE.BoxGeometry(12, 11, 12), new THREE.MeshBasicMaterial({ color: 0xff8a3a })); l.position.set(x, 46, z); st.add(l); }
    st.userData.signs = []; st.userData.lamps = [];
    for (const x of [-250, 250]) { B(20, 8, 20, x, 4, -190, stone); B(9, 40, 9, x, 28, -190, stone); const l = new THREE.Mesh(new THREE.BoxGeometry(17, 15, 17), new THREE.MeshBasicMaterial({ color: 0xff8a3a })); l.position.set(x, 56, -190); st.add(l); st.userData.lamps.push(l); const pl = new THREE.PointLight(0xff8a3a, 0.9, 300, 1.4); pl.position.set(x, 58, -160); pl.layers.enable(1); st.add(pl); (st.userData.lampL = st.userData.lampL || []).push(pl); B(26, 6, 26, x, 67, -190, stone); B(14, 6, 14, x, 73, -190, stone); }
    // maples: dark leaning trunks and a canopy of flat red blocks
    const leafM = [0xd8322e, 0xb81f26, 0xef5a3c, 0x8f1a22, 0xc62828].map(c => new THREE.MeshBasicMaterial({ color: c, fog: false })), bark = new THREE.MeshBasicMaterial({ color: 0x1c1118 });
    // Every tree is a real volume now: a leaning trunk that forks into boughs running off in all directions, each ending in a
    // layered pad of leaf blocks. Lit from the top by colour (dark underneath, bright on the crown) so it reads from any angle,
    // including from underneath. All leaves in the yard are drawn as five instanced batches.
    const leafBins = [[], [], [], [], []], SHADE = [3, 1, 4, 0, 2], V = (x, y, z) => new THREE.Vector3(x, y, z), UPV = V(0, 1, 0);
    const limb = (a, b, th) => { const d = new THREE.Vector3().subVectors(b, a), len = d.length(), m = new THREE.Mesh(new THREE.BoxGeometry(th, len + th * 0.6, th), bark); m.position.copy(a).addScaledVector(d, 0.5); m.quaternion.setFromUnitVectors(UPV, d.normalize()); m.rotateY(hsh(len) * 3); st.add(m); };
    const pad = (c, rx, ry, rz, n, s, seed) => { for (let i = 0; i < n; i++) { const a = hsh(seed + i * 3) * 6.283, u = Math.sqrt(hsh(seed + i * 7 + 1)), yy = (hsh(seed + i * 11 + 2) - 0.5) * 2 * ry * (1 - 0.6 * u), sz = (9 + hsh(seed + i * 13 + 3) * 14) * s;
        const m4 = new THREE.Matrix4().compose(V(c.x + Math.cos(a) * u * rx, c.y + yy, c.z + Math.sin(a) * u * rz), new THREE.Quaternion().setFromEuler(new THREE.Euler((hsh(seed + i * 17 + 4) - 0.5) * 0.9, hsh(seed + i * 19 + 5) * 6.283, (hsh(seed + i * 23 + 6) - 0.5) * 0.9)), V(sz, sz * 0.5, sz * 0.92));
        const t = Math.max(0, Math.min(0.999, (yy / ry * 0.5 + 0.5) * 0.72 + hsh(seed + i * 29 + 7) * 0.42 - 0.06)); leafBins[SHADE[(t * 5) | 0]].push(m4); } };
    const maple = (x, z, s, dir, seed) => {
      const p0 = V(x, 0, z), p1 = V(x - dir * 16 * s, 92 * s, z + 5 * s), p2 = V(x - dir * 50 * s, 160 * s, z + 12 * s);
      limb(p0, p1, 17 * s); limb(p1, p2, 13 * s);
      for (let k = 0; k < 4; k++) { const a = k * 1.571 + 0.6; limb(V(x + Math.cos(a) * 17 * s, 0, z + Math.sin(a) * 15 * s), V(x - dir * 3 * s, 26 * s, z + 1 * s), 9 * s); }
      const cx = x - dir * 92 * s, cy = 224 * s, cz = z + 12 * s, N = 8;
      for (let k = 0; k < N; k++) {
        const top = k === N - 1, a = k / (N - 1) * 6.283 + hsh(seed + k * 5) * 0.6, R = top ? 0 : (64 + hsh(seed + k * 9 + 2) * 60) * s;
        const tip = V(cx + Math.cos(a) * R, cy + (top ? 30 * s : (hsh(seed + k * 3 + 1) - 0.45) * 46 * s), cz + Math.sin(a) * R * 0.9);
        const el = new THREE.Vector3().lerpVectors(p2, tip, 0.52); el.y -= (8 + hsh(seed + k * 7 + 3) * 14) * s; el.x += (hsh(seed + k * 13) - 0.5) * 22 * s; el.z += (hsh(seed + k * 17) - 0.5) * 22 * s;
        limb(p2, el, 8 * s); limb(el, tip, 5.5 * s);
        pad(V(tip.x, tip.y + 7 * s, tip.z), (42 + hsh(seed + k * 21) * 18) * s, 15 * s, (38 + hsh(seed + k * 27) * 16) * s, top ? 38 : 30, s, seed + k * 101);
      }
      // one low bough reaching back the other way, so the trunk is not bare on its far side
      const lt = V(x + dir * 74 * s, 152 * s, z - 34 * s), le = V(x + dir * 30 * s, 118 * s, z - 12 * s); limb(p1, le, 7 * s); limb(le, lt, 5 * s); pad(V(lt.x, lt.y + 6 * s, lt.z), 36 * s, 12 * s, 32 * s, 22, s, seed + 997);
    };
    maple(470, -140, 1, 1, 10); maple(-520, -170, 0.9, -1, 400); maple(-880, 60, 1.2, -1, 800); maple(900, 90, 1.25, 1, 1200);
    { const lgeo = new THREE.BoxGeometry(1, 1, 1); for (let b = 0; b < 5; b++) { const im = new THREE.InstancedMesh(lgeo, leafM[b], leafBins[b].length); leafBins[b].forEach((m4, i) => im.setMatrixAt(i, m4)); im.instanceMatrix.needsUpdate = true; im.frustumCulled = false; st.add(im); } }
    // leaves on the wind
    st.userData.leaves = []; const lg = new THREE.PlaneGeometry(4.5, 3.5);
    for (let i = 0; i < 44; i++) { const m = new THREE.Mesh(lg, leafM[i % 5]); m.material.side = THREE.DoubleSide; m.userData = { x: (hsh(i * 3 + 700) - 0.5) * 1500, y: hsh(i * 5 + 701) * 260, z: -220 + hsh(i * 7 + 702) * 330, v: 0.35 + hsh(i * 11 + 703) * 0.55, p: hsh(i * 13) * 6.283 }; st.add(m); st.userData.leaves.push(m); }
    st.userData.fx = { sky: skyM, glare: gl, cap };
    scene.add(st); return st;
  }

  // ----------------------------------------------------------- atmosphere director
  // One mood is always being blended toward. Which mood depends on the fight: the round, who is close to death, burnout, supers, the KO.
  // A stage only has to hand over its sky planes and floor glare; the lights, weather, wind, rays, mist and grade are shared by every stage.
  let ambL, keyL, rimL, sunL, projL, hitL = [], hitN = 0, WIND = -0.5, atm = null;
  const _clip = [new THREE.Plane(new THREE.Vector3(0, -1, 0), 0)], _noclip = [], _gc = new THREE.Color(0x6a6e78), _gcA = new THREE.Color(0x868a92), _gcB = new THREE.Color(0x3e4048), _gcD = new THREE.Color(0x2c2e34);
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
      A.night = new THREE.Mesh(fx.sky.geometry, new THREE.MeshBasicMaterial({ map: t, fog: false, transparent: true, opacity: 0, side: THREE.BackSide, depthWrite: false })); A.night.position.copy(fx.sky.position); A.night.scale.set(-0.996, 1, 0.996); A.night.renderOrder = -1; scene.add(A.night);
      if (fx.cap) { const nc = t.image.getContext('2d').getImageData(2, 2, 1, 1).data; A.ncap = new THREE.Mesh(fx.cap.geometry, new THREE.MeshBasicMaterial({ color: (nc[0] << 16) | (nc[1] << 8) | nc[2], fog: false, side: THREE.DoubleSide, transparent: true, opacity: 0, depthWrite: false })); A.ncap.rotation.x = Math.PI / 2; A.ncap.position.y = fx.cap.position.y - 2; A.night.add(A.ncap); } }
    // light shafts fanning out from behind the skyline
    A.rays = []; for (let i = 0; i < 7; i++) { const m = new THREE.Mesh(new THREE.PlaneGeometry(46 + (i % 3) * 22, 1100), new THREE.MeshBasicMaterial({ color: 0xffffff, transparent: true, opacity: 0, depthWrite: false, blending: THREE.AdditiveBlending, fog: false })); m.position.set(0, 230, -1180); m.scale.set(1.6, 1.6, 1); m.rotation.z = (i - 3) * 0.3 + 0.07; scene.add(m); A.rays.push(m); }
    // ground mist in two drifting sheets
    const mt = tex(128, 16, (c, w, h) => { for (let i = 0; i < 90; i++) { c.fillStyle = 'rgba(255,255,255,' + (0.05 + hsh(i) * 0.16).toFixed(3) + ')'; const y = hsh(i * 3) * h | 0; c.fillRect(hsh(i * 7) * w | 0, y, 10 + hsh(i * 11) * 44, 1 + (y > 8 ? 1 : 0)); } }, [3, 1]);
    A.mist = []; for (let i = 0; i < 2; i++) { const t2 = mt.clone(); t2.needsUpdate = true; const m = new THREE.Mesh(new THREE.PlaneGeometry(1700, 70), new THREE.MeshBasicMaterial({ map: t2, transparent: true, opacity: 0.3, depthWrite: false, fog: false })); m.position.set(0, i ? 30 : 16, i ? -220 : 22); if (!i) m.scale.y = 0.5; scene.add(m); A.mist.push(m); }
    // rain as slanted streaks, and one cloud of motes that serves as snow, embers or ash
    const NR = 260, rg = new THREE.BufferGeometry(); A.rp = new Float32Array(NR * 6); rg.setAttribute('position', new THREE.BufferAttribute(A.rp, 3)); A.rain = new THREE.LineSegments(rg, new THREE.LineBasicMaterial({ color: 0xaab8e0, transparent: true, opacity: 0.55, fog: false })); A.rain.frustumCulled = false; scene.add(A.rain);
    A.rd = []; for (let i = 0; i < NR; i++) A.rd.push({ x: (hsh(i * 3 + 5) - 0.5) * 1300, y: hsh(i * 7 + 6) * 300, z: -240 + hsh(i * 11 + 7) * 320, v: 7 + hsh(i * 13) * 5 });
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
      if (sup >= 0 && SIM.CH[s.p[sup].ch].mv[s.p[sup].mv].ca) { t.ambI *= 0.55; t.keyI *= 0.45; t.bolt = 1; t.ember = 1; t.wind += 1.6; t.rain = Math.max(t.rain || 0, 0.7); }
      if (sup >= 0) { rate = 0.22; t.ambI *= 0.42; t.keyI *= 0.5; t.rim = ACC[s.p[sup].ch].slice(); t.rimI = 2; t.rays = 1; t.vigA = 0.62; t.glare = ACC[s.p[sup].ch].slice(); t.wind += 0.8; t.sky = [t.sky[0] * 0.45, t.sky[1] * 0.45, t.sky[2] * 0.5]; }
      if (s.phase === 'ko') { rate = 0.1; t.rimI *= 1.5; t.keyI *= 0.6; t.vigA = 0.55; t.wind *= 0.3; t.rays += 0.4; }
    }
    A.crit = crit; mlerp(A.cur, t, rate); const c = A.cur;
    // lightning
    if (c.bolt > 0.2 && A.flash <= 0 && hsh(tick * 7 + 3) < c.bolt * 0.012) A.flash = 9;
    const fl = A.flash > 0 ? (A.flash > 6 || A.flash === 3 ? 1 : 0.25) : 0; if (A.flash > 0) A.flash--;
    WIND = -(c.wind + 0.25 * Math.sin(tick * 0.013) + 0.15 * Math.sin(tick * 0.041));
    ambL.color.setRGB(c.amb[0], c.amb[1], c.amb[2]); ambL.intensity = c.ambI + fl * 0.9; keyL.color.setRGB(c.key[0], c.key[1], c.key[2]); keyL.intensity = c.keyI + fl * 0.5;
    const shim = 1 + 0.06 * Math.sin(tick * 0.21) + 0.04 * Math.sin(tick * 0.057); rimL.color.setRGB(c.rim[0], c.rim[1], c.rim[2]); rimL.intensity = c.rimI * shim; sunL.color.setRGB(c.rim[0], c.rim[1], c.rim[2]); sunL.intensity = 0; for (const h of hitL) h.intensity *= 0.8;
    { const q = s && s.proj && s.proj.find(x => x); if (q) { projL.position.set(q.x / F, q.y / F, 24); projL.color.set(CSSA[q.ch]); projL.intensity = 1.5 + 0.3 * Math.sin(tick * 0.9); } else projL.intensity = 0; }
    scene.fog.color.setRGB(c.fog[0], c.fog[1], c.fog[2]); scene.fog.near = c.fogN - 330 + CAM.d; scene.fog.far = 1170 + CAM.d;
    const fx = stage.userData.fx; if (fx) { if (fx.sky) fx.sky.material.color.setRGB(Math.min(1, c.sky[0] + fl), Math.min(1, c.sky[1] + fl), Math.min(1, c.sky[2] + fl)); if (fx.cap) fx.cap.material.color.setRGB(0.086 * Math.min(1, c.sky[0] + fl), 0.067 * Math.min(1, c.sky[1] + fl), 0.106 * Math.min(1, c.sky[2] + fl)); if (A.ncap) A.ncap.material.opacity = Math.max(0, Math.min(1, c.night)); if (fx.glare) { fx.glare.material.color.setRGB(c.glare[0], c.glare[1], c.glare[2]); fx.glare.material.opacity = c.glareO * (0.85 + 0.15 * Math.sin(tick * 0.09)); } }
    if (A.night) { A.night.material.opacity = Math.max(0, Math.min(1, c.night)); A.night.visible = c.night > 0.01; }
    for (let i = 0; i < A.rays.length; i++) { const m = A.rays[i]; m.material.color.setRGB(c.rim[0], c.rim[1], c.rim[2]); m.material.opacity = c.rays * 0.085 * (0.6 + 0.4 * Math.sin(tick * 0.02 + i * 1.7)); m.rotation.z = (i - 3) * 0.3 + 0.07 + 0.03 * Math.sin(tick * 0.008 + i); m.visible = stage.visible; }
    for (let i = 0; i < A.mist.length; i++) { const m = A.mist[i]; m.material.color.setRGB(Math.min(1, c.fog[0] * 2.2 + 0.1), Math.min(1, c.fog[1] * 2.2 + 0.1), Math.min(1, c.fog[2] * 2.2 + 0.1)); m.material.opacity = c.mist * (i ? 0.9 : 0.55); m.material.map.offset.x -= WIND * (i ? 0.0005 : 0.0011); m.position.x = camX; m.rotation.y = CAM.yaw * D; m.visible = stage.visible; }
    // rain
    const rp = A.rp, nr = Math.round(A.rd.length * Math.min(1, c.rain)); A.rain.visible = nr > 0 && stage.visible; A.rain.geometry.setDrawRange(0, nr * 2);
    for (let i = 0; i < nr; i++) { const d = A.rd[i]; d.y -= d.v; d.x += WIND * 2.2; if (d.y < 0) { d.y = 290 + (i % 9) * 4; d.x = camX + ((i * 131 + tick * 3) % 1300) - 620; } rp[i * 6] = d.x; rp[i * 6 + 1] = d.y; rp[i * 6 + 2] = d.z; rp[i * 6 + 3] = d.x - WIND * 3.4; rp[i * 6 + 4] = d.y + 13; rp[i * 6 + 5] = d.z; }
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
    cam = new THREE.PerspectiveCamera(FOV0, 16 / 9, 4, 4200);
    ambL = new THREE.AmbientLight(0xa88a94, 0.6); scene.add(ambL); keyL = new THREE.DirectionalLight(0xffe8d0, 0.62); keyL.position.set(-0.4, 0.9, 1.2); scene.add(keyL); rimL = new THREE.DirectionalLight(0xff3a2a, 0.95); rimL.position.set(0.15, 0.5, -1); scene.add(rimL);
    sunL = new THREE.PointLight(0xff3a2a, 1.1, 0, 1); sunL.position.set(0, 215, -430); scene.add(sunL); projL = new THREE.PointLight(0xffffff, 0, 280, 1.4); scene.add(projL);
    for (let i = 0; i < 2; i++) { const h = new THREE.PointLight(0xffffff, 0, 320, 1.3); scene.add(h); hitL.push(h); }
    for (const l of [ambL, keyL, rimL, sunL, projL, hitL[0], hitL[1]]) l.layers.enable(1);
    stage = buildStage(); atmInit();
    for (let i = 0; i < 2; i++) { const s = new THREE.Mesh(new THREE.CircleGeometry(20, 12), new THREE.MeshBasicMaterial({ color: 0x080410, transparent: true, opacity: 0.55 })); s.rotation.x = -Math.PI / 2; s.position.y = 0.4; scene.add(s); shadows.push(s); }
    g2 = c2.getContext('2d'); g2.imageSmoothingEnabled = false;
  }
  function setFighters(chars) {
    for (const r of rigs) { scene.remove(r.G); if (r.T) scene.remove(r.T); } rigs = [];
    for (let i = 0; i < 2; i++) { const r = (KM ? buildRigS : chars[i] === 1 ? buildRigK : buildRig)(chars[i], i === 1 && chars[0] === chars[1]); scene.add(r.G); scene.add(r.T); rigs.push(r); }
    fx = []; combo[0] = combo[1] = null; msg = null; superFx = null; shake = 0; flash = 0;
  }
  // ----------------------------------------------------------- camera
  // An ultra-wide lens held close. It sits side-on while the fight is being read, and the director below swings it
  // around the action for the moments that deserve it: supers, the Slipstream, throws, the KO, the walk-on.
  // yaw orbits around the point being looked at, pit is elevation (negative = low angle), roll is the dutch tilt.
  // Kicks are one-shot shoves from impacts that decay on their own.
  const FOV0 = 78, CAM = { yaw: 0, pit: 4, roll: 0, zm: 1, fov: FOV0, d: 120, kRoll: 0, kZm: 0, kFov: 0, kYaw: 0, snap: 1 };
  function camKick(roll, zm, fov, yaw) { if (Math.abs(roll) > Math.abs(CAM.kRoll)) CAM.kRoll = roll; CAM.kZm = Math.min(CAM.kZm, zm || 0); CAM.kFov = Math.max(CAM.kFov, fov || 0); if (yaw && Math.abs(yaw) > Math.abs(CAM.kYaw)) CAM.kYaw = yaw; }
  function camDirect(s) {
    const t = { yaw: 0, pit: 4, roll: 0, zm: 1, fov: FOV0, k: 0.1, solo: 0 };
    if (!s) return t;
    const ez = u => u * u * (3 - 2 * u), cl = (v, a, b) => Math.max(a, Math.min(b, v));
    t.yaw = Math.sin(tick * 0.013) * 1.8; t.roll = Math.sin(tick * 0.009) * 0.5;
    if (s.phase === 'intro') { const u = cl(s.pt / (SIM.INTRO * 0.8), 0, 1), e = 1 - ez(u); t.yaw = -62 * e; t.pit = 4 + 9 * e; t.zm = 1 + 0.3 * e; t.roll = -5 * e; t.k = 0.3; return t; }
    for (let i = 0; i < 2; i++) {
      const p = s.p[i], o = s.p[1 - i], m = p.st === 'atk' ? SIM.CH[p.ch].mv[p.mv] : null;
      if (m && m.sup) {
        if (s.freeze > 0 && s.frzWho === i && superFx) { const dur = superFx.ca ? 84 : [0, 40, 44, 60][superFx.lv || 1], u = ez(cl(1 - s.freeze / dur, 0, 1)), big = superFx.ca ? 1 : 0;
          t.yaw = p.face * (big ? -58 + 100 * u : -40 + 66 * u); t.pit = big ? -9 + 6 * u : -6; t.roll = p.face * (big ? 12 - 20 * u : 8 - 12 * u); t.fov = FOV0 + (big ? 0 : 4) * (1 - u); t.k = 0.34; t.solo = 1; return t; }
        // the super itself: a slow arc that leans into the direction of the attack, low and looking up when it leaves the ground
        t.yaw = p.face * (m.ca ? 20 : 13) * Math.sin(Math.min(3.1, p.mf * 0.045)); t.roll = p.face * (m.ca ? 6 : 4) * Math.cos(p.mf * 0.06); t.pit = p.y > 30 * F ? -5 : 1; t.k = 0.14; return t;
      }
      if (m && m.home && p.mf >= m.home[0] && !p.hmE && !(p.contact && !p.fr)) { t.yaw = -p.face * 27; t.roll = -p.face * 7; t.pit = 1; t.fov = FOV0 + 6; t.k = 0.3; t.pad = 44; return t; }
      if (p.st === 'throwing') { const u = cl(p.sf / 26, 0, 1); t.yaw = p.face * 26 * ez(u); t.roll = p.face * 4; t.zm = 0.88; t.pit = 1; t.k = 0.16; return t; }
      if (m && m.di) { t.yaw = -p.face * 16; t.roll = -p.face * 5; t.zm = 0.9; t.k = 0.22; }
      else if (p.st === 'drush' || p.st === 'run') { t.yaw += -p.face * 11; t.roll += -p.face * 2.5; t.fov = FOV0 + 4; t.k = 0.14; }
      else if (o.st === 'ahit' && o.y > 46 * F && p.st !== 'ahit') { t.yaw += (o.vx > 0 ? 1 : -1) * 7; t.pit = 0; }
      else if (p.st === 'crumple') { t.yaw = -o.face * 14; t.zm = 0.86; t.roll = o.face * 3; t.k = 0.08; }
    }
    if (s.phase === 'ko' && s.rw >= 0) { const w = s.p[s.rw], l = s.p[1 - s.rw], side = w.x <= l.x ? -1 : 1;
      if (s.pt < 150) { const u = ez(cl(s.pt / 46, 0, 1)); t.yaw = side * (46 * u + Math.min(18, s.pt * 0.1)); t.roll = side * 9 * (1 - cl(s.pt / 80, 0, 1)); t.pit = -3; t.k = 0.2; t.solo = 1; }
      else if (w.st === 'win') { t.yaw = -w.face * (30 - Math.min(52, (s.pt - 150) * 0.22)); t.pit = -4; t.k = 0.06; t.solo = 1; }
    }
    if (s.freeze > 0 && !superFx) { t.zm = 0.84; t.fov = FOV0 + 6; t.k = 0.3; }   // the held beat of a perfect parry
    return t;
  }
  function camApply(s, sx, sy, direct) {
    const t = direct ? camDirect(s) : { yaw: Math.sin(tick * 0.0031) * 16, pit: 6, roll: 0, zm: 1, fov: FOV0, k: 0.05, solo: 1 };
    if (CAM.snap) { CAM.yaw = t.yaw; CAM.pit = t.pit; CAM.roll = t.roll; CAM.zm = t.zm; CAM.fov = t.fov; CAM.snap = 0; }
    CAM.yaw += (t.yaw - CAM.yaw) * t.k; CAM.pit += (t.pit - CAM.pit) * t.k; CAM.roll += (t.roll - CAM.roll) * t.k; CAM.zm += (t.zm - CAM.zm) * Math.max(0.1, t.k); CAM.fov += (t.fov - CAM.fov) * Math.max(0.12, t.k);
    CAM.kRoll *= 0.86; CAM.kZm *= 0.84; CAM.kFov *= 0.86; CAM.kYaw *= 0.88;
    const yaw = (CAM.yaw + CAM.kYaw) * D, pit = CAM.pit * D, fov = Math.max(48, Math.min(104, CAM.fov + CAM.kFov)), hk = Math.tan(fov * D / 2) * 16 / 9;
    let d = camW * CAM.zm * (1 + CAM.kZm) / (2 * hk);
    // swung round to the side, the near fighter rushes toward the lens: back off just enough that both stay in frame
    if (s && !t.solo) { const a = Math.abs(s.p[0].x - s.p[1].x) / F / 2 + 36 + (t.pad || 0); d = Math.max(d, a * (Math.cos(yaw) / (0.84 * hk) + Math.abs(Math.sin(yaw)))); }
    CAM.d = d;
    if (Math.abs(cam.fov - fov) > 0.01) { cam.fov = fov; cam.updateProjectionMatrix(); }
    const r = d * Math.cos(pit), tx = camX + sx, ty = camY + sy;
    cam.position.set(tx + r * Math.sin(yaw), Math.max(7, ty + d * Math.sin(pit)), r * Math.cos(yaw)); cam.up.set(0, 1, 0); cam.lookAt(tx, ty, 0); cam.rotateZ((CAM.roll + CAM.kRoll) * D);
  }
  function toScreen(wx, wy) { const v = new THREE.Vector3(wx, wy, 0).project(cam); return [(v.x + 1) * VW / 2, (1 - v.y) * VH / 2]; }

  // ----------------------------------------------------------- events -> fx
  function events(ev, s) {
    for (const e of ev) {
      const x = (e.x || 0) / F, y = (e.y || 0) / F;
      if ((e.t === 'parry' || e.t === 'pparry') && rigs[e.who]) { const rg = rigs[e.who], hy = y - s.p[e.who].y / F; rg.pt = 0; rg.pk = hy > 58 ? 'pryH' : hy < 26 ? 'pryL' : 'pryM'; rg.pn = (rg.pn || 0) + 1; }
      if (e.t === 'hit' && !e.thr) { const vi = 1 - e.who, v = s.p[vi], am0 = s.p[e.who].st === 'atk' ? SIM.CH[s.p[e.who].ch].mv[s.p[e.who].mv] : null; react[vi] = v.cb ? 'CR' : e.wrong === 'l' || (am0 && am0.hits[0] && am0.hits[0].g === 'l') ? 'LOW' : !am0 || am0.hsp <= 9 ? 'L' : am0.hsp <= 11 ? 'M' : /Kick/.test(am0.n || '') ? 'K' : 'H'; }
      if (e.t === 'hit') { const dr = s.p[e.who].face; camKick(dr * (e.ko ? 8 : e.fin ? 9 : e.lvl >= 2 ? 3 : 1.2), e.ko ? -0.14 : e.fin ? -0.16 : e.lvl >= 2 ? -0.05 : -0.02, e.ko ? 12 : e.fin ? 14 : e.lvl >= 2 ? 4 : 0, e.ko ? dr * 8 : 0); }
      if (e.t === 'hit' && e.free) { const c = CSSA[s.p[e.who].ch]; fx.push({ k: 'ripple', x, y, t: 0, c, r: 46 }); fx.push({ k: 'word', x, y: y + 44, t: 0, s: 'CHASE', c }); }
      if (e.t === 'hit' && e.fin) { const c = CSSA[s.p[e.who].ch]; fx.push({ k: 'kanji', t: 0, s: '\u6975', c }); for (let k = 0; k < 3; k++) fx.push({ k: 'ripple', x, y, t: -k * 4, c: k & 1 ? '#ffffff' : c, r: 60 + k * 30 }); shake = 30; flash = 8; if (atm) atm.flash = 9; }
      if (e.t === 'hit') { fx.push({ k: 'spark', x, y, lvl: e.lvl, t: 0, ko: e.ko, c: CSSA[s.p[e.who].ch], c2: CSSB[s.p[e.who].ch], d: s.p[e.who].face }); { const hl = hitL[hitN++ & 1]; if (hl) { hl.position.set(x, y, 34); hl.color.set(CSSA[s.p[e.who].ch]); hl.intensity = 1.8 + e.lvl * 1.1; } } if (e.di) { fx.push({ k: 'paint', x, y, t: 0, big: 1 }); fx.push({ k: 'paint', x: x + 7, y: y + 12, t: -3, big: 1 }); fx.push({ k: 'word', x, y: y + 58, t: 0, s: e.crum ? 'CRUMPLE!!' : 'DRIVE IMPACT!', c: '#ff5a3a', z: 16, life: 70 }); shake = 20; flash = 4; } const am = s.p[e.who].st === 'atk' ? SIM.CH[s.p[e.who].ch].mv[s.p[e.who].mv] : null; if (am && am.sup && e.lvl === 2 && !e.fin) { fx.push({ k: 'kanji', t: 0, s: ['', '\u65ac', '\u7834', '\u6ec5'][am.sa], c: ['', '#e9a58a', '#f1dcb4', '#d8322e'][am.sa] }); shake = 20; flash = 5; } if (e.wrong) fx.push({ k: 'word', x, y: y + 30, t: 0, s: e.wrong === 'h' ? 'OVERHEAD!' : 'LOW!', c: e.wrong === 'h' ? '#ff8a1e' : '#e9a58a' });  shake = Math.max(shake, e.ko ? 22 : [3, 7, 11][e.lvl]); if (e.lvl >= 2 || e.ko) flash = e.ko ? 6 : 2; combo[e.who] = { n: e.combo, d: e.total || e.dmg, t: 0, ch: e.ch }; }
      else if (e.t === 'block') { fx.push({ k: 'guard', x, y, t: 0 }); shake = Math.max(shake, 2); camKick(s.p[e.who].face * 0.8, -0.012, 0); }
      else if (e.t === 'parry') { fx.push({ k: 'guard', x, y, t: 0, big: 1, c: CSSA[s.p[e.who].ch] }); fx.push({ k: 'spark', x, y, lvl: 0, t: 0, c: CSSA[s.p[e.who].ch], c2: CSSB[s.p[e.who].ch], d: s.p[e.who].face }); }
      else if (e.t === 'pparry') { fx.push({ k: 'guard', x, y, t: 0, big: 1, c: CSSA[s.p[e.who].ch] }); fx.push({ k: 'ripple', x, y, t: 0, c: '#ffffff', r: 60 }); fx.push({ k: 'ring', x, y, t: 0 }); fx.push({ k: 'word', x, y: y + 40, t: 0, s: 'PERFECT PARRY', c: '#f6e7c8' }); flash = 2; }
      else if (e.t === 'armor') { fx.push({ k: 'guard', x, y, t: 0, big: 1 }); fx.push({ k: 'word', x, y: y + 34, t: 0, s: 'ARMOR', c: '#f1dcb4' }); shake = Math.max(shake, 6); }
      else if (e.t === 'splat') { camKick(x > 0 ? 6 : -6, -0.1, 8, x > 0 ? 8 : -8); fx.push({ k: 'paint', x, y, t: 0 }); fx.push({ k: 'word', x: x - 30, y: y + 50, t: 0, s: e.stun ? 'STUN!' : 'WALL SPLAT', c: '#d8322e' }); shake = 14; flash = 2; }
      else if (e.t === 'walljump') { fx.push({ k: 'dust', x, y: y + 10, t: 0, big: 1 }); fx.push({ k: 'ripple', x, y: y + 30, t: 0, c: CSSA[s.p[e.who].ch], r: 20 }); }
      else if (e.t === 'burnout') { const p = s.p[e.who]; fx.push({ k: 'spark', x: p.x / F, y: p.y / F + 44, lvl: 2, t: 0, c: '#8a8e98', c2: '#3a3a44', d: p.face }); fx.push({ k: 'spark', x: p.x / F, y: p.y / F + 20, lvl: 2, t: -4, c: '#5a5e68', c2: CSSA[p.ch], d: -p.face }); fx.push({ k: 'ripple', x: p.x / F, y: p.y / F + 40, t: 0, c: '#8a8e98', r: 70 }); shake = Math.max(shake, 10); fx.push({ k: 'word', x: p.x / F, y: 110, t: 0, s: 'BURNOUT', c: '#b8b0c0' }); }
      else if (e.t === 'rush') { const p = s.p[e.who]; fx.push({ k: 'dust', x: p.x / F, y: 0, t: 0, big: 1 }); fx.push({ k: 'word', x: p.x / F, y: 104, t: 0, s: 'GHOST STEP', c: '#f08a5a' }); }
      else if (e.t === 'di') { fx.push({ k: 'distart', who: e.who, t: 0 }); shake = Math.max(shake, 4); }
      else if (e.t === 'diclash') { camKick(7, -0.14, 12); fx.push({ k: 'paint', x, y, t: 0, big: 1 }); fx.push({ k: 'word', x, y: y + 60, t: 0, s: 'IMPACT CLASH!', c: '#fff', z: 16, life: 70 }); flash = 4; shake = 14; }
      else if (e.t === 'wbounce') { fx.push({ k: 'spark', x, y, lvl: 2, t: 0, wall: 1 }); shake = 14; flash = 2; camKick(x > 0 ? 5 : -5, -0.1, 8, x > 0 ? 7 : -7); }
      else if (e.t === 'gbounce' || e.t === 'kd') { fx.push({ k: 'dust', x, y: 0, t: 0, big: 1 }); shake = Math.max(shake, e.t === 'kd' ? 5 : 9); if (e.t === 'gbounce') camKick((tick & 1 ? 4 : -4), -0.08, 7); }
      else if (e.t === 'jump' || e.t === 'land' || e.t === 'dash' || e.t === 'run') { const p = s.p[e.who]; fx.push({ k: 'dust', x: p.x / F, y: 0, t: 0 }); }
      else if (e.t === 'tech' || e.t === 'clash') fx.push({ k: 'guard', x, y, t: 0, big: 1 });
      else if (e.t === 'pend') fx.push({ k: 'ring', x, y, t: 0 });
      else if (e.t === 'super') { superFx = { who: e.who, t: 0, lv: e.lv, ca: e.ca ? 1 : 0, name: SIM.CH[s.p[e.who].ch].mv[e.k || 'SA' + e.lv].n }; if (e.ca) { flash = 6; shake = 12; } }
      else if (e.t === 'home') { camKick(0, 0, 9); const c = CSSA[s.p[e.who].ch]; fx.push({ k: 'ripple', x, y: y + 36, t: 0, c, r: 34 }); fx.push({ k: 'ripple', x, y: y + 36, t: -4, c: '#ffffff', r: 22 }); fx.push({ k: 'dust', x, y: 0, t: 0, big: 1 }); shake = Math.max(shake, 3); }
      else if (e.t === 'ex') fx.push({ k: 'exflash', who: e.who, t: 0 });
      else if (e.t === 'round') msg = { big: s.round >= s.cfg.rounds * 2 - 1 && s.p[0].wins === s.p[1].wins && s.round > 1 ? 'FINAL ROUND' : 'ROUND ' + s.round, jp: '第' + '一二三四五'[s.round - 1] + '回戦', t: 0, dur: 80, col: '#f1dcb4' };
      else if (e.t === 'fight') msg = { big: 'FIGHT!', jp: '始め！', t: 0, dur: 50, col: '#d8322e', slam: 1 };
      else if (e.t === 'ko') msg = e.type === 'time' ? { big: 'TIME OVER', jp: '時間切れ', t: 0, dur: 150, col: '#f1dcb4' } : e.type === 'draw' || e.type === 'dko' ? { big: e.type === 'dko' ? 'DOUBLE K.O.' : 'DRAW', jp: '引き分け', t: 0, dur: 150, col: '#f1dcb4' } : { big: 'K.O.', jp: e.type === 'perfect' ? '完全勝利 PERFECT' : '一本！', t: -34, dur: 190, col: '#d8322e', slam: 1 };
    }
  }

  // ----------------------------------------------------------- 3D frame
  function draw3d(s, menuPan) {
    tick++; atmStep(s);
    const L = stage.userData.lamps; for (let i = 0; i < L.length; i++) { const fk = Math.sin(tick * 0.13 + i * 2) + 0.5 * Math.sin(tick * 0.37 + i); L[i].material.color.setHSL(0.07, 1, 0.55 + 0.06 * fk); if (stage.userData.lampL) stage.userData.lampL[i].intensity = 0; }
    const LV = stage.userData.leaves; for (let i = 0; i < LV.length; i++) { const m = LV[i], u = m.userData; u.y -= u.v; u.x += WIND * (0.9 + u.v); if (u.y < 0) { u.y = 250 + (i % 7) * 6; u.x = camX + ((i * 137) % 900) - 380; } m.position.set(u.x + Math.sin(tick * 0.03 + u.p) * 14, u.y, u.z); m.rotation.set(tick * 0.04 + u.p, tick * 0.05 * u.v, tick * 0.03 + u.p); }
    let tx = 0, tw = 440, ty = 72;
    if (s) {
      const a = s.p[0], b = s.p[1], ax = a.x / F, bx = b.x / F;
      const hy = Math.max(a.y, b.y) / F; tx = (ax + bx) / 2; tw = Math.max(236, Math.min(480, Math.abs(ax - bx) + 150), Math.min(600, (hy + 108) * 2.09)); ty = tw * 9 / 32 - 17;   // wide enough for the highest fighter's head to clear the bars, floor always in frame   // high juggles: pull back far enough to keep the floor and the top fighter in frame
      if (superFx && superFx.t < (superFx.ca ? 80 : 40)) { const p = s.p[superFx.who]; tx = p.x / F; tw = superFx.ca ? 226 : 240 - (superFx.lv || 1) * 6; ty = 54; }
      if (s.phase === 'ko' && s.pt < 150 && s.rw >= 0) { const l = s.p[1 - s.rw]; tx = l.x / F; tw = 214; ty = 40 + l.y / F * 0.6; }
      else if (s.phase === 'ko' && s.rw >= 0 && s.p[s.rw].st === 'win') { const w = s.p[s.rw]; tx = w.x / F; tw = 250; ty = 58; }
      if (s.slow > 0 && s.phase === 'fight') { tw = Math.min(tw, 250); }
      if (snapCam) { camX = Math.max(-(SIM.STAGE / F + 30 - tw / 2), Math.min(SIM.STAGE / F + 30 - tw / 2, tx)); camW = tw; camY = ty; snapCam = false; }
      const lim = SIM.STAGE / F + 30 - tw / 2; tx = Math.max(-lim, Math.min(lim, tx));
      camX += (tx - camX) * 0.22; camW += (tw - camW) * 0.14; camY += (ty - camY) * 0.2;
      const dim = s.freeze > 0; stage.visible = !dim; scene.background.setHex(dim ? 0x07040a : 0x16111b);
      for (let i = 0; i < 2; i++) {
        const p = s.p[i], o = s.p[1 - i], r = rigs[i]; r.G.visible = true; if (r.T) r.T.visible = true;
        const fp = fighterPose(p, o, s.frame, i); if (typeof window !== 'undefined' && window.__bg && window.__bg.pose && window.__bg.show === i) { if (window.__bg.pose === 'bind') fp.bindPose = 1; else fp.po = norm(PZ[window.__bg.pose], ST); fp.loose = 0; fp.yaw = 0; fp.straight = 1; }
        let jx = 0; if (p.hs > 0 && (p.st === 'hit' || p.st === 'ahit' || p.st === 'block')) jx = ((p.hs & 1) ? 1 : -1) * Math.min(3, p.hs * 0.5);
        const fl = (p.hs > 0 && (p.st === 'hit' || p.st === 'ahit')) ? Math.min(0.9, p.hs * 0.14) : 0;
        const m = p.st === 'atk' ? SIM.CH[p.ch].mv[p.mv] : null; let tint = [0, 0, 0];
        if (m && (m.od || m.sup || m.tint || m.fire)) { const k = 0.25 + 0.2 * Math.sin(s.frame * 0.9); tint = m.sup ? [k, k * 0.6, 0] : [k, k, 0]; }
        if (m && m.di) { const a = s.frame * 0.7; tint = [0.35 + 0.3 * Math.sin(a), 0.35 + 0.3 * Math.sin(a + 2.1), 0.35 + 0.3 * Math.sin(a + 4.2)]; }
        if (m && !m.isAir && m.hits[0] && m.hits[0].g === 'h' && p.mf < m.hits[0].s) { const k = 0.3 + 0.3 * Math.sin(s.frame * 1.2); tint = [k + 0.2, k * 0.4, 0]; }
        if (p.st === 'drush' || (p.st === 'atk' && p.drb)) tint = [0, 0.45, 0.18];
        r.mode = p.st === 'parry' ? 'parry' : m && m.home && !p.hmE && !(p.contact && !p.fr) ? 'home' : '';
        r.echo = r.mode === 'home' || p.st === 'drush' ? 2 : (p.st === 'dashf' || p.st === 'dashb' || p.st === 'run' || fp.echo || Math.abs(r.vel || 0) > 3) ? 1 : 0;
        r.phase = p.st === 'dashb' && p.sf < 11 ? (p.sf > 2 && p.sf < 6 ? 0.65 : 0.45) : p.st === 'dashf' && p.sf < 4 ? 0.45 : (p.st === 'getup' && p.sf < 6) ? 0.5 : 0;
        // round start: he comes in out of nothing, seated in the air, and unfolds into his stance
        if (s.phase === 'intro') { const u = Math.min(1, s.pt / (SIM.INTRO || 90)); fp.po = mix(P('win'), fp.po, sm((u - 0.45) / 0.45)); fp.loose = 1; r.phase = u < 0.16 ? 0.85 : u < 0.3 ? 0.65 : u < 0.46 ? 0.45 : 0; r.echo = 0; }
        if (r.mode === 'parry') { const a = ACC[p.ch], k = 0.16 + 0.12 * Math.sin(s.frame * 0.8); tint = [a[0] * k, a[1] * k, a[2] * k]; }
        if (r.mode === 'home') { const a = ACC[p.ch], w = p.mf >= m.home[0] ? 0.04 + 0.05 * Math.sin(s.frame * 1.6) : 0, k = 0.06 + 0.1 * Math.min(1, p.mf / m.home[0]); tint = [a[0] * k + w, a[1] * k + w, a[2] * k + w]; }
        if (p.bo && (s.frame >> 3) & 1) tint = [tint[0] - 0.18, tint[1] - 0.18, tint[2] - 0.12];
        if (s.freeze > 0 && s.frzWho === i) { const a = ACC[p.ch]; tint = [a[0] * 0.3 + 0.06, a[1] * 0.3 + 0.06, a[2] * 0.3 + 0.06]; }
        applyPose(r, fp, p.face, fl, tint);
        r.G.position.set(p.x / F + jx, p.y / F, i === 0 ? 2 : -2);
        { const o = s.p[1 - i], m0 = p.st === 'atk' && p.mv === 'throw' ? SIM.CH[p.ch].mv.throw : null; r.grab = p.st === 'throwing' && o.st === 'thrown' ? { x: o.x / F, y: o.y / F + 50, wrap: 1, sq: p.pmT > 0 ? Math.sin(p.pmT / 9 * Math.PI) : 0 } : m0 && p.mf >= 1 && p.mf <= m0.su + 7 ? { x: p.x / F + p.face * (10 + 46 * Math.min(1, p.mf / m0.su)), y: p.y / F + 54, wrap: 0, sq: 0 } : null; }
        trailStep(r, p.face);
        { r.pt = r.pt == null ? 99 : r.pt + 1; r.boT = p.bo ? 1 : 0; const gx = p.x / F, vx0 = gx - (r.lx == null ? gx : r.lx); r.lx = gx; r.vel = (r.vel || 0) * 0.7 + vx0 * 0.3;
          if (m && m.home && p.mf >= m.home[0] && !p.contact && !p.hmE && p.hs <= 0) { if (!r.skin) fx.push({ k: 'echo', x: gx - vx0, y: p.y / F + 30, t: 0, c: CSSA[p.ch], c2: CSSB[p.ch] }); if (tick % 3 === 0) fx.push({ k: 'ripple', x: gx - vx0 * 2, y: p.y / F + 34, t: 0, c: CSSA[p.ch], r: 12 }); }
          if (Math.abs(vx0) > 2.2 && (tick & 1) && !r.skin) fx.push({ k: 'echo', x: gx - vx0 * 2, y: p.y / F + 34, t: 0, c: CSSB[p.ch], c2: CSSA[p.ch] });
          if (p.st === 'parry' && tick % 6 === 0) fx.push({ k: 'ripple', x: gx + p.face * 8, y: p.y / F + 38, t: 0, c: CSSA[p.ch], r: 30 });
          if (p.st === 'parry' && tick % 3 === 0) fx.push({ k: 'echo', x: gx + (hsh(tick * 7 + i) - 0.5) * 16, y: p.y / F + 36, t: 0, c: CSSA[p.ch], c2: '#ffffff' });
          if (p.bo && tick % 14 === 0 && p.st !== 'kd') fx.push({ k: 'ripple', x: gx + (hsh(tick + i * 3) - 0.5) * 20, y: p.y / F + 30 + hsh(tick * 3 + i) * 40, t: 0, c: '#8a8e98', r: 9 }); }
        shadows[i].visible = true; shadows[i].position.x = p.x / F; const sc = Math.max(0.45, 1 - p.y / F / 220); shadows[i].scale.set(sc * (p.st === 'kd' ? 1.8 : 1), sc, 1);
      }
      // projectiles
      while (projMeshes.length < s.proj.length) { const g = new THREE.Group(); const c1 = new THREE.Mesh(new THREE.IcosahedronGeometry(9, 0), new THREE.MeshBasicMaterial({ color: 0xffffff })); const c2 = new THREE.Mesh(new THREE.IcosahedronGeometry(15, 0), new THREE.MeshBasicMaterial({ color: 0x33d8ff, transparent: true, opacity: 0.6 })); const c3 = new THREE.Mesh(new THREE.IcosahedronGeometry(20, 0), new THREE.MeshBasicMaterial({ color: 0x2060ff, transparent: true, opacity: 0.3 })); g.add(c1, c2, c3); scene.add(g); projMeshes.push(g); }
      projMeshes.forEach((g, i) => { const q = s.proj[i]; g.visible = !!q; if (!q) return; const pr = SIM.CH[q.ch].mv[q.k].proj; g.position.set(q.x / F, q.y / F, 0); const k = pr.big ? 1.7 : 1, w = 1 + 0.15 * Math.sin(q.life * 1.3); g.scale.set(k * w * (pr.big ? 2.2 : 1.25), k * w, k * w); g.rotation.x = q.life * 0.5; g.children[1].material.color.setHex(pr.big ? 0xffb07a : q.ch === 0 ? 0xff5a3a : 0xf3d9a8); g.children[2].material.color.setHex(q.ch === 0 || pr.big ? 0xb81c24 : 0xa8763c); });
    } else {
      camX = Math.sin(tick * 0.004) * 200; camW = 400; camY = 84;
      for (const r of rigs) { r.G.visible = !!menuPan; if (r.T) r.T.visible = !!menuPan; } for (const sh of shadows) sh.visible = !!menuPan; projMeshes.forEach(g => g.visible = false); stage.visible = true; scene.background.setHex(0x16111b);
      if (menuPan) { camX = 0; camW = menuPan.w || 340; camY = menuPan.y || 34; const px = menuPan.x || 75; for (let i = 0; i < 2; i++) { const fake = { ch: menuPan.chars[i], st: menuPan.pose && menuPan.pose[i] ? 'win' : 'idle', sub: 'stand', sf: 30, subf: 0, y: 0, vy: 0 }; const fp = fighterPose(fake, fake, tick); applyPose(rigs[i], fp, i ? -1 : 1, 0, [0, 0, 0]); rigs[i].G.position.set(i ? px : -px, 0, i ? -2 : 2); trailStep(rigs[i], i ? -1 : 1); shadows[i].position.x = i ? px : -px; shadows[i].scale.set(1, 1, 1); } }
    }
    let sx = 0, sy = 0; if (shake > 0) { sx = ((shake * 7919) % 5 - 2) * Math.min(3, shake * 0.35); sy = ((shake * 104729) % 5 - 2) * Math.min(3, shake * 0.35); shake--; }
    // photo mode for scenery shots: window.__bg = { cam: [x, y, z, lookX, lookY, lookZ, fov, roll] } hides the fighters and takes the camera
    const bgm = typeof window !== 'undefined' && window.__bg;
    if (bgm) { for (const r of rigs) { r.G.visible = false; if (r.T) r.T.visible = false; } for (const sh of shadows) sh.visible = false; projMeshes.forEach(g => g.visible = false); fx.length = 0;
      if (bgm.show != null && rigs[bgm.show]) { rigs[bgm.show].G.visible = true; if (rigs[bgm.show].T) rigs[bgm.show].T.visible = true; } if (bgm.plain) { stage.visible = false; scene.background.setHex(bgm.plain); } }
    camApply(s, sx, sy, !!s);
    if (bgm && bgm.cam) { const c = bgm.cam; cam.fov = c[6] || FOV0; cam.updateProjectionMatrix(); cam.position.set(c[0], c[1], c[2]); cam.up.set(0, 1, 0); cam.lookAt(c[3], c[4], c[5]); if (c[7]) cam.rotateZ(c[7] * D); }
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
      if (p.bo) txt('LOW TIDE', flip ? 118 : 362, 38, 8, (s.frame >> 3) & 1 ? '#d8322e' : '#9a8488', flip ? 'right' : 'left');
      // super art gauge: three stocks
      const mx = i ? 338 : 22, mw = 120, lv = Math.floor(p.sa / 1000);
      rect(mx - 2, 252, mw + 4, 10, '#120b10'); rect(mx - 1, 253, mw + 2, 8, '#f4f0e0'); rect(mx, 254, mw, 6, '#1c1418');
      const fw = Math.round(mw * p.sa / 3000); rect(i ? mx + mw - fw : mx, 254, fw, 6, lv >= 3 ? ((s.frame >> 2) & 1 ? '#fff' : '#f1dcb4') : lv >= 1 ? '#ffb020' : '#b8463c'); rect(mx + 39, 254, 2, 6, '#120b10'); rect(mx + 79, 254, 2, 6, '#120b10');
      const caOn = lv >= 3 && p.hp * 4 <= C.hp && p.hp > 0;
      if (caOn) { const pu = 0.5 + 0.5 * Math.sin(s.frame * 0.3); g2.globalAlpha = 0.35 + 0.45 * pu; rect(i ? mx + mw - fw : mx, 254, fw, 6, CSSA[p.ch]); g2.globalAlpha = 1; rect(mx - 2, 250, mw + 4, 1, CSSA[p.ch]); rect(mx - 2, 263, mw + 4, 1, CSSA[p.ch]); txt('FT', i ? mx - 6 : mx + mw + 6, 253, 8, (s.frame >> 2) & 1 ? '#ffffff' : CSSA[p.ch], i ? 'right' : 'left'); txt('\u5965\u7fa9', i ? mx - 26 : mx + mw + 26, 250, 12, CSSA[p.ch], i ? 'right' : 'left', 1); }
      else txt('TA' + lv, i ? mx - 6 : mx + mw + 6, 253, 8, lv >= 3 ? '#f1dcb4' : lv ? '#ffb020' : '#5a4448', i ? 'right' : 'left');
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
      const i = superFx.who, lv = superFx.lv || 1, dur = superFx.ca ? 84 : [0, 40, 44, 60][lv]; let t = superFx.t++;
      // the cinematic runs on the sim's own freeze clock, so it can never outlast or trail the pause it belongs to
      if (s.freeze > 0 && s.frzWho === i && s.p[i].st === 'atk') t = superFx.t = Math.max(0, dur - s.freeze); else if (t < dur) t = superFx.t = dur;
      const col = CSSA[s.p[i].ch];
      if (superFx.ca && t === dur) flash = Math.max(flash, 4);
      if (superFx.ca && t < dur) {
        // Critical Art: the world goes out, the fighter's own colour floods in, and the name is carved across the screen
        const p = s.p[i], q = toScreen(p.x / F, p.y / F + 55), dir = i ? -1 : 1, ac = CSSA[p.ch], hi = CSSH[p.ch], u = t / dur;
        g2.fillStyle = 'rgba(4,2,8,' + Math.min(0.22, t * 0.05) + ')'; g2.fillRect(0, 0, VW, VH);
        // falling threads of the fighter's substance
        g2.strokeStyle = ac; g2.lineWidth = 1; for (let k = 0; k < 46; k++) { const hx = hsh(k * 17 + 3) * VW, sp = 3 + hsh(k * 5 + 1) * 7, yy = ((hsh(k * 29 + 7) * VH + t * sp * 2.2) % (VH + 60)) - 30, ln = 10 + sp * 5; g2.globalAlpha = 0.18 + 0.4 * hsh(k * 3 + 2); g2.beginPath(); g2.moveTo(hx + (yy - 135) * 0.06 * dir, yy); g2.lineTo(hx + (yy + ln - 135) * 0.06 * dir, yy + ln); g2.stroke(); }
        // pulse rings breathing out of the body
        for (let k = 0; k < 4; k++) { const rr = ((t * 5 + k * 55) % 220) + 52; g2.globalAlpha = Math.max(0, 0.7 - rr / 300); g2.strokeStyle = k & 1 ? '#ffffff' : ac; g2.lineWidth = 2.5 - rr / 120; g2.beginPath(); g2.ellipse(q[0], q[1], rr * 0.7, rr, 0, 0, 7); g2.stroke(); }
        g2.globalAlpha = 0.55; burst(q[0], q[1], 78 + (t % 5) * 6, 520, 40, ac, t * 0.035 * dir, 1.5); g2.globalAlpha = 1;
        // the two characters slam in one after the other on the far side from the fighter
        const kx = i ? 110 : 370, k1 = Math.min(1, t / 6), k2 = Math.max(0, Math.min(1, (t - 10) / 6)), out = t > dur - 8 ? (dur - t) / 8 : 1;
        g2.globalAlpha = Math.max(0, out) * 0.16; txt('\u5965\u7fa9', kx + Math.sin(t * 0.05) * 6, 6, 168, ac, 'center', 1, 0); g2.globalAlpha = Math.max(0, out);
        if (k1 > 0) { g2.save(); g2.translate(kx - 44, 96); g2.scale(1 + (1 - k1) * 2.4, 1 + (1 - k1) * 2.4); g2.globalAlpha = k1 * Math.max(0, out); txt('\u5965', 0, -48, 96, '#ffffff', 'center', 1, ac); g2.restore(); }
        if (k2 > 0) { g2.save(); g2.translate(kx + 44, 128); g2.scale(1 + (1 - k2) * 2.4, 1 + (1 - k2) * 2.4); g2.globalAlpha = k2 * Math.max(0, out); txt('\u7fa9', 0, -48, 96, ac, 'center', 1, '#ffffff'); g2.restore(); }
        g2.globalAlpha = 1;
        if (t === 6 || t === 16) { shake = Math.max(shake, 10); flash = Math.max(flash, 2); }
        const bh = 42, sl = Math.min(1, t / 5); rect(0, 0, VW, bh * sl, '#06040a'); rect(0, VH - bh * sl, VW, bh * sl, '#06040a'); rect(0, bh * sl, VW, 2, ac); rect(0, bh * sl + 3, VW, 1, hi); rect(0, VH - bh * sl - 2, VW, 2, ac); rect(0, VH - bh * sl - 4, VW, 1, hi);
        const sx = Math.min(1, Math.max(0, (t - 14) / 8)), bx = i ? 480 - 320 * sx : -320 + 320 * sx; rect(bx, 180, 320, 44, 'rgba(6,4,10,0.92)'); rect(bx, 180, 320, 2, ac); rect(bx, 222, 320, 2, ac); rect(i ? bx : bx + 316, 180, 4, 44, hi);
        txt('FINAL TIDE', bx + 160, 185, 16, (t >> 2) & 1 ? '#ffffff' : ac, 'center'); txt(superFx.name.toUpperCase(), bx + 160, 206, 8, hi, 'center');
        for (let k = 0; k < 3; k++) rect(bx + 132 + k * 20, 216, 16, 3, ac);
      } else if (t < dur + 6) {
        const p = s.p[i], q = toScreen(p.x / F, p.y / F + 55), dir = i ? -1 : 1;
        if (lv >= 2) { g2.fillStyle = 'rgba(6,4,12,' + (lv === 3 ? 0.26 + 0.08 * Math.sin(t * 0.8) : 0.2) + ')'; g2.fillRect(0, 0, VW, VH); }
        g2.globalAlpha = 0.6; burst(q[0], q[1], 84 + (t % 6) * 6, 520, 24 + lv * 10, col, t * 0.05 * dir, 2); if (t < 10) { g2.globalAlpha = 0.8 * (1 - t / 10); burst(q[0], q[1], 30, 150 - t * 8, 12, '#fff', -t * 0.15, 3); } g2.globalAlpha = 0.7;
        for (let k = 0; k < lv; k++) { g2.strokeStyle = k % 2 ? '#fff' : col; g2.lineWidth = 3; g2.beginPath(); g2.arc(q[0], q[1], ((t * 7 + k * 60) % 190) + 70, 0, 7); g2.stroke(); }
        g2.globalAlpha = 1;
        const bh = lv === 3 ? 36 : lv === 2 ? 22 : 10, sl = Math.min(1, t / 6); rect(0, 0, VW, bh * sl, '#120b10'); rect(0, VH - bh * sl, VW, bh * sl, '#120b10'); rect(0, bh * sl, VW, 2, col); rect(0, VH - bh * sl - 2, VW, 2, col);
        if (lv === 3) { g2.globalAlpha = 0.4; for (let k = 0; k < 4; k++) txt('\u6fc0\u7a81', ((k * 190 + t * 9 * dir) % 760 + 760) % 760 - 150, 76, 96, k % 2 ? col : '#ffffff', 'left', 1, 0); g2.globalAlpha = 1; }
        const sx = Math.min(1, t / 8), bx = i ? 480 - 300 * sx : -300 + 300 * sx; rect(bx, 186, 300, 36, 'rgba(10,6,18,0.9)'); rect(bx, 186, 300, 2, col); rect(bx, 220, 300, 2, col);
        txt(superFx.name.toUpperCase(), bx + 150, 192, 8, '#fff', 'center'); txt('\u8d85\u5fc5\u6bba  TIDE ART', bx + 132, 202, 16, col, 'center', 1); for (let k = 0; k < 3; k++) rect(bx + 232 + k * 14, 206, 10, 10, k < lv ? col : '#3a2228');
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
  return { snap() { snapCam = true; CAM.snap = 1; }, poseKeys() { return Object.keys(PZ); }, init, setFighters, events, draw3d, clear, drawFx, hud, boxes, txt, rect, burst, toScreen, get g() { return g2; }, get tick() { return tick; }, VW, VH, hsh, say(m) { msg = m; }, FJ, FP };
})();
