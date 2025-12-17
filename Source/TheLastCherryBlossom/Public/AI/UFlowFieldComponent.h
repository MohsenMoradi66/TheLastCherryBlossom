#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/GridPathfinderComponent.h"
#include "UFlowFieldComponent.generated.h"

USTRUCT()
struct FFlowFieldCell
{
	GENERATED_BODY()
	
	int32 Cost = -1;
	bool bInCorridor = false;
	bool bObstacle = false;
	FVector BaseDirection = FVector::ZeroVector;

	FVector Direction = FVector::ZeroVector;      // بردار نهایی
	FVector PathVector = FVector::ZeroVector;     // بردار مسیر
	FVector RepulsionVector = FVector::ZeroVector; // بردار دافعه
};

class UGridPathfinderComponent; // forward

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class THELASTCHERRYBLOSSOM_API UFlowFieldComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UFlowFieldComponent();

	// Destination فقط برای compute integration یا نمایش استفاده می‌شود
	void GenerateFlowField(const FVector& Destination, const TArray<FVector>& Path, int32 CorridorWidthCm);
	FVector GetDirectionAtLocation(const FVector& Location) const;
	FIntVector WorldToGridIndex(const FVector& Location) const;
	FVector GridIndexToWorld(const FIntVector& Index) const;
	void DrawDebugFlowField() const;
	void DrawDebugCorridor(const TArray<FVector>& Path, float CorridorWidthCm);
	void DebugPrintStats() const;

	UFUNCTION(BlueprintCallable, Category="FlowField")
	FIntPoint WorldToGrid(const FVector& WorldLocation) const;

	const FFlowFieldCell* GetCell(const FIntPoint& Coord) const;
    float GetCellSize() const { return CellSize; }

private:
	
	void BuildCorridorFromPath(const TArray<FVector>& Path, int32 CorridorWidthCm);

	// تنظیم پایه گرید
	UPROPERTY(EditAnywhere, Category="FlowField")
	float CellSize = 50.f;

	// ابعاد گرید (محاسبه شده یا قابل تنظیم)
	UPROPERTY(EditAnywhere, Category="FlowField")
	int32 GridWidth = 50;

	UPROPERTY(EditAnywhere, Category="FlowField")
	int32 GridHeight = 50;

	// مبدا شبکه (گوشه پایین-چپ)
	UPROPERTY(EditAnywhere, Category="FlowField")
	FVector Origin = FVector::ZeroVector;

	// مقصد فلوفیلد (برای محاسبات مسیر/نمایش)
	FVector FlowFieldDestination = FVector::ZeroVector;

	// خود گرید
	TArray<FFlowFieldCell> FlowFieldGrid;

	// اشاره‌گر به کامپوننت مسیر یاب
	UGridPathfinderComponent* PathfinderComp;


	int32 DebugCorridorWidthCells = 0; // برای نمایش کریدور در ادیتور


protected:
	virtual void BeginPlay() override;
};
