#include "Characters/AUnitCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "AI/UFlowFieldComponent.h"
#include "Core/ARTSPlayerController.h"
#include "NavigationSystem.h"
#include "NavigationSystem.h"
#include "NavAreas/NavArea_Null.h"
#include "NavAreas/NavArea_Obstacle.h" // یا NavArea_Default در صورت نیاز
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"


AUnitCharacter::AUnitCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    
    // غیرفعال کردن تعامل فیزیکی
    GetCharacterMovement()->bEnablePhysicsInteraction = false;
    GetCharacterMovement()->PushForceFactor = 0.f;
    GetCharacterMovement()->bPushForceUsingZOffset = false;

    // غیرفعال کردن سیستم اجتناب از دیگر یونیت‌ها
    GetCharacterMovement()->bUseRVOAvoidance = false;
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->SetAvoidanceEnabled(false);   // جلوگیری از ورود به avoidance
    GetCapsuleComponent()->SetCanEverAffectNavigation(true); // یونیت روی NavMesh اثر نداره
    
    // Collision Capsule
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
    GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block); // فقط زمین و اشیا
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // نادیده گرفتن یونیت‌ها
    

    // Mesh فقط برای کلیک
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    GetMesh()->SetCollisionObjectType(ECC_WorldDynamic);
    GetMesh()->SetCollisionResponseToAllChannels(ECR_Ignore);
    GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    GetMesh()->SetGenerateOverlapEvents(false);

    // Selection Capsule برای LineTrace
    SelectionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("SelectionCapsule"));
    SelectionCapsule->SetupAttachment(GetMesh());
    SelectionCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SelectionCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
    SelectionCapsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    SelectionCapsule->SetGenerateOverlapEvents(false);
    SelectionCapsule->SetCapsuleRadius(50.f);
    SelectionCapsule->SetCapsuleHalfHeight(100.f);
    SelectionCapsule->SetRelativeLocation(FVector(0.f, 0.f, 100.f));

    // اضافه کردن Pathfinder (NavMesh)
    GridPathfinder = CreateDefaultSubobject<UGridPathfinderComponent>(TEXT("GridPathfinder"));

    // اضافه کردن کامپوننت فلو فیلد
    FlowFieldComponent = CreateDefaultSubobject<UFlowFieldComponent>(TEXT("FlowFieldComponent"));

    // مقادیر پیش‌فرض
    bIsSelected = false;
    bIsRotating = false;
    MaxSpeed = 450.f;
    CurrentSpeed = 0.f;
    RotationSpeed = 900.f;
    CurrentPathIndex = 0;
    Acceleration = 800.f;
    Deceleration = 1200.f;
    
}

void AUnitCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    bReachedFormationTarget = false;

    if (GetCharacterMovement())
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] MovementMode=%d MaxWalkSpeed=%f"), *GetName(), (int)GetCharacterMovement()->MovementMode, GetCharacterMovement()->MaxWalkSpeed);
    }


    if (!FlowFieldComponent) {
        UE_LOG(LogTemp, Warning, TEXT("[%s] Tick: FlowFieldComponent == nullptr"), *GetName());
        return;
    }

    
    SetMaxSpeed(MaxSpeed); // اعمال سرعت از متغیر قابل تنظیم
    // تنظیمات چرخش
    GetCharacterMovement()->RotationRate = FRotator(0.f, RotationSpeed, 0.f); // ← اینجا
    bUseControllerRotationYaw = false;
    UE_LOG(LogTemp, Warning, TEXT("%s: BeginPlay -> Pathfinder ready."), *GetName());
    
}

void AUnitCharacter::SetSelected_Implementation(bool bSelected)
{
    this->bIsSelected = bSelected;
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

    SelectionCapsule->SetHiddenInGame(!bNowSelected);
}

void AUnitCharacter::SetMaxSpeed(float NewMaxSpeed)
{
    MaxSpeed = NewMaxSpeed; // بروزرسانی متغیر داخلی
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->MaxWalkSpeed = MaxSpeed; // اعمال روی CharacterMovement
    }

    UE_LOG(LogTemp, Warning, TEXT("[%s] MaxSpeed updated to: %f"), *GetName(), MaxSpeed);
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
    {
        GetCharacterMovement()->RotationRate = FRotator(0.f, RotationSpeed, 0.f);
    }

    UE_LOG(LogTemp, Warning, TEXT("[%s] RotationSpeed updated to: %f"), *GetName(), RotationSpeed);
}

void AUnitCharacter::PlayRandomHitMontage()
{
    if (HitMontages.Num() == 0) return;

    int32 Index = FMath::RandRange(0, HitMontages.Num() - 1);
    if (UAnimMontage* MontageToPlay = HitMontages[Index])
    {
       if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
       {
          AnimInstance->Montage_Play(MontageToPlay);
       }
    }
}

void AUnitCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AUnitCharacter::SetPathAndMove(const TArray<FVector>& Path, bool bIsSingleUnit)
{
    if (CurrentState == EUnitState::Dead || CurrentState == EUnitState::Stunned)
        return;

    if (Path.Num() == 0)
        return;

    CurrentPath = Path;
    CurrentPathIndex = 0;
    FinalGoalLocation = Path.Last();

    if (bIsSingleUnit)
    {
        bUseFlowField = false;
        SetUnitState(EUnitState::Moving_Single);
    }
    else
    {
        bUseFlowField = true;
        SetFlowFieldDestination(CurrentPath[0], CurrentPath);
        SetUnitState(EUnitState::Moving_Cluster);
    }
}

void AUnitCharacter::SetUnitState(EUnitState NewState)
{
    // اگر وضعیت جدید همان وضعیت فعلی است، برگرد
    if (CurrentState == NewState) return;

    CurrentState = NewState;
    
    // اینجا می‌تونیم کدهای مربوط به شروع وضعیت جدید رو اضافه کنیم
    switch (CurrentState)
    {
    case EUnitState::Idle:
        // وقتی Idle شد، مسیر فعلی رو پاک کن
        CurrentPath.Empty();
        CurrentPathIndex = 0;
        
        // 1. توقف فوری حرکت و حذف تمام ورودی‌های باقی‌مانده
        GetCharacterMovement()->StopMovementImmediately(); 
        
        // 2. 🛑 پاکسازی Flow Field و غیرفعال‌سازی پرچم آن (برای قطع نویز)
        // فرض بر این است که ClusterFlowField یک متغیر در کلاس AUnitCharacter است.
        ClusterFlowField = nullptr; // مرجع به Flow Field خوشه را پاک می‌کند
        bUseFlowField = false;      // پرچم استفاده از Flow Field را خاموش می‌کند
        
        // 3. خاموش کردن چرخش بر اساس حرکت در زمان توقف
        GetCharacterMovement()->bOrientRotationToMovement = false; 
        
        break;

    case EUnitState::Moving:
        // وقتی Moving شد، فقط اجازه حرکت داریم
        // 💡 اگر یونیت قرار است در حالت Moving بچرخد، این خط را فعال کنید:
        // GetCharacterMovement()->bOrientRotationToMovement = true;
        break;
        
    case EUnitState::Attacking:
        // توقف حرکت و پخش انیمیشن حمله
        GetCharacterMovement()->StopMovementImmediately();
        break;

    case EUnitState::Dead:
        // غیر فعال کردن حرکت و collision
        GetCharacterMovement()->DisableMovement();
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        // همچنین مطمئن شوید که یونیت اثر ناوبری را از دست می‌دهد
        GetCapsuleComponent()->SetCanEverAffectNavigation(false);
        break;

    case EUnitState::Stunned:
        // توقف کامل حرکت و جلو گیری از تغییر مسیر
        GetCharacterMovement()->StopMovementImmediately();
        break;

    default:
        break;
    }

    UE_LOG(LogTemp, Warning, TEXT("[%s] State changed to %s"), *GetName(), *UEnum::GetValueAsString(CurrentState));
}

void AUnitCharacter::SetFlowFieldDestination(const FVector& Destination, const TArray<FVector>& Path, int32 CorridorWidth)
{
    if (FlowFieldComponent)
    {
        FlowFieldComponent->GenerateFlowField(Destination, Path, CorridorWidth);
    }
}

void AUnitCharacter::SetClusterFlowField(UFlowFieldComponent* NewFlow)
{
    if (NewFlow && ClusterFlowField != NewFlow)
    {
        ClusterFlowField = NewFlow;
        UE_LOG(LogTemp, Warning, TEXT("[%s] FlowField assigned!"), *GetName());
    }
}

void AUnitCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    switch (CurrentState)
    {
    // ============================================================
    //  حرکت تک یونیت (Path مستقیم - بدون FlowField)
    // ============================================================
    case EUnitState::Moving_Single:
        {
            if (CurrentPath.Num() == 0)
                break;

            FVector TargetPoint = CurrentPath[CurrentPathIndex];
            FVector ToTarget = TargetPoint - GetActorLocation();

            if (ToTarget.Size() < 80.f)
            {
                CurrentPathIndex++;

                if (CurrentPathIndex >= CurrentPath.Num())
                {
                    UE_LOG(LogTemp, Warning,
                        TEXT("[%s] SINGLE: Path finished → Formation"), *GetName());

                    SetUnitState(EUnitState::MovingToFormation);
                    break;
                }
            }
            else
            {
                AddMovementInput(ToTarget.GetSafeNormal(), 1.f);
            }

            break;
        }


    // ============================================================
    //  حرکت خوشه‌ای (FlowField – فقط برای چند یونیت)
    // ============================================================
    case EUnitState::Moving_Cluster:
        {
            if (!ClusterFlowField)
                break;

            FVector MyLoc = GetActorLocation();
            FIntPoint Cell = ClusterFlowField->WorldToGrid(MyLoc);
            const FFlowFieldCell* FlowCell = ClusterFlowField->GetCell(Cell);

            if (!FlowCell)
                break;

            FVector Dir = FlowCell->Direction;
            if (Dir.IsNearlyZero())
                Dir = FlowCell->PathVector;

            AddMovementInput(Dir.GetSafeNormal(), 1.f);

            float DistToEnd = FVector::Dist(MyLoc, FinalGoalLocation);
                if (DistToEnd <= FormationSphereRadius) // ← استفاده از متغیر موجود
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("[%s] CLUSTER: FlowField end → Formation"), *GetName());

                SetUnitState(EUnitState::MovingToFormation);
            }
            break;
        }


    // ============================================================
    //  حرکت دقیق به Slot نهایی در آرایش
    // ============================================================
    case EUnitState::MovingToFormation:
        {
            FVector ToSlot = FormationTarget - GetActorLocation();
            float Dist = ToSlot.Size();

            if (Dist > 30.f)
            {
                AddMovementInput(ToSlot.GetSafeNormal(), 0.6f);
            }
            else
            {
                GetCharacterMovement()->StopMovementImmediately();
                SetActorLocation(FormationTarget);
                SetUnitState(EUnitState::Idle);
            }
            break;
        }


    default:
        break;
    }
}

void AUnitCharacter::FollowPathDirectly(const TArray<FVector>& PathPoints)
{
    if (PathPoints.Num() == 0) return;

    CurrentPath = PathPoints;  // ← همان CurrentPath موجود در کلاس
    CurrentPathIndex = 0;
    bUseFlowField = false;     // تک یونیت، FlowField نداریم

    SetUnitState(EUnitState::Moving_Single);
}

void AUnitCharacter::MoveDirectlyToTarget(const FVector& Target)
{
    CurrentPath = { Target };
    CurrentPathIndex = 0;

    bUseFlowField = false;
    SetUnitState(EUnitState::Moving_Single);
}









