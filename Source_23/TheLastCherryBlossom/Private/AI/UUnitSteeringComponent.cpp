// UUnitSteeringComponent.cpp

#include "AI/UUnitSteeringComponent.h"
#include "AI/UUnitMovementComponent.h"
#include "Characters/AUnitCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "../TheLastCherryBlossom.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

// ============================================================================
// سازنده و مقداردهی اولیه
// ============================================================================

UUnitSteeringComponent::UUnitSteeringComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UUnitSteeringComponent::Initialize(AUnitCharacter* InOwner)
{
	Owner = InOwner;
	if (!Owner)
	{
		return;
	}

	if (UCharacterMovementComponent* MoveComp = Owner->GetCharacterMovement())
	{
		MoveComp->bUseControllerDesiredRotation = false;
		MoveComp->bOrientRotationToMovement = false;
		MoveComp->bEnablePhysicsInteraction = false;
		MoveComp->bPushForceUsingZOffset = false;
		MoveComp->SetPlaneConstraintEnabled(true);
		MoveComp->SetPlaneConstraintNormal(FVector::UpVector);
	}

	if (UCapsuleComponent* Capsule = Owner->GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
}

// ============================================================================
// مدیریت مسیر / هدف / گروه (API عمومی)
// ============================================================================

void UUnitSteeringComponent::SetPath(const TArray<FVector>& NewPath)
{
	ClearPath();

	if (NewPath.Num() < 2)
	{
		return;
	}

	// اگر یونیت از قبل در حال حرکت بود، سرعت فعلی برای شروع نرم حفظ می‌شود
	const bool bWasAlreadyMoving = bHasTarget && Owner && (Owner->GetVelocity().Size2D() > 10.f);
	const FVector SavedDirection = bWasAlreadyMoving ? Owner->GetVelocity().GetSafeNormal() : FVector::ZeroVector;

	CurrentPath = NewPath;
	UpdatePathCache();
	TotalPathLength = PathCache.TotalLength;
	bHasPath = true;

	// حفظ Offset فعلی نسبت به مسیر جدید در صورت امکان
	float ReusedOffset = DesiredLateralOffset;
	if (!bWasAlreadyMoving || FMath::IsNearlyZero(ReusedOffset, 1.f))
	{
		const FVector StartPoint = CurrentPath[0];
		const FVector NextPoint = CurrentPath[1];
		const FVector Tangent = (NextPoint - StartPoint).GetSafeNormal();
		const FVector RightDir = FVector::CrossProduct(Tangent, FVector::UpVector).GetSafeNormal();
		const FVector ToUnit = Owner->GetActorLocation() - StartPoint;
		ReusedOffset = FVector::DotProduct(ToUnit, RightDir);
	}
	DesiredLateralOffset = ReusedOffset;

	if (Owner && Owner->GetCharacterMovement())
	{
		const FVector BaseTangent = (CurrentPath[1] - CurrentPath[0]).GetSafeNormal();
		const FVector StartMoveDir = bWasAlreadyMoving ? SavedDirection : BaseTangent;
		const float MaxSpeed = Owner->GetCharacterMovement()->MaxWalkSpeed;

		// فقط برای پیوستگی بصری شروع حرکت - نه بخشی از سیستم Movement اصلی
		Owner->GetCharacterMovement()->Velocity = StartMoveDir * MaxSpeed;
	}

	EvadeState = EEvadeState::None;
	bDecisionLocked = false;
	SetSteeringState(ESteeringState::MovingOnPath);
}

void UUnitSteeringComponent::ClearPath()
{
	bHasPath = false;
	CurrentPath.Empty();
	TotalPathLength = 0.f;
	PathCache.Reset();

	bHasTarget = false;
	TargetSlot = FVector::ZeroVector;

	bHasGroupCenter = false;
	GroupCenter = FVector::ZeroVector;
	GroupForward = FVector::ForwardVector;

	DesiredLateralOffset = 0.f;
	EvadeState = EEvadeState::None;
	bDecisionLocked = false;
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

void UUnitSteeringComponent::SetTargetSlot(const FVector& InSlotLocation)
{
	TargetSlot = InSlotLocation;
	bHasTarget = true;
}

void UUnitSteeringComponent::ClearTarget()
{
	bHasTarget = false;
	StopMovement();
}

void UUnitSteeringComponent::ForceStop()
{
	StopMovement();
	ClearPath();
	ClearTarget();
	ClearGroupParams();
	SetSteeringState(ESteeringState::Idle);
}

// ============================================================================
// توابع سازگاری با بقیه‌ی کدبیس
// این‌ها منطق جدیدی اضافه نمی‌کنند - فقط زیرسیستم‌های موجود را از بیرون در دسترس می‌گذارند.
// ============================================================================

void UUnitSteeringComponent::ResetDirection()
{
	// جهت فعلی (اعم از فرار) کنار گذاشته می‌شود تا در Tick بعدی ExecuteMovement
	// وضعیت از نو و بر اساس شرایط فعلی محیط ارزیابی شود.
	EvadeState = EEvadeState::None;
	bDecisionLocked = false;
}

void UUnitSteeringComponent::ResetUnstuckAttempts()
{
	UnstuckAttempts = 0;
	bIsRotatingForUnstuck = false;
}

void UUnitSteeringComponent::AttemptUnstuck(float DeltaTime)
{
	if (!Owner)
	{
		return;
	}

	// اگر یونیت هنوز رسماً وارد چرخه‌ی Stuck/Unstucking نشده، این تابع به‌صورت
	// دستی آن را آغاز می‌کند (مثلاً وقتی سیستم دیگری از بیرون گیر کردن را تشخیص داده).
	if (SteeringState != ESteeringState::Stuck && SteeringState != ESteeringState::Unstucking)
	{
		EnterStuckState();
	}

	TickUnstuck(DeltaTime);
}

// ============================================================================
// 1. PATH FOLLOWING
// این بخش هیچ اطلاعی از موانع ندارد - فقط CurrentPath را می‌بیند.
// ============================================================================

void UUnitSteeringComponent::UpdatePathCache()
{
	PathCache.Reset();

	if (CurrentPath.Num() < 2)
	{
		return;
	}

	float Accumulated = 0.f;
	for (int32 i = 0; i < CurrentPath.Num() - 1; ++i)
	{
		const float SegmentLength = FVector::Dist(CurrentPath[i], CurrentPath[i + 1]);
		if (SegmentLength < SMALL_NUMBER)
		{
			continue;
		}

		PathCache.SegmentLengths.Add(SegmentLength);
		PathCache.AccumulatedLengths.Add(Accumulated);
		Accumulated += SegmentLength;
	}
	PathCache.TotalLength = Accumulated;
}

// قدم ۱: پیدا کردن نزدیک‌ترین نقطه روی مسیر و DistanceAlongPath
float UUnitSteeringComponent::GetDistanceAlongPath(const FVector& Location) const
{
	if (!PathCache.IsValid())
	{
		return 0.f;
	}

	float BestDistance = 0.f;
	float BestDistSq = FLT_MAX;

	for (int32 i = 0; i < CurrentPath.Num() - 1; ++i)
	{
		const FVector& A = CurrentPath[i];
		const FVector& B = CurrentPath[i + 1];
		const FVector AB = B - A;
		const float ABLenSq = AB.SizeSquared();

		if (ABLenSq < SMALL_NUMBER)
		{
			continue;
		}

		const FVector AC = Location - A;
		const float T = FMath::Clamp(FVector::DotProduct(AC, AB) / ABLenSq, 0.f, 1.f);
		const FVector Closest = A + AB * T;
		const float DistSq = FVector::DistSquared(Location, Closest);

		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestDistance = PathCache.AccumulatedLengths[i] + (T * PathCache.SegmentLengths[i]);
		}
	}

	return BestDistance;
}

// قدم ۲: تولید TargetPoint و Tangent روی مسیر بر اساس فاصله‌ی تجمعی
bool UUnitSteeringComponent::GetPointOnPathFast(float DistanceFromStart, FVector& OutPoint, FVector& OutTangent) const
{
	if (!PathCache.IsValid())
	{
		return false;
	}

	DistanceFromStart = FMath::Clamp(DistanceFromStart, 0.f, PathCache.TotalLength);

	for (int32 i = 0; i < PathCache.SegmentLengths.Num(); ++i)
	{
		const float SegStart = PathCache.AccumulatedLengths[i];
		const float SegEnd = SegStart + PathCache.SegmentLengths[i];

		if (DistanceFromStart <= SegEnd)
		{
			const float T = (DistanceFromStart - SegStart) / PathCache.SegmentLengths[i];
			const FVector& A = CurrentPath[i];
			const FVector& B = CurrentPath[i + 1];

			OutPoint = FMath::Lerp(A, B, T);
			OutTangent = (B - A).GetSafeNormal();
			return true;
		}
	}

	return false;
}

// قدم ۳: DistanceToEnd
float UUnitSteeringComponent::GetDistanceToEndFast(const FVector& Location) const
{
	if (!PathCache.IsValid())
	{
		return 0.f;
	}
	return PathCache.TotalLength - GetDistanceAlongPath(Location);
}

// قدم ۴: LookAhead پویا - نزدیک انتهای مسیر کوچک‌تر می‌شود تا توقف نرم باشد
float UUnitSteeringComponent::CalculateDynamicLookAhead(float DistanceToEnd) const
{
	if (DistanceToEnd >= BRAKING_DISTANCE)
	{
		return LookAheadDistance;
	}

	const float T = DistanceToEnd / BRAKING_DISTANCE;
	return FMath::Lerp(MIN_LOOK_AHEAD, LookAheadDistance, T);
}

// اجرای کامل سیستم Path Following: تمام قدم‌های بالا را به ترتیب طی می‌کند
// و DesiredDirection نهایی را تولید می‌کند. هیچ اطلاعی از موانع ندارد.
FPathFollowResult UUnitSteeringComponent::ComputePathFollowing(const FVector& MyPos) const
{
	FPathFollowResult Result;

	Result.DistanceAlongPath = GetDistanceAlongPath(MyPos);
	Result.DistanceToEnd = GetDistanceToEndFast(MyPos);

	if (Result.DistanceToEnd <= STOP_DISTANCE)
	{
		Result.bReachedEnd = true;
		return Result;
	}

	const float LookAhead = CalculateDynamicLookAhead(Result.DistanceToEnd);
	const float TargetDistance = FMath::Clamp(Result.DistanceAlongPath + LookAhead, 0.f, TotalPathLength);

	if (!GetPointOnPathFast(TargetDistance, Result.PathPoint, Result.PathTangent))
	{
		Result.bReachedEnd = true;
		return Result;
	}

	Result.DesiredDirection = (Result.PathPoint - MyPos).GetSafeNormal2D();
	if (Result.DesiredDirection.IsNearlyZero())
	{
		Result.DesiredDirection = Result.PathTangent.GetSafeNormal2D();
	}

	return Result;
}

// ============================================================================
// 2. FORMATION
// Offset همیشه نسبت به Tangent مسیر اعمال می‌شود و به‌مرور فشرده می‌شود.
// فشرده‌سازی کاملاً مستقل از سیستم فرار است.
// ============================================================================

void UUnitSteeringComponent::UpdateFormationCompression(float DeltaTime)
{
	const float MinCompressed = AvoidanceRadius * CompressedOffsetRadiusFactor;

	float TargetOffset = 0.f;
	if (DesiredLateralOffset > 0.f)
	{
		TargetOffset = FMath::Min(DesiredLateralOffset, MinCompressed);
	}
	else if (DesiredLateralOffset < 0.f)
	{
		TargetOffset = FMath::Max(DesiredLateralOffset, -MinCompressed);
	}

	const float RemainingDelta = DesiredLateralOffset - TargetOffset;
	if (FMath::IsNearlyZero(RemainingDelta, 4.0f))
	{
		DesiredLateralOffset = TargetOffset;
		return;
	}

	const float DirectionSign = (RemainingDelta > 0.f) ? -1.f : 1.f;
	const float OffsetChange = DirectionSign * LinearCompressionSpeed * DeltaTime;

	DesiredLateralOffset = (FMath::Abs(OffsetChange) >= FMath::Abs(RemainingDelta))
		? TargetOffset
		: DesiredLateralOffset + OffsetChange;
}

// DesiredTarget = PathPoint + RightVector * DesiredOffset
FVector UUnitSteeringComponent::ComputeFormationDirection(const FVector& MyPos, const FVector& PathPoint,
	const FVector& PathTangent, const FVector& FallbackDirection) const
{
	const FVector RightVector = FVector::CrossProduct(PathTangent, FVector::UpVector).GetSafeNormal();
	const FVector DesiredTarget = PathPoint + RightVector * DesiredLateralOffset;

	FVector Direction = (DesiredTarget - MyPos).GetSafeNormal2D();
	return Direction.IsNearlyZero() ? FallbackDirection : Direction;
}

// ============================================================================
// 3. STATIC OBSTACLE DETECTION
// فقط OverlapSphere. فقط اطلاعات برمی‌گرداند - هیچ تصمیمی نمی‌گیرد.
// ============================================================================

float UUnitSteeringComponent::GetStaticScanRadius() const
{
	if (!Owner)
	{
		return StaticMinScanRadius;
	}

	const float Speed = Owner->GetVelocity().Size2D();
	const float Radius = StaticMinScanRadius + (Speed * StaticSpeedToRadiusFactor);
	return FMath::Clamp(Radius, StaticMinScanRadius, StaticMaxScanRadius);
}

float UUnitSteeringComponent::GetStaticFOVDotThreshold() const
{
	const float HalfAngleRadians = FMath::DegreesToRadians(StaticFieldOfViewAngle * 0.5f);
	return FMath::Cos(HalfAngleRadians);
}

FStaticObstacleResult UUnitSteeringComponent::DetectStaticObstacles(const FVector& MyPos, const FVector& DesiredDirection) const
{
	FStaticObstacleResult Result;

	if (!Owner || DesiredDirection.IsNearlyZero())
	{
		return Result;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return Result;
	}

	FCollisionObjectQueryParams ObjQuery;
	ObjQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjQuery.AddObjectTypesToQuery(ECC_RTS_Obstacle);
	ObjQuery.AddObjectTypesToQuery(ECC_GameTraceChannel2);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Owner);

	TArray<FOverlapResult> Hits;
	World->OverlapMultiByObjectType(
		Hits, MyPos, FQuat::Identity, ObjQuery,
		FCollisionShape::MakeSphere(GetStaticScanRadius()), Params);

	const FVector RightVector = FVector::CrossProduct(DesiredDirection, FVector::UpVector).GetSafeNormal();
	const float DotThreshold = GetStaticFOVDotThreshold();

	for (const FOverlapResult& Hit : Hits)
	{
		AActor* Obstacle = Hit.GetActor();
		if (!Obstacle || Obstacle == Owner)
		{
			continue;
		}

		FVector ToObstacle = Obstacle->GetActorLocation() - MyPos;
		ToObstacle.Z = 0.f;

		const float Distance = ToObstacle.Size2D();
		if (Distance < KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector DirToObstacle = ToObstacle / Distance;
		if (FVector::DotProduct(DesiredDirection, DirToObstacle) < DotThreshold)
		{
			continue;
		}

		// مانعی داخل زاویه دید و جلوی یونیت پیدا شد
		Result.bFrontBlocked = true;
		Result.bLeftFree = IsPathClear(MyPos - RightVector * AvoidanceRadius, DesiredDirection, AvoidanceRadius, EEvadeSource::Static);
		Result.bRightFree = IsPathClear(MyPos + RightVector * AvoidanceRadius, DesiredDirection, AvoidanceRadius, EEvadeSource::Static);
		break;
	}

	return Result;
}

// ============================================================================
// 4. DYNAMIC UNIT DETECTION
// فقط OverlapSphere. یونیت‌های Dead/Idle/هم‌جهت-هم‌سرعت نادیده گرفته می‌شوند.
// ============================================================================

float UUnitSteeringComponent::GetDynamicScanRadius() const
{
	if (!Owner)
	{
		return DynamicMinScanRadius;
	}

	const float Speed = Owner->GetVelocity().Size2D();
	const float Radius = DynamicMinScanRadius + (Speed * DynamicSpeedToRadiusFactor);
	return FMath::Clamp(Radius, DynamicMinScanRadius, DynamicMaxScanRadius);
}

float UUnitSteeringComponent::GetDynamicFOVDotThreshold() const
{
	const float HalfAngleRadians = FMath::DegreesToRadians(DynamicFieldOfViewAngle * 0.5f);
	return FMath::Cos(HalfAngleRadians);
}

bool UUnitSteeringComponent::IsUnitMovingRelevantly(const AUnitCharacter* Unit) const
{
	if (!Unit)
	{
		return false;
	}

	const EUnitState State = Unit->GetUnitState();
	if (State == EUnitState::Idle || State == EUnitState::Dead || State == EUnitState::Stunned)
	{
		return false;
	}

	return Unit->GetVelocity().Size2D() >= MinMovementSpeedForAvoidance;
}

bool UUnitSteeringComponent::ShouldIgnoreUnit(const AUnitCharacter* OtherUnit, const FVector& MyVelocity) const
{
	if (!IsUnitMovingRelevantly(OtherUnit))
	{
		return true;
	}

	const FVector OtherVel = OtherUnit->GetVelocity();
	const float OtherSpeed = OtherVel.Size2D();
	const float MySpeed = MyVelocity.Size2D();

	if (MySpeed > 10.f && OtherSpeed > 10.f)
	{
		const float DotProduct = FVector::DotProduct(MyVelocity.GetSafeNormal(), OtherVel.GetSafeNormal());
		if (DotProduct > SameDirectionDotThreshold)
		{
			const float SpeedRatio = OtherSpeed / MySpeed;
			if (SpeedRatio > 0.7f && SpeedRatio < 1.3f)
			{
				// یونیت تقریباً هم‌جهت و هم‌سرعت است - نادیده گرفته می‌شود
				return true;
			}
		}
	}

	return false;
}

FDynamicUnitResult UUnitSteeringComponent::DetectDynamicUnits(const FVector& MyPos, const FVector& MyVelocity,
	const FVector& DesiredDirection) const
{
	FDynamicUnitResult Result;

	if (!Owner)
	{
		return Result;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return Result;
	}

	FCollisionObjectQueryParams ObjQuery;
	ObjQuery.AddObjectTypesToQuery(ECC_GameTraceChannel4);
	ObjQuery.AddObjectTypesToQuery(ECC_GameTraceChannel5);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Owner);

	TArray<FOverlapResult> Hits;
	World->OverlapMultiByObjectType(
		Hits, MyPos, FQuat::Identity, ObjQuery,
		FCollisionShape::MakeSphere(GetDynamicScanRadius()), Params);

	const FVector RightVector = FVector::CrossProduct(DesiredDirection, FVector::UpVector).GetSafeNormal();
	const float DotThreshold = GetDynamicFOVDotThreshold();

	for (const FOverlapResult& Hit : Hits)
	{
		AUnitCharacter* Other = Cast<AUnitCharacter>(Hit.GetActor());
		if (!Other || Other == Owner || ShouldIgnoreUnit(Other, MyVelocity))
		{
			continue;
		}

		FVector ToOther = Other->GetActorLocation() - MyPos;
		ToOther.Z = 0.f;

		const float Distance = ToOther.Size2D();
		if (Distance < KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector DirToOther = ToOther / Distance;
		if (FVector::DotProduct(DesiredDirection, DirToOther) < DotThreshold)
		{
			continue;
		}

		Result.bFrontBlocked = true;

		const float Side = FVector::DotProduct(RightVector, DirToOther);
		if (Side > 0.f)
		{
			Result.bRightFree = false;
		}
		else
		{
			Result.bLeftFree = false;
		}
	}

	return Result;
}

// ============================================================================
// IsPathClear - فقط یک سؤال را جواب می‌دهد
// ============================================================================

bool UUnitSteeringComponent::IsPathClear(const FVector& FromPos, const FVector& Direction,
	float CheckDistance, EEvadeSource Source) const
{
	if (!Owner || Direction.IsNearlyZero())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionObjectQueryParams ObjQuery;
	switch (Source)
	{
	case EEvadeSource::Static:
		ObjQuery.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjQuery.AddObjectTypesToQuery(ECC_RTS_Obstacle);
		ObjQuery.AddObjectTypesToQuery(ECC_GameTraceChannel2);
		break;

	case EEvadeSource::Dynamic:
		ObjQuery.AddObjectTypesToQuery(ECC_GameTraceChannel4);
		ObjQuery.AddObjectTypesToQuery(ECC_GameTraceChannel5);
		break;

	case EEvadeSource::Both:
		ObjQuery.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjQuery.AddObjectTypesToQuery(ECC_RTS_Obstacle);
		ObjQuery.AddObjectTypesToQuery(ECC_GameTraceChannel2);
		ObjQuery.AddObjectTypesToQuery(ECC_GameTraceChannel4);
		ObjQuery.AddObjectTypesToQuery(ECC_GameTraceChannel5);
		break;

	default:
		return true;
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Owner);

	const FVector End = FromPos + Direction.GetSafeNormal2D() * CheckDistance;

	TArray<FHitResult> Hits;
	const bool bHit = World->SweepMultiByObjectType(
		Hits, FromPos, End, FQuat::Identity, ObjQuery,
		FCollisionShape::MakeSphere(AvoidanceRadius * 0.9f), Params);

	if (!bHit)
	{
		return true;
	}

	for (const FHitResult& Hit : Hits)
	{
		AActor* Actor = Hit.GetActor();
		if (!Actor || Actor == Owner)
		{
			continue;
		}

		if (const AUnitCharacter* Unit = Cast<AUnitCharacter>(Actor))
		{
			if (Unit->GetUnitState() == EUnitState::Dead)
			{
				continue;
			}
		}

		return false;
	}

	return true;
}

// ============================================================================
// 5. EVADE DECISION
// بدون Timer، بدون شمارنده. فقط یک‌بار تصمیم می‌گیرد و تا باز شدن مسیر
// مستقیم آن را قفل نگه می‌دارد - هرگز چپ/راست را عوض نمی‌کند.
// ============================================================================

EEvadeState UUnitSteeringComponent::DecideEvadeDirection(bool bLeftFree, bool bRightFree,
	const FVector& MyPos, const FVector& DesiredDirection) const
{
	if (bLeftFree && !bRightFree)
	{
		return EEvadeState::Left;
	}

	if (!bLeftFree && bRightFree)
	{
		return EEvadeState::Right;
	}

	if (!bLeftFree && !bRightFree)
	{
		return EEvadeState::Blocked;
	}

	// هر دو سمت آزادند - فقط همین یک‌بار با یک بررسی اضافه تصمیم می‌گیریم
	const FVector RightVector = FVector::CrossProduct(DesiredDirection, FVector::UpVector).GetSafeNormal();
	const FVector LeftProbe = MyPos - RightVector * AvoidanceRadius;
	const FVector RightProbe = MyPos + RightVector * AvoidanceRadius;

	const bool bLeftClear = IsPathClear(LeftProbe, DesiredDirection, AvoidanceRadius * 2.f, EEvadeSource::Both);
	const bool bRightClear = IsPathClear(RightProbe, DesiredDirection, AvoidanceRadius * 2.f, EEvadeSource::Both);

	if (bLeftClear && !bRightClear)
	{
		return EEvadeState::Left;
	}
	if (bRightClear && !bLeftClear)
	{
		return EEvadeState::Right;
	}

	// تساوی کامل - انتخاب پیش‌فرض پایدار و قابل پیش‌بینی
	return EEvadeState::Right;
}

void UUnitSteeringComponent::UpdateEvadeDecision(bool bBlocked, bool bLeftFree, bool bRightFree,
	const FVector& MyPos, const FVector& DesiredDirection)
{
	// مسیر مستقیم باز است -> هر حالت فراری پاک می‌شود و تصمیم آزاد می‌شود
	if (!bBlocked)
	{
		if (EvadeState != EEvadeState::None)
		{
			EvadeState = EEvadeState::None;
			bDecisionLocked = false;
			SetSteeringState(ESteeringState::MovingOnPath);
		}
		return;
	}

	// مانعی هست ولی تصمیم قبلاً گرفته شده -> هیچ تغییری نده، تصمیم حفظ می‌شود
	if (bDecisionLocked)
	{
		return;
	}

	// اولین باری که مانع دیده می‌شود -> فقط یک‌بار تصمیم بگیر و قفل کن
	EvadeState = DecideEvadeDirection(bLeftFree, bRightFree, MyPos, DesiredDirection);
	bDecisionLocked = true;
	SetSteeringState(ESteeringState::Evading);
}

// ============================================================================
// 6. STEERING DECISION
// ورودی: DesiredDirection + EvadeState فعلی -> خروجی: FinalDirection
// ============================================================================

FVector UUnitSteeringComponent::ComputeSteeringDirection(const FVector& DesiredDirection) const
{
	if (EvadeState == EEvadeState::None)
	{
		return DesiredDirection;
	}

	if (EvadeState == EEvadeState::Blocked)
	{
		return FVector::ZeroVector;
	}

	const FVector RightVector = FVector::CrossProduct(DesiredDirection, FVector::UpVector).GetSafeNormal();
	const FVector EvadeDirection = (EvadeState == EEvadeState::Left) ? -RightVector : RightVector;

	const FVector Blended = (DesiredDirection + EvadeDirection * EvadeWeight).GetSafeNormal2D();
	return Blended.IsNearlyZero() ? DesiredDirection : Blended;
}

// ============================================================================
// 7. MOVEMENT
// تنها جایی که MoveDirection و StopImmediately فراخوانی می‌شوند.
// ============================================================================

void UUnitSteeringComponent::ApplyFinalMovement(const FVector& FinalDirection)
{
	if (!Owner || !Owner->UnitMovement)
	{
		return;
	}

	Owner->UnitMovement->MoveDirection(FinalDirection);

	if (SteeringState == ESteeringState::Idle)
	{
		SetSteeringState(ESteeringState::MovingOnPath);
	}
}

void UUnitSteeringComponent::StopMovement()
{
	if (Owner && Owner->UnitMovement)
	{
		Owner->UnitMovement->StopImmediately();
	}
}

void UUnitSteeringComponent::HandlePathCompleted()
{
	EvadeState = EEvadeState::None;
	bDecisionLocked = false;
	StopMovement();
	SetSteeringState(ESteeringState::Idle);
}

// ============================================================================
// 8. STUCK DETECTION
// ============================================================================

bool UUnitSteeringComponent::UpdateStuckTimer(float DeltaTime, float CurrentSpeed)
{
	if (!bHasTarget || !Owner)
	{
		StuckTimer = 0.f;
		return false;
	}

	if (CurrentSpeed >= StuckSpeedThreshold)
	{
		StuckTimer = 0.f;
		StuckReferencePosition = Owner->GetActorLocation();
		StuckReferenceCheckTime = GetWorld()->GetTimeSeconds();
		return false;
	}

	StuckTimer += DeltaTime;

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - StuckReferenceCheckTime >= StuckPositionCheckInterval)
	{
		const FVector CurrentPos = Owner->GetActorLocation();
		const float DistanceMoved = FVector::Dist2D(CurrentPos, StuckReferencePosition);

		StuckReferencePosition = CurrentPos;
		StuckReferenceCheckTime = CurrentTime;

		if (DistanceMoved < StuckMinDistanceMoved)
		{
			return true;
		}
	}

	return StuckTimer >= StuckTimeThreshold;
}

void UUnitSteeringComponent::EnterStuckState()
{
	SetSteeringState(ESteeringState::Stuck);
	StopMovement();
}

void UUnitSteeringComponent::TickUnstuck(float DeltaTime)
{
	if (!Owner)
	{
		return;
	}

	if (SteeringState == ESteeringState::Stuck)
	{
		StartUnstuckAttempt();
		return;
	}

	// SteeringState == Unstucking
	if (bIsRotatingForUnstuck)
	{
		if (RotateTowardUnstuckTarget(DeltaTime))
		{
			bIsRotatingForUnstuck = false;
		}
		return;
	}

	const float Elapsed = GetWorld()->GetTimeSeconds() - UnstuckStartTime;
	if (Elapsed >= UnstuckTimeout)
	{
		EvaluateUnstuckResult();
		return;
	}

	MoveTowardUnstuckTarget();
}

void UUnitSteeringComponent::StartUnstuckAttempt()
{
	// الگوهای زاویه/فاصله برای تلاش‌های متوالی - بعد از این تعداد، تصادفی می‌شود
	static const float Angles[]    = { 180.f, 135.f, 225.f, 90.f, 270.f };
	static const float Distances[] = {  80.f, 100.f, 100.f, 120.f, 120.f };
	constexpr int32 NumPredefinedAttempts = UE_ARRAY_COUNT(Angles);

	float Angle;
	float Distance;
	if (UnstuckAttempts < NumPredefinedAttempts)
	{
		Angle = Angles[UnstuckAttempts];
		Distance = Distances[UnstuckAttempts];
	}
	else
	{
		Angle = FMath::FRandRange(0.f, 360.f);
		Distance = 150.f;
	}

	const FVector MyPos = Owner->GetActorLocation();
	const FVector ReferenceDir = bHasTarget
		? (TargetSlot - MyPos).GetSafeNormal2D()
		: Owner->GetActorForwardVector();

	const float BaseAngle = FMath::Atan2(ReferenceDir.Y, ReferenceDir.X);
	const float NewAngle = BaseAngle + FMath::DegreesToRadians(Angle);

	UnstuckDirection = FVector(FMath::Cos(NewAngle), FMath::Sin(NewAngle), 0.f).GetSafeNormal();
	UnstuckTargetLocation = MyPos + UnstuckDirection * Distance;

	UnstuckTargetRotation = UnstuckDirection.Rotation();
	UnstuckTargetRotation.Pitch = 0.f;
	UnstuckTargetRotation.Roll = 0.f;

	bIsRotatingForUnstuck = true;
	UnstuckStartTime = GetWorld()->GetTimeSeconds();
	UnstuckAttempts++;

	SetSteeringState(ESteeringState::Unstucking);
}

bool UUnitSteeringComponent::RotateTowardUnstuckTarget(float DeltaTime)
{
	const FRotator CurrentRotation = Owner->GetActorRotation();
	const FRotator NewRotation = FMath::RInterpTo(CurrentRotation, UnstuckTargetRotation, DeltaTime, UnstuckRotationSpeed);

	Owner->SetActorRotation(NewRotation);

	return NewRotation.Equals(UnstuckTargetRotation, 1.f);
}

void UUnitSteeringComponent::MoveTowardUnstuckTarget()
{
	const FVector MyPos = Owner->GetActorLocation();
	const float DistanceToTarget = FVector::Dist2D(MyPos, UnstuckTargetLocation);

	if (DistanceToTarget <= STOP_DISTANCE)
	{
		EvaluateUnstuckResult();
		return;
	}

	const FVector MoveDir = (UnstuckTargetLocation - MyPos).GetSafeNormal2D();

	if (!IsPathClear(MyPos, MoveDir, AvoidanceRadius, EEvadeSource::Both))
	{
		StopMovement();
		return;
	}

	ApplyFinalMovement(MoveDir);
}

void UUnitSteeringComponent::EvaluateUnstuckResult()
{
	const FVector MyPos = Owner->GetActorLocation();
	const float DistanceToTarget = FVector::Dist2D(MyPos, UnstuckTargetLocation);
	const float CurrentSpeed = Owner->UnitMovement ? Owner->UnitMovement->GetVelocity().Size2D() : 0.f;

	const bool bReachedTarget = DistanceToTarget < STOP_DISTANCE * 1.5f;
	const bool bIsMoving = CurrentSpeed > StuckSpeedThreshold;
	const bool bMadeProgress = DistanceToTarget < UnstuckMoveDistance * 0.5f;
	const bool bSuccess = bReachedTarget || bIsMoving || bMadeProgress;

	if (bSuccess)
	{
		UnstuckAttempts = 0;
		StuckTimer = 0.f;
		RestorePathAfterUnstuck();
		return;
	}

	if (UnstuckAttempts < MaxUnstuckAttempts)
	{
		StartUnstuckAttempt();
		return;
	}

	// تمام تلاش‌ها ناموفق بود - یونیت کاملاً متوقف و بی‌کار می‌شود
	UnstuckAttempts = 0;
	StopMovement();
	ClearTarget();
	SetSteeringState(ESteeringState::Idle);
}

void UUnitSteeringComponent::RestorePathAfterUnstuck()
{
	if (!bHasPath || !PathCache.IsValid())
	{
		SetSteeringState(ESteeringState::Idle);
		return;
	}

	// حرکت روی مسیر اصلی به‌صورت عادی در Tick بعدی توسط ExecuteMovement از سر گرفته می‌شود
	SetSteeringState(ESteeringState::MovingOnPath);
}

void UUnitSteeringComponent::SetSteeringState(ESteeringState NewState)
{
	if (SteeringState == NewState)
	{
		return;
	}

	SteeringState = NewState;

	if (!Owner)
	{
		return;
	}

	// همگام‌سازی با EUnitState بیرونی (برای سازگاری با بقیه‌ی سیستم‌های بازی)
	switch (NewState)
	{
	case ESteeringState::Idle:
		Owner->SetUnitState(EUnitState::Idle);
		break;

	case ESteeringState::MovingOnPath:
	case ESteeringState::Evading:
		Owner->SetUnitState(EUnitState::Moving);
		break;

	case ESteeringState::Stuck:
	case ESteeringState::Unstucking:
		Owner->SetUnitState(EUnitState::Stuck);
		break;
	}
}

void UUnitSteeringComponent::ExecuteMovement(float DeltaTime)
{
	// ---------- Validate ----------
	if (!Owner || !Owner->UnitMovement || !bHasPath || CurrentPath.Num() < 2 || !PathCache.IsValid())
	{
		StopMovement();
		return;
	}

	// ---------- Handle Stuck ----------
	if (SteeringState == ESteeringState::Stuck || SteeringState == ESteeringState::Unstucking)
	{
		TickUnstuck(DeltaTime);
		return;
	}

	const FVector MyPos = Owner->GetActorLocation();
	const FVector MyVelocity = Owner->UnitMovement->GetVelocity();

	if (UpdateStuckTimer(DeltaTime, MyVelocity.Size2D()))
	{
		EnterStuckState();
		TickUnstuck(DeltaTime);
		return;
	}

	// ---------- Path Following ----------
	const FPathFollowResult PathResult = ComputePathFollowing(MyPos);

	if (PathResult.bReachedEnd)
	{
		HandlePathCompleted();
		return;
	}

	// ---------- Formation ----------
	UpdateFormationCompression(DeltaTime);
	const FVector DesiredDirection = ComputeFormationDirection(
		MyPos, PathResult.PathPoint, PathResult.PathTangent, PathResult.DesiredDirection);

	// ---------- Static Scan ----------
	const FStaticObstacleResult StaticResult = DetectStaticObstacles(MyPos, DesiredDirection);

	// ---------- Dynamic Scan ----------
	const FDynamicUnitResult DynamicResult = DetectDynamicUnits(MyPos, MyVelocity, DesiredDirection);

	const bool bBlocked = StaticResult.bFrontBlocked || DynamicResult.bFrontBlocked;
	const bool bLeftFree = StaticResult.bLeftFree && DynamicResult.bLeftFree;
	const bool bRightFree = StaticResult.bRightFree && DynamicResult.bRightFree;

	// ---------- Evade Decision ----------
	UpdateEvadeDecision(bBlocked, bLeftFree, bRightFree, MyPos, DesiredDirection);

	// ---------- Steering ----------
	const FVector FinalDirection = ComputeSteeringDirection(DesiredDirection);

	if (FinalDirection.IsNearlyZero())
	{
		StopMovement();
		return;
	}

	// ---------- Move ----------
	ApplyFinalMovement(FinalDirection);
}