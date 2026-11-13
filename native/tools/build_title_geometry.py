"""Author opaque extruded title geometry. No texture is used by the resulting meshes.
Requires fonttools, shapely, numpy, mapbox-earcut, pillow, opencv-python-headless.
"""
from pathlib import Path
from fontTools.ttLib import TTFont
from fontTools.pens.basePen import BasePen
from shapely.geometry import Polygon, box
from shapely.ops import unary_union
from shapely import affinity
import numpy as np, mapbox_earcut as ear, cv2
from PIL import Image
root=Path(__file__).resolve().parents[1]; out=root/'Art/Identity/Geometry';out.mkdir(exist_ok=True)
class Pen(BasePen):
 def __init__(self,gs):super().__init__(gs);self.rings=[];self.r=[]
 def _moveTo(self,p):self.r=[p]
 def _lineTo(self,p):self.r.append(p)
 def _qCurveToOne(self,b,c):
  a=self.r[-1]
  for t in np.linspace(0,1,9)[1:]:self.r.append(tuple((1-t)**2*np.array(a)+2*(1-t)*t*np.array(b)+t*t*np.array(c)))
 def _curveToOne(self,b,c,d):
  a=self.r[-1]
  for t in np.linspace(0,1,12)[1:]:self.r.append(tuple((1-t)**3*np.array(a)+3*(1-t)**2*t*np.array(b)+3*(1-t)*t*t*np.array(c)+t**3*np.array(d)))
 def _closePath(self):self.rings.append(self.r)
 def _endPath(self):self._closePath()
def polys(g):return [g] if g.geom_type=='Polygon' else [p for p in g.geoms if p.geom_type=='Polygon']
def export(name,g,depth):
 verts=[];faces=[]
 def tri(a,b,c):
  n=len(verts);verts.extend([a,b,c]);faces.append((n+1,n+2,n+3))
 for p in polys(g):
  if p.area<.015:continue
  from shapely.geometry.polygon import orient
  p=orient(p,1);rings=[np.array(p.exterior.coords[:-1])]+[np.array(r.coords[:-1]) for r in p.interiors];v=np.concatenate(rings);ends=np.cumsum([len(r) for r in rings],dtype=np.uint32)
  ix=ear.triangulate_float64(v,ends).reshape(-1,3)
  for a,b,c in ix:
   tri((-depth,*v[c]),(-depth,*v[b]),(-depth,*v[a]));tri((0,*v[a]),(0,*v[b]),(0,*v[c]))
  for r in rings:
   for a,b in zip(r,np.roll(r,-1,axis=0)):
    tri((-depth,*a),(-depth,*b),(0,*b));tri((-depth,*a),(0,*b),(0,*a))
 # OBJ coordinates use X depth, Y horizontal, Z vertical; import without axis conversion.
 with (out/(name+'.obj')).open('w') as f:
  f.write('o '+name+'\n')
  for x,y,z in verts:f.write(f'v {x:.4f} {-y:.4f} {z:.4f}\n')
  f.write('vt 0 0\n')
  for a,b,c in faces:f.write(f'f {a}/1 {c}/1 {b}/1\n')
 print(name,len(faces),'triangles')
# Bespoke mechanical letterforms: clipped corners, heavy stems and angular counters.
def chamfer(x,y,w,h,c):
 return Polygon([(x+c,y),(x+w-c,y),(x+w,y+c),(x+w,y+h-c),(x+w-c,y+h),(x+c,y+h),(x,y+h-c),(x,y+c)])
def glyph(ch):
 outer=chamfer(0,0,80,160,10)
 if ch=='G':return outer.difference(box(24,26,56,134)).difference(box(49,89,90,134)).union(box(44,63,80,89))
 if ch=='E':return unary_union([box(0,0,24,160),box(0,0,78,26),box(0,68,65,94),box(0,134,80,160)])
 if ch=='K':return unary_union([box(0,0,24,160),Polygon([(20,70),(51,160),(80,160),(47,80),(80,0),(51,0)])])
 if ch=='I':return box(0,0,28,160)
 if ch=='T':return unary_union([box(0,134,80,160),box(27,0,53,160)])
 if ch=='O':return outer.difference(chamfer(24,26,32,108,4))
 if ch=='S':return Polygon([(10,160),(80,160),(80,134),(24,134),(24,94),(68,94),(80,82),(80,10),(70,0),(0,0),(0,26),(56,26),(56,68),(12,68),(0,80),(0,150)])
 if ch=='U':return outer.difference(box(24,28,56,170))
items=[];cursor=0
for ch in 'GEKITOTSU':
 g=glyph(ch);items.append(affinity.translate(g,xoff=cursor));cursor+=(28 if ch=='I' else 80)+5
word=unary_union(items)
for x in [65,280,480]:
 crack=Polygon([(x-17,0),(x-15,0),(x+8,52),(x+3,67),(x+30,119),(x+32,160),(x+28,160),(x+27,121),(x,68),(x+4,53)])
 word=word.difference(crack)
word=affinity.skew(word,xs=9,origin=(0,0));xmin,ymin,xmax,ymax=word.bounds
word=affinity.scale(word,xfact=200/(xmax-xmin),yfact=55/160,origin=(0,0));word=affinity.translate(word,xoff=-100,yoff=-27.5)
export('SM_GkTitleLatin',word,4)
a=np.array(Image.open(root/'Art/Identity/Gekitotsu-Impact-Logo.png').convert('RGBA'))
mask=((a[:,:,0]>150)&(a[:,:,1]<75)&(a[:,:,2]<70)&(a[:,:,3]>200)).astype('uint8')*255
cont,h=cv2.findContours(mask,cv2.RETR_CCOMP,cv2.CHAIN_APPROX_SIMPLE);shapes=[]
for i,c in enumerate(cont):
 if h[0,i,3]>=0 or cv2.contourArea(c)<100:continue
 def ring(c):return [(float(p[0][0])/a.shape[1]*200-100, (0.5-float(p[0][1])/a.shape[0])*65) for p in cv2.approxPolyDP(c,1.2,True)]
 holes=[];j=h[0,i,2]
 while j!=-1:
  if len(ring(cont[j]))>2:holes.append(ring(cont[j]))
  j=h[0,j,0]
 shapes.append(Polygon(ring(c),holes).buffer(0))
export('SM_GkTitleKanji',unary_union(shapes),4)
