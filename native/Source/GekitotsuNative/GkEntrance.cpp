#include "GkMatch.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"

// The entrance holds the untouched round state. It never advances combat behind the camera.
bool AGkMatch::StepEntrance()
{
 if(Screen!=3 || Mode==1 || S.phase!=gk::PH_INTRO || S.round!=1)return false;
 if(!bEntrance && !bEntrancePlayed && S.pt==0){
  bEntrance=true;EntranceFrame=0;EntrancePoseFrames=120;CallHold=0;Director.Reset();
  if(auto* Sound=Voices.Find(TEXT("R6_Intro")))EntrancePoseFrames=FMath::Clamp(FMath::CeilToInt((*Sound)->GetDuration()*60)+12,120,210);
  UE_LOG(LogTemp,Display,TEXT("GkEntrance start: %d frames; stage fly-through 72 frames"),EntranceEnd());
 }
 if(!bEntrance)return false;
 ++EntranceFrame;
 if(EntranceFrame==EntranceA())FighterVoice(0,TEXT("R6_Intro"),3);
 if(EntranceFrame==EntranceB())FighterVoice(1,TEXT("R6_Parry"),3);
 for(int i=0;i<2;++i){int Start=i?EntranceBWalk():72;
  if(EntranceFrame>=Start && EntranceFrame<Start+72 && (EntranceFrame-Start)%24==8){
   Snd(TEXT("Step_01"),.30f,.96f+i*.06f);
  }
 }
 if(EntranceFrame>=EntranceEnd())FinishEntrance();
 return true;
}
void AGkMatch::FinishEntrance()
{
 if(!bEntrance)return;
 bEntrance=false;bEntrancePlayed=true;CamD.bSnap=true;
 for(int i=0;i<2;++i)if(FighterVoices[i])FighterVoices[i]->Stop();
 S.pt=FMath::Max(1,gk::INTRO-36);S.nev=0;gk::Event Round{};Round.type=gk::E_ROUND;Round.who=-1;Round.a=S.round;OnEvent(Round);
 UE_LOG(LogTemp,Display,TEXT("GkEntrance end: frame%d; hp%d/%d timer%d"),EntranceFrame,S.p[0].hp,S.p[1].hp,S.time);
}
float AGkMatch::EntranceX(int32 Who) const
{
 const float Final=S.p[Who].x/float(gk::F),Start=Who?EntranceBWalk():72;
 const float Walk=FMath::SmoothStep(Start,Start+72.f,float(EntranceFrame));
 return Final+(Who?220.f:-220.f)*(1-Walk);
}
FName AGkMatch::EntranceClip(int32 Who) const
{
 const int Start=Who?EntranceBWalk():72,Pose=Who?EntranceB():EntranceA();
 if(EntranceFrame<Start)return TEXT("idle");
 if(EntranceFrame<Pose)return TEXT("introWalk");
 if(EntranceFrame<EntranceDolly()+28)return Who?TEXT("introB"):TEXT("introA");
 return TEXT("idle");
}
float AGkMatch::EntranceSample(int32 Who,float Length) const
{
 const FName Slot=EntranceClip(Who);const auto* Pick=Clips[2].Find(Slot);
 const float From=Pick?Pick->From*Length:0,To=Pick?Pick->To*Length:Length,Span=FMath::Max(.02f,To-From);
 if(Slot==TEXT("introWalk")){const float Age=(EntranceFrame-(Who?EntranceBWalk():72))/60.f;return From+FMath::Fmod(FMath::Max(0.f,Age),Span);}
 if(Slot==TEXT("introA") || Slot==TEXT("introB")){const float Age=(EntranceFrame-(Who?EntranceB():EntranceA()))/float(EntrancePoseFrames);return FMath::Lerp(From,To,FMath::Clamp(Age,0.f,1.f));}
 return From+FMath::Fmod(EntranceFrame/60.f,Span);
}
void AGkMatch::EntranceCamera()
{
 if(!bEntrance || Screen!=3 || !Cam || bMenu)return;
 const FVector NormalEye=Cam->GetActorLocation();const FRotator NormalRot=Cam->GetActorRotation();const float NormalFov=Cam->GetCameraComponent()->FieldOfView;
 const float Ground=FloorPoint(At(0,0)).Z;
 FVector Eye,Focus;float Fov=47,Roll=0;
 if(EntranceFrame<72){
  const float U=FMath::SmoothStep(0.f,72.f,float(EntranceFrame));
  const FVector A(-1250,-650,Ground+430),B(-620,-120,Ground+210),C(-400,180,Ground+110);
  Eye=Wd((1-U)*(1-U)*A+2*U*(1-U)*B+U*U*C);
  Focus=Wd(FVector(100,FMath::Lerp(-70.f,30.f,U),Ground+100));Fov=FMath::Lerp(66.f,50.f,U);Roll=FMath::Sin(U*PI)*-8.f*ShakeGain;
 }else{
  const int Who=EntranceFrame<EntranceBWalk()?0:1;const int Start=Who?EntranceBWalk():72,Pose=Who?EntranceB():EntranceA();
  const float Side=Who?-1.f:1.f,U=FMath::Clamp((EntranceFrame-Pose)/float(EntrancePoseFrames),0.f,1.f);
  Focus=Wd(FVector(0,EntranceX(Who),Ground+75));
  if(Body[Who])Focus=Body[Who]->GetBoneLocationByName(TEXT("spine_03"),EBoneSpaces::WorldSpace)+FVector(0,0,9);
  const bool Walking=EntranceFrame<Pose;const float Distance=Walking?560:FMath::Lerp(440.f,320.f,U);
  Focus.Z+=Walking?-10.f:28.f;
  Eye=Focus+Anchor.TransformVectorNoScale(FVector(-Distance,Side*(Walking?-100.f:FMath::Lerp(-95.f,-25.f,U)),Walking?-12.f:12.f));
  Fov=Walking?48.f:43.f;Roll=Side*(Walking?-1.f:FMath::Sin(U*PI)*2.f)*ShakeGain;
  if(EntranceFrame>=EntranceDolly()){
   const float Pull=FMath::SmoothStep(float(EntranceDolly()),float(EntranceEnd()),float(EntranceFrame));
   Eye=FMath::Lerp(Eye,NormalEye,Pull);Focus=FMath::Lerp(Focus,NormalEye+NormalRot.Vector()*700.f,Pull);Fov=FMath::Lerp(Fov,NormalFov,Pull);Roll*=1-Pull;
  }
 }
 // Keep the authored path in front of scenery rather than passing through a cliff or tree.
 FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(GkEntranceCamera),false,this);
 if(GetWorld()->SweepSingleByChannel(Hit,Focus,Eye,FQuat::Identity,ECC_WorldStatic,FCollisionShape::MakeSphere(12),Query))Eye=Hit.Location+Hit.Normal*15.f;
 FRotator Rotation=(Focus-Eye).Rotation();Rotation.Roll=Roll;
 Cam->SetActorLocationAndRotation(Eye,Rotation);Cam->GetCameraComponent()->SetFieldOfView(Fov);
}
