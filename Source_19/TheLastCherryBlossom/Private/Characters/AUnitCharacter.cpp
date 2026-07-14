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
        UnitMovement->SetMaxSpeed(MaxSpeed);                      
        UnitMovement->SetRotationInterpSpeed(RotationInterpSpeed);  
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
        MaxSpeed = NewMaxSpeed;            
        if (UnitMovement)
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
    if (SteeringComp)
    {
        SteeringComp->ClearPath();
        SteeringComp->ClearTarget();
        SteeringComp->ResetDirection();
        SteeringComp->ClearGroupParams();
        SteeringComp->SetDesiredLateralOffset(0.f);
    }
    
    if (UnitMovement)
    {
        UnitMovement->StopImmediately();
    }
    
    // ===== فقط اگر مرده یا گیج نیست، به Idle برو =====
    if (CurrentState != EUnitState::Dead && CurrentState != EUnitState::Stunned)
    {
        SetUnitState(EUnitState::Idle);  // اینجا Collision به درستی تنظیم می‌شود
    }
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
    RotationInterpSpeed = NewRotationSpeed;   // این خط اضافه بشه
    if (UnitMovement)
        UnitMovement->SetRotationInterpSpeed(NewRotationSpeed);
}

// ================== تابع SetUnitState اصلاح شده ==================

// AUnitCharacter.cpp
void AUnitCharacter::SetUnitState(EUnitState NewState)
{
    if (CurrentState == NewState) return;
    
    EUnitState OldState = CurrentState;
    CurrentState = NewState;
    
    // ===== خروج از وضعیت قبلی =====
    if (OldState == EUnitState::Stuck)
    {
        // رفع گیر انجام شد
        if (SteeringComp)
        {
            SteeringComp->ResetUnstuckAttempts();  // ریست تلاش‌ها
        }
    }
    
    // ===== ورود به وضعیت جدید =====
    switch (NewState)
    {
    case EUnitState::Idle:
        // تنظیم Collision برای Idle
            SetIdleCollision();
        if (UnitMovement) UnitMovement->StopImmediately();
        break;
            
    case EUnitState::Moving:
        // تنظیم Collision برای Moving
            SetMovingCollision();
        break;
            
    case EUnitState::Stuck:
        // تنظیم Collision برای Stuck (مثل Idle ولی با تفاوت)
            SetStuckCollision();
            
        // شروع فرآیند رفع گیر
        if (SteeringComp)
        {
            SteeringComp->AttemptUnstuck(GetWorld()->GetDeltaSeconds());
        }
        break;
            
    case EUnitState::Dead:
    case EUnitState::Stunned:
        // تنظیم Collision برای مرده/گیج
        SetDeadCollision();
        break;
            
    case EUnitState::Attacking:
        // تنظیم Collision برای حمله
            SetAttackingCollision();
        break;
    }
    
    // ===== اطلاع به سایر سیستم‌ها =====
    OnStateChanged(OldState, NewState);
}

// در AUnitCharacter.cpp پیاده‌سازی کن:

void AUnitCharacter::SetIdleCollision()
{
    UCapsuleComponent* Capsule = GetCapsuleComponent();
    if (!Capsule) return;
    
    Capsule->SetCollisionObjectType(ECC_GameTraceChannel5); // RTS_IdleUnit
    Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Block);
    Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel5, ECR_Block);
    Capsule->SetCollisionResponseToChannel(ECC_RTS_Obstacle, ECR_Block);
    Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
}

void AUnitCharacter::SetMovingCollision()
{
    UCapsuleComponent* Capsule = GetCapsuleComponent();
    if (!Capsule) return;
    
    Capsule->SetCollisionObjectType(ECC_GameTraceChannel4); // RTS_MovingUnit
    Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Block);
    Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel5, ECR_Block);
    Capsule->SetCollisionResponseToChannel(ECC_RTS_Obstacle, ECR_Block);
    Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
}

void AUnitCharacter::SetStuckCollision()
{
    UCapsuleComponent* Capsule = GetCapsuleComponent();
    if (!Capsule) return;
    
    Capsule->SetCollisionObjectType(ECC_GameTraceChannel5);
    Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Block);
    Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel5, ECR_Block);
    Capsule->SetCollisionResponseToChannel(ECC_RTS_Obstacle, ECR_Block);
    Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
}

void AUnitCharacter::SetDeadCollision()
{
    UCapsuleComponent* Capsule = GetCapsuleComponent();
    if (!Capsule) return;
    
    Capsule->SetCollisionObjectType(ECC_WorldDynamic);
    Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
    Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
}

void AUnitCharacter::SetAttackingCollision()
{
    // مثل Moving یا هر چیزی که می‌خوای
    SetMovingCollision();
}

void AUnitCharacter::OnStateChanged(EUnitState OldState, EUnitState NewState)
{
    // اینجا می‌تونی Event/Delegate صدا بزنی یا Log کنی
    UE_LOG(LogTemp, Log, TEXT("Unit %s state changed from %d to %d"), 
           *GetName(), (int32)OldState, (int32)NewState);
}