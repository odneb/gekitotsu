#include "GkMatch.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void AGkMatch::LoadVoices()
{
 for(const TCHAR* Name:{TEXT("Ann_Round1"),TEXT("Ann_Round2"),TEXT("Ann_Round3"),TEXT("Ann_Final"),TEXT("Ann_Fight"),TEXT("Ann_KO"),TEXT("Ann_Win"),TEXT("Ann_Perfect"),TEXT("Ann_Time"),TEXT("Ann_Combo")})
 {
  const FString Path=FString::Printf(TEXT("/Game/GkVoice/Announcer/%s.%s"),Name,Name);
  if(USoundBase* Sound=LoadObject<USoundBase>(nullptr,*Path)){Voices.Add(FName(Name),Sound);}
 }
 for(const TCHAR* Name:{TEXT("R6_Intro"),TEXT("R6_Beam"),TEXT("R6_Home"),TEXT("R6_AntiAir"),TEXT("R6_Super1"),TEXT("R6_Super2"),TEXT("R6_Super3"),TEXT("R6_Critical"),TEXT("R6_Win"),TEXT("R6_Parry"),TEXT("R6_Oops"),TEXT("R6_Defeat"),TEXT("R6_Attack1"),TEXT("R6_Attack2"),TEXT("R6_Attack3"),TEXT("R6_Attack4"),TEXT("R6_Hurt1"),TEXT("R6_Hurt2"),TEXT("R6_Hurt3")})
 {const FString Path=FString::Printf(TEXT("/Game/GkVoice/Robot1/%s.%s"),Name,Name);if(USoundBase* Sound=LoadObject<USoundBase>(nullptr,*Path))Voices.Add(FName(Name),Sound);}
 UE_LOG(LogTemp,Display,TEXT("GkVoice: %d announcer and Male 6 takes loaded"),Voices.Num());
}
void AGkMatch::Announce(const TCHAR* Name,bool Interrupt)
{
 if(Screen!=3||bMenu)return;
 if(Announcer&&Announcer->IsPlaying()){if(!Interrupt)return;Announcer->Stop();}
 if(Announcer){Announcer->DestroyComponent();Announcer=nullptr;}
 if(TObjectPtr<USoundBase>* Sound=Voices.Find(FName(Name)))
 {
  Announcer=UGameplayStatics::SpawnSound2D(this,*Sound,.85f,1.f,0.f,nullptr,false,false);
  UE_LOG(LogTemp,Display,TEXT("GkVoice: %s"),Name);
 }
}
void AGkMatch::FighterVoice(int32 Who,const TCHAR* Name,int32 Priority)
{
 if(Who<0||Who>1||Screen!=3||bMenu)return;
 const double VoiceTime=GetWorld()->GetTimeSeconds();
 UAudioComponent* PlayingVoice=FighterVoices[Who];
 if(Priority<3 && VoiceTime<VoiceNext[Who])return;
 if(PlayingVoice && PlayingVoice->IsPlaying() && Priority<=VoicePriority[Who])return;
 if(TObjectPtr<USoundBase>* Sound=Voices.Find(FName(Name))){
  if(PlayingVoice){PlayingVoice->Stop();PlayingVoice->DestroyComponent();}
  FighterVoices[Who]=UGameplayStatics::SpawnSound2D(this,*Sound,.82f,1.f,0.f,nullptr,false,false);
  VoicePriority[Who]=Priority;VoiceNext[Who]=VoiceTime+(Priority>=2?.85:.35);
  UE_LOG(LogTemp,Display,TEXT("GkVoice Robot%d %s"),Who+1,Name);
 }
}
void AGkMatch::VoiceEvent(const gk::Event& E)
{
 switch(E.type)
 {
 case gk::E_SUPER:FighterVoice(E.who,E.b?TEXT("R6_Critical"):E.a==3?TEXT("R6_Super3"):E.a==2?TEXT("R6_Super2"):TEXT("R6_Super1"),3);break;
 case gk::E_SPECIAL:if(E.c>=0 && !gk::move(E.c).sup)FighterVoice(E.who,E.c==gk::moveByKey("AUP")||E.c==gk::moveByKey("SRK_M")||E.c==gk::moveByKey("SRK_OD")?TEXT("R6_AntiAir"):gk::move(E.c).has_home?TEXT("R6_Home"):TEXT("R6_Beam"),2);break;
 case gk::E_WHIFF:{const auto& P=S.p[E.who&1];if(P.mv>=0&&!gk::move(P.mv).sup&&!gk::move(P.mv).sp){const FString Name=FString::Printf(TEXT("R6_Attack%d"),1+(S.frame+E.who)%4);FighterVoice(E.who,*Name,0);}break;}
 case gk::E_PPARRY:FighterVoice(E.who,TEXT("R6_Parry"),2);break;
 case gk::E_ROUND:Announce(S.round>=3?TEXT("Ann_Final"):S.round==2?TEXT("Ann_Round2"):TEXT("Ann_Round1"),true);break;
 case gk::E_FIGHT:Announce(TEXT("Ann_Fight"),true);FighterVoice(0,TEXT("R6_Intro"),2);break;
 case gk::E_KO:if(E.who>=0)FighterVoice(1-E.who,TEXT("R6_Defeat"),4);Announce(S.koType==gk::KO_TIME?TEXT("Ann_Time"):TEXT("Ann_KO"),true);break;
 case gk::E_WINPOSE:FighterVoice(E.who,TEXT("R6_Win"),3);Announce(S.koType==gk::KO_PERFECT?TEXT("Ann_Perfect"):TEXT("Ann_Win"),false);break;
 case gk::E_HIT:{if(!(E.f&gk::H_KO)){const FString Name=FString::Printf(TEXT("R6_Hurt%d"),1+S.frame%3);FighterVoice(1-E.who,*Name,1);}}if(E.c==8 && !(E.f&gk::H_KO))Announce(TEXT("Ann_Combo"),false);break;
 default:break;
 }
}
