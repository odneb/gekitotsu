#include "gk_sim.inl"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using namespace gk;
static State setup(int face,int location=0){State s;newMatch(s,false,2);s.phase=PH_FIGHT;s.p[0].x=location*F;s.p[1].x=s.p[0].x+face*40*F;s.p[0].face=face;s.p[1].face=-face;return s;}
int main(){
 int throws=0;
 for(int face:{-1,1})for(int kind:{1,2,3,4})for(int pos:{-318,0,318}){
  auto s=setup(face,pos);auto& a=s.p[0];auto& d=s.p[1];debugStart(s,0,M_throw);
  const int input=kind==1?(face>0?IN_R:IN_L):kind==2?(face>0?IN_L:IN_R):kind==3?IN_U:IN_D;
  int hp=d.hp,last=d.x,release=0;bool caught=false;
  for(int f=0;f<180;++f){const bool held=d.st==THROWN;step(s,input,0);
   if(d.st==THROWN){caught=true;assert(d.hp==hp);if(held)assert(iabs(d.x-last)<14*F);}
   if(d.hp<hp && !release){release=a.tf;assert(release==18);assert(hp-d.hp==120);assert(d.st==AHIT);}
   last=d.x;
  }
  assert(caught&&release);assert(a.st!=THROWING&&d.st!=THROWN);++throws;
 }
 for(int face:{-1,1}){
  auto s=setup(face);debugStart(s,0,M_throw);for(int f=0;f<6;++f)step(s,0,0);assert(s.p[1].st==THROWN);
  step(s,0,IN_LT|IN_MD);step(s,0,0);assert(s.p[0].st==TECH&&s.p[1].st==TECH);assert(s.p[1].hp==1000);
  s=setup(face);debugStart(s,0,M_throw);debugStart(s,1,M_throw);for(int f=0;f<7;++f)step(s,0,0);assert(s.p[0].st==TECH&&s.p[1].st==TECH);
  s=setup(face);s.p[1].x+=face*130*F;debugStart(s,0,M_throw);for(int f=0;f<100;++f)step(s,0,0);assert(s.p[1].hp==1000&&s.p[0].st==IDLE);
  s=setup(face);s.p[1].st=PARRY;debugStart(s,0,M_throw);for(int f=0;f<100;++f)step(s,0,IN_PA);assert(s.p[1].hp==796); // punish damage remains unchanged
 }
 for(int face:{-1,1})for(int mode=0;mode<6;++mode){
  auto s=setup(face);s.p[0].sa=3000;auto& a=s.p[0];auto& d=s.p[1];
  if(mode==2)d.x+=face*240*F;
  if(mode==3){d.y=65*F;d.st=AHIT;d.vy=300;d.combo=2;}
  debugStart(s,0,M_SA2);assert(a.sa==1000);
  int held=0,hits=0;bool interrupted=false;
  for(int f=0;f<280;++f){
   const int input=mode==1?(face>0?IN_R:IN_L):mode==4?IN_PA:0;
   step(s,0,input);if(d.superHold)++held;
   for(int e=0;e<s.nev;++e)if(s.ev[e].type==E_HIT&&s.ev[e].who==0)++hits;
   if(mode==5 && d.superHold && !interrupted){a.st=HIT;a.mv=-1;a.stun=20;interrupted=true;}
  }
  if(mode==0||mode==3){assert(held>0);assert(hits==2);assert(d.hp<900);}
  if(mode==1||mode==2||mode==4){assert(!held);assert(!hits);}
  if(mode==5){assert(interrupted);assert(hits==1);}
  assert(!d.superHold);assert(a.st!=ATK&&d.st!=HIT&&d.st!=AHIT);
 }
 for(int face:{-1,1})for(int guard:{0,1}){
  auto s=setup(face);const int forward=face>0?IN_R:IN_L,defence=guard?forward:0;
  step(s,forward|IN_SP|IN_AS,defence);assert(s.p[0].mv==M_SRK_OD);assert(s.p[0].dr==4000);
  int hits=0,blocks=0;
  for(int f=0;f<250;++f){step(s,0,defence);for(int j=0;j<s.nev;++j){hits+=s.ev[j].type==E_HIT;blocks+=s.ev[j].type==E_BLOCK;}}
  assert(hits==(guard?0:2));assert(blocks==(guard?2:0));assert(s.p[1].hp==(guard?1000:872));assert(s.p[0].st==IDLE);
 }
 puts("PASS: two-beat enhanced anti-air, two-bar drive cost, mirrored hit/block and recovery");
 puts("PASS: SA2 hit/air confirm, block, parry, whiff, interrupted latch, two-bar cost and recovery, mirrored");
 puts("PASS: 24 directional/mirrored/corner throws; continuous held trajectory; f18 release; tech, mutual tech, whiff and parry punish");
}
