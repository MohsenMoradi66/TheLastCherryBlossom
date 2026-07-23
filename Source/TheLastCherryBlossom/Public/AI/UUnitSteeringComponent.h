// UUnitSteeringComponent.h
//
// این کامپوننت مسئول محاسبه‌ی «جهت حرکت نهایی» یونیت روی یک Path از پیش
// آماده‌شده است. مسیریابی (Pathfinding) کار این کلاس نیست؛ Path از بیرون
// داده می‌شود و این سیستم فقط یونیت را روی آن هدایت می‌کند، از موانع ثابت
// و متحرک عبور می‌دهد، فرمیشن را حفظ می‌کند و در صورت گیر کردن خودش را
// آزاد می‌کند.
//
// طراحی بر اساس ۸ زیرسیستم مستقل است:
//   Path Following / Formation / Static Obstacle Detection /
//   Dynamic Unit Detection / Evade Decision / Steering Decision /
//   Movement / Stuck & Unstuck
//
// ExecuteMovement() فقط نقش هماهنگ‌کننده دارد و هیچ منطقی داخل آن نیست.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UUnitSteeringComponent.generated.h"

class AUnitCharacter;

// ==============================================================
// منبع موانع - برای انتخاب Collision Channel داخل IsPathClear
// ==============================================================
UENUM()
enum class EEvadeSource : uint8
{
	None,
	Static,
	Dynamic,
	Both
};

// ==============================================================
// وضعیت داخلی سیستم فرار (Evade)
// این حالت فقط توسط UpdateEvadeDecision تغییر می‌کند و هیچ Timer ندارد.
// ==============================================================
UENUM()
enum class EEvadeState : uint8
{
	None,	// هیچ فراری فعال نیست - مسیر مستقیم باز است
	Left,	// تصمیم قفل‌شده: فرار به چپ
	Right,	// تصمیم قفل‌شده: فرار به راست
	Blocked	// هر دو سمت بسته است - توقف کامل
};

// ==============================================================
// ماشین حالت اصلی سیستم Steering.
// تمام حالت‌های ممکن باید اینجا صراحتاً وجود داشته باشند - هیچ حالت
// مخفی یا ضمنی مجاز نیست.
//
//   Idle -> Moving -> Evading -> Moving -> Stuck -> Unstucking -> Moving
// ==============================================================
UENUM(BlueprintType)
enum class ESteeringState : uint8
{
	Idle,			// بدون مسیر / بدون هدف
	MovingOnPath,	// دنبال کردن عادی مسیر، بدون مانع
	Evading,		// در حال فرار از مانع (تصمیم قفل‌شده)
	Stuck,			// گیر کردن تشخیص داده شده
	Unstucking		// در حال اجرای روال رفع گیر
};

// خروجی سیستم تشخیص موانع ثابت - فقط اطلاعات، بدون هیچ تصمیمی
struct FStaticObstacleResult
{
	bool bFrontBlocked = false;
	bool bLeftFree = true;
	bool bRightFree = true;
};

// خروجی سیستم تشخیص یونیت‌های متحرک - فقط اطلاعات، بدون هیچ تصمیمی
struct FDynamicUnitResult
{
	bool bFrontBlocked = false;
	bool bLeftFree = true;
	bool bRightFree = true;
};

// خروجی سیستم Path Following
struct FPathFollowResult
{
	FVector DesiredDirection = FVector::ForwardVector;
	FVector PathPoint = FVector::ZeroVector;
	FVector PathTangent = FVector::ForwardVector;
	float DistanceAlongPath = 0.f;
	float DistanceToEnd = 0.f;
	bool bReachedEnd = false;
};

// کش پیش‌محاسبه‌شده‌ی طول قطعات مسیر - از محاسبه‌ی تکراری هر Tick جلوگیری می‌کند
struct FPathCache
{
	TArray<float> SegmentLengths;
	TArray<float> AccumulatedLengths;
	float TotalLength = 0.f;

	void Reset()
	{
		SegmentLengths.Empty();
		AccumulatedLengths.Empty();
		TotalLength = 0.f;
	}

	bool IsValid() const { return SegmentLengths.Num() > 0; }
};

/**
 * UUnitSteeringComponent
 *
 * مسئولیت این کلاس: محاسبه‌ی جهت حرکت نهایی یونیت روی یک Path داده‌شده.
 * برای صدها یونیت هم‌زمان طراحی شده - بدون تخصیص حافظه در Tick و بدون
 * الگوریتم‌های سنگین (Boids / RVO / Detour Crowd استفاده نمی‌شود).
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THELASTCHERRYBLOSSOM_API UUnitSteeringComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUnitSteeringComponent();

	// ==================== API عمومی ====================

	void Initialize(AUnitCharacter* InOwner);

	/** تنها نقطه‌ی ورودی سیستم - هر Tick یک‌بار فراخوانی می‌شود. فقط هماهنگ‌کننده است. */
	void ExecuteMovement(float DeltaTime);

	void SetPath(const TArray<FVector>& NewPath);
	void ClearPath();

	void SetGroupParams(const FVector& InCenter, const FVector& InForward);
	void ClearGroupParams();

	void SetDesiredLateralOffset(float NewOffset) { DesiredLateralOffset = NewOffset; }
	float GetDesiredLateralOffset() const { return DesiredLateralOffset; }

	void SetTargetSlot(const FVector& InSlotLocation);
	void ClearTarget();

	/** توقف کامل و پاک کردن همه‌ی وضعیت‌های داخلی (مسیر، هدف، فرار، فرمیشن) */
	void ForceStop();

	bool IsMoving() const { return bHasPath && bHasTarget; }
	bool IsEvading() const { return SteeringState == ESteeringState::Evading; }
	bool IsStuck() const { return SteeringState == ESteeringState::Stuck || SteeringState == ESteeringState::Unstucking; }
	ESteeringState GetSteeringState() const { return SteeringState; }

	// ---- توابع سازگاری با بقیه‌ی کدبیس (AUnitCharacter و غیره) ----
	// این توابع منطق جدید ندارند؛ فقط Wrapper نازک روی زیرسیستم‌های داخلی هستند.

	/** حالت فرار فعلی را پاک می‌کند تا در Tick بعدی از نو ارزیابی شود */
	void ResetDirection();

	/** شمارنده‌ی تلاش‌های رفع گیر را صفر می‌کند - برای شروع تازه از بیرون */
	void ResetUnstuckAttempts();

	/** به‌صورت دستی روال رفع گیر را شروع/ادامه می‌دهد (مثلاً وقتی از بیرون Stuck تشخیص داده شده) */
	void AttemptUnstuck(float DeltaTime);

protected:
	// ==================== اندازه و حرکت پایه ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance")
	float AvoidanceRadius = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Movement")
	float LookAheadDistance = 150.f;

	/** وزن بردار فرار هنگام ترکیب با DesiredDirection در Steering Decision */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Movement")
	float EvadeWeight = 1.0f;

	// ==================== فیلتر یونیت‌های متحرک ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Dynamic")
	float MinMovementSpeedForAvoidance = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Dynamic")
	float SameDirectionDotThreshold = 0.8f;

	// ==================== موانع ثابت (Static) ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Static")
	float StaticFieldOfViewAngle = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Static")
	float StaticMinScanRadius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Static")
	float StaticMaxScanRadius = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Static")
	float StaticSpeedToRadiusFactor = 0.5f;

	// ==================== یونیت‌های متحرک (Dynamic) ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Dynamic")
	float DynamicFieldOfViewAngle = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Dynamic")
	float DynamicMinScanRadius = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Dynamic")
	float DynamicMaxScanRadius = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Dynamic")
	float DynamicSpeedToRadiusFactor = 0.4f;

	// ==================== تشخیص گیر کردن ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Stuck")
	float StuckSpeedThreshold = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Stuck")
	float StuckTimeThreshold = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Stuck")
	float StuckPositionCheckInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Stuck")
	float StuckMinDistanceMoved = 3.f;

	// ==================== رفع گیر (Unstuck) ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Unstuck")
	float UnstuckRotationSpeed = 360.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Unstuck")
	float UnstuckMoveDistance = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Unstuck")
	float UnstuckTimeout = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Unstuck")
	int32 MaxUnstuckAttempts = 4;

	// ==================== فرمیشن ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Formation")
	float CompressedOffsetRadiusFactor = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Formation")
	float LinearCompressionSpeed = 8.f;

private:
	// ========================================================
	// ------------------- 1. Path Following -------------------
	// این بخش هیچ اطلاعی از موانع ندارد. فقط ورودی آن CurrentPath است.
	// ========================================================
	void UpdatePathCache();
	float GetDistanceAlongPath(const FVector& Location) const;
	bool GetPointOnPathFast(float DistanceFromStart, FVector& OutPoint, FVector& OutTangent) const;
	float GetDistanceToEndFast(const FVector& Location) const;
	float CalculateDynamicLookAhead(float DistanceToEnd) const;

	/** تمام مراحل Path Following را به ترتیب اجرا و DesiredDirection را برمی‌گرداند */
	FPathFollowResult ComputePathFollowing(const FVector& MyPos) const;

	// ========================================================
	// --------------------- 2. Formation -----------------------
	// فشرده‌سازی Offset مستقل از سیستم فرار است.
	// ========================================================
	void UpdateFormationCompression(float DeltaTime);
	FVector ComputeFormationDirection(const FVector& MyPos, const FVector& PathPoint,
		const FVector& PathTangent, const FVector& FallbackDirection) const;

	// ========================================================
	// ------------- 3. Static Obstacle Detection ---------------
	// فقط OverlapSphere. فقط اطلاعات برمی‌گرداند، تصمیم نمی‌گیرد.
	// ========================================================
	float GetStaticScanRadius() const;
	float GetStaticFOVDotThreshold() const;
	FStaticObstacleResult DetectStaticObstacles(const FVector& MyPos, const FVector& DesiredDirection) const;

	// ========================================================
	// -------------- 4. Dynamic Unit Detection -------------------
	// فقط OverlapSphere. یونیت‌های Dead/Idle/هم‌جهت نادیده گرفته می‌شوند.
	// ========================================================
	float GetDynamicScanRadius() const;
	float GetDynamicFOVDotThreshold() const;
	bool IsUnitMovingRelevantly(const AUnitCharacter* Unit) const;
	bool ShouldIgnoreUnit(const AUnitCharacter* OtherUnit, const FVector& MyVelocity) const;
	FDynamicUnitResult DetectDynamicUnits(const FVector& MyPos, const FVector& MyVelocity,
		const FVector& DesiredDirection) const;

	// ========================================================
	// ------------------ 5. Evade Decision -----------------------
	// بدون Timer، بدون شمارنده، بدون تصمیم‌گیری مجدد در هر Tick.
	// ========================================================

	/** وضعیت فرار را بر اساس بسته/باز بودن مسیر به‌روزرسانی می‌کند - فقط یک‌بار تصمیم می‌گیرد و آن را قفل نگه می‌دارد */
	void UpdateEvadeDecision(bool bBlocked, bool bLeftFree, bool bRightFree,
		const FVector& MyPos, const FVector& DesiredDirection);

	/** فقط در لحظه‌ی قفل شدن تصمیم فراخوانی می‌شود - Left/Right/Blocked را انتخاب می‌کند */
	EEvadeState DecideEvadeDirection(bool bLeftFree, bool bRightFree,
		const FVector& MyPos, const FVector& DesiredDirection) const;

	// ========================================================
	// ----------------- 6. Steering Decision ------------------------
	// ورودی: DesiredDirection + EvadeState فعلی -> خروجی: FinalDirection
	// ========================================================
	FVector ComputeSteeringDirection(const FVector& DesiredDirection) const;

	// ========================================================
	// -------------------- IsPathClear ------------------------------
	// فقط یک سؤال را جواب می‌دهد: آیا این مسیر آزاد است؟
	// ========================================================
	bool IsPathClear(const FVector& FromPos, const FVector& Direction,
		float CheckDistance, EEvadeSource Source) const;

	// ========================================================
	// ------------------------ 7. Movement ----------------------------
	// تنها جایی که MoveDirection/StopImmediately فراخوانی می‌شوند.
	// ========================================================
	void ApplyFinalMovement(const FVector& FinalDirection);
	void StopMovement();
	void HandlePathCompleted();

	// ========================================================
	// ------------------- 8. Stuck Detection --------------------------
	// ========================================================
	bool UpdateStuckTimer(float DeltaTime, float CurrentSpeed);
	void EnterStuckState();

	// ========================================================
	// ---------------------- 8. Unstuck --------------------------------
	// چرخش -> حرکت به نقطه‌ی خروج -> بررسی موفقیت -> بازگشت به مسیر
	// ========================================================
	void TickUnstuck(float DeltaTime);
	void StartUnstuckAttempt();
	bool RotateTowardUnstuckTarget(float DeltaTime);
	void MoveTowardUnstuckTarget();
	void EvaluateUnstuckResult();
	void RestorePathAfterUnstuck();

	// ========================================================
	// -------------------- State Machine -------------------------------
	// ========================================================
	void SetSteeringState(ESteeringState NewState);

	// ========================================================
	// ------------------------- متغیرهای عضو ------------------------------
	// ========================================================

	UPROPERTY()
	AUnitCharacter* Owner = nullptr;

	// --- مسیر ---
	TArray<FVector> CurrentPath;
	FPathCache PathCache;
	float TotalPathLength = 0.f;
	bool bHasPath = false;

	// --- هدف ---
	bool bHasTarget = false;
	FVector TargetSlot = FVector::ZeroVector;

	// --- گروه ---
	bool bHasGroupCenter = false;
	FVector GroupCenter = FVector::ZeroVector;
	FVector GroupForward = FVector::ForwardVector;

	// --- فرمیشن ---
	float DesiredLateralOffset = 0.f;

	// --- فرار (بدون هیچ Timer) ---
	EEvadeState EvadeState = EEvadeState::None;
	bool bDecisionLocked = false;

	// --- ماشین حالت اصلی ---
	ESteeringState SteeringState = ESteeringState::Idle;

	// --- گیر کردن ---
	float StuckTimer = 0.f;
	FVector StuckReferencePosition = FVector::ZeroVector;
	float StuckReferenceCheckTime = 0.f;

	// --- رفع گیر ---
	bool bIsRotatingForUnstuck = false;
	FVector UnstuckDirection = FVector::ZeroVector;
	FVector UnstuckTargetLocation = FVector::ZeroVector;
	FRotator UnstuckTargetRotation = FRotator::ZeroRotator;
	float UnstuckStartTime = 0.f;
	int32 UnstuckAttempts = 0;

	// --- ثابت‌ها ---
	static constexpr float STOP_DISTANCE = 20.f;
	static constexpr float BRAKING_DISTANCE = 400.f;
	static constexpr float MIN_LOOK_AHEAD = 50.f;
};