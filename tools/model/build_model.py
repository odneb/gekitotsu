import numpy as np, json, heapq, base64, io, sys
from PIL import Image
ch=np.load('/home/claude/mtools/ch.npz',allow_pickle=True); names=list(ch['names']); N={n:i for i,n in enumerate(names)}; par=ch['par'].tolist()
pos=ch['pos'].astype(np.float64); nor=ch['nor'].astype(np.float64); uv=ch['uv'].astype(np.float64); idx=ch['idx'].astype(np.int64); ji=ch['ji'].astype(np.int64); jw=ch['jw'].astype(np.float64)
d=np.load('/home/claude/mtools/weld.npz'); wp=d['wp']; W=d['W']; tri=d['tri']; inv=d['inv']; J=d['J']
cl=np.load('/home/claude/mtools/cloth.npz'); cloth=cl['cloth'].copy(); comp=cl['comp'].copy()
nw=len(wp); NB=len(names)
adj=[set() for _ in range(nw)]
for a,b,c in tri: adj[a]|={b,c}; adj[b]|={a,c}; adj[c]|={a,b}
sizes=np.bincount(comp[comp>=0])
for c in range(len(sizes)):
    if sizes[c]<20: cloth[comp==c]=False; comp[comp==c]=-1
grp=lambda ns: W[:,[N[n] for n in ns]].sum(1)
wLeg=grp(['Hips','LeftUpLeg','RightUpLeg','LeftLeg','RightLeg','LeftFoot','RightFoot'])
coat=cloth&(wLeg>0.5)
bones=[[n]+J[i].tolist() for i,n in enumerate(names)]   # name, x,y,z (bind, metres)
chains=[]   # dict(a=anchor bone, j=[bone idx], kind)
newSI=np.zeros((nw,4),int); newSW=np.zeros((nw,4))
def addbone(p,name): bones.append([name]+[float(x) for x in p]); return len(bones)-1
# ---- coat: sectors round the hips, bands down from the belt
K=10; cx,cz=J[N['Hips']][0],J[N['Hips']][2]; y0=0.985; r0=0.13; SB=[0,0.14,0.3,0.48,0.68,0.9]
ci=np.where(coat)[0]; th=np.arctan2(wp[ci,0]-cx,wp[ci,2]-cz); ring=np.stack([cx+r0*np.sin(th),np.full(len(ci),y0),cz+r0*np.cos(th)],1); s=np.linalg.norm(wp[ci]-ring,axis=1)
cen=(np.arange(K)+0.5)/K*2*np.pi-np.pi; sw=2*np.pi/K
cb=[]  # bone idx per sector/band
for i in range(K):
    dth=np.abs((th-cen[i]+np.pi)%(2*np.pi)-np.pi); js=[]; pts=[]
    for k,sk in enumerate(SB):
        if k==0: p=np.array([cx+r0*np.sin(cen[i]),y0,cz+r0*np.cos(cen[i])])
        else:
            hb=(SB[k]-SB[k-1])*0.55; m=(dth<sw*0.75)&(np.abs(s-sk)<hb)
            if m.sum()>=3: p=wp[ci[m]].mean(0)
            else:
                dirv=(pts[-1]-pts[-2]) if len(pts)>1 else np.array([np.sin(cen[i])*0.5,-0.85,np.cos(cen[i])*0.5]); dirv=dirv/np.linalg.norm(dirv); p=pts[-1]+dirv*(SB[k]-SB[k-1])
        pts.append(p); js.append(addbone(p,'coat%d_%d'%(i,k)))
    cb.append(js); chains.append(dict(a=int(N['Hips']),j=[int(x) for x in js],kind=0))
for n_,v in enumerate(ci):
    t=(th[n_]-cen[0])/sw; i0=int(np.floor(t))%K; ft=t-np.floor(t); i1=(i0+1)%K
    k0=max(0,np.searchsorted(SB,s[n_])-1); k0=min(k0,len(SB)-2); fs=np.clip((s[n_]-SB[k0])/(SB[k0+1]-SB[k0]),0,1)
    newSI[v]=[cb[i0][k0],cb[i0][k0+1],cb[i1][k0],cb[i1][k0+1]]; newSW[v]=[(1-ft)*(1-fs),(1-ft)*fs,ft*(1-fs),ft*fs]
# ---- strands: one chain each, laid along the distance from where it joins the body
for c in range(len(sizes)):
    m=(comp==c)&cloth&~coat
    if m.sum()<20: 
        cloth[m]=False; continue
    vs=np.where(m)[0]; root=[v for v in vs if any(not cloth[b] for b in adj[v])]
    if not root: root=[vs[np.argmax(wp[vs,1])]]
    dist={v:np.inf for v in vs}; pq=[]
    for r in root: dist[r]=0; heapq.heappush(pq,(0,r))
    while pq:
        dd,a=heapq.heappop(pq)
        if dd>dist[a]: continue
        for b in adj[a]:
            if b in dist:
                nd=dd+np.linalg.norm(wp[a]-wp[b])
                if nd<dist[b]: dist[b]=nd; heapq.heappush(pq,(nd,b))
    dv=np.array([dist[v] for v in vs]); dv[~np.isfinite(dv)]=dv[np.isfinite(dv)].max(); L=dv.max()
    n=int(min(7,max(3,round(L/0.12)+1))); sk=np.linspace(0,L,n); hb=L/(n-1)*0.55
    rc=wp[root].mean(0); pts=[]; js=[]
    for k in range(n):
        mm=np.abs(dv-sk[k])<hb; p=rc if k==0 else (wp[vs[mm]].mean(0) if mm.sum()>=1 else pts[-1]+(pts[-1]-pts[-2]))
        pts.append(p); js.append(addbone(p,'str%d_%d'%(c,k)))
    cand=['Head'] if np.linalg.norm(rc-J[N['Head']])<0.3 or rc[1]>1.42 else ['Spine','Spine01','Spine02','Hips']
    a=min(cand,key=lambda nm: np.linalg.norm(rc-J[N[nm]]))
    chains.append(dict(a=int(N[a]),j=[int(x) for x in js],kind=1 if a=='Head' else 2)); print('strand',c,'verts',len(vs),'len',round(L,2),'joints',n,'anchor',a,'root',np.round(rc,2))
    for q,v in enumerate(vs):
        k0=int(min(n-2,max(0,np.searchsorted(sk,dv[q])-1))); fs=np.clip((dv[q]-sk[k0])/(sk[k0+1]-sk[k0]),0,1)
        newSI[v]=[js[k0],js[k0+1],0,0]; newSW[v]=[1-fs,fs,0,0]
print('bones',len(bones),'chains',len(chains),'cloth verts',cloth.sum())
# ---- per original vertex skin
SI=np.zeros((len(pos),4),int); SWt=np.zeros((len(pos),4))
KNEE={}; fixed=0
for S in ['Left','Right']:
    k=J[N[S+'Leg']]; ax=J[N[S+'Foot']]-k; KNEE[S]=(N[S+'UpLeg'],N[S+'Leg'],k,ax/np.linalg.norm(ax))
for v in range(len(pos)):
    w=inv[v]
    if cloth[w]: SI[v]=newSI[w]; SWt[v]=newSW[w]
    else:
        dw=np.zeros(NB); np.add.at(dw,ji[v],jw[v])
        # The auto-rig put the thigh-to-shin blend 10 cm below the knee joint, so the top of the shin followed the thigh and tore
        # away from the cuff when the knee bent. Re-centre that blend on the joint itself, the way the elbows and ankles already are.
        for S in KNEE:
            u,l,k,ax=KNEE[S]; tt=dw[u]+dw[l]
            if tt>0.05:
                sp=(pos[v]-k)@ax; rr=np.linalg.norm((pos[v]-k)-sp*ax)
                if rr<0.3: f=np.clip((sp+0.06)/0.13,0,1); f=f*f*(3-2*f); dw[u]=tt*(1-f); dw[l]=tt*f; fixed+=1
        o=np.argsort(-dw)[:4]; SI[v]=o; SWt[v]=dw[o]
print('knee re-weighted',fixed); SWt/=SWt.sum(1,keepdims=True); sw8=np.round(SWt*255).astype(int)
for v in range(len(pos)): sw8[v,np.argmax(sw8[v])]+=255-sw8[v].sum()
assert len(bones)<256
# ---- texture: pad chart edges so nothing bleeds, lift the blacks to a dark plum, shrink
T=1024; tex=np.asarray(Image.open('/home/claude/mtools/ch_img1.png').convert('RGB').resize((2048,2048),Image.LANCZOS)).astype(np.float64)/255
from PIL import ImageDraw
mask=Image.new('L',(2048,2048),0); dr=ImageDraw.Draw(mask)
for t in idx: dr.polygon([(float(uv[v,0]%1)*2048,float(uv[v,1]%1)*2048) for v in t],fill=255,outline=255)
mk=np.asarray(mask)>0
import scipy.ndimage as ndi
ii=ndi.distance_transform_edt(~mk,return_distances=False,return_indices=True); tex=tex[ii[0],ii[1]]
lum=tex.mean(2,keepdims=True); tex=tex+np.power(np.clip(1-lum*3,0,1),2)*np.array([0.085,0.062,0.078])
im=Image.fromarray(np.round(np.clip(tex,0,1)*255).astype(np.uint8)).resize((T,T),Image.LANCZOS); bio=io.BytesIO(); im.save(bio,'WEBP',quality=82,method=6); texb=bio.getvalue(); im.save('/home/claude/mtools/tex_out.png'); print('texture bytes',len(texb))
# ---- pack
hip=J[N['Hips']]; P=pos-hip; mn=P.min(0); mx=P.max(0); q=np.round((P-mn)/(mx-mn)*65535).astype(np.uint16)
B64=lambda a: base64.b64encode(a.tobytes()).decode()
out=dict(nv=len(pos),mn=mn.round(5).tolist(),mx=mx.round(5).tolist(),pos=B64(q),nor=B64(np.round(nor*127).astype(np.int8)),uv=B64(np.round(np.clip(uv%1,0,1)*65535).astype(np.uint16)),si=B64(SI.astype(np.uint8)),sw=B64(sw8.astype(np.uint8)),idx=B64(idx.astype(np.uint16)),
  bones=[[str(b[0])]+[round(float(b[1]-hip[0]),5),round(float(b[2]-hip[1]),5),round(float(b[3]-hip[2]),5)] for b in bones],par=[int(x) for x in par],chains=chains,hipY=round(float(hip[1]),5),tex='data:image/webp;base64,'+base64.b64encode(texb).decode())
js='const MDK = '+json.dumps(out,separators=(',',':'))+';\n'; open('/home/claude/game/model_k.js','w').write(js); print('model_k.js bytes',len(js))
