

#include "AI/UFlowFieldComponent.h"
#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#endif

UFlowFieldComponent::UFlowFieldComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UFlowFieldComponent::BeginPlay()
{
    Super::BeginPlay();
}

FVector UFlowFieldComponent::GridIndexToWorld(const FIntVector& Index) const
{
    float X = (Index.X + 0.5f) * CellSize + Origin.X;
    float Y = (Index.Y + 0.5f) * CellSize + Origin.Y;
    return FVector(X, Y, Origin.Z);
}

FIntVector UFlowFieldComponent::WorldToGridIndex(const FVector& Location) const
{
    FVector Relative = Location - Origin;
    int32 X = FMath::FloorToInt(Relative.X / CellSize);
    int32 Y = FMath::FloorToInt(Relative.Y / CellSize);
    return FIntVector(X, Y, 0);
}

FIntPoint UFlowFieldComponent::WorldToGrid(const FVector& WorldLocation) const
{
    FVector Relative = WorldLocation - Origin;
    int32 X = FMath::FloorToInt(Relative.X / CellSize);
    int32 Y = FMath::FloorToInt(Relative.Y / CellSize);
    return FIntPoint(X, Y);
}

FFlowFieldCell UFlowFieldComponent::GetCell(const FIntPoint& Coord) const
{
    if (Coord.X < 0 || Coord.Y < 0 || Coord.X >= GridWidth || Coord.Y >= GridHeight)
        return FFlowFieldCell();

    int32 Index = Coord.Y * GridWidth + Coord.X;
    if (FlowFieldGrid.IsValidIndex(Index))
        return FlowFieldGrid[Index];

    return FFlowFieldCell();
}

FVector UFlowFieldComponent::GetDirectionAtLocation(const FVector& Location) const
{
    FIntPoint Index = WorldToGrid(Location);
    if (Index.X >= 0 && Index.X < GridWidth && Index.Y >= 0 && Index.Y < GridHeight)
    {
        const FFlowFieldCell& Cell = FlowFieldGrid[Index.Y * GridWidth + Index.X];
        return Cell.Direction;
    }
    return FVector::ZeroVector;
}

void UFlowFieldComponent::BuildCorridorFromPath(const TArray<FVector>& Path, int32 CorridorWidthCm)
{
    if (Path.Num() < 2) return;

    int32 HalfWidthCells = FMath::CeilToInt((CorridorWidthCm * 0.5f) / CellSize);
    HalfWidthCells = FMath::Max(1, HalfWidthCells);
    float StepSize = CellSize * 0.5f;

    for (int32 i = 0; i < Path.Num() - 1; i++)
    {
        FVector Start = Path[i];
        FVector End = Path[i + 1];
        FVector ForwardDir = (End - Start).GetSafeNormal();
        FVector RightDir = FVector::CrossProduct(FVector::UpVector, ForwardDir).GetSafeNormal();

        int32 NumSteps = FMath::CeilToInt(FVector::Dist(Start, End) / StepSize);
        for (int32 step = 0; step <= NumSteps; step++)
        {
            FVector Center = Start + ForwardDir * (step * StepSize);
            for (int32 offset = -HalfWidthCells; offset <= HalfWidthCells; offset++)
            {
                FVector OffsetPos = Center + RightDir * offset * CellSize;
                FIntPoint CellIndex = WorldToGrid(OffsetPos);
                if (CellIndex.X >= 0 && CellIndex.X < GridWidth && CellIndex.Y >= 0 && CellIndex.Y < GridHeight)
                {
                    int32 FlatIndex = CellIndex.Y * GridWidth + CellIndex.X;
                    FFlowFieldCell& Cell = FlowFieldGrid[FlatIndex];
                    Cell.bInCorridor = true;
                    Cell.PathVector = ForwardDir;
                }
            }
        }

        // پر کردن گوشه‌ها (نقطه اتصال دو قطعه)
        if (i < Path.Num() - 2)
        {
            FVector CornerCenter = Path[i + 1];
            FVector PrevDir = (Path[i + 1] - Path[i]).GetSafeNormal();
            FVector NextDir = (Path[i + 2] - Path[i + 1]).GetSafeNormal();
            FVector CornerDir = (PrevDir + NextDir).GetSafeNormal();

            for (int32 oy = -HalfWidthCells; oy <= HalfWidthCells; oy++)
            {
                for (int32 ox = -HalfWidthCells; ox <= HalfWidthCells; ox++)
                {
                    FVector SamplePos = CornerCenter + FVector(ox * CellSize, oy * CellSize, 0);
                    FIntPoint CornerIndex = WorldToGrid(SamplePos);
                    if (CornerIndex.X >= 0 && CornerIndex.X < GridWidth && CornerIndex.Y >= 0 && CornerIndex.Y < GridHeight)
                    {
                        int32 FlatIndex = CornerIndex.Y * GridWidth + CornerIndex.X;
                        FFlowFieldCell& Cell = FlowFieldGrid[FlatIndex];
                        Cell.bInCorridor = true;
                        Cell.PathVector = CornerDir;
                    }
                }
            }
        }
    }
}

void UFlowFieldComponent::GenerateFlowField(const FVector& Destination, const TArray<FVector>& Path, int32 CorridorWidthCm)
{
    if (Path.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("FlowFieldComponent: Path has less than 2 points, cannot generate."));
        return;
    }

    FlowFieldDestination = Destination;

    // محاسبه Bounds بر اساس مسیر + مقصد
    FVector Min = Path[0], Max = Path[0];
    for (const FVector& P : Path)
    {
        Min = Min.ComponentMin(P);
        Max = Max.ComponentMax(P);
    }
    Min = Min.ComponentMin(Destination);
    Max = Max.ComponentMax(Destination);

    const float PaddingCm = 500.f;
    Min -= FVector(PaddingCm, PaddingCm, 0);
    Max += FVector(PaddingCm, PaddingCm, 0);

    Origin = FVector(Min.X, Min.Y, 0);
    GridWidth = FMath::Max(1, FMath::CeilToInt((Max.X - Min.X) / CellSize));
    GridHeight = FMath::Max(1, FMath::CeilToInt((Max.Y - Min.Y) / CellSize));

    FlowFieldGrid.Init(FFlowFieldCell(), GridWidth * GridHeight);

    // 1. ساختن راهرو حول مسیر
    BuildCorridorFromPath(Path, CorridorWidthCm);

    // 2. تنظیم بردار نهایی = همان PathVector (بدون هیچ اصلاحیه)
    for (int32 i = 0; i < FlowFieldGrid.Num(); i++)
    {
        FFlowFieldCell& Cell = FlowFieldGrid[i];
        if (Cell.bInCorridor)
            Cell.Direction = Cell.PathVector;
        else
            Cell.Direction = FVector::ZeroVector;
    }

    // ===== دیباگ – فقط در حالت غیر Shipping =====
#if !UE_BUILD_SHIPPING
    DrawDebugCorridor(Path, CorridorWidthCm);
    DrawDebugFlowField();
#endif

    UE_LOG(LogTemp, Warning, TEXT("FlowField generated (pure corridor). Grid=%dx%d CellSize=%.1f Origin=(%.1f,%.1f) Corridor=%dcm"),
        GridWidth, GridHeight, CellSize, Origin.X, Origin.Y, CorridorWidthCm);

    DebugPrintStats();
}

void UFlowFieldComponent::DebugPrintStats() const
{
    int32 Total = FlowFieldGrid.Num();
    int32 InCorr = 0, DirCount = 0;
    for (const FFlowFieldCell& C : FlowFieldGrid)
    {
        if (C.bInCorridor) InCorr++;
        if (!C.Direction.IsNearlyZero()) DirCount++;
    }
    UE_LOG(LogTemp, Warning, TEXT("FlowField stats: Total=%d InCorridor=%d WithDir=%d GridWxH=%dx%d CellSize=%.1f"),
        Total, InCorr, DirCount, GridWidth, GridHeight, CellSize);
}

void UFlowFieldComponent::DrawDebugFlowField() const
{
#if !UE_BUILD_SHIPPING
    if (FlowFieldGrid.Num() == 0 || !GetWorld()) return;

    const float ArrowSize = 15.f;
    const float ArrowScale = 0.5f;

    for (int32 y = 0; y < GridHeight; y++)
    {
        for (int32 x = 0; x < GridWidth; x++)
        {
            int32 Index = y * GridWidth + x;
            const FFlowFieldCell& Cell = FlowFieldGrid[Index];
            if (!Cell.bInCorridor) continue;

            FVector Start = GridIndexToWorld(FIntVector(x, y, 0));
            if (!Cell.Direction.IsNearlyZero())
            {
                FVector End = Start + Cell.Direction.GetSafeNormal() * (CellSize * ArrowScale);
                DrawDebugDirectionalArrow(GetWorld(), Start, End, ArrowSize, FColor::Green, false, 5.f, 0, 1.5f);
            }
            else
            {
                DrawDebugPoint(GetWorld(), Start, 8.f, FColor::Yellow, false, 10.f);
            }
        }
    }
#endif
}

void UFlowFieldComponent::DrawDebugCorridor(const TArray<FVector>& Path, float CorridorWidthCm)
{
#if !UE_BUILD_SHIPPING
    if (!GetWorld() || Path.Num() < 2) return;

    const float HalfWidth = CorridorWidthCm * 0.5f;
    const FVector UpOffset(0, 0, 5.f);

    for (int32 i = 0; i < Path.Num() - 1; ++i)
    {
        FVector Start = Path[i] + UpOffset;
        FVector End = Path[i + 1] + UpOffset;
        FVector Dir = (End - Start).GetSafeNormal();
        FVector Perp = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();

        FVector LeftA = Start - Perp * HalfWidth;
        FVector RightA = Start + Perp * HalfWidth;
        FVector LeftB = End - Perp * HalfWidth;
        FVector RightB = End + Perp * HalfWidth;

        DrawDebugLine(GetWorld(), LeftA, LeftB, FColor::Yellow, false, 10.f, 0, 2.f);
        DrawDebugLine(GetWorld(), RightA, RightB, FColor::Yellow, false, 10.f, 0, 2.f);
        DrawDebugLine(GetWorld(), LeftA, RightA, FColor::Yellow, false, 10.f, 0, 1.f);
        DrawDebugLine(GetWorld(), LeftB, RightB, FColor::Yellow, false, 10.f, 0, 1.f);
    }
#endif
}