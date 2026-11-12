# contact sheets from Saved/shots: sheet.py <prefix> [rows per sheet] [crop fraction of width]  -> Saved/sheets/<prefix>_NN.jpg
import sys, os, glob, re
from PIL import Image, ImageDraw
P = os.path.expanduser('~/mnt/Epic Games/GekitotsuNative/Saved/')
prefix = sys.argv[1]; rows = int(sys.argv[2]) if len(sys.argv) > 2 else 6; cf = float(sys.argv[3]) if len(sys.argv) > 3 else 0.42
files = sorted(glob.glob(P + 'shots/' + prefix + '*.png'), key=os.path.getmtime)
groups = {}; order = []
for f in files:
    m = re.match(r"(.*)_(\d+)\.png$", os.path.basename(f))
    if not m: continue
    if m.group(1) not in groups: groups[m.group(1)] = []; order.append(m.group(1))
    groups[m.group(1)].append(f)
os.makedirs(P + 'sheets', exist_ok=True)
for old in glob.glob(P + 'sheets/' + prefix + '_*.jpg'): os.rename(old, old + '.old')
CW = int(sys.argv[4]) if len(sys.argv) > 4 else 250; CH = int(sys.argv[5]) if len(sys.argv) > 5 else 250
for s in range(0, len(order), rows):
    part = order[s:s + rows]; n = max(len(groups[g]) for g in part)
    sheet = Image.new('RGB', (n * CW, len(part) * CH), (20, 20, 20)); d = ImageDraw.Draw(sheet)
    for r, g in enumerate(part):
        for c, f in enumerate(groups[g]):
            im = Image.open(f).convert('RGB'); w, h = im.size; cw = int(w * cf); im = im.crop(((w - cw) // 2, 0, (w + cw) // 2, h)); im.thumbnail((CW, CH)); sheet.paste(im, (c * CW + (CW - im.size[0]) // 2, r * CH))
        d.text((6, r * CH + 4), g[len(prefix):].strip('_'), fill=(255, 220, 60))
    sheet.save(P + 'sheets/%s_%02d.jpg' % (prefix, s // rows), quality=80)
print(len(order), 'groups', (len(order) + rows - 1) // rows, 'sheets')
