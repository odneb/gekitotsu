import numpy as np, json, heapq
d=np.load('/home/claude/mtools/weld.npz'); wp=d['wp']; wc=d['wc']; W=d['W']; tri=d['tri']; red=d['red']; J=d['J']
ch=np.load('/home/claude/mtools/ch.npz',allow_pickle=True); names=list(ch['names']); N={n:i for i,n in enumerate(names)}
nw=len(wp)
def segd(p,a,b):
    ab=b-a; t=np.clip(((p-a)@ab)/(ab@ab),0,1); return np.linalg.norm(p-(a+t[:,None]*ab),axis=1)
grp=lambda ns: W[:,[N[n] for n in ns]].sum(1)
wHead=grp(['Head','head_end','headfront','neck']); wArm=grp(['LeftArm','LeftForeArm','LeftHand','RightArm','RightForeArm','RightHand','LeftShoulder','RightShoulder']); wLeg=grp(['Hips','LeftUpLeg','RightUpLeg','LeftLeg','RightLeg']); wFoot=grp(['LeftFoot','RightFoot','LeftToeBase','RightToeBase']); wSp=grp(['Spine02','Spine01','Spine'])
P=lambda n: J[N[n]]
dArm=np.minimum.reduce([segd(wp,P(s+'Arm'),P(s+'ForeArm'))-0.085 for s in ('Left','Right')]+[segd(wp,P(s+'ForeArm'),P(s+'Hand'))-0.07 for s in ('Left','Right')]+[segd(wp,P(s+'Hand'),P(s+'Hand')+(P(s+'Hand')-P(s+'ForeArm'))*0.6)-0.085 for s in ('Left','Right')])
dTor=np.minimum.reduce([segd(wp,P('Hips'),P('Spine'))-0.21, segd(wp,P('LeftArm'),P('RightArm'))-0.1, segd(wp,P('Spine'),P('Head'))-0.12])
hc=P('Head')+np.array([0,0.11,0.0]); dHead=np.linalg.norm(wp-hc,axis=1)-0.15
dLeg=np.minimum.reduce([segd(wp,P(s+'Leg'),P(s+'Foot'))-0.075 for s in ('Left','Right')])
y=wp[:,1]
upper=red&(wHead+wArm+wSp>0.5)&(dArm>0)&(dTor>0)&(dHead>0)
coat=red&(wLeg+wFoot>0.5)&(y<0.985)&(y>0.17)&~((y<0.45)&(dLeg<0))
cloth=upper|coat
# adjacency
adj=[set() for _ in range(nw)]
for a,b,c in tri: adj[a]|={b,c}; adj[b]|={a,c}; adj[c]|={a,b}
# fill: non-cloth with most neighbours cloth
for it in range(2):
    add=[i for i in range(nw) if not cloth[i] and len(adj[i])>0 and sum(cloth[j] for j in adj[i])>=0.7*len(adj[i])]
    cloth[add]=True
# remove: cloth with almost no cloth neighbours
rem=[i for i in range(nw) if cloth[i] and sum(cloth[j] for j in adj[i])<=1]; cloth[rem]=False
# components
comp=-np.ones(nw,int); nc=0
for s in range(nw):
    if cloth[s] and comp[s]<0:
        st=[s]; comp[s]=nc
        while st:
            a=st.pop()
            for b in adj[a]:
                if cloth[b] and comp[b]<0: comp[b]=nc; st.append(b)
        nc+=1
sizes=np.bincount(comp[comp>=0]); print('cloth verts',cloth.sum(),'components',nc, sorted(sizes.tolist(),reverse=True)[:40])
for c in np.argsort(-sizes)[:22]:
    m=comp==c; p=wp[m]; print(c,sizes[c],'bbox',np.round(p.min(0),2),np.round(p.max(0),2),'head',round(wHead[m].mean(),2),'arm',round(wArm[m].mean(),2),'leg',round(wLeg[m].mean(),2))
np.savez('/home/claude/mtools/cloth.npz',cloth=cloth,comp=comp)
rng=np.random.RandomState(3); pal=rng.rand(nc,3)*0.8+0.2
c=np.tile(np.array([[0.55,0.55,0.55]]),(nw,1))*(wc.mean(1,keepdims=True)*1.5+0.3); c[cloth]=pal[comp[cloth]]
json.dump({'pos':wp.round(4).ravel().tolist(),'idx':tri.ravel().tolist(),'col':c.round(3).ravel().tolist()},open('/home/claude/mview/dbg.json','w'))
