#include "AI/UUnitSteeringComponent.h"
#include "Characters/AUnitCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

UUnitSteeringComponent::UUnitSteeringComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // نیازی به تیک مجزا ندارد، توسط کاراکتر مدیریت می‌شود
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

FVector UUnitSteeringComponent::GetGroupCompressionForce() const
{
    if (!bHasGroupCenter || !Owner) return FVector::ZeroVector;
    FVector MyPos = Owner->GetActorLocation();
    FVector RightDir = FVector::CrossProduct(GroupForward, FVector::UpVector).GetSafeNormal();
    FVector ToCenter = GroupCenter - MyPos;
    float LateralDist = FVector::DotProduct(ToCenter, RightDir);
    float AbsLateral = FMath::Abs(LateralDist);
    if (AbsLateral <= CompressionRadius)
        return FVector::ZeroVector;
    float Excess = AbsLateral - CompressionRadius;
    float Strength = FMath::Clamp(Excess / CompressionRadius, 0.f, 1.f) * CompressionStrength;
    float DirectionSign = (LateralDist > 0.f) ? -1.f : 1.f;
    return RightDir * DirectionSign * Strength;
}

void UUnitSteeringComponent::ComputeEvadeDirection()
{
    if (!Owner || !bHasTarget) return;

    UWorld* World = GetWorld();
    if (!World) return;

    FVector MyPos = Owner->GetActorLocation();
    FVector DesiredDir = (TargetSlot - MyPos).GetSafeNormal();
    if (DesiredDir.IsNearlyZero()) return;

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Owner);
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(AvoidanceRadius);
    World->OverlapMultiByChannel(Overlaps, MyPos, FQuat::Identity, ECC_Pawn, SphereShape, Params);

    FVector SumAvoidDir = FVector::ZeroVector;
    int32 Count = 0;

    for (const auto& Overlap : Overlaps)
    {
        AUnitCharacter* Other = Cast<AUnitCharacter>(Overlap.GetActor());
        if (!Other || Other == Owner) continue;
        
        EUnitState OtherState = Other->GetUnitState();
        if (OtherState == EUnitState::Dead || OtherState == EUnitState::Idle)
            continue;

        FVector ToOther = Other->GetActorLocation() - MyPos;
        float Dist = ToOther.Size();
        if (Dist < 0.1f) continue;
        
        FVector DirToOther = ToOther / Dist;
        float Dot = FVector::DotProduct(DesiredDir, DirToOther);
        float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(Dot));
        if (AngleDeg > AvoidanceAngle * 0.5f) continue;

        // هرچقدر نزدیک‌تر، نیروی فرار شدیدتر
        float Intensity = FMath::Clamp(1.0f - (Dist / AvoidanceRadius), 0.3f, 1.0f);
        FVector RightDir = FVector::CrossProduct(DesiredDir, FVector::UpVector).GetSafeNormal();
        float Side = FMath::Sign(FVector::DotProduct(RightDir, DirToOther));
        FVector EvadeDir = (Side > 0) ? -RightDir : RightDir;
        SumAvoidDir += EvadeDir * Intensity;
        Count++;
    }

    if (Count > 0)
    {
        EvadeDirection = SumAvoidDir.GetSafeNormal();
        bEvading = true;
        EvadeTimer = EvadeTime;
    }
    else
    {
        // در صورت عدم وجود مانع، به مرور تایمر فرار کم می‌شود تا جهش ناگهانی رخ ندهد
        if (EvadeTimer > 0.f)
        {
            EvadeTimer -= GetWorld()->GetDeltaSeconds();
        }
        else
        {
            bEvading = false;
            EvadeDirection = FVector::ZeroVector;
        }
    }
}

void UUnitSteeringComponent::SetTargetSlot(const FVector& InSlotLocation)
{
    TargetSlot = InSlotLocation;
    bHasTarget = true;
}

void UUnitSteeringComponent::UpdateGroupCompression(float DeltaTime, const FVector& CurrentTangent)
{
    if (!Owner || CurrentTangent.IsNearlyZero()) return;

    if (FMath::IsNearlyZero(DesiredLateralOffset, 4.0f))
    {
        DesiredLateralOffset = 0.f;
        return;
    }

    UWorld* World = GetWorld();
    if (World)
    {
        FVector MyPos = Owner->GetActorLocation();
        FVector RightDir = FVector::CrossProduct(CurrentTangent, FVector::UpVector).GetSafeNormal();
        float ToCenterSign = (DesiredLateralOffset > 0.f) ? -1.f : 1.f;
        FVector DirectionToCenter = RightDir * ToCenterSign;

        float MyCapsuleRadius = 35.f;
        if (Owner->GetCapsuleComponent())
        {
            MyCapsuleRadius = Owner->GetCapsuleComponent()->GetScaledCapsuleRadius();
        }

        float CheckDistance = MyCapsuleRadius + 2.0f; 
        FVector SensorPoint = MyPos + (DirectionToCenter * CheckDistance);

        TArray<FOverlapResult> CenterOverlaps;
        FCollisionQueryParams SeparationParams;
        SeparationParams.AddIgnoredActor(Owner);
        
        float ScanSphereRadius = MyCapsuleRadius * 0.55f;
        FCollisionShape CenterScanSphere = FCollisionShape::MakeSphere(ScanSphereRadius);
        
        bool bIsCenterBlocked = false;
        if (World->OverlapMultiByChannel(CenterOverlaps, SensorPoint, FQuat::Identity, ECC_Pawn, CenterScanSphere, SeparationParams))
        {
            for (const auto& Overlap : CenterOverlaps)
            {
                AUnitCharacter* OtherUnit = Cast<AUnitCharacter>(Overlap.GetActor());
                if (OtherUnit && OtherUnit != Owner)
                {
                    if (OtherUnit->GetUnitState() == EUnitState::Moving_Cluster)
                    {
                        bIsCenterBlocked = true;
                        break; 
                    }
                }
            }
        }

        if (bIsCenterBlocked) 
        {
            return; 
        }
    }

    const float LinearCompressionSpeed = 40.f; 
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

FVector UUnitSteeringComponent::ApplyEvadeSubsystem(const FVector& DynamicTargetSlot, const FVector& CurrentTangent)
{
    // این تابع به دلیل انتقال کل منطق فرار به صورت زنده به کامپوننت استیرینگ، اکنون صرفاً بک‌آپ است.
    return DynamicTargetSlot;
}

void UUnitSteeringComponent::UpdateSteering(float DeltaTime)
{
    if (!Owner || !bHasTarget) return;
    
    FVector MyPos = Owner->GetActorLocation();
    DistanceToGoal = FVector::Dist2D(MyPos, TargetSlot);

    const float ActualStopDistance = 20.f; 
    if (DistanceToGoal <= ActualStopDistance)
    {
        ClearTarget();
        Owner->SetUnitState(EUnitState::Idle);
        return;
    }

    // ================== حل قطعی باگ سرعت فشرده‌سازی ==================
    // جهت حرکت فیزیکی را "فقط" به سمت جلو (راستای اسلات اصلی) تنظیم میکنیم
    // این کار باعث می‌شود حرکت رو به جلوی کاراکتر ۱۰۰٪ ثابت و بدون افت سرعت بماند
    FVector MoveDir = (TargetSlot - MyPos).GetSafeNormal();

    // سیستم فرار متحرک (چون اضطراری است کمی زاویه را تغییر می‌دهد)
    if (bEvading && !EvadeDirection.IsNearlyZero())
    {
        MoveDir = (MoveDir * 0.7f + EvadeDirection * 0.3f).GetSafeNormal();
    }

    // حذف نوسانات ریز بصری
    float SmoothFactor = (DistanceToGoal < 60.f) ? 45.f : 35.f;
    if (CurrentDirection.IsNearlyZero())
        CurrentDirection = MoveDir;
    else
        CurrentDirection = FMath::VInterpTo(CurrentDirection, MoveDir, DeltaTime, SmoothFactor);
    
    CurrentDirection = CurrentDirection.GetSafeNormal();

    // تزریق سرعت خالص رو به جلو بدون دخالت دادن نرخ فشرده‌سازی عرضی
    float CharacterMaxSpeed = Owner->GetCharacterMovement()->MaxWalkSpeed;
    Owner->GetCharacterMovement()->Velocity = CurrentDirection * CharacterMaxSpeed;
    
    // چرخش نرم مش کاراکتر
    if (Owner->GetCharacterMovement()->Velocity.Size2D() > 20.f)
    {
        FRotator TargetRot = Owner->GetCharacterMovement()->Velocity.Rotation();
        TargetRot.Pitch = 0.f; 
        TargetRot.Roll = 0.f;
        Owner->SetActorRotation(FMath::RInterpTo(Owner->GetActorRotation(), TargetRot, DeltaTime, 22.f));
    }
}



