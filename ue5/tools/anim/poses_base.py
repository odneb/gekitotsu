# Writes Content/Gk/kagu.txt: every pose and every move's animation for the fighter, as plain text the game reads at
# the start of a match. One line per pose or move. Poses are targets in his own space: x = his left, y = the way he
# faces, z = up, centimetres from the point between his feet. All of it is authored here for this character.
import re, sys, json
DEF = dict(hip=(0, -7, 83), ln=0, ln2=0, sd=0, tw=0, pt=0, hd=0, hy=0, hl=(13, 46, 132), hr=(-25, -18, 80), fl=(9, 26, 9), fr=(-13, -27, 9),
           el=(0.3, -0.2, -1), er=(-0.8, -0.6, 0.2), kl=(0.5, 1, 0), kr=(-0.9, 0.6, 0), tl=(0.1, 1, -0.5), tr=(-0.8, 0.5, -0.5), roll=0, spin=0)
def V(x, y, z): return (x, y, z)
def Mk(hip, ln, ln2, tw, pt, hd, hy, hl, hr, fl, fr): d = dict(DEF); d.update(hip=hip, ln=ln, ln2=ln2, tw=tw, pt=pt, hd=hd, hy=hy, hl=hl, hr=hr, fl=fl, fr=fr); return d
def With(p, el, er): p = dict(p); p.update(el=el, er=er); return p
def Knees(p, kl, kr): p = dict(p); p.update(kl=kl, kr=kr); return p
def Toes(p, tl, tr): p = dict(p); p.update(tl=tl, tr=tr); return p
def Also(p, **kw): p = dict(p); p.update(kw); return p
AB, ABr = V(0.4, -1, 0.4), V(-0.4, -1, 0.4)
P = {}
# standing: a reed in the wind. Weight on the back leg, lead hand offered open, the other trailing low behind.
P['Idle'] = Mk(V(0, -7, 83), -7, 2, -52, -34, 6, 42, V(13, 46, 132), V(-25, -18, 80), V(9, 26, 9), V(-13, -27, 9))
P['Idle2'] = Mk(V(0, -3, 81), 2, 10, -48, -32, -2, 40, V(15, 44, 124), V(-24, -14, 84), V(9, 26, 9), V(-13, -27, 9))
P['Crouch'] = Knees(Mk(V(0, -8, 50), 30, 40, -40, -26, -30, 30, V(12, 40, 70), V(-22, -20, 56), V(11, 30, 9), V(-15, -34, 11)), V(0.7, 1, 0.3), V(-0.9, 0.5, 0.2))
P['WalkA'] = Mk(V(0, 0, 84), 2, 6, -46, -30, 2, 36, V(14, 44, 128), V(-24, -12, 84), V(9, 34, 9), V(-12, -10, 16))
P['WalkB'] = Mk(V(0, 2, 82), 4, 8, -46, -30, 0, 36, V(15, 42, 126), V(-24, -16, 82), V(9, 4, 15), V(-13, -30, 9))
P['Jump'] = Mk(V(0, 0, 95), 24, 34, -30, -20, -20, 20, V(14, 26, 104), V(-18, 10, 98), V(10, 24, 58), V(-10, 2, 50))
P['Fall'] = With(Mk(V(0, 0, 95), -12, -18, -40, -30, 12, 30, V(34, 10, 140), V(-36, -12, 134), V(10, 14, 24), V(-12, -16, 30)), V(1, 0, -0.3), V(-1, 0, -0.3))
# the dash is a ninja's: body thrown forward, both arms streaming behind
P['Dash'] = With(Mk(V(0, 10, 70), 46, 54, -24, -14, -40, 14, V(16, -22, 96), V(-20, -34, 92), V(9, 40, 9), V(-12, -36, 22)), AB, ABr)
P['BackDash'] = Mk(V(0, -14, 82), -26, -32, -60, -40, 22, 40, V(20, 30, 120), V(-26, -6, 100), V(9, 22, 14), V(-12, -30, 9))
P['BlockS'] = With(Mk(V(0, -10, 84), -4, 4, -30, -26, 2, 24, V(6, 30, 128), V(-8, 26, 108), V(9, 22, 9), V(-13, -28, 9)), V(0.6, -0.3, -1), V(-0.6, -0.3, -1))
P['BlockC'] = With(Knees(Mk(V(0, -8, 50), 22, 30, -30, -22, -22, 24, V(6, 30, 84), V(-8, 26, 70), V(11, 28, 9), V(-15, -32, 11)), V(0.7, 1, 0.3), V(-0.9, 0.5, 0.2)), V(0.6, -0.3, -1), V(-0.6, -0.3, -1))
P['Hit'] = Mk(V(0, -14, 80), -22, -34, -60, -40, -20, 30, V(24, 10, 124), V(-28, -20, 104), V(9, 20, 9), V(-13, -32, 9))
P['Launch'] = Mk(V(0, 0, 60), -40, -56, -50, -30, -26, 20, V(30, -10, 90), V(-34, -30, 70), V(12, 34, 70), V(-10, 20, 44))
P['Down'] = Mk(V(0, -10, 14), -84, -90, -20, -10, -10, 0, V(34, -40, 10), V(-36, -60, 10), V(12, 60, 12), V(-12, 50, 26))
P['Rise'] = Knees(Mk(V(0, -6, 46), 34, 44, -30, -20, -30, 20, V(14, 34, 10), V(-20, -10, 60), V(11, 26, 9), V(-15, -34, 11)), V(0.7, 1, 0.3), V(-0.9, 0.5, 0.2))
P['Parry'] = With(Mk(V(0, -6, 82), -2, 2, -66, -38, 2, 54, V(4, 40, 120), V(-22, -10, 96), V(9, 24, 9), V(-13, -28, 9)), V(0.8, 0, -0.6), V(-0.8, -0.6, 0.2))
# grounded strikes: thrown while swaying away or toppling in
P['LS'] = Mk(V(0, -2, 84), -4, 0, -70, -40, 4, 60, V(6, 80, 130), V(-24, -16, 84), V(9, 26, 9), V(-13, -27, 9))
P['MW'] = Mk(V(0, -10, 82), -12, -8, -76, -44, 6, 66, V(10, 30, 120), V(-22, -10, 112), V(9, 24, 9), V(-13, -28, 9))
P['MS'] = With(Mk(V(0, 14, 74), 30, 36, 28, 6, -26, -20, V(22, -10, 96), V(-4, 60, 116), V(9, 40, 9), V(-13, -34, 14)), V(0.6, -1, 0), V(-0.5, -0.3, -1))
P['HW'] = With(Mk(V(0, -12, 82), -18, -26, -84, -50, 10, 70, V(12, 34, 116), V(-30, -24, 150), V(9, 24, 10), V(-13, -28, 9)), V(0.3, -0.2, -1), V(-1, -0.4, 0.4))
P['HS'] = With(Mk(V(0, 16, 72), 34, 42, 46, 14, -28, -34, V(26, -16, 84), V(-2, 66, 112), V(9, 44, 9), V(-13, -36, 16)), V(0.6, -1, 0), V(-0.5, -0.2, 1))
P['CL'] = Knees(Mk(V(0, -10, 48), 10, 14, -50, -36, -10, 40, V(14, 20, 62), V(-22, -22, 40), V(8, 70, 14), V(-15, -30, 11)), V(0.3, 1, 0.6), V(-0.9, 0.5, 0.2))
P['CM'] = Knees(Mk(V(0, -4, 30), -36, -30, -60, -44, 30, 50, V(16, -6, 50), V(-24, -34, 14), V(6, 88, 14), V(-16, -10, 12)), V(0.3, 1, 0.6), V(-1, 0.4, 0.3))
P['CHW'] = Knees(Mk(V(0, -4, 44), 36, 46, -60, -40, -30, 50, V(14, 30, 70), V(-10, 10, 40), V(11, 28, 9), V(-15, -32, 11)), V(0.7, 1, 0.3), V(-0.9, 0.5, 0.2))
P['CHS'] = With(Mk(V(0, 8, 88), -10, -16, 40, 10, -18, -30, V(22, -14, 96), V(-6, 36, 186), V(9, 26, 12), V(-13, -24, 9)), V(0.6, -1, 0), V(-0.6, 0.4, -0.4))
# in the air
P['JL'] = Mk(V(0, 0, 95), 20, 26, -60, -40, -12, 50, V(6, 50, 96), V(-20, 0, 100), V(10, 20, 56), V(-10, -4, 48))
P['JM'] = Knees(Mk(V(0, 0, 95), -20, -16, -70, -60, 14, 60, V(20, 10, 120), V(-26, -20, 110), V(6, 74, 70), V(-10, -10, 52)), V(0.4, 0.6, 1), V(-0.9, 0.6, 0))
P['JH'] = Knees(Mk(V(0, 0, 95), -16, -12, 60, 70, 10, -50, V(26, -10, 118), V(-22, -26, 112), V(10, -10, 54), V(-4, 76, 70)), V(0.5, 1, 0), V(-0.4, 0.6, 1))
P['JD'] = With(Mk(V(0, 0, 100), 16, 20, -40, -30, -14, 30, V(30, 10, 142), V(-32, -10, 138), V(6, 40, 30), V(-10, -14, 74)), V(1, 0, -0.2), V(-1, 0, -0.2))
# specials
P['FireW'] = Mk(V(0, -12, 80), -8, -4, -80, -50, 4, 70, V(4, -4, 96), V(-14, -10, 92), V(9, 24, 9), V(-13, -28, 9))
P['FireS'] = With(Mk(V(0, 12, 72), 22, 26, -70, -30, -18, 60, V(6, 62, 112), V(-28, -40, 104), V(9, 46, 9), V(-13, -38, 12)), V(0.3, -0.2, -1), V(-0.6, -1, 0.3))
P['SurgeW'] = Mk(V(0, 4, 86), 4, 6, 30, 30, 0, -20, V(18, 20, 118), V(-22, 6, 110), V(9, 6, 9), V(-6, 10, 40))
P['SurgeS'] = Knees(Mk(V(0, 18, 86), -28, -24, 60, 64, 16, -50, V(24, -12, 116), V(-26, -24, 104), V(9, 6, 9), V(-2, 96, 100)), V(0.5, 1, 0), V(-0.4, 0.5, 1))
P['RiseS'] = Knees(With(Mk(V(0, 4, 96), -10, -16, 70, 40, -16, -50, V(24, -10, 100), V(-4, 26, 190), V(10, 26, 40), V(-10, 0, 14)), V(0.6, -1, 0), V(-0.6, 0.4, -0.4)), V(0.3, 1, 0.8), V(-0.9, 0.6, 0))
P['Vanish'] = With(Mk(V(0, 0, 84), 0, 0, -88, -60, 4, 80, V(8, 12, 112), V(-8, 10, 104), V(8, 10, 9), V(-10, -10, 9)), V(0.9, -0.2, -0.6), V(-0.9, -0.2, -0.6))
P['Whirl'] = Knees(With(Mk(V(0, 0, 95), -8, -6, -20, -10, 6, 10, V(44, 6, 128), V(-44, -6, 124), V(60, 0, 76), V(-8, 0, 44)), V(1, 0, 0.2), V(-1, 0, 0.2)), V(0.6, 0.2, 1), V(-0.6, 1, 0))
P['Dive'] = Knees(Mk(V(0, 0, 100), -30, -22, -80, -70, 22, 70, V(26, -16, 146), V(-30, -30, 130), V(4, 60, 34), V(-10, -16, 96)), V(0.3, 1, 0.4), V(-0.6, 1, 0.4))
# Slipstream: coil, then a dart with one hand speared ahead and the legs trailing
P['HomeW'] = With(Mk(V(0, -6, 56), 50, 58, -40, -26, -40, 30, V(16, -20, 80), V(-20, -30, 74), V(10, 24, 9), V(-13, -26, 10)), AB, ABr)
P['HomeS'] = Knees(With(Mk(V(0, 0, 90), 60, 66, -30, -20, -62, 20, V(8, 60, 150), V(-22, -40, 96), V(8, -30, 60), V(-10, -50, 54)), V(0.3, -0.2, -1), ABr), V(0.3, 0.4, -1), V(-0.3, 0.4, -1))
P['Grab'] = With(Mk(V(0, 6, 80), 14, 18, -30, -20, -8, 20, V(10, 52, 124), V(-10, 50, 112), V(9, 34, 9), V(-13, -28, 9)), V(0.6, -0.3, -1), V(-0.6, -0.3, -1))
P['Held'] = Mk(V(0, 0, 84), -14, -22, -40, -30, -20, 20, V(22, -6, 100), V(-24, -16, 92), V(9, 10, 12), V(-12, -14, 10))
P['SuperS'] = With(Mk(V(0, 20, 70), 40, 46, 40, 12, -34, -30, V(26, -30, 90), V(-2, 70, 112), V(9, 50, 9), V(-13, -40, 18)), AB, V(-0.5, -0.2, 1))
P['Win'] = With(Mk(V(0, 0, 90), -4, -2, -90, -80, 8, 80, V(10, 10, 108), V(-12, 6, 102), V(8, 8, 9), V(-9, -8, 9)), V(0.9, 0, -0.4), V(-0.9, 0, -0.4))

# the standing poses: sunk into the legs, lead hand held loose at chest height rather than thrust out
for _k, _dz in (('Idle', 0), ('Idle2', -3), ('WalkA', 2), ('WalkB', 0)):
    _p = dict(P[_k]); _h = _p['hip']; _p['hip'] = (_h[0], _h[1], min(_h[2], 77 + _dz)); _p['hl'] = (17, 30, 112 + _dz); _p['el'] = (0.7, -0.5, -1); _p['hr'] = (-22, -4, 92 + _dz); _p['er'] = (-0.8, -0.8, -0.2); P[_k] = _p

# ---------------------------------------------------------------------------------------------- moves
# wind-up pose, strike pose, follow-through ('-' for none), the limb steered onto the hitbox (1 left hand, 2 right hand,
# 3 left foot, 4 right foot), then the swing trail: the angle its bulge points on screen for a fighter facing right
# (0 forward, 90 up), its size in centimetres and its shape (3 thrust streak, 4 crescent, 2 ring).
M = {}
def mv(key, wind, strike, limb, arc=0, size=0, kind=4, rec='-', spin=0): M[key] = dict(wind=wind, strike=strike, rec=rec, limb=limb, arc=arc, size=size, kind=kind, spin=spin)
mv('S5L', 'Idle2', 'LS', 1, 0, 90, 3)
mv('S5M', 'MW', 'MS', 2, 10, 150)
mv('S5H', 'HW', 'HS', 2, -25, 210)
mv('S2L', 'Crouch', 'CL', 3, 0, 90, 3)
mv('S2M', 'Crouch', 'CM', 3, -8, 170)
mv('S2H', 'CHW', 'CHS', 2, 70, 230)
mv('JL', 'Jump', 'JL', 1, -10, 90, 3)
mv('JM', 'Jump', 'JM', 3, 0, 150)
mv('JH', 'Jump', 'JH', 4, -40, 200)
mv('J2H', 'Fall', 'JD', 3, -90, 210)
mv('FIRE', 'FireW', 'FireS', 0)
mv('SURGE', 'SurgeW', 'SurgeS', 4, 5, 240)
mv('RISE', 'CHW', 'RiseS', 2, 90, 260)
mv('VANISH', 'Vanish', 'Vanish', 0)
mv('WHIRL', 'Jump', 'Whirl', 0, 0, 230, 2)
mv('DIVE', 'Jump', 'Dive', 3, -60, 200)
mv('HOME', 'HomeW', 'HomeS', 1)
mv('THROW', 'Idle2', 'Grab', 0)
mv('SUPER1', 'MW', 'SuperS', 2, 0, 320)
mv('SUPER2', 'MW', 'SuperS', 2, 0, 380)
mv('SUPER3', 'MW', 'SuperS', 2, 0, 460)

def num(v): return ('%.3f' % v).rstrip('0').rstrip('.') if isinstance(v, float) else str(v)
def write(path):
    L = ['# GEKITOTSU: poses and move animation for the fighter. Written by tools/make_kagu.py; edit that, not this.']
    for k, p in P.items():
        t = ['pose', k]
        for f in ('hip', 'ln', 'ln2', 'sd', 'tw', 'pt', 'hd', 'hy', 'hl', 'hr', 'fl', 'fr', 'el', 'er', 'kl', 'kr', 'tl', 'tr', 'roll', 'spin'):
            v = p[f]; t.append(f); t += [num(x) for x in v] if isinstance(v, tuple) else [num(v)]
        L.append(' '.join(t))
    for k, m in M.items(): L.append('move %s wind %s strike %s rec %s limb %d arc %s size %s kind %d spin %s' % (k, m['wind'], m['strike'], m['rec'], m['limb'], num(m['arc']), num(m['size']), m['kind'], num(m['spin'])))
    open(path, 'w').write('\n'.join(L) + '\n'); print('wrote', path, len(P), 'poses', len(M), 'moves')
if __name__ == '__main__': write(sys.argv[1] if len(sys.argv) > 1 else 'kagu.txt')
