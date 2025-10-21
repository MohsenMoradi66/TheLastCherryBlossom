#include "AI/UFlowFieldComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetSystemLibrary.h"

UFlowFieldComponent::UFlowFieldComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FVector UFlowFieldComponent::GetDirectionAtLocation(const FVector& Location) const
{
	FIntVector Index = WorldToGridIndex(Location);
	if (Index.X >= 0 && Index.X < GridSize && Index.Y >= 0 && Index.Y < GridSize)
	{
		const FFlowFieldCell& Cell = FlowFieldGrid[Index.X + Index.Y * GridSize];
		return Cell.Direction;
	}
	return FVector::ZeroVector;
}

FIntVector UFlowFieldComponent::WorldToGridIndex(const FVector& Location) const
{
	FVector RelativeLocation = Location - FlowFieldDestination;
	int32 X = FMath::RoundToInt(RelativeLocation.X / CellSize) + GridSize / 2;
	int32 Y = FMath::RoundToInt(RelativeLocation.Y / CellSize) + GridSize / 2;
	return FIntVector(X, Y, 0);
}

FVector UFlowFieldComponent::GridIndexToWorld(const FIntVector& Index) const
{
	float X = (Index.X - GridSize / 2) * CellSize;
	float Y = (Index.Y - GridSize / 2) * CellSize;
	return FVector(X, Y, 0) + FlowFieldDestination;
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
	UE_LOG(LogTemp, Warning, TEXT("FlowField stats: Total=%d Obst=%d InCorridor=%d WithDir=%d"), Total, Obst, InCorr, DirCount);

	

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

FIntPoint UFlowFieldComponent::WorldToGrid(const FVector& WorldLocation) const
{
	FVector Relative = WorldLocation - FlowFieldDestination;
	int32 X = FMath::RoundToInt(Relative.X / CellSize) + GridSize / 2;
	int32 Y = FMath::RoundToInt(Relative.Y / CellSize) + GridSize / 2;
	return FIntPoint(X, Y);
}

const FFlowFieldCell* UFlowFieldComponent::GetCell(const FIntPoint& Coord) const
{
	if (Coord.X < 0 || Coord.Y < 0 || Coord.X >= GridSize || Coord.Y >= GridSize)
		return nullptr;

	int32 Index = Coord.Y * GridSize + Coord.X;
	if (!FlowFieldGrid.IsValidIndex(Index))
		return nullptr;

	return &FlowFieldGrid[Index];
}

void UFlowFieldComponent::DrawDebugFlowField() const
{
    if (FlowFieldGrid.Num() == 0 || !GetWorld()) return;

    const float ArrowSize = 15.f;
    const float RepulsionScale = 0.25f; // کوتاه‌تر کردن بردار دافعه
    const float PathArrowScale = 0.5f;  // کوتاه‌تر کردن PathVector
    const float DirectionThreshold = 0.01f; // فقط بردارهای قابل توجه رسم شوند

    for (int32 y = 0; y < GridSize; y++)
    {
        for (int32 x = 0; x < GridSize; x++)
        {
            int32 Index = y * GridSize + x;
            const FFlowFieldCell& Cell = FlowFieldGrid[Index];

            // فقط سلول‌های داخل کریدور
            if (!Cell.bInCorridor) 
                continue;

            FVector Start = FlowFieldDestination + FVector((x - GridSize / 2) * CellSize, (y - GridSize / 2) * CellSize, 0);

            // --- رنگ پایه ---
            FColor BaseColor = Cell.Direction.IsNearlyZero(DirectionThreshold) ? FColor::Yellow : FColor::Green;

            // --- Direction نهایی (سبز) ---
            if (!Cell.Direction.IsNearlyZero(DirectionThreshold))
            {
                FVector End = Start + Cell.Direction.GetSafeNormal() * (CellSize * PathArrowScale);
                DrawDebugDirectionalArrow(GetWorld(), Start, End, ArrowSize, FColor::Green, false, 5.f, 0, 1.5f);
            }
            else
            {
                // اگر کریدور است اما جهت ندارد، نقطه زرد
                DrawDebugPoint(GetWorld(), Start, 5.f, BaseColor, false, 10.f);
            }

            // --- PathVector (آبی روشن) ---
            if (!Cell.PathVector.IsNearlyZero(DirectionThreshold))
            {
                FVector PathEnd = Start + Cell.PathVector.GetSafeNormal() * (CellSize * PathArrowScale);
                DrawDebugDirectionalArrow(GetWorld(), Start, PathEnd, ArrowSize, FColor::Cyan, false, 2.f, 0, 1.0f);
            }

            // --- RepulsionVector (قرمز) ---
            if (!Cell.RepulsionVector.IsNearlyZero(DirectionThreshold))
            {
                FVector RepEnd = Start + Cell.RepulsionVector.GetSafeNormal() * (CellSize * RepulsionScale);
                DrawDebugDirectionalArrow(GetWorld(), Start, RepEnd, ArrowSize, FColor::Red, false, 3.f, 0, 0.8f);
            }
        }
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

void UFlowFieldComponent::BuildCorridorFromPath(const TArray<FVector>& Path, int32 CorridorWidth)
{
    if (Path.Num() < 2) return;

    int32 HalfWidth = CorridorWidth / 2;
    float StepSize = CellSize * 0.5f; // نمونه برداری با فاصله کمتر از CellSize

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

            // برای همه سلول‌ها در عرض کریدور
            for (int32 offset = -HalfWidth; offset <= HalfWidth; offset++)
            {
                FVector OffsetPos = Center + RightDir * offset * CellSize;
                FIntPoint CellIndex = WorldToGrid(OffsetPos);

                if (CellIndex.X >= 0 && CellIndex.X < GridSize && CellIndex.Y >= 0 && CellIndex.Y < GridSize)
                {
                    int32 FlatIndex = CellIndex.Y * GridSize + CellIndex.X;
                    FFlowFieldCell& Cell = FlowFieldGrid[FlatIndex];

                    if (!Cell.bObstacle)
                    {
                        Cell.bInCorridor = true;
                        Cell.PathVector = ForwardDir;  // مسیر اصلی
                    }
                }
            }
        }

        // پر کردن گوشه‌ها برای اتصال بخش‌های مسیر
        if (i < Path.Num() - 2)
        {
            FVector NextForward = (Path[i + 2] - Path[i + 1]).GetSafeNormal();
            FVector CornerDir = (ForwardDir + NextForward).GetSafeNormal();

            for (int32 offsetX = -HalfWidth; offsetX <= HalfWidth; offsetX++)
            {
                for (int32 offsetY = -HalfWidth; offsetY <= HalfWidth; offsetY++)
                {
                    FVector CornerPos = Path[i + 1] + FVector(offsetX * CellSize, offsetY * CellSize, 0);
                    FIntPoint CornerIndex = WorldToGrid(CornerPos);

                    if (CornerIndex.X >= 0 && CornerIndex.X < GridSize && CornerIndex.Y >= 0 && CornerIndex.Y < GridSize)
                    {
                        int32 FlatIndex = CornerIndex.Y * GridSize + CornerIndex.X;
                        FFlowFieldCell& Cell = FlowFieldGrid[FlatIndex];

                        if (!Cell.bObstacle)
                        {
                            Cell.bInCorridor = true;
                            Cell.PathVector = CornerDir;  // بردار ترکیبی گوشه
                        }
                    }
                }
            }
        }
    }
}

void UFlowFieldComponent::GenerateFlowField(const FVector& Destination, const TArray<FVector>& Path, int32 CorridorWidth)
{
    FlowFieldDestination = Destination;
    FlowFieldGrid.Init(FFlowFieldCell(), GridSize * GridSize);

    if (!PathfinderComp || Path.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("FlowFieldComponent: Cannot generate flowfield."));
        return;
    }

    // مرحله 1: ساخت کریدور بر اساس مسیر
    BuildCorridorFromPath(Path, CorridorWidth);

    // مرحله 2: شناسایی موانع و غیرفعال کردن مسیر در آنها
    for (int32 y = 0; y < GridSize; y++)
    {
        for (int32 x = 0; x < GridSize; x++)
        {
            int32 Index = y * GridSize + x;
            FFlowFieldCell& Cell = FlowFieldGrid[Index];

            FVector WorldPos = GridIndexToWorld(FIntVector(x, y, 0));
            if (!PathfinderComp->IsLocationWalkable(WorldPos))
            {
                Cell.bObstacle = true;
                Cell.Direction = FVector::ZeroVector;
            }
        }
    }

    // -----------------------------
    // مرحله 3: محاسبه بردار دافعه
    // -----------------------------
    const float DesiredRepulsionCm = 30.f; // دافعه دلخواه (۳۰ سانتیمتر)
    int32 RepulsionRadius = FMath::CeilToInt(DesiredRepulsionCm / CellSize);
    RepulsionRadius = FMath::Max(1, RepulsionRadius); // حداقل یک سلول

    for (int32 y = 0; y < GridSize; y++)
    {
        for (int32 x = 0; x < GridSize; x++)
        {
            int32 Index = y * GridSize + x;
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

                    if (nx >= 0 && nx < GridSize && ny >= 0 && ny < GridSize)
                    {
                        int32 NeighborIndex = ny * GridSize + nx;
                        if (FlowFieldGrid[NeighborIndex].bObstacle)
                        {
                            float Dist = FMath::Sqrt(float(dx*dx + dy*dy));
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

    // مرحله 4: ترکیب بردار مسیر و بردار دافعه
    for (int32 y = 0; y < GridSize; y++)
    {
        for (int32 x = 0; x < GridSize; x++)
        {
            int32 Index = y * GridSize + x;
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
    UE_LOG(LogTemp, Warning, TEXT("FlowField generated with obstacle avoidance (Repulsion=%.1f cm)."), DesiredRepulsionCm);
}







