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

class UGridPathfinderComponent; // 👈 Forward Declaration

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class THELASTCHERRYBLOSSOM_API UFlowFieldComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UFlowFieldComponent();

	void GenerateFlowField(const FVector& Destination, const TArray<FVector>& Path, int32 CorridorWidth);
	FVector GetDirectionAtLocation(const FVector& Location) const;
	FIntVector WorldToGridIndex(const FVector& Location) const;
	FVector GridIndexToWorld(const FIntVector& Index) const;
	void DrawDebugFlowField() const;

	void DebugPrintStats() const;

	UFUNCTION(BlueprintCallable, Category="FlowField")
	FIntPoint WorldToGrid(const FVector& WorldLocation) const;

	const FFlowFieldCell* GetCell(const FIntPoint& Coord) const;
    float GetCellSize() const { return CellSize; }
private:
	
	void BuildCorridorFromPath(const TArray<FVector>& Path, int32 CorridorWidth);

	UPROPERTY(EditAnywhere, Category="FlowField")
	int32 GridSize = 100;
	
	FVector FlowFieldDestination;
	TArray<FFlowFieldCell> FlowFieldGrid;
	
	// 👇 اشاره‌گر به کامپوننت مسیر یاب
	UGridPathfinderComponent* PathfinderComp;
	
	

protected:
	
	virtual void BeginPlay() override;

	// اندازه سلول‌ها
	UPROPERTY(EditAnywhere, Category="FlowField")
	float CellSize = 50.f;

	// تعداد سلول در عرض و طول
	UPROPERTY(EditAnywhere, Category="FlowField")
	int32 GridWidth = 50;

	UPROPERTY(EditAnywhere, Category="FlowField")
	int32 GridHeight = 50;

	// مبدا شبکه (مثلاً گوشه پایین-چپ زمین)
	UPROPERTY(EditAnywhere, Category="FlowField")
	FVector Origin = FVector::ZeroVector;

	// خود گرید
	UPROPERTY()
	TArray<FFlowFieldCell> Grid;
};
