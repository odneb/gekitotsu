#include "GkMatch.h"
#include "Text3DComponent.h"
#include "Extensions/Text3DDefaultMaterialExtension.h"
#include "Components/PrimitiveComponent.h"
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
  SpatialFont->GetMutableInternalCompositeFont()=FCompositeFont(FName(TEXT("Black")),FPaths::EngineContentDir()/TEXT("Slate/Fonts/Roboto-Black.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
  if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/GkUI/M_GkInkType.M_GkInkType"))){
   SpatialFront=UMaterialInstanceDynamic::Create(Material,this);SpatialFront->SetVectorParameterValue(TEXT("Ink"),FLinearColor(.98,.84,.61));
   SpatialSide=UMaterialInstanceDynamic::Create(Material,this);SpatialSide->SetVectorParameterValue(TEXT("Ink"),FLinearColor(.64,.025,.018));
  }
 }
 if(!SpatialFront)return;
 FVector2D Size(1280,720);if(GEngine && GEngine->GameViewport)GEngine->GameViewport->GetViewportSize(Size);
 const float Depth=140.f,Width=2*Depth*FMath::Tan(FMath::DegreesToRadians(Cam->GetCameraComponent()->FieldOfView*.5f)),Height=Width*Size.Y/FMath::Max(1.f,float(Size.X));
 auto Show=[&](int32 Index,const FString& Word,float X,float Y,float H,float MaxW,float Kick){
  auto& Type=SpatialType[Index];
  if(Word.IsEmpty()){if(Type)Type->SetVisibility(false,true);return;}
  if(!Type){
   Type=NewObject<UText3DComponent>(this);Type->SetupAttachment(RootComponent);Type->RegisterComponent();
   Type->SetFont(SpatialFont);Type->SetTypeface(TEXT("Black"));Type->SetFontSize(40);Type->SetExtrude(7);Type->SetBevel(.7f);Type->SetBevelSegments(1);
   Type->SetHorizontalAlignment(EText3DHorizontalTextAlignment::Center);Type->SetVerticalAlignment(EText3DVerticalTextAlignment::Center);
   if(auto* Material=Type->GetCastedMaterialExtension<UText3DDefaultMaterialExtension>()){Material->SetStyle(EText3DMaterialStyle::Custom);Material->SetUseSingleMaterial(false);}
   Type->OnTextGenerated().AddLambda([Text=Type.Get()](){
    TArray<USceneComponent*> Children;Text->GetChildrenComponents(true,Children);
    for(auto* Child:Children)if(auto* Primitive=Cast<UPrimitiveComponent>(Child))Primitive->SetCastShadow(false);
   });
   Type->SetFrontMaterial(SpatialFront);Type->SetBevelMaterial(SpatialSide);Type->SetExtrudeMaterial(SpatialSide);Type->SetBackMaterial(SpatialSide);
  }
  if(Type->GetText().ToString()!=Word){Type->SetHasMaxWidth(true);Type->SetMaxWidth(FMath::Max(24.f,Word.Len()*24.f));Type->SetText(FText::FromString(Word));}
  Type->SetVisibility(true,true);
  const float Scale=FMath::Min(Height*H/30.f,Width*MaxW/FMath::Max(24.f,Word.Len()*24.f))*(1+Kick*.22f);
  Type->SetWorldScale3D(FVector(Scale,Scale*.86f,Scale));
  const FVector Position=Cam->GetActorLocation()+Cam->GetActorForwardVector()*Depth+Cam->GetActorRightVector()*((X-.5f)*Width)+Cam->GetActorUpVector()*((.5f-Y)*Height);
  FRotator Rotation=Cam->GetActorRotation();Rotation.Roll+=-5.f-Kick*12.f;Rotation.Yaw+=-8.f+Kick*20.f;
  Type->SetWorldLocationAndRotation(Position,Rotation);
 };
 FString Headline;float X=.72f,Y=.24f,H=.12f,MaxW=.45f,Kick=FMath::Exp(-PageAge*10);
 if(!bMenu && MenuPage==0 && !bNoHud){
  if(Screen==0){static const TCHAR* Choices[]={TEXT("VERSUS CPU"),TEXT("TRAINING"),TEXT("WATCH"),TEXT("CPU LEVEL"),TEXT("CONTROLS"),TEXT("HOW TO PLAY"),TEXT("SOUND & FX"),TEXT("QUIT GAME")};Headline=Choices[FMath::Clamp(MenuAt,0,7)];Y=.80f;H=.11f;Kick=FMath::Exp(-CursorAge*12);}
  else if(Screen==1){Headline=CostumeName(Setup.Costume[SelWho]);Y=.79f;H=.10f;Kick=FMath::Exp(-CursorAge*12);}
  else if(Screen==2){Headline=TEXT("DREADMARSH");X=.63f;Y=.36f;MaxW=.64f;H=.18f;}
  else if(Screen==6){Headline=TEXT("VICTORY");Y=.22f;}
  else if(Screen==7){Headline=TEXT("VERSUS");X=.5f;Y=.46f;H=.22f;}
  else if(Screen==3 && CallHold>0){Headline=CallWord;X=.5f;Y=.40f;H=.22f;MaxW=.82f;Kick=FMath::Exp(-CallAge*.23f);}
 }
 if(!bNoHud && (bMenu || MenuPage>0)){
  Headline=MenuPage==1?TEXT("YOUR CONTROLS"):MenuPage==2?TEXT("SOUND & FX"):MenuPage==3?TEXT("MAKE TROUBLE"):bDebugMenu?TEXT("BACKSTAGE"):TEXT("TIME OUT");
  X=.73f;Y=.26f;H=.13f;MaxW=.43f;
 }
 Show(0,Headline,X,Y,H,MaxW,Kick);
 for(int32 i=0;i<2;++i){
  const bool Visible=Screen==3&&!bMenu&&!bNoHud&&Combo[i]>=2&&ComboTimer[i]>0;
  Show(i+1,Visible?FString::FromInt(Combo[i]):FString(),i?.86f:.14f,.30f,.15f,.18f,FMath::Exp(-FMath::Max(0,80-ComboTimer[i])*.2f));
 }
}
