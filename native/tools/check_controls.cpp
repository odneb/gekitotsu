#include "gk_sim.inl"
#include <cstdio>
#include <cassert>
#include <initializer_list>
#include <cstring>
using namespace gk;
int inputMove(int mask,bool air=true,int face=1,int* drive=nullptr) {
 State s;newMatch(s,false,2);s.phase=PH_FIGHT;s.p[0].x=-face*120*F;s.p[1].x=face*120*F;s.p[0].face=face;s.p[1].face=-face;
 if(air){s.p[0].y=150*F;s.p[0].st=JUMP;s.p[0].sf=12;s.p[0].vy=0;}
 step(s,mask,0);if(drive)*drive=s.p[0].dr;return s.p[0].mv;
}
int main(){
 for(int face:{1,-1})for(int dir:std::initializer_list<int>{0,IN_U,IN_D,IN_L,IN_R})for(int assist:std::initializer_list<int>{0,IN_AS})for(int b=0;b<3;++b){
  const char* normal[2][3]={{"jLP","jMP","jHK"},{"jLK","jMK","jHP"}};
  int got=inputMove(dir|assist|(IN_LT<<b),true,face);
  if(got!=moveByKey(normal[assist?1:0][b])){printf("FAIL face%d dir%d as%d button%d got%s\n",face,dir,assist,b,moveKey(got));return 1;}
 }
 for(int face:{1,-1}){
  int forward=face==1?IN_R:IN_L,back=face==1?IN_L:IN_R,drive=0;
  assert(inputMove(IN_SP,true,face)==M_ABEAM);
  assert(inputMove(IN_SP|back,true,face)==M_ABEAM);
  assert(inputMove(IN_SP|forward,true,face)==M_AUP);
  assert(inputMove(IN_SP|IN_D,true,face)==M_ASPK);
  assert(inputMove(IN_SP|forward,false,face)==M_SRK_M);
  assert(inputMove(IN_SP|back,false,face)==M_DL_H);
  assert(inputMove(IN_HM,true,face)==M_AHOME);
  assert(inputMove(IN_HM,false,face)==M_HOME);
  assert(inputMove(IN_SP|IN_AS,true,face,&drive)==M_ABEAM);assert(drive==DMAX);
 }
 State s;newMatch(s,false,2);s.phase=PH_FIGHT;s.p[0].y=300*F;s.p[0].st=JUMP;
 step(s,IN_SP,0);assert(s.p[0].mv==M_ABEAM && s.p[0].airBeamUsed);
 s.p[0].st=JUMP;s.p[0].mv=-1;step(s,0,0);step(s,IN_SP,0);assert(s.p[0].mv!=M_ABEAM);
 s.p[0].y=0;land(s,s.p[0],0);assert(!s.p[0].airBeamUsed);
 puts("PASS: 60 aerial normal inputs; mirrored beam, dive, anti-air and dedicated homing; no assist meter charge for identical air specials");
}
