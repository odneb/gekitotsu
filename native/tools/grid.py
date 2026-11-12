# grid.py <prefix> [cols] [cell width]: every still in Saved/shots whose name starts with the prefix, whole frame, in a grid -> Saved/sheets/<prefix>_grid_NN.jpg
import sys, os, glob
from PIL import Image
P = os.path.expanduser('~/mnt/Epic Games/GekitotsuNative/Saved/')
pre = sys.argv[1]; cols = int(sys.argv[2]) if len(sys.argv) > 2 else 2; cw = int(sys.argv[3]) if len(sys.argv) > 3 else 640; per = int(sys.argv[4]) if len(sys.argv) > 4 else 6
fs = sorted(glob.glob(P + 'shots/' + pre + '*.png')); os.makedirs(P + 'sheets', exist_ok=True)
for s in range(0, len(fs), per):
    part = fs[s:s + per]; ims = [Image.open(f).convert('RGB') for f in part]; ch = int(cw * ims[0].size[1] / ims[0].size[0]); rows = (len(part) + cols - 1) // cols
    sh = Image.new('RGB', (cols * cw, rows * ch), (10, 10, 10))
    for k, im in enumerate(ims): sh.paste(im.resize((cw, ch)), ((k % cols) * cw, (k // cols) * ch))
    sh.save(P + 'sheets/%s_grid_%02d.jpg' % (pre, s // per), quality=82)
print(len(fs), 'stills')
