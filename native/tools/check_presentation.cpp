#include "../Source/GekitotsuNative/GkPresentationDirector.h"
#include <cassert>
#include <fstream>
#include <iostream>
int main(int argc,char** argv){
 assert(argc==2);std::ifstream f(argv[1]);std::stringstream text;text<<f.rdbuf();FGkPresentationDirector d;assert(d.Load(text.str()));
 d.Start("heavy",0,1,true);d.Tick(1./60,true,1,1,true);assert(d.Out.mode==1);
 d.Start("ko",1,-1,true);d.Start("land",0,1,false);d.Tick(1./60,true,1,1,true);assert(d.Out.priority==100 && d.Out.who==1);
 for(int i=0;i<80;++i)d.Tick(1./60,true,1,1,false);assert(d.playing.empty() && d.Out.mode==0);
 d.Start("super",0,1,true);for(int i=0;i<8;++i)d.Tick(1./60,true,0,0,true);assert(d.Out.mode==0 && d.Out.ink==0 && d.Out.zoom==0 && d.Out.roll==0);
 d.Tick(1./60,false,1,1,true);assert(d.playing.empty() && d.Out.priority==-1);
 assert(!d.Load("broken data"));assert(!d.cues.empty());
 d.Reset();d.Start("heavy",0,1,true);d.Start("heavy",0,1,true);assert(d.playing.size()==1);
 d.Reset();d.Start("super",0,1,true);d.Tick(.016,true,1,1,false);assert(d.Out.zoom<=.025f && std::abs(d.Out.roll)<=.7f);
 std::cout<<"PASS presentation priorities, finite cues, invalid-file recovery, no repeated impact strobe, pause/reset, reduced flash/camera and interruptible combat limits\n";
}
