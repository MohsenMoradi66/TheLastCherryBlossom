#include "AI/UFlowFieldComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Containers/Queue.h"  // برای TQueue در Flood Fill

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
    // اگر خارج از محدوده گرید باشد، یک سلول خالی برگردون
    if (Coord.X < 0 || Coord.Y < 0 || Coord.X >= GridWidth || Coord.Y >= GridHeight)
    {
        return FFlowFieldCell();
    }

    int32 Index = Coord.Y * GridWidth + Coord.X;
    if (FlowFieldGrid.IsValidIndex(Index))
    {
        return FlowFieldGrid[Index];
    }

    return FFlowFieldCell(); // در صورت نامعتبر بودن ایندکس
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

void UFlowFieldComponent::MarkReachableCellsFromDestination()
{
    if (FlowFieldGrid.Num() == 0) return;

    TArray<bool> Visited;
    Visited.Init(false, GridWidth * GridHeight);

    TQueue<FIntPoint> Queue;

    FIntPoint DestGrid = WorldToGrid(FlowFieldDestination);
    if (DestGrid.X >= 0 && DestGrid.X < GridWidth && DestGrid.Y >= 0 && DestGrid.Y < GridHeight)
    {
        int32 DestIndex = DestGrid.Y * GridWidth + DestGrid.X;
        const FFlowFieldCell& DestCell = FlowFieldGrid[DestIndex];
        if (!DestCell.bObstacle && DestCell.bInCorridor)
        {
            Queue.Enqueue(DestGrid);
            Visited[DestIndex] = true;
        }
    }

    // 8 جهت برای اتصال بهتر (مورب هم اجازه می‌ده)
    const TArray<FIntPoint> Directions = {
        FIntPoint(0,1), FIntPoint(0,-1), FIntPoint(1,0), FIntPoint(-1,0),
        FIntPoint(1,1), FIntPoint(1,-1), FIntPoint(-1,1), FIntPoint(-1,-1)
    };

    while (!Queue.IsEmpty())
    {
        FIntPoint Current;
        Queue.Dequeue(Current);

        for (const FIntPoint& Dir : Directions)
        {
            FIntPoint Neighbor = Current + Dir;
            if (Neighbor.X >= 0 && Neighbor.X < GridWidth && Neighbor.Y >= 0 && Neighbor.Y < GridHeight)
            {
                int32 NIndex = Neighbor.Y * GridWidth + Neighbor.X;
                const FFlowFieldCell& NCell = FlowFieldGrid[NIndex];
                if (!Visited[NIndex] && !NCell.bObstacle && NCell.bInCorridor)
                {
                    Visited[NIndex] = true;
                    Queue.Enqueue(Neighbor);
                }
            }
        }
    }

    // حذف سلول‌هایی که به مقصد وصل نیستند (Dead Ends)
    int32 RemovedCount = 0;
    for (int32 i = 0; i < FlowFieldGrid.Num(); i++)
    {
        if (FlowFieldGrid[i].bInCorridor && !Visited[i])
        {
            FlowFieldGrid[i].bInCorridor = false;
            FlowFieldGrid[i].PathVector = FVector::ZeroVector;
            FlowFieldGrid[i].Direction = FVector::ZeroVector;
            RemovedCount++;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("MarkReachableCells: Removed %d dead-end cells."), RemovedCount);
}

void UFlowFieldComponent::SmoothDirections(int32 Iterations /*= 5*/) // افزایش تکرار برای نرم‌تر شدن
{
    if (FlowFieldGrid.Num() == 0 || Iterations <= 0) return;

    TArray<FFlowFieldCell> TempGrid = FlowFieldGrid;

    const TArray<FIntPoint> Directions = {
        FIntPoint(0,1), FIntPoint(0,-1), FIntPoint(1,0), FIntPoint(-1,0)
    };

    for (int32 Iter = 0; Iter < Iterations; ++Iter)
    {
        for (int32 y = 0; y < GridHeight; ++y)
        {
            for (int32 x = 0; x < GridWidth; ++x)
            {
                int32 Index = y * GridWidth + x;
                const FFlowFieldCell& Cell = FlowFieldGrid[Index];
                if (Cell.bObstacle || !Cell.bInCorridor) continue;

                FVector AvgDir = Cell.Direction;
                int32 Count = 1;

                for (const FIntPoint& Dir : Directions)
                {
                    int32 nx = x + Dir.X;
                    int32 ny = y + Dir.Y;
                    if (nx >= 0 && nx < GridWidth && ny >= 0 && ny < GridHeight)
                    {
                        int32 NIndex = ny * GridWidth + nx;
                        const FFlowFieldCell& NCell = FlowFieldGrid[NIndex];
                        if (!NCell.bObstacle && NCell.bInCorridor && !NCell.Direction.IsNearlyZero())
                        {
                            AvgDir += NCell.Direction;
                            Count++;
                        }
                    }
                }

                if (Count > 1)
                {
                    TempGrid[Index].Direction = (AvgDir / Count).GetSafeNormal();
                }
            }
        }
        FlowFieldGrid = TempGrid;
    }

    UE_LOG(LogTemp, Warning, TEXT("Smoothed directions over %d iterations."), Iterations);
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

                    if (!Cell.bObstacle)
                    {
                        Cell.bInCorridor = true;
                        Cell.PathVector = ForwardDir;
                    }
                }
            }
        }

        // پر کردن گوشه‌ها
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
        UE_LOG(LogTemp, Warning, TEXT("FlowFieldComponent: Cannot generate flowfield - missing path or pathfinder."));
        return;
    }

    FlowFieldDestination = Destination;

    // محاسبه Bounds با padding
    FVector Min = Path[0];
    FVector Max = Path[0];
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

    float LocalCellSize = GetCellSize();
    Origin = FVector(Min.X, Min.Y, 0);
    GridWidth = FMath::Max(1, FMath::CeilToInt((Max.X - Min.X) / LocalCellSize));
    GridHeight = FMath::Max(1, FMath::CeilToInt((Max.Y - Min.Y) / LocalCellSize));

    FlowFieldGrid.Init(FFlowFieldCell(), GridWidth * GridHeight);
    DebugCorridorWidthCells = CorridorWidthCm;

    // 1. ساخت کریدور
    BuildCorridorFromPath(Path, CorridorWidthCm);

    // 2. حذف Dead Ends
    MarkReachableCellsFromDestination();

    // 3. رسم دیباگ کریدور
    DrawDebugCorridor(Path, CorridorWidthCm);

    // 4. شناسایی موانع (از Pathfinder)
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
                Cell.bInCorridor = false;
                Cell.Direction = FVector::ZeroVector;
            }
        }
    }

    // 5. محاسبه بردار دافعه — فقط از موانع دقیقاً روبه‌رو
    const float DesiredRepulsionCm = 200.f;           // شعاع تأثیر دافعه
    const float RepulsionStrength = 2.0f;             // قدرت دافعه (افزایش یافته چون محدودتر شده)
    const float FrontDotThreshold = 0.866f;           // cos(30°) → فقط موانع با زاویه کمتر از ۳۰ درجه نسبت به جلو
    int32 RepulsionRadiusCells = FMath::Max(1, FMath::CeilToInt(DesiredRepulsionCm / LocalCellSize));

    for (int32 y = 0; y < GridHeight; y++)
    {
        for (int32 x = 0; x < GridWidth; x++)
        {
            int32 Index = y * GridWidth + x;
            FFlowFieldCell& Cell = FlowFieldGrid[Index];
            if (Cell.bObstacle || !Cell.bInCorridor || Cell.PathVector.IsNearlyZero()) continue;

            FVector PathDir = Cell.PathVector.GetSafeNormal();
            FVector Repulsion = FVector::ZeroVector;
            FVector CellWorld = GridIndexToWorld(FIntVector(x, y, 0));

            for (int32 dy = -RepulsionRadiusCells; dy <= RepulsionRadiusCells; dy++)
            {
                for (int32 dx = -RepulsionRadiusCells; dx <= RepulsionRadiusCells; dx++)
                {
                    if (dx == 0 && dy == 0) continue;

                    int32 nx = x + dx;
                    int32 ny = y + dy;
                    if (nx >= 0 && nx < GridWidth && ny >= 0 && ny < GridHeight)
                    {
                        int32 NIndex = ny * GridWidth + nx;
                        if (FlowFieldGrid[NIndex].bObstacle)
                        {
                            FVector ObstWorld = GridIndexToWorld(FIntVector(nx, ny, 0));
                            FVector DirToObst = (ObstWorld - CellWorld).GetSafeNormal();
                            float Dot = FVector::DotProduct(PathDir, DirToObst);

                            // فقط موانع دقیقاً روبه‌رو تأثیر می‌گذارند
                            if (Dot > FrontDotThreshold)
                            {
                                float Dist = FVector::Dist(CellWorld, ObstWorld);
                                if (Dist <= DesiredRepulsionCm)
                                {
                                    // شیب ملایم: هرچه نزدیک‌تر، دافعه قوی‌تر
                                    float Weight = (1.f - Dist / DesiredRepulsionCm) * RepulsionStrength;

                                    // جهت دافعه: مستقیماً دور شدن از مانع
                                    FVector RepulseDir = (CellWorld - ObstWorld).GetSafeNormal();

                                    Repulsion += RepulseDir * Weight;
                                }
                            }
                        }
                    }
                }
            }

            Cell.RepulsionVector = Repulsion;
        }
    }

    // 6. ترکیب نهایی: PathVector + RepulsionVector
    for (int32 i = 0; i < FlowFieldGrid.Num(); i++)
    {
        FFlowFieldCell& Cell = FlowFieldGrid[i];
        if (Cell.bObstacle || !Cell.bInCorridor) continue;

        FVector FinalDir = Cell.PathVector + Cell.RepulsionVector;

        // اگر دافعه خیلی قوی باشه و جهت رو کامل معکوس کنه، حداقل جهت اصلی حفظ بشه
        Cell.Direction = FinalDir.IsNearlyZero() ? Cell.PathVector : FinalDir.GetSafeNormal();
    }

    // 7. نرم کردن جهت‌ها (Smoothing)
    SmoothDirections(5);

    // 8. رسم نهایی Flow Field
    DrawDebugFlowField();

    UE_LOG(LogTemp, Warning, TEXT("FlowField generated successfully. Grid=%dx%d CellSize=%.1f Origin=(%.1f,%.1f) Corridor=%dcm"),
        GridWidth, GridHeight, LocalCellSize, Origin.X, Origin.Y, DebugCorridorWidthCells);

    DebugPrintStats();
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
    const float PathArrowScale = 0.5f;
    const float DirectionThreshold = 0.01f;

    for (int32 y = 0; y < GridHeight; y++)
    {
        for (int32 x = 0; x < GridWidth; x++)
        {
            int32 Index = y * GridWidth + x;
            const FFlowFieldCell& Cell = FlowFieldGrid[Index];

            if (!Cell.bInCorridor) continue;

            FVector Start = GridIndexToWorld(FIntVector(x, y, 0));

            // جهت نهایی (سبز)
            if (!Cell.Direction.IsNearlyZero(DirectionThreshold))
            {
                FVector End = Start + Cell.Direction.GetSafeNormal() * (CellSize * PathArrowScale);
                DrawDebugDirectionalArrow(GetWorld(), Start, End, ArrowSize, FColor::Green, false, 5.f, 0, 1.5f);
            }
            else
            {
                DrawDebugPoint(GetWorld(), Start, 8.f, FColor::Yellow, false, 10.f);
            }
        }
    }
}

void UFlowFieldComponent::DrawDebugCorridor(const TArray<FVector>& Path, float CorridorWidthCm)
{
    if (!GetWorld() || Path.Num() < 2) return;

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

        DrawDebugLine(GetWorld(), LeftA, LeftB, FColor::Yellow, false, 10.f, 0, 2.f);
        DrawDebugLine(GetWorld(), RightA, RightB, FColor::Yellow, false, 10.f, 0, 2.f);
        DrawDebugLine(GetWorld(), LeftA, RightA, FColor::Yellow, false, 10.f, 0, 1.f);
        DrawDebugLine(GetWorld(), LeftB, RightB, FColor::Yellow, false, 10.f, 0, 1.f);
    }
}