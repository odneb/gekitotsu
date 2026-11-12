# GEKITOTSU animation: every frame of every clip is a key pose, authored here and baked for the game. Nothing is
# interpolated at run time. A pose is written as targets in the fighter's own space (x = his left, y = the way he
# faces, z = up, centimetres from the point between his feet); limbs are solved with two-bone IK against the bind
# pose of the rebuilt model. The game receives, for each frame and each bone, the transform that carries the bind
# pose to the key pose, once as authored and once mirrored (a fighter facing right is shown in mirror image).
import json, math, sys, os, struct
import numpy as np
from scipy.spatial.transform import Rotation
HERE = os.path.dirname(os.path.abspath(__file__))
REF = json.load(open(os.path.join(HERE, 'ref.json')))
ORDER = ['Hips', 'Spine02', 'Spine01', 'Spine', 'neck', 'Head', 'head_end', 'headfront', 'LeftShoulder', 'LeftArm', 'LeftForeArm', 'LeftHand', 'RightShoulder', 'RightArm', 'RightForeArm', 'RightHand',
         'LeftUpLeg', 'LeftLeg', 'LeftFoot', 'LeftToeBase', 'RightUpLeg', 'RightLeg', 'RightFoot', 'RightToeBase']
ORDER = [b for b in ORDER if b in REF]
RP = {k: np.array([v['head'][0] * 100, -v['head'][1] * 100, v['head'][2] * 100]) for k, v in REF.items()}   # Blender metres to the game's centimetres
PAR = {k: v['parent'] for k, v in REF.items()}
CHILD = {'Hips': 'Spine02', 'Spine02': 'Spine01', 'Spine01': 'Spine', 'Spine': 'neck', 'neck': 'Head'}
for s in ('Left', 'Right'): CHILD.update({s + 'Shoulder': s + 'Arm', s + 'Arm': s + 'ForeArm', s + 'ForeArm': s + 'Hand', s + 'UpLeg': s + 'Leg', s + 'Leg': s + 'Foot', s + 'Foot': s + 'ToeBase'})
V = lambda *a: np.array(a, float)
def nrm(v, fb=(0, 0, 1)):
    n = np.linalg.norm(v); return v / n if n > 1e-8 else np.array(fb, float)
def rotz(v, deg):
    a = math.radians(deg); c, s = math.cos(a), math.sin(a); return V(v[0] * c - v[1] * s, v[0] * s + v[1] * c, v[2])
def lean(deg, side, f, l):
    a, b = math.radians(deg), math.radians(side); return nrm(f * math.sin(a) + l * math.sin(b) + V(0, 0, math.cos(a) * math.cos(b)))
def frame(d, h):
    x = nrm(d, (1, 0, 0)); z = nrm(np.asarray(h, float))
    if abs(np.dot(x, z)) > 0.9999: z = V(0, 0, 1) if abs(x[2]) < 0.999 else V(1, 0, 0)
    y = nrm(np.cross(z, x)); z2 = np.cross(x, y); return np.stack([x, y, z2], 1)
def ik2(a, t, l1, l2, pole):
    d = t - a; ln = min(max(np.linalg.norm(d), 1e-3), (l1 + l2) * 0.999); dn = nrm(d, (0, 0, -1))
    x = (l1 * l1 - l2 * l2 + ln * ln) / (2 * ln); h = math.sqrt(max(0.0, l1 * l1 - x * x))
    side = np.asarray(pole, float) - dn * np.dot(pole, dn); side = nrm(side, (0, 0, -1))
    return a + dn * x + side * h, a + dn * ln
seg = lambda a, b: float(np.linalg.norm(RP[b] - RP[a]))
def ref_hint(b):
    L = b.startswith('Left'); s = 'Left' if L else 'Right'
    if 'Arm' in b and 'Shoulder' not in b:
        x = np.cross(RP[s + 'ForeArm'] - RP[s + 'Arm'], RP[s + 'Hand'] - RP[s + 'ForeArm']); return x if np.linalg.norm(x) > 0.5 else V(0, 0, 1)
    if b.endswith('Foot'): return V(0, 0, 1)
    if 'Leg' in b:
        x = np.cross(RP[s + 'Leg'] - RP[s + 'UpLeg'], RP[s + 'Foot'] - RP[s + 'Leg']); return x if np.linalg.norm(x) > 0.5 else V(1, 0, 0)
    return V(0, 1, 0)

def solve(P):
    """returns {bone: (R 3x3, t 3)}: the transform that carries the bind pose of that bone's skin to the pose"""
    g = lambda k: np.asarray(P[k], float)
    Fwd, Cf, Cl, Pl = rotz(V(0, 1, 0), P['pt']), rotz(V(0, 1, 0), P['tw']), rotz(V(1, 0, 0), P['tw']), rotz(V(1, 0, 0), P['pt'])
    D1, D2, D3 = lean(P['ln'] * 0.5, P['sd'] * 0.5, Fwd, Pl), lean(P['ln'], P['sd'], Cf, Cl), lean(P['ln2'], P['sd'], Cf, Cl)
    Hf = rotz(Cf, P['hy']); Dh = lean(P['ln2'] * 0.4 + P['hd'], 0, Hf, rotz(Cl, P['hy']))
    J = {}; H = {}
    J['Hips'] = g('hip'); J['Spine02'] = J['Hips'] + D1 * seg('Hips', 'Spine02'); J['Spine01'] = J['Spine02'] + D2 * seg('Spine02', 'Spine01')
    J['Spine'] = J['Spine01'] + D3 * seg('Spine01', 'Spine'); J['neck'] = J['Spine'] + D3 * seg('Spine', 'neck'); J['Head'] = J['neck'] + Dh * seg('neck', 'Head')
    H.update(Hips=Fwd, Spine02=Fwd, Spine01=Cf, Spine=Cf, neck=Hf, Head=Hf)
    ChestL = nrm(np.cross(D3, Cf)); ChestL = -ChestL if np.dot(ChestL, Cl) < 0 else ChestL
    PelvL = nrm(np.cross(D1, Fwd)); PelvL = -PelvL if np.dot(PelvL, Pl) < 0 else PelvL
    for s, sg, hk, ek, fk, kk, tk in (('Left', 1, 'hl', 'el', 'fl', 'kl', 'tl'), ('Right', -1, 'hr', 'er', 'fr', 'kr', 'tr')):
        Ro = RP[s + 'Arm'] - RP['Spine']; Rs = RP[s + 'Shoulder'] - RP['Spine']
        ShP = J['Spine'] + ChestL * abs(Ro[0]) * sg + D3 * Ro[2] + Cf * Ro[1]; ClP = J['Spine'] + ChestL * abs(Rs[0]) * sg + D3 * Rs[2] + Cf * Rs[1]
        El, Wr = ik2(ShP, g(hk), seg(s + 'Arm', s + 'ForeArm'), seg(s + 'ForeArm', s + 'Hand'), g(ek))
        J[s + 'Shoulder'], J[s + 'Arm'], J[s + 'ForeArm'], J[s + 'Hand'] = ClP, ShP, El, Wr
        an = np.cross(El - ShP, Wr - El); an = an if np.linalg.norm(an) > 0.5 else V(0, 0, 1)
        H[s + 'Shoulder'] = Cf; H[s + 'Arm'] = an; H[s + 'ForeArm'] = an
        Rh = RP[s + 'UpLeg'] - RP['Hips']; HipJ = J['Hips'] + PelvL * abs(Rh[0]) * sg + D1 * Rh[2] + Fwd * Rh[1]
        Kn, An = ik2(HipJ, g(fk), seg(s + 'UpLeg', s + 'Leg'), seg(s + 'Leg', s + 'Foot'), g(kk))
        J[s + 'UpLeg'], J[s + 'Leg'], J[s + 'Foot'] = HipJ, Kn, An; J[s + 'ToeBase'] = An + nrm(g(tk)) * seg(s + 'Foot', s + 'ToeBase')
        lgn = np.cross(Kn - HipJ, An - Kn); lgn = lgn if np.linalg.norm(lgn) > 0.5 else V(1, 0, 0)
        H[s + 'UpLeg'] = lgn; H[s + 'Leg'] = lgn; H[s + 'Foot'] = V(0, 0, 1)
    out = {}
    for b in ORDER:
        c = CHILD.get(b)
        if c and b in J and c in J:
            R = frame(J[c] - J[b], H[b]) @ frame(RP[c] - RP[b], ref_hint(b)).T
            sc = P.get('scale', {}).get(b, 1.0)
            out[b] = (R * sc, J[b] - (R * sc) @ RP[b])
        else: out[b] = out[PAR[b]] if PAR[b] in out else (np.eye(3), np.zeros(3))
    return out

def mirror(P):
    mx = lambda v: (-v[0], v[1], v[2]); Q = dict(P)
    Q['hip'] = mx(P['hip']); Q['sd'], Q['tw'], Q['pt'], Q['hy'] = -P['sd'], -P['tw'], -P['pt'], -P['hy']
    for a, b in (('hl', 'hr'), ('fl', 'fr'), ('el', 'er'), ('kl', 'kr'), ('tl', 'tr')): Q[a] = mx(P[b]); Q[b] = mx(P[a])
    if 'scale' in P: Q['scale'] = {(k.replace('Left', '@').replace('Right', 'Left').replace('@', 'Right')): v for k, v in P['scale'].items()}
    return Q

def blend(A, B, k):
    if k <= 0: return A
    if k >= 1: return B
    R = {}
    for key in A:
        if key == 'scale': continue
        a, b = A[key], B[key]; R[key] = tuple(x + (y - x) * k for x, y in zip(a, b)) if isinstance(a, tuple) else a + (b - a) * k
    sa, sb = A.get('scale', {}), B.get('scale', {})
    if sa or sb: R['scale'] = {n: sa.get(n, 1.0) + (sb.get(n, 1.0) - sa.get(n, 1.0)) * k for n in set(sa) | set(sb)}
    return R
ease = lambda k: (lambda t: t * t * (3 - 2 * t))(min(1.0, max(0.0, k)))
