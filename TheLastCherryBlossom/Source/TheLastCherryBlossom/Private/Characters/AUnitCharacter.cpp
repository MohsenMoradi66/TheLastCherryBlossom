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

    if (Path.Num() > 0)
    {
        CurrentPath = Path;
        CurrentPathIndex = 0;

        FinalGoalLocation = CurrentPath.Last();
        FinalGoalRadius = 500.f;

        // فقط خوشه‌های چند نفره FlowField ساخته شود
        bUseFlowField = !bIsSingleUnit;
        if (bUseFlowField)
        {
            SetFlowFieldDestination(CurrentPath[CurrentPathIndex], CurrentPath);
        }

        SetUnitState(EUnitState::MovingToGoal);

        UE_LOG(LogTemp, Warning, TEXT("[%s] Set new path with %d waypoints. bUseFlowField=%d"), *GetName(), CurrentPath.Num(), bUseFlowField);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] Received an empty path. No movement command issued."), *GetName());
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

void AUnitCharacter::MoveDirectlyToTarget(const FVector& Target)
{
    CurrentPath.Empty();
    CurrentPath.Add(Target);
    CurrentPathIndex = 0;

    // ⚡ فقط مسیر مستقیم فعال شود
    bUseFlowField = false;
    SetUnitState(EUnitState::MovingToFormation);
}

void AUnitCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    switch (CurrentState)
    {
        // ============================================================
        //  حالت حرکت اولیه: از FlowField (برای خوشه‌های چند یونیت) 
        //  یا مسیر مستقیم (خوشه تک‌نفره)
        // ============================================================
        case EUnitState::MovingToGoal:
        {
            if (bUseFlowField && ClusterFlowField)
            {
                // --- دریافت سلول FlowField ---
                FVector MyLocation = GetActorLocation();
                FIntPoint CellCoord = ClusterFlowField->WorldToGrid(MyLocation);
                const FFlowFieldCell* Cell = ClusterFlowField->GetCell(CellCoord);

                if (Cell)
                {
                    FVector MoveDir = Cell->Direction;
                    if (MoveDir.IsNearlyZero())
                        MoveDir = Cell->PathVector;

                    MoveDir = MoveDir.GetSafeNormal();

                    if (!MoveDir.IsNearlyZero())
                        AddMovementInput(MoveDir, 1.f);
                }

                // چک ورود به Formation Sphere (فقط برای FlowField)
                float DistToSphereCenter = FVector::Dist(GetActorLocation(), FinalGoalLocation);
                if (DistToSphereCenter <= FormationSphereRadius)
                {
                    bUseFlowField = false;
                    SetUnitState(EUnitState::MovingToFormation);
                    bReachedFormationTarget = false;

                    UE_LOG(LogTemp, Warning,
                        TEXT("[%s] Entered Formation Sphere → switching to MovingToFormation"),
                        *GetName());
                }
            }
            else
            {
                // --- مسیر نقطه به نقطه برای تک یونیت ---
                if (CurrentPath.IsValidIndex(CurrentPathIndex))
                {
                    FVector TargetPoint = CurrentPath[CurrentPathIndex];
                    FVector Dir = (TargetPoint - GetActorLocation());
                    float Dist = Dir.Size();

                    if (Dist > KINDA_SMALL_NUMBER)
                    {
                        Dir /= Dist; // نرمال‌سازی
                        AddMovementInput(Dir, 1.f);
                    }

                    const float ReachThreshold = 50.f; // فاصله‌ای که مسیر به نقطه بعدی تغییر کند
                    if (Dist < ReachThreshold)
                    {
                        CurrentPathIndex++;
                        if (!CurrentPath.IsValidIndex(CurrentPathIndex))
                        {
                            // مسیر تک یونیت تمام شد → حرکت به FormationSlot
                            SetUnitState(EUnitState::MovingToFormation);
                            bReachedFormationTarget = false;
                            UE_LOG(LogTemp, Warning,
                                TEXT("[%s] Single-unit path finished, switching to Formation"),
                                *GetName());
                        }
                    }
                }

            }
            break;
        }

        // ============================================================
        //  حرکت به سمت Slot نهایی در آرایش
        // ============================================================
        case EUnitState::MovingToFormation:
        {
            if (!bReachedFormationTarget)
            {
                FVector ToTarget = FormationTarget - GetActorLocation();
                float DistToTarget = ToTarget.Size();

                const float StopThreshold = 35.f;

                if (DistToTarget > StopThreshold)
                {
                    FVector Dir = ToTarget.GetSafeNormal();
                    float SpeedFactor = FMath::Clamp(DistToTarget / 100.f, 0.2f, 1.f);
                    AddMovementInput(Dir, SpeedFactor);
                }
                else
                {
                    // رسیدن به اسلات Formation
                    bReachedFormationTarget = true;

                    GetCharacterMovement()->StopMovementImmediately();
                    SetActorLocation(FormationTarget);

                    // پاکسازی مسیر و FlowField
                    CurrentPath.Empty();
                    CurrentPathIndex = 0;
                    ClusterFlowField = nullptr;
                    bUseFlowField = false;

                    // حالت Idle
                    SetUnitState(EUnitState::Idle);

                    UE_LOG(LogTemp, Warning,
                        TEXT("[%s] Reached Formation Slot → entering Idle"),
                        *GetName());
                }
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

    SetUnitState(EUnitState::MovingToGoal);
}











