
#include "Characters/AUnitCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "AI/UUnitSteeringComponent.h"
#include "Components/CapsuleComponent.h"


AUnitCharacter::AUnitCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    GetCharacterMovement()->bEnablePhysicsInteraction = false;
    GetCharacterMovement()->PushForceFactor = 0.f;
    GetCharacterMovement()->bPushForceUsingZOffset = false;
    GetCharacterMovement()->bUseRVOAvoidance = false;
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->SetAvoidanceEnabled(false);
    GetCapsuleComponent()->SetCanEverAffectNavigation(true);

    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
    GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

    GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    GetMesh()->SetCollisionObjectType(ECC_WorldDynamic);
    GetMesh()->SetCollisionResponseToAllChannels(ECR_Ignore);
    GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    GetMesh()->SetGenerateOverlapEvents(false);
    
    // بعد از ایجاد GridPathfinder و SteeringComponent، این کد را اضافه کنید
    SelectionCircleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionCircle"));
    SelectionCircleMesh->SetupAttachment(GetRootComponent());
    SelectionCircleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SelectionCircleMesh->SetHiddenInGame(true); // ابتدا مخفی


    GetCharacterMovement()->BrakingFrictionFactor = 2.0f;      // ضریب اصطکاک هنگام ترمز (بالا بردن اصطکاک)
    GetCharacterMovement()->BrakingDecelerationWalking = 5000.f; // شتاب منفی بسیار شدید هنگام توقف (ترمز آنی)
    GetCharacterMovement()->bRequestedMoveUseAcceleration = false; // نادیده گرفتن شتاب روی مسیرهای درخواستی

    // بارگذاری مش صفحه (Plane) به عنوان دایره موقت
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane"));
    if (PlaneMesh.Succeeded())
    {
       SelectionCircleMesh->SetStaticMesh(PlaneMesh.Object);
    }

    // موقعیت زیر پای یونیت (نسبت به ریشه)
    const float ZOffset = -GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 5.f;
    SelectionCircleMesh->SetRelativeLocation(FVector(0.f, 0.f, ZOffset));
    SelectionCircleMesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 1.0f)); // اندازه دلخواه

    
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
    SetMaxSpeed(MaxSpeed);
    GetCharacterMovement()->RotationRate = FRotator(0.f, RotationSpeed, 0.f);
    bUseControllerRotationYaw = false;

    if (SteeringComp)
        SteeringComp->Initialize(this);
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
    // افکت هایلایت روی مش اصلی (اختیاری)
    if (GetMesh())
    {
       GetMesh()->SetRenderCustomDepth(bNowSelected);
    }
    
    // نمایش/مخفی کردن دایره زیر پا
    if (SelectionCircleMesh)
    {
       SelectionCircleMesh->SetHiddenInGame(!bNowSelected);
    }
}

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

void AUnitCharacter::ClearMovementState()
{
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->Velocity = FVector::ZeroVector;
    }

    if (SteeringComp)
    {
        SteeringComp->ClearTarget();
        SteeringComp->ResetDirection();
    }

    CurrentPath.Empty();
    PathProgress = 0.f;
    TotalPathLength = 0.f;

    if (CurrentState != EUnitState::Dead && CurrentState != EUnitState::Stunned)
        SetUnitState(EUnitState::Idle);
}

FVector AUnitCharacter::GetDirectionAtProgress(float Progress) const
{
    if (CurrentPath.Num() < 2) return FVector::ForwardVector;
    float TotalLen = TotalPathLength; // از متغیر کش شده استفاده کن
    float TargetLen = Progress * TotalLen;
    float Accum = 0.f;
    for (int32 i = 0; i < CurrentPath.Num() - 1; i++)
    {
        float SegLen = FVector::Dist(CurrentPath[i], CurrentPath[i+1]);
        if (TargetLen <= Accum + SegLen)
        {
            return (CurrentPath[i+1] - CurrentPath[i]).GetSafeNormal();
        }
        Accum += SegLen;
    }
    // اگر به اینجا رسیدیم یعنی Progress حدود 1 است، آخرین قطعه را برگردان
    int32 LastIndex = CurrentPath.Num() - 2;
    return (CurrentPath[LastIndex + 1] - CurrentPath[LastIndex]).GetSafeNormal();
}

void AUnitCharacter::FindClosestPointAndTangent(const FVector& Location, const TArray<FVector>& Path, FVector& OutPoint, FVector& OutTangent) const
{
    if (Path.Num() < 2) return;
    
    float MinDistSq = FLT_MAX;
    int32 BestSeg = 0;
    float BestT = 0.f;
    
    for (int32 i = 0; i < Path.Num() - 1; i++)
    {
        const FVector& A = Path[i];
        const FVector& B = Path[i+1];
        FVector AB = B - A;
        float ABLenSq = AB.SizeSquared();
        if (ABLenSq < SMALL_NUMBER) continue;
        
        FVector AC = Location - A;
        float t = FMath::Clamp(FVector::DotProduct(AC, AB) / ABLenSq, 0.f, 1.f);
        FVector Closest = A + AB * t;
        float DistSq = (Location - Closest).SizeSquared();
        if (DistSq < MinDistSq)
        {
            MinDistSq = DistSq;
            BestSeg = i;
            BestT = t;
        }
    }
    
    OutPoint = Path[BestSeg] + (Path[BestSeg+1] - Path[BestSeg]) * BestT;
    OutTangent = (Path[BestSeg+1] - Path[BestSeg]).GetSafeNormal();
}

FVector AUnitCharacter::GetClosestTangent(const FVector& Location, float& OutDistanceToEnd) const
{
    if (CurrentPath.Num() < 2) return FVector::ZeroVector;

    int32 BestSeg = 0;
    float BestT = 0.f;
    float MinDistSq = FLT_MAX;

    // پیدا کردن نزدیک‌ترین قطعه و پارامتر t
    for (int32 i = 0; i < CurrentPath.Num() - 1; i++)
    {
        const FVector& A = CurrentPath[i];
        const FVector& B = CurrentPath[i+1];
        FVector AB = B - A;
        float ABLenSq = AB.SizeSquared();
        if (ABLenSq < SMALL_NUMBER) continue;

        FVector AC = Location - A;
        float t = FMath::Clamp(FVector::DotProduct(AC, AB) / ABLenSq, 0.f, 1.f);
        FVector Closest = A + AB * t;
        float DistSq = (Location - Closest).SizeSquared();
        if (DistSq < MinDistSq)
        {
            MinDistSq = DistSq;
            BestSeg = i;
            BestT = t;
        }
    }

    // محاسبه فاصله از نقطه جاری روی مسیر تا انتهای مسیر
    FVector ClosestPoint = CurrentPath[BestSeg] + (CurrentPath[BestSeg+1] - CurrentPath[BestSeg]) * BestT;
    OutDistanceToEnd = FVector::Dist(ClosestPoint, CurrentPath[BestSeg+1]);
    for (int32 i = BestSeg + 1; i < CurrentPath.Num() - 1; i++)
    {
        OutDistanceToEnd += FVector::Dist(CurrentPath[i], CurrentPath[i+1]);
    }

    // مماس = جهت قطعه
    return (CurrentPath[BestSeg+1] - CurrentPath[BestSeg]).GetSafeNormal();
}

float AUnitCharacter::CalculateCurrentLateralOffset() const
{
    if (CurrentPath.Num() < 2) return 0.f;
    
    // نزدیک‌ترین نقطه روی مسیر به موقعیت فعلی
    int32 BestSeg = 0;
    float BestT = 0.f;
    float MinDistSq = FLT_MAX;
    
    for (int32 i = 0; i < CurrentPath.Num() - 1; i++)
    {
       const FVector& A = CurrentPath[i];
       const FVector& B = CurrentPath[i+1];
       FVector AB = B - A;
       float ABLenSq = AB.SizeSquared();
       if (ABLenSq < SMALL_NUMBER) continue;
        
       FVector AC = GetActorLocation() - A;
       float t = FMath::Clamp(FVector::DotProduct(AC, AB) / ABLenSq, 0.f, 1.f);
       FVector Closest = A + AB * t;
       float DistSq = (GetActorLocation() - Closest).SizeSquared();
       if (DistSq < MinDistSq)
       {
          MinDistSq = DistSq;
          BestSeg = i;
          BestT = t;
       }
    }
    
    // نقطه نزدیک روی مسیر و مماس
    FVector ClosestPoint = CurrentPath[BestSeg] + (CurrentPath[BestSeg+1] - CurrentPath[BestSeg]) * BestT;
    FVector Tangent = (CurrentPath[BestSeg+1] - CurrentPath[BestSeg]).GetSafeNormal();
    FVector RightDir = FVector::CrossProduct(Tangent, FVector::UpVector).GetSafeNormal();
    FVector ToUnit = GetActorLocation() - ClosestPoint;
    
    return FVector::DotProduct(ToUnit, RightDir);
}

void AUnitCharacter::SetUnitState(EUnitState NewState)
{
    if (CurrentState == NewState) return;
    CurrentState = NewState;

    switch (CurrentState)
    {
    case EUnitState::Idle:
        if (GetCharacterMovement())
        {
            // ۱. ترمز اضطراری فیزیکی
            GetCharacterMovement()->StopMovementImmediately(); 
            GetCharacterMovement()->Velocity = FVector::ZeroVector;
            GetCharacterMovement()->ClearAccumulatedForces();
            
            // ۲. تغییر وضعیت به حالت ایستاده در سیستم ناوبری آنریل
            GetCharacterMovement()->SetMovementMode(MOVE_None); 
        }
        
        // ۳. متوقف کردن آنی مونتاژهای انیمیشن بدون زمان محوشدگی طولانی
        if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
        {
            // مقدار 0.05f باعث می‌شود انیمیشن حرکت فورا و بدون تاخیر قطع شود
            AnimInst->Montage_Stop(0.05f); 
        }
        break;

    case EUnitState::Moving_Cluster:
        if (GetCharacterMovement())
        {
            GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        }
        break;

        // بقیه وضعیت‌ها ...
    default: break;
    }
    bHasReachedFinalDestination = false;
}

FVector AUnitCharacter::GetTargetOnPerpendicularLine(float HalfWidth, float& OutDistanceToEnd) const
{
    if (CurrentPath.Num() < 2) 
        return GetActorLocation();

    // ۱. پیدا کردن نزدیک‌ترین قطعه روی مسیر
    int32 BestSeg = 0;
    float BestT = 0.f;
    float MinDistSq = FLT_MAX;
    
    for (int32 i = 0; i < CurrentPath.Num() - 1; i++)
    {
        const FVector& A = CurrentPath[i];
        const FVector& B = CurrentPath[i+1];
        FVector AB = B - A;
        float ABLenSq = AB.SizeSquared();
        if (ABLenSq < SMALL_NUMBER) continue;

        FVector AC = GetActorLocation() - A;
        float t = FMath::Clamp(FVector::DotProduct(AC, AB) / ABLenSq, 0.f, 1.f);
        FVector Closest = A + AB * t;
        float DistSq = (GetActorLocation() - Closest).SizeSquared();
        if (DistSq < MinDistSq)
        {
            MinDistSq = DistSq;
            BestSeg = i;
            BestT = t;
        }
    }

    // ۲. محاسبه دقیق فاصله باقی‌مانده تا انتهای مسیر
    float CurrentLen = 0.f;
    for (int32 i = 0; i < BestSeg; i++)
        CurrentLen += FVector::Dist(CurrentPath[i], CurrentPath[i+1]);
    CurrentLen += BestT * FVector::Dist(CurrentPath[BestSeg], CurrentPath[BestSeg+1]);
    OutDistanceToEnd = TotalPathLength - CurrentLen;

    // ۳. اصلاح نگاه به جلو: جلوگیری از صفر مطلق شدن برای حفظ رفتار خطی
    float ActualLookAhead = LookAheadDistance;
    const float BRAKING_DISTANCE = 400.f; 
    if (OutDistanceToEnd < BRAKING_DISTANCE)
    {
        float T = OutDistanceToEnd / BRAKING_DISTANCE;
        // حداقل ۵۰ واحد نگاه به جلو را حفظ می‌کنیم تا جهت مماس گم نشود
        ActualLookAhead = FMath::Lerp(50.f, LookAheadDistance, T); 
    }
    
    float TargetLen = FMath::Min(CurrentLen + ActualLookAhead, TotalPathLength);
    
    // ۴. پیدا کردن نقطه روی مسیر
    FVector TargetPointOnPath;
    float Accum = 0.f;
    int32 TargetSeg = 0;
    for (int32 i = 0; i < CurrentPath.Num() - 1; i++)
    {
        float SegLen = FVector::Dist(CurrentPath[i], CurrentPath[i+1]);
        if (TargetLen <= Accum + SegLen)
        {
            float t2 = (TargetLen - Accum) / SegLen;
            TargetPointOnPath = CurrentPath[i] + (CurrentPath[i+1] - CurrentPath[i]) * t2;
            TargetSeg = i;
            break;
        }
        Accum += SegLen;
    }

    // ۵. پایدارسازی مماس (Tangent): در قطعه آخر فقط از جهت قطعه آخر استفاده کن 
    // تا لرزش ناشی از میانگین‌گیری با قطعات قبلی حذف شود.
    FVector Tangent;
    if (TargetSeg >= CurrentPath.Num() - 2 || OutDistanceToEnd < BRAKING_DISTANCE)
    {
        Tangent = (CurrentPath.Last() - CurrentPath[CurrentPath.Num() - 2]).GetSafeNormal();
    }
    else
    {
        FVector DirPrev = (CurrentPath[TargetSeg+1] - CurrentPath[TargetSeg]).GetSafeNormal();
        FVector DirNext = (CurrentPath[TargetSeg+2] - CurrentPath[TargetSeg+1]).GetSafeNormal();
        Tangent = (DirPrev + DirNext).GetSafeNormal();
    }
    
    FVector RightDir = FVector::CrossProduct(Tangent, FVector::UpVector).GetSafeNormal();

    // ================== اصلاح کلیدی این بخش ==================
    // خواندن آفست از کامپوننت استیرینگ (جایی که تغییرات فشردگی روی آن اعمال می‌شود)
    float ActiveLateralOffset = DesiredLateralOffset;
    if (SteeringComp)
    {
        ActiveLateralOffset = SteeringComp->GetDesiredLateralOffset();
    }
    // =========================================================

    FVector Target = TargetPointOnPath + RightDir * (ActiveLateralOffset + HalfWidth);
    
    return Target;
}

void AUnitCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // همیشه زمان گذشته از آخرین دستور حرکت را افزایش بده
    TimeSinceLastMoveCommand += DeltaTime;
    
    CurrentSpeed = GetVelocity().Size2D();

    if (CurrentState == EUnitState::Dead || CurrentState == EUnitState::Stunned) return;
    if (!SteeringComp || CurrentState != EUnitState::Moving_Cluster) return;
    if (CurrentPath.Num() < 2) { SetUnitState(EUnitState::Idle); return; }

    // ۱. ذخیره آفست عرضی "قبل" از فشرده‌سازی
    float OldOffset = SteeringComp->GetDesiredLateralOffset();

    float DistanceToEnd = 0.f;
    FVector Target = GetTargetOnPerpendicularLine(0.f, DistanceToEnd); 
    SteeringComp->SetTargetSlot(Target);
    
    // محاسبات سیستم فرار از یونیت‌های متحرک
    SteeringComp->ComputeEvadeDirection();

    // ۲. دریافت مماس واقعی مسیر
    FVector CurrentTangent = (Target - GetActorLocation()).GetSafeNormal();

    // ۳. اجرای فشرده‌سازی (مقدار DesiredLateralOffset در استیرینگ تغییر میکند)
    if (!SteeringComp->IsEvading())
    {
        SteeringComp->UpdateGroupCompression(DeltaTime, CurrentTangent);
    }

    // ۴. ================== تزریق مستقیم موقعیت عرضی ==================
    // تفاوت آفست قبل و بعد از فشرده‌سازی را پیدا می‌کنیم
    float NewOffset = SteeringComp->GetDesiredLateralOffset();
    float OffsetDelta = NewOffset - OldOffset;

    if (!FMath::IsNearlyZero(OffsetDelta, 0.01f))
    {
        // بردار جهت عمود بر مسیر (راست)
        FVector RightDir = FVector::CrossProduct(CurrentTangent, FVector::UpVector).GetSafeNormal();
        
        // جابجایی کاراکتر به صورت مستقیم بر روی محور عرضی بدون درگیر کردن سیستم Velocity
        FVector NewLocation = GetActorLocation() + (RightDir * OffsetDelta);
        
        // جابجایی امن کپسول (با حفظ برخوردها و نرفتن داخل دیوار)
        SetActorLocation(NewLocation, true);
    }
    // ===================================================================

    // ۵. بررسی شرایط توقف نهایی
    float DistanceToTarget = FVector::Dist2D(GetActorLocation(), Target);
    const float ExactStopThreshold = 20.f; 

    if (DistanceToEnd <= ExactStopThreshold || DistanceToTarget <= ExactStopThreshold)
    {
        if (SteeringComp) SteeringComp->ClearTarget();
        SetUnitState(EUnitState::Idle);
        return; 
    }

    // ۶. اعمال نهایی به استیرینگ برای جلو رفتن فیزیکی خالص
    SteeringComp->SetDistanceToGoal(DistanceToTarget);
    SteeringComp->UpdateSteering(DeltaTime);
}

float AUnitCharacter::ComputeInitialProgress(const FVector& Location, const TArray<FVector>& Path)
{
    if (Path.Num() < 2) return 0.f;

    float MinDistSq = FLT_MAX;
    int32 BestSeg = 0;
    float BestT = 0.f; // درصد پیشرفت دقیق روی سگمنت فعلی (بین 0 تا 1)

    // ۱. پیدا کردن دقیق‌ترین قطعه و موقعیت پارامتریک روی آن قطعه
    for (int32 i = 0; i < Path.Num() - 1; i++)
    {
        const FVector& A = Path[i];
        const FVector& B = Path[i+1];
        FVector AB = B - A;
        float ABLenSq = AB.SizeSquared();
        if (ABLenSq < SMALL_NUMBER) continue;

        FVector AC = Location - A;
        // محاسبه تصویر موقعیت کاراکتر روی بردار قطعه مسیر
        float t = FMath::Clamp(FVector::DotProduct(AC, AB) / ABLenSq, 0.f, 1.f);
        FVector Closest = A + AB * t;
        float DistSq = (Location - Closest).SizeSquared();
        
        if (DistSq < MinDistSq)
        {
            MinDistSq = DistSq;
            BestSeg = i;
            BestT = t;
        }
    }

    // ۲. محاسبه دقیق طول کل مسیر طی شده تا قبل از سگمنت فعلی
    float LenBefore = 0.f;
    for (int32 i = 0; i < BestSeg; i++)
    {
        LenBefore += FVector::Dist(Path[i], Path[i+1]);
    }

    // ۳. اضافه کردن دقیقِ مقداری که روی سگمنت فعلی جلو رفته‌ایم
    float CurrentSegLen = FVector::Dist(Path[BestSeg], Path[BestSeg+1]);
    LenBefore += BestT * CurrentSegLen;

    // ۴. محاسبه طول کل مسیر برای به دست آوردن درصد نهایی (Progress)
    float TotalLen = 0.f;
    for (int32 i = 0; i < Path.Num() - 1; i++)
    {
        TotalLen += FVector::Dist(Path[i], Path[i+1]);
    }

    return (TotalLen > 0.f) ? (LenBefore / TotalLen) : 0.f;
}

void AUnitCharacter::SetPathAndMove(const TArray<FVector>& Path)
{
    if (CurrentState == EUnitState::Dead || CurrentState == EUnitState::Stunned) return;
    if (Path.Num() < 2) return;

    // ================== اصلاح روش سپر زمانی ==================
    if (CurrentState == EUnitState::Moving_Cluster && TimeSinceLastMoveCommand < 0.12f)
    {
        CurrentPath = Path;
        FinalGoalLocation = Path.Last();
        
        TotalPathLength = 0.f;
        for (int32 i = 0; i < CurrentPath.Num() - 1; i++)
            TotalPathLength += FVector::Dist(CurrentPath[i], CurrentPath[i+1]);
        
        // خیلی مهم: پروگرس را فورا با مسیر جدید همگام میکنیم تا تابع Tick در فریم بعدی تله‌پورت نکند
        PathProgress = ComputeInitialProgress(GetActorLocation(), CurrentPath);
        
        return; // خروج امن بدون دستکاری سرعت فیزیکی و جهت استیرینگ
    }
    
    TimeSinceLastMoveCommand = 0.f;
    // =========================================================

    bool bWasAlreadyMoving = (CurrentState == EUnitState::Moving_Cluster) && (GetVelocity().Size2D() > 10.f);

    FVector SavedDirection = FVector::ZeroVector;
    if (bWasAlreadyMoving)
    {
        SavedDirection = GetVelocity().GetSafeNormal();
    }

    CurrentPath = Path;
    FinalGoalLocation = Path.Last();

    TotalPathLength = 0.f;
    for (int32 i = 0; i < CurrentPath.Num() - 1; i++)
        TotalPathLength += FVector::Dist(CurrentPath[i], CurrentPath[i+1]);

    PathProgress = ComputeInitialProgress(GetActorLocation(), CurrentPath);

    if (SteeringComp)
    {
        float ReusedOffset = SteeringComp->GetDesiredLateralOffset();
        
        if (!bWasAlreadyMoving || FMath::IsNearlyZero(ReusedOffset, 1.f))
        {
            FVector StartPoint = CurrentPath[0];
            FVector NextPoint = CurrentPath[1];
            FVector Tangent = (NextPoint - StartPoint).GetSafeNormal();
            FVector RightDir = FVector::CrossProduct(Tangent, FVector::UpVector).GetSafeNormal();
            FVector ToUnit = GetActorLocation() - StartPoint;
            ReusedOffset = FVector::DotProduct(ToUnit, RightDir);
        }

        DesiredLateralOffset = ReusedOffset;
        SteeringComp->SetDesiredLateralOffset(DesiredLateralOffset);
        
        if (!SavedDirection.IsNearlyZero() && GetCharacterMovement())
        {
            GetCharacterMovement()->Velocity = SavedDirection * MaxSpeed;
        }
    }

    CurrentTargetWaypointIndex = 1;
    
    if (CurrentState != EUnitState::Moving_Cluster)
    {
        SetUnitState(EUnitState::Moving_Cluster);
    }

    bHasReachedFinalDestination = false;
}

