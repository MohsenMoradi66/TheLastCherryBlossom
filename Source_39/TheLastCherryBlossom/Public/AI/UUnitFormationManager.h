#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UUnitFormationManager.generated.h"

class AUnitCharacter;
class UFlowFieldComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class THELASTCHERRYBLOSSOM_API UUnitFormationManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UUnitFormationManager();

	// ورودی اصلی: یونیت‌ها و مقصد نهایی
	void MoveUnitsWithClustering(const TArray<AUnitCharacter*>& Units, const FVector& Goal);

	float CalculateCorridorWidthForCluster(
		const TArray<AUnitCharacter*>& Cluster,
		const FVector& ClusterCenter,
		const FVector& PathDirection,
		AUnitCharacter* Seed);

	bool IsUnitPathClear(
		AUnitCharacter* Unit,
		const TArray<FVector>& PathPoints,
		float CheckDistance);

	// فقط C++، حذف UFUNCTION
	TArray<TArray<float>> GetFormationMatrix(int32 UnitCount) const;
	float GetFormationSphereRadiusByCluster(const TArray<AUnitCharacter*>& Cluster);

private:
	// ذخیره FlowFieldهای خوشه
	UPROPERTY()
	TArray<UFlowFieldComponent*> ClusterFlowFields;

	// ---------- توابع جدید (Formation + Assignment) ----------
	void GenerateFinalFormation(const TArray<AUnitCharacter*>& Cluster, const FVector& Goal);

	// نسخهٔ کامل و ایمن: ساخت اسلات‌ها
	void BuildFormationSlots(const TArray<AUnitCharacter*>& Cluster, const FVector& Goal, TArray<FVector>& OutSlots);

	// ساخت ماتریس هزینه (فاصله یونیت -> اسلات)
	void BuildCostMatrix(const TArray<AUnitCharacter*>& Cluster, const TArray<FVector>& Slots, TArray<TArray<float>>& OutCost);

	// Hungarian solver (TArray friendly)
	TArray<int32> HungarianSolve(const TArray<TArray<float>>& Cost);

	// fallback greedy fast assign برای خوشه‌های خیلی بزرگ
	static void GreedyAssign(const TArray<TArray<float>>& Cost, TArray<int32>& OutAssignment);

	// ساده سازی و جدا سازی اسلات‌ها برای جلوگیری از برخورد اسلات‌ها
	static void ApplySimpleSeparation(TArray<FVector>& Slots, float MinSeparation);

	// حرکت دادن یونیت‌ها به اسلات‌های اختصاص داده شده
	void MoveClusterToAssignedSlots(const TArray<AUnitCharacter*>& Cluster, const TArray<FVector>& Slots, const TArray<int32>& Assignment);

	// تابع کلی که همه را کنار هم می‌چیند
	void AssignOptimalFormation(const TArray<AUnitCharacter*>& Cluster, const FVector& Goal);

	// ---------- Debug ----------
	bool bDrawFormationDebug = true;
	float DebugDrawTime = 8.0f;
};
