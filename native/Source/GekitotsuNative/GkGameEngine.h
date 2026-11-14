#pragma once

#include "Engine/GameEngine.h"
#include "GkGameEngine.generated.h"

UCLASS()
class UGkGameEngine : public UGameEngine
{
	GENERATED_BODY()

public:
	bool Exec(UWorld* World, const TCHAR* Command, FOutputDevice& Output) override;
};
