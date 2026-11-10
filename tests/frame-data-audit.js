// Conformance audit: measure every move in the simulation and compare with SF6 Ken's published frame data.
const S=require('../src/sim.js');const B=S.BT;
// key: [damage, startup, total (grounded) or null, hitAdv or 'KD', blockAdv]
const X={ '5LP':[300,4,13,4,-1],'5MP':[600,5,22,4,-2],'5HP':[800,10,31,3,-2],'5LK':[300,5,18,0,-2],'5MK':[600,8,30,3,-5],'5HK':[800,12,38,1,-5],
 '2LP':[300,4,14,5,-1],'2HP':[800,8,35,3,-7],'2LK':[200,5,17,1,-3],'2MK':[500,7,28,-2,-6],'2HK':[900,8,34,'KD',-10],
 'CB2':[400,11,40,'KD',-14],'TFK2':[400,11,39,'KD',-12],'TFK3':[700,13,43,'KD',-11],
 'HAD_H':[600,12,49,-5,-11],'HAD_OD':[800,12,40,2,-2],
 'SRK_L':[1100,5,47,'KD',-23],'SRK_M':[1300,6,55,'KD',-28],'SRK_H':[1400,7,66,'KD',-36],'SRK_OD':[1600,6,89,'KD',-40],
 'TAT_L':[700,4,46,'KD',-14],'TAT_M':[900,14,61,'KD',-12],'TAT_H':[1000,16,78,'KD',-12],'TAT_OD':[1700,9,90,'KD',-61],
 'DL_H':[1200,28,53,3,1],'DL_OD':[1200,9,48,1,-9],
 'JIN_L':[500,12,42,1,-11],'JIN_M':[600,16,42,2,-7],'JIN_H':[700,25,46,'KD',-2],'JIN_OD':[600,13,39,-4,-7],
 'KAZE':[600,6,28,3,-5],'KAZE_OD':[500,6,28,3,-5],'GORAI':[1000,18,40,3,-3],'GORAI_OD':[1000,17,43,-3,-7],'SENKA':[800,10,37,'KD',-3],'SENKA_OD':[800,10,33,'KD',-4],'KASAI':[500,15,46,'KD',-12],
 'TK':[1000,18,40,3,-3],'FSK':[800,8,35,1,-4],'QSRK':[1700,8,66,'KD',-35],'QTAT':[1200,13,78,3,-9],'QDL':[1200,9,47,2,-8],
 'SA1':[2000,7,50,'KD',-24],'SA2':[2800,6,89,'KD',-5],'SA3':[4000,7,100,'KD',-40],'DI':[800,26,62,'KD',-3],'DREV':[500,20,48,'KD',-6] };
const LOW={'2LK':1,'2MK':1,'2HK':1,'KAZE':1,'KAZE_OD':1};
const crouchers={'TFK3':1,'SENKA':1,'SENKA_OD':1};
function run(key,mode){ // mode: 'whiff' | 'hit' | 'block'
  const s=S.newMatch({chars:[0,1],training:true}); const a=s.p[0],d=s.p[1]; a.x=-2000; d.x=mode==='whiff'?30000:1400; a.dr=6000;a.sa=3000;d.dr=6000;
  S.step(s,0,0); S.debugStart(s,0,key); if(a.sa<3000)a.sa=3000;
  let su=null,aFree=null,dFree=null,dmg=0,firstTouch=null,kd=false,frames=0,hp0=d.hp; const M=S.CH[0].mv[key];
  for(let f=1;f<400;f++){ if(su===null && a.st==='atk' && a.mv===key){ const act=S.hitboxes(a).length>0 || (M.proj && a.mf===M.proj.f); }
    const fz=s.freeze>0; S.step(s,0,mode==='block'?(LOW[key]?B.R|B.D:B.R):0);
    if(fz) continue; frames++;
    if(su===null && a.st==='atk' && a.mv===key && (M.proj ? a.mf>=M.proj.f : M.hits.some(h=>a.mf>=h.s&&a.mf<=h.e))) su=a.mf+1;
    for(const e of s.ev){ if(e.t==='hit'&&e.who===0){dmg+=e.dmg; if(firstTouch===null)firstTouch=f} if(e.t==='block'&&firstTouch===null)firstTouch=f; if(e.t==='kd')kd=true }
    if(aFree===null && a.st==='idle') aFree=f; if(firstTouch!==null && dFree===null && d.st==='idle') dFree=f; if(d.st==='ahit'||d.st==='kd')kd=true;
    if(aFree!==null && (mode==='whiff'||dFree!==null||kd&&f>aFree+2)) break; }
  return {su,total:mode==='whiff'?countTotal(key):null,adv:(dFree!==null&&aFree!==null&&!kd)?dFree-aFree:(kd?'KD':null),dmg,touch:firstTouch!==null};
}
function countTotal(key){ const s=S.newMatch({chars:[0,1],training:true}); const a=s.p[0]; a.x=-2000; s.p[1].x=30000; a.sa=3000; S.step(s,0,0); S.debugStart(s,0,key); let n=0; for(let f=0;f<400;f++){ const fz=s.freeze>0; S.step(s,0,0); if(fz)continue; if(a.st==='idle')break; n++; } return n+1; }
let bad=0,ok=0; const rows=[];
for(const k in X){ const [ed,esu,etot,ehit,eblk]=X[k]; const w=run(k,'whiff'),h=run(k,'hit'),b=run(k,'block'); const issues=[];
  if(w.su!==esu) issues.push('startup '+w.su+' want '+esu);
  if(etot!==null && w.total!==etot) issues.push('total '+w.total+' want '+etot);
  if(!crouchers[k]||true){ if(!h.touch) issues.push('did not connect at point blank'); else { if(Math.round(h.dmg*10)!==ed) issues.push('damage '+h.dmg*10+' want '+ed); if(String(h.adv)!==String(ehit)) issues.push('hit '+h.adv+' want '+ehit); }
    if(b.touch){ if(b.adv!==eblk) issues.push('block '+b.adv+' want '+eblk); } else issues.push('block n/a'); }
  if(issues.length){bad++;rows.push(k.padEnd(9)+issues.join('; '))} else ok++; }
console.log('moves audited',ok+bad,'exact',ok,'mismatched',bad); console.log(rows.join('\n'));
