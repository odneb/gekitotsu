import sys, json, struct, numpy as np
fn=sys.argv[1]; out=sys.argv[2]
f=open(fn,'rb'); f.read(12); clen,_=struct.unpack('<I4s',f.read(8)); js=json.loads(f.read(clen)); blen,_=struct.unpack('<I4s',f.read(8)); bin_=f.read(blen)
acc=js['accessors']; bv=js['bufferViews']
CT={5120:np.int8,5121:np.uint8,5122:np.int16,5123:np.uint16,5125:np.uint32,5126:np.float32}; NC={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}
def get(i):
    a=acc[i]; v=bv[a['bufferView']]; dt=CT[a['componentType']]; n=NC[a['type']]; off=v.get('byteOffset',0)+a.get('byteOffset',0); st=v.get('byteStride')
    if st and st!=np.dtype(dt).itemsize*n:
        arr=np.array([np.frombuffer(bin_,dt,n,off+k*st) for k in range(a['count'])])
    else: arr=np.frombuffer(bin_,dt,a['count']*n,off).reshape(a['count'],n)
    return arr
p=js['meshes'][0]['primitives'][0]; A=p['attributes']
d=dict(pos=get(A['POSITION']),nor=get(A['NORMAL']),uv=get(A['TEXCOORD_0']),ji=get(A['JOINTS_0']),jw=get(A['WEIGHTS_0']),idx=get(p['indices']).reshape(-1,3),ibm=get(js['skins'][0]['inverseBindMatrices']).reshape(-1,4,4))
names=[js['nodes'][j].get('name') for j in js['skins'][0]['joints']]
par=[]
jn=js['skins'][0]['joints']
for j in jn:
    pp=-1
    for k,n in enumerate(js['nodes']):
        if j in n.get('children',[]): pp=jn.index(k) if k in jn else -1
    par.append(pp)
np.savez(out,names=np.array(names),par=np.array(par),**d)
for i,im in enumerate(js.get('images',[])):
    v=bv[im['bufferView']]; open(out.replace('.npz','_img%d.png'%i),'wb').write(bin_[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']])
print(names,par); print({k:(v.shape,v.dtype) for k,v in d.items()})
