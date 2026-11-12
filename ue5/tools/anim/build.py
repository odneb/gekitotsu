# Builds every clip the game plays, frame by frame, and bakes them: Content/Gk/kagu_anim.txt (the clips, as lists of
# frame numbers) and kagu_anim.bin (each distinct frame: per bone, position, rotation and scale, as authored and mirrored).
import sys, os, json, math, struct
import numpy as np
from scipy.spatial.transform import Rotation
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import anim, poses
from anim import blend, ease
P = poses.P
OUT = sys.argv[1] if len(sys.argv) > 1 else '.'
frames = []; index = {}; clips = {}
def key(p): return json.dumps(p, sort_keys=True)
def fid(p):
    k = key(p)
    if k not in index: index[k] = len(frames); frames.append(p)
    return index[k]
def clip(name, seq, loop=False): clips[name] = ([fid(p) for p in seq], loop)
def cycle(keys, per, hold):
    """a loop through the keys, 'per' frames from one key to the next, with a new drawing every 'hold' frames"""
    n = len(keys); out = []
    for f in range(n * per):
        q = (f // hold) * hold; i = q // per; out.append(blend(keys[i], keys[(i + 1) % n], ease((q % per) / per)))
    return out

# --- states
clip('Idle', cycle([P['Idle'], P['Idle2']], 36, 6), True)
clip('Walk', cycle([P['WalkA'], P['WalkB'], P['WalkC'], P['WalkD']], 8, 4), True)
clip('WalkBack', cycle([P['WalkA'], P['WalkD'], P['WalkC'], P['WalkB']], 9, 3), True)
clip('Crouch', [P['Crouch']]); clip('PreJump', [blend(P['Idle'], P['Crouch'], 0.7)]); clip('Land', [blend(P['Crouch'], P['Idle'], 0.35)] * 2 + [blend(P['Crouch'], P['Idle'], 0.7)])
clip('Air', [blend(P['Jump'], P['Fall'], ease((i + 0.5) / 6)) for i in range(6)])
clip('Dash', [blend(P['Idle'], P['Dash'], 0.6)] * 2 + [P['Dash']]); clip('BackDash', [blend(P['Idle'], P['BackDash'], 0.6)] * 2 + [P['BackDash']])
clip('BlockS', [P['BlockS']]); clip('BlockC', [P['BlockC']])
clip('Parry', [P['Parry']] * 11 + [blend(P['Parry'], P['Idle'], ease((((f - 10) // 3) * 3) / 18)) for f in range(11, 30)])
clip('Hit', [blend(P['Hit'], P['Idle'], ease(((f // 2) * 2) / 14) * 0.8) for f in range(15)])
clip('Launch', [P['Launch']]); clip('Down', [P['Down']]); clip('Held', [P['Held']]); clip('Win', [P['Win']])
clip('Rise', [P['Rise']] * 9 + [blend(P['Rise'], P['Idle'], ease(((f - 9) // 3 * 3) / 9)) for f in range(9, 19)])
clip('Grab', [P['Grab']] * 20 + [P['MS']])

# --- moves: (wind-up, strike, limb steered onto the hitbox: 1 left hand, 2 right hand, 3 left foot, 4 right foot)
MOVES = {'S5L': ('LW', 'LS', 2), 'S5M': ('MW', 'MS', 1), 'S5H': ('HW', 'HS', 1), 'S2L': ('Crouch', 'CL', 4), 'S2M': ('Crouch', 'CM', 3), 'S2H': ('CHW', 'CHS', 1),
         'JL': ('Jump', 'JL', 1), 'JM': ('Jump', 'JM', 3), 'JH': ('Jump', 'JH', 4), 'J2H': ('Fall', 'JD', 3), 'FIRE': ('FireW', 'FireS', 0), 'SURGE': ('SurgeW', 'SurgeS', 4),
         'RISE': ('CHW', 'RiseS', 2), 'VANISH': ('Vanish', 'Vanish', 0), 'WHIRL': ('Jump', 'Whirl', 0), 'DIVE': ('Jump', 'Dive', 3), 'HOME': ('HomeW', 'HomeS', 1),
         'THROW': ('LW', 'Grab', 0), 'SUPER1': ('MW', 'HS', 1), 'SUPER2': ('MW', 'HS', 1), 'SUPER3': ('MW', 'HS', 1)}
for line in open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'moves.txt')):
    t = line.split(); name = t[0]; su, ac, rc, hx, hy, hw, hh, lvy, fl = [int(x) for x in t[1:]]
    wind, strike, limb = MOVES[name]; W, S = P[wind], P[strike]
    base = P['Jump'] if fl & 2 else P['Crouch'] if fl & 1 else P['Idle']
    su1 = max(1, su); snap = max(1, su1 - 3); seq = []
    for f in range(su + ac + rc + 1):
        if f <= su1:
            if f <= snap: q = min(snap, ((f + 1) // 2) * 2); R = blend(base, W, ease(q / snap))        # the wind-up is drawn on twos
            else: R = blend(W, S, ease((f - snap) / (su1 - snap)))                                      # then three frames of snap into the blow
        elif f <= su1 + ac or name in ('HOME', 'DIVE'): R = S
        else:
            q = ((f - su1 - ac) // 3) * 3; k = max(0.0, (q / max(1, rc) - 0.25) / 0.75); R = blend(S, base, ease(k))   # the blow is held, then recovers on threes
        if limb and su1 - 2 < f <= su1 + ac + 2 and hw > 0:
            R = dict(R); tg = (6.0 if limb in (1, 3) else -6.0, hx + hw * 0.82, max(8.0, hy + hh * (0.9 if lvy > 1500 else 0.5)))
            R[{1: 'hl', 2: 'hr', 3: 'fl', 4: 'fr'}[limb]] = tg
        seq.append(R)
    clip(name, seq)
# every key pose is also a one-frame clip, for inspection in the game
for n, p in P.items():
    if n not in clips: clip('pose_' + n, [p])

# --- bake
B = anim.ORDER; data = np.zeros((len(frames), 2, len(B), 8), np.float32)
for i, p in enumerate(frames):
    for v, pose in enumerate((p, anim.mirror(p))):
        D = anim.solve(pose)
        for j, b in enumerate(B):
            R, t = D[b]; s = float(np.cbrt(max(1e-9, np.linalg.det(R)))); q = Rotation.from_matrix(R / s).as_quat()
            data[i, v, j] = (t[0], t[1], t[2], q[0], q[1], q[2], q[3], s)
os.makedirs(OUT, exist_ok=True)
data.astype('<f4').tofile(os.path.join(OUT, 'kagu_anim.bin'))
with open(os.path.join(OUT, 'kagu_anim.txt'), 'w') as f:
    f.write('# GEKITOTSU baked animation. Written by tools/anim/build.py.\nframes %d\nbones %s\n' % (len(frames), ' '.join(B)))
    for n, (seq, loop) in clips.items(): f.write('clip %s %s %s\n' % (n, 'loop' if loop else 'once', ' '.join(str(x) for x in seq)))
print('frames', len(frames), 'clips', len(clips), 'bytes', data.nbytes)
if len(sys.argv) > 2:   # a preview sheet of one clip: every distinct drawing in order
    seq = clips[sys.argv[2]][0]; uniq = []
    for k, x in enumerate(seq):
        if not uniq or uniq[-1][1] != x: uniq.append((k, x))
    json.dump([('%s f%d' % (sys.argv[2], k), frames[x]) for k, x in uniq], open('/tmp/spec.json', 'w'))
