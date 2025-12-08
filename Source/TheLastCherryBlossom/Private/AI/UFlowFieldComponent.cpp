#include "AI/UFlowFieldComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetSystemLibrary.h"

UFlowFieldComponent::UFlowFieldComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFlowFieldComponent::BeginPlay()
{
	Super::BeginPlay();
	PathfinderComp = GetOwner()->FindComponentByClass<UGridPathfinderComponent>();
	if (!PathfinderComp)
	{
		UE_LOG(LogTemp, Error, TEXT("FlowFieldComponent: PathfinderComp not found on %s"), *GetOwner()->GetName());
	}
}

FVector UFlowFieldComponent::GridIndexToWorld(const FIntVector& Index) const
{
	// Index برحسب سلول‌ها نسبت به Origin
	float X = (Index.X + 0.5f) * CellSize + Origin.X; // مرکز سلول
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

const FFlowFieldCell* UFlowFieldComponent::GetCell(const FIntPoint& Coord) const
{
	if (Coord.X < 0 || Coord.Y < 0 || Coord.X >= GridWidth || Coord.Y >= GridHeight)
		return nullptr;

	int32 Index = Coord.Y * GridWidth + Coord.X;
	if (!FlowFieldGrid.IsValidIndex(Index))
		return nullptr;

	return &FlowFieldGrid[Index];
}

FVector UFlowFieldComponent::GetDirectionAtLocation(const FVector& Location) const
{
	FIntVector Index = WorldToGridIndex(Location);
	if (Index.X >= 0 && Index.X < GridWidth && Index.Y >= 0 && Index.Y < GridHeight)
	{
		const FFlowFieldCell& Cell = FlowFieldGrid[Index.X + Index.Y * GridWidth];
		return Cell.Direction;
	}
	return FVector::ZeroVector;
}

void UFlowFieldComponent::DebugPrintStats() const
{
	int32 Total = FlowFieldGrid.Num();
	int32 Obst = 0, InCorr = 0, DirCount = 0;
	for (const FFlowFieldCell& C : FlowFieldGrid)
	{
		if (C.bObstacle) Obst++;
		if (C.bInCorridor) InCorr++;
		if (!C.Direction.IsNearlyZero()) DirCount++;
	}
	UE_LOG(LogTemp, Warning, TEXT("FlowField stats: Total=%d Obst=%d InCorridor=%d WithDir=%d GridWxH=%dx%d CellSize=%.1f Origin=(%.1f,%.1f)"),
		Total, Obst, InCorr, DirCount, GridWidth, GridHeight, CellSize, Origin.X, Origin.Y);
}

void UFlowFieldComponent::DrawDebugFlowField() const
{
    if (FlowFieldGrid.Num() == 0 || !GetWorld()) return;

    const float ArrowSize = 15.f;
    const float RepulsionScale = 0.25f;
    const float PathArrowScale = 0.5f;
    const float DirectionThreshold = 0.01f;

    for (int32 y = 0; y < GridHeight; y++)
    {
        for (int32 x = 0; x < GridWidth; x++)
        {
            int32 Index = y * GridWidth + x;
            const FFlowFieldCell& Cell = FlowFieldGrid[Index];

            if (!Cell.bInCorridor) 
                continue;

            FVector Start = GridIndexToWorld(FIntVector(x, y, 0));

            FColor BaseColor = Cell.Direction.IsNearlyZero(DirectionThreshold) ? FColor::Yellow : FColor::Green;

            if (!Cell.Direction.IsNearlyZero(DirectionThreshold))
            {
                FVector End = Start + Cell.Direction.GetSafeNormal() * (CellSize * PathArrowScale);
                DrawDebugDirectionalArrow(GetWorld(), Start, End, ArrowSize, FColor::Green, false, 5.f, 0, 1.5f);
            }
            else
            {
                DrawDebugPoint(GetWorld(), Start, 5.f, BaseColor, false, 10.f);
            }

            if (!Cell.PathVector.IsNearlyZero(DirectionThreshold))
            {
                FVector PathEnd = Start + Cell.PathVector.GetSafeNormal() * (CellSize * PathArrowScale);
                DrawDebugDirectionalArrow(GetWorld(), Start, PathEnd, ArrowSize, FColor::Cyan, false, 2.f, 0, 1.0f);
            }

            if (!Cell.RepulsionVector.IsNearlyZero(DirectionThreshold))
            {
                FVector RepEnd = Start + Cell.RepulsionVector.GetSafeNormal() * (CellSize * RepulsionScale);
                DrawDebugDirectionalArrow(GetWorld(), Start, RepEnd, ArrowSize, FColor::Red, false, 3.f, 0, 0.8f);
            }
        }
    }
}

void UFlowFieldComponent::DrawDebugCorridor(const TArray<FVector>& Path, float CorridorWidthCm)
{
    if (!GetWorld() || Path.Num() < 2) return;

    // 📋 لاگ برای بررسی مقدار واقعی عرض کریدور
    UE_LOG(LogTemp, Warning, TEXT("[DrawDebugCorridor] CorridorWidth = %.1f cm, PathPoints = %d"), CorridorWidthCm, Path.Num());

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

        // ترسیم خطوط کریدور
        DrawDebugLine(GetWorld(), LeftA, LeftB, FColor::Yellow, false, 10.f, 0, 2.f);
        DrawDebugLine(GetWorld(), RightA, RightB, FColor::Yellow, false, 10.f, 0, 2.f);
        DrawDebugLine(GetWorld(), LeftA, RightA, FColor::Yellow, false, 10.f, 0, 1.f);
        DrawDebugLine(GetWorld(), LeftB, RightB, FColor::Yellow, false, 10.f, 0, 1.f);
    }
}

static float DistanceFromPath(const FVector& Point, const TArray<FVector>& Path)
{
    float MinDist = FLT_MAX;

    for (int32 i = 0; i < Path.Num() - 1; i++)
    {
        FVector A = Path[i];
        FVector B = Path[i + 1];

        FVector AB = B - A;
        FVector AP = Point - A;

        float T = FVector::DotProduct(AP, AB) / FVector::DotProduct(AB, AB);
        T = FMath::Clamp(T, 0.f, 1.f);

        FVector Closest = A + T * AB;
        float Dist = FVector::Dist(Point, Closest);

        MinDist = FMath::Min(MinDist, Dist);
    }

    return MinDist;
}

void UFlowFieldComponent::BuildCorridorFromPath(const TArray<FVector>& Path, int32 CorridorWidthCm)
{
    if (Path.Num() < 2) return;

    // انتقال عرض کوریدور (سانتی‌متر) به تعداد سلول‌ها
    int32 HalfWidthCells = FMath::CeilToInt((CorridorWidthCm * 0.5f) / CellSize);
    HalfWidthCells = FMath::Max(1, HalfWidthCells); // حداقل یک سلول پوشش

    float StepSize = CellSize * 0.5f; // نمونه‌برداری با رزولوشن بهتر

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

                    if (!Cell.bObstacle)
                    {
                        Cell.bInCorridor = true;
                        Cell.PathVector = ForwardDir;
                    }
                }
            }
        }

        // پر کردن گوشه‌ها (نمونه برداری مربع اطراف نقطه گوشه)
        if (i < Path.Num() - 2)
        {
            FVector CornerCenter = Path[i + 1];
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

                        if (!Cell.bObstacle)
                        {
                            Cell.bInCorridor = true;
                            // بردار ترکیبی ساده: جهت متوسطِ قبل و بعد
                            FVector PrevF = (Path[i + 1] - Path[i]).GetSafeNormal();
                            FVector NextF = (Path[i + 2] - Path[i + 1]).GetSafeNormal();
                            Cell.PathVector = (PrevF + NextF).GetSafeNormal();
                        }
                    }
                }
            }
        }
    }
}

void UFlowFieldComponent::GenerateFlowField(const FVector& Destination, const TArray<FVector>& Path, int32 CorridorWidthCm)
{
    if (!PathfinderComp || Path.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("FlowFieldComponent: Cannot generate flowfield."));
        return;
    }

    FlowFieldDestination = Destination;

    // ---------- محاسبه محدوده (Bounds) از روی Path و Destination ----------
    FVector Min = Path[0];
    FVector Max = Path[0];
    for (const FVector& P : Path)
    {
        Min.X = FMath::Min(Min.X, P.X);
        Min.Y = FMath::Min(Min.Y, P.Y);
        Max.X = FMath::Max(Max.X, P.X);
        Max.Y = FMath::Max(Max.Y, P.Y);
    }
    Min.X = FMath::Min(Min.X, Destination.X);
    Min.Y = FMath::Min(Min.Y, Destination.Y);
    Max.X = FMath::Max(Max.X, Destination.X);
    Max.Y = FMath::Max(Max.Y, Destination.Y);

    // padding برای اطمینان از اینکه کمی خارج از مسیر هم پوشش داده می‌شود
    const float PaddingCm = 500.f; // 5 متر padding
    Min -= FVector(PaddingCm, PaddingCm, 0);
    Max += FVector(PaddingCm, PaddingCm, 0);

    // ---------- تعیین Origin و اندازهٔ گرید بر حسب CellSize ----------
    const float LocalCellSize = GetCellSize(); // ← نام متغیر تغییر کرد
    Origin = FVector(Min.X, Min.Y, 0);
    GridWidth = FMath::CeilToInt((Max.X - Min.X) / LocalCellSize);
    GridHeight = FMath::CeilToInt((Max.Y - Min.Y) / LocalCellSize);

    GridWidth = FMath::Max(1, GridWidth);
    GridHeight = FMath::Max(1, GridHeight);

    // مقداردهی گرید با اندازهٔ صحیح
    FlowFieldGrid.Init(FFlowFieldCell(), GridWidth * GridHeight);

    // ---------- ذخیره عرض کریدور برای Debug ----------
    DebugCorridorWidthCells = CorridorWidthCm;

    // ---------- ساخت کریدور و علامت‌گذاری سلول‌ها ----------
    BuildCorridorFromPath(Path, CorridorWidthCm);

    // ---------- رسم خطوط زرد برای نمایش عرض کریدور ----------
    DrawDebugCorridor(Path, CorridorWidthCm);

    // ---------- شناسایی موانع ----------
    for (int32 y = 0; y < GridHeight; y++)
    {
        for (int32 x = 0; x < GridWidth; x++)
        {
            int32 Index = y * GridWidth + x;
            FFlowFieldCell& Cell = FlowFieldGrid[Index];

            FVector WorldPos = GridIndexToWorld(FIntVector(x, y, 0));
            if (!PathfinderComp->IsLocationWalkable(WorldPos))
            {
                Cell.bObstacle = true;
                Cell.Direction = FVector::ZeroVector;
            }
        }
    }

    // ---------- محاسبه بردار دافعه ----------
    const float DesiredRepulsionCm = 30.f;
    int32 RepulsionRadius = FMath::CeilToInt(DesiredRepulsionCm / LocalCellSize);
    RepulsionRadius = FMath::Max(1, RepulsionRadius);

    for (int32 y = 0; y < GridHeight; y++)
    {
        for (int32 x = 0; x < GridWidth; x++)
        {
            int32 Index = y * GridWidth + x;
            FFlowFieldCell& Cell = FlowFieldGrid[Index];

            if (Cell.bObstacle) continue;

            FVector Repulsion = FVector::ZeroVector;
            FVector CellWorld = GridIndexToWorld(FIntVector(x, y, 0));

            for (int32 dy = -RepulsionRadius; dy <= RepulsionRadius; dy++)
            {
                for (int32 dx = -RepulsionRadius; dx <= RepulsionRadius; dx++)
                {
                    if (dx == 0 && dy == 0) continue;

                    int32 nx = x + dx;
                    int32 ny = y + dy;

                    if (nx >= 0 && nx < GridWidth && ny >= 0 && ny < GridHeight)
                    {
                        int32 NeighborIndex = ny * GridWidth + nx;
                        if (FlowFieldGrid[NeighborIndex].bObstacle)
                        {
                            float Dist = FMath::Sqrt(float(dx * dx + dy * dy));
                            if (Dist <= RepulsionRadius)
                            {
                                float Weight = 1.f / Dist;
                                FVector ToCell = CellWorld - GridIndexToWorld(FIntVector(nx, ny, 0));
                                Repulsion += ToCell.GetSafeNormal() * Weight;
                            }
                        }
                    }
                }
            }

            Cell.RepulsionVector = Repulsion;
        }
    }

    // ---------- ترکیب مسیر و دافعه ----------
    for (int32 y = 0; y < GridHeight; y++)
    {
        for (int32 x = 0; x < GridWidth; x++)
        {
            int32 Index = y * GridWidth + x;
            FFlowFieldCell& Cell = FlowFieldGrid[Index];

            if (Cell.bObstacle) continue;

            FVector FinalDir = Cell.PathVector + Cell.RepulsionVector;
            if (FinalDir.IsNearlyZero())
            {
                Cell.Direction = Cell.PathVector;
            }
            else
            {
                Cell.Direction = FinalDir.GetSafeNormal();
            }
        }
    }

    DrawDebugFlowField();
    UE_LOG(LogTemp, Warning, TEXT("FlowField generated. Grid=%dx%d CellSize=%.1f Origin=(%.1f,%.1f), CorridorWidth=%dcm"),
        GridWidth, GridHeight, LocalCellSize, Origin.X, Origin.Y, DebugCorridorWidthCells);
}


