#include "GkMatch.h"
#include "Text3DComponent.h"
#include "Extensions/Text3DDefaultMaterialExtension.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Font.h"
#include "Fonts/CompositeFont.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/GameViewportClient.h"
#include "Misc/Paths.h"

void AGkMatch::UpdateSpatialType(float Delta)
{
 if(!Cam || !bPlaced)return;
 if(!SpatialFont){
  SpatialFont=NewObject<UFont>(this);SpatialFont->FontCacheType=EFontCacheType::Runtime;
  SpatialFont->GetMutableInternalCompositeFont()=FCompositeFont(FName(TEXT("Black")),FPaths::ProjectContentDir()/TEXT("GkUI/Fonts/Machina/GekitotsuMachina.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
  if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/GkUI/M_GkPrintSurface.M_GkPrintSurface"))){
   SpatialFront=UMaterialInstanceDynamic::Create(Material,this);SpatialFront->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.91,.72,.44));
   SpatialSide=UMaterialInstanceDynamic::Create(Material,this);SpatialSide->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.009,.007,.005));
  }
 }
 if(!SpatialFront)return;
 UpdateMenuSet();
 FVector2D Size(1280,720);if(GEngine && GEngine->GameViewport)GEngine->GameViewport->GetViewportSize(Size);
 const float Depth=140.f,Width=2*Depth*FMath::Tan(FMath::DegreesToRadians(Cam->GetCameraComponent()->FieldOfView*.5f)),Height=Width*Size.Y/FMath::Max(1.f,float(Size.X));
 auto Show=[&](int32 Index,const FString& Word,float X,float Y,float H,float MaxW,float Kick){
  auto& Type=SpatialType[Index];
  if(Word.IsEmpty()){if(Type)Type->SetVisibility(false,true);return;}
  if(!Type){
   Type=NewObject<UText3DComponent>(this);Type->SetupAttachment(RootComponent);Type->RegisterComponent();
   Type->SetFont(SpatialFont);Type->SetTypeface(TEXT("Black"));Type->SetFontSize(40);Type->SetExtrude(5);Type->SetBevel(.7f);Type->SetBevelSegments(1);
   Type->SetHorizontalAlignment(EText3DHorizontalTextAlignment::Center);Type->SetVerticalAlignment(EText3DVerticalTextAlignment::Center);
   if(auto* Material=Type->GetCastedMaterialExtension<UText3DDefaultMaterialExtension>()){Material->SetStyle(EText3DMaterialStyle::Custom);Material->SetUseSingleMaterial(false);}
   Type->OnTextGenerated().AddLambda([Text=Type.Get()](){
    TArray<USceneComponent*> Children;Text->GetChildrenComponents(true,Children);
    for(auto* Child:Children)if(auto* Primitive=Cast<UPrimitiveComponent>(Child))Primitive->SetCastShadow(false);
   });
   Type->SetFrontMaterial(SpatialFront);Type->SetBevelMaterial(SpatialSide);Type->SetExtrudeMaterial(SpatialSide);Type->SetBackMaterial(SpatialSide);
  }
  if(Type->GetText().ToString()!=Word){Type->SetHasMaxWidth(true);Type->SetMaxWidth(FMath::Max(24.f,Word.Len()*20.f));Type->SetText(FText::FromString(Word));}
  Type->SetVisibility(true,true);Type->SetFrontMaterial(SpatialFront);
  const float Scale=FMath::Min(Height*H/30.f,Width*MaxW/FMath::Max(24.f,Word.Len()*20.f))*(1+Kick*.22f);
  Type->SetWorldScale3D(FVector(Scale,Scale,Scale));
  const FVector Position=Cam->GetActorLocation()+Cam->GetActorForwardVector()*Depth+Cam->GetActorRightVector()*((X-.5f)*Width)+Cam->GetActorUpVector()*((.5f-Y)*Height);
  FRotator Rotation=Cam->GetActorRotation();Rotation.Roll+=-1.f-Kick*5.f;Rotation.Yaw+=-5.f+Kick*14.f;
  Type->SetWorldLocationAndRotation(Position,Rotation);
 };
 for(int i=0;i<8;++i)if(SpatialType[i])SpatialType[i]->SetVisibility(false,true);
 // Physical title: two watertight extruded meshes, separately animated, no image plane.
 const bool TitleVisible=!bNoHud&&!bMenu&&MenuPage==0&&(Screen==5||Screen==0);
 for(int i=0;i<2;++i){
  if(TitleVisible&&!TitleMesh[i]){
   const TCHAR* Paths[]={TEXT("/Game/GkUI/SM_GkTitleLatin.SM_GkTitleLatin"),TEXT("/Game/GkUI/SM_GkTitleKanji.SM_GkTitleKanji")};
   if(auto* Mesh=LoadObject<UStaticMesh>(nullptr,Paths[i])){
    TitleMesh[i]=NewObject<UStaticMeshComponent>(this);TitleMesh[i]->SetupAttachment(RootComponent);TitleMesh[i]->SetStaticMesh(Mesh);TitleMesh[i]->SetCollisionEnabled(ECollisionEnabled::NoCollision);TitleMesh[i]->SetCastShadow(false);TitleMesh[i]->RegisterComponent();
    if(!TitleRed){TitleRed=UMaterialInstanceDynamic::Create(SpatialFront->Parent,this);TitleRed->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.75,.006,.003));}
    TitleMesh[i]->SetMaterial(0,i?TitleRed.Get():SpatialFront.Get());
   }
  }
  if(TitleMesh[i]){
   TitleMesh[i]->SetVisibility(TitleVisible);
   if(TitleVisible){
    const bool Opening=Screen==5;
    const float D=Opening?(i?370.f:400.f):(i?170.f:180.f);
    const float FW=2*D*FMath::Tan(FMath::DegreesToRadians(Cam->GetCameraComponent()->FieldOfView*.5f)),FH=FW*Size.Y/Size.X;
    const float K=FMath::Exp(-FMath::Max(0.f,PageAge-.08f*i)*7.f);
    const float X=Opening?(i?.42f:.46f):.19f,Y=Opening?(i?.46f:.43f):(i?.20f:.16f);
    const float Sy=Opening?(i?FW*.58f/109.f:FW*.86f/200.f):(i?FW*.22f/109.f:FW*.28f/200.f);
    const float Sz=Opening?(i?FH*.66f/53.f:FH*.78f/55.f):(i?FH*.14f/53.f:Sy*1.45f);
    FVector P=Cam->GetActorLocation()+Cam->GetActorForwardVector()*D+Cam->GetActorRightVector()*((X-.5f)*FW-K*FW*.5f)+Cam->GetActorUpVector()*((.5f-Y)*FH);
    FRotator R=Cam->GetActorRotation();R.Yaw+=-3.f+FMath::Sin(UiTime*.35f)*1.5f+K*20;
    TitleMesh[i]->SetWorldLocationAndRotation(P,R);TitleMesh[i]->SetWorldScale3D(FVector(Sy,Sy,Sz));
   }
  }
 }
 if(bNoHud)return;
 float Kick=FMath::Exp(-CursorAge*10);
 if(MenuPage || bMenu){
  const FString Word=MenuPage==4?TEXT("OPTIONS"):MenuPage==1?TEXT("YOUR BUTTONS"):MenuPage==2?TEXT("SOUND + FX"):MenuPage==3?TEXT("MOVE LIST"):bDebugMenu?TEXT("BACKSTAGE"):TEXT("PAUSE");
  Show(0,Word,.43f,.15f,.14f,.78f,FMath::Exp(-PageAge*10));
 }else if(Screen==0){
  Show(0,TEXT("VERSUS"),.17f,.48f,.065f,.30f,MenuAt==0?Kick:0);
  Show(3,TEXT("TRAINING"),.17f,.60f,.065f,.30f,MenuAt==1?Kick:0);
  Show(4,TEXT("OPTIONS"),.17f,.72f,.065f,.30f,MenuAt==2?Kick:0);
  Show(5,TEXT("QUIT"),.12f,.86f,.040f,.18f,MenuAt==3?Kick:0);
  const int Indices[]={0,3,4,5};for(int k=0;k<4;++k)if(SpatialType[Indices[k]])SpatialType[Indices[k]]->SetFrontMaterial(MenuAt==k?SpatialSide:SpatialFront);
 }else if(Screen==1){
  Show(0,TEXT("ROBOT"),.23f,.30f,.16f,.41f,FMath::Exp(-PageAge*9));
  Show(6,TEXT("01"),.20f,.48f,.15f,.26f,0);if(SpatialType[6] && TitleRed)SpatialType[6]->SetFrontMaterial(TitleRed);
  Show(3,CostumeName(Setup.Costume[SelWho]),.22f,.605f,.045f,.33f,Kick);
 }else if(Screen==2){Show(0,TEXT("DREAD"),.19f,.29f,.13f,.34f,0);Show(3,TEXT("MARSH"),.19f,.45f,.13f,.34f,0);}
 else if(Screen==6){Show(0,TEXT("VICTORY"),.30f,.33f,.18f,.55f,FMath::Exp(-PageAge*9));}
 else if(Screen==7){Show(0,TEXT("VS"),.5f,.45f,.30f,.40f,FMath::Exp(-PageAge*9));}
 else if(Screen==3 && CallHold>0){const float Exit=FMath::SmoothStep(8.f,18.f,float(CallAge));Show(0,CallWord,.5f,FMath::Lerp(.40f,.16f,Exit),FMath::Lerp(.20f,.045f,Exit),FMath::Lerp(.70f,.26f,Exit),FMath::Exp(-CallAge*.23f));}
 else if(Screen==3 && S.phase==gk::PH_OVER){Show(0,TEXT("VICTORY"),.5f,.40f,.20f,.80f,0);}
 if(Screen==3&&!bMenu)for(int i=0;i<2;++i){const bool Visible=Combo[i]>=2&&ComboTimer[i]>0;Show(i+1,Visible?FString::FromInt(Combo[i]):FString(),i?.86f:.14f,.30f,.16f,.18f,FMath::Exp(-FMath::Max(0,80-ComboTimer[i])*.2f));}
}
