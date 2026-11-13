#include "GkMatch.h"
#include "NiagaraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

void AGkMatch::EnergyOrb(int32 Who,float Radius)
{
 if(!FilmOrb[Who])
 {
  UStaticMesh* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
  UMaterialInterface* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/GkFx/M_GkFilmEnergy.M_GkFilmEnergy"));
  if(!Sphere || !Material) return;
  FilmOrb[Who]=NewObject<UStaticMeshComponent>(this);
  FilmOrb[Who]->SetupAttachment(RootComponent);FilmOrb[Who]->SetStaticMesh(Sphere);FilmOrb[Who]->SetMaterial(0,Material);
  FilmOrb[Who]->SetCollisionEnabled(ECollisionEnabled::NoCollision);FilmOrb[Who]->SetCastShadow(false);
  FilmOrb[Who]->SetMobility(EComponentMobility::Movable);FilmOrb[Who]->RegisterComponent();
  FilmEnergyLight[Who]=NewObject<UPointLightComponent>(this);FilmEnergyLight[Who]->SetupAttachment(RootComponent);
  FilmEnergyLight[Who]->SetMobility(EComponentMobility::Movable);FilmEnergyLight[Who]->SetCastShadows(false);
  FilmEnergyLight[Who]->SetIntensityUnits(ELightUnits::Unitless);FilmEnergyLight[Who]->SetLightColor(FLinearColor(.08f,.5f,1.f));
  FilmEnergyLight[Who]->SetAttenuationRadius(850.f);FilmEnergyLight[Who]->SetVolumetricScatteringIntensity(.35f);
  FilmEnergyLight[Who]->RegisterComponent();
 }
 const gk::Fighter& P=S.p[Who];
 FVector Pos=At(P.x+P.face*34*gk::F,P.y+56*gk::F);
 if(Body[Who] && Body[Who]->GetBoneIndex(TEXT("hand_l"))!=INDEX_NONE && Body[Who]->GetBoneIndex(TEXT("hand_r"))!=INDEX_NONE)
 {
  Pos=Anchor.InverseTransformPosition((Body[Who]->GetBoneLocationByName(TEXT("hand_l"),EBoneSpaces::WorldSpace)+Body[Who]->GetBoneLocationByName(TEXT("hand_r"),EBoneSpaces::WorldSpace))*.5);
 }
 Pos.Y+=P.face*Radius*.25f;
 FilmOrb[Who]->SetRelativeLocation(Pos);FilmOrb[Who]->SetRelativeScale3D(FVector(Radius/50.f));FilmOrb[Who]->SetVisibility(true);
 FilmEnergyLight[Who]->SetRelativeLocation(Pos+FVector(-70,0,0));FilmEnergyLight[Who]->SetIntensity(1400.f+Radius*90.f);FilmEnergyLight[Who]->SetVisibility(true);
}

#include "EngineUtils.h"
#include "UObject/UnrealType.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/Paths.h"

void AGkMatch::DumpFilmFog()
{
 UE_LOG(LogTemp,Display,TEXT("FOG bridge %s tracked actors%d"),*GetPathNameSafe(FilmNinja.Get()),Stirrers.Num());
 for(TActorIterator<AActor> It(GetWorld());It;++It)
 {
  if(It->GetClass()->GetName()!=TEXT("NinjaLive_C"))continue;
  UE_LOG(LogTemp,Display,TEXT("FOG ACTOR %s at %s"),*It->GetName(),*It->GetActorLocation().ToString());
  TInlineComponentArray<UActorComponent*> Components;It->GetComponents(Components);
  for(UActorComponent* C:Components)
  {
   if(UPrimitiveComponent* P=Cast<UPrimitiveComponent>(C))UE_LOG(LogTemp,Display,TEXT("FOG COMP %s active%d visible%d hidden%d at%s bounds%s"),*C->GetName(),C->IsActive(),P->IsVisible(),P->bHiddenInGame,*P->GetComponentLocation().ToString(),*P->Bounds.BoxExtent.ToString());
   if(C->GetName()!=TEXT("NinjaLiveComponent"))continue;
   for(TFieldIterator<FProperty> P(C->GetClass());P;++P)
   {
    FString N=P->GetName();
    if(!(N.Contains(TEXT("Tracked"))||N.Contains(TEXT("SimulationPosition"))||N.Contains(TEXT("Overlap"))||N.Contains(TEXT("Brush"))||N.Contains(TEXT("Volume"))||N.Contains(TEXT("RT_"))||N.Contains(TEXT("Activated"))||N==TEXT("DensityNoiseAmount")))continue;
    if(FArrayProperty* Array=CastField<FArrayProperty>(*P)){FScriptArrayHelper Values(Array,Array->ContainerPtrToValuePtr<void>(C));UE_LOG(LogTemp,Display,TEXT("FOG ARRAY %s count%d"),*N,Values.Num());}
    FString Value;P->ExportTextItem_Direct(Value,P->ContainerPtrToValuePtr<void>(C),nullptr,C,PPF_None);
    UE_LOG(LogTemp,Display,TEXT("FOG PROP %s=%s"),*N,*Value);
   }
   if(FObjectPropertyBase* P=FindFProperty<FObjectPropertyBase>(C->GetClass(),TEXT("RT_VelocityDensity")))
   {
    if(UTextureRenderTarget2D* RT=Cast<UTextureRenderTarget2D>(P->GetObjectPropertyValue_InContainer(C)))
    {
     TArray<FLinearColor> Data;UKismetRenderingLibrary::ReadRenderTargetRaw(this,RT,Data,false);
     FLinearColor Sum(0,0,0,0),Max(0,0,0,0);for(FLinearColor V:Data){Sum+=V;for(int k=0;k<4;++k)Max.Component(k)=FMath::Max(Max.Component(k),V.Component(k));}
     if(Data.Num())Sum/=float(Data.Num());
     UE_LOG(LogTemp,Display,TEXT("FOG BUFFER %dx%d count%d mean%s max%s"),RT->SizeX,RT->SizeY,Data.Num(),*Sum.ToString(),*Max.ToString());
    }
   }
  }
 }
}
