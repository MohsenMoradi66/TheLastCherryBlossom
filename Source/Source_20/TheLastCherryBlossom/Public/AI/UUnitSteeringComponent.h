// UUnitSteeringComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UUnitSteeringComponent.generated.h"

class AUnitCharacter;

// ================== ساختارهای پیش‌بینی برخورد ==================

USTRUCT(BlueprintType)
struct FCollisionPrediction
{
    GENERATED_BODY()
    
    bool bWillCollide = false;
    float TimeToCollision = FLT_MAX;
    FVector AvoidDirection = FVector::ZeroVector;
    float AvoidStrength = 0.f;
};

// ================== منبع فرار ==================

UENUM(BlueprintType)
enum class EEvadeSource : uint8
{
    None,
    Static,
    Dynamic,
    Both
};

// ================== ساختارهای موانع ==================

struct FStaticObstacleResult
{
    bool bHasObstacle = false;
    FVector ObstacleLocation = FVector::ZeroVector;
    float ObstacleRadius = 50.f;
    float DistanceToObstacle = FLT_MAX;
    FVector AvoidDirection = FVector::ZeroVector;
    float AvoidStrength = 0.f;
    float TimeToCollision = FLT_MAX;
};

struct FDynamicUnitResult
{
    bool bHasUnit = false;
    class AUnitCharacter* OtherUnit = nullptr;
    FVector UnitLocation = FVector::ZeroVector;
    FVector UnitVelocity = FVector::ZeroVector;
    float DistanceToUnit = FLT_MAX;
    FVector AvoidDirection = FVector::ZeroVector;
    float AvoidStrength = 0.f;
    float TimeToCollision = FLT_MAX;
    float ClosestDistance = FLT_MAX;
};

// ================== ساختارهای مسیر ==================

USTRUCT(BlueprintType)
struct FPathSegmentInfo
{
    GENERATED_BODY()
    
    int32 SegmentIndex = 0;
    float T = 0.f;
    FVector ClosestPoint = FVector::ZeroVector;
    FVector Tangent = FVector::ForwardVector;
    float DistanceToPointSq = 0.f;
    float LengthFromStart = 0.f;
};

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

// ================== کلاس اصلی ==================

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class THELASTCHERRYBLOSSOM_API UUnitSteeringComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UUnitSteeringComponent();

    void Initialize(AUnitCharacter* InOwner);
    void ExecuteMovement(float DeltaTime);
    void SetPath(const TArray<FVector>& NewPath);
    void ClearPath();
    void SetGroupParams(const FVector& InCenter, const FVector& InForward);
    void ClearGroupParams();
    
    void SetDesiredLateralOffset(float NewOffset) { DesiredLateralOffset = NewOffset; }
    float GetDesiredLateralOffset() const { return DesiredLateralOffset; }
    
    void SetTargetSlot(const FVector& InSlotLocation)
    {
        TargetSlot = InSlotLocation;
        bHasTarget = true;
    }
    
    bool IsMoving() const { return bHasPath && bHasTarget; }
    bool IsEvading() const { return bIsEvading; }
    EEvadeSource GetActiveEvadeSource() const { return ActiveEvadeSource; }
    void ForceStop();
    void ClearTarget();
    void ResetDirection();

    void ApplyMovementWithTarget(float DeltaTime, const FVector& InTarget);
    bool IsMovingTowardGoal() const;
    
    bool IsStuck() const;

    // توابع رفع گیر
    void ResetUnstuckAttempts() { UnstuckAttempts = 0; bIsUnstaking = false; }
    void AttemptUnstuck(float DeltaTime);
    void HandleUnstuckState(float DeltaTime);

protected:
    // ===== اندازه‌ی فیزیکی خود یونیت =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance")
    float AvoidanceRadius = 40.f;
    
    UPROPERTY(EditAnywhere, Category = "Movement")
    float LookAheadDistance = 150.f;
    
    // ===== پارامترهای فیلتر یونیت‌های ساکن =====
    UPROPERTY(EditAnywhere, Category = "Avoidance|Filters")
    float MinMovementSpeedForAvoidance = 30.f;
    
    UPROPERTY(EditAnywhere, Category = "Avoidance|Filters")
    float SameDirectionDotThreshold = 0.8f;
    
    // ========================================================
    // ===== پارامترهای موانع ثابت (Static) =====
    // ========================================================
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Static")
    float StaticFieldOfViewAngle = 120.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Static")
    float StaticMinScanRadius = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Static")
    float StaticMaxScanRadius = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Static")
    float StaticSpeedToRadiusFactor = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Static")
    float StaticSafeDistance = 60.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Static")
    float StaticMinEvadeDistance = 15.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Static")
    float StaticMaxEvadeDistance = 35.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Static")
    float StaticEvadeBlendBase = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Static")
    float StaticEvadeBlendRange = 0.15f;

    // ========================================================
    // ===== پارامترهای یونیت‌های متحرک (Dynamic) =====
    // ========================================================
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Dynamic")
    float DynamicFieldOfViewAngle = 120.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Dynamic")
    float DynamicMinScanRadius = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Dynamic")
    float DynamicMaxScanRadius = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Dynamic")
    float DynamicSpeedToRadiusFactor = 0.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Dynamic")
    float DynamicSafeDistance = 80.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Dynamic")
    float DynamicPredictionTime = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Dynamic")
    float DynamicMinEvadeDistance = 15.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Dynamic")
    float DynamicMaxEvadeDistance = 35.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Dynamic")
    float DynamicEvadeBlendBase = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance|Dynamic")
    float DynamicEvadeBlendRange = 0.25f;

    // ===== تشخیص گیر کردن =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Stuck")
    float StuckSpeedThreshold = 15.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Stuck")
    float StuckTimeThreshold = 0.5f;

    // ===== فشرده‌سازی گروه =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Formation")
    float CompressedOffsetRadiusFactor = 1.5f;

private:
    // ========== توابع مسیر ==========
    void UpdatePathCache();
    float GetDistanceAlongPath(const FVector& Location) const;
    bool GetPointOnPathFast(float DistanceFromStart, FVector& OutPoint, FVector& OutTangent) const;
    float GetDistanceToEndFast(const FVector& Location) const;
    float CalculateDynamicLookAhead(float DistanceToEnd) const;
    
    // ========== تشخیص موانع ==========
    FStaticObstacleResult CheckStaticObstacles(const FVector& MyPos, const FVector& DesiredDir, float DeltaTime) const;
    FDynamicUnitResult CheckDynamicUnits(const FVector& MyPos, const FVector& MyVelocity, const FVector& DesiredDir, float DeltaTime) const;
    
    // ========== سیستم فرار ==========
    FVector ComputeEvadeOffset(const FVector& DesiredDir, const FVector& ObstacleDir, float Strength,
                                float MinDist, float MaxDist, float BlendBase, float BlendRange) const;
    bool IsPathClear(const FVector& MyPos, const FVector& DesiredDir, float CheckDistance, EEvadeSource Source) const;
    
    // ========== توابع کمکی ==========
    void UpdateGroupCompression(float DeltaTime, const FVector& CurrentTangent);
    
    
    bool IsUnitMovingRelevantly(const AUnitCharacter* Unit) const;
    bool ShouldIgnoreUnit(const AUnitCharacter* OtherUnit, const FVector& MyPos, const FVector& MyVelocity) const;
    
    // ========== توابع اسکن ==========
    float GetStaticScanRadius() const;
    float GetDynamicUnitScanRadius() const;
    float GetStaticFOVDotThreshold() const;
    float GetDynamicFOVDotThreshold() const;
    bool IsWithinFieldOfView(const FVector& DesiredDir, const FVector& ToObstacle, float DotThreshold) const;
    
    // ========== تشخیص و رفع گیر ==========
    bool UpdateStuckDetection(float DeltaTime, float CurrentSpeed);
    void StartUnstuck();
    void CheckUnstuckSuccess(float DeltaTime);
    void RestorePathAfterUnstuck();
    void HandleUnstuck(float DeltaTime);
    
    // ========== متغیرهای مسیر ==========
    TArray<FVector> CurrentPath;
    float TotalPathLength = 0.f;
    bool bHasPath = false;
    FPathCache PathCache;
    
    // ========== متغیرهای حرکتی ==========
    AUnitCharacter* Owner = nullptr;
    bool bHasTarget = false;
    FVector TargetSlot;
    FVector CurrentDirection;
    float DistanceToGoal = 0.f;
    
    // ========== متغیرهای گروه ==========
    bool bHasGroupCenter = false;
    FVector GroupCenter;
    FVector GroupForward;
    
    // ========== وضعیت فرار ==========
    bool bIsEvading = false;
    EEvadeSource ActiveEvadeSource = EEvadeSource::None;
    
    // ========== متغیرهای فشرده‌سازی ==========
    float DesiredLateralOffset = 0.f;
    float CompressionRadius = 150.f;
    float CompressionStrength = 1.0f;
    float LinearCompressionSpeed = 8.f;
    
    // ========== مدیریت فریم اول ==========
    bool bIsFirstFrameOfNewPath = false;
    float TimeSinceLastMoveCommand = 0.f;
    
    float StuckTimer = 0.f;
    
    // ========== متغیرهای رفع گیر ==========
    bool bIsUnstaking = false;
    FVector UnstuckDirection = FVector::ZeroVector;
    FVector UnstuckTarget = FVector::ZeroVector;
    float UnstuckStartTime = 0.f;
    int32 UnstuckAttempts = 0;
    FRotator UnstuckTargetRotation = FRotator::ZeroRotator;
    bool bIsRotatingForUnstuck = false;
    
    UPROPERTY(EditAnywhere, Category = "Steering|Unstuck")
    float UnstuckRotationSpeed = 360.f;
    
    UPROPERTY(EditAnywhere, Category = "Steering|Unstuck")
    float UnstuckMoveDistance = 80.f;
    
    UPROPERTY(EditAnywhere, Category = "Steering|Unstuck")
    float UnstuckTimeout = 1.5f;
    
    UPROPERTY(EditAnywhere, Category = "Steering|Unstuck")
    int32 MaxUnstuckAttempts = 4;
    
    // ========== ثابت‌ها ==========
    static constexpr float STOP_DISTANCE = 20.f;
    static constexpr float BRAKING_DISTANCE = 400.f;
    static constexpr float MIN_LOOK_AHEAD = 50.f;
    static constexpr float FIRST_FRAME_TIME = 0.04f;
    static constexpr float LATERAL_OFFSET_EPSILON = 0.01f;
};