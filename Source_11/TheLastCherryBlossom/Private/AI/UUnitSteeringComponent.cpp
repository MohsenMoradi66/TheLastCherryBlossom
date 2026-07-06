#include "AI/UUnitSteeringComponent.h"
#include "AI/UUnitMovementComponent.h"
#include "Characters/AUnitCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "../TheLastCherryBlossom.h" 
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

UUnitSteeringComponent::UUnitSteeringComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    CurrentDirection = FVector::ForwardVector;
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
    bEvading = false;
    if (Owner && Owner->GetCharacterMovement())
    {
        Owner->GetCharacterMovement()->StopMovementImmediately();
        Owner->GetCharacterMovement()->Velocity = FVector::ZeroVector;
    }
}

void UUnitSteeringComponent::ResetDirection()
{
    CurrentDirection = FVector::ZeroVector;
    bEvading = false;
    EvadeTimer = 0.f;
    EvadeDirection = FVector::ZeroVector;
}

void UUnitSteeringComponent::ClearPath()
{
    // پاک کردن مسیر
    bHasPath = false;
    CurrentPath.Empty();
    TotalPathLength = 0.f;
    PathCache.Reset();
    
    // پاک کردن هدف
    bHasTarget = false;
    TargetSlot = FVector::ZeroVector;
    
    // پاک کردن وضعیت فرار
    bEvading = false;
    EvadeTimer = 0.f;
    EvadeDirection = FVector::ZeroVector;
    
    // پاک کردن جهت
    CurrentDirection = FVector::ZeroVector;
    
    // پاک کردن پارامترهای گروه
    bHasGroupCenter = false;
    GroupCenter = FVector::ZeroVector;
    GroupForward = FVector::ForwardVector;
    
    // ریست آفست
    DesiredLateralOffset = 0.f;
    
    // ریست پرچم‌های فریم اول
    bIsFirstFrameOfNewPath = false;
    TimeSinceLastMoveCommand = 0.f;
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
    
    // جستجوی خطی ساده (چون تعداد قطعات معمولاً کمه)
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

// ================== تنظیم مسیر ==================

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
    
    // جایگزین کردن بخش انتهایی تابع SetPath
    if (Owner->GetCharacterMovement() && CurrentPath.Num() >= 2)
    {
        FVector BaseTangent = (CurrentPath[1] - CurrentPath[0]).GetSafeNormal();
        FVector MoveDir = bWasAlreadyMoving ? SavedDirection : BaseTangent;
    
        // 🌟 اصلاح شد: استفاده مستقیم از MaxWalkSpeed به جای متغیر کمکی کاراکتر
        float ActualMaxSpeed = Owner->GetCharacterMovement()->MaxWalkSpeed;
        Owner->GetCharacterMovement()->Velocity = MoveDir * ActualMaxSpeed;

        ResetDirection();
        bIsFirstFrameOfNewPath = true;
        TimeSinceLastMoveCommand = 0.f;

    }
}

// ================== تابع اصلی حرکت ==================

void UUnitSteeringComponent::ExecuteMovement(float DeltaTime)
{
    if (!Owner) return;
    if (!bHasPath || CurrentPath.Num() < 2 || !PathCache.IsValid())
    {
        if (bHasTarget) ClearTarget();
        return;
    }
    
    TimeSinceLastMoveCommand += DeltaTime;
    if (bIsFirstFrameOfNewPath && TimeSinceLastMoveCommand > FIRST_FRAME_TIME)
    {
        bIsFirstFrameOfNewPath = false;
    }
    
    float OldOffset = DesiredLateralOffset;
    float DistanceToEnd = GetDistanceToEndFast(Owner->GetActorLocation());
    FVector Target;
    
    if (bIsFirstFrameOfNewPath)
    {
        FVector AbsoluteTangent = (CurrentPath[1] - CurrentPath[0]).GetSafeNormal();
        Target = Owner->GetActorLocation() + (AbsoluteTangent * LookAheadDistance);
    }
    else
    {
        float LookAhead = CalculateDynamicLookAhead(DistanceToEnd);
        float TargetDistance = GetDistanceAlongPath(Owner->GetActorLocation()) + LookAhead;
        TargetDistance = FMath::Min(TargetDistance, TotalPathLength);
        
        FVector TargetOnPath, TargetTangent;
        if (GetPointOnPathFast(TargetDistance, TargetOnPath, TargetTangent))
        {
            FVector RightDir = FVector::CrossProduct(TargetTangent, FVector::UpVector).GetSafeNormal();
            Target = TargetOnPath + RightDir * DesiredLateralOffset;
        }
        else
        {
            Target = Owner->GetActorLocation();
        }
    }
    
    SetTargetSlot(Target);
    
    // ✅ بهبود 1: محاسبه فرار با هدف اصلی (نه Target موقت)
    ComputeEvadeDirection();
    
    // ✅ بهبود 2: اگر در حالت فرار هستیم، Target را اصلاح کن
    FVector FinalTarget = Target;
    FVector MyPos = Owner->GetActorLocation();
    
    if (bEvading && !EvadeDirection.IsNearlyZero())
    {
        // ترکیب جهت اصلی با جهت فرار
        FVector DesiredDir = (Target - MyPos).GetSafeNormal();
        FVector CombinedDir = (DesiredDir * 0.6f + EvadeDirection * 0.4f);
        CombinedDir = CombinedDir.GetSafeNormal();
        
        // هدف موقت برای فرار (در مسیر ترکیبی)
        float EvadeDistance = FMath::Min(
            FVector::Dist2D(MyPos, Target) * 0.5f, // نصف فاصله تا هدف
            300.f // حداکثر 300 واحد
        );
        FinalTarget = MyPos + (CombinedDir * EvadeDistance);
    }
    
    FVector CurrentTangent = (FinalTarget - MyPos).GetSafeNormal();
    
    if (!bEvading)
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
    
    // ✅ بهبود 3: بررسی رسیدن به هدف نهایی (نه Target موقت)
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
    
    // ✅ بهبود 4: اگر در حالت فرار هستیم و به هدف نزدیک شدیم، فرار را متوقف کن
    if (bEvading && DistToFinalGoal < AvoidanceRadius * 0.5f)
    {
        bEvading = false;
        EvadeDirection = FVector::ZeroVector;
        EvadeTimer = 0.f;
    }
    
    // ✅ بهبود 5: اعمال حرکت با هدف اصلاح شده
    ApplyMovementWithTarget(DeltaTime, FinalTarget);
}

void UUnitSteeringComponent::ApplyMovementWithTarget(float DeltaTime, const FVector& InTarget)
{
    if (!Owner || !bHasTarget) return;
    
    FVector MyPos = Owner->GetActorLocation();
    FVector ToTarget = InTarget - MyPos;
    float Distance = ToTarget.Size2D();
    
    if (Distance <= STOP_DISTANCE)
    {
        if (bEvading)
        {
            bEvading = false;
            EvadeDirection = FVector::ZeroVector;
            EvadeTimer = 0.f;
            
            if (Owner->UnitMovement)
            {
                Owner->UnitMovement->MoveTo(TargetSlot, false); // ✅ چرخش نرم
            }
        }
        return;
    }
    
    FVector MoveDir = ToTarget.GetSafeNormal2D();
    if (MoveDir.IsNearlyZero()) return;
    
    if (Owner->UnitMovement)
    {
        if (bEvading)
        {
            Owner->UnitMovement->MoveTo(InTarget, false); // ✅ چرخش نرم
        }
        else
        {
            Owner->UnitMovement->MoveTo(TargetSlot, false); // ✅ چرخش نرم
        }
        
        // ✅ دیگر چرخش فوری نمی‌کنیم - بگذار UnitMovement خودش مدیریت کند
    }
}

bool UUnitSteeringComponent::IsMovingTowardGoal() const
{
    if (!Owner || !bHasTarget) return false;
    
    FVector MyPos = Owner->GetActorLocation();
    float DistToGoal = FVector::Dist2D(MyPos, TargetSlot);
    
    // اگر فاصله تا هدف کمتر از 10 واحد است، به هدف رسیده
    if (DistToGoal <= STOP_DISTANCE) return false;
    
    // بررسی کنید که آیا سرعت کافی برای حرکت وجود دارد
    if (Owner->UnitMovement)
    {
        return Owner->UnitMovement->IsMoving();
    }
    
    return false;
}

// ================== توابع کمکی ==================

float UUnitSteeringComponent::CalculateDynamicLookAhead(float DistanceToEnd) const
{
    if (DistanceToEnd >= BRAKING_DISTANCE)
        return LookAheadDistance;
    
    float T = DistanceToEnd / BRAKING_DISTANCE;
    return FMath::Lerp(MIN_LOOK_AHEAD, LookAheadDistance, T);
}

// ================== فرار از برخورد ==================

void UUnitSteeringComponent::ComputeEvadeDirection()
{
    if (!Owner || !bHasTarget) return;

    UWorld* World = GetWorld();
    if (!World) return;

    FVector MyPos = Owner->GetActorLocation();
    FVector DesiredDir = (TargetSlot - MyPos).GetSafeNormal();
    if (DesiredDir.IsNearlyZero()) return;

    // ۱. تنظیم دقیق کانال‌ها: اسکن موانع ثابت (کانال ۲)، موانع اختصاصی و فقط یونیت‌های متحرک
    FCollisionObjectQueryParams ObjectQueryParams;
    ObjectQueryParams.AddObjectTypesToQuery(ECC_RTS_Obstacle);     // موانع اختصاصی
    ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel2); // 🌟 اضافه شد: ساختمان‌ها و درختان
    ObjectQueryParams.AddObjectTypesToQuery(ECC_RTS_MovingUnit);   // 🌟 فقط یونیت‌های در حال حرکت

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Owner);
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(AvoidanceRadius);
    
    World->OverlapMultiByObjectType(Overlaps, MyPos, FQuat::Identity, ObjectQueryParams, SphereShape, Params);
    FVector SumAvoidDir = FVector::ZeroVector;
    int32 Count = 0;

    for (const auto& Overlap : Overlaps)
    {
        AActor* OtherActor = Overlap.GetActor();
        if (!OtherActor || OtherActor == Owner) continue;
        
        // ۲. فیلتر کردن سخت‌گیرانه‌ی کاراکترها
        AUnitCharacter* OtherUnit = Cast<AUnitCharacter>(OtherActor);
        if (OtherUnit)
        {
            // 🌟 یونیت‌های مرده، شوکه شده یا ایستاده (Idle) باید کاملاً نادیده گرفته شوند
            if (OtherUnit->GetUnitState() == EUnitState::Dead || 
                OtherUnit->GetUnitState() == EUnitState::Stunned || 
                OtherUnit->GetUnitState() == EUnitState::Idle)
            {
                continue;
            }
        }

        FVector ToOther = OtherActor->GetActorLocation() - MyPos;
        float Dist = ToOther.Size();
        if (Dist < 0.1f) continue;
        
        FVector DirToOther = ToOther / Dist;
        float Dot = FVector::DotProduct(DesiredDir, DirToOther);
        float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(Dot));
        
        // فقط به موانعی که در زاویه دید جلوی کاراکتر هستند واکنش نشان بده
        if (AngleDeg > AvoidanceAngle * 0.5f) continue;

        // محاسبه شدت فرار بر اساس فاصله (هر چه نزدیک‌تر، فرمان فرار شدیدتر)
        float Intensity = FMath::Clamp(1.0f - (Dist / AvoidanceRadius), 0.3f, 1.0f);
        FVector RightDir = FVector::CrossProduct(DesiredDir, FVector::UpVector).GetSafeNormal();
        float Side = FMath::Sign(FVector::DotProduct(RightDir, DirToOther));
        
        // محاسبه بردار فرار به سمت چپ یا راست مانع
        FVector EvadeDir = (Side > 0) ? -RightDir : RightDir;
        SumAvoidDir += EvadeDir * Intensity;
        Count++;
    }

    // ۳. اعمال بردار فرار نهایی
    if (Count > 0)
    {
        EvadeDirection = SumAvoidDir.GetSafeNormal();
        bEvading = true;
        EvadeTimer = EvadeTime;
    }
    else
    {
        if (EvadeTimer > 0.f)
        {
            EvadeTimer -= World->GetDeltaSeconds();
        }
        else
        {
            bEvading = false;
            EvadeDirection = FVector::ZeroVector;
        }
    }
}

// ================== فشرده‌سازی گروهی ==================

void UUnitSteeringComponent::UpdateGroupCompression(float DeltaTime, const FVector& CurrentTangent)
{
    if (!Owner || CurrentTangent.IsNearlyZero()) return;

    if (FMath::IsNearlyZero(DesiredLateralOffset, 4.0f))
    {
        DesiredLateralOffset = 0.f;
        return;
    }

    float DirectionSign = (DesiredLateralOffset > 0.f) ? -1.f : 1.f;
    float OffsetChange = DirectionSign * LinearCompressionSpeed * DeltaTime;

    if (FMath::Abs(OffsetChange) >= FMath::Abs(DesiredLateralOffset))
    {
        DesiredLateralOffset = 0.f;
    }
    else
    {
        DesiredLateralOffset += OffsetChange;
    }
}

// ================== اعمال حرکت ==================

void UUnitSteeringComponent::UpdateRotation(float DeltaTime)
{
    if (Owner->GetCharacterMovement()->Velocity.Size2D() > 20.f)
    {
        FRotator TargetRot = Owner->GetCharacterMovement()->Velocity.Rotation();
        TargetRot.Pitch = 0.f;
        TargetRot.Roll = 0.f;
        Owner->SetActorRotation(FMath::RInterpTo(Owner->GetActorRotation(), TargetRot, DeltaTime, 22.f));
    }
}

void UUnitSteeringComponent::ForceStop()
{
    // توقف کامل فیزیکی
    if (Owner && Owner->GetCharacterMovement())
    {
        Owner->GetCharacterMovement()->StopMovementImmediately();
        Owner->GetCharacterMovement()->Velocity = FVector::ZeroVector;
    }
    
    // پاک کردن همه چیز
    ClearPath();
    ClearTarget();
    ResetDirection();
    ClearGroupParams();
    DesiredLateralOffset = 0.f;
    bIsFirstFrameOfNewPath = false;
    TimeSinceLastMoveCommand = 0.f;
}