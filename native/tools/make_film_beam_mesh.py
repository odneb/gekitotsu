from pathlib import Path
import math
out=[]; rings=65;sides=32
for j in range(rings):
 t=j/(rings-1);profile=.18+.82*min(1,t/.06);profile*=1-.12*t
 for i in range(sides+1):
  a=i/sides*2*math.pi;r=50*profile
  out.append(f'v {r*math.cos(a):.6f} {r*math.sin(a):.6f} {100*t-50:.6f}')
for j in range(rings):
 for i in range(sides+1):out.append(f'vt {i/sides:.6f} {j/(rings-1):.6f}')
for j in range(rings):
 for i in range(sides+1):
  a=i/sides*2*math.pi;out.append(f'vn {math.cos(a):.6f} {math.sin(a):.6f} 0')
out.append('s 1')
for j in range(rings-1):
 for i in range(sides):
  a=j*(sides+1)+i+1;b=a+1;c=a+sides+1;d=c+1
  for tri in [(a,b,c),(b,d,c)]:out.append('f '+' '.join(f'{n}/{n}/{n}' for n in tri))
dest=Path(__file__).resolve().parents[1]/'Saved/FilmAssets';dest.mkdir(parents=True,exist_ok=True)
(dest/'SM_GkFilmBeam.obj').write_text('\n'.join(out))
