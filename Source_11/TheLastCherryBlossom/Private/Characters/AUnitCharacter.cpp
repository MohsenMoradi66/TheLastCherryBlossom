#include "Characters/AUnitCharacter.h"
#include "AI/UUnitMovementComponent.h"  // ✅ اضافه شد
#include "AI/UUnitSteeringComponent.h"
#include "AI/GridPathfinderComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "../TheLastCherryBlossom.h"

AUnitCharacter::AUnitCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // ============================================================
    // 1. ایجاد کامپوننت‌های جدید
    // ============================================================
    UnitMovement = CreateDefaultSubobject<UUnitMovementComponent>(TEXT("UnitMovement"));
    GridPathfinder = CreateDefaultSubobject<UGridPathfinderComponent>(TEXT("GridPathfinder"));
    SteeringComp = CreateDefaultSubobject<UUnitSteeringComponent>(TEXT("SteeringComp"));

    // ============================================================
    // 2. غیرفعال کردن کامل CharacterMovementComponent
    // ============================================================
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        MoveComp->Deactivate();
        MoveComp->SetComponentTickEnabled(false);
        MoveComp->Velocity = FVector::ZeroVector;
        MoveComp->StopMovementImmediately();
    }

    // ============================================================
    // 3. تنظیمات کپسول (همانند قبل)
    // ============================================================
    GetCapsuleComponent()->SetCanEverAffectNavigation(false);
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
    GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Overlap);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_RTS_Obstacle, ECR_Block);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_RTS_MovingUnit, ECR_Block);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_RTS_IdleUnit, ECR_Block);

    // ============================================================
    // 4. تنظیمات Mesh
    // ============================================================
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    GetMesh()->SetCollisionObjectType(ECC_WorldDynamic);
    GetMesh()->SetCollisionResponseToAllChannels(ECR_Ignore);
    GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    GetMesh()->SetGenerateOverlapEvents(false);

    // ============================================================
    // 5. Selection Circle
    // ============================================================
    SelectionCircleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionCircle"));
    SelectionCircleMesh->SetupAttachment(GetRootComponent());
    SelectionCircleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SelectionCircleMesh->SetHiddenInGame(true);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane"));
    if (PlaneMesh.Succeeded())
    {
        SelectionCircleMesh->SetStaticMesh(PlaneMesh.Object);
    }

    const float ZOffset = -GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 5.f;
    SelectionCircleMesh->SetRelativeLocation(FVector(0.f, 0.f, ZOffset));
    SelectionCircleMesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 1.0f));

    // ============================================================
    // 6. مقداردهی اولیه متغیرها
    // ============================================================
    bIsSelected = false;
    CurrentSpeed = 0.f;
    FinalGoalRadius = 600.f;
}

void AUnitCharacter::BeginPlay()
{
    Super::BeginPlay();

    // ============================================================
    // 7. مقداردهی سیستم حرکت جدید
    // ============================================================
    if (UnitMovement)
    {
        UnitMovement->Initialize(this);
        UnitMovement->SetMaxSpeed(450.f);  // مقدار پیش‌فرض
    }

    // ============================================================
    // 8. مقداردهی Steering
    // ============================================================
    if (SteeringComp)
    {
        SteeringComp->Initialize(this);
    }

    // ============================================================
    // 9. تنظیم وضعیت اولیه
    // ============================================================
    SetUnitState(EUnitState::Idle);
}

void AUnitCharacter::SetSelected_Implementation(bool bSelected)
{
    bIsSelected = bSelected;
    OnSelectedChanged(bIsSelected);
}

bool AUnitCharacter::IsSelected_Implementation() const
{
    return bIsSelected;
}

void AUnitCharacter::OnSelectedChanged(bool bNowSelected)
{
    if (GetMesh())
    {
        GetMesh()->SetRenderCustomDepth(bNowSelected);
    }
    
    if (SelectionCircleMesh)
    {
        SelectionCircleMesh->SetHiddenInGame(!bNowSelected);
    }
}

void AUnitCharacter::SetMaxSpeed(float NewMaxSpeed)
{
    if (UnitMovement)
    {
        UnitMovement->SetMaxSpeed(NewMaxSpeed);
    }
}

float AUnitCharacter::GetSpeed() const
{
    if (UnitMovement)
    {
        return UnitMovement->GetCurrentSpeed();
    }
    return 0.f;
}

void AUnitCharacter::PlayRandomHitMontage()
{
    if (HitMontages.Num() == 0) return;
    int32 Index = FMath::RandRange(0, HitMontages.Num() - 1);
    if (UAnimMontage* MontageToPlay = HitMontages[Index])
        if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
            AnimInstance->Montage_Play(MontageToPlay);
}

void AUnitCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AUnitCharacter::SetUnitState(EUnitState NewState)
{
    CurrentState = NewState; 
    
    if (!GetCapsuleComponent()) return;

    if (NewState == EUnitState::Idle)
    {
        GetCapsuleComponent()->SetCollisionObjectType(ECC_RTS_IdleUnit);
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_RTS_MovingUnit, ECR_Block);
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_RTS_IdleUnit, ECR_Block);
    }
    else if (NewState == EUnitState::Moving)
    {
        GetCapsuleComponent()->SetCollisionObjectType(ECC_RTS_MovingUnit);
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_RTS_MovingUnit, ECR_Block);
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_RTS_IdleUnit, ECR_Block);
    }
}

void AUnitCharacter::SetPathAndMove(const TArray<FVector>& Path)
{
    if (CurrentState == EUnitState::Dead || CurrentState == EUnitState::Stunned) return;
    if (Path.Num() < 2) return;
    
    ClearMovementState();
    
    // ✅ سیستم حرکت جدید - فقط Steering را فعال می‌کنیم
    if (SteeringComp)
    {
        SteeringComp->SetPath(Path);
    }
    
    SetUnitState(EUnitState::Moving);
    FinalGoalLocation = Path.Last();
}

void AUnitCharacter::ClearMovementState()
{
    // ✅ پاک کردن Steering
    if (SteeringComp)
    {
        SteeringComp->ClearPath();
        SteeringComp->ClearTarget();
        SteeringComp->ResetDirection();
        SteeringComp->ClearGroupParams();
        SteeringComp->SetDesiredLateralOffset(0.f);
    }
    
    // ✅ توقف فوری توسط سیستم حرکت جدید
    if (UnitMovement)
    {
        UnitMovement->StopImmediately();
    }
    
    if (CurrentState != EUnitState::Dead && CurrentState != EUnitState::Stunned)
        SetUnitState(EUnitState::Idle);
}

void AUnitCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // ✅ بروزرسانی سرعت جاری (برای نمایش)
    if (UnitMovement)
    {
        CurrentSpeed = UnitMovement->GetCurrentSpeed();
    }
    
    if (CurrentState == EUnitState::Dead || CurrentState == EUnitState::Stunned) return;
    if (!SteeringComp) return;
    
    // ✅ فقط Steering را اجرا کن (خودش حرکت را اعمال می‌کند)
    SteeringComp->ExecuteMovement(DeltaTime);
}

void AUnitCharacter::SetSteeringGroupParams(const FVector& Center, const FVector& Forward)
{
    if (SteeringComp)
        SteeringComp->SetGroupParams(Center, Forward);
}

void AUnitCharacter::ClearSteeringGroupParams()
{
    if (SteeringComp)
        SteeringComp->ClearGroupParams();
}

void AUnitCharacter::SetRotationSpeed(float NewRotationSpeed)
{
    if (UnitMovement)
    {
        // ✅ استفاده از Setter عمومی
        UnitMovement->SetRotationInterpSpeed(NewRotationSpeed);
    }
}