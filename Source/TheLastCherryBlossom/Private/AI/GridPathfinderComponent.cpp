
#include "AI/GridPathfinderComponent.h"
#include "../TheLastCherryBlossom.h" 
#include "GameFramework/Pawn.h"
#include "Components/CapsuleComponent.h"


void UGridPathfinderComponent::BeginPlay()
{
    Super::BeginPlay();
    
    if (APawn* PawnOwner = Cast<APawn>(GetOwner()))
    {
        if (UCapsuleComponent* Capsule = PawnOwner->FindComponentByClass<UCapsuleComponent>())
        {
            CharacterRadius = Capsule->GetUnscaledCapsuleRadius();
            UE_LOG(LogTemp, Log, TEXT("%s: Character radius = %f"), *GetOwner()->GetName(), CharacterRadius);
        }
        else
        {
            CharacterRadius = 42.f;
        }
    }
}

UGridPathfinderComponent::UGridPathfinderComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

bool UGridPathfinderComponent::FindClosestWalkable(const FVector& Origin, FVector& OutLocation) const
{
    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!NavSys) return false;
    
    FNavLocation NavLocation;
    if (NavSys->ProjectPointToNavigation(Origin, NavLocation, FVector(SearchRadius)))
    {
        OutLocation = NavLocation.Location;
        return true;
    }
    return false;
}

void UGridPathfinderComponent::RequestPathAsync(FVector StartWorld, FVector GoalWorld, FPathResultCallback OnPathReady)
{
    if (!OnPathReady) return;
    
    FPathRequest Request(StartWorld, GoalWorld, MoveTemp(OnPathReady));
    PendingRequests.Enqueue(Request);
}

void UGridPathfinderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    ProcessActiveRequest();
}

void UGridPathfinderComponent::ProcessActiveRequest()
{
    if (!bHasActiveRequest)
    {
        StartNextRequest();
        if (!bHasActiveRequest) return;
    }
    
    if (bCancelCurrent)
    {
        bCancelCurrent = false;
        bHasActiveRequest = false;
        ActiveRequest.Reset();
        StartNextRequest();
        return;
    }
    
    switch (ActiveRequest.State)
    {
    case EPathfindingState::DirectLOS:
        ProcessDirectLOS();
        break;
    case EPathfindingState::NavQuerying:
        ProcessNavQuery();
        break;
    case EPathfindingState::Resampling:
        ProcessResampling();
        break;
    case EPathfindingState::Smoothing:
        ProcessSmoothing();
        break;
    case EPathfindingState::Finished:
        bHasActiveRequest = false;
        ActiveRequest.Reset();
        StartNextRequest();
        break;
    default:
        break;
    }
}

void UGridPathfinderComponent::StartNextRequest()
{
    if (bHasActiveRequest) return;
    
    FPathRequest Req;
    if (!PendingRequests.Dequeue(Req)) return;
    
    ActiveRequest.Reset();
    ActiveRequest.Start = Req.Start;
    ActiveRequest.Goal = Req.Goal;
    ActiveRequest.Callback = MoveTemp(Req.Callback);
    ActiveRequest.State = EPathfindingState::DirectLOS;
    bHasActiveRequest = true;
    bCancelCurrent = false;
}

void UGridPathfinderComponent::ProcessDirectLOS()
{
    FCollisionObjectQueryParams ObjectQueryParams;
    SetupObstacleQueryParams(ObjectQueryParams);
    
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GetOwner());
    
    FHitResult HitResult;
    bool bBlocked = GetWorld()->SweepSingleByObjectType(
        HitResult,
        ActiveRequest.Start,
        ActiveRequest.Goal,
        FQuat::Identity,
        ObjectQueryParams,
        FCollisionShape::MakeSphere(CharacterRadius),
        QueryParams);
    
    if (!bBlocked)
    {
        TArray<FVector> DirectPath = { ActiveRequest.Start, ActiveRequest.Goal };
        FinishRequest(true, DirectPath);
        return;
    }
    
    ActiveRequest.State = EPathfindingState::NavQuerying;
    bWaitingForNavResult = false;
}

void UGridPathfinderComponent::ProcessNavQuery()
{
    if (!bWaitingForNavResult)
    {
        UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
        if (!NavSys)
        {
            FinishRequest(false);
            return;
        }
        
        FVector ActualGoal = ActiveRequest.Goal;
        if (!IsLocationWalkable(ActiveRequest.Goal))
        {
            if (!FindClosestWalkable(ActiveRequest.Goal, ActualGoal))
            {
                FinishRequest(false);
                return;
            }
        }
        
        FPathFindingQuery Query(GetOwner(), *NavSys->GetDefaultNavDataInstance(), ActiveRequest.Start, ActualGoal);
        Query.bAllowPartialPaths = true;
        
        PendingResult = NavSys->FindPathSync(Query, EPathFindingMode::Regular);
        bWaitingForNavResult = true;
        return;
    }
    
    if (PendingResult.Result != ENavigationQueryResult::Success || !PendingResult.Path.IsValid())
    {
        FinishRequest(false);
        return;
    }
    
    ActiveRequest.RawPath.Empty();
    const TArray<FNavPathPoint>& PathPoints = PendingResult.Path->GetPathPoints();
    for (const FNavPathPoint& Point : PathPoints)
    {
        ActiveRequest.RawPath.Add(Point.Location);
    }
    
    if (ActiveRequest.RawPath.Num() > 0 && ActiveRequest.RawPath[0].Equals(ActiveRequest.Start, 1.0f))
    {
        ActiveRequest.RawPath.RemoveAt(0);
    }
    
    ActiveRequest.State = EPathfindingState::Resampling;
    ActiveRequest.ResampleCurrentIndex = 0;
    ActiveRequest.ResampleRemaining = CharacterRadius * SegmentLengthMultiplier;
    
    if (ActiveRequest.RawPath.Num() >= 1)
    {
        ActiveRequest.ResampleCurrentPoint = ActiveRequest.RawPath[0];
    }
    else
    {
        FinishRequest(false);
    }
}

void UGridPathfinderComponent::ProcessResampling()
{
    if (ActiveRequest.RawPath.Num() < 2)
    {
        ActiveRequest.ResampledPath = ActiveRequest.RawPath;
        ActiveRequest.State = EPathfindingState::Smoothing;
        ActiveRequest.SmoothStartIndex = 0;
        ActiveRequest.FinalPath.Empty();
        return;
    }
    
    const float SegmentLength = CharacterRadius * SegmentLengthMultiplier;
    TArray<FVector>& OutResampled = ActiveRequest.ResampledPath;
    
    if (ActiveRequest.ResampleCurrentIndex == 0)
    {
        OutResampled.Empty();
        OutResampled.Add(ActiveRequest.RawPath[0]);
        ActiveRequest.ResampleCurrentPoint = ActiveRequest.RawPath[0];
        ActiveRequest.ResampleRemaining = SegmentLength;
        ActiveRequest.ResampleCurrentIndex = 1;
    }
    
    int32 StepsDone = 0;
    while (StepsDone < ResampleStepsPerFrame && ActiveRequest.ResampleCurrentIndex < ActiveRequest.RawPath.Num())
    {
        FVector Next = ActiveRequest.RawPath[ActiveRequest.ResampleCurrentIndex];
        FVector Dir = (Next - ActiveRequest.ResampleCurrentPoint).GetSafeNormal();
        float Dist = FVector::Dist(ActiveRequest.ResampleCurrentPoint, Next);
        
        while (Dist >= ActiveRequest.ResampleRemaining && StepsDone < ResampleStepsPerFrame)
        {
            FVector NewPoint = ActiveRequest.ResampleCurrentPoint + Dir * ActiveRequest.ResampleRemaining;
            OutResampled.Add(NewPoint);
            ActiveRequest.ResampleCurrentPoint = NewPoint;
            Dist -= ActiveRequest.ResampleRemaining;
            ActiveRequest.ResampleRemaining = SegmentLength;
            StepsDone++;
        }
        
        ActiveRequest.ResampleRemaining -= Dist;
        ActiveRequest.ResampleCurrentPoint = Next;
        ActiveRequest.ResampleCurrentIndex++;
    }
    
    if (ActiveRequest.ResampleCurrentIndex >= ActiveRequest.RawPath.Num())
    {
        const FVector& LastRaw = ActiveRequest.RawPath.Last();
        if (!OutResampled.Last().Equals(LastRaw, KINDA_SMALL_NUMBER))
            OutResampled.Add(LastRaw);
        
        ActiveRequest.State = EPathfindingState::Smoothing;
        ActiveRequest.SmoothStartIndex = 0;
        ActiveRequest.FinalPath.Empty();
    }
}

void UGridPathfinderComponent::ProcessSmoothing()
{
    TArray<FVector>& InputPath = ActiveRequest.ResampledPath;
    
    if (InputPath.Num() < 2)
    {
        FinishRequest(true, InputPath);
        return;
    }
    
    if (ActiveRequest.FinalPath.Num() == 0)
    {
        ActiveRequest.FinalPath.Add(InputPath[0]);
        ActiveRequest.SmoothStartIndex = 0;
    }
    
    FCollisionObjectQueryParams ObjectQueryParams;
    SetupObstacleQueryParams(ObjectQueryParams);
    
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GetOwner());
    
    for (int32 Step = 0; Step < SmoothingStepsPerFrame; ++Step)
    {
        if (ActiveRequest.SmoothStartIndex >= InputPath.Num() - 1)
            break;
        
        int32 EndIndex = InputPath.Num() - 1;
        while (EndIndex > ActiveRequest.SmoothStartIndex + 1)
        {
            FVector Start = InputPath[ActiveRequest.SmoothStartIndex];
            FVector End = InputPath[EndIndex];
            FHitResult HitResult;
            
            bool bBlocked = GetWorld()->SweepSingleByObjectType(
                HitResult, Start, End, FQuat::Identity, ObjectQueryParams,
                FCollisionShape::MakeSphere(CharacterRadius), QueryParams);
            
            if (!bBlocked)
                break;
            
            EndIndex--;
        }
        
        ActiveRequest.FinalPath.Add(InputPath[EndIndex]);
        ActiveRequest.SmoothStartIndex = EndIndex;
    }
    
    if (ActiveRequest.SmoothStartIndex >= InputPath.Num() - 1)
        FinishRequest(true, ActiveRequest.FinalPath);
}

void UGridPathfinderComponent::FinishRequest(bool bSuccess, const TArray<FVector>& Path)
{
    if (ActiveRequest.Callback)
    {
        if (bSuccess && Path.Num() >= 2)
            ActiveRequest.Callback(Path);
        else
            ActiveRequest.Callback(TArray<FVector>());
    }
    
    ActiveRequest.State = EPathfindingState::Finished;
}

TArray<FVector> UGridPathfinderComponent::ResampleFullPath(const TArray<FVector>& InputPath, float SegmentLength) const
{
    TArray<FVector> Resampled;
    if (InputPath.Num() < 2) return InputPath;
    
    Resampled.Add(InputPath[0]);
    float Remaining = SegmentLength;
    FVector Current = InputPath[0];
    
    for (int32 i = 1; i < InputPath.Num(); i++)
    {
        FVector Next = InputPath[i];
        FVector Dir = (Next - Current).GetSafeNormal();
        float Dist = FVector::Dist(Current, Next);
        
        while (Dist >= Remaining)
        {
            FVector NewPoint = Current + Dir * Remaining;
            Resampled.Add(NewPoint);
            Current = NewPoint;
            Dist -= Remaining;
            Remaining = SegmentLength;
        }
        
        Remaining -= Dist;
        Current = Next;
    }
    
    if (!Resampled.Last().Equals(InputPath.Last(), KINDA_SMALL_NUMBER))
        Resampled.Add(InputPath.Last());
    
    return Resampled;
}

// ========== تابع SmoothFullPath (اضافه شد) ==========
TArray<FVector> UGridPathfinderComponent::SmoothFullPath(const TArray<FVector>& InputPath)
{
    TArray<FVector> SmoothedPath;
    if (InputPath.Num() < 2) return InputPath;
    
    FCollisionObjectQueryParams ObjectQueryParams;
    SetupObstacleQueryParams(ObjectQueryParams);
    
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GetOwner());
    
    int32 StartIndex = 0;
    SmoothedPath.Add(InputPath[0]);
    
    while (StartIndex < InputPath.Num() - 1)
    {
        int32 EndIndex = InputPath.Num() - 1;
        while (EndIndex > StartIndex + 1)
        {
            FHitResult HitResult;
            bool bBlocked = GetWorld()->SweepSingleByObjectType(
                HitResult, InputPath[StartIndex], InputPath[EndIndex], FQuat::Identity,
                ObjectQueryParams,
                FCollisionShape::MakeSphere(CharacterRadius),
                QueryParams);
            
            if (!bBlocked) break;
            EndIndex--;
        }
        
        SmoothedPath.Add(InputPath[EndIndex]);
        StartIndex = EndIndex;
    }
    
    return SmoothedPath;
}

TArray<FVector> UGridPathfinderComponent::FindPathShared(const FVector& Start, const FVector& Goal)
{
    TArray<FVector> FinalPath;
    
    if (!GetWorld())
    {
        UE_LOG(LogTemp, Warning, TEXT("FindPathShared: No World"));
        return FinalPath;
    }
    
    // تست مسیر مستقیم
    {
        FCollisionObjectQueryParams ObjectQueryParams;
        SetupObstacleQueryParams(ObjectQueryParams);
        
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(GetOwner());
        
        FHitResult HitResult;
        bool bBlocked = GetWorld()->SweepSingleByObjectType(
            HitResult, Start, Goal, FQuat::Identity,
            ObjectQueryParams,
            FCollisionShape::MakeSphere(CharacterRadius),
            QueryParams);
        
        if (!bBlocked)
        {
            FinalPath.Add(Start);
            FinalPath.Add(Goal);
            return FinalPath;
        }
    }
    
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavSys)
    {
        UE_LOG(LogTemp, Warning, TEXT("FindPathShared: No NavigationSystem"));
        return FinalPath;
    }
    
    FVector ActualGoal = Goal;
    if (!IsLocationWalkable(Goal))
    {
        if (!FindClosestWalkable(Goal, ActualGoal))
        {
            UE_LOG(LogTemp, Warning, TEXT("FindPathShared: Could not find walkable location near goal"));
            return FinalPath;
        }
    }
    
    FPathFindingQuery Query(GetOwner(), *NavSys->GetDefaultNavDataInstance(), Start, ActualGoal);
    Query.bAllowPartialPaths = true;
    
    FPathFindingResult Result = NavSys->FindPathSync(Query, EPathFindingMode::Regular);
    
    if (Result.Result != ENavigationQueryResult::Success || !Result.Path.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("FindPathShared: Pathfinding failed with result %d"), (int32)Result.Result);
        return FinalPath;
    }
    
    for (const FNavPathPoint& Point : Result.Path->GetPathPoints())
        FinalPath.Add(Point.Location);
    
    if (FinalPath.Num() > 0 && FinalPath[0].Equals(Start, 1.0f))
        FinalPath.RemoveAt(0);
    
    float SegmentLength = CharacterRadius * SegmentLengthMultiplier;
    TArray<FVector> Resampled = ResampleFullPath(FinalPath, SegmentLength);
    FinalPath = SmoothFullPath(Resampled);
    
    return FinalPath;
}

void UGridPathfinderComponent::CancelAllRequests()
{
    FPathRequest Dummy;
    while (PendingRequests.Dequeue(Dummy)) {}
    bCancelCurrent = true;
    
    if (bHasActiveRequest && ActiveRequest.Callback)
    {
        ActiveRequest.Callback(TArray<FVector>());
    }
    bHasActiveRequest = false;
    ActiveRequest.Reset();
}

bool UGridPathfinderComponent::IsLocationWalkable(const FVector& Location) const
{
    if (!GetWorld()) return false;
    
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavSys) return false;
    
    FNavLocation NavLocation;
    if (!NavSys->ProjectPointToNavigation(Location, NavLocation))
        return false;
    
    FCollisionObjectQueryParams ObjectQueryParams;
    SetupObstacleQueryParams(ObjectQueryParams);
    
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GetOwner());
    
    bool bBlocked = GetWorld()->OverlapAnyTestByObjectType(
        Location, FQuat::Identity, ObjectQueryParams,
        FCollisionShape::MakeSphere(CharacterRadius * 1.3f), QueryParams);
    
    return !bBlocked;
}

void UGridPathfinderComponent::SetupObstacleQueryParams(FCollisionObjectQueryParams& ObjectQueryParams) const
{
    // ۱. اسکن کانال اول (موانع اختصاصی یا RTS_Obstacle)
    ObjectQueryParams.AddObjectTypesToQuery(ECC_RTS_Obstacle); 
    
    // ۲. اسکن کانال دوم (ساختمان‌ها و درختان که در ادیتور ست کردی)
    ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel2); 
}