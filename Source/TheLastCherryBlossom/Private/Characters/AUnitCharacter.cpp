#include "Characters/AUnitCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "AI/UFlowFieldComponent.h"
#include "Core/ARTSPlayerController.h"
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
    if (CurrentState == NewState) return;

    CurrentState = NewState;

    switch (CurrentState)
    {
    case EUnitState::Idle:
        GetCharacterMovement()->Velocity = FVector::ZeroVector;
        // اگر قبلاً DisableMovement کردی، اینجا لازم نیست دوباره کنی
        if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
        {
            AnimInst->Montage_Stop(0.1f);
        }
        break;

    case EUnitState::Moving_Single:
    case EUnitState::Moving_Cluster:
    case EUnitState::MovingToFormation:
        // <<< این خط خیلی مهمه >>>
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);  // حتماً فعال کن
        break;

    case EUnitState::Dead:
        GetCharacterMovement()->DisableMovement();
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        GetCapsuleComponent()->SetCanEverAffectNavigation(false);
        break;

    case EUnitState::Stunned:
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

    // محاسبه سرعت لحظه‌ای افقی (بدون در نظر گرفتن پرش یا سقوط)
    CurrentSpeed = GetVelocity().Size2D();  // بدون تعریف float جدید

    if (CurrentSpeed > 5.0f)
    {
        UE_LOG(LogTemp, Log, TEXT("[%s] Speed: %.1f / Max: %.1f | State: %s | DistToSlot: %.1f"),
            *GetName(),
            CurrentSpeed,
            MaxSpeed,
            *UEnum::GetValueAsString(CurrentState),
            FVector::Dist(GetActorLocation(), FormationTarget));
    }
    // اگر سرعت ≤ 5 باشه، هیچی چاپ نمی‌شه → لاگ تمیز می‌مونه
    switch (CurrentState)
    {
    // ============================================================
   
        case EUnitState::Moving_Single:
{
    if (CurrentPath.Num() == 0)
        break;

    FVector MyLocation = GetActorLocation();

    // اگر هنوز در حال دنبال کردن Waypointها هستیم
    if (CurrentPathIndex < CurrentPath.Num())
    {
        FVector CurrentTarget = CurrentPath[CurrentPathIndex];
        FVector ToTarget = CurrentTarget - MyLocation;
        float DistToTarget = ToTarget.Size();

        // Acceptance radius برای هر waypoint
        const float WaypointAcceptanceRadius = 100.f;

        if (DistToTarget <= WaypointAcceptanceRadius)
        {
            CurrentPathIndex++;

            if (CurrentPathIndex >= CurrentPath.Num())
                break;
            
            CurrentTarget = CurrentPath[CurrentPathIndex];
            ToTarget = CurrentTarget - MyLocation;
        }

        FVector DesiredDirection = ToTarget.GetSafeNormal();

        SmoothedDirection = FMath::VInterpTo(SmoothedDirection, DesiredDirection, DeltaTime, 8.0f);

        if (!SmoothedDirection.IsNearlyZero())
        {
            AddMovementInput(SmoothedDirection, 1.f);
        }
    }
    else // تمام waypointها طی شده → مستقیم به FinalGoal
    {
        FVector ToGoal = FinalGoalLocation - MyLocation;
        float DistToGoal = ToGoal.Size();

        if (DistToGoal > 50.f)
        {
            FVector DesiredDirection = ToGoal.GetSafeNormal();
            SmoothedDirection = FMath::VInterpTo(SmoothedDirection, DesiredDirection, DeltaTime, 6.0f);
            AddMovementInput(SmoothedDirection, 1.f);
        }
        else
        {
            SmoothedDirection = FVector::ZeroVector;
        }
    }

    // <<< تغییر اصلی: وقتی وارد کره تشکیلات شد، مستقیم به اسلات شخصی برو >>>
    float DistToFormation = FVector::Dist(MyLocation, FinalGoalLocation);
    if (DistToFormation <= FinalGoalRadius && !bReachedFormationTarget)
    {
        bReachedFormationTarget = true;
        
        // این خط باعث می‌شه FlowField یا مسیر قبلی خاموش بشه و مستقیم به اسلات بره
        MoveDirectlyToTarget(FormationTarget);

        UE_LOG(LogTemp, Warning, TEXT("[%s] SINGLE: Entered Formation Sphere → Moving directly to personal slot %s"), *GetName(), *FormationTarget.ToString());
    }

    break;
}
    // ============================================================
    case EUnitState::Moving_Cluster:
            {
                if (!ClusterFlowField)
                    break;

                FVector MyLoc = GetActorLocation();
                FIntPoint Cell = ClusterFlowField->WorldToGrid(MyLoc);

                // <<< تغییر اصلی: دیگر اشاره‌گر نیست، کپی از سلول رو می‌گیریم >>>
                FFlowFieldCell FlowCell = ClusterFlowField->GetCell(Cell);

                // حالا چک می‌کنیم که آیا سلول معتبر و داخل کریدور هست یا نه
                if (!FlowCell.bInCorridor)
                    break;

                // اولویت با Direction نهایی (که ترکیب Path + Repulsion + Smoothing هست)
                FVector Dir = FlowCell.Direction;

                // اگر Direction صفر بود (مثلاً در نزدیکی مقصد)، از PathVector استفاده کن
                if (Dir.IsNearlyZero())
                {
                    Dir = FlowCell.PathVector;
                }

                // اگر هنوز هم صفر بود، حرکت نکن (جلوگیری از لرزش)
                if (!Dir.IsNearlyZero())
                {
                    AddMovementInput(Dir.GetSafeNormal(), 1.f);
                }

                // <<< تغییر اصلی: وقتی وارد کره تشکیلات شد، مستقیم به اسلات شخصی برو >>>
                float DistToGoal = FVector::Dist(MyLoc, FinalGoalLocation);
                if (DistToGoal <= FinalGoalRadius && !bReachedFormationTarget)
                {
                    bReachedFormationTarget = true;

                    // FlowField رو خاموش کن و مستقیم به اسلات اختصاص‌داده‌شده برو
                    MoveDirectlyToTarget(FormationTarget);

                    UE_LOG(LogTemp, Warning, TEXT("[%s] CLUSTER: Entered Formation Sphere → Moving directly to personal slot %s"), 
                        *GetName(), *FormationTarget.ToString());
                }

                break;
            }

    // ============================================================
    case EUnitState::MovingToFormation:
            {
                FVector CurrentLocation = GetActorLocation();
                FVector ToSlot = FormationTarget - CurrentLocation;

                // فقط فاصله افقی (XY) رو حساب کن – ارتفاع زمین تأثیر نذاره
                ToSlot.Z = 0.f;
                float Dist = ToSlot.Size();

                // شرط توقف: خیلی نزدیک شد یا گیر کرد (سرعت کم شد)
                if (Dist <= 40.f || (Dist <= 100.f && CurrentSpeed < 80.f))
                {
                    GetCharacterMovement()->StopMovementImmediately();
                    GetCharacterMovement()->Velocity = FVector::ZeroVector;

                    // قفل روی صفحه XY (زمین)
                    GetCharacterMovement()->bConstrainToPlane = true;
                    GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0, 0, 1));

                    // کاملاً حرکت رو غیرفعال کن
                    GetCharacterMovement()->DisableMovement();

                    SetUnitState(EUnitState::Idle);

                    UE_LOG(LogTemp, Log, TEXT("[%s] Final stop at formation slot. Dist: %.1f | Speed: %.1f"), *GetName(), Dist, CurrentSpeed);
                }
                else
                {
                    // جهت مسطح‌شده (فقط XY)
                    FVector Direction = ToSlot.GetSafeNormal();

                    // سرعت کامل تا 100 واحد، بعد کمی کند شو (حداقل 70% سرعت)
                    float SpeedScale = (Dist > 100.f) ? 1.0f : FMath::Clamp(Dist / 100.f, 0.7f, 1.0f);

                    AddMovementInput(Direction, SpeedScale);

                    // اگر به هر دلیلی سرعت افتاد (مثل friction)، force کن تا گیر نکنه
                    if (CurrentSpeed < MaxSpeed * 0.7f)
                    {
                        GetCharacterMovement()->Velocity = Direction * (MaxSpeed * SpeedScale);
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

    SetUnitState(EUnitState::Moving_Single);
}

void AUnitCharacter::MoveDirectlyToTarget(const FVector& Target)
{
    CurrentPath = { Target };
    CurrentPathIndex = 0;

    bUseFlowField = false;
    SetUnitState(EUnitState:: MovingToFormation);
}









