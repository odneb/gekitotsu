global.addEventListener=()=>{};global.navigator={};global.localStorage={getItem:()=>null,setItem(){}};global.window={};
global.SIM=require('../src/sim.js');global.R={hsh(n){n=Math.imul(n^0x9e3779b9,0x85ebca6b);n^=n>>>13;n=Math.imul(n,0xc2b2ae35);return((n^(n>>>16))>>>0)/4294967296}};
const src=require('fs').readFileSync(__dirname+'/../src/io.js','utf8');eval(src.replace('const AI =','global.AI ='));
// how often does the CPU use the air game and the grab?
const tot={};let matches=0,unfinished=0,frames=0;
for(const [c,la,lb] of [[[0,1],3,3],[[1,0],3,2],[[0,1],2,2],[[0,0],3,3],[[1,1],2,3],[[0,1],1,1]]){
 const s=SIM.newMatch({chars:c});const a=AI.make(la),b=AI.make(lb);let n=0;const last=['',''];
 while(s.phase!=='over'&&n<40000){SIM.step(s,AI.think(a,s,0),AI.think(b,s,1));n++;
  for(let i=0;i<2;i++){const p=s.p[i],k=p.st==='atk'?p.mv:p.st==='throwing'?'T'+(p.tk||0):'';if(k!==last[i]){last[i]=k;if(['jUP','jDN','jBK','ASPK','AUP','ABK','T1','T2','T3','T4'].includes(k))tot[k]=(tot[k]||0)+1;}}
  for(const e of s.ev){if(e.t==='jump'&&e.dj)tot.doubleJump=(tot.doubleJump||0)+1;if(e.t==='walljump')tot.wallJump=(tot.wallJump||0)+1;if(e.t==='gbounce')tot.groundBounce=(tot.groundBounce||0)+1;if(e.t==='hit'&&e.thr&&e.lvl===0)tot.pummel=(tot.pummel||0)+1;}}
 matches++;frames+=n;if(s.phase!=='over')unfinished++;}
console.log('matches',matches,'unfinished',unfinished,'frames',frames);console.log(JSON.stringify(tot));
