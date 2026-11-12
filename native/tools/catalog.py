# catalog.py <name> <n samples> <regex> [regex...] : a script that shows every matching robot clip at n moments
import sys, re, os
P = os.path.expanduser('~/mnt/Epic Games/GekitotsuNative/Saved/')
clips = [l.split('|')[0] for l in open(P + 'manny_clips.txt').read().split('\n') if l]
name = sys.argv[1]; n = int(sys.argv[2]); out = []; k = 0
for c in clips:
    if not any(re.search(r, c) for r in sys.argv[3:]): continue
    base = c.split('/')[-1]; short = re.sub(r'^A_UnarmedFightingAnimations?V._|^Anim_SC_|^A_|^SpecialMove_|_Manny$|_IP', '', base)
    tag = 'S1' if 'Style_One' in c else 'S2' if 'Style_Two' in c else re.search(r'V(\d)', c).group(0) if 'Unarm' in c else ''
    for j in range(n):
        out += ['lab|idle|%d|%s.%s|frac' % (int(100 * (j + 0.5) / n), c, base), 'wait|1', 'shot|%s_%03d_%s%s_%d' % (name, k, short, tag, j)]
    k += 1
out.append('quit'); open(P + 'script_%s.txt' % name, 'w').write('\n'.join(out) + '\n'); print(k, 'clips', len(out))
