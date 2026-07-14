// UUnitMovementComponent.cpp
#include "../TheLastCherryBlossom.h"
#include "AI/UUnitMovementComponent.h"
#include "Characters/AUnitCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

UUnitMovementComponent::UUnitMovementComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UUnitMovementComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UUnitMovementComponent::Initialize(AUnitCharacter* InOwner)
{
    Owner = InOwner;
    if (!Owner)
    {
        return;
    }

    // ============================
    // Disable CharacterMovement
    // ============================
    if (UCharacterMovementComponent* MoveComp = Owner->GetCharacterMovement())
    {
        MoveComp->StopMovementImmediately();

        MoveComp->Deactivate();
        MoveComp->SetComponentTickEnabled(false);

        MoveComp->Velocity = FVector::ZeroVector;

        MoveComp->bOrientRotationToMovement = false;
        MoveComp->bUseControllerDesiredRotation = false;

        MoveComp->SetPlaneConstraintEnabled(true);
        MoveComp->SetPlaneConstraintNormal(FVector::UpVector);
    }

    // ============================
    // Capsule
    // ============================
    if (UCapsuleComponent* Capsule = Owner->GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Capsule->SetSimulatePhysics(false);
    }

    // ============================
    // Reset Movement
    // ============================
    TargetLocation = Owner->GetActorLocation();

    CurrentVelocity = FVector::ZeroVector;
    DesiredDirection = FVector::ZeroVector;

    bIsMoving = false;
    bMovementLocked = false;

    bHasTargetRotation = false;
    bUseSmoothRotation = true;
}

void UUnitMovementComponent::SetMaxSpeed(float NewSpeed)
{
    MaxSpeed = FMath::Max(NewSpeed, 10.f);
}

void UUnitMovementComponent::StopImmediately()
{
    bIsMoving = false;
    CurrentVelocity = FVector::ZeroVector;
    TargetLocation = Owner->GetActorLocation();
    bHasTargetRotation = false;
}

void UUnitMovementComponent::SetRotationInstant(const FRotator& NewRotation)
{
    if (bMovementLocked) return;
    Owner->SetActorRotation(NewRotation);
    bHasTargetRotation = false;
    bUseSmoothRotation = false;
}

void UUnitMovementComponent::SetRotationSmooth(const FRotator& InTargetRotation, float InInterpSpeed)
{
    if (bMovementLocked) return;
    this->TargetRotation = InTargetRotation;
    this->RotationInterpSpeed = InInterpSpeed;
    bHasTargetRotation = true;
    bUseSmoothRotation = true;
}

void UUnitMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!Owner || bMovementLocked) return;

    // ۱. اعمال حرکت
    ApplyMovement(DeltaTime);

    // ۲. اعمال چرخش
    ApplyRotation(DeltaTime);

}

void UUnitMovementComponent::ApplyMovement(float DeltaTime)
{
    if (!Owner || !bIsMoving)
    {
        return;
    }

    FVector CurrentLocation = Owner->GetActorLocation();

    FVector Goal = TargetLocation;
    Goal.Z = CurrentLocation.Z;

    FVector ToGoal = Goal - CurrentLocation;
    float Distance = ToGoal.Size2D();

    // رسیدن به مقصد
    if (Distance <= STOP_DISTANCE)
    {
        StopImmediately();
        Owner->SetUnitState(EUnitState::Idle);
        return;
    }

    FVector MoveDir = ToGoal.GetSafeNormal2D();

    if (MoveDir.IsNearlyZero())
    {
        StopImmediately();
        return;
    }

    // سرعت ثابت
    CurrentVelocity = MoveDir * MaxSpeed;

    // مقدار جابه‌جایی این فریم
    float MoveDistance = MaxSpeed * DeltaTime;

    // جلوگیری از رد شدن از مقصد
    MoveDistance = FMath::Min(MoveDistance, Distance);

    FVector DeltaMove = MoveDir * MoveDistance;

    FHitResult Hit;
    Hit.Reset();

    Owner->AddActorWorldOffset(
        DeltaMove,
        true,
        &Hit
    );

    if (Hit.bBlockingHit)
    {
        CurrentVelocity = FVector::ZeroVector;
        return;
    }
}

void UUnitMovementComponent::ApplyRotation(float DeltaTime)
{
    if (!Owner) return;
    
    FVector CurrentVel = CurrentVelocity;
    float Speed = CurrentVel.Size2D();
    
    // اگر حرکت نمی‌کند، چرخش را نادیده بگیر
    if (Speed < 10.f && !bHasTargetRotation)
    {
        return;
    }
    
    FRotator TargetRot;
    bool bShouldRotate = false;
    
    // ۱. اگر چرخش هدف داریم (SetRotationSmooth صدا زده شده)
    if (bHasTargetRotation)
    {
        TargetRot = TargetRotation;
        bShouldRotate = true;
        
        // اگر به چرخش هدف رسیدیم
        if (Owner->GetActorRotation().Equals(TargetRot, 0.5f))
        {
            bHasTargetRotation = false;
            bUseSmoothRotation = false;
            return;
        }
    }
    // ۲. چرخش به سمت حرکت (حالت عادی)
    else if (bIsMoving && Speed > 10.f)
    {
        TargetRot = CurrentVelocity.Rotation();
        TargetRot.Pitch = 0.f;
        TargetRot.Roll = 0.f;
        bShouldRotate = true;
    }
    else
    {
        return;
    }
    
    if (!bShouldRotate) return;
    
    // 🌟 اعمال چرخش نرم با RInterpTo
    if (bUseSmoothRotation)
    {
        FRotator CurrentRot = Owner->GetActorRotation();
        FRotator NewRot = FMath::RInterpTo(
            CurrentRot, 
            TargetRot, 
            DeltaTime, 
            RotationInterpSpeed
        );
        Owner->SetActorRotation(NewRot);
        
        if (NewRot.Equals(TargetRot, 1.f))
        {
            Owner->SetActorRotation(TargetRot);
            if (bHasTargetRotation)
            {
                bHasTargetRotation = false;
            }
        }
    }
    else
    {
        // چرخش فوری
        Owner->SetActorRotation(TargetRot);
        if (bHasTargetRotation)
        {
            bHasTargetRotation = false;
        }
    }
}

void UUnitMovementComponent::MoveTo(const FVector& InTargetLocation, bool bInInstantRotation)
{
    if (bMovementLocked) return;

    this->TargetLocation = InTargetLocation;
    this->bInstantRotation = bInInstantRotation;
    this->bIsMoving = true;
    
    if (bInInstantRotation)
    {
        bUseSmoothRotation = false;
    }
    else
    {
        bUseSmoothRotation = true;
    }
    
    FVector Direction = (TargetLocation - Owner->GetActorLocation());
    Direction.Z = 0.f;
    if (!Direction.IsNearlyZero())
    {
        Direction.Normalize();
        DesiredDirection = Direction;
        
        if (bInInstantRotation)
        {
            Owner->SetActorRotation(Direction.Rotation());
        }
        else
        {
            TargetRotation = Direction.Rotation();
            bHasTargetRotation = true;
            bUseSmoothRotation = true;
        }
    }
}

void UUnitMovementComponent::MoveDirection(const FVector& Direction)
{
    if (bMovementLocked) return;

    FVector NormalizedDir = Direction.GetSafeNormal2D();
    if (NormalizedDir.IsNearlyZero()) return;

    bIsMoving = true;
    DesiredDirection = NormalizedDir;
    TargetLocation = Owner->GetActorLocation() + (NormalizedDir * 500.f);

    bUseSmoothRotation = true;
    TargetRotation = NormalizedDir.Rotation();
    bHasTargetRotation = true;
}

void UUnitMovementComponent::SetRotationInterpSpeed(float NewSpeed)
{
    RotationInterpSpeed = FMath::Max(NewSpeed, 1.f);
}

void UUnitMovementComponent::SetLocationDirect(const FVector& NewLocation)
{
    if (!Owner || bMovementLocked) return;
    Owner->SetActorLocation(NewLocation, false);
}