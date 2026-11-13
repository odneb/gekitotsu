#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GkStageAuthoring.generated.h"

/** Editor-only authoring of the playable strip in the copied Dreadmarsh map. */
UCLASS()
class GEKITOTSUNATIVE_API UGkStageAuthoring : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable, Category="Gekitotsu|Authoring")
 static int32 RefreshNinja();
 UFUNCTION(BlueprintCallable, Category="Gekitotsu|Authoring")
 static int32 PrepareForest(UWorld* World, FVector Center, float Yaw);
};
