// GridPathfinderComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Pawn.h"
#include "GridPathfinderComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class THELASTCHERRYBLOSSOM_API UGridPathfinderComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UGridPathfinderComponent();

	// بررسی اینکه نقطه روی NavMesh هست یا نه
	bool IsLocationWalkable(const FVector& Location) const;

	// پیدا کردن نزدیک‌ترین نقطه Walkable
	bool FindClosestWalkable(const FVector& Origin, FVector& OutLocation) const;

	// مسیر‌یابی اصلی
	TArray<FVector> FindPathShared(const FVector& StartWorld, const FVector& GoalWorld);

	TArray<FVector> ProcessFinalPath(const TArray<FVector>& InputPath);

	TArray<FVector>ResamplePath(const TArray<FVector>& InputPath, float SegmentLength) const;
	
	// شعاع Capsule کاراکتر
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pathfinding")
	float CharacterRadius = 0.f;

	// شعاع جستجو برای پیدا کردن نزدیک‌ترین نقطه Walkable
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pathfinding")
	float SearchRadius = 500.f;

	protected:
	
	void BeginPlay();
};