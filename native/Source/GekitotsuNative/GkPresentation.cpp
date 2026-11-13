#include "GkMatch.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Engine/SkeletalMesh.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

void AGkMatch::LoadPresentation()
{
 FString Text; if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectSavedDir()/TEXT("presentation.txt"))))return;
 TArray<FString> Parts;Text.ParseIntoArray(Parts,TEXT("|"));
 float* Values[]={&MusicGain,&EffectsGain,&VoiceGain,&ShakeGain,&FlashGain};
 for(int32 i=0;i<5 && Parts.IsValidIndex(i);++i)*Values[i]=FMath::Clamp(FCString::Atof(*Parts[i]),0.f,1.f);
}
void AGkMatch::SavePresentation()
{
 FFileHelper::SaveStringToFile(FString::Printf(TEXT("%.2f|%.2f|%.2f|%.2f|%.2f"),MusicGain,EffectsGain,VoiceGain,ShakeGain,FlashGain),*(FPaths::ProjectSavedDir()/TEXT("presentation.txt")));
}
void AGkMatch::MenuSound(int32 SoundKind)
{
 MenuIdleAge=0;
 const TCHAR* Name=SoundKind==1?TEXT("AbilityReady_06"):SoundKind==2?TEXT("Vanish_01"):TEXT("Swing_03");
 if(const auto* Takes=SndSets.Find(FName(Name)))if(Takes->Num())
  UGameplayStatics::PlaySound2D(this,(*Takes)[0],EffectsGain*(SoundKind==0?.15f:.32f),SoundKind==0?1.35f+.07f*(MenuSoundStep++%4):SoundKind==1?1.05f:.85f);
 if(SoundKind==1 && MenuPage==0){
  MenuStampAge=0;
  MenuStamp=Screen==5?TEXT("LET'S GO!"):Screen==1?FString(CostumeName(Setup.Costume[SelWho]))+TEXT(" / LOCKED"):Screen==2?TEXT("STAGE SET!"):Screen==6?TEXT("ONE MORE ROUND!"):TEXT("READY!");
  if(const auto* Takes=SndSets.Find(TEXT("ImpactBurst_01")))if(Takes->Num())UGameplayStatics::PlaySound2D(this,(*Takes)[0],EffectsGain*.10f,1.4f);
 }

 CursorAge=0;
}
void AGkMatch::UpdatePresentation(float Delta)
{
 if(UiPageKey<0){LoadPresentation();BrandLogo=LoadObject<UTexture2D>(nullptr,TEXT("/Game/GkUI/T_GekitotsuImpact.T_GekitotsuImpact"));}
 UiTime+=Delta;PageAge+=Delta;CursorAge+=Delta;MenuIdleAge+=Delta;MenuCutAge+=Delta;MenuStampAge+=Delta;
 const int32 Shot=(Screen==0 || Screen==5)?int32(MenuIdleAge/7.f)%3:0;
 if(Shot!=AttractShot){AttractShot=Shot;MenuCutAge=0;}
 if(Announcer)Announcer->SetVolumeMultiplier(.85f*VoiceGain);for(auto Voice:FighterVoices)if(Voice)Voice->SetVolumeMultiplier(.82f*VoiceGain);
 const int32 Page=Screen*10+MenuPage+(bMenu?100:0)+(Screen==1?SelWho*1000:0);
 if(Page!=UiPageKey){UiPageKey=Page;PageAge=0;CursorAge=0;MenuCutAge=0;MenuIdleAge=0;}
 if(Screen==2 && MenuPalette==0)PaletteOverride=int32(PageAge/6.f)%6;
 const int32 Cursor=MenuAt+SelWho*100+StageAt*1000+(Screen==1?Setup.Costume[SelWho]*10000:0);
 if(Cursor!=UiCursorKey){UiCursorKey=Cursor;CursorAge=0;}
 // Stagger two small views: 15 Hz each, never a second full-resolution scene.
 PortraitDue-=Delta;if(PortraitDue>0 || Screen!=3 || bMenu || bNoHud || !bPlaced)return;
 PortraitDue=1.f/30.f;
 const int32 i=int32(UiTime*30.f)&1;
 if(!Body[i] || !Body[i]->GetSkinnedAsset())return;
 if(!PortraitCamera[i]) {
  auto* Target=NewObject<UTextureRenderTarget2D>(this);PortraitTarget[i]=Target;
  Target->ClearColor=FLinearColor(.018f,.025f,.045f);Target->InitCustomFormat(256,256,PF_B8G8R8A8,false);Target->UpdateResourceImmediate(true);
  auto* Capture=NewObject<USceneCaptureComponent2D>(this);PortraitCamera[i]=Capture;
  Capture->SetupAttachment(RootComponent);Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;
  Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
  Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;Capture->TextureTarget=Target;
  Capture->ShowFlags.SetAtmosphere(false);Capture->ShowFlags.SetFog(false);Capture->ShowFlags.SetVolumetricFog(false);
  Capture->ShowFlags.SetDynamicShadows(false);Capture->ShowFlags.SetMotionBlur(false);Capture->ShowFlags.SetTemporalAA(true);Capture->bAlwaysPersistRenderingState=true;
  Capture->PostProcessSettings.bOverride_AutoExposureMethod=true;Capture->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
  Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
  Capture->PostProcessSettings.bOverride_AutoExposureBias=true;Capture->PostProcessSettings.AutoExposureBias=0;
  Capture->FOVAngle=28;Capture->RegisterComponent();Capture->ShowOnlyComponent(Body[i]);
 }
 const FVector Head=Body[i]->GetBoneLocationByName(TEXT("head"),EBoneSpaces::WorldSpace);
 const FVector Eye=Head+Anchor.TransformVectorNoScale(FVector(-82,i?-45:45,3));
 PortraitCamera[i]->SetWorldLocationAndRotation(Eye,(Head-FVector(0,0,5)-Eye).Rotation());
 if(Cam){
  PortraitCamera[i]->PostProcessSettings=Cam->GetCameraComponent()->PostProcessSettings;
  PortraitCamera[i]->PostProcessSettings.bOverride_DepthOfFieldFstop=true;PortraitCamera[i]->PostProcessSettings.DepthOfFieldFstop=32.f;
  PortraitCamera[i]->PostProcessSettings.bOverride_VignetteIntensity=true;PortraitCamera[i]->PostProcessSettings.VignetteIntensity=0;
  PortraitCamera[i]->PostProcessSettings.bOverride_BloomIntensity=true;PortraitCamera[i]->PostProcessSettings.BloomIntensity=0;
 }
 PortraitCamera[i]->CaptureScene();
}

void AGkMatch::ApplyPersonality(int32 Who,FName Slot,float Frame,TArray<FTransform>& Pose)
{
 if(!Body[Who] || !Body[Who]->GetSkinnedAsset())return;
 const FReferenceSkeleton& Skeleton=Body[Who]->GetSkinnedAsset()->GetRefSkeleton();
 auto Twist=[&](const TCHAR* Bone,const FVector& Axis,float Angle){const int32 Index=Skeleton.FindBoneIndex(FName(Bone));if(Pose.IsValidIndex(Index)){
  FQuat Q=Pose[Index].GetRotation()*FQuat(Axis.GetSafeNormal(),FMath::DegreesToRadians(Angle));Q.Normalize();Pose[Index].SetRotation(Q);
 }};
 // Restrained secondary motion on authored clips: chin up, shoulders loose, not a new root path.
 if(Slot==TEXT("run")) {
  // Athletic legs, swept-back arms: a committed anime sprint instead of a guarded jog.
  if(const FGkClip* Flight=Clips[2].Find(TEXT("mv_HOME")))if(Flight->Anim){
   TArray<FTransform> Arms;EvalClip(Who,Flight->Anim,.2f*Flight->Anim->GetPlayLength(),Arms);
   const int32 Left=Skeleton.FindBoneIndex(TEXT("clavicle_l")),Right=Skeleton.FindBoneIndex(TEXT("clavicle_r"));
   for(int32 Bone=0;Bone<Pose.Num();++Bone){int32 Parent=Bone;while(Parent>=0 && Parent!=Left && Parent!=Right)Parent=Skeleton.GetParentIndex(Parent);
    if(Parent>=0 && Arms.IsValidIndex(Bone)){FTransform Mix;Mix.Blend(Pose[Bone],Arms[Bone],.85f);Pose[Bone]=Mix;}
   }
  }
  Twist(TEXT("spine_03"),FVector(0,1,0),-8);Twist(TEXT("head"),FVector(0,1,0),8);
 }
 if(Slot==TEXT("win")) {
  const float Beat=FMath::Sin(Frame*.055f);
  Twist(TEXT("head"),FVector(1,0,0),10+Beat*4);
  Twist(TEXT("spine_03"),FVector(1,0,0),-4);
 }
}

FLinearColor AGkMatch::CostumeColor(int32 Outfit,bool Accent)
{
 static const FLinearColor Primary[8]={{.47f,.065f,.06f},{.085f,.20f,.43f},{.85f,.35f,.025f},{.035f,.35f,.19f},{.30f,.07f,.51f},{.035f,.045f,.065f},{.78f,.80f,.74f},{.80f,.06f,.25f}};
 static const FLinearColor Secondary[8]={{.65f,.65f,.59f},{.65f,.65f,.59f},{.035f,.055f,.09f},{.72f,.64f,.38f},{.50f,.62f,.70f},{.78f,.43f,.09f},{.16f,.07f,.27f},{.045f,.48f,.50f}};
 return Accent?Secondary[FMath::Clamp(Outfit,0,7)]:Primary[FMath::Clamp(Outfit,0,7)];
}
const TCHAR* AGkMatch::CostumeName(int32 Outfit)
{
 static const TCHAR* Names[]={TEXT("CRIMSON"),TEXT("COBALT"),TEXT("SUNBURST"),TEXT("EMERALD"),TEXT("AMETHYST"),TEXT("OBSIDIAN"),TEXT("PEARL"),TEXT("CANDY VOLT")};
 return Names[FMath::Clamp(Outfit,0,7)];
}
