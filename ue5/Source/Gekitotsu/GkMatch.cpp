#include "GkMatch.h"
#include "GkGame.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

AGkMatch::AGkMatch()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AGkMatch::BeginPlay()
{
	Super::BeginPlay();
	if (!Game) { Game = new FGkGame(); Game->Begin(this); }
}

void AGkMatch::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Game) { delete Game; Game = nullptr; }
	Super::EndPlay(EndPlayReason);
}

void AGkMatch::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Game) { Game->Tick(DeltaSeconds); }
}

FString AGkMatch::Debug(const FString& Command) { return Game ? Game->Debug(Command) : FString(TEXT("no match")); }

void AGkHUD::DrawHUD()
{
	Super::DrawHUD();
	if (FGkGame* G = FGkGame::Current) { G->DrawHud(Canvas); }
}

AGkGameMode::AGkGameMode() { HUDClass = AGkHUD::StaticClass(); DefaultPawnClass = nullptr; }

void AGkGameMode::StartPlay()
{
	Super::StartPlay();
	if (UWorld* W = GetWorld()) { W->SpawnActor<AGkMatch>(FVector::ZeroVector, FRotator::ZeroRotator); }
}
