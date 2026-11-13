#include "GkMatch.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	struct FForestPalette
	{
		FLinearColor Key, Fog, Rim, Grade;
		float KeyScale, FogScale, Pitch, Yaw;
	};

	const FForestPalette Palettes[] = {
		{{1.00f, 0.88f, 0.68f}, {0.72f, 0.77f, 0.49f}, {1.00f, 0.91f, 0.66f}, {1.03f, 1.02f, 0.96f}, 1.00f, 0.80f, -3.f, 0.f},
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
	if (!ForestSun.IsValid())
	{
		for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
		{
			UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(It->GetLightComponent());
			if (Light && Light->IsVisible() && (!ForestSun.IsValid() || Light->Intensity > ForestSunIntensity))
			{
				ForestSun = Light;
				ForestSunIntensity = Light->Intensity;
				ForestSunRotation = Light->GetComponentRotation();
				ForestSunColor = Light->GetLightColor();
			}
		}
		if (UDirectionalLightComponent* Light = ForestSun.Get())
		{
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetDynamicShadowDistanceMovableLight(6500.f);
			Light->ContactShadowLength = 0.08f;
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
		Light->SetLightColor(FMath::Lerp(ForestSunColor, Key, 0.65f));
		Light->SetIntensity(ForestSunIntensity * KeyScale * (1.f + 1.8f * Flash));
	}
	if (UExponentialHeightFogComponent* FogComponent = ForestFog.Get())
	{
		FogComponent->SetFogDensity(ForestFogDensity * FogScale);
		FogComponent->SetFogInscatteringColor(FMath::Lerp(ForestFogColor, FogTint, 0.75f));
		FogComponent->SetVolumetricFogAlbedo(FogTint.ToFColorSRGB());
	}
	if (BackLamp)
	{
		BackLamp->SetLightColor(ForestRim);
		BackLamp->SetIntensity(LampI * (1.f + FMath::Min(Flash, 1.f)));
	}
}
