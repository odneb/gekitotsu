#include "gk_sim.inl"
#include <cassert>
#include <cstdio>
using namespace gk;
int main(){
 int cases=0;
 for(int owner=0;owner<2;++owner)for(int face=-1;face<=1;face+=2)for(int kind=1;kind<=4;++kind)for(int wall=0;wall<2;++wall){
  State s;newMatch(s,true,2);s.phase=PH_FIGHT;auto& a=s.p[owner];auto& d=s.p[1-owner];
  a.x=(wall?face*280:-face*25)*F;d.x=a.x+face*50*F;a.face=face;d.face=-face;
  const int fw=face>0?IN_R:IN_L,bw=face>0?IN_L:IN_R,input=kind==1?fw:kind==2?bw:kind==3?IN_U:IN_D;
  debugStart(s,owner,moveByKey("throw"));int hits=0;bool held=false;
  for(int f=0;f<300;++f){step(s,owner?0:input,owner?input:0);held|=d.st==THROWN;
   for(int j=0;j<s.nev;++j)if(s.ev[j].type==E_HIT && (s.ev[j].f&H_THROW))++hits;
   assert(iabs(a.x)<=STAGE && iabs(d.x)<=STAGE);assert(a.y>=0 && d.y>=0);
  }
  assert(held && hits==1);assert(a.st!=THROWING && d.st!=THROWN);++cases;
 }
 State s;newMatch(s,true,2);s.phase=PH_FIGHT;s.p[0].x=-25*F;s.p[1].x=25*F;
 debugStart(s,0,moveByKey("throw"));debugStart(s,1,moveByKey("throw"));bool teched=false;
 for(int f=0;f<60;++f){step(s,0,0);for(int j=0;j<s.nev;++j)teched|=s.ev[j].type==E_TECH;}
 assert(teched && s.p[0].hp==1000 && s.p[1].hp==1000);
 printf("PASS: %d directional throws, both players/facings/walls; one impact, recovery, bounds; simultaneous throw tech\n",cases);
}
