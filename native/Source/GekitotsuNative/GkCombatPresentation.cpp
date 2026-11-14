#include "GkMatch.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SpotLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"

void AGkMatch::LoadDirector()
{
 const FString File=FPaths::ProjectContentDir()/TEXT("Gk/cinematography.txt");
 FString Text;if(FFileHelper::LoadFileToString(Text,*File) && Director.Load(TCHAR_TO_UTF8(*Text))){
  DirectorFileTime=IFileManager::Get().GetTimeStamp(*File);
  UE_LOG(LogTemp,Display,TEXT("GkDirector loaded %d cues"),int(Director.cues.size()));
 } else UE_LOG(LogTemp,Warning,TEXT("GkDirector invalid or missing timeline; retaining last valid cues"));
 FString Surfaces;if(FFileHelper::LoadFileToString(Surfaces,*(FPaths::ProjectContentDir()/TEXT("Gk/surfaces.txt")))){
  TArray<FString> Rows;Surfaces.ParseIntoArrayLines(Rows);for(const FString& Row:Rows){if(Row.StartsWith(TEXT("#")))continue;TArray<FString> V;Row.ParseIntoArray(V,TEXT("|"));if(V.Num()!=7)continue;
   FSurfaceResponse Profile;Profile.Tint=FLinearColor(FCString::Atof(*V[1]),FCString::Atof(*V[2]),FCString::Atof(*V[3]));Profile.Dust=FCString::Atof(*V[4]);Profile.Speed=FCString::Atof(*V[5]);Profile.Wake=FCString::Atof(*V[6]);SurfaceProfiles.Add(V[0],Profile);
  }
 }
 bDirectorLoaded=true;
 if(!MangaMid)if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/GkFx/M_GkCombatPrint.M_GkCombatPrint")))MangaMid=UMaterialInstanceDynamic::Create(Material,this);
 if(auto* C=IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth")))C->Set(3,ECVF_SetByCode);
 for(int i=0;i<2;++i)if(Body[i]){Body[i]->SetRenderCustomDepth(true);Body[i]->SetCustomDepthStencilValue(i+1);}
}
void AGkMatch::DirectEvent(const gk::Event& E)
{
 if(!bDirectorLoaded)LoadDirector();
 if(!bDirectorEnabled || Screen!=3 || bMenu)return;
 const int Who=E.who>=0?E.who&1:0;const auto& P=S.p[Who];
 const char* Cue=nullptr;
 const bool HeavyNormal=P.mv==gk::moveByKey("5HP") || P.mv==gk::moveByKey("5HK") || P.mv==gk::moveByKey("2HP") || P.mv==gk::moveByKey("2HK") || P.mv==gk::moveByKey("jHP") || P.mv==gk::moveByKey("jHK");
 switch(E.type){
 case gk::E_HIT:
  Cue=(E.f&gk::H_KO)?"ko":(E.f&gk::H_FINISH)?"finish":(E.f&gk::H_IMPACT)?"impact":E.e?"counter":(E.a>=2 || HeavyNormal)?"heavy":nullptr;
  if(E.f&(gk::H_FINISH|gk::H_IMPACT))SurfaceResponse(At(E.x,0),1.1f,P.face);
  break;
 case gk::E_DI:Cue="drive";break;
 case gk::E_ARMOR:Cue="armor";break;
 case gk::E_DICLASH:Cue="impact";break;
 case gk::E_LAND:Cue="land";break;
 case gk::E_HOME:Cue="flight";break;
 case gk::E_PPARRY:Cue="parry";break;
 case gk::E_SUPER:Cue="super";break;
 case gk::E_EX:Cue="od";break;
 case gk::E_FIRE:if(P.mv>=0 && gk::move(P.mv).od)Cue="release";break;
 case gk::E_KO:if(Director.Out.priority<100)Cue="ko";break;
 case gk::E_GBOUNCE:case gk::E_KD:Cue="land";SurfaceResponse(At(P.x,0),1.2f,0);break;
 default:break;
 }
 if(Cue){
  DirectorContact=At(E.x,E.y,-12);if(E.type==gk::E_SUPER || E.type==gk::E_DI)DirectorContact=At(P.x,P.y+55*gk::F,-12);
  Director.Start(Cue,Who,P.face,S.freeze>0 || P.hs>0 || S.phase==gk::PH_KO);
  UE_LOG(LogTemp,Verbose,TEXT("GkDirector %s player%d frame%d"),UTF8_TO_TCHAR(Cue),Who,S.frame);
 }
}
void AGkMatch::UpdateDirector(float Delta)
{
 if(!bDirectorLoaded)LoadDirector();
 if(Screen==3 && !bMenu)PressureAge+=Delta;
 Director.enabled=bDirectorEnabled;
 DirectorReloadDue-=Delta;
 if(DirectorReloadDue<=0){DirectorReloadDue=2.f;const FString File=FPaths::ProjectContentDir()/TEXT("Gk/cinematography.txt");
  if(IFileManager::Get().GetTimeStamp(*File)!=DirectorFileTime)LoadDirector();
 }
 const bool Active=Screen==3 && !bMenu;
 Director.Tick(Delta,Active,FlashGain,ShakeGain,S.freeze>0 || S.p[0].hs>0 || S.p[1].hs>0 || S.phase==gk::PH_KO);
 if(!Active)return;
 for(int i=0;i<2;++i){const auto& P=S.p[i];
  if(P.st==gk::ATK && P.mv>=0){const auto& M=gk::move(P.mv);
   if((P.mv==gk::moveByKey("SA3") || P.mv==gk::moveByKey("CA") || P.mv==gk::moveByKey("ABEAM")) && P.mf>=M.hits[0].s && DirectorBeamFrame[i]<M.hits[0].s && P.hs==0)
    {Director.Start("release",i,P.face,false);SurfaceResponse(At(P.x,0),1.2f,-P.face);}
  }
  DirectorBeamFrame[i]=P.st==gk::ATK?P.mf:-1;
 }
}
void AGkMatch::ApplyDirector()
{
 if(!Cam)return;
 const auto& D=Director.Out;auto* Camera=Cam->GetCameraComponent();
 if(MangaMid){
  const bool Active=bDirectorEnabled && Screen==3 && !bMenu;
  Camera->PostProcessSettings.AddBlendable(MangaMid,Active?1.f:0.f);
  MangaMid->SetScalarParameterValue(TEXT("Mode"),D.mode);
  MangaMid->SetScalarParameterValue(TEXT("Amount"),D.ink);
  MangaMid->SetScalarParameterValue(TEXT("Dim"),D.dim);
  MangaMid->SetScalarParameterValue(TEXT("Lines"),D.lines);
  MangaMid->SetScalarParameterValue(TEXT("Beat"),float(int(Director.clock)/3));
  const FLinearColor Accent=D.mode==3?FLinearColor(.3f,.9f,1.f):CostumeColor(Setup.Costume[D.who]);
  MangaMid->SetVectorParameterValue(TEXT("Accent"),Accent.GetClamped(0,1));
  FVector2D Focus(.5,.5);if(auto* PC=GetWorld()->GetFirstPlayerController()){
   FVector2D Pixel;int X,Y;PC->GetViewportSize(X,Y);
   if(X>0 && Y>0 && PC->ProjectWorldLocationToScreen(Wd(DirectorContact),Pixel))Focus=FVector2D(FMath::Clamp(Pixel.X/X,.2,.8),FMath::Clamp(Pixel.Y/Y,.2,.8));
  }
  MangaMid->SetVectorParameterValue(TEXT("Focus"),FLinearColor(Focus.X,Focus.Y,0,0));
 }
 if(Screen!=3 || bMenu || !bDirectorEnabled)return;
 // Small lens punctuation keeps both players in view; extreme authored shots belong to super freeze.
 Camera->SetFieldOfView(Camera->FieldOfView*(1.f-D.zoom));
 auto Rot=Cam->GetActorRotation();Rot.Roll+=D.roll;Cam->SetActorRotation(Rot);
 const FLinearColor Energy(.12f,.65f,1.f);
 for(int i=0;i<2;++i)for(int k=0;k<4;++k)if(auto* Material=BodyMid[i][k]){
  if(D.rim>0)Material->SetVectorParameterValue(TEXT("Rim"),ForestRim*(1.f+D.rim*.7f));
  const auto& Self=S.p[i];if(Self.st==gk::ATK && Self.mv>=0 && gk::move(Self.mv).di){Material->SetVectorParameterValue(TEXT("Rim"),FLinearColor(.05,1.2,1.6));Material->SetVectorParameterValue(TEXT("KeyTint"),FLinearColor(1.05,.85,.95));}
  const int Other=1-i;const auto& A=S.p[Other];
  const bool Beam=A.st==gk::ATK && A.mv>=0 && (A.mv==gk::moveByKey("SA3") || A.mv==gk::moveByKey("CA") || A.mv==gk::moveByKey("ABEAM")) && A.mf>=gk::move(A.mv).hits[0].s && A.mf<=gk::move(A.mv).lastAct;
  if(Beam){const FVector L=Anchor.TransformVectorNoScale(FVector(-.35f,A.x<S.p[i].x?-1.f:1.f,.25f)).GetSafeNormal();
   Material->SetVectorParameterValue(TEXT("Key"),FLinearColor(L.X,L.Y,L.Z));Material->SetVectorParameterValue(TEXT("KeyTint"),FLinearColor(.55f,.9f,1.3f));Material->SetVectorParameterValue(TEXT("Rim"),Energy*2.f);
  }
 }
 if(Music)Music->SetVolumeMultiplier(MusicGain*MusicVol*SongGain*(1.f-D.duck)*(bEntrance?.65f:1.f));
}
void AGkMatch::SurfaceResponse(const FVector& Point,float Strength,float Direction)
{
 const FVector Feet=FloorPoint(Point)+FVector(0,0,4);
 const bool Wet=ShrineWater || Storm>.48f;
 const FString Key=Wet?TEXT("wet"):Kind==1?TEXT("sand"):(AirKind==1 || AirKind==2)?TEXT("foliage"):TEXT("forest");
 FSurfaceResponse Profile;Profile.Tint=FLinearColor(.24f,.30f,.15f);if(auto* Found=SurfaceProfiles.Find(Key))Profile=*Found;
 PressureAt=Feet;PressureAge=0;PressureStrength=Strength*Profile.Wake;
 // Broken low brush curls and ballistic grit; keep the ground plane readable.
 if(Strength>.3f)for(int Side:{-1,1})for(int k=0;k<3;++k){
  const FVector A=Feet+FVector(-10,Side*(8+k*12),2+k*3),B=Feet+FVector(-10,Side*(35+k*22)*Strength,10+(2-k)*14*Strength);
  Fx.Stroke(A,B,(Wet?2.f:7.f)*Strength,.18f,Wet?FLinearColor(.4,.75,1):FLinearColor(.48,.37,.22));
 }
 const FLinearColor Color=Profile.Tint;
 // Small ballistic fragments, not large translucent circles intersecting the floor.
 for(int k=0;k<int(6+Strength*5);++k){const float Side=Direction==0?(k&1?1.f:-1.f):Direction;
  auto& Chip=Fx.Add(2,Feet+FVector(-8,Side*4,2),FVector(Fx.Rng.FRandRange(-20,20),Side*Fx.Rng.FRandRange(70,180)*Strength*Profile.Speed,Fx.Rng.FRandRange(70,180)*Strength),.30f+.10f*Strength,Wet?2.f:3.f,.5f,Color,.8f);
  Chip.Grav=800;Chip.Stretch=Wet?2.5f:1.f;
 }
 if(Wet)Snd(TEXT("Land_08"),.18f*Strength,1.3f);
}
