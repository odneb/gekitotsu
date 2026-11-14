#include "GkMatch.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/GameViewportClient.h"
#include "Materials/MaterialInstanceDynamic.h"

// Graphic staging is physical geometry. Gameplay stays in its untouched arena below.
void AGkMatch::UpdateMenuSet()
{
 const bool Studio=!bNoHud && Screen!=4 && Screen!=2 && (Screen!=3 || bMenu);
 const bool Stage=!bNoHud && Screen==2;
 const bool Call=!bNoHud && Screen==3 && !bMenu && CallHold>0 && CallAge<10;
 if(!PrintFinish){if(auto* M=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/GkUI/M_GkPrintFinish.M_GkPrintFinish")))PrintFinish=UMaterialInstanceDynamic::Create(M,this);}
 auto& PP=Cam->GetCameraComponent()->PostProcessSettings;
 if(PrintFinish)PP.AddBlendable(PrintFinish,Studio?1.f:0.f);
 if(ForestCel)PP.AddBlendable(ForestCel,Studio?0.f:1.f);
 PP.bOverride_SceneFringeIntensity=true;PP.SceneFringeIntensity=0;
 if(!Studio){if(!bForestLook){PP.bOverride_BloomIntensity=true;PP.BloomIntensity=.18f;}PP.bOverride_VignetteIntensity=true;PP.VignetteIntensity=.18f;PP.bOverride_ColorGamma=false;}
 for(auto Plane:MenuSetPlanes)if(Plane)Plane->SetVisibility(false);
 for(auto Model:MenuCostumes)if(Model)Model->SetVisibility(false);
 if(!Studio&&!Stage&&!Call)return;
 auto* Surface=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/GkUI/M_GkPrintSurface.M_GkPrintSurface"));if(!Surface)return;
 if(!MenuPaper){MenuPaper=UMaterialInstanceDynamic::Create(Surface,this);MenuBlack=UMaterialInstanceDynamic::Create(Surface,this);}
 const bool RedSet=Screen==5||Screen==7;
 MenuPaper->SetVectorParameterValue(TEXT("Tint"),RedSet?FLinearColor(.65,.004,.002):FLinearColor(.009,.006,.004));
 MenuBlack->SetVectorParameterValue(TEXT("Tint"),RedSet?FLinearColor(.009,.006,.004):FLinearColor(.65,.004,.002));
 if(Studio){
  Cam->SetActorLocationAndRotation(Wd(FVector(0,0,15000)),Anchor.GetRotation());Cam->GetCameraComponent()->SetFieldOfView(40.f);
  PP.bOverride_ColorSaturation=PP.bOverride_ColorContrast=PP.bOverride_ColorGain=PP.bOverride_ColorGamma=true;
  PP.ColorSaturation=PP.ColorContrast=PP.ColorGain=PP.ColorGamma=FVector4(1,1,1,1);
  PP.bOverride_VignetteIntensity=true;PP.VignetteIntensity=0;PP.bOverride_BloomIntensity=true;PP.BloomIntensity=0;
  PP.bOverride_DepthOfFieldFstop=true;PP.DepthOfFieldFstop=32;
 }
 FVector2D Size(1280,720);if(GEngine&&GEngine->GameViewport)GEngine->GameViewport->GetViewportSize(Size);
 const float Aspect=Size.Y/FMath::Max(1.f,float(Size.X));
 const float Tan=FMath::Tan(FMath::DegreesToRadians(Cam->GetCameraComponent()->FieldOfView*.5f));const FVector Origin=Cam->GetActorLocation();
 auto Point=[&](float D,float X,float Y){const float FW=2*D*Tan;return Origin+Cam->GetActorForwardVector()*D+Cam->GetActorRightVector()*((X-.5f)*FW)+Cam->GetActorUpVector()*((.5f-Y)*FW*Aspect);};
 auto Plane=[&](int I,float X,float Y,float W,float H,float D,float Roll,UMaterialInterface* Paint){
  auto& Mesh=MenuSetPlanes[I];if(!Mesh){Mesh=NewObject<UStaticMeshComponent>(this);Mesh->SetupAttachment(RootComponent);Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetCastShadow(false);Mesh->RegisterComponent();}
  Mesh->SetVisibility(true);Mesh->SetMaterial(0,Paint);FRotator R=Cam->GetActorRotation();R.Roll+=Roll;
  Mesh->SetWorldLocationAndRotation(Point(D,X,Y),R);Mesh->SetWorldScale3D(FVector(.005f,2*D*Tan*W/100.f,2*D*Tan*Aspect*H/100.f));
 };
 if(Stage){Plane(0,.035,.50,.58,2,175,-11,MenuPaper);Plane(1,.39,.04,.025,2,180,-11,MenuBlack);return;}
 if(Call){
  const float K=FMath::Clamp((10.f-CallAge)/5.f,0.f,1.f);
  Plane(0,.5,.41,.92,.27f*K,170,-7,MenuPaper);Plane(1,.53,.43,1.15,.035f*K,165,-13,MenuBlack);return;
 }
 Plane(0,.5,.5,2,2,950,0,MenuPaper);
 Plane(1,.08,.55,.10,2,900,-30,MenuBlack);
 Plane(2,.43,.65,.08,2,895,-30,MenuBlack);
 Plane(3,.94,.55,.22,2,890,-30,MenuBlack);
 Plane(4,.64,1.05,1.5,.08,885,-15,MenuBlack);
 if(Screen==0 && MenuPage==0){const float Rows[]={.48,.60,.72,.86};Plane(5,.17,Rows[FMath::Clamp(MenuAt,0,3)],.39,.135,158,-3,MenuBlack);}
 const int Who=Screen==1?SelWho:Screen==6?FMath::Clamp(S.winner,0,1):0;
 for(int I=0;I<2;++I){if(!Body[I])continue;auto* B=Body[I].Get();bool Show=(Screen==7||I==Who) && MenuPage==0;B->SetVisibility(Show);if(!Show)continue;
  const bool Opening=Screen==5;const float D=Opening?(AttractShot==0?180.f:AttractShot==1?210.f:235.f):Screen==7?380.f:Screen==1?320.f:Screen==6?720.f:360.f;
  const float X=Opening?(AttractShot==1?.80f:.84f):Screen==7?(I?.79f:.21f):.73f,Y=Opening?.20f:Screen==6?.52f:.19f;
  const FVector Head=B->GetComponentTransform().InverseTransformPosition(B->GetBoneLocationByName(TEXT("head"),EBoneSpaces::WorldSpace));
  const float Response=Screen==1?10.f*FMath::Exp(-CursorAge*8):0;
  const FQuat Q=Anchor.GetRotation()*FRotator(0,Opening?(AttractShot==0?110.f:AttractShot==1?65.f:120.f):(Screen==0&&AttractShot==1?105.f:I?105.f:75.f)+Response,0).Quaternion();
  FVector Pivot=Head;
  if(Screen==6){
   float Low=1e9f,High=-1e9f;
   const auto& Skeleton=B->GetSkinnedAsset()->GetRefSkeleton();
   for(const TCHAR* Name:{TEXT("head"),TEXT("hand_l"),TEXT("hand_r"),TEXT("foot_l"),TEXT("foot_r")}){
    int Bone=Skeleton.FindBoneIndex(FName(Name));if(!B->BoneSpaceTransforms.IsValidIndex(Bone))continue;
    FTransform Pose=B->BoneSpaceTransforms[Bone];for(int Parent=Skeleton.GetParentIndex(Bone);Parent>=0;Parent=Skeleton.GetParentIndex(Parent))Pose=Pose*B->BoneSpaceTransforms[Parent];
    Low=FMath::Min(Low,float(Pose.GetTranslation().Z));High=FMath::Max(High,float(Pose.GetTranslation().Z));
   }
   if(High>Low)Pivot=FVector(0,0,(Low+High)*.5f);
  }
  B->SetWorldScale3D(FVector(1));B->SetWorldLocationAndRotation(Point(D,X,Y)-Q.RotateVector(Pivot),Q);
 }
 if(Screen==1 && MenuPage==0 && Body[Who] && Body[Who]->GetSkinnedAsset()){
  auto* Source=Body[Who].Get();const FVector Key=Anchor.TransformVectorNoScale(FVector(-.65,-.35,.68)).GetSafeNormal();
  for(int C=0;C<8;++C){auto& Model=MenuCostumes[C];if(!Model){
   Model=NewObject<UPoseableMeshComponent>(this);Model->SetupAttachment(RootComponent);Model->SetCollisionEnabled(ECollisionEnabled::NoCollision);Model->SetCastShadow(false);Model->RegisterComponent();Model->SetSkinnedAssetAndUpdate(Source->GetSkinnedAsset());
   for(int M=0;M<Model->GetNumMaterials();++M){auto* Base=BodyMid[Who][FMath::Min(3,M)];if(!Base)continue;auto* Mat=UMaterialInstanceDynamic::Create(Base->Parent,this);Mids.Add(Mat);Model->SetMaterial(M,Mat);
    Mat->SetVectorParameterValue(TEXT("Paint"),M==0?CostumeColor(C):CostumeColor(C,true));Mat->SetVectorParameterValue(TEXT("Shadow"),FLinearColor(.009,.006,.004));Mat->SetVectorParameterValue(TEXT("KeyTint"),FLinearColor::White);Mat->SetVectorParameterValue(TEXT("Key"),FLinearColor(Key.X,Key.Y,Key.Z));Mat->SetVectorParameterValue(TEXT("Rim"),FLinearColor(.66,.48,.27));
   }
   if(InkMid[Who]){auto* Outline=UMaterialInstanceDynamic::Create(InkMid[Who]->Parent,this);Mids.Add(Outline);Outline->SetVectorParameterValue(TEXT("Outlines Color"),FLinearColor(.006,.004,.003));Outline->SetScalarParameterValue(TEXT("Outlines_Thickness"),.12f);Model->SetOverlayMaterial(Outline);}
  }
  Model->SetVisibility(true);Model->BoneSpaceTransforms=Source->BoneSpaceTransforms;Model->MarkRefreshTransformDirty();const bool Selected=C==Setup.Costume[Who];
  const float Scale=Selected?.125f:.105f;Model->SetWorldScale3D(FVector(Scale));Model->SetWorldLocationAndRotation(Point(220,.078f+C*.12f,.875f),Anchor.GetRotation()*FRotator(0,85+FMath::Sin(UiTime*1.3f+C)*5,0).Quaternion());
  }
 }
}
