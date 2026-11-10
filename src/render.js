// ===================================================================
// Presentation: low-res 3D fighters on a 2D gameplay plane, 2D fx + HUD.
// Reads sim state only; never writes it.
// ===================================================================
const R = (function () {
  'use strict';
  const D = Math.PI / 180, F = SIM.F, VW = 480, VH = 270;
  let ren, scene, cam, g2, stage, rigs = [], shadows = [], projMeshes = [], fx = [], shake = 0, flash = 0, camX = 0, camW = 440, camY = 70, tick = 0;
  const combo = [null, null]; let msg = null, superFx = null; const FJ = "'DotGothic16','Press Start 2P',monospace", FP = "'Press Start 2P','DotGothic16',monospace";

  // ----------------------------------------------------------- poses (world angles, deg)
  const base = { l: 6, t: 10, h: 0, af: [55, 80], ab: [20, 115], lf: [30, -35], lb: [-22, -20], yaw: 0, rot: 0 };
  const cr = { l: 14, t: 24, h: -4, af: [60, 85], ab: [30, 110], lf: [85, -120], lb: [25, -115] };
  const jp = { l: 5, t: 8, af: [40, 70], ab: [-20, 60], lf: [75, -95], lb: [25, -70] };
  const PO = {
    idle: {}, idle2: { t: 13, af: [51, 86], ab: [17, 119], lf: [33, -42], lb: [-25, -26] },
    walkA: { lf: [42, -28], lb: [-32, -12] }, walkB: { lf: [-6, -22], lb: [18, -48] },
    crouch: cr, prejump: { l: 10, t: 15, lf: [55, -80], lb: [10, -70], af: [20, 40], ab: [-10, 40] },
    jump: jp, fall: { l: 2, t: 4, af: [110, 40], ab: [80, 50], lf: [35, -40], lb: [-8, -30] },
    dashf: { l: 28, t: 14, h: 6, af: [-40, 70], ab: [70, 80], lf: [70, -50], lb: [-50, -30] },
    dashb: { l: -14, t: -6, af: [60, 60], ab: [40, 80], lf: [40, -10], lb: [-30, -50] },
    blockS: { l: -5, t: -4, h: 8, af: [75, 100], ab: [65, 110], lf: [25, -30], lb: [-25, -22] },
    blockC: Object.assign({}, cr, { t: 14, af: [80, 100], ab: [70, 110] }),
    hitHi: { l: -16, t: -22, h: -30, af: [10, 40], ab: [-30, 50], lf: [35, -25], lb: [-8, -30] },
    hitLo: { l: 18, t: 34, h: 25, af: [40, 60], ab: [30, 70], lf: [25, -40], lb: [-20, -35] },
    hitCr: Object.assign({}, cr, { l: 0, t: -5, h: -25, af: [20, 40], ab: [-10, 50] }),
    ahit: { l: -20, t: -15, h: -25, af: [60, 30], ab: [90, 30], lf: [50, -30], lb: [20, -55] },
    kd: { rot: -90, l: 0, t: 0, h: -5, af: [30, 10], ab: [10, 10], lf: [8, -5], lb: [-6, -12] },
    win: { l: -4, t: -6, h: -12, af: [175, 8], ab: [25, 40], lf: [8, -8], lb: [-8, -6], yaw: 20 },
    thrown: { l: -10, t: -10, h: -20, af: [40, 10], ab: [30, 10], lf: [10, -10], lb: [-10, -10] },
    grabW: { l: 10, t: 10, af: [70, 30], ab: [60, 35] }, grabS: { l: 16, t: 16, af: [88, 10], ab: [82, 12], lf: [45, -45], lb: [-35, -10] },
    lift: { l: -10, t: -15, af: [130, 20], ab: [125, 20] }, slam: { l: 25, t: 30, af: [40, 10], ab: [35, 10], lf: [45, -50], lb: [-30, -10] },
    mpW: { t: 2, af: [30, 110], yaw: 14 }, mpS: { l: 12, t: 22, af: [78, 6], ab: [28, 112], lf: [38, -42], lb: [-30, -10], yaw: -26 },
    cmkS: Object.assign({}, cr, { l: -14, t: 16, lf: [56, -1], lb: [34, -120], ab: [-36, 26], af: [40, 80] }),
    axW: { l: -8, t: -6, lf: [150, -14], lb: [-6, -10], af: [30, 60], ab: [-30, 60] }, axS: { l: 14, t: 18, lf: [64, -3], lb: [-22, -14], af: [10, 60], ab: [-50, 40] },
    diW: { l: -14, t: -22, h: -6, af: [70, 60], ab: [-60, 120], lf: [34, -40], lb: [-30, -26], yaw: 50 }, diS: { l: 28, t: 36, h: 8, af: [-20, 100], ab: [94, 2], lf: [52, -56], lb: [-48, -4], yaw: -55 },
    parry: { l: 0, t: -2, h: 0, af: [64, 62], ab: [58, 70], lf: [28, -34], lb: [-24, -22], yaw: 30 },
    crumA: { l: 26, t: 44, h: 34, af: [30, 30], ab: [20, 30], lf: [40, -70], lb: [-10, -60] }, crumB: { l: 44, t: 60, h: 40, af: [50, 20], ab: [40, 20], lf: [70, -120], lb: [20, -110] },
    splat: { l: -10, t: -14, h: -30, af: [-70, 20], ab: [-100, 20], lf: [14, -10], lb: [-10, -14] },
    jabW: { t: 6, af: [40, 125] }, jabS: { l: 8, t: 16, af: [90, 2], ab: [20, 120], lf: [35, -38], lb: [-26, -14], yaw: -12 },
    strW: { l: 2, t: 0, af: [60, 70], ab: [-25, 125], yaw: 24 }, strS: { l: 14, t: 26, h: 6, af: [20, 120], ab: [92, 2], lf: [42, -48], lb: [-36, -6], yaw: -40 },
    lkW: { l: -4, t: 4, lf: [70, -95], lb: [-12, -18] }, lkS: { l: -12, t: 2, lf: [70, -4], lb: [-10, -16], af: [40, 90], ab: [-10, 100] },
    hkW: { l: -6, t: 0, lb: [55, -115], lf: [10, -20], yaw: 35, af: [70, 70] }, hkS: { l: -28, t: -8, h: 10, lb: [112, -4], lf: [4, -10], af: [30, 60], ab: [-40, 40], yaw: -75 },
    cjabW: Object.assign({}, cr, { af: [50, 120] }), cjabS: Object.assign({}, cr, { t: 28, af: [88, 4] }),
    clkW: Object.assign({}, cr, { lf: [95, -100] }), clkS: Object.assign({}, cr, { l: -6, t: 20, lf: [58, -2], lb: [30, -118], ab: [-30, 30] }),
    cupW: Object.assign({}, cr, { t: 30, ab: [-30, 95], yaw: 20 }), cupS: { l: -4, t: -10, h: -12, ab: [168, 12], af: [10, 80], lf: [25, -25], lb: [-15, -12], yaw: -30 },
    swW: Object.assign({}, cr, { yaw: 30 }), swS: { l: -30, t: -5, lb: [58, -2], lf: [70, -125], af: [-40, 20], ab: [20, 90], yaw: -70 },
    ohW: { l: -4, t: -10, ab: [185, 25], af: [60, 80], yaw: 15 }, ohS: { l: 20, t: 30, h: 10, ab: [62, 6], af: [10, 100], lf: [45, -50], lb: [-35, -8], yaw: -25 },
    jlpS: Object.assign({}, jp, { t: 16, af: [62, 5] }),
    jhpW: Object.assign({}, jp, { ab: [190, 30], af: [150, 30] }), jhpS: Object.assign({}, jp, { l: 15, t: 25, ab: [55, 5], af: [50, 10] }),
    jlkS: { l: 5, t: 8, lf: [85, -100], lb: [-10, -30], af: [40, 70], ab: [-20, 60] },
    jhkW: { l: -10, lb: [70, -110], lf: [20, -60], af: [80, 40], ab: [0, 40] }, jhkS: { l: -25, t: -5, lb: [62, -2], lf: [40, -100], af: [120, 20], ab: [-60, 10], yaw: -40 },
    fbW: { l: -6, t: -8, af: [-25, 75], ab: [-35, 65], lf: [20, -30], lb: [-28, -25], yaw: 25 }, fbS: { l: 16, t: 22, h: 4, af: [86, 6], ab: [80, 10], lf: [48, -50], lb: [-40, -6], yaw: -10 },
    srS: { l: -6, t: -12, h: -18, ab: [172, 10], af: [-10, 90], lf: [80, -105], lb: [-5, -8], yaw: -20 }, srS2: { l: -4, t: -10, h: -20, ab: [178, 4], af: [-20, 60], lf: [60, -80], lb: [-8, -10], yaw: -20 },
    spW: { l: -5, lb: [40, -100], yaw: 40, af: [80, 60], ab: [60, 60] }, spS: { l: -18, t: -4, lb: [98, -2], lf: [-4, -8], af: [100, 20], ab: [-70, 20] },
    elW: { l: 22, t: 10, af: [30, 130], ab: [-30, 90], lf: [60, -55], lb: [-40, -25] }, elS: { l: 32, t: 20, h: 4, af: [84, 140], ab: [-50, 40], lf: [55, -55], lb: [-52, -4], yaw: -30 },
    knS: { l: -8, t: -6, lf: [112, -125], lb: [-6, -6], af: [150, 60], ab: [-30, 70] },
    hmW: { l: -10, t: -18, h: -10, af: [195, 20], ab: [190, 25], lf: [20, -25], lb: [-20, -20] }, hmS: { l: 30, t: 38, h: 20, af: [48, 6], ab: [42, 8], lf: [50, -60], lb: [-38, -10] },
    dkS: { l: -30, t: -5, lf: [48, -2], lb: [85, -125], af: [150, 30], ab: [-50, 30] }
  };
  function P(n) { const o = PO[n] || PO.idle, b = base; return { l: o.l ?? b.l, t: o.t ?? b.t, h: o.h ?? b.h, af: o.af || b.af, ab: o.ab || b.ab, lf: o.lf || b.lf, lb: o.lb || b.lb, yaw: o.yaw ?? 0, rot: o.rot ?? 0 }; }
  function mix(a, b, k) { const m = (x, y) => x + (y - x) * k, m2 = (x, y) => [m(x[0], y[0]), m(x[1], y[1])]; return { l: m(a.l, b.l), t: m(a.t, b.t), h: m(a.h, b.h), af: m2(a.af, b.af), ab: m2(a.ab, b.ab), lf: m2(a.lf, b.lf), lb: m2(a.lb, b.lb), yaw: m(a.yaw, b.yaw), rot: m(a.rot, b.rot) }; }
  const sm = k => k <= 0 ? 0 : k >= 1 ? 1 : k * k * (3 - 2 * k);
  function keyed(k, f) { if (f <= k[0][0]) return P(k[0][1]); for (let i = 1; i < k.length; i++) if (f <= k[i][0]) { const a = k[i - 1], b = k[i]; let u = (f - a[0]) / (b[0] - a[0]); return mix(P(a[1]), P(b[1]), u * u * (3 - 2 * u)); } return P(k[k.length - 1][1]); }

  // pose for a fighter from sim state -> {pose, grounded, extraYaw}
  function fighterPose(p, o, sFrame) {
    const C = SIM.CH[p.ch]; let po, ground = p.y <= 0, spin = 0, dy = 0, dx = 0;
    switch (p.st) {
      case 'idle':
        if (p.sub === 'crouch') po = mix(P('idle'), P('crouch'), sm(Math.min(1, (p.subf + 1) / 4)));
        else if (p.sub === 'walkf' || p.sub === 'walkb') { const k = (Math.sin(p.subf * 0.33) + 1) / 2; po = mix(P('walkA'), P('walkB'), k); if (p.sub === 'walkb') po.l = -2; }
        else po = mix(P('idle'), P('idle2'), (Math.sin(sFrame * 0.11 + p.ch) + 1) / 2);
        break;
      case 'prejump': case 'land': po = P('prejump'); break;
      case 'jump': po = mix(P('jump'), P('fall'), sm((500 - p.vy) / 1200)); break;
      case 'dashf': po = mix(P('dashf'), P('idle'), sm((p.sf - 11) / 6)); break;
      case 'drush': po = mix(P('dashf'), P('idle'), sm((p.sf - 26) / 8)); break;
      case 'parry': po = P('parry'); break; case 'prec': po = mix(P('parry'), P('idle'), sm(p.sf / 11)); break;
      case 'dashb': po = mix(P('dashb'), P('idle'), sm((p.sf - 14) / 8)); dy = Math.max(0, Math.sin(Math.min(1, p.sf / 14) * Math.PI) * 10); ground = dy <= 0; break;
      case 'block': po = P(p.cb ? 'blockC' : 'blockS'); break;
      case 'hit': { if (p.crum === 1) { po = mix(P('crumA'), P('crumB'), sm(p.sf / 60)); break; } if (p.crum === 2) { po = P('splat'); break; } const h = P(p.cb ? 'hitCr' : p.hitHi ? 'hitHi' : 'hitLo'); po = mix(h, P(p.cb ? 'crouch' : 'idle'), sm(1 - p.stun / 8)); break; }
      case 'ahit': po = P('ahit'); po.rot = -40 - Math.max(0, Math.min(75, (700 - p.vy) / 18)); ground = false; break;
      case 'kd': po = P('kd'); ground = false; dy = -44; break;
      case 'getup': { const u = p.sf / SIM.GETUP; po = u < 0.5 ? mix(P('kd'), P('crouch'), sm(u * 2)) : mix(P('crouch'), P('idle'), sm(u * 2 - 1)); if (u < 0.5) { ground = false; dy = -44 * (1 - sm(u * 2)); } break; }
      case 'tech': po = mix(P('hitHi'), P('idle'), sm(p.sf / 22)); break;
      case 'win': po = mix(P('idle'), P('win'), sm(p.sf / 14)); break;
      case 'throwing': po = keyed([[0, 'grabS'], [10, 'lift'], [20, 'lift'], [24, 'slam'], [34, 'slam'], [44, 'idle']], p.sf); break;
      case 'thrown': { po = P('thrown'); const t = o.sf; po.rot = t < 10 ? 0 : -Math.min(150, (t - 10) * 14); ground = false; dy = 0; break; }
      case 'atk': {
        const m = C.mv[p.mv], a = m.an || {}, f = p.mf;
        const bn = m.isAir ? 'jump' : m.crouch ? 'crouch' : 'idle';
        if (a.k) po = keyed(a.k, f);
        else {
          const su = m.su - 1, w = Math.max(1, Math.floor(su * 0.55)), end = m.lastAct + 2, rec = m.t > 900 ? end + 14 : m.t;
          if (f <= w) po = mix(P(bn), P(a.w), sm(f / w));
          else if (f < su) { const u = (f - w) / (su - w); po = mix(P(a.w), P(a.s), u * u * u); }
          else if (f <= end) po = P(a.s);
          else po = mix(P(a.s), P(bn), sm((f - end) / (rec - end)));
        }
        if (a.spin && f >= a.spin[0]) spin = Math.min(1, (f - a.spin[0]) / (a.spin[1] - a.spin[0])) * a.spin[2];
        if (m.thr) po = keyed([[0, 'idle'], [3, 'grabW'], [5, 'grabS'], [8, 'grabS'], [m.t, 'idle']], f);
        break;
      }
      default: po = P('idle');
    }
    return { po: po, ground: ground, spin: spin, dy: dy };
  }

  // ----------------------------------------------------------- rig
  const LOOK = [
    { top: 0xf1efe6, pants: 0xe9e6da, skin: 0xe6b088, hair: 0x1a1418, glove: 0xd01f2e, belt: 0x18181c, band: 0xd01f2e, shoe: 0xe6b088, trim: 0xd01f2e },
    { top: 0x1c1f2e, pants: 0x2848d0, skin: 0xd9a27c, hair: 0xf3dc4a, glove: 0xffd21e, belt: 0xffd21e, band: 0, shoe: 0xf4f4f4, trim: 0xffd21e }
  ];
  const ALT = [{ top: 0x27357a, pants: 0x1f2a63, band: 0xf2c230, glove: 0xf2c230, trim: 0xf2c230 }, { top: 0xb3202c, pants: 0x25252c, hair: 0xdfe6ee, glove: 0xffffff, belt: 0xffffff, trim: 0xffffff }];
  function buildRig(ch, alt) {
    const lk = Object.assign({}, LOOK[ch], alt ? ALT[ch] : {}), mats = {}, all = [];
    const mat = k => mats[k] || (mats[k] = new THREE.MeshLambertMaterial({ color: lk[k] }));
    const ol = new THREE.MeshBasicMaterial({ color: 0x0a0810, side: THREE.BackSide });
    function bx(par, w, h, d, x, y, z, k) {
      const g = new THREE.BoxGeometry(w, h, d); const m = new THREE.Mesh(g, mat(k)); m.position.set(x, y, z); par.add(m);
      const o = new THREE.Mesh(g, ol); o.scale.set((w + 2.4) / w, (h + 2.4) / h, (d + 2.4) / d); m.add(o); all.push(m); return m;
    }
    const G = new THREE.Group(), spin = new THREE.Group(), pel = new THREE.Group(), tor = new THREE.Group(), head = new THREE.Group();
    G.add(spin); spin.add(pel); pel.add(tor); tor.add(head);
    bx(pel, 15, 10, 21, 0, 0, 0, 'pants'); bx(pel, 16, 4, 22, 0, 5, 0, 'belt');
    tor.position.set(0, 6, 0); bx(tor, 16, 25, 25, 0, 13, 0, 'top'); bx(tor, 3, 21, 9, 8, 14, 0, ch === 0 ? 'skin' : 'trim');
    head.position.set(0, 27, 0); bx(head, 6, 4, 6, 0, 1, 0, 'skin'); bx(head, 13, 13, 12, 1, 9, 0, 'skin'); bx(head, 14, 5, 13, 0, 16, 0, 'hair'); bx(head, 5, 12, 13, -6, 10, 0, 'hair');
    bx(head, 2, 2, 8, 7.5, 10, 0, 'hair');
    if (ch === 0) { bx(head, 14.5, 3, 13.5, 0.8, 12.5, 0, 'band'); const tl = bx(head, 12, 2.5, 2, -13, 11, 3, 'band'); tl.rotation.z = 0.35; const t2 = bx(head, 10, 2.5, 2, -12, 9, -2, 'band'); t2.rotation.z = 0.6; }
    else { bx(head, 8, 6, 9, 3, 19, 0, 'hair'); bx(tor, 12, 9, 20, -6, 24, 0, 'top'); }
    const limbs = {};
    function arm(z, n) { const s = new THREE.Group(); s.position.set(0, 23, z); tor.add(s); bx(s, 8, 9, 8, 0, 0, 0, ch === 0 ? 'top' : 'skin'); bx(s, 6.5, 14, 6.5, 0, -8, 0, 'skin'); const e = new THREE.Group(); e.position.set(0, -15, 0); s.add(e); bx(e, 6.5, 13, 6.5, 0, -6, 0, ch === 0 ? 'skin' : 'trim'); bx(e, 8.5, 8.5, 8.5, 0, -15, 0, 'glove'); limbs[n] = [s, e]; }
    function leg(z, n) { const s = new THREE.Group(); s.position.set(0, -3, z); pel.add(s); bx(s, 10, 24, 10, 0, -11, 0, 'pants'); const e = new THREE.Group(); e.position.set(0, -24, 0); s.add(e); bx(e, 8.5, 22, 8.5, 0, -11, 0, 'pants'); bx(e, 14, 5, 9, 3, -24, 0, 'shoe'); limbs[n] = [s, e]; }
    arm(15, 'aP'); arm(-15, 'aN'); leg(6.5, 'lP'); leg(-6.5, 'lN');
    return { G, spin, pel, tor, head, limbs, mats, all };
  }
  function applyPose(rig, fp, face, flashK, tint) {
    const po = fp.po, L = po.l, T = po.t;
    rig.spin.rotation.z = -po.rot * D; rig.pel.rotation.z = -L * D; rig.tor.rotation.z = -T * D; rig.head.rotation.z = (L + T - po.h) * D;
    // lead limbs sit on the far side from the camera, whichever way we face
    const lead = face > 0 ? 'N' : 'P', rear = face > 0 ? 'P' : 'N';
    const A = (n, a) => { rig.limbs[n][0].rotation.z = (a[0] + L + T) * D; rig.limbs[n][1].rotation.z = a[1] * D; };
    const Lg = (n, a) => { rig.limbs[n][0].rotation.z = (a[0] + L) * D; rig.limbs[n][1].rotation.z = a[1] * D; };
    A('a' + lead, po.af); A('a' + rear, po.ab); Lg('l' + lead, po.lf); Lg('l' + rear, po.lb);
    const fy = a => -24 * Math.cos(a[0] * D) - 24 * Math.cos((a[0] + a[1]) * D) - 3.5;
    let hip = 52;
    if (fp.ground) hip = -Math.min(fy(po.lf), fy(po.lb)) + 3;
    rig.spin.position.y = hip + fp.dy;
    const yaw = (26 + po.yaw + fp.spin) * D;
    rig.G.rotation.y = face > 0 ? -yaw : Math.PI + yaw;
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
      if (e.t === 'hit') { fx.push({ k: 'spark', x, y, lvl: e.lvl, t: 0, ko: e.ko }); if (e.di) fx.push({ k: 'paint', x, y, t: 0 }); if (e.wrong) fx.push({ k: 'word', x, y: y + 30, t: 0, s: e.wrong === 'h' ? 'OVERHEAD!' : 'LOW!', c: e.wrong === 'h' ? '#ff8a1e' : '#33a0ff' }); if (e.crum) fx.push({ k: 'word', x, y: y + 44, t: 0, s: 'CRUMPLE', c: '#ffd21e' }); shake = Math.max(shake, e.ko ? 22 : [3, 7, 11][e.lvl]); if (e.lvl >= 2 || e.ko) flash = e.ko ? 6 : 2; combo[e.who] = { n: e.combo, d: e.total || e.dmg, t: 0, ch: e.ch }; }
      else if (e.t === 'block') { fx.push({ k: 'guard', x, y, t: 0 }); shake = Math.max(shake, 2); }
      else if (e.t === 'parry') fx.push({ k: 'guard', x, y, t: 0, big: 1 });
      else if (e.t === 'pparry') { fx.push({ k: 'guard', x, y, t: 0, big: 1 }); fx.push({ k: 'ring', x, y, t: 0 }); fx.push({ k: 'word', x, y: y + 40, t: 0, s: 'PERFECT PARRY', c: '#7fdcff' }); flash = 2; }
      else if (e.t === 'armor') { fx.push({ k: 'guard', x, y, t: 0, big: 1 }); fx.push({ k: 'word', x, y: y + 34, t: 0, s: 'ARMOR', c: '#ffd21e' }); shake = Math.max(shake, 6); }
      else if (e.t === 'splat') { fx.push({ k: 'paint', x, y, t: 0 }); fx.push({ k: 'word', x: x - 30, y: y + 50, t: 0, s: e.stun ? 'STUN!' : 'WALL SPLAT', c: '#ff3355' }); shake = 14; flash = 2; }
      else if (e.t === 'burnout') { const p = s.p[e.who]; fx.push({ k: 'word', x: p.x / F, y: 110, t: 0, s: 'BURNOUT', c: '#b8b0c0' }); }
      else if (e.t === 'rush') { const p = s.p[e.who]; fx.push({ k: 'dust', x: p.x / F, y: 0, t: 0, big: 1 }); fx.push({ k: 'word', x: p.x / F, y: 104, t: 0, s: 'DRIVE RUSH', c: '#5dffa0' }); }
      else if (e.t === 'di') { const p = s.p[e.who]; fx.push({ k: 'word', x: p.x / F, y: 110, t: 0, s: 'DRIVE IMPACT', c: '#ff5ad2' }); }
      else if (e.t === 'wbounce') { fx.push({ k: 'spark', x, y, lvl: 2, t: 0, wall: 1 }); shake = 14; flash = 2; }
      else if (e.t === 'gbounce' || e.t === 'kd') { fx.push({ k: 'dust', x, y: 0, t: 0, big: 1 }); shake = Math.max(shake, e.t === 'kd' ? 5 : 9); }
      else if (e.t === 'jump' || e.t === 'land' || e.t === 'dash') { const p = s.p[e.who]; fx.push({ k: 'dust', x: p.x / F, y: 0, t: 0 }); }
      else if (e.t === 'tech' || e.t === 'clash') fx.push({ k: 'guard', x, y, t: 0, big: 1 });
      else if (e.t === 'pend') fx.push({ k: 'ring', x, y, t: 0 });
      else if (e.t === 'super') superFx = { who: e.who, t: 0, lv: e.lv, name: SIM.CH[s.p[e.who].ch].mv['SA' + e.lv].n };
      else if (e.t === 'ex') fx.push({ k: 'exflash', who: e.who, t: 0 });
      else if (e.t === 'round') msg = { big: s.round >= s.cfg.rounds * 2 - 1 && s.p[0].wins === s.p[1].wins && s.round > 1 ? 'FINAL ROUND' : 'ROUND ' + s.round, jp: '第' + '一二三四五'[s.round - 1] + '回戦', t: 0, dur: 80, col: '#ffd21e' };
      else if (e.t === 'fight') msg = { big: 'FIGHT!', jp: '始め！', t: 0, dur: 50, col: '#ff3355', slam: 1 };
      else if (e.t === 'ko') msg = e.type === 'time' ? { big: 'TIME OVER', jp: '時間切れ', t: 0, dur: 150, col: '#ffd21e' } : e.type === 'draw' || e.type === 'dko' ? { big: e.type === 'dko' ? 'DOUBLE K.O.' : 'DRAW', jp: '引き分け', t: 0, dur: 150, col: '#ffd21e' } : { big: 'K.O.', jp: e.type === 'perfect' ? '完全勝利 PERFECT' : '一本！', t: 0, dur: 170, col: '#ff3355', slam: 1 };
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
      tx = (ax + bx) / 2; tw = Math.max(330, Math.min(520, Math.abs(ax - bx) + 190)); ty = 68 + Math.max(a.y, b.y) / F * 0.42;
      if (superFx && superFx.t < 40) { const p = s.p[superFx.who]; tx = p.x / F; tw = 300; ty = 62; }
      const lim = SIM.STAGE / F + 30 - tw / 2; tx = Math.max(-lim, Math.min(lim, tx));
      camX += (tx - camX) * 0.22; camW += (tw - camW) * 0.14; camY += (ty - camY) * 0.2;
      const dim = s.freeze > 0; stage.visible = !dim; scene.background.setHex(dim ? 0x05030c : 0x0b0a24);
      for (let i = 0; i < 2; i++) {
        const p = s.p[i], o = s.p[1 - i], r = rigs[i]; r.G.visible = true;
        const fp = fighterPose(p, o, s.frame);
        let jx = 0; if (p.hs > 0 && (p.st === 'hit' || p.st === 'ahit' || p.st === 'block')) jx = ((p.hs & 1) ? 1 : -1) * Math.min(3, p.hs * 0.5);
        const fl = (p.hs > 0 && (p.st === 'hit' || p.st === 'ahit')) ? Math.min(0.9, p.hs * 0.14) : 0;
        const m = p.st === 'atk' ? SIM.CH[p.ch].mv[p.mv] : null; let tint = [0, 0, 0];
        if (m && (m.od || m.sup)) { const k = 0.25 + 0.2 * Math.sin(s.frame * 0.9); tint = m.sup ? [k, k * 0.6, 0] : [k, k, 0]; }
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
      if (menuPan) { camX = 0; camW = menuPan.w || 340; camY = menuPan.y || 34; for (let i = 0; i < 2; i++) { const fake = { ch: menuPan.chars[i], st: menuPan.pose && menuPan.pose[i] ? 'win' : 'idle', sub: 'stand', sf: 30, subf: 0, y: 0, vy: 0 }; const fp = fighterPose(fake, fake, tick); applyPose(rigs[i], fp, i ? -1 : 1, 0, [0, 0, 0]); rigs[i].G.position.set(i ? 75 : -75, 0, 0); shadows[i].position.x = i ? 75 : -75; shadows[i].scale.set(1, 1, 1); } }
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
      } else if (f.k === 'word') { const u = t / 44; g2.globalAlpha = u > 0.75 ? Math.max(0, 1 - (u - 0.75) * 4) : 1; txt(f.s, x, y - 14 - t * 0.35, 8, f.c, 'center'); g2.globalAlpha = 1; dead = t >= 44;
      } else if (f.k === 'paint') { const u = t / 22; g2.globalAlpha = 1 - u * u; const cols = ['#ff5ad2', '#33e0ff', '#ffd21e', '#5dffa0', '#ff3355']; for (let k = 0; k < 14; k++) { const a = k * 0.449 + f.x, rr = (10 + 60 * Math.sqrt(u)) * (0.5 + hsh(k * 7 + 3)); g2.fillStyle = cols[k % 5]; const s2 = Math.round(10 * (1 - u * 0.5) * (0.6 + hsh(k))); g2.fillRect(Math.round(x + Math.cos(a) * rr) - s2 / 2, Math.round(y + Math.sin(a) * rr * 0.8) - s2 / 2, s2, s2); } g2.globalAlpha = 1; dead = t >= 22;
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
      if (cm && !cm.isAir && cm.hits[0] && p.mf <= cm.hits[0].e && (cm.hits[0].g === 'h' || cm.thr || cm.di)) { const q = toScreen(p.x / F, p.y / F + 112), oh = cm.hits[0].g === 'h'; if ((s.frame >> 1) & 1 || p.mf > 4) { txt('!', q[0], q[1] - 12, 16, cm.di ? '#ff5ad2' : '#ff8a1e', 'center'); txt(cm.di ? 'IMPACT' : oh ? 'OVERHEAD' : 'THROW', q[0], q[1] + 6, 8, cm.di ? '#ff5ad2' : '#ff8a1e', 'center'); } }
      const c = combo[i];
      if (c) { c.t++; if (c.n >= 2 || c.ch) { const pop = c.t < 5 ? 1 + (5 - c.t) * 0.12 : 1, cx = i ? 400 : 80, a = c.t > 70 ? Math.max(0, 1 - (c.t - 70) / 12) : 1; g2.globalAlpha = a; if (c.n >= 2) { g2.save(); g2.translate(cx, 78); g2.scale(pop, pop); txt('' + c.n, 0, -16, 24, '#ffd21e', 'right'); txt('HITS', 4, -8, 8, '#fff', 'left'); g2.restore(); txt(c.d + ' DMG', cx, 90, 8, '#ff8a6a', 'center'); txt('連撃', cx + 40, 60, 12, '#ff3355', 'center', 1); } if (c.ch && c.t < 40) txt(c.ch === 2 ? 'PUNISH COUNTER' : 'COUNTER', cx, 104, 8, c.ch === 2 ? '#ffd21e' : '#33e0ff', 'center'); g2.globalAlpha = 1; } if (c.t > 82) combo[i] = null; }
    }
    rect(226, 8, 28, 26, '#0a0612'); rect(227, 9, 26, 24, '#f4f0e0'); rect(228, 10, 24, 22, '#1a1030');
    const tm = s.cfg.training ? '∞' : ('' + Math.max(0, Math.ceil(s.time / 60))).padStart(2, '0'); txt(tm, 240.5, 14, s.cfg.training ? 16 : 12, s.time < 600 && !s.cfg.training ? '#ff3355' : '#ffd21e', 'center', s.cfg.training);
    if (superFx) {
      const t = superFx.t++, i = superFx.who;
      if (t < 46) { const p = s.p[i], q = toScreen(p.x / F, p.y / F + 55); g2.globalAlpha = 0.8; burst(q[0], q[1], 40 + (t % 6) * 6, 420, 36, i ? '#33e0ff' : '#ffd21e', t * 0.05, 2); burst(q[0], q[1], 30, 90 - t, 12, '#fff', -t * 0.15, 3); g2.globalAlpha = 1;
        const sl = Math.min(1, t / 8), bx = i ? 480 - 300 * sl : -300 + 300 * sl; rect(bx, 186, 300, 34, 'rgba(10,6,18,0.85)'); rect(bx, 186, 300, 2, '#ff3355'); rect(bx, 218, 300, 2, '#ff3355');
        txt(superFx.name.toUpperCase(), bx + 150, 192, 8, '#fff', 'center'); txt('超必殺  SUPER ART LV' + superFx.lv, bx + 150, 202, 16, '#ffd21e', 'center', 1); }
      if (t > 60) superFx = null;
    }
    if (msg) {
      const t = msg.t++, u = Math.min(1, t / 7), out = t > msg.dur - 8 ? (t - (msg.dur - 8)) / 8 : 0; let sc = msg.slam ? 1 + (1 - u) * 2.5 : 1; g2.globalAlpha = Math.max(0, (msg.slam ? u : 1) * (1 - out));
      const w = msg.slam ? 480 : 480 * sm(t / 10); rect(240 - w / 2, 106, w, 46, 'rgba(10,6,18,0.72)'); rect(240 - w / 2, 106, w, 2, msg.col); rect(240 - w / 2, 150, w, 2, msg.col);
      g2.save(); g2.translate(240, 122); g2.scale(sc, sc); txt(msg.big, 0, -10, 24, msg.col, 'center'); g2.restore(); txt(msg.jp, 240, 136, 12, '#fff', 'center', 1); g2.globalAlpha = 1; if (t >= msg.dur) msg = null;
    }
  }
  function boxes(s) {
    const dr = (b, col) => { const a = toScreen(b[0] / F, b[3] / F), c = toScreen(b[2] / F, b[1] / F); g2.fillStyle = col + '44'; g2.fillRect(a[0], a[1], c[0] - a[0], c[1] - a[1]); g2.strokeStyle = col; g2.lineWidth = 1; g2.strokeRect(a[0] + 0.5, a[1] + 0.5, c[0] - a[0], c[1] - a[1]); };
    for (const p of s.p) { for (const u of SIM.hurt(p)) dr(u, '#33e0ff'); for (const h of SIM.hitboxes(p)) dr(h.b, '#ff3355'); }
    for (const q of s.proj) dr(SIM.pbox(q), '#ff3355');
  }
  return { init, setFighters, events, draw3d, clear, drawFx, hud, boxes, txt, rect, burst, toScreen, get g() { return g2; }, get tick() { return tick; }, VW, VH, hsh, say(m) { msg = m; }, FJ, FP };
})();
