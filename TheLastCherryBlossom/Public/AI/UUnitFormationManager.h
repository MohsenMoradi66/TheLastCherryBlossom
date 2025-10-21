#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UUnitFormationManager.generated.h"

class AUnitCharacter;
class UFlowFieldComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class THELASTCHERRYBLOSSOM_API UFormationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFormationComponent();

	UFUNCTION(BlueprintCallable, Category="Formation")
	void MoveUnitsWithClustering(const TArray<AUnitCharacter*>& Units, const FVector& Goal);

	private:

	UPROPERTY()
	TArray<UFlowFieldComponent*> ClusterFlowFields;
	
};
