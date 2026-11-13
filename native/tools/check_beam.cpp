#include "gk_sim.inl"
#include <cassert>
#include <cstdio>
#include <initializer_list>

using namespace gk;
struct Result { int damage, hits, blocked; };
Result exercise(const char* key, int face, bool guard, bool behind=false) {
 State s; newMatch(s,false,2); s.phase=PH_FIGHT;
 s.p[0].x=-face*120*F; s.p[1].x=face*120*F;
 s.p[0].face=behind ? -face : face; s.p[1].face=-face;
 s.p[0].sa=3000; if(key[0]=='C') s.p[0].hp=200;
 debugStart(s,0,moveByKey(key));
 if(behind) s.p[0].face=-face;
 int hits=0,blocked=0; const int origin=s.p[0].x;
 for(int f=0;f<250;++f) {
  step(s,0,guard ? (face>0 ? IN_R:IN_L) : 0);
  for(int e=0;e<s.nev;++e) {if(s.ev[e].type==E_HIT) ++hits;if(s.ev[e].type==E_BLOCK) ++blocked;}
  if (s.p[0].st==ATK && s.p[0].mf<58) { assert(s.p[0].vx==0 && s.p[0].y==0); } // no scripted rush or flight
 }
 assert(s.p[0].st==IDLE); assert(s.p[0].sa<1000);
 return {1000-s.p[1].hp,hits,blocked};
}
int main() {
 for(const char* key: {"SA3","CA"}) {
  auto a=exercise(key,1,false), b=exercise(key,-1,false), g=exercise(key,1,true), w=exercise(key,1,false,true);
  printf("%s: hit %d (%d hits), mirror %d, guard %d (%d blocks), behind %d\n",key,a.damage,a.hits,b.damage,g.damage,g.blocked,w.damage);
  assert(a.damage>200 && a.damage<600); assert(a.damage==b.damage); assert(a.hits==7);
  assert(g.hits==0 && g.blocked==7); assert(g.damage<a.damage); assert(w.damage==0);
 }
 puts("PASS beam reach, mirrored facing, seven hits, guard, recovery, meter and stationary commitment");
}
