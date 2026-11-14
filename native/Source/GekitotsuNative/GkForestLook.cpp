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
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/IConsoleManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

namespace
{
    TAutoConsoleVariable<float> GkRimPower(TEXT("gk.Look.RimPower"),3.8f,TEXT("Fighter RimPower look-development control."));
    TAutoConsoleVariable<float> GkRimWidth(TEXT("gk.Look.RimWidth"),1.1f,TEXT("Fighter RimWidth look-development control."));
    TAutoConsoleVariable<float> GkShoulderStrength(TEXT("gk.Look.ShoulderStrength"),0.75f,TEXT("Fighter ShoulderStrength look-development control."));
    TAutoConsoleVariable<float> GkKeyShadowCut(TEXT("gk.Look.KeyShadowCut"),-0.05f,TEXT("Fighter KeyShadowCut look-development control."));
    TAutoConsoleVariable<float> GkKeyHighlightCut(TEXT("gk.Look.KeyHighlightCut"),0.6f,TEXT("Fighter KeyHighlightCut look-development control."));
    TAutoConsoleVariable<float> GkSpecularStrength(TEXT("gk.Look.SpecularStrength"),0.12f,TEXT("Fighter SpecularStrength look-development control."));
    TAutoConsoleVariable<float> GkMistNoise(TEXT("gk.Look.MistNoise"),3.0f,TEXT("FluidNinja MistNoise control."));
    TAutoConsoleVariable<float> GkMistNoiseTile(TEXT("gk.Look.MistNoiseTile"),1.2f,TEXT("FluidNinja MistNoiseTile control."));
    TAutoConsoleVariable<float> GkMistDepth(TEXT("gk.Look.MistDepth"),24.0f,TEXT("FluidNinja MistDepth control."));
    TAutoConsoleVariable<float> GkMistNoiseSpeed(TEXT("gk.Look.MistNoiseSpeed"),0.025f,TEXT("FluidNinja MistNoiseSpeed control."));
    TAutoConsoleVariable<float> GkCloudScale(TEXT("gk.Look.CloudScale"),1.35f,TEXT("Sky cloud vertical scale."));
    TAutoConsoleVariable<float> GkCloudOffset(TEXT("gk.Look.CloudOffset"),.88f,TEXT("Sky cloud vertical offset."));
    TAutoConsoleVariable<float> GkCloudLift(TEXT("gk.Look.CloudLift"),.22f,TEXT("Sky cloud middle value."));
    TAutoConsoleVariable<float> GkSunScatter(TEXT("gk.Look.SunScatter"),4.0f,TEXT("Atmospheric SunScatter control."));
    TAutoConsoleVariable<float> GkFogAnisotropy(TEXT("gk.Look.FogAnisotropy"),0.65f,TEXT("Atmospheric FogAnisotropy control."));
    TAutoConsoleVariable<float> GkCelEdgeSoftness(TEXT("gk.Look.CelEdgeSoftness"),0.08f,TEXT("Environment CelEdgeSoftness control."));
    TAutoConsoleVariable<float> GkCreaseStrength(TEXT("gk.Look.CreaseStrength"),0.12f,TEXT("Environment CreaseStrength control."));
    // Runtime look-development controls also make matched capture comparisons reproducible.
    TAutoConsoleVariable<float> GkShadowDistance(TEXT("gk.Look.ShadowDistance"),6500.f,TEXT("Directional shadow coverage in centimetres."));
    TAutoConsoleVariable<int32> GkMistResolution(TEXT("gk.Look.MistResolution"),256,TEXT("FluidNinja grid width; height is twice width."));
    TAutoConsoleVariable<float> GkMistSteps(TEXT("gk.Look.MistSteps"),128.f,TEXT("FluidNinja volume view samples."));
    TAutoConsoleVariable<float> GkMistDensity(TEXT("gk.Look.MistDensity"),4.f,TEXT("FluidNinja vapor optical density."));
    TAutoConsoleVariable<float> GkMistHeight(TEXT("gk.Look.MistHeight"),.045f,TEXT("FluidNinja vapor extrusion."));
    TAutoConsoleVariable<float> GkMistPower(TEXT("gk.Look.MistPower"),.5f,TEXT("FluidNinja density shaping exponent."));
    TAutoConsoleVariable<float> GkMistLight(TEXT("gk.Look.MistLight"),.18f,TEXT("FluidNinja directional light response."));
    TAutoConsoleVariable<float> GkGroundWetAmount(TEXT("gk.Look.GroundWetAmount"),0.f,TEXT("Patchy ground wetness, preserving dry terrain."));
    TAutoConsoleVariable<float> GkGroundWetCut(TEXT("gk.Look.GroundWetCut"),.29f,TEXT("Wet-patch coverage threshold."));
    TAutoConsoleVariable<float> GkGroundRoughness(TEXT("gk.Look.GroundRoughness"),1.f,TEXT("Dirt and stone roughness multiplier; vegetation remains matte."));
    TAutoConsoleVariable<float> GkBloomThreshold(TEXT("gk.Look.BloomThreshold"),1.f,TEXT("Scene-linear brightness required for selective bloom."));
    TAutoConsoleVariable<float> GkSkyScale(TEXT("gk.Look.SkyScale"),1.f,TEXT("Multiplier for palette-specific sky fill."));
    TAutoConsoleVariable<float> GkSunScale(TEXT("gk.Look.SunScale"),1.f,TEXT("Multiplier for authored forest sunlight."));
    TAutoConsoleVariable<float> GkSunPitch(TEXT("gk.Look.SunPitch"),-8.f,TEXT("Forest sunlight pitch offset in degrees."));
    TAutoConsoleVariable<float> GkSunYaw(TEXT("gk.Look.SunYaw"),4.f,TEXT("Forest sunlight yaw offset in degrees."));
    TAutoConsoleVariable<float> GkFogScale(TEXT("gk.Look.FogScale"),1.f,TEXT("Multiplier for forest atmospheric density."));
    TAutoConsoleVariable<float> GkCelStrength(TEXT("gk.Look.CelStrength"),0.35f,TEXT("Environment cel shading blend."));
    TAutoConsoleVariable<float> GkCelShadow(TEXT("gk.Look.CelShadow"),.32f,TEXT("Environment shadow illumination threshold."));
    TAutoConsoleVariable<float> GkCelHigh(TEXT("gk.Look.CelHigh"),.72f,TEXT("Environment highlight illumination threshold."));
    TAutoConsoleVariable<float> GkDebugLight(TEXT("gk.Look.DebugLight"),0.f,TEXT("Diagnostic illumination bands, zero for normal rendering."));
    TAutoConsoleVariable<float> GkBloom(TEXT("gk.Look.Bloom"),.8f,TEXT("Selective bloom intensity above scene-linear white."));

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

	if(FMath::Abs(FilmWetness-Storm)>.05f || FilmGroundRoughness!=GkGroundRoughness.GetValueOnGameThread() || FilmGroundWetAmount!=GkGroundWetAmount.GetValueOnGameThread() || FilmGroundWetCut!=GkGroundWetCut.GetValueOnGameThread()){
  FilmGroundRoughness=GkGroundRoughness.GetValueOnGameThread();
  FilmGroundWetAmount=GkGroundWetAmount.GetValueOnGameThread();
  FilmGroundWetCut=GkGroundWetCut.GetValueOnGameThread();
  FilmWetness=Storm;
  for(TActorIterator<ALandscapeProxy> It(GetWorld());It;++It)if(It->bUseDynamicMaterialInstance) {
   It->SetLandscapeMaterialScalarParameterValue(TEXT("FilmWetAmount"),FMath::Lerp(FilmGroundWetAmount,.25f,Storm));
   It->SetLandscapeMaterialScalarParameterValue(TEXT("FilmWetThreshold"),FilmGroundWetCut-.08f*Storm);
   for(int k=1;k<=8;++k)It->SetLandscapeMaterialScalarParameterValue(FName(*FString::Printf(TEXT("Layer_%02d_Roughness_Intensity"),k)),((k==3 || k==6 || k==8)?.88f:.90f)*(1.f-.35f*Storm)*((k==3 || k>=5)?FMath::Clamp(FilmGroundRoughness,.1f,2.f):1.f));
  }
 }
 if (!ForestSun.IsValid())
 { // Fill lights illuminate the set; the key light supplies its graphic cast shadows.
  for(TActorIterator<AActor> It(GetWorld());It;++It){TInlineComponentArray<UPointLightComponent*> Lights;It->GetComponents(Lights);for(UPointLightComponent* Light:Lights)Light->SetCastShadows(false);}

        // Retain authored leaf masks and wind; only the foliage color instances change.
        const TCHAR* LeafSources[]={TEXT("MI_DreadMarsh_OakLeaves_01"),TEXT("MI_DreadMarsh_Willow_Leaves_02"),TEXT("MI_DreadMarsh_Cypress_Leaves_01")};
        const TCHAR* LeafTargets[]={TEXT("/Game/GkFx/MI_GkFilmOak"),TEXT("/Game/GkFx/MI_GkFilmWillow"),TEXT("/Game/GkFx/MI_GkFilmCypress")};
        UMaterialInterface* LeafMaterials[3];for(int k=0;k<3;++k)LeafMaterials[k]=LoadObject<UMaterialInterface>(nullptr,LeafTargets[k]);
        for(TActorIterator<AActor> It(GetWorld());It;++It) {
            TInlineComponentArray<UStaticMeshComponent*> Meshes;It->GetComponents(Meshes);
            for(auto* Mesh:Meshes)for(int Slot=0;Slot<Mesh->GetNumMaterials();++Slot)
                if(auto* Material=Mesh->GetMaterial(Slot))for(int k=0;k<3;++k)
                    if(LeafMaterials[k] && Material->GetName()==LeafSources[k])Mesh->SetMaterial(Slot,LeafMaterials[k]);
        }

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
    if(!ForestFill.IsValid()) {
        for(TActorIterator<ASkyLight> It(GetWorld());It;++It) {
            auto* Light=It->GetLightComponent();
            if(Light && Light->IsVisible()){ForestFill=Light;break;}
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
		Light->SetVolumetricScatteringIntensity(GkSunScatter.GetValueOnGameThread());
		Light->SetDynamicShadowDistanceMovableLight(GkShadowDistance.GetValueOnGameThread());
		Light->SetWorldRotation(ForestSunRotation + FRotator(Pitch+GkSunPitch.GetValueOnGameThread(), Yaw+GkSunYaw.GetValueOnGameThread(), 0.0));
		Light->SetLightColor(FMath::Lerp(ForestSunColor, Key, 0.92f));
		Light->SetIntensity(ForestSunIntensity * KeyScale * GkSunScale.GetValueOnGameThread() * (1.f + 1.8f * Flash * FlashGain));
	}
	if (UExponentialHeightFogComponent* FogComponent = ForestFog.Get())
	{
        // Preserve the environment author's height, both layers, falloff and scattering.
        // Daylight reproduces the saved map fog; other palettes only shift color/density.
        FogComponent->SetFogDensity(ForestFogDensity * (FogScale / Palettes[0].FogScale) * GkFogScale.GetValueOnGameThread());
        FogComponent->SetFogInscatteringColor(FMath::Lerp(ForestFogColor,FogTint,(1.f-PaletteWeights[0])*.65f));
	}
    if(auto* Fill=ForestFill.Get()) {
        const FLinearColor FillColors[6]={{.55f,.66f,1.f},{.28f,.42f,1.f},{.40f,.47f,.78f},{.70f,.53f,.42f},{.28f,.65f,.34f},{.48f,.38f,.85f}};
        const float FillPower[6]={3.0f,.65f,1.25f,2.1f,1.35f,1.2f};
        FLinearColor Tint(0,0,0);float Power=0;
        for(int i=0;i<6;++i){Tint+=FillColors[i]*PaletteWeights[i];Power+=FillPower[i]*PaletteWeights[i];}
        Fill->SetLightColor(Tint);Fill->SetIntensity(Power*GkSkyScale.GetValueOnGameThread());
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
  FilmSky->SetScalarParameterValue(TEXT("CloudVerticalScale"),GkCloudScale.GetValueOnGameThread());
  FilmSky->SetScalarParameterValue(TEXT("CloudVerticalOffset"),GkCloudOffset.GetValueOnGameThread());
  FilmSky->SetScalarParameterValue(TEXT("CloudShadowLift"),GkCloudLift.GetValueOnGameThread());
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
  const int32 Resolution=FMath::Clamp(GkMistResolution.GetValueOnGameThread(),64,512);
  bool Resize=false;
  const TPair<FName,int32> Resolutions[]={{TEXT("ResolutionX"),Resolution},{TEXT("ResolutionY"),Resolution*2}};
  for(const auto& Setting:Resolutions) {
   if(FIntProperty* P=FindFProperty<FIntProperty>(FilmNinja->GetClass(),Setting.Key)) {
    if(P->GetPropertyValue_InContainer(FilmNinja)!=Setting.Value) {
     P->SetPropertyValue_InContainer(FilmNinja,Setting.Value);Resize=true;
    }
   }
   FilmNinja->SetVariableInt(FName(*(TEXT("User.")+Setting.Key.ToString())),Setting.Value);
  }
  if(Resize)FilmNinja->ReinitializeSystem();

  // LIVE-2 reconstructs this component on activation. Apply stage overrides to the live
  // instance, including its Niagara user values, instead of editing discarded templates.
  auto SetSimFloat=[this](const TCHAR* Property,const TCHAR* User,float Value){
   if(FNumericProperty* P=FindFProperty<FNumericProperty>(FilmNinja->GetClass(),Property))
    if(P->IsFloatingPoint())P->SetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(FilmNinja),Value);
   FilmNinja->SetVariableFloat(FName(User),Value);
  };
  SetSimFloat(TEXT("DirectVelocityY"),TEXT("User.Composite.DirectVelocityY"),-.08f-.3f*Storm);
  SetSimFloat(TEXT("DensityNoiseAmount"),TEXT("User.Composite.DensityNoiseAmount"),GkMistNoise.GetValueOnGameThread());
  SetSimFloat(TEXT("DensityNoiseTile"),TEXT("User.Composite.DensityNoiseTile"),GkMistNoiseTile.GetValueOnGameThread());
  SetSimFloat(TEXT("DensityNoiseSpeed"),TEXT("User.Composite.DensityNoiseSpeed"),GkMistNoiseSpeed.GetValueOnGameThread());
  const FVector Extents(GkMistDepth.GetValueOnGameThread(),28,1.5);
  if(FStructProperty* P=FindFProperty<FStructProperty>(FilmNinja->GetClass(),TEXT("ExtentsXYZ")))
   if(P->Struct==TBaseStructure<FVector>::Get())*P->ContainerPtrToValuePtr<FVector>(FilmNinja)=Extents;
  FilmNinja->SetVariableVec3(TEXT("User.ExtentsXYZ"),Extents);
 }


    if(NinjaLevel)if(ULevel* FogLevel=NinjaLevel->GetLoadedLevel()) {
        for(AActor* Actor:FogLevel->Actors)if(IsValid(Actor)) {
            TInlineComponentArray<UStaticMeshComponent*> Meshes;Actor->GetComponents(Meshes);
            for(auto* Mesh:Meshes)if(auto* MID=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0))) {
                if(MID->GetMaterial()->GetName()!=TEXT("M_GkCelMistVolume"))continue;
                MID->SetScalarParameterValue(TEXT("MaxViewSteps"),FMath::Clamp(GkMistSteps.GetValueOnGameThread(),32.f,160.f));
                MID->SetScalarParameterValue(TEXT("DensityPow"),GkMistPower.GetValueOnGameThread());
                MID->SetScalarParameterValue(TEXT("Density"),GkMistDensity.GetValueOnGameThread());
                MID->SetScalarParameterValue(TEXT("Extrude"),GkMistHeight.GetValueOnGameThread());
                MID->SetScalarParameterValue(TEXT("LightIntensity"),GkMistLight.GetValueOnGameThread());
            }
        }
    }
    if(auto* Cel=Cast<UMaterialInstanceDynamic>(ForestCel)) {
        Cel->SetScalarParameterValue(TEXT("CelEdgeSoftness"),GkCelEdgeSoftness.GetValueOnGameThread());
        Cel->SetScalarParameterValue(TEXT("CreaseStrength"),GkCreaseStrength.GetValueOnGameThread());
        Cel->SetScalarParameterValue(TEXT("CelStrength"),GkCelStrength.GetValueOnGameThread());
        Cel->SetScalarParameterValue(TEXT("ShadowThreshold"),GkCelShadow.GetValueOnGameThread());
        Cel->SetScalarParameterValue(TEXT("HighlightThreshold"),GkCelHigh.GetValueOnGameThread());
        Cel->SetScalarParameterValue(TEXT("DebugIllumination"),GkDebugLight.GetValueOnGameThread());
    }
    for(int i=0;i<2;++i)for(int k=0;k<4;++k)if(BodyMid[i][k] && BodyMid[i][k]->Parent==ForestFighterCel) {
        BodyMid[i][k]->SetScalarParameterValue(TEXT("RimPower"),GkRimPower.GetValueOnGameThread());
        BodyMid[i][k]->SetScalarParameterValue(TEXT("RimWidth"),GkRimWidth.GetValueOnGameThread());
        BodyMid[i][k]->SetScalarParameterValue(TEXT("ShoulderStrength"),GkShoulderStrength.GetValueOnGameThread());
        BodyMid[i][k]->SetScalarParameterValue(TEXT("KeyShadowCut"),GkKeyShadowCut.GetValueOnGameThread());
        BodyMid[i][k]->SetScalarParameterValue(TEXT("KeyHighlightCut"),GkKeyHighlightCut.GetValueOnGameThread());
        BodyMid[i][k]->SetScalarParameterValue(TEXT("SpecularStrength"),GkSpecularStrength.GetValueOnGameThread());
    }
    if(Cam) {
        auto& PP=Cam->GetCameraComponent()->PostProcessSettings;
        PP.bOverride_BloomIntensity=true;PP.BloomIntensity=GkBloom.GetValueOnGameThread();
        PP.bOverride_BloomThreshold=true;PP.BloomThreshold=GkBloomThreshold.GetValueOnGameThread();
    }
	if (BackLamp)
	{
		BackLamp->SetLightColor(ForestRim);
		BackLamp->SetIntensity(LampI * (1.f + FMath::Min(Flash*FlashGain, 1.f)));
	}
}
