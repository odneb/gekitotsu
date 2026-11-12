# Turns the generated, pre-shaded colour texture into what a cel shader wants: flat colour per material, a shade
# colour per material, and the thin dark creases kept as ink lines. Writes Kagu_Flat.png / Shiro_Flat.png (lit colour,
# alpha = ink line) and Kagu_Shade.png / Shiro_Shade.png (the colour each area takes in shadow).
import numpy as np, sys
from PIL import Image, ImageFilter
Image.MAX_IMAGE_PIXELS = None
src = Image.open(sys.argv[1]).convert('RGB'); N = int(sys.argv[3]) if len(sys.argv) > 3 else 2048
im = src.resize((N, N), Image.LANCZOS); a = np.asarray(im).astype(np.float32) / 255.0
r, g, b = a[..., 0], a[..., 1], a[..., 2]; mx = a.max(-1); mn = a.min(-1); lum = 0.3 * r + 0.5 * g + 0.2 * b
red = (r - np.maximum(g, b)) > 0.10
white = (mx > 0.5) & ((mx - mn) < 0.16)
cls = np.where(white, 2, np.where(red, 1, 0)).astype(np.uint8)        # 0 black cloth, 1 red cloth, 2 bone-white mask and wraps
# clean speckle: a mode filter over a small window
cls = np.asarray(Image.fromarray(cls * 100).filter(ImageFilter.ModeFilter(7))) // 100
# bleed every island's material out past its edge, so texture filtering never pulls the gutter colour onto the model
import os
cv = sys.argv[4] if len(sys.argv) > 4 else ''
if cv and os.path.exists(cv):
    from scipy import ndimage
    covered = np.asarray(Image.open(cv).convert('L').resize((N, N), Image.NEAREST)) > 127
    covered = ndimage.binary_erosion(covered, iterations=1)
    idx = ndimage.distance_transform_edt(~covered, return_distances=False, return_indices=True)
    cls = cls[idx[0], idx[1]]; print('bled', float((~covered).mean().round(3)))
# ink: thin features much darker than their surroundings, inside the red and white areas
blur_s = np.asarray(Image.fromarray((lum * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.2))).astype(np.float32) / 255
blur_l = np.asarray(Image.fromarray((lum * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(9))).astype(np.float32) / 255
ink = np.clip((blur_l * 0.72 - blur_s) / np.maximum(blur_l, 0.05) * 6.0, 0, 1) * (cls > 0) * (np.where(white, 2, np.where(red, 1, 0)) > 0)
ink = np.asarray(Image.fromarray((ink * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.7))).astype(np.float32) / 255
# black cloth is drawn as charcoal blue, never true black: the fighter has to stay brighter than the stage behind him
PAL = {'Kagu': ([(0.115, 0.105, 0.150), (0.860, 0.090, 0.055), (0.940, 0.910, 0.860)], [(0.045, 0.040, 0.085), (0.460, 0.030, 0.100), (0.580, 0.520, 0.660)]),
       'Shiro': ([(0.230, 0.250, 0.320), (0.930, 0.940, 0.960), (0.970, 0.970, 0.970)], [(0.095, 0.110, 0.200), (0.500, 0.590, 0.820), (0.540, 0.620, 0.840)])}
out = sys.argv[2]
for name, (lit, shade) in PAL.items():
    L = np.array(lit, np.float32)[cls]; S = np.array(shade, np.float32)[cls]
    Image.fromarray((np.dstack([L, (1 - ink)[..., None]]) * 255).astype(np.uint8), 'RGBA').save('%s/%s_Flat.png' % (out, name))
    Image.fromarray((S * 255).astype(np.uint8), 'RGB').save('%s/%s_Shade.png' % (out, name))
prev = np.array(PAL['Kagu'][0], np.float32)[cls] * (1 - ink[..., None] * 0.9)
Image.fromarray((prev * 255).astype(np.uint8)).resize((900, 900)).save(out + '/flat_view.jpg', quality=85)
print('classes', [float((cls == k).mean().round(3)) for k in range(3)], 'ink', float((ink > 0.5).mean().round(4)))
