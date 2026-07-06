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
    if (!Owner) return;

    // غیرفعال کردن کامل CharacterMovementComponent
    if (UCharacterMovementComponent* MoveComp = Owner->GetCharacterMovement())
    {
        MoveComp->Deactivate();
        MoveComp->SetComponentTickEnabled(false);
        MoveComp->Velocity = FVector::ZeroVector;
    }

    // تنظیمات کپسول برای حرکت فیزیکی
    if (UCapsuleComponent* Capsule = Owner->GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Capsule->SetSimulatePhysics(false);
    }

    TargetLocation = Owner->GetActorLocation();
    CurrentVelocity = FVector::ZeroVector;
    bIsMoving = false;
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

    // ۳. بروزرسانی سرعت در Owner (برای نمایش)
    if (bIsMoving && !CurrentVelocity.IsNearlyZero())
    {
        Owner->CurrentSpeed = MaxSpeed;
    }
    else
    {
        Owner->CurrentSpeed = 0.f;
    }
}

void UUnitMovementComponent::ApplyMovement(float DeltaTime)
{
    if (!bIsMoving) return;

    FVector CurrentLocation = Owner->GetActorLocation();
    FVector Target = TargetLocation;
    Target.Z = CurrentLocation.Z;

    FVector ToTarget = Target - CurrentLocation;
    float Distance = ToTarget.Size2D();

    if (Distance <= STOP_DISTANCE)
    {
        StopImmediately();
        Owner->SetUnitState(EUnitState::Idle);
        return;
    }

    FVector MoveDir = ToTarget.GetSafeNormal2D();
    if (MoveDir.IsNearlyZero())
    {
        StopImmediately();
        return;
    }

    float Speed = MaxSpeed;
    CurrentVelocity = MoveDir * Speed;
    FVector NewLocation = CurrentLocation + (CurrentVelocity * DeltaTime);

    // ✅ اصلاح: با Sweep=true حرکت کن
    FHitResult Hit;
    bool bMoved = Owner->SetActorLocation(NewLocation, true, &Hit);  // true = Sweep

    // اگر برخورد داشت، متوقف شو
    if (!bMoved && Hit.bBlockingHit)
    {
        // به جای اینکه به داخل یونیت دیگر برو، در جای خود بمان
        // Steering خودش مسیر جدید پیدا می‌کند
        CurrentVelocity = FVector::ZeroVector;
        
        // اگر خیلی نزدیک هستیم، کمی عقب‌تر برو
        if (Distance < 30.f)
        {
            FVector BackDir = -MoveDir;
            FVector BackLocation = CurrentLocation + (BackDir * 10.f);
            Owner->SetActorLocation(BackLocation, true);
        }
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