#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GkMatch.generated.h"

struct FGkGame;

/**
 * The match as Unreal sees it. Everything it does lives in FGkGame (GkGame.h): this class only gives that a place in
 * the world, a tick, and one command entry point that test scripts and tools talk to.
 */
UCLASS()
class GEKITOTSU_API AGkMatch : public AActor
{
	GENERATED_BODY()
public:
	AGkMatch();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Test and tuning commands, as text. Returns a reply. See FGkGame::Debug for the list. */
	UFUNCTION(BlueprintCallable, Category = "Gekitotsu") FString Debug(const FString& Command);

	UPROPERTY() TArray<TObjectPtr<UObject>> Keep;
	FGkGame* Game = nullptr;
};

UCLASS()
class GEKITOTSU_API AGkHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
};

UCLASS()
class GEKITOTSU_API AGkGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AGkGameMode();
	virtual void StartPlay() override;
};
