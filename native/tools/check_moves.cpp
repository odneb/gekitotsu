#include "gk_sim.inl"
#include <cstdio>
#include <cassert>
using namespace gk;
int main(){
 int failures=0;
 for(int m=0;m<moveCount();++m){
  State s;newMatch(s,false,2);s.phase=PH_FIGHT;s.p[0].x=-60*F;s.p[1].x=60*F;s.p[0].sa=3000;
  if(move(m).isAir){s.p[0].y=60*F;s.p[0].st=JUMP;}
  debugStart(s,0,m);
  if(move(m).isAir) {s.p[0].y=60*F;s.p[0].vy=300;}
  for(int f=0;f<1200;++f){step(s,0,0);for(auto &p:s.p){assert(p.hp>=0 && p.hp<=1000);assert(p.y>=0);assert(p.dr>=0 && p.dr<=DMAX);}}
  if(s.p[0].st==ATK || s.p[0].st==THROWING || s.p[0].y>0){printf("STUCK %s state=%d frame=%d height=%d\n",moveKey(m),s.p[0].st,s.p[0].mf,s.p[0].y);++failures;}
 }
 printf("Move recovery check: %d moves, %d failures\n",moveCount(),failures);return failures?1:0;
}
