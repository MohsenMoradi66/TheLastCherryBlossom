#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/GridPathfinderComponent.h"
#include "UFlowFieldComponent.generated.h"


USTRUCT(BlueprintType)
struct FFlowFieldCell
{
    GENERATED_BODY()

    // هزینه (در این پیاده‌سازی استفاده نمی‌شود ولی برای سازگاری نگه داشته شده)
    UPROPERTY(BlueprintReadOnly, Category = "FlowField")
    int32 Cost = -1;

    // آیا سلول داخل راهروی معتبر است
    UPROPERTY(BlueprintReadOnly, Category = "FlowField")
    bool bInCorridor = false;

    // بردار جهت مسیر اصلی (موازی با نزدیک‌ترین قطعه از مسیر ورودی)
    UPROPERTY(BlueprintReadOnly, Category = "FlowField")
    FVector PathVector = FVector::ZeroVector;

    // بردار نهایی خروجی (همان PathVector پس از اعمال صاف‌سازی اختیاری)
    UPROPERTY(BlueprintReadOnly, Category = "FlowField")
    FVector Direction = FVector::ZeroVector;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THELASTCHERRYBLOSSOM_API UFlowFieldComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UFlowFieldComponent();

    // تولید فلوفیلد صرفاً بر اساس مسیر و عرض راهرو
    UFUNCTION(BlueprintCallable, Category = "FlowField")
    void GenerateFlowField(const FVector& Destination, const TArray<FVector>& Path, int32 CorridorWidthCm);

    UFUNCTION(BlueprintCallable, Category = "FlowField")
    FVector GetDirectionAtLocation(const FVector& Location) const;

    UFUNCTION(BlueprintCallable, Category = "FlowField")
    FIntVector WorldToGridIndex(const FVector& Location) const;

    UFUNCTION(BlueprintCallable, Category = "FlowField")
    FVector GridIndexToWorld(const FIntVector& Index) const;

    UFUNCTION(BlueprintCallable, Category = "FlowField")
    FIntPoint WorldToGrid(const FVector& WorldLocation) const;

    UFUNCTION(BlueprintCallable, Category = "FlowField")
    FFlowFieldCell GetCell(const FIntPoint& Coord) const;

    UFUNCTION(BlueprintCallable, Category = "FlowField")
    float GetCellSize() const { return CellSize; }

    // دیباگ
    UFUNCTION(BlueprintCallable, Category = "FlowField|Debug")
    void DrawDebugFlowField() const;

    UFUNCTION(BlueprintCallable, Category = "FlowField|Debug")
    void DrawDebugCorridor(const TArray<FVector>& Path, float CorridorWidthCm);

    UFUNCTION(BlueprintCallable, Category = "FlowField|Debug")
    void DebugPrintStats() const;

protected:
    virtual void BeginPlay() override;

    // تنظیمات گرید
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlowField|Grid")
    float CellSize = 50.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField|Grid")
    int32 GridWidth = 50;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField|Grid")
    int32 GridHeight = 50;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField|Grid")
    FVector Origin = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField")
    FVector FlowFieldDestination = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField")
    TArray<FFlowFieldCell> FlowFieldGrid;

    // کنترل نمایش دیباگ در ادیتور
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Debug")
    bool bDebugDrawDirection = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Debug")
    float DebugDrawDuration = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Debug")
    bool bEnableDebugText = false;

private:
    void BuildCorridorFromPath(const TArray<FVector>& Path, int32 CorridorWidthCm);
};