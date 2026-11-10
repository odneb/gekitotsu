const S=require('../src/sim.js');const B=S.BT;const A=B.AS;
function run(script,opt={}){const s=S.newMatch({chars:[0,1],training:true});if(opt.pre)opt.pre(s);let log=[];
 for(let f=0;f<script.length;f++){S.step(s,script[f][0],script[f][1]||0);for(const e of s.ev){if(e.t==='hit')log.push(s.frame+':'+s.p[e.who].mv+' h'+e.combo+' d'+e.dmg+(e.total&&e.combo>1?'='+e.total:'')+(e.ch?' C'+e.ch:'')+(e.crum?' CRUMPLE':'')+(e.wrong?' wrong-'+e.wrong:'')); else if(['block','wbounce','gbounce','kd','tech','grab','parry','pparry','armor','splat','burnout','rush','super','di','ex','run','diclash'].includes(e.t))log.push(s.frame+':'+e.t)}}
 return {s,log}}
function seq(parts){const o=[];for(const [n,a,b] of parts)for(let i=0;i<n;i++)o.push([a,b||0]);return o}
const W=[52,B.R], show=(n,r)=>console.log(n+' => '+r.log.join(' | '));
const mash=(b,n)=>Array.from({length:n},()=>[[2,A|b],[8,A]]).flat();
show('A+L x3 (want 5LK>JIN_L>KAZE)', run(seq([W].concat(mash(B.LT,10)))));
show('A+M x5 (want 5MP>CB2>run>QDL>SA2)', run(seq([W].concat(mash(B.MD,26)))));
show('A+H x5 (want 5HP>JIN_OD>KAZE_OD>KASAI>SA3)', run(seq([W].concat(mash(B.HV,30)))));
show('A+H burnout route', run(seq([W].concat(mash(B.HV,30))),{pre:s=>{s.p[0].dr=0;s.p[0].bo=1}}));
show('M,M,M triple flash', run(seq([W,[2,B.MD],[9,0],[2,B.MD],[10,0],[2,B.MD],[70,0]])));
show('2M xx 2SP > 4SP', run(seq([W,[2,B.D|B.MD],[4,B.D],[2,B.D|B.SP],[26,0],[3,B.L|B.SP],[60,0]])));
show('2M xx 2SP > SP senka', run(seq([W,[2,B.D|B.MD],[4,B.D],[2,B.D|B.SP],[26,0],[3,B.SP],[60,0]])));
show('2M DRC A+M A+M 6SP', run(seq([W,[2,B.D|B.MD],[3,0],[2,B.R],[2,0],[2,B.R],[18,0],[2,A|B.MD],[9,A],[2,A|B.MD],[12,A],[2,B.R|B.SP],[90,0]])));
show('run > H', run(seq([[40,B.R],[2,B.MD|B.HV],[10,0],[2,B.HV],[60,0]])));
show('run > M overhead vs crouch block', run(seq([[40,B.R,0],[2,B.MD|B.HV,B.R|B.D],[12,0,B.R|B.D],[2,B.MD,B.R|B.D],[60,0,B.R|B.D]])));
show('run > 6SP', run(seq([[40,B.R],[2,B.MD|B.HV],[12,0],[2,B.R|B.SP],[90,0]])));
show('run > stop > 5L', run(seq([[40,B.R],[2,B.MD|B.HV],[12,0],[2,B.LT],[16,0],[2,B.LT],[30,0]])));
show('5H punish counter spin', run(seq([W,[2,0,B.HV],[20,0,0],[2,B.HV,0],[80,0]])));
show('SP/4SP/air SP', run(seq([[10,0],[2,B.SP],[70,0],[30,B.R],[2,B.L|B.SP],[90,0],[3,B.U|B.R],[14,0],[2,B.SP],[80,0]])));
show('214M tatsu, 623H', run(seq([W,[1,B.D],[1,B.D|B.L],[1,B.L],[2,B.L|B.MD],[90,0],[40,B.R],[1,B.R],[1,B.D],[1,B.D|B.R],[2,B.D|B.R|B.HV],[90,0]])));
show('SA1/SA2/SA3 raw', run(seq([W,[2,B.HV|B.SP],[110,0],[40,B.R],[2,B.L|B.HV|B.SP],[160,0],[40,B.R],[2,B.D|B.HV|B.SP],[200,0]])));
show('DI crumple', run(seq([W,[10,0],[2,B.DI,0],[18,0,0],[2,0,B.LT],[120,0]])));
show('parry hold then DR', run(seq([[20,0],[6,B.PA],[2,B.PA|B.R],[2,B.PA],[2,B.PA|B.R],[30,0],[2,A|B.MD],[60,0]])));
show('air parry attempt', (()=>{const r=run(seq([[10,0],[3,B.U],[10,0],[20,B.PA]]));r.log.push('state '+r.s.p[0].st);return r})());
function lcg(seed){let x=seed;return()=>{x=(Math.imul(x,1103515245)+12345)>>>0;return x}}
let bad=0,frames=0,h1=0;
for(let rep=0;rep<2;rep++){let hs=0;for(let seed=1;seed<=30;seed++){const g=lcg(seed);const s=S.newMatch({chars:[seed&1,(seed>>1)&1],training:seed%5===0});let a=0,b=0;
 for(let n=0;n<12000&&s.phase!=='over';n++){if(n%(3+seed%5)===0)a=g()>>>8&2047;if(n%(2+seed%7)===0)b=g()>>>8&2047; if(seed%3===0){s.p[0].sa=3000;s.p[1].sa=3000}
  S.step(s,a,b);frames++;for(const p of s.p){if(!Number.isFinite(p.x+p.y+p.vx+p.vy+p.pv+p.hp+p.sa+p.dr)||Math.abs(p.x)>S.STAGE||p.y<0||p.hp<0||p.dr<0||p.dr>6000||p.sa<0||(p.st==='ahit'&&p.y<=0)||(p.st==='parry'&&p.y>0)){bad++;if(bad<4)console.log('BAD',seed,n,JSON.stringify(p).slice(0,300))}}}
 hs=(hs*31+S.hash(s))>>>0;} if(rep===0)h1=hs; else console.log('fuzz frames',frames,'bad',bad,'deterministic',h1===hs);}
