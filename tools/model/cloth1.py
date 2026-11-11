import numpy as np, json, heapq
from PIL import Image
d=np.load('/home/claude/mtools/ch.npz',allow_pickle=True)
pos=d['pos'].astype(np.float64); idx=d['idx'].astype(np.int64); ji=d['ji']; jw=d['jw']; names=list(d['names']); ibm=d['ibm']; uv=d['uv']
J=np.array([np.linalg.inv(m.T)[:3,3] for m in ibm]); N={n:i for i,n in enumerate(names)}
tex=np.asarray(Image.open('/home/claude/mtools/ch_img1.png').convert('RGB').resize((1024,1024),Image.LANCZOS)).astype(np.float64)/255
u=np.clip((uv[:,0]%1)*1023,0,1023).astype(int); v=np.clip((uv[:,1]%1)*1023,0,1023).astype(int); col=tex[v,u]
key=np.round(pos*20000).astype(np.int64); _,first,inv=np.unique(key,axis=0,return_index=True,return_inverse=True); inv=inv.ravel(); nw=inv.max()+1
wp=pos[first]; wc=np.zeros((nw,3)); cnt=np.zeros(nw); np.add.at(wc,inv,col); np.add.at(cnt,inv,1); wc/=cnt[:,None]
# welded skin: accumulate per-bone weights
W=np.zeros((nw,len(names))); 
for k in range(4): np.add.at(W,(inv,ji[:,k]),jw[:,k])
W/=W.sum(1,keepdims=True)
tri=inv[idx]; tri=tri[(tri[:,0]!=tri[:,1])&(tri[:,1]!=tri[:,2])&(tri[:,0]!=tri[:,2])]
red=(wc[:,0]>0.16)&(wc[:,0]>wc[:,1]*2.0)&(wc[:,0]>wc[:,2]*2.0)
grp=lambda ns: W[:,[N[n] for n in ns]].sum(1)
wHead=grp(['Head','head_end','headfront','neck']); wArm=grp(['LeftArm','LeftForeArm','LeftHand','RightArm','RightForeArm','RightHand','LeftShoulder','RightShoulder']); wLeg=grp(['Hips','LeftUpLeg','RightUpLeg','LeftLeg','RightLeg']); wFoot=grp(['LeftFoot','RightFoot','LeftToeBase','RightToeBase']); wSp=grp(['Spine02','Spine01','Spine'])
def segd(p,a,b):
    ab=b-a; t=np.clip(((p-a)@ab)/(ab@ab),0,1); return np.linalg.norm(p-(a+t[:,None]*ab),axis=1)
print('red verts',red.sum(),'of',nw)
print('y range red by group: head',(red&(wHead>.5)).sum(),'arm',(red&(wArm>.5)).sum(),'leg',(red&(wLeg>.5)).sum(),'foot',(red&(wFoot>.5)).sum(),'spine',(red&(wSp>.5)).sum())
np.savez('/home/claude/mtools/weld.npz',wp=wp,wc=wc,W=W,tri=tri,inv=inv,red=red,J=J)
# debug render data: colour by group for red verts
c=np.tile(np.array([[0.75,0.75,0.75]]),(nw,1)); c[~red]*=wc[~red].mean(1,keepdims=True)*2+0.25
c[red&(wHead>.5)]=[0.1,0.5,1]; c[red&(wArm>.5)]=[0.1,0.8,0.2]; c[red&(wLeg>.5)]=[1,0.2,0.1]; c[red&(wFoot>.5)]=[1,0.8,0]; c[red&(wSp>.5)]=[0.8,0.2,0.9]
json.dump({'pos':wp.round(4).ravel().tolist(),'idx':tri.ravel().tolist(),'col':c.round(3).ravel().tolist(),'J':J.round(4).tolist()},open('/home/claude/mview/dbg.json','w'))
