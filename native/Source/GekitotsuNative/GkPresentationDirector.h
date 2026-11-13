#pragma once
#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

// Presentation only: never reads or writes combat timing, random state, damage or inputs.
// Timelines are in 60 Hz presentation frames and continue through combat hitstop.
struct FGkPresentationDirector {
 struct Cue {
  std::string name; int priority=0, start=0, end=1, mode=0;
  float ink=0, dim=0, rim=0, zoom=0, roll=0, lines=0, duck=0;
 };
 struct Sequence { std::string name; double age=0; int who=0, face=1, priority=0; bool safe=false; };
 struct Frame {
  int mode=0, who=0, face=1, priority=-1; float ink=0, dim=0, rim=0, zoom=0, roll=0, lines=0, duck=0;
 } Out;
 std::vector<Cue> cues; std::vector<Sequence> playing;
 double clock=0, lastImpact=-1000; bool enabled=true;
 void Reset(){playing.clear();Out=Frame{};lastImpact=-1000;}
 // Invalid rows are rejected; a bad live edit leaves the last valid configuration intact.
 bool Load(const std::string& text){
  std::vector<Cue> next; std::istringstream all(text); std::string line;
  while(std::getline(all,line)){
   if(line.empty() || line[0]=='#')continue;
   std::replace(line.begin(),line.end(),'|',' ');std::istringstream row(line);Cue c;
   if(!(row>>c.name>>c.priority>>c.start>>c.end>>c.mode>>c.ink>>c.dim>>c.rim>>c.zoom>>c.roll>>c.lines>>c.duck))return false;
   if(c.start<0 || c.end<=c.start || c.end>360 || c.mode<0 || c.mode>4)return false;
   for(float v:{c.ink,c.dim,c.rim,c.zoom,c.roll,c.lines,c.duck})if(!std::isfinite(v))return false;
   c.ink=std::clamp(c.ink,0.f,1.f);c.dim=std::clamp(c.dim,0.f,.8f);c.rim=std::clamp(c.rim,0.f,3.f);
   c.zoom=std::clamp(c.zoom,0.f,.15f);c.roll=std::clamp(c.roll,-4.f,4.f);c.lines=std::clamp(c.lines,0.f,1.f);c.duck=std::clamp(c.duck,0.f,.6f);
   next.push_back(c);
  }
  if(next.empty())return false;cues=std::move(next);Reset();return true;
 }
 void Start(const std::string& name,int who,int face,bool safe){
  if(!enabled)return;int priority=-1;for(const auto& c:cues)if(c.name==name)priority=std::max(priority,c.priority);
  if(priority<0)return;
  // No repeated strobe from a multihit beam. KO/finish can always punctuate the sequence.
  if((name=="heavy" || name=="counter" || name=="impact") && clock-lastImpact<10)return;
  if(name=="heavy" || name=="counter" || name=="impact")lastImpact=clock;
  for(auto& s:playing)if(s.name==name && s.who==who && s.age<5)return;
  if(playing.size()>=4){auto it=std::min_element(playing.begin(),playing.end(),[](auto&a,auto&b){return a.priority<b.priority;});if(it->priority>priority)return;playing.erase(it);}
  playing.push_back({name,0,who,face,priority,safe});
 }
 void Tick(double delta,bool active,float flash,float camera,bool safeNow){
  Out=Frame{};if(!active || !enabled){playing.clear();return;}
  // Evaluate first so every newly triggered impact is visible even on a slow render frame.
  for(const auto& s:playing)for(const auto& c:cues)if(s.name==c.name && s.age>=c.start && s.age<c.end){
   if(c.priority<Out.priority)continue;
   const float tail=c.mode?1.f:float(std::clamp((c.end-s.age)/std::min(8,c.end-c.start),0.0,1.0));
   Out.mode=flash>.01f?c.mode:0;Out.ink=c.ink*flash;Out.dim=c.dim*tail;Out.rim=c.rim*tail;
   Out.zoom=c.zoom*tail*camera;Out.roll=c.roll*s.face*tail*camera;
   if(!(s.safe && safeNow)){Out.zoom=std::min(Out.zoom,.025f);Out.roll=std::clamp(Out.roll,-.7f,.7f);}
   Out.lines=c.lines*tail;Out.duck=c.duck*tail;Out.who=s.who;Out.face=s.face;Out.priority=c.priority;
  }
  const double frames=std::clamp(delta,0.0,.1)*60;clock+=frames;
  for(auto& s:playing)s.age+=frames;
  playing.erase(std::remove_if(playing.begin(),playing.end(),[&](const auto&s){int end=0;for(const auto& c:cues)if(c.name==s.name)end=std::max(end,c.end);return s.age>=end;}),playing.end());
 }
};
