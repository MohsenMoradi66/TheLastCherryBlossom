#include "Characters/AUnitCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "../TheLastCherryBlossom.h" 
#include "Components/CapsuleComponent.h"

AUnitCharacter::AUnitCharacter()
{


    PrimaryActorTick.bCanEverTick = true;

    // تنظیمات جابه‌جایی و هوش مصنوعی
    GetCharacterMovement()->bEnablePhysicsInteraction = false;
    GetCharacterMovement()->PushForceFactor = 0.f;
    GetCharacterMovement()->bPushForceUsingZOffset = false;
    GetCharacterMovement()->bUseRVOAvoidance = false;
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->SetAvoidanceEnabled(false);

    // 🌟 راه حل مشکل اول: کاراکترها نباید تحت هیچ شرایطی لایه ناویگیشن زمین را خراب کنند
    GetCapsuleComponent()->SetCanEverAffectNavigation(false); 

    // تنظیمات کپسول فیزیکی کاراکترها
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn); // این مقدار در BeginPlay به Idle تغییر می‌کند
    
    // 🌟 راه حل مشکل دوم: پاسخ اولیه را روی Overlap می‌گذاریم تا توابع Sweep کور نشوند
    GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Overlap);
    
    // دیوارها و موانع استاتیک ادیتور حتماً باید بلاک شوند
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    
    // کانال‌های اختصاصی پروژه که در هدر تعریف کردید را بلاک می‌کنیم تا یونیت‌ها از درون ساختمان‌ها و هم‌دیگر رد نشوند
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_RTS_Obstacle, ECR_Block);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block); // Buildings&Trees
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_RTS_MovingUnit, ECR_Block);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_RTS_IdleUnit, ECR_Block);
    
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    GetMesh()->SetCollisionObjectType(ECC_WorldDynamic);
    GetMesh()->SetCollisionResponseToAllChannels(ECR_Ignore);
    GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    GetMesh()->SetGenerateOverlapEvents(false);
    
    SelectionCircleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionCircle"));
    SelectionCircleMesh->SetupAttachment(GetRootComponent());
    SelectionCircleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SelectionCircleMesh->SetHiddenInGame(true);

    
    // در انتهای سازنده AUnitCharacter::AUnitCharacter() این مقادیر را اصلاح کنید:

    GetCharacterMovement()->BrakingFrictionFactor = 0.0f;          // اصطکاک ترمز صفر
    GetCharacterMovement()->BrakingDecelerationWalking = 0.0f;      // شتاب منفی ترمز صفر
    GetCharacterMovement()->bRequestedMoveUseAcceleration = false;  // عدم استفاده از شتاب برای درخواست‌های حرکتی

    // 🌟 بسیار مهم: شتاب گرفتن کاراکتر را بی‌نهایت یا بسیار بزرگ کنید تا فوراً به ماکسیمم سرعت برسد
    GetCharacterMovement()->MaxAcceleration = 999999.f;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane"));
    if (PlaneMesh.Succeeded())
    {
        SelectionCircleMesh->SetStaticMesh(PlaneMesh.Object);
    }

    const float ZOffset = -GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 5.f;
    SelectionCircleMesh->SetRelativeLocation(FVector(0.f, 0.f, ZOffset));
    SelectionCircleMesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 1.0f));
    
    GridPathfinder = CreateDefaultSubobject<UGridPathfinderComponent>(TEXT("GridPathfinder"));
    SteeringComp = CreateDefaultSubobject<UUnitSteeringComponent>(TEXT("SteeringComp"));

    bIsSelected = false;
    MaxSpeed = 450.f;
    CurrentSpeed = 0.f;
    RotationSpeed = 900.f;
}

void AUnitCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    // مقداردهی اولیه وضعیت به Idle برای تنظیم دقیق کانال کپسول فیزیکی
    SetUnitState(EUnitState::Idle); 

    SetMaxSpeed(MaxSpeed);
    GetCharacterMovement()->RotationRate = FRotator(0.f, RotationSpeed, 0.f);
    bUseControllerRotationYaw = false;

    if (SteeringComp)
        SteeringComp->Initialize(this);
}

// ================== توابع انتخاب ==================

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

// ================== توابع حرکت پایه ==================

void AUnitCharacter::SetMaxSpeed(float NewMaxSpeed)
{
    MaxSpeed = NewMaxSpeed;
    if (GetCharacterMovement())
        GetCharacterMovement()->MaxWalkSpeed = MaxSpeed;
}

float AUnitCharacter::GetSpeed() const
{
    FVector Velocity = GetVelocity();
    return FVector(Velocity.X, Velocity.Y, 0.f).Size();
}

void AUnitCharacter::SetRotationSpeed(float NewRotationSpeed)
{
    RotationSpeed = NewRotationSpeed;
    if (GetCharacterMovement())
        GetCharacterMovement()->RotationRate = FRotator(0.f, RotationSpeed, 0.f);
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
        // سربازهای دیگر باید با این سرباز ایستاده برخورد فیزیکی داشته باشند اما از آن فرار (Evade) نکنند
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_RTS_MovingUnit, ECR_Block);
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_RTS_IdleUnit, ECR_Block);
    }
    else if (NewState == EUnitState::Moving)
    {
        GetCapsuleComponent()->SetCollisionObjectType(ECC_RTS_MovingUnit);
        // سربازهای دیگر هم برخورد فیزیکی دارند و هم سیستم Evade روی این لایه حساس است
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_RTS_MovingUnit, ECR_Block);
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_RTS_IdleUnit, ECR_Block);
    }
}

// ================== تابع اصلی SetPathAndMove (فقط به Steering پاس بده) ==================

void AUnitCharacter::SetPathAndMove(const TArray<FVector>& Path)
{
    if (CurrentState == EUnitState::Dead || CurrentState == EUnitState::Stunned) return;
    if (Path.Num() < 2) return;
    
    ClearMovementState();
    
    if (SteeringComp)
    {
        SteeringComp->SetPath(Path);
    }
    
    if (CurrentState != EUnitState::Moving)
    {
        SetUnitState(EUnitState::Moving);
    }
    
    FinalGoalLocation = Path.Last();
}

// ================== ClearMovementState (ریست کامل) ==================

void AUnitCharacter::ClearMovementState()
{
    // 1. توقف کامل فیزیکی
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->Velocity = FVector::ZeroVector;
        GetCharacterMovement()->ClearAccumulatedForces();
    }
    
    // 2. پاک کردن کامل وضعیت استیرینگ
    if (SteeringComp)
    {
        SteeringComp->ClearPath();
        SteeringComp->ClearTarget();
        SteeringComp->ResetDirection();
        SteeringComp->ClearGroupParams();
        SteeringComp->SetDesiredLateralOffset(0.f);
    }
    
    // 3. تغییر وضعیت به Idle
    if (CurrentState != EUnitState::Dead && CurrentState != EUnitState::Stunned)
        SetUnitState(EUnitState::Idle);
}

// ================== حلقه اصلی Tick (فقط اجراکننده) ==================

void AUnitCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // 🌟 اصلاح شد: به جای خواندن مستقیم از فیزیک، سرعت را بر اساس وضعیت باینری ست می‌کنیم
    if (CurrentState == EUnitState::Moving && GetCharacterMovement() && GetCharacterMovement()->Velocity.Size2D() > 10.f)
    {
        CurrentSpeed = GetCharacterMovement()->MaxWalkSpeed; // قفل روی ماکسیمم سرعت
    }
    else
    {
        CurrentSpeed = 0.f; // قفل روی صفر مطلق
    }
    
    if (CurrentState == EUnitState::Dead || CurrentState == EUnitState::Stunned) return;
    if (!SteeringComp) return;
    
    SteeringComp->ExecuteMovement(DeltaTime);
}

// ================== توابع گروهی (فقط واسطه) ==================

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

