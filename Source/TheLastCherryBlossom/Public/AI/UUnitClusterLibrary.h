#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UUnitClusterLibrary.generated.h"

class AUnitCharacter;

/**
 * Global clustering utility.
 * NOT a UObject instance — a static library (BlueprintFunctionLibrary).
 * Note: We intentionally DO NOT mark ClusterUnits as UFUNCTION because
 * UHT cannot reflect nested TArray<TArray<...>> return types.
 */
UCLASS()
class THELASTCHERRYBLOSSOM_API UUnitClusterLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Static C++ API — call from C++ code
	static TArray<TArray<AUnitCharacter*>> ClusterUnits(
		const TArray<AUnitCharacter*>& Units,
		float Radius = 500.f
	);

private:
	static TArray<TArray<AUnitCharacter*>> SimpleClusterFixedRadius(
		const TArray<AUnitCharacter*>& Units,
		float Radius
	);
};
