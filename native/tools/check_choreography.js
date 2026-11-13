// Targeted checks for the editable source of the native move data.
const assert = require('node:assert/strict');
const sim = require('../../src/sim.js');
for (const face of [-1, 1]) for (const mode of ['hit', 'block', 'whiff', 'parry', 'air', 'interrupt']) {
 const s=sim.newMatch({chars:[0,0],rounds:2});s.phase='fight';
 const a=s.p[0],d=s.p[1];a.x=-face*20*sim.F;d.x=face*20*sim.F;a.face=face;d.face=-face;a.sa=3000;
 if(mode==='whiff')d.x+=face*240*sim.F;
 if(mode==='air'){d.y=65*sim.F;d.st='ahit';d.vy=300;d.combo=2;}
 sim.debugStart(s,0,'SA2');assert.equal(a.sa,1000);
 let held=0,hits=0,interrupted=false;
 for(let f=0;f<280;++f){
  sim.step(s,0,mode==='block'?(face>0?8:4):mode==='parry'?1024:0);
  if(d.superHold)++held;hits+=s.ev.filter(e=>e.t==='hit'&&e.who===0).length;
  if(mode==='interrupt'&&d.superHold&&!interrupted){a.st='hit';a.mv='';a.stun=20;interrupted=true;}
 }
 if(mode==='hit'||mode==='air'){assert(held>0);assert.equal(hits,2);}
 if(['block','whiff','parry'].includes(mode)){assert.equal(held,0);assert.equal(hits,0);}
 if(mode==='interrupt'){assert(interrupted);assert.equal(hits,1);}
 assert.equal(d.superHold,0);
}
console.log('PASS: editable move source SA2 hit/air, block/parry/whiff, interruption and cost, both facings');
