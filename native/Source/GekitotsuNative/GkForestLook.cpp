#include "GkMatch.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "UObject/UnrealType.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/IConsoleManager.h"

namespace
{
	struct FForestPalette
	{
		FLinearColor Key, Fog, Rim, Grade;
		float KeyScale, FogScale, Pitch, Yaw;
	};

	const FForestPalette Palettes[] = {
		{{1.00f, 0.78f, 0.49f}, {0.36f, 0.40f, 0.64f}, {1.00f, 0.55f, 0.22f}, {1.03f, 1.02f, 0.96f}, 2.05f, 1.10f, 8.f, -14.f},
		{{0.40f, 0.59f, 1.00f}, {0.17f, 0.25f, 0.49f}, {0.49f, 0.72f, 1.00f}, {0.58f, 0.74f, 1.06f}, 0.40f, 0.88f, 7.f, 18.f},
		{{0.56f, 0.70f, 1.00f}, {0.39f, 0.47f, 0.59f}, {0.54f, 0.77f, 1.00f}, {0.79f, 0.89f, 1.05f}, 0.57f, 1.42f, 1.f, -10.f},
		{{1.00f, 0.63f, 0.34f}, {0.93f, 0.60f, 0.32f}, {1.00f, 0.65f, 0.31f}, {1.13f, 0.93f, 0.72f}, 0.75f, 1.53f, 8.f, -14.f},
		{{0.56f, 1.00f, 0.34f}, {0.26f, 0.78f, 0.27f}, {0.59f, 1.00f, 0.52f}, {0.75f, 1.08f, 0.81f}, 0.67f, 1.45f, 2.f, 20.f},
		{{0.65f, 0.56f, 1.00f}, {0.56f, 0.28f, 0.72f}, {1.00f, 0.50f, 0.84f}, {0.87f, 0.72f, 1.16f}, 0.62f, 1.16f, 8.f, -22.f},
	};
}

FVector AGkMatch::FloorPoint(const FVector& Local) const
{
	const FVector World = Wd(Local);
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GkForestFloor), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, World + FVector(0, 0, 24), World - FVector(0, 0, 250), ECC_WorldStatic, Query)
		&& Hit.ImpactNormal.Z > 0.55)
	{
		return Anchor.InverseTransformPosition(Hit.ImpactPoint);
	}
	return Local;
}

void AGkMatch::UpdateForestLook(float DeltaSeconds)
{
    // Reuse the authored weather system; LIVE-2 is the sole owner of ground fog.
    if(!FilmRain) {
        if(UNiagaraSystem* Rain=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/SoStylized/Effects/Systems/NS_RainAndFog.NS_RainAndFog"))) {
            FilmRain=NewObject<UNiagaraComponent>(this);FilmRain->SetupAttachment(RootComponent);
            FilmRain->SetAutoActivate(false);FilmRain->SetAsset(Rain);FilmRain->RegisterComponent();
            FilmRain->SetVariableFloat(TEXT("User.Rain Rate Max"),1800.f);
            // The authored emitter samples the gameplay camera in world space.
            FilmRain->SetAbsolute(true,true,true);FilmRain->SetWorldTransform(FTransform::Identity);
            FilmRain->SetSystemFixedBounds(FBox(FVector(-60000,-60000,-15000),FVector(60000,60000,15000)));
            if(FParse::Param(FCommandLine::Get(),TEXT("gkweatherprobe"))) {
                TArray<FNiagaraVariable> Parameters;Rain->GetExposedParameters().GetParameters(Parameters);
                for(FNiagaraVariable& Parameter:Parameters){
                    if(const uint8* Bytes=Rain->GetExposedParameters().GetParameterData(Parameter))Parameter.SetData(Bytes);
                    UE_LOG(LogTemp,Display,TEXT("GkRain PARAM %s"),*Parameter.ToString());
                }
                for(const FNiagaraEmitterHandle& Handle:Rain->GetEmitterHandles())if(const FVersionedNiagaraEmitterData* Data=Handle.GetEmitterData())
                    UE_LOG(LogTemp,Display,TEXT("GkRain EMITTER %s local%d target%d bounds%s"),*Handle.GetName().ToString(),Data->bLocalSpace,int(Data->SimTarget),*Data->FixedBounds.ToString());
            }
            FilmRain->SetVariableFloat(TEXT("User.Rain Fog Rate Max"),0.f);
            FilmRain->SetVariableFloat(TEXT("User.Rain Fog Rate Alpha"),0.f);
            FilmRain->SetVariableFloat(TEXT("User.Snow Intensity"),0.f);
            FilmRain->SetVariableFloat(TEXT("User.Rain Intensity"),0.f);
            FilmRain->SetVariableLinearColor(TEXT("User.Rain Color"),FLinearColor(.45f,.60f,.85f,.7f));
            FilmRain->Activate(true);
            UE_LOG(LogTemp,Display,TEXT("GkFilm: authored Niagara rain enabled, auxiliary fog disabled"));
        }
    }
    if(FilmRain) {
        FilmRain->SetVariableFloat(TEXT("User.Rain Intensity"),FMath::Clamp((Storm-.25f)/.75f,0.f,1.f));
        FilmRain->SetVariableFloat(TEXT("User.Wind Intensity"),Storm*.35f);
        FilmRain->SetVariableVec3(TEXT("User.Wind Direction"),Anchor.TransformVectorNoScale(FVector(0,-1,0)));
    }
	if (!FilmSky)
	{
		for (TActorIterator<AActor> It(GetWorld());It;++It)
		{
			if (It->GetClass()->GetName()!=TEXT("BP_Sky_C")) continue;
			if (UStaticMeshComponent* Mesh=It->FindComponentByClass<UStaticMeshComponent>())
			{
				if (UMaterialInterface* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/GkFx/M_GkFilmSky.M_GkFilmSky")))
				{
					FilmSky=Mesh->CreateDynamicMaterialInstance(0,Material);
					UE_LOG(LogTemp,Display,TEXT("GkFilm: installed sky on %s"),*It->GetName());
				}
			}
		}
	}
	if(FMath::Abs(FilmWetness-Storm)>.05f){
  FilmWetness=Storm;
  for(TActorIterator<ALandscapeProxy> It(GetWorld());It;++It)if(It->bUseDynamicMaterialInstance)
   for(int k=1;k<=8;++k)It->SetLandscapeMaterialScalarParameterValue(FName(*FString::Printf(TEXT("Layer_%02d_Roughness_Intensity"),k)),((k==3 || k==6 || k==8)?.42f:.82f)*(1.f-.72f*Storm));
 }
 if (!ForestSun.IsValid())
 { // Fill lights illuminate the set; the key light supplies its graphic cast shadows.
  for(TActorIterator<AActor> It(GetWorld());It;++It){TInlineComponentArray<UPointLightComponent*> Lights;It->GetComponents(Lights);for(UPointLightComponent* Light:Lights)Light->SetCastShadows(false);}

		for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
		{
			UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(It->GetLightComponent());
			if (Light && Light->IsVisible() && (!ForestSun.IsValid() || Light->Intensity > ForestSunIntensity))
			{
				ForestSun = Light;
				ForestSunIntensity = Light->Intensity;
				ForestSunRotation = Anchor.TransformVectorNoScale(FVector(-0.55,0.65,-0.8)).Rotation();
				ForestSunColor = Light->GetLightColor();
			}
		}
		if (UDirectionalLightComponent* Light = ForestSun.Get())
		{
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetDynamicShadowDistanceMovableLight(6500.f);
			Light->ContactShadowLength = 0.08f;
            Light->SetVolumetricScatteringIntensity(4.0f);
            Light->bEnableLightShaftOcclusion=true;Light->OcclusionMaskDarkness=.65f;
            Light->bEnableLightShaftBloom=true;Light->SetBloomScale(.50f);Light->SetBloomThreshold(1.f);
		}
	}
	if (!ForestFog.IsValid())
	{
		for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
		{
			UExponentialHeightFogComponent* FogComponent = It->GetComponent();
			if (FogComponent && FogComponent->IsVisible())
			{
				ForestFog = FogComponent;
				ForestFogDensity = FogComponent->FogDensity;
				ForestFogColor = FogComponent->FogInscatteringLuminance;
				break;
			}
		}
	}

	// Keep the established health/round storm cue. Daylight is the first-round baseline.
	int32 Target = PaletteOverride;
	if (Target == INDEX_NONE)
	{
		const bool bStorm = Screen == 3 && (S.round >= 3 || FMath::Min(S.p[0].hp, S.p[1].hp) < 420);
		Target = bStorm ? 2 : (Screen == 3 && S.round == 2 ? 5 : 0);
	}
	const float Blend = 1.f - FMath::Exp(-2.f * FMath::Max(DeltaSeconds, 0.f));
	FLinearColor Key(0, 0, 0), FogTint(0, 0, 0);
	ForestRim = ForestGrade = FLinearColor(0, 0, 0);
	float KeyScale = 0.f, FogScale = 0.f, Pitch = 0.f, Yaw = 0.f;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Palettes); ++Index)
	{
		float& Weight = PaletteWeights[Index];
		Weight = FMath::Lerp(Weight, Index == Target ? 1.f : 0.f, Blend);
		const FForestPalette& Palette = Palettes[Index];
		Key += Palette.Key * Weight;
		FogTint += Palette.Fog * Weight;
		ForestRim += Palette.Rim * Weight;
		ForestGrade += Palette.Grade * Weight;
		KeyScale += Palette.KeyScale * Weight;
		FogScale += Palette.FogScale * Weight;
		Pitch += Palette.Pitch * Weight;
		Yaw += Palette.Yaw * Weight;
	}
	if (UDirectionalLightComponent* Light = ForestSun.Get())
	{
		Light->SetWorldRotation(ForestSunRotation + FRotator(Pitch, Yaw, 0.0));
		Light->SetLightColor(FMath::Lerp(ForestSunColor, Key, 0.92f));
		Light->SetIntensity(ForestSunIntensity * KeyScale * (1.f + 1.8f * Flash * FlashGain));
	}
	if (UExponentialHeightFogComponent* FogComponent = ForestFog.Get())
	{
		FogComponent->SecondFogData.FogDensity=.012f*FogScale;
        FogComponent->SecondFogData.FogHeightFalloff=1.2f;
        FogComponent->SecondFogData.FogHeightOffset=-385.f;
        FogComponent->SetVolumetricFog(true);
        FogComponent->SetVolumetricFogScatteringDistribution(.65f);
  FogComponent->SetFogDensity(.008f * FogScale);
  FogComponent->SetFogHeightFalloff(.07f);
  FogComponent->SetWorldLocation(Anchor.GetLocation()+FVector(0,0,400));
  FogComponent->SetStartDistance(800.f);
  FogComponent->SetFogMaxOpacity(.55f);
		FogComponent->SetFogInscatteringColor(FMath::Lerp(ForestFogColor, FogTint, 0.75f));
		FogComponent->SetVolumetricFogAlbedo(FogTint.ToFColorSRGB());
	}
	if (FilmSky)
	{
		const FLinearColor Zenith[6]={{.015f,.05f,.30f},{.004f,.012f,.06f},{.025f,.025f,.095f},{.36f,.19f,.08f},{.015f,.18f,.035f},{.06f,.055f,.25f}};
		const FLinearColor Horizon[6]={{.10f,.25f,.55f},{.03f,.09f,.23f},{.18f,.14f,.31f},{.95f,.62f,.30f},{.40f,.80f,.08f},{.98f,.24f,.35f}};
		FLinearColor Z(0,0,0),H(0,0,0);
		for(int32 k=0;k<6;++k){Z+=Zenith[k]*PaletteWeights[k];H+=Horizon[k]*PaletteWeights[k];}
		const FVector Moon=Anchor.TransformVectorNoScale(FVector(.97,.22,.15)).GetSafeNormal();
  FilmSky->SetVectorParameterValue(TEXT("MoonDir"),FLinearColor(Moon.X,Moon.Y,Moon.Z));
  FilmSky->SetScalarParameterValue(TEXT("Night"),PaletteWeights[1]);
  FilmSky->SetVectorParameterValue(TEXT("Zenith"),Z);
		FilmSky->SetVectorParameterValue(TEXT("Horizon"),H);
		const FLinearColor CloudHigh[6]={{1.8f,1.7f,1.5f},{.035f,.09f,.22f},{.16f,.12f,.30f},{1.f,.7f,.38f},{.3f,.7f,.06f},{1.f,.33f,.45f}};
  const FLinearColor CloudLow[6]={{.22f,.32f,.53f},{.008f,.025f,.07f},{.04f,.035f,.12f},{.28f,.18f,.10f},{.018f,.12f,.03f},{.09f,.055f,.20f}};
  FLinearColor CH(0,0,0),CL(0,0,0);for(int k=0;k<6;++k){CH+=CloudHigh[k]*PaletteWeights[k];CL+=CloudLow[k]*PaletteWeights[k];}
  FilmSky->SetVectorParameterValue(TEXT("CloudLight"),CH*(1.f+Flash*FlashGain));
  FilmSky->SetVectorParameterValue(TEXT("CloudShadow"),CL);
		if(ForestSun.IsValid()){const FVector D=-ForestSun->GetForwardVector();FilmSky->SetVectorParameterValue(TEXT("SunDir"),FLinearColor(D.X,D.Y,D.Z));}
	}
 if(IsValid(FilmNinja)){
  if(FFloatProperty* P=FindFProperty<FFloatProperty>(FilmNinja->GetClass(),TEXT("DirectVelocityY")))P->SetPropertyValue_InContainer(FilmNinja,-.08f-.3f*Storm);
 }


	if (BackLamp)
	{
		BackLamp->SetLightColor(ForestRim);
		BackLamp->SetIntensity(LampI * (1.f + FMath::Min(Flash*FlashGain, 1.f)));
	}
}
