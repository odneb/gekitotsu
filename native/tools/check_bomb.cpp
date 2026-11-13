#include "gk_sim.inl"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using namespace gk;
int main(){
 for(int side:{1,-1})for(int edge:{0,1})for(int air:{0,1}){
  State s;newMatch(s,false,2);s.phase=PH_FIGHT;s.p[0].sa=3000;
  s.p[0].x=side*(edge?280:-25)*F;s.p[1].x=s.p[0].x+side*50*F;
  s.p[0].face=side;s.p[1].face=-side;if(air){s.p[1].y=70*F;s.p[1].st=AHIT;s.p[1].vy=300;}
  debugStart(s,0,M_SA2);int latches=0,bursts=0;bool held=false;
  for(int f=0;f<400;f++){
   step(s,0,0);for(int j=0;j<s.nev;j++){if(s.ev[j].type==E_GRAB&&s.ev[j].a==5)++latches;if(s.ev[j].type==E_HIT&&(s.ev[j].f&H_FINISH))++bursts;}
   if(s.p[0].tk==5 && s.p[1].st==THROWN){held=true;assert(s.p[1].x==s.p[0].x+side*24*F);assert(s.p[1].y==s.p[0].y);}
   for(auto& p:s.p){assert(p.hp>=0&&p.hp<=1000);assert(iabs(p.x)<=STAGE);}
  }
  if(!(held&&latches==1&&bursts==1)){fprintf(stderr,"FAIL side%d edge%d air%d held%d latch%d burst%d hp%d state%d\n",side,edge,air,held,latches,bursts,s.p[1].hp,s.p[0].st);return 1;}assert(s.p[0].st!=THROWING&&s.p[1].st!=THROWN);assert(s.p[0].hp==1000);assert(s.p[1].hp<800);assert(s.p[0].sa<2000);
 }
 for(int defense:{0,1,2}){
  State s;newMatch(s,false,2);s.phase=PH_FIGHT;s.p[0].sa=3000;s.p[0].x=-25*F;s.p[1].x=(defense==0?300:25)*F;
  debugStart(s,0,M_SA2);bool latch=false;
  for(int f=0;f<180;f++){step(s,0,defense==1?IN_R:defense==2?IN_PA:0);latch|=s.p[0].tk==5;}
  assert(!latch);assert(s.p[1].hp==1000);assert(s.p[0].st!=ATK);
 }
 for(int owner:{0,1})for(int mod:{0,1})for(int hp:{1,250,1000})for(int training:{0,1}){
  State s;newMatch(s,training,2);s.phase=PH_FIGHT;auto& a=s.p[owner];auto& d=s.p[1-owner];
  a.x=-25*F;d.x=25*F;a.face=1;d.face=-1;a.sa=3000;d.hp=hp;
  start(s,a,M_SA2,owner,mod);int damage=0,bursts=0;
  for(int f=0;f<400;++f){step(s,0,0);for(int j=0;j<s.nev;++j)if(s.ev[j].type==E_HIT){damage+=s.ev[j].b;bursts+=(s.ev[j].f&H_FINISH)?1:0;}if(training)assert(d.hp>=1);}
  if(hp==1000)assert(damage==(mod?240:300));
  assert(bursts==(hp==1 && !training?0:1));assert(a.st!=THROWING && d.st!=THROWN);
 }
 puts("PASS: hug bomb confirms once, holds both facings and corners, catches aerial hit, detonates once; block/parry/whiff do not latch; no self damage or stuck states");
}
