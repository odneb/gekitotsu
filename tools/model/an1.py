import numpy as np
d=np.load('/home/claude/mtools/ch.npz',allow_pickle=True)
pos=d['pos']; idx=d['idx'].astype(np.int64); ji=d['ji']; jw=d['jw']; names=list(d['names']); ibm=d['ibm']
# weld
key=np.round(pos*20000).astype(np.int64); _,inv=np.unique(key,axis=0,return_inverse=True); inv=inv.ravel()
nw=inv.max()+1; print('verts',len(pos),'welded',nw,'tris',len(idx))
# components via union-find on welded
par=np.arange(nw)
def find(x):
    while par[x]!=x:
        par[x]=par[par[x]]; x=par[x]
    return x
for t in idx:
    a,b,c=inv[t]; ra,rb,rc=find(a),find(b),find(c)
    if ra!=rb: par[rb]=ra
    ra=find(a)
    if rc!=ra: par[rc]=ra
roots=np.array([find(i) for i in range(nw)]); u,cnt=np.unique(roots,return_counts=True)
print('components',len(u)); order=np.argsort(-cnt)
wp=np.zeros((nw,3)); wp[inv]=pos
for o in order[:25]:
    m=roots==u[o]; p=wp[m]; print(cnt[o],'bbox',np.round(p.min(0),2),np.round(p.max(0),2))
# bind joint positions: inverse of IBM (column-major in glTF -> transpose)
J=[np.linalg.inv(m.T) for m in ibm]
for n,m in zip(names,J): print(n,np.round(m[:3,3],3), 'scale',np.round(np.linalg.norm(m[:3,0]),4))
# dominant bone per vertex
dom=ji[np.arange(len(pos)),jw.argmax(1)]; 
for k,n in enumerate(names): print(n,(dom==k).sum(),end='; ')
