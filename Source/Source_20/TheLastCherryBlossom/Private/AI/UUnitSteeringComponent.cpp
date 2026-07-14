// UUnitSteeringComponent.cpp
#include "AI/UUnitSteeringComponent.h"
#include "AI/UUnitMovementComponent.h"
#include "Characters/AUnitCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "../TheLastCherryBlossom.h" 
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

// ================== سازنده و مقداردهی اولیه ==================

UUnitSteeringComponent::UUnitSteeringComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    CurrentDirection = FVector::ForwardVector;
    bIsEvading = false;
    ActiveEvadeSource = EEvadeSource::None;
}

void UUnitSteeringComponent::Initialize(AUnitCharacter* InOwner)
{
    Owner = InOwner;
    if (!Owner) return;

    UCharacterMovementComponent* MoveComp = Owner->GetCharacterMovement();
    if (MoveComp)
    {
        MoveComp->bUseControllerDesiredRotation = false;
        MoveComp->bOrientRotationToMovement = false;
        MoveComp->bEnablePhysicsInteraction = false;
        MoveComp->bPushForceUsingZOffset = false;
        MoveComp->SetPlaneConstraintEnabled(true);
        MoveComp->SetPlaneConstraintNormal(FVector::UpVector);
    }

    UCapsuleComponent* Capsule = Owner->GetCapsuleComponent();
    if (Capsule)
    {
        Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
}

void UUnitSteeringComponent::ClearTarget()
{
    bHasTarget = false;
    bIsEvading = false;
    ActiveEvadeSource = EEvadeSource::None;
    
    if (Owner && Owner->GetCharacterMovement())
    {
        Owner->GetCharacterMovement()->StopMovementImmediately();
        Owner->GetCharacterMovement()->Velocity = FVector::ZeroVector;
    }
}

void UUnitSteeringComponent::ResetDirection()
{
    CurrentDirection = FVector::ZeroVector;
    bIsEvading = false;
    ActiveEvadeSource = EEvadeSource::None;
}

void UUnitSteeringComponent::ClearPath()
{
    bHasPath = false;
    CurrentPath.Empty();
    TotalPathLength = 0.f;
    PathCache.Reset();
    
    bHasTarget = false;
    TargetSlot = FVector::ZeroVector;
    
    CurrentDirection = FVector::ZeroVector;
    bHasGroupCenter = false;
    GroupCenter = FVector::ZeroVector;
    GroupForward = FVector::ForwardVector;
    
    DesiredLateralOffset = 0.f;
    bIsFirstFrameOfNewPath = false;
    TimeSinceLastMoveCommand = 0.f;
    bIsEvading = false;
    ActiveEvadeSource = EEvadeSource::None;
}

void UUnitSteeringComponent::SetGroupParams(const FVector& InCenter, const FVector& InForward)
{
    GroupCenter = InCenter;
    GroupForward = InForward.GetSafeNormal();
    bHasGroupCenter = true;
}

void UUnitSteeringComponent::ClearGroupParams()
{
    bHasGroupCenter = false;
}

void UUnitSteeringComponent::ForceStop()
{
    if (Owner && Owner->GetCharacterMovement())
    {
        Owner->GetCharacterMovement()->StopMovementImmediately();
        Owner->GetCharacterMovement()->Velocity = FVector::ZeroVector;
    }
    
    ClearPath();
    ClearTarget();
    ResetDirection();
    ClearGroupParams();
    DesiredLateralOffset = 0.f;
    bIsFirstFrameOfNewPath = false;
    TimeSinceLastMoveCommand = 0.f;
}

// ================== مدیریت مسیر ==================

void UUnitSteeringComponent::SetPath(const TArray<FVector>& NewPath)
{
    ClearPath();
    if (NewPath.Num() < 2) return;
    
    bool bWasAlreadyMoving = bHasTarget && (Owner->GetVelocity().Size2D() > 10.f);
    FVector SavedDirection = bWasAlreadyMoving ? Owner->GetVelocity().GetSafeNormal() : FVector::ZeroVector;
    
    CurrentPath = NewPath;
    UpdatePathCache();
    TotalPathLength = PathCache.TotalLength;
    bHasPath = true;
    
    float ReusedOffset = DesiredLateralOffset;
    if (!bWasAlreadyMoving || FMath::IsNearlyZero(ReusedOffset, 1.f))
    {
        if (CurrentPath.Num() >= 2)
        {
            FVector StartPoint = CurrentPath[0];
            FVector NextPoint = CurrentPath[1];
            FVector Tangent = (NextPoint - StartPoint).GetSafeNormal();
            FVector RightDir = FVector::CrossProduct(Tangent, FVector::UpVector).GetSafeNormal();
            FVector ToUnit = Owner->GetActorLocation() - StartPoint;
            ReusedOffset = FVector::DotProduct(ToUnit, RightDir);
        }
    }
    
    DesiredLateralOffset = ReusedOffset;
    
    if (Owner->GetCharacterMovement() && CurrentPath.Num() >= 2)
    {
        FVector BaseTangent = (CurrentPath[1] - CurrentPath[0]).GetSafeNormal();
        FVector MoveDir = bWasAlreadyMoving ? SavedDirection : BaseTangent;
    
        float ActualMaxSpeed = Owner->GetCharacterMovement()->MaxWalkSpeed;
        Owner->GetCharacterMovement()->Velocity = MoveDir * ActualMaxSpeed;

        ResetDirection();
        bIsFirstFrameOfNewPath = true;
        TimeSinceLastMoveCommand = 0.f;
    }
}

// ================== کش مسیر ==================

void UUnitSteeringComponent::UpdatePathCache()
{
    PathCache.Reset();
    
    if (CurrentPath.Num() < 2) return;
    
    float Accumulated = 0.f;
    for (int32 i = 0; i < CurrentPath.Num() - 1; i++)
    {
        float Len = FVector::Dist(CurrentPath[i], CurrentPath[i + 1]);
        if (Len < SMALL_NUMBER) continue;
        
        PathCache.SegmentLengths.Add(Len);
        PathCache.AccumulatedLengths.Add(Accumulated);
        Accumulated += Len;
    }
    PathCache.TotalLength = Accumulated;
}

float UUnitSteeringComponent::GetDistanceAlongPath(const FVector& Location) const
{
    if (!PathCache.IsValid()) return 0.f;
    
    float BestDistance = 0.f;
    float BestDistSq = FLT_MAX;
    
    for (int32 i = 0; i < CurrentPath.Num() - 1; i++)
    {
        const FVector& A = CurrentPath[i];
        const FVector& B = CurrentPath[i + 1];
        FVector AB = B - A;
        float ABLenSq = AB.SizeSquared();
        
        if (ABLenSq < SMALL_NUMBER) continue;
        
        FVector AC = Location - A;
        float t = FMath::Clamp(FVector::DotProduct(AC, AB) / ABLenSq, 0.f, 1.f);
        FVector Closest = A + AB * t;
        float DistSq = (Location - Closest).SizeSquared();
        
        if (DistSq < BestDistSq)
        {
            BestDistSq = DistSq;
            BestDistance = PathCache.AccumulatedLengths[i] + (t * PathCache.SegmentLengths[i]);
        }
    }
    
    return BestDistance;
}

bool UUnitSteeringComponent::GetPointOnPathFast(float DistanceFromStart, FVector& OutPoint, FVector& OutTangent) const
{
    if (!PathCache.IsValid()) return false;
    
    DistanceFromStart = FMath::Clamp(DistanceFromStart, 0.f, PathCache.TotalLength);
    
    for (int32 i = 0; i < PathCache.SegmentLengths.Num(); i++)
    {
        float SegStart = PathCache.AccumulatedLengths[i];
        float SegEnd = SegStart + PathCache.SegmentLengths[i];
        
        if (DistanceFromStart <= SegEnd)
        {
            float t = (DistanceFromStart - SegStart) / PathCache.SegmentLengths[i];
            const FVector& A = CurrentPath[i];
            const FVector& B = CurrentPath[i + 1];
            
            OutPoint = FMath::Lerp(A, B, t);
            OutTangent = (B - A).GetSafeNormal();
            return true;
        }
    }
    
    return false;
}

float UUnitSteeringComponent::GetDistanceToEndFast(const FVector& Location) const
{
    if (!PathCache.IsValid()) return 0.f;
    return PathCache.TotalLength - GetDistanceAlongPath(Location);
}

float UUnitSteeringComponent::CalculateDynamicLookAhead(float DistanceToEnd) const
{
    if (DistanceToEnd >= BRAKING_DISTANCE)
        return LookAheadDistance;
    
    float T = DistanceToEnd / BRAKING_DISTANCE;
    return FMath::Lerp(MIN_LOOK_AHEAD, LookAheadDistance, T);
}

// ================== توابع کمکی برای فیلتر کردن یونیت‌ها ==================

bool UUnitSteeringComponent::IsUnitMovingRelevantly(const AUnitCharacter* Unit) const
{
    if (!Unit) return false;
    
    EUnitState State = Unit->GetUnitState();
    if (State == EUnitState::Idle || 
        State == EUnitState::Dead || 
        State == EUnitState::Stunned)
    {
        return false;
    }
    
    float Speed = Unit->GetVelocity().Size2D();
    if (Speed < MinMovementSpeedForAvoidance)
    {
        return false;
    }
    
    return true;
}

bool UUnitSteeringComponent::ShouldIgnoreUnit(const AUnitCharacter* OtherUnit, 
                                               const FVector& MyPos, 
                                               const FVector& MyVelocity) const
{
    if (!OtherUnit) return true;
    
    if (!IsUnitMovingRelevantly(OtherUnit))
    {
        return true;
    }
    
    FVector OtherVel = OtherUnit->GetVelocity();
    float OtherSpeed = OtherVel.Size2D();
    float MySpeed = MyVelocity.Size2D();
    
    if (MySpeed > 10.f && OtherSpeed > 10.f)
    {
        FVector MyDir = MyVelocity.GetSafeNormal();
        FVector OtherDir = OtherVel.GetSafeNormal();
        float DotProduct = FVector::DotProduct(MyDir, OtherDir);
        
        if (DotProduct > SameDirectionDotThreshold)
        {
            float SpeedRatio = OtherSpeed / MySpeed;
            if (SpeedRatio > 0.7f && SpeedRatio < 1.3f)
            {
                return true;
            }
        }
    }
    
    return false;
}

// ================== بررسی آزاد بودن مسیر ==================

bool UUnitSteeringComponent::IsPathClear(const FVector& MyPos, const FVector& DesiredDir, float CheckDistance, EEvadeSource Source) const
{
    if (!Owner || DesiredDir.IsNearlyZero()) return false;
    if (Source == EEvadeSource::None) return true;
    
    UWorld* World = GetWorld();
    if (!World) return false;
    
    FVector CheckPoint = MyPos + (DesiredDir * CheckDistance);
    
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Owner);
    Params.bTraceComplex = false;
    
    FCollisionObjectQueryParams ObjectQueryParams;
    bool bCheckStatic = (Source == EEvadeSource::Static || Source == EEvadeSource::Both);
    bool bCheckDynamic = (Source == EEvadeSource::Dynamic || Source == EEvadeSource::Both);
    
    if (bCheckStatic)
    {
        ObjectQueryParams.AddObjectTypesToQuery(ECC_RTS_Obstacle);
        ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel2);
        ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
    }
    if (bCheckDynamic)
    {
        ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel4);
        ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel5);
    }
    
    TArray<FOverlapResult> Overlaps;
    FCollisionShape CheckShape = FCollisionShape::MakeSphere(AvoidanceRadius * 0.8f);
    
    World->OverlapMultiByObjectType(Overlaps, CheckPoint, FQuat::Identity, ObjectQueryParams, CheckShape, Params);
    
    for (const auto& Overlap : Overlaps)
    {
        AActor* HitActor = Overlap.GetActor();
        if (!HitActor || HitActor == Owner) continue;
        
        if (AUnitCharacter* Unit = Cast<AUnitCharacter>(HitActor))
        {
            if (!bCheckDynamic) continue;
            if (Unit->GetUnitState() == EUnitState::Dead) continue;
            return false;
        }
        else
        {
            if (!bCheckStatic) continue;
            return false;
        }
    }
    
    return true;
}

FVector UUnitSteeringComponent::ComputeEvadeOffset(const FVector& DesiredDir, const FVector& ObstacleDir, float Strength,
                                                    float MinDist, float MaxDist, float BlendBase, float BlendRange) const
{
    float EvadeDistance = FMath::Lerp(MinDist, MaxDist, FMath::Clamp(Strength, 0.f, 1.f));
    
    float BlendFactor = BlendBase + (1.0f - Strength) * BlendRange;
    FVector CombinedDir = (DesiredDir * (1.0f - BlendFactor) + ObstacleDir * BlendFactor);
    CombinedDir = CombinedDir.GetSafeNormal();
    
    if (CombinedDir.IsNearlyZero())
    {
        CombinedDir = ObstacleDir;
    }
    
    return CombinedDir * EvadeDistance;
}

// ================== تشخیص موانع ثابت ==================

FStaticObstacleResult UUnitSteeringComponent::CheckStaticObstacles(const FVector& MyPos, const FVector& DesiredDir, float DeltaTime) const
{
    FStaticObstacleResult Result;
    
    if (!Owner || DesiredDir.IsNearlyZero()) return Result;
    
    UWorld* World = GetWorld();
    if (!World) return Result;
    
    const float SCAN_RADIUS = GetStaticScanRadius();
    const float SAFE_DISTANCE = StaticSafeDistance;
    const float DOT_THRESHOLD = GetStaticFOVDotThreshold();
    
    FCollisionObjectQueryParams ObjectQueryParams;
    ObjectQueryParams.AddObjectTypesToQuery(ECC_RTS_Obstacle);
    ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel2);
    ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
    
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Owner);
    Params.bTraceComplex = false;
    
    FCollisionShape ScanShape = FCollisionShape::MakeSphere(SCAN_RADIUS);
    World->OverlapMultiByObjectType(Overlaps, MyPos, FQuat::Identity, ObjectQueryParams, ScanShape, Params);
    
    float BestScore = -1.f;
    FStaticObstacleResult BestResult;
    
    for (const auto& Overlap : Overlaps)
    {
        AActor* Obstacle = Overlap.GetActor();
        if (!Obstacle || Obstacle == Owner) continue;
        if (Obstacle->IsA<AUnitCharacter>()) continue;
        
        FVector ObstaclePos = Obstacle->GetActorLocation();
        FVector ToObstacle = ObstaclePos - MyPos;
        float DistToObstacle = ToObstacle.Size2D();
        
        if (DistToObstacle < 0.1f) continue;
        
        FVector DirToObstacle = ToObstacle / DistToObstacle;
        if (!IsWithinFieldOfView(DesiredDir, DirToObstacle, DOT_THRESHOLD)) continue;
        
        float ObstacleRadius = 50.f;
        UCapsuleComponent* ObstCapsule = Obstacle->FindComponentByClass<UCapsuleComponent>();
        if (ObstCapsule)
        {
            ObstacleRadius = ObstCapsule->GetScaledCapsuleRadius() * 1.3f;
        }
        else
        {
            FBox Bounds = Obstacle->GetComponentsBoundingBox();
            if (Bounds.IsValid)
            {
                FVector Extent = Bounds.GetExtent();
                ObstacleRadius = FMath::Max(Extent.X, Extent.Y) * 1.2f;
            }
        }
        
        float DistanceWeight = 1.0f - (DistToObstacle / SCAN_RADIUS);
        float DirectionWeight = FMath::Max(0.f, FVector::DotProduct(DesiredDir, DirToObstacle));
        float ThreatScore = (DistanceWeight * 0.75f) + (DirectionWeight * 0.25f);
        
        if (DistToObstacle > SCAN_RADIUS * 0.7f)
        {
            ThreatScore *= 0.5f;
        }
        
        if (ThreatScore > BestScore)
        {
            BestScore = ThreatScore;
            BestResult.bHasObstacle = true;
            BestResult.ObstacleLocation = ObstaclePos;
            BestResult.ObstacleRadius = ObstacleRadius;
            BestResult.DistanceToObstacle = DistToObstacle;
            
            FVector RightDir = FVector::CrossProduct(DesiredDir, FVector::UpVector).GetSafeNormal();
            FVector PerpDir1 = RightDir;
            FVector PerpDir2 = -RightDir;
            
            FVector TestPos1 = MyPos + (PerpDir1 * (AvoidanceRadius + ObstacleRadius));
            FVector TestPos2 = MyPos + (PerpDir2 * (AvoidanceRadius + ObstacleRadius));
            
            float DistToObstacle1 = FVector::Dist2D(TestPos1, ObstaclePos);
            float DistToObstacle2 = FVector::Dist2D(TestPos2, ObstaclePos);
            
            BestResult.AvoidDirection = (DistToObstacle1 > DistToObstacle2) ? PerpDir1 : PerpDir2;
            
            // ✅ شدت بر اساس فاصله (نزدیک‌تر = قوی‌تر)
            float DistanceFactor = 1.0f - FMath::Clamp((DistToObstacle - SAFE_DISTANCE) / (SCAN_RADIUS - SAFE_DISTANCE), 0.f, 1.f);
            BestResult.AvoidStrength = FMath::Clamp(DistanceFactor * 0.8f + 0.2f, 0.2f, 1.0f);
            
            if (DistToObstacle < SAFE_DISTANCE * 0.5f)
            {
                BestResult.AvoidStrength = 1.0f;
                BestResult.TimeToCollision = 0.1f;
            }
            else
            {
                float Speed = Owner->GetVelocity().Size2D();
                float TimeToObstacle = (DistToObstacle - SAFE_DISTANCE) / (Speed > 1.f ? Speed : 100.f);
                BestResult.TimeToCollision = FMath::Max(TimeToObstacle * 0.5f, 0.1f);
            }
        }
    }
    
    return BestResult;
}

// ================== تشخیص یونیت‌های متحرک ==================

FDynamicUnitResult UUnitSteeringComponent::CheckDynamicUnits(const FVector& MyPos, 
                                                              const FVector& MyVelocity, 
                                                              const FVector& DesiredDir, 
                                                              float DeltaTime) const
{
    FDynamicUnitResult Result;
    
    if (!Owner || DesiredDir.IsNearlyZero()) return Result;
    
    UWorld* World = GetWorld();
    if (!World) return Result;
    
    const float SCAN_RADIUS = GetDynamicUnitScanRadius();
    const float SAFE_DISTANCE = DynamicSafeDistance;
    const float DOT_THRESHOLD = GetDynamicFOVDotThreshold();
    const float PREDICTION_TIME = DynamicPredictionTime;
    
    FCollisionObjectQueryParams ObjectQueryParams;
    ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel4);
    ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel5);
    
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Owner);
    Params.bTraceComplex = false;
    
    FCollisionShape ScanShape = FCollisionShape::MakeSphere(SCAN_RADIUS);
    World->OverlapMultiByObjectType(Overlaps, MyPos, FQuat::Identity, ObjectQueryParams, ScanShape, Params);
    
    float BestThreat = -1.f;
    FDynamicUnitResult BestResult;
    
    for (const auto& Overlap : Overlaps)
    {
        AUnitCharacter* OtherUnit = Cast<AUnitCharacter>(Overlap.GetActor());
        if (!OtherUnit || OtherUnit == Owner) continue;
        
        EUnitState OtherState = OtherUnit->GetUnitState();
        if (OtherState == EUnitState::Dead) continue;
        
        FVector OtherPos = OtherUnit->GetActorLocation();
        FVector RelativePos = OtherPos - MyPos;
        float Distance = RelativePos.Size2D();
        
        FVector DirToUnit = RelativePos.GetSafeNormal();
        if (!IsWithinFieldOfView(DesiredDir, DirToUnit, DOT_THRESHOLD)) continue;
        
        if (Distance > SCAN_RADIUS * 0.8f) continue;
        
        FVector OtherVel = OtherUnit->GetVelocity();
        float OtherSpeed = OtherVel.Size2D();
        
        bool bIsStationaryState = (OtherState == EUnitState::Idle || OtherState == EUnitState::Stunned);
        bool bIsStuckUnit = false;
        
        if (!bIsStationaryState)
        {
            if (UUnitSteeringComponent* OtherSteering = OtherUnit->FindComponentByClass<UUnitSteeringComponent>())
            {
                bIsStuckUnit = OtherSteering->IsStuck();
            }
        }
        
        bool bIsBlocking = bIsStationaryState || bIsStuckUnit;
        
        if (!bIsBlocking)
        {
            if (ShouldIgnoreUnit(OtherUnit, MyPos, MyVelocity)) continue;
            if (OtherSpeed < 10.f) continue;
        }
        
        if (Distance < SAFE_DISTANCE)
        {
            FDynamicUnitResult ImmediateResult;
            ImmediateResult.bHasUnit = true;
            ImmediateResult.OtherUnit = OtherUnit;
            ImmediateResult.UnitLocation = OtherPos;
            ImmediateResult.UnitVelocity = OtherVel;
            ImmediateResult.DistanceToUnit = Distance;
            ImmediateResult.TimeToCollision = 0.f;
            ImmediateResult.ClosestDistance = Distance;
            
            FVector RightDir = FVector::CrossProduct(DesiredDir, FVector::UpVector).GetSafeNormal();
            float Side = FMath::Sign(FVector::DotProduct(RightDir, DirToUnit));
            ImmediateResult.AvoidDirection = (Side > 0) ? -RightDir : RightDir;
            ImmediateResult.AvoidStrength = 1.0f;
            
            return ImmediateResult;
        }
        
        if (bIsBlocking)
        {
            float DistanceThreat = FMath::Clamp(1.0f - (Distance / SCAN_RADIUS), 0.f, 1.f);
            float ThreatScore = DistanceThreat * 0.75f;
            
            if (ThreatScore > BestThreat)
            {
                BestThreat = ThreatScore;
                BestResult.bHasUnit = true;
                BestResult.OtherUnit = OtherUnit;
                BestResult.UnitLocation = OtherPos;
                BestResult.UnitVelocity = OtherVel;
                BestResult.DistanceToUnit = Distance;
                
                float MySpeed = MyVelocity.Size2D();
                BestResult.TimeToCollision = Distance / (MySpeed > 1.f ? MySpeed : 100.f);
                BestResult.ClosestDistance = Distance;
                
                FVector RightDir = FVector::CrossProduct(DesiredDir, FVector::UpVector).GetSafeNormal();
                float Side = FMath::Sign(FVector::DotProduct(RightDir, DirToUnit));
                BestResult.AvoidDirection = (Side > 0) ? -RightDir : RightDir;
                
                // ✅ شدت بر اساس فاصله
                float DistanceFactor = 1.0f - FMath::Clamp((Distance - SAFE_DISTANCE) / (SCAN_RADIUS - SAFE_DISTANCE), 0.f, 1.f);
                BestResult.AvoidStrength = FMath::Clamp(DistanceFactor * 0.8f + 0.2f, 0.2f, 1.0f);
            }
            continue;
        }
        
        FVector RelativeVel = OtherVel - MyVelocity;
        float RelativeSpeed = RelativeVel.Size();
        
        if (RelativeSpeed > 1.f)
        {
            float T = -FVector::DotProduct(RelativePos, RelativeVel) / (RelativeSpeed * RelativeSpeed);
            T = FMath::Clamp(T, 0.f, PREDICTION_TIME);
            
            FVector MyFuturePos = MyPos + (MyVelocity * T);
            FVector OtherFuturePos = OtherPos + (OtherVel * T);
            float ClosestDist = FVector::Dist2D(MyFuturePos, OtherFuturePos);
            
            if (ClosestDist < SAFE_DISTANCE)
            {
                float DistanceThreat = 1.0f - (ClosestDist / SAFE_DISTANCE);
                float TimeThreat = 1.0f - (T / PREDICTION_TIME);
                float ThreatScore = (DistanceThreat * 0.75f) + (TimeThreat * 0.25f);
                
                if (ThreatScore > BestThreat)
                {
                    BestThreat = ThreatScore;
                    BestResult.bHasUnit = true;
                    BestResult.OtherUnit = OtherUnit;
                    BestResult.UnitLocation = OtherPos;
                    BestResult.UnitVelocity = OtherVel;
                    BestResult.DistanceToUnit = Distance;
                    BestResult.TimeToCollision = T;
                    BestResult.ClosestDistance = ClosestDist;
                    
                    FVector ToOtherAtClosest = (OtherFuturePos - MyFuturePos).GetSafeNormal();
                    FVector RightDir = FVector::CrossProduct(DesiredDir, FVector::UpVector).GetSafeNormal();
                    float Side = FMath::Sign(FVector::DotProduct(RightDir, ToOtherAtClosest));
                    BestResult.AvoidDirection = (Side > 0) ? -RightDir : RightDir;
                    
                    // ✅ شدت بر اساس تهدید
                    BestResult.AvoidStrength = FMath::Clamp(ThreatScore * 0.8f + 0.2f, 0.2f, 1.0f);
                }
            }
        }
    }
    
    return BestResult;
}

// ================== فشرده‌سازی گروه ==================

void UUnitSteeringComponent::UpdateGroupCompression(float DeltaTime, const FVector& CurrentTangent)
{
    if (!Owner || CurrentTangent.IsNearlyZero()) return;

    float MinCompressed = AvoidanceRadius * CompressedOffsetRadiusFactor;

    float TargetOffset = 0.f;
    if (DesiredLateralOffset > 0.f)
    {
        TargetOffset = FMath::Min(DesiredLateralOffset, MinCompressed);
    }
    else if (DesiredLateralOffset < 0.f)
    {
        TargetOffset = FMath::Max(DesiredLateralOffset, -MinCompressed);
    }

    float RemainingDelta = DesiredLateralOffset - TargetOffset;

    if (FMath::IsNearlyZero(RemainingDelta, 4.0f))
    {
        DesiredLateralOffset = TargetOffset;
        return;
    }

    float DirectionSign = (RemainingDelta > 0.f) ? -1.f : 1.f;
    float OffsetChange = DirectionSign * LinearCompressionSpeed * DeltaTime;

    if (FMath::Abs(OffsetChange) >= FMath::Abs(RemainingDelta))
    {
        DesiredLateralOffset = TargetOffset;
    }
    else
    {
        DesiredLateralOffset += OffsetChange;
    }
}

// ================== تابع اصلی اجرای حرکت (اصلاح‌شده) ==================

void UUnitSteeringComponent::ExecuteMovement(float DeltaTime)
{
    if (!Owner) return;
    
    // ========== بررسی وضعیت Stuck ==========
    if (Owner->GetUnitState() == EUnitState::Stuck)
    {
        HandleUnstuckState(DeltaTime);
        return;
    }
    
    // ========== بررسی اعتبار مسیر ==========
    if (!bHasPath || CurrentPath.Num() < 2 || !PathCache.IsValid())
    {
        if (bHasTarget) ClearTarget();
        return;
    }
    
    // ========== مدیریت فریم اول ==========
    TimeSinceLastMoveCommand += DeltaTime;
    if (bIsFirstFrameOfNewPath && TimeSinceLastMoveCommand > FIRST_FRAME_TIME)
    {
        bIsFirstFrameOfNewPath = false;
    }
    
    float OldOffset = DesiredLateralOffset;
    float DistanceToEnd = GetDistanceToEndFast(Owner->GetActorLocation());
    FVector Target;
    FVector MyPos = Owner->GetActorLocation();
    FVector MyVel = Owner->GetVelocity();
    
    // ========== تشخیص گیر کردن ==========
    bool bWasStuck = UpdateStuckDetection(DeltaTime, MyVel.Size2D());
    if (bWasStuck)
    {
        Owner->SetUnitState(EUnitState::Stuck);
        return;
    }
    
    // ========== محاسبه هدف روی مسیر ==========
    if (bIsFirstFrameOfNewPath)
    {
        FVector AbsoluteTangent = (CurrentPath[1] - CurrentPath[0]).GetSafeNormal();
        Target = MyPos + (AbsoluteTangent * LookAheadDistance);
    }
    else
    {
        float LookAhead = CalculateDynamicLookAhead(DistanceToEnd);
        float TargetDistance = GetDistanceAlongPath(MyPos) + LookAhead;
        TargetDistance = FMath::Min(TargetDistance, TotalPathLength);
        
        FVector TargetOnPath, TargetTangent;
        if (GetPointOnPathFast(TargetDistance, TargetOnPath, TargetTangent))
        {
            FVector RightDir = FVector::CrossProduct(TargetTangent, FVector::UpVector).GetSafeNormal();
            Target = TargetOnPath + RightDir * DesiredLateralOffset;
        }
        else
        {
            Target = MyPos;
        }
    }
    
    SetTargetSlot(Target);
    
    // ========== محاسبه جهت مطلوب ==========
    FVector DesiredDir = (Target - MyPos).GetSafeNormal();
    
    // ========== بررسی موانع (هر فریم) ==========
    FStaticObstacleResult StaticResult = CheckStaticObstacles(MyPos, DesiredDir, DeltaTime);
    FDynamicUnitResult DynamicResult = CheckDynamicUnits(MyPos, MyVel, DesiredDir, DeltaTime);
    
    // ========== محاسبه افست فرار (مستمر) ==========
    FVector TotalEvadeOffset = FVector::ZeroVector;
    bool bHasAnyObstacle = false;
    
    // Static
    if (StaticResult.bHasObstacle)
    {
        FVector StaticOffset = ComputeEvadeOffset(DesiredDir, StaticResult.AvoidDirection, StaticResult.AvoidStrength,
                                                   StaticMinEvadeDistance, StaticMaxEvadeDistance,
                                                   StaticEvadeBlendBase, StaticEvadeBlendRange);
        TotalEvadeOffset += StaticOffset;
        bHasAnyObstacle = true;
    }
    
    // Dynamic
    if (DynamicResult.bHasUnit)
    {
        FVector DynamicOffset = ComputeEvadeOffset(DesiredDir, DynamicResult.AvoidDirection, DynamicResult.AvoidStrength,
                                                    DynamicMinEvadeDistance, DynamicMaxEvadeDistance,
                                                    DynamicEvadeBlendBase, DynamicEvadeBlendRange);
        TotalEvadeOffset += DynamicOffset;
        bHasAnyObstacle = true;
    }
    
    // ========== هدف نهایی ==========
    FVector FinalTarget = Target;
    
    if (bHasAnyObstacle)
    {
        FinalTarget = MyPos + TotalEvadeOffset;
        bIsEvading = true;
        ActiveEvadeSource = (StaticResult.bHasObstacle && DynamicResult.bHasUnit) ? EEvadeSource::Both :
                            (StaticResult.bHasObstacle ? EEvadeSource::Static : EEvadeSource::Dynamic);
    }
    else
    {
        bIsEvading = false;
        ActiveEvadeSource = EEvadeSource::None;
    }
    
    // ========== آپدیت گروه و افست ==========
    FVector CurrentTangent = (FinalTarget - MyPos).GetSafeNormal();
    
    if (!bIsEvading)
    {
        UpdateGroupCompression(DeltaTime, CurrentTangent);
    }
    
    float NewOffset = DesiredLateralOffset;
    float OffsetDelta = NewOffset - OldOffset;
    
    if (!bIsFirstFrameOfNewPath && !FMath::IsNearlyZero(OffsetDelta, LATERAL_OFFSET_EPSILON))
    {
        FVector RightDir = FVector::CrossProduct(CurrentTangent, FVector::UpVector).GetSafeNormal();
        FVector NewLocation = MyPos + (RightDir * OffsetDelta);
        Owner->SetActorLocation(NewLocation, true);
    }
    
    // ========== بررسی رسیدن به هدف ==========
    float DistToFinalGoal = FVector::Dist2D(MyPos, TargetSlot);
    if (DistToFinalGoal <= STOP_DISTANCE)
    {
        ClearTarget();
        Owner->SetUnitState(EUnitState::Idle);
        if (Owner->UnitMovement)
        {
            Owner->UnitMovement->StopImmediately();
        }
        return;
    }
    
    // ========== شرط توقف فرار هنگام نزدیکی به هدف ==========
    if (bIsEvading && DistToFinalGoal < AvoidanceRadius * 0.5f)
    {
        bIsEvading = false;
        ActiveEvadeSource = EEvadeSource::None;
        FinalTarget = TargetSlot;
    }
    
    // ========== اعمال حرکت نهایی ==========
    ApplyMovementWithTarget(DeltaTime, FinalTarget);
}

// ================== اعمال حرکت ==================

void UUnitSteeringComponent::ApplyMovementWithTarget(float DeltaTime, const FVector& InTarget)
{
    if (!Owner || !bHasTarget) return;
    
    FVector MyPos = Owner->GetActorLocation();
    FVector ToTarget = InTarget - MyPos;
    float Distance = ToTarget.Size2D();
    
    if (Distance <= STOP_DISTANCE)
    {
        if (bIsEvading)
        {
            bIsEvading = false;
            ActiveEvadeSource = EEvadeSource::None;
            
            if (Owner->UnitMovement)
            {
                Owner->UnitMovement->MoveTo(TargetSlot, false);
            }
        }
        return;
    }
    
    FVector MoveDir = ToTarget.GetSafeNormal2D();
    if (MoveDir.IsNearlyZero()) return;
    
    if (Owner->UnitMovement)
    {
        Owner->UnitMovement->MoveTo(InTarget, false);
    }
}

bool UUnitSteeringComponent::IsMovingTowardGoal() const
{
    if (!Owner || !bHasTarget) return false;
    
    FVector MyPos = Owner->GetActorLocation();
    float DistToGoal = FVector::Dist2D(MyPos, TargetSlot);
    
    if (DistToGoal <= STOP_DISTANCE) return false;
    
    if (Owner->UnitMovement)
    {
        return Owner->UnitMovement->IsMoving();
    }
    
    return false;
}

// ================== توابع اسکن ==================

float UUnitSteeringComponent::GetStaticScanRadius() const
{
    if (!Owner) return StaticMinScanRadius;
    
    float Speed = Owner->GetVelocity().Size2D();
    float Radius = StaticMinScanRadius + (Speed * StaticSpeedToRadiusFactor);
    
    return FMath::Clamp(Radius, StaticMinScanRadius, StaticMaxScanRadius);
}

float UUnitSteeringComponent::GetDynamicUnitScanRadius() const
{
    if (!Owner) return DynamicMinScanRadius;
    
    float Speed = Owner->GetVelocity().Size2D();
    float Radius = DynamicMinScanRadius + (Speed * DynamicSpeedToRadiusFactor);
    
    return FMath::Clamp(Radius, DynamicMinScanRadius, DynamicMaxScanRadius);
}

float UUnitSteeringComponent::GetStaticFOVDotThreshold() const
{
    float HalfAngleRadians = FMath::DegreesToRadians(StaticFieldOfViewAngle * 0.5f);
    return FMath::Cos(HalfAngleRadians);
}

float UUnitSteeringComponent::GetDynamicFOVDotThreshold() const
{
    float HalfAngleRadians = FMath::DegreesToRadians(DynamicFieldOfViewAngle * 0.5f);
    return FMath::Cos(HalfAngleRadians);
}

bool UUnitSteeringComponent::IsWithinFieldOfView(const FVector& DesiredDir, const FVector& ToObstacle, float DotThreshold) const
{
    if (DesiredDir.IsNearlyZero() || ToObstacle.IsNearlyZero()) return false;
    
    float DotProduct = FVector::DotProduct(DesiredDir, ToObstacle.GetSafeNormal());
    return DotProduct >= DotThreshold;
}

// ================== تشخیص و رفع گیر ==================

bool UUnitSteeringComponent::UpdateStuckDetection(float DeltaTime, float CurrentSpeed)
{
    if (!bHasTarget || bIsFirstFrameOfNewPath)
    {
        StuckTimer = 0.f;
        return false;
    }
    
    static FVector LastPosition = FVector::ZeroVector;
    static float LastPositionTime = 0.f;
    
    if (CurrentSpeed < StuckSpeedThreshold)
    {
        StuckTimer += DeltaTime;
        
        if (GetWorld()->GetTimeSeconds() - LastPositionTime > 0.5f)
        {
            FVector CurrentPos = Owner->GetActorLocation();
            float DistanceMoved = FVector::Dist2D(CurrentPos, LastPosition);
            
            if (DistanceMoved < 3.f && StuckTimer > 0.5f)
            {
                return true;
            }
            
            LastPosition = CurrentPos;
            LastPositionTime = GetWorld()->GetTimeSeconds();
        }
        
        if (StuckTimer >= StuckTimeThreshold)
        {
            return true;
        }
    }
    else
    {
        StuckTimer = 0.f;
        LastPosition = Owner->GetActorLocation();
        LastPositionTime = GetWorld()->GetTimeSeconds();
    }
    
    return false;
}

bool UUnitSteeringComponent::IsStuck() const
{
    if (!Owner) return false;
    return Owner->GetUnitState() == EUnitState::Stuck;
}

void UUnitSteeringComponent::HandleUnstuck(float DeltaTime)
{
    if (bIsUnstaking)
    {
        CheckUnstuckSuccess(DeltaTime);
    }
    else
    {
        AttemptUnstuck(DeltaTime);
    }
}

void UUnitSteeringComponent::AttemptUnstuck(float DeltaTime)
{
    if (!bIsUnstaking)
    {
        StartUnstuck();
    }
    else
    {
        CheckUnstuckSuccess(DeltaTime);
    }
}

void UUnitSteeringComponent::StartUnstuck()
{
    if (!Owner || !bHasTarget) return;
    
    FVector MyPos = Owner->GetActorLocation();
    FVector TargetDir = (TargetSlot - MyPos).GetSafeNormal2D();
    
    float Angle = 0.f;
    float Distance = 0.f;
    
    switch (UnstuckAttempts)
    {
    case 0:  Angle = 180.f; Distance = 80.f; break;
    case 1:  Angle = 135.f; Distance = 100.f; break;
    case 2:  Angle = 225.f; Distance = 100.f; break;
    case 3:  Angle = 90.f; Distance = 120.f; break;
    case 4:  Angle = 270.f; Distance = 120.f; break;
    default: Angle = FMath::FRandRange(0.f, 360.f); Distance = 150.f; break;
    }
    
    float CurrentAngle = FMath::Atan2(TargetDir.Y, TargetDir.X);
    float NewAngle = CurrentAngle + FMath::DegreesToRadians(Angle);
    
    UnstuckDirection = FVector(FMath::Cos(NewAngle), FMath::Sin(NewAngle), 0.f);
    UnstuckDirection.Normalize();
    
    UnstuckTarget = MyPos + (UnstuckDirection * Distance);
    UnstuckTarget.Z = MyPos.Z;
    
    SetTargetSlot(UnstuckTarget);
    
    UnstuckTargetRotation = UnstuckDirection.Rotation();
    UnstuckTargetRotation.Pitch = 0.f;
    UnstuckTargetRotation.Roll = 0.f;
    bIsRotatingForUnstuck = true;
    
    bIsUnstaking = true;
    UnstuckStartTime = GetWorld()->GetTimeSeconds();
    UnstuckAttempts++;
    
    bIsEvading = false;
    ActiveEvadeSource = EEvadeSource::None;
    
    UE_LOG(LogTemp, Log, TEXT("Unit %s unstuck attempt %d: Angle=%.0f, Dist=%.0f"), 
           *Owner->GetName(), UnstuckAttempts, Angle, Distance);
}

void UUnitSteeringComponent::HandleUnstuckState(float DeltaTime)
{
    if (!Owner || !bHasTarget) return;
    
    if (bIsRotatingForUnstuck)
    {
        FRotator CurrentRot = Owner->GetActorRotation();
        FRotator NewRot = FMath::RInterpTo(
            CurrentRot, 
            UnstuckTargetRotation, 
            DeltaTime, 
            UnstuckRotationSpeed / 60.f
        );
        Owner->SetActorRotation(NewRot);
        
        if (CurrentRot.Equals(UnstuckTargetRotation, 2.f))
        {
            Owner->SetActorRotation(UnstuckTargetRotation);
            bIsRotatingForUnstuck = false;
        }
        
        return;
    }
    
    FVector MyPos = Owner->GetActorLocation();
    float DistToTarget = FVector::Dist2D(MyPos, TargetSlot);
    
    if (DistToTarget > STOP_DISTANCE)
    {
        ApplyMovementWithTarget(DeltaTime, TargetSlot);
    }
    else
    {
        CheckUnstuckSuccess(DeltaTime);
    }
}

void UUnitSteeringComponent::CheckUnstuckSuccess(float DeltaTime)
{
    if (!bIsUnstaking || !Owner) return;
    
    float ElapsedTime = GetWorld()->GetTimeSeconds() - UnstuckStartTime;
    
    if (ElapsedTime > UnstuckTimeout)
    {
        FVector MyPos = Owner->GetActorLocation();
        float DistToTarget = FVector::Dist2D(MyPos, TargetSlot);
        float CurrentSpeed = Owner->GetVelocity().Size2D();
        
        bool bReachedTarget = DistToTarget < 30.f;
        bool bIsMoving = CurrentSpeed > StuckSpeedThreshold;
        bool bMadeProgress = DistToTarget < UnstuckMoveDistance * 0.5f;
        
        bool bSuccess = bReachedTarget || bIsMoving || bMadeProgress;
        
        if (bSuccess)
        {
            bIsUnstaking = false;
            bIsRotatingForUnstuck = false;
            UnstuckAttempts = 0;
            
            RestorePathAfterUnstuck();
            Owner->SetUnitState(EUnitState::Moving);
            bIsEvading = false;
            
            UE_LOG(LogTemp, Log, TEXT("Unit %s unstuck SUCCESS! (Speed=%.0f, Dist=%.0f)"), 
                   *Owner->GetName(), CurrentSpeed, DistToTarget);
        }
        else
        {
            bIsUnstaking = false;
            bIsRotatingForUnstuck = false;
            
            if (UnstuckAttempts < MaxUnstuckAttempts)
            {
                UE_LOG(LogTemp, Warning, TEXT("Unit %s unstuck attempt %d FAILED, retrying..."), 
                       *Owner->GetName(), UnstuckAttempts);
                StartUnstuck();
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Unit %s COMPLETELY STUCK! Going idle."), *Owner->GetName());
                Owner->SetUnitState(EUnitState::Idle);
                if (Owner->UnitMovement)
                {
                    Owner->UnitMovement->StopImmediately();
                }
                ClearTarget();
                bIsUnstaking = false;
                bIsRotatingForUnstuck = false;
                UnstuckAttempts = 0;
            }
        }
    }
}

void UUnitSteeringComponent::RestorePathAfterUnstuck()
{
    if (!bHasPath || CurrentPath.Num() < 2) return;
    
    FVector MyPos = Owner->GetActorLocation();
    float DistanceToEnd = GetDistanceToEndFast(MyPos);
    
    if (DistanceToEnd > STOP_DISTANCE)
    {
        float LookAhead = CalculateDynamicLookAhead(DistanceToEnd);
        float TargetDistance = GetDistanceAlongPath(MyPos) + LookAhead;
        TargetDistance = FMath::Min(TargetDistance, TotalPathLength);
        
        FVector TargetOnPath, TargetTangent;
        if (GetPointOnPathFast(TargetDistance, TargetOnPath, TargetTangent))
        {
            FVector RightDir = FVector::CrossProduct(TargetTangent, FVector::UpVector).GetSafeNormal();
            TargetSlot = TargetOnPath + RightDir * DesiredLateralOffset;
            bHasTarget = true;
            
            if (Owner->UnitMovement)
            {
                Owner->UnitMovement->MoveTo(TargetSlot, false);
            }
            
            FRotator TargetRot = (TargetSlot - MyPos).Rotation();
            TargetRot.Pitch = 0.f;
            TargetRot.Roll = 0.f;
            Owner->SetActorRotation(TargetRot);
        }
    }
}